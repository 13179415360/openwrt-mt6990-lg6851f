'use strict';
/* v6: decode MT6990 ECELL NR signal units and use one serving-cell sample. */
'require view';
'require rpc';
'require poll';
'require modem5g.common_v4 as common';

const callStatus = rpc.declare({ object: 'luci.modem5g', method: 'get_status', expect: {} });
const callCells = rpc.declare({ object: 'luci.modem5g', method: 'get_cells', expect: {} });
const callDeviceStats = rpc.declare({ object: 'luci-rpc', method: 'getNetworkDevices', expect: { '': {} } });

function netValue(n, key) {
	let a = n?.[key];
	return Array.isArray(a) ? a.map(x => x.address ? `${x.address}/${x.mask}` : String(x)).join('\n') : (a || '-');
}

function csv(text) {
	let out = [], value = '', quoted = false;
	for (let c of text) {
		if (c == '"') quoted = !quoted;
		else if (c == ',' && !quoted) { out.push(value); value = ''; }
		else value += c;
	}
	out.push(value);
	return out;
}

function bandFor(arfcn, rat) {
	if (rat == 11) {
		if (arfcn >= 499200 && arfcn <= 537999) return 'n41';
		if (arfcn >= 620000 && arfcn <= 653333) return 'n77 / n78';
		if (arfcn >= 620000 && arfcn <= 680000) return 'n77';
		return 'NR';
	}
	return 'LTE';
}

function decode(value, kind, rat) {
	let n = +value;
	if (!Number.isFinite(n) || n == 255) return null;
	if (kind == 'rsrp') return n - (rat == 11 ? 156 : 140);
	if (kind == 'rsrq') return n / 2 - 43;
	/* MT6990 ECELL uses the same 0.5 dB SINR step as its extended CESQ. */
	return n / 2 - 23;
}

function parseCells(result) {
	let raw = common.resultText(result), match = raw.match(/\+ECELL:\s*([^\r\n]+)/);
	if (!match) return [];
	let fields = csv(match[1]), count = +fields.shift(), rows = [];
	for (let i = 0; i < count && fields.length >= 15; i++) {
		let f = fields.splice(0, 15), arfcn = +f[14], rat = +f[0];
		rows.push({ serving: i == 0, rat, band: bandFor(arfcn, rat), arfcn, pci: +f[5], rsrp: decode(f[6], 'rsrp', rat), rsrq: decode(f[7], 'rsrq', rat), sinr: decode(f[11], 'sinr', rat) });
	}
	return rows.filter(r => r.arfcn > 0 && r.pci >= 0);
}

function statusSignal(data, key) {
	let m = (data.cesq || '').match(new RegExp(`${key}=(-?[0-9.]+)`));
	return m ? m[1] : null;
}

function signalText(data, key, unit) {
	let value = statusSignal(data, key);
	return value == null ? _('Unavailable') : `${value} ${unit}`;
}

function servingSignal(data, rows, key, unit) {
	let serving = rows?.find(r => r.serving), field = key.toLowerCase();
	let value = serving?.[field];
	return value == null ? signalText(data, key, unit) : `${value} ${unit}`;
}

function qualityClass(value, good, mid) {
	if (value == null) return '';
	value = +value;
	return value >= good ? 'm5-signal-good' : value >= mid ? 'm5-signal-mid' : 'm5-signal-poor';
}

let lastTraffic = null;

function speed(bytes) {
	let mbps = Math.max(0, bytes || 0) * 8 / 1000000;
	return `${mbps < 10 ? mbps.toFixed(1) : Math.round(mbps)} Mbps`;
}

function gaugePercent(bytes) {
	/* A logarithmic 0-1000 Mbps scale keeps ordinary traffic visibly alive
	 * without making a short cellular speed burst pin the dial at 100%. */
	let mbps = Math.max(0, bytes || 0) * 8 / 1000000;
	return Math.min(100, Math.log1p(mbps) / Math.log1p(1000) * 100);
}

