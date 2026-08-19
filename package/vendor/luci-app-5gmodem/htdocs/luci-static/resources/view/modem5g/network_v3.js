'use strict'; // v3: card summary with bottom-collapsed engineering details
'require view';
'require rpc';
'require ui';
'require modem5g.common_v4 as common';

const getNetwork = rpc.declare({ object: 'luci.modem5g', method: 'get_network', expect: {} });
const setRadio = rpc.declare({ object: 'luci.modem5g', method: 'set_radio', params: [ 'enabled' ] });
const setRat = rpc.declare({ object: 'luci.modem5g', method: 'set_rat', params: [ 'mode' ] });
const ifup = rpc.declare({ object: 'luci.modem5g', method: 'ifup' });
const ifdown = rpc.declare({ object: 'luci.modem5g', method: 'ifdown' });

function parsedNetwork(data) {
	let status = {};
	try { status = JSON.parse(data?.status || '{}'); } catch (e) {}
	let config = data?.config || '', value = key => {
		let m = config.match(new RegExp(`network\\.modem5g\\.${key}='([^']*)'`));
		return m ? m[1] : '-';
	};
	let address = status['ipv4-address']?.[0]?.address || '-', gateway = status.route?.find(r => r.mask == 0)?.nexthop || '-';
	let uptime = +status.uptime || 0, elapsed = uptime >= 3600 ? `${Math.floor(uptime / 3600)}h ${Math.floor(uptime % 3600 / 60)}m` : `${Math.floor(uptime / 60)}m ${uptime % 60}s`;
	return { status, config, value, address, gateway, elapsed };
}

function networkState(data) {
	let n = parsedNetwork(data), up = n.status.up === true;
	return E('div', {}, [ E('div', { 'class': 'm5-hero m5-net-hero' }, [
			common.metric(_('Connection'), up ? _('Online') : _('Offline')),
			common.metric(_('Interface'), n.status.l3_device || 'ccmni2'),
			common.metric(_('IPv4 address'), n.address),
			common.metric(_('Uptime'), n.elapsed),
			common.metric(_('Protocol'), n.status.proto || '-'),
			common.metric(_('Gateway'), n.gateway),
			common.metric(_('APN'), n.value('apn')),
			common.metric(_('IP type'), n.value('iptype'))
		]), E('details', { 'class': 'm5-net-details' }, [
			E('summary', {}, _('Engineering information')),
			E('div', { id: 'm5-net-engineering' }, engineeringInfo(data))
		]) ]);
}

function engineeringInfo(data) {
	let n = parsedNetwork(data);
	return E('div', {}, [
		E('pre', { 'class': 'm5-raw' }, data?.status || '-'),
		E('p', { 'class': 'm5-note' }, n.config || '-')
	]);
}

return view.extend({
	load() { return getNetwork(); },
	result(r) { document.getElementById('m5-net-result').textContent = common.resultText(r); return getNetwork().then(n => { document.getElementById('m5-net-state').replaceChildren(networkState(n)); document.getElementById('m5-net-engineering').replaceChildren(engineeringInfo(n)); }); },
	handleCall(fn, value) { return fn(value).then(L.bind(this.result, this)).catch(e => common.callError(_('5G Modem'), e)); },
	render(data) { return E('div', { 'class': 'm5-page' }, [ common.style(), common.extraStyle(), E('style', {}, '.m5-net-hero{grid-template-columns:repeat(4,minmax(0,1fr))}.m5-net-hero .m5-metric:first-child{grid-row:auto;grid-column:auto}.m5-net-details{margin-top:1rem}.m5-net-details summary{cursor:pointer;color:#2587c8;font-weight:600}.m5-net-actions .btn{flex:1;min-width:10rem;padding:.7rem}.m5-net-mode{max-width:none;grid-template-columns:10rem minmax(16rem,1fr) auto}.m5-net-mode .btn{min-width:10rem;padding:.65rem 1rem}@media(max-width:800px){.m5-net-hero{grid-template-columns:repeat(2,minmax(0,1fr))}.m5-net-mode{grid-template-columns:1fr}.m5-net-actions .btn{min-width:calc(50% - .55rem)}}@media(max-width:480px){.m5-net-hero{grid-template-columns:1fr}.m5-net-actions .btn{min-width:100%}}'), E('h2', {}, _('Dial-up and Network')),
		common.panel(_('Current modem5g interface'), E('div', { id: 'm5-net-state' }, networkState(data))),
		common.panel(_('Connection control'), E('div', { 'class': 'm5-actions m5-net-actions' }, [
			E('button', { 'class': 'btn cbi-button cbi-button-positive', click: ui.createHandlerFn(this, 'handleCall', ifup) }, _('Connect / ifup')),
			E('button', { 'class': 'btn cbi-button cbi-button-negative', click: ui.createHandlerFn(this, 'handleCall', ifdown) }, _('Disconnect / ifdown')),
			E('button', { 'class': 'btn cbi-button cbi-button-action', click: ui.createHandlerFn(this, 'handleCall', setRadio, true) }, _('Radio on')),
			E('button', { 'class': 'btn cbi-button', click: ui.createHandlerFn(this, 'handleCall', setRadio, false) }, _('Radio off'))
		])),
		common.panel(_('Preferred network mode'), E('div', { 'class': 'm5-form m5-net-mode' }, [ E('label', {}, _('RAT mode')), E('select', { id: 'm5-rat-mode' }, [ E('option', { value: 21 }, _('3G / 4G / 5G automatic')), E('option', { value: 19 }, _('4G / 5G automatic')), E('option', { value: 15 }, _('5G only')), E('option', { value: 3 }, _('4G only')) ]), E('button', { 'class': 'btn cbi-button cbi-button-apply', click: ui.createHandlerFn(this, function() { return this.handleCall(setRat, +document.getElementById('m5-rat-mode').value); }) }, _('Apply mode')) ])),
		common.panel(_('Operation result'), E('pre', { id: 'm5-net-result', 'class': 'm5-raw' }, `${data.radio || ''}\n${data.rat || ''}`)),
		E('p', { 'class': 'm5-note' }, _('APN, IP type, authentication, MTU and retry settings remain integrated with LuCI Network → modem5g, preventing two pages from writing conflicting configurations.'))
	]); }, handleSaveApply: null, handleSave: null, handleReset: null
});
