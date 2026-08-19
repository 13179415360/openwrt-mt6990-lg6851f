// SPDX-License-Identifier: GPL-2.0-only
/*
 * MT6990 global CPU hang detector
 *
 * The vendor kernel runs one high-priority "wdtk-N" thread on every CPU and
 * only considers a round healthy after every online CPU has checked in.  It
 * deliberately does not compare per-CPU local_clock() values.  This 6.18
 * adaptation keeps that bitmap contract without depending on the vendor
 * AEE/MRDUMP private ABI.  A round which remains incomplete through a second
 * confirmation window is converted into a normal panic so the existing
 * ramoops -> LG6851F p46 archive path retains the evidence.
 */

#include <linux/atomic.h>
#include <linux/cpu.h>
#include <linux/cpuhotplug.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/ktime.h>
#include <linux/module.h>
#include <linux/nmi.h>
#include <linux/notifier.h>
#include <linux/pm.h>
#include <linux/reboot.h>
#include <linux/sched/debug.h>
#include <linux/sched/rt.h>
#include <linux/sched/task.h>
#include <linux/suspend.h>
#include <linux/workqueue.h>

#define MT6990_HANGDET_PERIOD_MS		2000
#define MT6990_HANGDET_TIMEOUT_MS	15000
#define MT6990_HANGDET_CONFIRM_MS	8000
#define MT6990_HANGDET_BOOT_GRACE_MS	60000

struct mt6990_hangdet_cpu {
	struct task_struct *task;
	atomic64_t sequence;
};

static DEFINE_PER_CPU(struct mt6990_hangdet_cpu, mt6990_hangdet_cpus);
static DECLARE_DELAYED_WORK(mt6990_hangdet_work, NULL);
static struct workqueue_struct *mt6990_hangdet_wq;
static atomic_long_t mt6990_hangdet_kick_mask = ATOMIC_LONG_INIT(0);
static atomic_t mt6990_hangdet_fired = ATOMIC_INIT(0);
static atomic_t mt6990_hangdet_suspended = ATOMIC_INIT(0);
static bool mt6990_hangdet_confirming;
static unsigned long mt6990_hangdet_missing_mask;
static u64 mt6990_hangdet_deadline_ns;
static int mt6990_hangdet_hp_state;

static int mt6990_hangdet_thread(void *unused)
{
	struct mt6990_hangdet_cpu *state = this_cpu_ptr(&mt6990_hangdet_cpus);

	/* Match the vendor's real-time kicker design: this thread must still
	 * run when ordinary CFS work is saturated.
	 */
	sched_set_fifo(current);

	while (!kthread_should_stop()) {
		atomic64_inc(&state->sequence);
		atomic_long_or(BIT(smp_processor_id()),
			       &mt6990_hangdet_kick_mask);
		set_current_state(TASK_INTERRUPTIBLE);
		schedule_timeout(msecs_to_jiffies(MT6990_HANGDET_PERIOD_MS));
	}

	__set_current_state(TASK_RUNNING);
	return 0;
}

static int mt6990_hangdet_cpu_online(unsigned int cpu)
{
	struct mt6990_hangdet_cpu *state = per_cpu_ptr(&mt6990_hangdet_cpus,
							 cpu);
	struct task_struct *task;

	if (state->task)
		return 0;

	atomic64_set(&state->sequence, 0);
	task = kthread_create_on_cpu(mt6990_hangdet_thread, NULL, cpu,
				     "mt6990-wdtk/%u");
	if (IS_ERR(task))
		return PTR_ERR(task);

	state->task = task;
	wake_up_process(task);
	pr_info("MT6990-HANGDET: CPU%u global heartbeat online\n", cpu);
	return 0;
}

static int mt6990_hangdet_cpu_offline(unsigned int cpu)
{
	struct mt6990_hangdet_cpu *state = per_cpu_ptr(&mt6990_hangdet_cpus,
							 cpu);

	if (state->task) {
		kthread_stop(state->task);
		state->task = NULL;
	}
	return 0;
}

static unsigned long mt6990_hangdet_online_mask(void)
{
	unsigned long mask = 0;
	unsigned int cpu;

	for_each_online_cpu(cpu)
		mask |= BIT(cpu);
	return mask;
}

static void mt6990_hangdet_dump(unsigned long missing)
{
	unsigned int cpu;

	pr_emerg("MT6990-HANGDET: confirmed global CPU stall missing=0x%lx kick=0x%lx online=0x%lx\n",
		 missing, atomic_long_read(&mt6990_hangdet_kick_mask),
		 mt6990_hangdet_online_mask());
	for_each_online_cpu(cpu) {
		struct mt6990_hangdet_cpu *state;

		state = per_cpu_ptr(&mt6990_hangdet_cpus, cpu);
		pr_emerg("MT6990-HANGDET: CPU%u seq=%lld reported=%u task=%s/%d\n",
			 cpu, atomic64_read(&state->sequence),
			 !(missing & BIT(cpu)),
			 state->task ? state->task->comm : "none",
			 state->task ? state->task->pid : -1);
	}

	/* Keep the pre-panic path bounded so the 31 second hardware WDT cannot
	 * reset the board while a full task-list dump blocks on the UART.
	 */
	trigger_all_cpu_backtrace();
	panic("MT6990 global hang detector: missing CPU mask 0x%lx", missing);
}

