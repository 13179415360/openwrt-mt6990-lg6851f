'use strict';
'require view';
'require view.pwmfan.control_v4 as pwmfan';

return view.extend({
	title: _('PWM Fan Control'),

	load() {
		return pwmfan.load();
	},

	syncTheme(page) {
		return pwmfan.syncTheme(page);
	},

	update(data) {
		const cpuTemperatures = (data.temperatures || []).filter(function(sensor) {
			return /^cpu_little[0-9]+$/.test(sensor.name) && Number.isFinite(sensor.temp);
		});

		if (cpuTemperatures.length)
			data.hottest = Math.max.apply(null, cpuTemperatures.map(function(sensor) { return sensor.temp; }));

		const result = pwmfan.update(data);
		const sensorDetails = document.getElementById('pwmfan-sensors');
		if (sensorDetails)
			sensorDetails.parentNode.remove();

		const label = document.querySelector('.pwmfan-temp-line span');
		if (label)
			label.textContent = _('CPU temperature');

		return result;
	},

	handleSet(value, ev) {
		return pwmfan.handleSet.call(this, value, ev);
	},

	handleAuto(ev) {
		return pwmfan.handleAuto.call(this, ev);
	},

	render(data) {
		return pwmfan.render.call(this, data);
	},

	handleSaveApply: null,
	handleSave: null,
	handleReset: null
});
