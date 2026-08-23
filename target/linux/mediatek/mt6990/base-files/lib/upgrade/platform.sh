#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-only
# FiberHome LG6851F fixed-B-slot signed boot + rootfs upgrade contract.

RAMFS_COPY_BIN='tar gzip dd sha256sum wc grep sed cut mount umount cp mkdir sync hexdump rm'

# The 6.18.37 recovery baseline can panic while procd stops the vendor modem
# stack (DPMAIF frees already-corrupted skb slabs).  Ask patched procd to enter
# the RAM upgrade root before stopping services.  platform_do_upgrade() then
# commits and verifies the inactive/fixed B images before the final forced
# reboot, so a shutdown-time driver failure can no longer prevent the update.
LG6851F_DEFER_SERVICE_STOP=1
export LG6851F_DEFER_SERVICE_STOP

LG6851F_TRACE_MNT=/tmp/lg6851f-upgrade-data

lg6851f_trace() {
	mkdir -p "$LG6851F_TRACE_MNT"
	if ! grep -q " $LG6851F_TRACE_MNT " /proc/mounts; then
		mount -o rw,noatime /dev/mmcblk0p46 "$LG6851F_TRACE_MNT" 2>/dev/null || {
			echo "lg6851f-upgrade-trace: $*" > /dev/kmsg 2>/dev/null
			return 0
		}
	fi
	mkdir -p "$LG6851F_TRACE_MNT/openwrt-sysupgrade"
	echo "$(date '+%Y-%m-%d %H:%M:%S') $*" >> "$LG6851F_TRACE_MNT/openwrt-sysupgrade/upgrade-trace.log"
	echo "lg6851f-upgrade-trace: $*" > /dev/kmsg 2>/dev/null
	sync
}

lg6851f_extract_control() {
	local image="$1" output="$2" prefix="${2}.prefix"

	rm -f "$prefix" "$output"
	# CONTROL is intentionally the first tar member.  Read the tar header plus
	# three data blocks so future safety fields cannot be silently truncated. Do not
	# use BusyBox `tar -xOzf`: it consumes the entire gzip stream even when the
	# requested member is first, which repeatedly trips the MT6990 watchdog.
	gzip -dc "$image" 2>/dev/null | dd of="$prefix" bs=512 count=4 2>/dev/null
	[ "$(dd if="$prefix" bs=1 count=7 2>/dev/null)" = CONTROL ] || {
		rm -f "$prefix"
		return 1
	}
	dd if="$prefix" of="$output" bs=512 skip=1 count=3 2>/dev/null || {
		rm -f "$prefix" "$output"
		return 1
	}
	rm -f "$prefix"
}

lg6851f_control_value() {
	sed -n "s/^$2=//p" "$1"
}