static void mt6990_hangdet_check(struct work_struct *work)
{
	unsigned long online, kicked, missing;
	u64 now = ktime_get_mono_fast_ns();

	if (atomic_read(&mt6990_hangdet_suspended))
		goto requeue;

	online = mt6990_hangdet_online_mask();
	kicked = atomic_long_read(&mt6990_hangdet_kick_mask);
	if ((kicked & online) == online) {
		/* Like the vendor [wdk-k] path: a complete CPU bitmap closes the
		 * round.  Threads which race with xchg simply report next period.
		 */
		atomic_long_xchg(&mt6990_hangdet_kick_mask, 0);
		mt6990_hangdet_deadline_ns = now +
			MT6990_HANGDET_TIMEOUT_MS * NSEC_PER_MSEC;
		if (mt6990_hangdet_confirming)
			pr_warn("MT6990-HANGDET: pending stall recovered kick=0x%lx online=0x%lx\n",
				kicked, online);
		mt6990_hangdet_confirming = false;
		mt6990_hangdet_missing_mask = 0;
		goto requeue;
	}

	if (now < mt6990_hangdet_deadline_ns)
		goto requeue;

	/* Re-read after the deadline to close the check/report race. */
	kicked = atomic_long_read(&mt6990_hangdet_kick_mask);
	missing = online & ~kicked;
	if (!missing)
		goto requeue;

	if (!mt6990_hangdet_confirming) {
		mt6990_hangdet_confirming = true;
		mt6990_hangdet_missing_mask = missing;
		mt6990_hangdet_deadline_ns = now +
			MT6990_HANGDET_CONFIRM_MS * NSEC_PER_MSEC;
		pr_warn("MT6990-HANGDET: incomplete round pending confirmation missing=0x%lx kick=0x%lx online=0x%lx\n",
			missing, kicked, online);
		goto requeue;
	}

	missing |= mt6990_hangdet_missing_mask;
	if (atomic_cmpxchg(&mt6990_hangdet_fired, 0, 1) == 0)
		mt6990_hangdet_dump(missing);

requeue:
	queue_delayed_work(mt6990_hangdet_wq, &mt6990_hangdet_work,
			   msecs_to_jiffies(MT6990_HANGDET_PERIOD_MS));
}

static int mt6990_hangdet_pm_notify(struct notifier_block *nb,
				    unsigned long action, void *unused)
{
	switch (action) {
	case PM_HIBERNATION_PREPARE:
	case PM_SUSPEND_PREPARE:
	case PM_RESTORE_PREPARE:
		atomic_set(&mt6990_hangdet_suspended, 1);
		break;
	case PM_POST_HIBERNATION:
	case PM_POST_SUSPEND:
	case PM_POST_RESTORE:
		atomic_long_set(&mt6990_hangdet_kick_mask, 0);
		mt6990_hangdet_confirming = false;
		mt6990_hangdet_missing_mask = 0;
		mt6990_hangdet_deadline_ns = ktime_get_mono_fast_ns() +
			MT6990_HANGDET_BOOT_GRACE_MS * NSEC_PER_MSEC;
		atomic_set(&mt6990_hangdet_suspended, 0);
		break;
	default:
		break;
	}
	return NOTIFY_OK;
}

static struct notifier_block mt6990_hangdet_pm_nb = {
	.notifier_call = mt6990_hangdet_pm_notify,
};

static int __init mt6990_hangdet_init(void)
{
	int ret;

	mt6990_hangdet_deadline_ns = ktime_get_mono_fast_ns() +
		MT6990_HANGDET_BOOT_GRACE_MS * NSEC_PER_MSEC;
	mt6990_hangdet_hp_state = cpuhp_setup_state(CPUHP_AP_ONLINE_DYN,
			"watchdog/mt6990-hangdet:online",
			mt6990_hangdet_cpu_online,
			mt6990_hangdet_cpu_offline);
	if (mt6990_hangdet_hp_state < 0)
		return mt6990_hangdet_hp_state;

	ret = register_pm_notifier(&mt6990_hangdet_pm_nb);
	if (ret) {
		cpuhp_remove_state(mt6990_hangdet_hp_state);
		return ret;
	}

	mt6990_hangdet_wq = alloc_workqueue("mt6990-hangdet",
		WQ_HIGHPRI | WQ_UNBOUND | WQ_MEM_RECLAIM, 1);
	if (!mt6990_hangdet_wq) {
		unregister_pm_notifier(&mt6990_hangdet_pm_nb);
		cpuhp_remove_state(mt6990_hangdet_hp_state);
		return -ENOMEM;
	}

	INIT_DELAYED_WORK(&mt6990_hangdet_work, mt6990_hangdet_check);
	queue_delayed_work(mt6990_hangdet_wq, &mt6990_hangdet_work,
			   msecs_to_jiffies(MT6990_HANGDET_PERIOD_MS));
	pr_info("MT6990-HANGDET: vendor bitmap takeover active period=%ums timeout=%ums confirm=%ums grace=%ums\n",
		MT6990_HANGDET_PERIOD_MS, MT6990_HANGDET_TIMEOUT_MS,
		MT6990_HANGDET_CONFIRM_MS, MT6990_HANGDET_BOOT_GRACE_MS);
	return 0;
}
late_initcall(mt6990_hangdet_init);

MODULE_DESCRIPTION("MediaTek MT6990 global CPU hang detector");
MODULE_LICENSE("GPL");
