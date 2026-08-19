'use strict';
'require view';
'require rpc';
'require ui';
'require modem5g.common_v4 as common';

const runAt = rpc.declare({ object: 'luci.modem5g', method: 'run_at', params: [ 'command' ] });

return view.extend({
	handleRun() {
		let command = document.getElementById('m5-at-command').value.trim();
		return runAt(command).then(r => {
			document.getElementById('m5-at-output').textContent = `> ${command}\n${common.resultText(r)}\n\n` + document.getElementById('m5-at-output').textContent;
			if (r.code) throw new Error(common.resultText(r));
		}).catch(e => common.callError(_('AT terminal'), e));
	},
	render() { return E('div', { 'class': 'm5-page' }, [ common.style(), E('h2', {}, _('AT Debug Terminal')),
		E('div', { 'class': 'alert-message warning' }, _('AT commands can interrupt registration, calls or data service. The terminal is available only to users with write permission and rejects control characters and shell syntax.')),
		common.panel(_('Command'), E('div', { 'class': 'm5-form' }, [ E('label', {}, _('AT command')), E('input', { id: 'm5-at-command', value: 'AT+CESQ', maxlength: 512, keydown: ev => { if (ev.key === 'Enter') this.handleRun(); } }), E('span'), E('button', { 'class': 'btn cbi-button cbi-button-action', click: ui.createHandlerFn(this, 'handleRun') }, _('Execute')) ])),
		common.panel(_('Terminal output'), E('pre', { id: 'm5-at-output', 'class': 'm5-raw' }, _('Ready.')))
	]); }, handleSaveApply: null, handleSave: null, handleReset: null
});