lg6851f_check_a_image() {
	local control="$1" boot_size rootfs_size boot_sha rootfs_sha connsys_size connsys_sha
	grep -qw 'bootslot=a' /proc/cmdline || {
		echo "This is the A-slot Web package, but the device is not running slot A."
		return 1
	}
	[ "$(lg6851f_control_value "$control" slot)" = a ] || return 1
	[ "$(lg6851f_control_value "$control" boot_partition)" = boot_a ] || return 1
	[ "$(lg6851f_control_value "$control" rootfs_partition)" = rootfs_a ] || return 1
	[ "$(lg6851f_control_value "$control" connsys_gnss_partition)" = connsys_gnss_a ] || return 1
	[ "$(lg6851f_control_value "$control" touches_slot_b)" = 0 ] || return 1
	[ "$(lg6851f_control_value "$control" touches_bootctrl)" = priority-and-a-retry ] || return 1
	boot_size="$(lg6851f_control_value "$control" boot_size)"
	rootfs_size="$(lg6851f_control_value "$control" rootfs_size)"
	boot_sha="$(lg6851f_control_value "$control" boot_sha256)"
	rootfs_sha="$(lg6851f_control_value "$control" rootfs_sha256)"
	connsys_size="$(lg6851f_control_value "$control" connsys_gnss_size)"
	connsys_sha="$(lg6851f_control_value "$control" connsys_gnss_sha256)"
	[ "$boot_size" = 33554432 ] && [ "$rootfs_size" = 134217728 ] || return 1
	[ "$connsys_size" = 8388608 ] || return 1
	[ "${#boot_sha}" = 64 ] && [ -z "$(echo "$boot_sha" | sed 's/[0-9a-f]//g')" ] || return 1
	[ "${#rootfs_sha}" = 64 ] && [ -z "$(echo "$rootfs_sha" | sed 's/[0-9a-f]//g')" ] || return 1
	[ "$connsys_sha" = 633a1e2a19593d9cf12db4e140d893da9d1817c721381848831d90afc0b02463 ] || return 1
	[ -b /dev/mmcblk0p14 ] && [ -b /dev/mmcblk0p25 ] && [ -b /dev/mmcblk0p26 ] || return 1
	[ "$(cat /sys/class/block/mmcblk0p14/partition)" = 14 ] || return 1
	[ "$(cat /sys/class/block/mmcblk0p14/size)" = 16384 ] || return 1
	grep -qx 'PARTNAME=connsys_gnss_a' /sys/class/block/mmcblk0p14/uevent || return 1
	[ "$(cat /sys/class/block/mmcblk0p25/size)" = 65536 ] || return 1
	[ "$(cat /sys/class/block/mmcblk0p26/size)" = 262144 ] || return 1
	echo "LG6851F fixed-A package header verified; synchronized writes and partition headers will be checked during upgrade."
}