function updateTraffic(devices) {
	let stat = devices?.ccmni2?.stats, now = Date.now() / 1000, rx = +stat?.rx_bytes, tx = +stat?.tx_bytes;
	if (!Number.isFinite(rx) || !Number.isFinite(tx)) return;
	let down = 0, up = 0;
	if (lastTraffic && now > lastTraffic.time) {
		down = Math.max(0, (rx - lastTraffic.rx) / (now - lastTraffic.time));
		up = Math.max(0, (tx - lastTraffic.tx) / (now - lastTraffic.time));
	}
	lastTraffic = { time: now, rx, tx };
	for (let item of [ [ 'm5-down', down ], [ 'm5-up', up ] ]) {
		let gauge = document.getElementById(`${item[0]}-gauge`), label = document.getElementById(`${item[0]}-value`);
		let progress = gauge?.querySelector('.m5-gauge-progress');
		if (progress) progress.style.setProperty('--gauge-angle', `${gaugePercent(item[1]) * 2.7}deg`);
		if (label) label.textContent = speed(item[1]);
	}
}

function gauge(id, title, direction) {
	return E('div', { 'class': `m5-gauge m5-gauge-${direction}`, id: `${id}-gauge` }, [
		E('span', { 'class': 'm5-gauge-progress' }), E('div', { 'class': 'm5-gauge-face' }, [
		E('span', { 'class': 'm5-gauge-icon' }, direction == 'down' ? '↓' : '↑'),
		E('strong', { id: `${id}-value` }, '0.0 Mbps'), E('small', {}, title)
	]) ]);
}

function cellTable(rows) {
	if (!rows.length) return E('p', { 'class': 'm5-note' }, _('No cell measurements are currently available.'));
	return E('div', { 'style': 'overflow-x:auto' }, E('table', { 'class': 'm5-cell-table' }, [
		E('thead', {}, E('tr', {}, [ _('Type'), _('Band'), _('ARFCN'), _('PCI'), _('RSRP'), _('RSRQ'), _('SINR') ].map(x => E('th', {}, x)))),
		E('tbody', {}, rows.map(r => E('tr', { 'class': r.serving ? 'm5-serving' : '' }, [
			E('td', {}, r.serving ? _('Serving') : _('Neighbor')), E('td', {}, r.band), E('td', {}, r.arfcn), E('td', {}, r.pci),
			E('td', { 'class': qualityClass(r.rsrp, -85, -100) }, r.rsrp == null ? '-' : `${r.rsrp} dBm`),
			E('td', { 'class': qualityClass(r.rsrq, -10, -15) }, r.rsrq == null ? '-' : `${r.rsrq} dB`),
			E('td', { 'class': qualityClass(r.sinr, 15, 0) }, r.sinr == null ? '-' : `${r.sinr} dB`)
		])))
	]));
}

function callOverview() { return Promise.all([ callStatus(), callCells() ]); }

