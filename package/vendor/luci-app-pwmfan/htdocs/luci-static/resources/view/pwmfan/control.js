'use strict';
'require baseclass';
'require rpc';
'require ui';

const callStatus = rpc.declare({
	object: 'luci.pwmfan',
	method: 'get_status',
	expect: { }
});

const callSetPwm = rpc.declare({
	object: 'luci.pwmfan',
	method: 'set_pwm',
	params: [ 'value' ]
});

const callRestoreAuto = rpc.declare({
	object: 'luci.pwmfan',
	method: 'restore_auto'
});

function speedLabel(value) {
	return ({ 100: _('Standard'), 125: _('Normal'), 170: _('Medium'),
		200: _('High'), 220: _('Very high'), 240: _('Maximum'),
		255: _('Emergency maximum') })[value] || _('PWM');
}

return baseclass.extend({
	title: _('MT6990 PWM Fan'),

	load() {
		return L.resolveDefault(callStatus(), { available: false });
	},

	update(data) {
		if (!data || !data.available)
			return;

		const pwm = document.getElementById('pwmfan-value');
		const state = document.getElementById('pwmfan-state');
		const temp = document.getElementById('pwmfan-temp');
		const bar = document.getElementById('pwmfan-bar');
		const target = document.getElementById('pwmfan-target');
		const status = document.getElementById('pwmfan-status');
		const rpm = document.getElementById('pwmfan-rpm');
		const channels = document.getElementById('pwmfan-channels');
		const controller = document.getElementById('pwmfan-controller');

		if (pwm) pwm.textContent = '%s (%s)'.format(data.pwm, speedLabel(data.pwm));
		if (state) state.textContent = '%s / %s'.format(data.cooling_state ?? '-', data.max_state ?? '-');
		if (temp) temp.textContent = data.hottest != null ? '%.1f °C'.format(data.hottest / 1000) : _('Unavailable');
		if (bar) bar.style.width = '%d%%'.format(Math.max(0, Math.min(100, ((data.hottest / 1000) - 35) * 100 / 55)));
		if (target) target.textContent = '%s (%s)'.format(data.pwm, data.reverse ? _('Reverse PWM') : _('PWM'));
		if (status) status.textContent = data.pwm > 0 ? _('Running') : _('Stopped');
		if (rpm) rpm.textContent = data.tach_available ? '%d RPM'.format(data.rpm) : _('Unavailable');
		if (channels) channels.textContent = (data.pwm_values || []).map(function(fan, index) {
			return '%s%d=%s'.format(_('Fan channel '), index + 1, fan.pwm ?? '-');
		}).join(' · ');
		if (controller) {
			const values = {};
			String(data.controller || '').trim().split(/\n/).forEach(function(line) {
				const pos = line.indexOf('=');
				if (pos > 0) values[line.substring(0, pos)] = line.substring(pos + 1);
			});
			controller.textContent = values.status ? '%s · %s · %s'.format(values.status, values.active, values.direction) : _('Detecting');
		}
	},

	handleSet(value, ev) {
		ev.currentTarget.blur();
		return callSetPwm(value).then(L.bind(function(result) {
			if (!result || !result.success)
				throw new Error(result?.error || _('PWM control failed'));
			this.update(result.status);
			ui.addNotification(null, E('p', {}, _('Temporary fan speed applied; thermal control remains active.')));
		}, this)).catch(function(err) {
				ui.addNotification(_('MT6990 PWM Fan'), E('p', {}, err.message), 'error');
		});
	},

	handleAuto(ev) {
		ev.currentTarget.blur();
		return callRestoreAuto().then(L.bind(function(result) {
			if (!result || !result.success)
				throw new Error(result?.error || _('Unable to restore automatic control'));
			this.update(result.status);
			ui.addNotification(null, E('p', {}, _('Fan output matched to the current temperature profile.')));
		}, this)).catch(function(err) {
				ui.addNotification(_('MT6990 PWM Fan'), E('p', {}, err.message), 'error');
		});
	},

	render(data) {
		if (!data || !data.available)
			return E('em', {}, _('No standard pwm-fan hwmon device was found.'));

		const buttons = [ 125, 170, 200, 220, 240, 255 ].map(L.bind(function(value) {
			return E('button', {
				'class': 'btn cbi-button pwmfan-speed',
				'click': ui.createHandlerFn(this, 'handleSet', value),
				'title': _('Verified reliable fan speed')
			}, [ '%s · %s'.format(value, speedLabel(value)) ]);
		}, this));

		buttons.push(E('button', {
			'class': 'btn cbi-button cbi-button-positive',
			'click': ui.createHandlerFn(this, 'handleAuto')
		}, [ _('Apply temperature profile') ]));

		const view = E('div', { 'class': 'pwmfan-page' }, [
			E('style', {}, [
				'.pwmfan-page{max-width:1050px;margin:0 auto}.pwmfan-head{display:flex;justify-content:space-between;align-items:center;margin:.2rem 0 1rem}',
				'.pwmfan-live{color:#278b45;font-weight:600}.pwmfan-live:before{content:"";display:inline-block;width:.65rem;height:.65rem;background:#2ecc71;border-radius:50%;margin-right:.45rem;box-shadow:0 0 0 .2rem rgba(46,204,113,.14)}',
				'.pwmfan-panel{padding:1.2rem 1.3rem;margin-bottom:1rem;border:1px solid var(--border-color-medium,#ddd);border-radius:.7rem;background:var(--background-color-high,#fff);box-shadow:0 1px 4px rgba(0,0,0,.04)}',
				'.pwmfan-panel h3{margin:0 0 1rem;font-size:1.15rem}.pwmfan-grid{display:grid;grid-template-columns:repeat(3,minmax(8em,1fr));gap:.75rem;margin-top:1rem}',
				'.pwmfan-metric{padding:.85rem;border-radius:.5rem;background:var(--background-color-medium,#f5f6f7)}.pwmfan-metric small{display:block;opacity:.68;margin-bottom:.3rem}.pwmfan-metric strong{font-size:1.12rem}',
				'.pwmfan-temp-line{display:flex;justify-content:space-between;font-weight:600}.pwmfan-gauge{height:2rem;background:linear-gradient(90deg,#84cc16 0%,#facc15 48%,#fb923c 72%,#ef4444 100%);border-radius:.55rem;overflow:hidden;margin:.55rem 0 .25rem;position:relative}',
				'.pwmfan-gauge span{display:block;height:100%;border-right:3px solid #fff;background:rgba(255,255,255,.15);transition:width .3s}.pwmfan-scale{display:flex;justify-content:space-between;font-size:.78rem;opacity:.7}',
				'.pwmfan-curve{display:grid;grid-template-columns:repeat(5,1fr);gap:.45rem}.pwmfan-step{text-align:center;padding:.8rem .3rem;border-radius:.45rem;background:var(--background-color-medium,#f5f6f7)}.pwmfan-step strong,.pwmfan-step small{display:block}.pwmfan-step small{opacity:.68;margin-top:.2rem}',
				'.pwmfan-actions{display:grid;grid-template-columns:repeat(3,1fr);gap:.65rem}.pwmfan-actions .btn{min-height:3.4rem;white-space:normal}.pwmfan-speed{border-color:#4c9ed9}.pwmfan-note{display:block;margin-top:.9rem;opacity:.72}',
				'@media(max-width:700px){.pwmfan-grid{grid-template-columns:repeat(2,1fr)}.pwmfan-actions{grid-template-columns:1fr}.pwmfan-curve{grid-template-columns:1fr}.pwmfan-head{align-items:flex-start;gap:1rem}}'
			].join('')),
			E('div', { 'class': 'pwmfan-head' }, [
				E('h2', {}, _('PWM Fan Control')),
				E('span', { 'class': 'pwmfan-live' }, _('Live status'))
			]),
			E('section', { 'class': 'pwmfan-panel' }, [
				E('h3', {}, _('Temperature / Fan Overview')),
				E('div', { 'class': 'pwmfan-temp-line' }, [ E('span', {}, _('Current maximum temperature')), E('strong', { 'id': 'pwmfan-temp' }) ]),
				E('div', { 'class': 'pwmfan-gauge' }, E('span', { 'id': 'pwmfan-bar' })),
				E('div', { 'class': 'pwmfan-scale' }, [ E('span', {}, '35°C ' + _('Low')), E('span', {}, '60°C'), E('span', {}, '75°C'), E('span', {}, '90°C ' + _('High')) ]),
				E('div', { 'class': 'pwmfan-grid' }, [
					E('div', { 'class': 'pwmfan-metric' }, [ E('small', {}, _('PWM / speed')), E('strong', { 'id': 'pwmfan-value' }) ]),
					E('div', { 'class': 'pwmfan-metric' }, [ E('small', {}, _('Target PWM level')), E('strong', { 'id': 'pwmfan-target' }) ]),
					E('div', { 'class': 'pwmfan-metric' }, [ E('small', {}, _('Thermal state')), E('strong', { 'id': 'pwmfan-state' }) ]),
					E('div', { 'class': 'pwmfan-metric' }, [ E('small', {}, _('Fan status')), E('strong', { 'id': 'pwmfan-status' }) ])
					,E('div', { 'class': 'pwmfan-metric' }, [ E('small', {}, _('Fan speed')), E('strong', { 'id': 'pwmfan-rpm' }) ])
					,E('div', { 'class': 'pwmfan-metric' }, [ E('small', {}, _('PWM channels')), E('strong', { 'id': 'pwmfan-channels' }) ])
				])
			]),
			E('section', { 'class': 'pwmfan-panel' }, [
				E('h3', {}, _('Automatic Temperature Curve')),
				E('div', { 'class': 'pwmfan-curve' }, [
					[ '<60°C', 'PWM 125' ], [ '60–65°C', 'PWM 125' ], [ '65–70°C', 'PWM 170' ], [ '70–75°C', 'PWM 200' ], [ '75–80°C', 'PWM 220' ], [ '80–85°C', 'PWM 240' ], [ '≥85°C', 'PWM 255' ]
				].map(function(step) { return E('div', { 'class': 'pwmfan-step' }, [ E('strong', {}, step[0]), E('small', {}, step[1]) ]); }))
			]),
			E('section', { 'class': 'pwmfan-panel' }, [
				E('h3', {}, _('Controls')),
				E('div', { 'class': 'pwmfan-actions' }, buttons),
				E('small', { 'class': 'pwmfan-note' }, _('Manual speed remains active for 60 seconds; Apply temperature profile restores automatic control immediately.'))
			]),
			E('section', { 'class': 'pwmfan-panel' }, [
				E('h3', {}, _('Automatic detection')),
				E('div', { 'class': 'pwmfan-metric' }, [ E('small', {}, _('Controller state / active route / polarity')), E('strong', { 'id': 'pwmfan-controller' }, _('Detecting')) ])
			])
		]);

		requestAnimationFrame(L.bind(this.update, this, data));
		return view;
	},

});
