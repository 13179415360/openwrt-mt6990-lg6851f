'use strict';
'require view';
'require rpc';
'require ui';
'require modem5g.common_v4 as common';

const listSms = rpc.declare({ object: 'luci.modem5g', method: 'list_sms', expect: {} });
const getSmsc = rpc.declare({ object: 'luci.modem5g', method: 'get_smsc', expect: {} });
const sendSms = rpc.declare({ object: 'luci.modem5g', method: 'send_sms', params: [ 'number', 'text' ] });
const deleteSms = rpc.declare({ object: 'luci.modem5g', method: 'delete_sms', params: [ 'index' ] });
const setSmsc = rpc.declare({ object: 'luci.modem5g', method: 'set_smsc', params: [ 'number' ] });

function decodeUcs2(hex) {
	if (!hex || hex.length % 4) return hex || '';
	let out = '';
	for (let i = 0; i < hex.length; i += 4)
		out += String.fromCharCode(parseInt(hex.slice(i, i + 4), 16));
	return out;
}

function readableSms(raw) {
	const records = [], groups = {};
	const re = /(?:\[(\d+)\][\s\S]*?\+CMGR:\s*|\+CMGL:\s*(\d+),)"([^"]*)","([^"]*)","[^"]*","([^"]*)"\s*\r?\n([0-9A-F]+)(?=\r?\n(?:\+CMGL:|\r?\nOK|$)|$)/gi;
	let m;
	while ((m = re.exec(raw || '')) !== null) {
		let hex = m[6], multipart8 = hex.match(/^050003([0-9A-F]{2})([0-9A-F]{2})([0-9A-F]{2})/i);
		let multipart16 = hex.match(/^060804([0-9A-F]{4})([0-9A-F]{2})([0-9A-F]{2})/i);
		let multipart = multipart16 || multipart8;
		let rec = { index: +(m[1] || m[2]), state: m[3], sender: m[4], time: m[5], text: '', ref: '', total: 1, part: 1 };
		if (multipart) {
			rec.ref = multipart[1].toUpperCase(); rec.total = parseInt(multipart[2], 16); rec.part = parseInt(multipart[3], 16);
			hex = hex.slice(multipart16 ? 14 : 12);
		}
		rec.text = decodeUcs2(hex);
		records.push(rec);
	}
	for (const rec of records) {
		let key = rec.ref ? `${rec.sender}|${rec.time}|${rec.ref}` : `index:${rec.index}`;
		if (!groups[key]) groups[key] = [];
		groups[key].push(rec);
	}
	let rendered = Object.values(groups).map(parts => {
		parts.sort((a, b) => a.part - b.part);
		let first = parts[0], indexes = parts.map(p => p.index).join(','), text = parts.map(p => p.text).join('');
		let incomplete = first.total > parts.length ? ` (${_('incomplete')}: ${parts.length}/${first.total})` : '';
		return `${_('Index')}: ${indexes}${incomplete}\n${_('From')}: ${first.sender}\n${_('Time')}: ${first.time}\n${_('Status')}: ${first.state}\n${text}`;
	});
	return rendered.length ? rendered.join('\n\n────────────────────────\n\n') : (raw || _('No messages'));
}

function smscNumber(result) {
	let m = common.resultText(result).match(/\+CSCA:\s*"([^"]+)"/);
	return m ? m[1] : '';
}

return view.extend({
	/* atcid is a single control channel.  Do not issue CMGL and CSCA in
	 * parallel while the page is loading. */
	load() { return listSms().then(messages => getSmsc().then(smsc => [ messages, smsc ])); },
	show(id, result) { document.getElementById(id).textContent = common.resultText(result); },
	handleRefresh() { return listSms().then(r => { document.getElementById('m5-sms-list').textContent = readableSms(r.output); }); },
	handleSend() { return sendSms(document.getElementById('m5-sms-number').value, document.getElementById('m5-sms-text').value).then(r => { this.show('m5-sms-result', r); if (r.code) throw new Error(common.resultText(r)); ui.addNotification(null, E('p', {}, _('SMS request completed.'))); }); },
	handleDelete() { return deleteSms(+document.getElementById('m5-sms-index').value).then(r => { this.show('m5-sms-result', r); return this.handleRefresh(); }); },
	handleSmsc() { return setSmsc(document.getElementById('m5-smsc').value).then(r => this.show('m5-sms-result', r)); },
	render(data) { return E('div', { 'class': 'm5-page' }, [ common.style(), E('h2', {}, _('5G Modem SMS')),
		common.panel(_('Inbox / stored messages'), E('div', {}, [ E('div', { 'class': 'm5-actions' }, E('button', { 'class': 'btn cbi-button cbi-button-action', click: ui.createHandlerFn(this, 'handleRefresh') }, _('Refresh'))), E('pre', { id: 'm5-sms-list', 'class': 'm5-raw' }, readableSms(data[0]?.output)) ])),
		common.panel(_('Send SMS'), E('div', { 'class': 'm5-form' }, [ E('label', {}, _('Recipient number')), E('input', { id: 'm5-sms-number', type: 'tel', placeholder: '+8613800138000' }), E('label', {}, _('Message (GSM/ASCII, max 160)')), E('textarea', { id: 'm5-sms-text', rows: 4, maxlength: 160 }), E('span'), E('button', { 'class': 'btn cbi-button cbi-button-positive', click: ui.createHandlerFn(this, 'handleSend') }, _('Send')) ])),
		common.panel(_('Delete / SMS center'), E('div', { 'class': 'm5-form' }, [ E('label', {}, _('Message index')), E('div', { 'class': 'm5-actions' }, [ E('input', { id: 'm5-sms-index', type: 'number', min: 0 }), E('button', { 'class': 'btn cbi-button cbi-button-negative', click: ui.createHandlerFn(this, 'handleDelete') }, _('Delete')) ]), E('label', {}, _('SMS center number')), E('div', { 'class': 'm5-actions' }, [ E('input', { id: 'm5-smsc', value: smscNumber(data[1]) }), E('button', { 'class': 'btn cbi-button cbi-button-apply', click: ui.createHandlerFn(this, 'handleSmsc') }, _('Set SMSC')) ]) ])),
		common.panel(_('Operation result'), E('pre', { id: 'm5-sms-result', 'class': 'm5-raw' }, '-'))
	]); }, handleSaveApply: null, handleSave: null, handleReset: null
});