return view.extend({
	load() { return callOverview(); },
	update(data, cells) {
		if (!data?.available) return;
		let rows = parseCells(cells?.serving);
		let values = {
			'm5-rat': data.rat, 'm5-operator': data.operator, 'm5-registration': data.registration,
			'm5-rsrp': servingSignal(data, rows, 'RSRP', 'dBm'), 'm5-rsrq': servingSignal(data, rows, 'RSRQ', 'dB'),
			'm5-sinr': servingSignal(data, rows, 'SINR', 'dB'),
			'm5-sim': data.sim, 'm5-imei': data.imei, 'm5-iccid': data.iccid, 'm5-device': data.network?.l3_device,
			'm5-ip4': netValue(data.network, 'ipv4-address'), 'm5-ip6': netValue(data.network, 'ipv6-address')
		};
		Object.keys(values).forEach(id => { let el = document.getElementById(id); if (el) el.textContent = values[id] || '-'; });
		let state = document.getElementById('m5-live-state');
		if (state) state.textContent = data.initializing ? _('Waiting for modem startup to settle') : _('Live · refresh every 30 seconds');
		let table = document.getElementById('m5-cell-wrap');
		if (table) table.replaceChildren(cellTable(rows));
		let raw = document.getElementById('m5-cell-raw');
		if (raw) raw.textContent = `${common.resultText(cells?.serving)}\n\n${common.resultText(cells?.engineering)}`;
	},
	render(result) {
		try { CSS.registerProperty({ name: '--gauge-angle', syntax: '<angle>', inherits: false, initialValue: '0deg' }); } catch (e) {}
		let data = result[0] || {}, cells = result[1] || {}, rows = parseCells(cells.serving);
		let page = E('div', { 'class': 'm5-page', 'style': 'padding-bottom:5rem' }, [ common.style(), common.extraStyle(),
			E('div', { 'class': 'm5-head' }, [ E('h2', {}, _('5G Modem Status')), E('span', { id: 'm5-live-state', 'class': 'm5-live' }, _('Live · refresh every 30 seconds')) ]),
			E('div', { 'class': 'm5-dashboard' }, [ common.panel(_('Mobile connection'), E('div', { 'class': 'm5-hero' }, [
				common.metric(_('Network'), data.rat, 'm5-rat'), common.metric(_('Operator'), data.operator, 'm5-operator'),
				common.metric(_('Registration'), data.registration, 'm5-registration'), common.metric(_('Interface state'), data.network?.up ? _('Connected') : _('Disconnected')),
				common.metric(_('Data interface'), data.network?.l3_device, 'm5-device'), common.metric(_('IPv4'), netValue(data.network, 'ipv4-address'), 'm5-ip4'), common.metric(_('IPv6'), netValue(data.network, 'ipv6-address'), 'm5-ip6')
			])), common.panel(_('Live traffic'), E('div', { 'class': 'm5-gauges' }, [ gauge('m5-down', _('Download speed'), 'down'), gauge('m5-up', _('Upload speed'), 'up') ])) ]),
			common.panel(_('Signal quality'), E('div', {}, [
				E('div', { 'class': 'm5-grid', 'style': 'grid-template-columns:repeat(auto-fit,minmax(12rem,1fr))' }, [
					common.metric('RSRP', servingSignal(data, rows, 'RSRP', 'dBm'), 'm5-rsrp'), common.metric('RSRQ', servingSignal(data, rows, 'RSRQ', 'dB'), 'm5-rsrq'),
					common.metric('SINR', servingSignal(data, rows, 'SINR', 'dB'), 'm5-sinr')
				]), E('p', { 'class': 'm5-note' }, _('Serving-cell measurements from the same AT+ECELL sample as the table below.'))
			])),
			common.panel(_('Module and SIM'), E('div', { 'class': 'm5-grid' }, [
				common.metric(_('SIM status'), data.sim, 'm5-sim'), common.metric(_('IMEI'), data.imei, 'm5-imei'),
				common.metric(_('ICCID'), data.iccid, 'm5-iccid'), common.metric(_('IMSI'), data.imsi)
			])),
			common.panel(`${_('Serving and neighbor cells')} (${rows.length})`, E('div', { id: 'm5-cell-wrap' }, cellTable(rows))),
			E('details', { 'class': 'm5-panel' }, [ E('summary', {}, _('Engineering diagnostics')), E('pre', { id: 'm5-cell-raw', 'class': 'm5-raw' }, `${common.resultText(cells.serving)}\n\n${common.resultText(cells.engineering)}`) ])
		]);
		poll.add(() => callOverview().then(r => this.update(r[0], r[1])), 30);
		poll.add(() => callDeviceStats().then(updateTraffic), 5);
		requestAnimationFrame(L.bind(this.update, this, data, cells));
		return page;
	},
	handleSaveApply: null, handleSave: null, handleReset: null
});