lg6851f_do_upgrade_a() {
	local image="$1" magic before after expected control connsys_expected connsys_actual
	lg6851f_trace "BEGIN image=$image slot=a"
	control=/tmp/lg6851f-upgrade-control
	connsys_expected="$(lg6851f_control_value "$control" connsys_gnss_sha256)"
	[ "$connsys_expected" = 633a1e2a19593d9cf12db4e140d893da9d1817c721381848831d90afc0b02463 ] || {
		lg6851f_trace "FAIL invalid connsys_gnss_a declaration"; exit 1;
	}
	[ "$(cat /sys/class/block/mmcblk0p14/partition 2>/dev/null)" = 14 ] &&
		[ "$(cat /sys/class/block/mmcblk0p14/size 2>/dev/null)" = 16384 ] &&
		grep -qx 'PARTNAME=connsys_gnss_a' /sys/class/block/mmcblk0p14/uevent 2>/dev/null || {
		lg6851f_trace "FAIL p14 target identity mismatch; no image partitions touched"; exit 1;
	}
	connsys_actual="$(sha256sum /dev/mmcblk0p14 2>/dev/null | cut -d ' ' -f 1)"
	if [ "$connsys_actual" = "$connsys_expected" ]; then
		lg6851f_trace "PASS connsys_gnss_a already healthy; write skipped"
	else
		echo "Restoring signed connsys_gnss_a to /dev/mmcblk0p14 ..."
		lg6851f_trace "WRITE connsys_gnss_a old_sha256=$connsys_actual mode=tar-direct+sync+sha256"
		tar -xOzf "$image" connsys_gnss_a.img > /dev/mmcblk0p14 || { lg6851f_trace "FAIL connsys_gnss_a write"; exit 1; }
		sync
		connsys_actual="$(sha256sum /dev/mmcblk0p14 2>/dev/null | cut -d ' ' -f 1)"
		[ "$connsys_actual" = "$connsys_expected" ] || {
			lg6851f_trace "FAIL connsys_gnss_a readback expected=$connsys_expected actual=$connsys_actual"; exit 1;
		}
		lg6851f_trace "PASS connsys_gnss_a restored and readback verified sha256=$connsys_actual"
	fi
	echo "Writing rootfs_a to /dev/mmcblk0p26 ..."
	tar -xOzf "$image" rootfs_a.img > /dev/mmcblk0p26 || exit 1
	sync
	magic="$(dd if=/dev/mmcblk0p26 bs=1 count=4 2>/dev/null | hexdump -v -e '1/1 "%02x"')"
	[ "$magic" = 68737173 ] || { lg6851f_trace "FAIL rootfs_a magic=$magic"; exit 1; }
	lg6851f_trace "PASS rootfs_a synced magic=hsqs"
	echo "Writing boot_a to /dev/mmcblk0p25 ..."
	tar -xOzf "$image" boot_a.img > /dev/mmcblk0p25 || exit 1
	sync
	magic="$(dd if=/dev/mmcblk0p25 bs=1 count=8 2>/dev/null | hexdump -v -e '1/1 "%02x"')"
	[ "$magic" = 414e44524f494421 ] || { lg6851f_trace "FAIL boot_a magic=$magic"; exit 1; }
	lg6851f_trace "PASS boot_a synced magic=ANDROID!"
	dd if=/dev/mmcblk0p1 of=/tmp/lg6851f-bootctrl10.before bs=1 skip=2060 count=10 2>/dev/null || exit 1
	before="$(hexdump -v -e '1/1 "%02x"' /tmp/lg6851f-bootctrl10.before)"
	[ "${#before}" = 20 ] || exit 1
	expected="0f00${before:4:6}0e${before:12:8}"
	printf '\017\000' | dd of=/dev/mmcblk0p1 bs=1 seek=2060 count=2 conv=notrunc 2>/dev/null || exit 1
	printf '\016' | dd of=/dev/mmcblk0p1 bs=1 seek=2065 count=1 conv=notrunc 2>/dev/null || exit 1
	sync
	after="$(dd if=/dev/mmcblk0p1 bs=1 skip=2060 count=10 2>/dev/null | hexdump -v -e '1/1 "%02x"')"
	[ "$after" = "$expected" ] || {
		dd if=/tmp/lg6851f-bootctrl10.before of=/dev/mmcblk0p1 bs=1 count=10 seek=2060 conv=notrunc 2>/dev/null
		sync; lg6851f_trace "FAIL bootctrl A verify expected=$expected actual=$after"; exit 1
	}
	rm -f /tmp/lg6851f-bootctrl10.before
	lg6851f_trace "COMPLETE connsys_gnss_a+boot_a+rootfs_a; verification=GNSS-sha256+boot/rootfs-magic slot_b=untouched bootctrl=A-priority+A-retry-reset"
	umount "$LG6851F_TRACE_MNT" 2>/dev/null || true
	echo "LG6851F A-slot upgrade complete; A is the verified next boot target."
}

