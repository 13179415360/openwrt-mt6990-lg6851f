'use strict';
'require view';
'require view.pwmfan.control_v3 as pwmfan';

return view.extend({
	title: _('PWM Fan Control'),

	load() {
		return pwmfan.load();
	},

	update(data) {
		return pwmfan.update(data);
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