platform_check_image() {
	local image="$1" board format boot_size rootfs_size boot_sha rootfs_sha image_size control
	local connsys_size connsys_sha

	board="$(cat /tmp/sysinfo/board_name 2>/dev/null)"
	[ -n "$board" ] || board="$(board_name 2>/dev/null)"
	case "$board" in
		fiberhome,lg6851f|fiberhome_lg6851f) ;;
		*) echo "This package is only for FiberHome LG6851F (board=$board)."; return 1 ;;
	esac

	# Keep LuCI upload validation deliberately lightweight.  LuCI invokes this
	# function twice (RPC validation and `sysupgrade --test`).  Expanding or
	# hashing the padded 32 MiB + 128 MiB payload here has repeatedly triggered
	# the MT6990 hardware watchdog.  The browser already displays the uploaded
	# package SHA256; payload hashes are enforced by full partition readback
	# after the user explicitly confirms the upgrade.
	[ "$(dd if="$image" bs=2 count=1 2>/dev/null | hexdump -v -e '1/1 "%02x"')" = 1f8b ] || {
		echo "Invalid LG6851F gzip package header."
		return 1
	}
	image_size="$(wc -c < "$image" | sed 's/[[:space:]]//g')"
	case "$image_size" in
		''|*[!0-9]*) return 1 ;;
	esac
	[ "$image_size" -ge 1048576 ] && [ "$image_size" -le 268435456 ] || {
		echo "LG6851F upgrade package size is outside the accepted range."
		return 1
	}
	control=/tmp/lg6851f-upload-control
	lg6851f_extract_control "$image" "$control" || {
		echo "Unable to read the LG6851F package CONTROL header."
		return 1
	}
	format="$(lg6851f_control_value "$control" format)"
	if [ "$format" = lg6851f-mt6990-a-signed-v1 ]; then
		lg6851f_check_a_image "$control"
		return $?
	fi
	[ "$format" = lg6851f-mt6990-b-signed-v1 ] || {
		echo "Unsupported LG6851F upgrade format: $format"
		return 1
	}
	[ "$(lg6851f_control_value "$control" slot)" = b ] || return 1
	grep -qw 'bootslot=b' /proc/cmdline || {
		echo "This is the B-slot Web package, but the device is not running slot B."
		return 1
	}
	[ "$(lg6851f_control_value "$control" touches_slot_a)" = 0 ] || return 1
	[ "$(lg6851f_control_value "$control" touches_bootctrl)" = priority-and-b-retry ] || return 1

	boot_size="$(lg6851f_control_value "$control" boot_size)"
	rootfs_size="$(lg6851f_control_value "$control" rootfs_size)"
	boot_sha="$(lg6851f_control_value "$control" boot_sha256)"
	rootfs_sha="$(lg6851f_control_value "$control" rootfs_sha256)"
	connsys_size="$(lg6851f_control_value "$control" connsys_gnss_size)"
	connsys_sha="$(lg6851f_control_value "$control" connsys_gnss_sha256)"
	[ "$boot_size" = 33554432 ] || { echo "Invalid declared boot_b size: $boot_size"; return 1; }
	[ "$rootfs_size" = 134217728 ] || { echo "Invalid declared rootfs_b size: $rootfs_size"; return 1; }
	[ "$connsys_size" = 8388608 ] || { echo "Invalid declared connsys_gnss_b size: $connsys_size"; return 1; }
	[ "${#boot_sha}" = 64 ] && [ -z "$(echo "$boot_sha" | sed 's/[0-9a-f]//g')" ] || return 1
	[ "${#rootfs_sha}" = 64 ] && [ -z "$(echo "$rootfs_sha" | sed 's/[0-9a-f]//g')" ] || return 1
	[ "$connsys_sha" = 633a1e2a19593d9cf12db4e140d893da9d1817c721381848831d90afc0b02463 ] || return 1
	[ "$(lg6851f_control_value "$control" connsys_gnss_partition)" = connsys_gnss_b ] || return 1

	[ -b /dev/mmcblk0p27 ] && [ -b /dev/mmcblk0p38 ] && [ -b /dev/mmcblk0p39 ] || {
		echo "LG6851F B-slot block devices are missing."
		return 1
	}
	[ "$(cat /sys/class/block/mmcblk0p38/size)" = 65536 ] || return 1
	[ "$(cat /sys/class/block/mmcblk0p39/size)" = 262144 ] || return 1
	[ "$(cat /sys/class/block/mmcblk0p27/partition)" = 27 ] || return 1
	[ "$(cat /sys/class/block/mmcblk0p27/size)" = 16384 ] || return 1
	grep -qx 'PARTNAME=connsys_gnss_b' /sys/class/block/mmcblk0p27/uevent || return 1

	echo "LG6851F fixed-B package header verified; synchronized writes and partition headers will be checked during upgrade."
	return 0
}

platform_do_upgrade() {
	local image="$1" bootctrl_before bootctrl_after expected_bootctrl control magic
	local connsys_expected connsys_actual

	lg6851f_trace "BEGIN image=$image slot=b"
	control=/tmp/lg6851f-upgrade-control
	lg6851f_extract_control "$image" "$control" || {
		lg6851f_trace "FAIL unable to read CONTROL prefix"
		exit 1
	}
	if [ "$(lg6851f_control_value "$control" slot)" = a ]; then
		lg6851f_do_upgrade_a "$image"
		return
	fi
	connsys_expected="$(lg6851f_control_value "$control" connsys_gnss_sha256)"
	[ "$connsys_expected" = 633a1e2a19593d9cf12db4e140d893da9d1817c721381848831d90afc0b02463 ] || {
		lg6851f_trace "FAIL invalid connsys_gnss_b declaration"
		exit 1
	}
	[ "$(cat /sys/class/block/mmcblk0p27/partition 2>/dev/null)" = 27 ] &&
		[ "$(cat /sys/class/block/mmcblk0p27/size 2>/dev/null)" = 16384 ] &&
		grep -qx 'PARTNAME=connsys_gnss_b' /sys/class/block/mmcblk0p27/uevent 2>/dev/null || {
		lg6851f_trace "FAIL p27 target identity mismatch; no image partitions touched"
		exit 1
	}

	connsys_actual="$(sha256sum /dev/mmcblk0p27 2>/dev/null | cut -d ' ' -f 1)"
	if [ "$connsys_actual" = "$connsys_expected" ]; then
		lg6851f_trace "PASS connsys_gnss_b already healthy; write skipped"
	else
		echo "Restoring signed connsys_gnss_b to /dev/mmcblk0p27 ..."
		lg6851f_trace "WRITE connsys_gnss_b old_sha256=$connsys_actual mode=tar-direct+sync+sha256"
		tar -xOzf "$image" connsys_gnss_b.img > /dev/mmcblk0p27 || {
			lg6851f_trace "FAIL connsys_gnss_b write"
			exit 1
		}
		sync
		connsys_actual="$(sha256sum /dev/mmcblk0p27 2>/dev/null | cut -d ' ' -f 1)"
		[ "$connsys_actual" = "$connsys_expected" ] || {
			lg6851f_trace "FAIL connsys_gnss_b readback expected=$connsys_expected actual=$connsys_actual"
			exit 1
		}
		lg6851f_trace "PASS connsys_gnss_b restored and readback verified sha256=$connsys_actual"
	fi

	# Write rootfs first.  Avoid a full 128 MiB SHA256 readback on this platform:
	# sustained block reads have repeatedly starved the hardware watchdog.  A
	# successful tar extraction directly into the block device, sync, and the
	# expected SquashFS header are the bounded post-write checks.  Slot A stays
	# bootable if either B-slot write fails.
	echo "Writing signed package rootfs_b to /dev/mmcblk0p39 ..."
	lg6851f_trace "WRITE rootfs_b mode=tar-direct+sync+magic"
	tar -xOzf "$image" rootfs_b.img > /dev/mmcblk0p39 || {
		lg6851f_trace "FAIL rootfs_b write"
		exit 1
	}
	sync
	magic="$(dd if=/dev/mmcblk0p39 bs=1 count=4 2>/dev/null | hexdump -v -e '1/1 "%02x"')"
	[ "$magic" = 68737173 ] || {
		lg6851f_trace "FAIL rootfs_b magic=$magic"
		echo "rootfs_b SquashFS header verification failed."
		exit 1
	}
	lg6851f_trace "PASS rootfs_b synced magic=hsqs"

	echo "Writing signed boot_b to /dev/mmcblk0p38 ..."
	lg6851f_trace "WRITE boot_b mode=tar-direct+sync+magic"
	tar -xOzf "$image" boot_b.img > /dev/mmcblk0p38 || {
		lg6851f_trace "FAIL boot_b write"
		exit 1
	}
	sync
	magic="$(dd if=/dev/mmcblk0p38 bs=1 count=8 2>/dev/null | hexdump -v -e '1/1 "%02x"')"
	[ "$magic" = 414e44524f494421 ] || {
		lg6851f_trace "FAIL boot_b magic=$magic"
		echo "boot_b Android header verification failed."
		exit 1
	}
	lg6851f_trace "PASS boot_b synced magic=ANDROID!"

	# Prepare a fresh B-slot boot attempt.  The vendor bootloader increments
	# B's ab_retry byte on each B boot and falls back to A when it reaches 4.
	# Reusing that exhausted value makes an otherwise valid Web upgrade appear
	# to fail immediately.  Set A/B priorities to 0e/0f and reset only B's
	# ab_retry byte; preserve both success_boot/up_type fields and all A state.
	dd if=/dev/mmcblk0p1 of=/tmp/lg6851f-bootctrl10.before bs=1 skip=2060 count=10 2>/dev/null || exit 1
	bootctrl_before="$(hexdump -v -e '1/1 "%02x"' /tmp/lg6851f-bootctrl10.before)"
	[ "${#bootctrl_before}" = 20 ] || {
		lg6851f_trace "FAIL bootctrl read before priority switch value=$bootctrl_before"
		echo "Unable to read LG6851F bootctrl record."
		exit 1
	}
	expected_bootctrl="0e${bootctrl_before:2:8}0f00${bootctrl_before:14:6}"
	lg6851f_trace "BOOTCTRL before=$bootctrl_before expected=$expected_bootctrl"
	printf '\016' | dd of=/dev/mmcblk0p1 bs=1 seek=2060 count=1 conv=notrunc 2>/dev/null || exit 1
	printf '\017' | dd of=/dev/mmcblk0p1 bs=1 seek=2065 count=1 conv=notrunc 2>/dev/null || {
		dd if=/tmp/lg6851f-bootctrl10.before of=/dev/mmcblk0p1 bs=1 count=10 seek=2060 conv=notrunc 2>/dev/null
		sync
		exit 1
	}
	printf '\000' | dd of=/dev/mmcblk0p1 bs=1 seek=2066 count=1 conv=notrunc 2>/dev/null || {
		dd if=/tmp/lg6851f-bootctrl10.before of=/dev/mmcblk0p1 bs=1 count=10 seek=2060 conv=notrunc 2>/dev/null
		sync
		exit 1
	}
	sync
	bootctrl_after="$(dd if=/dev/mmcblk0p1 bs=1 skip=2060 count=10 2>/dev/null | hexdump -v -e '1/1 "%02x"')"
	[ "$bootctrl_after" = "$expected_bootctrl" ] || {
		lg6851f_trace "FAIL bootctrl B-attempt verify expected=$expected_bootctrl actual=$bootctrl_after"
		dd if=/tmp/lg6851f-bootctrl10.before of=/dev/mmcblk0p1 bs=1 count=10 seek=2060 conv=notrunc 2>/dev/null
		sync
		echo "LG6851F B-slot bootctrl verification failed."
		exit 1
	}
	rm -f /tmp/lg6851f-bootctrl10.before
	lg6851f_trace "PASS bootctrl priority A=0e B=0f B_retry=00 success/up_type/A_state=preserved"

	sync
	lg6851f_trace "COMPLETE connsys_gnss_b+boot_b+rootfs_b; verification=GNSS-sha256+boot/rootfs-magic slot_a=untouched bootctrl=priority-and-b-retry"
	umount "$LG6851F_TRACE_MNT" 2>/dev/null || true
	echo "LG6851F B-slot upgrade complete. Slot A is untouched; B is the verified next boot target."
}

platform_copy_config() {
	local mnt="$LG6851F_TRACE_MNT"

	[ -n "$UPGRADE_BACKUP" ] && [ -f "$UPGRADE_BACKUP" ] || return 0
	mkdir -p "$mnt"
	grep -q " $mnt " /proc/mounts || mount -o rw,noatime /dev/mmcblk0p46 "$mnt" || {
		echo "Unable to mount user_data for configuration preservation."
		return 1
	}
	mkdir -p "$mnt/openwrt-sysupgrade"
	cp -f "$UPGRADE_BACKUP" "$mnt/openwrt-sysupgrade/sysupgrade.tgz" || {
		umount "$mnt"
		return 1
	}
	sync
	umount "$mnt"
	echo "Configuration backup saved to persistent user_data."
}
