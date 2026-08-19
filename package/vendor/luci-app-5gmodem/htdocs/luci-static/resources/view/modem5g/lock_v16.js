'use strict';
/* v16: decode NR ECELL RSRP with the MT6990 3GPP offset. */
'require view';
'require rpc';
'require ui';
'require modem5g.common_v4 as common';

const getLocks = rpc.declare({ object: 'luci.modem5g', method: 'get_locks', expect: {} });
const getLockRequest = rpc.declare({ object: 'luci.modem5g', method: 'get_lock_request', expect: {} });
const getCells = rpc.declare({ object: 'luci.modem5g', method: 'get_cells', expect: {} });
const setBands = rpc.declare({ object: 'luci.modem5g', method: 'set_band_lock', params: [ 'lte', 'nr' ] });
const resetBands = rpc.declare({ object: 'luci.modem5g', method: 'reset_band_lock' });
const setCell = rpc.declare({ object: 'luci.modem5g', method: 'set_cell_lock', params: [ 'arfcn', 'pci', 'rat', 'band' ] });
const clearCell = rpc.declare({ object: 'luci.modem5g', method: 'clear_cell_lock' });

const LTE = [ 1, 3, 5, 7, 8, 20, 28, 32, 38, 40, 41, 42, 43 ];
const NR = [ 1, 3, 5, 7, 8, 20, 28, 38, 40, 41, 77, 78 ];

function checks(id, prefix, values) {
	return E('div', { id: id, 'class': 'm5-band-grid' }, values.map(v =>
		E('label', { 'class': 'm5-band' }, [ E('input', { type: 'checkbox', value: v }), E('span', {}, `${prefix}${v}`) ])));
}

function selected(id) {
	return Array.from(document.querySelectorAll(`#${id} input:checked`)).map(el => +el.value);
}

function setChecked(id, values) {
	let wanted = new Set(values.map(Number));
	document.querySelectorAll(`#${id} input`).forEach(el => { el.checked = wanted.has(+el.value); });
}

function sameBands(actual, wanted) {
	return actual.length == wanted.length && actual.every((v, i) => v == wanted[i]);
}

function maskBands(hex, allowed) {
	let chunks = (hex || '').match(/[0-9a-fA-F]{8}/g) || [];
	return allowed.filter(band => {
		let bit = band - 1, chunk = chunks[Math.floor(bit / 32)];
		return chunk && (parseInt(chunk, 16) & Math.pow(2, bit % 32)) != 0;
	});
}

function parsedLockState(result) {
	let raw = common.resultText(result), lte = raw.match(/lte band info\s*=\s*([0-9a-f]+)/i), nr = raw.match(/nr band info\s*=\s*([0-9a-f]+)/i);
	let state = { raw, lte: lte ? maskBands(lte[1], LTE) : [], nr: nr ? maskBands(nr[1], NR) : [] };
	let saved = raw.match(/SAVED_LOCK_STATE[\s\S]*?MODE=([^\r\n]+)/);
	state.mode = saved ? saved[1].trim() : 'unknown';
	for (let key of [ 'STATUS', 'RAT', 'ARFCN', 'PCI', 'BAND' ]) {
		let m = raw.match(new RegExp(`^${key}=([^\\r\\n]+)`, 'm'));
		if (m) state[key.toLowerCase()] = m[1].trim();
	}
	if (!state.lte.length) {
		let requested = raw.match(/^LTE=([0-9,]+)$/m);
		if (requested) state.lte = requested[1].split(',').map(Number);
	}
	if (!state.nr.length) {
		let requested = raw.match(/^NR=([0-9,]+)$/m);
		if (requested) state.nr = requested[1].split(',').map(Number);
	}
	return state;
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

function parseCells(result) {
	let raw = common.resultText(result), match = raw.match(/\+ECELL:\s*([^\r\n]+)/);
	if (!match) return [];
	let fields = csv(match[1]), count = +fields.shift(), rows = [];
	for (let i = 0; i < count && fields.length >= 15; i++) {
		let f = fields.splice(0, 15), arfcn = +f[14], rat = +f[0], rsrp = +f[6], rsrq = +f[7], sinr = +f[11];
		let bandNo = rat == 11 ? (arfcn >= 499200 && arfcn <= 537999 ? 41 : arfcn >= 620000 && arfcn <= 653333 ? 78 : 0) : 0;
		let band = rat == 11 ? (bandNo ? 'n' + bandNo : 'NR') : 'LTE';
		if (arfcn > 0 && +f[5] >= 0) rows.push({
			serving: i == 0, rat: rat == 11 ? 'nr' : 'lte', bandNo, band, arfcn, pci: +f[5],
			rsrp: rsrp == 255 ? null : rsrp - (rat == 11 ? 156 : 140),
			rsrq: rsrq == 255 ? null : rsrq / 2 - 43,
			sinr: sinr == 255 ? null : sinr / 2 - 23
		});
	}
	return rows;
}

function candidateTable(rows) {
	if (!rows.length) return E('p', { 'class': 'm5-note' }, _('No cell measurements are currently available.'));
	return E('div', { 'style': 'overflow-x:auto' }, E('table', { 'class': 'm5-cell-table' }, [
		E('thead', {}, E('tr', {}, [ _('Type'), _('Band'), _('ARFCN'), _('PCI'), _('RSRP'), _('RSRQ'), _('SINR') ].map(x => E('th', {}, x)))),
		E('tbody', {}, rows.map(r => E('tr', { 'class': r.serving ? 'm5-serving' : '', click: function() {
			document.getElementById('m5-lock-arfcn').value = r.arfcn;
			document.getElementById('m5-lock-pci').value = r.pci;
			document.getElementById('m5-lock-rat').value = r.rat;
			document.getElementById('m5-lock-rat').dispatchEvent(new Event('change'));
			if (r.bandNo) document.getElementById('m5-lock-band').value = r.bandNo;
			document.getElementById('m5-lock-result').textContent =
				_('Cell selected: %s / PCI %s. It has not been applied yet.').format(r.arfcn, r.pci);
		} }, [
			E('td', {}, r.serving ? _('Serving') : _('Neighbor')), E('td', {}, r.band), E('td', {}, r.arfcn), E('td', {}, r.pci),
			E('td', {}, r.rsrp == null ? '-' : `${r.rsrp} dBm`), E('td', {}, r.rsrq == null ? '-' : `${r.rsrq} dB`),
			E('td', {}, r.sinr == null ? '-' : `${r.sinr} dB`)
		])))
	]));
}

return view.extend({
	busy: false,
	load() { return Promise.all([ getLocks(), getCells() ]); },
	setBusy(active) {
		this.busy = active;
		document.querySelectorAll('.m5-lock-action').forEach(el => { el.disabled = active; });
		let state = document.getElementById('m5-lock-progress');
		if (state) state.textContent = active ? _('Modem operation in progress; please wait…') : _('Ready');
	},
	showResult(result, sync, verify, refresh) {
		document.getElementById('m5-lock-result').textContent = common.resultText(result);
		if (!result || +result.code != 0)
			throw new Error(common.resultText(result));
		return (refresh || getLocks)().then(state => {
			document.getElementById('m5-lock-state').textContent = common.resultText(state);
			this.syncLocks(state);
			if (verify && !verify(parsedLockState(state)))
				throw new Error(_('The modem did not report the requested lock state. The operation was not confirmed.'));
			sync?.();
			return state;
		});
	},
	waitForOperation(verify) {
		let seenActive = false, inactiveAfterActive = 0, attempts = 0;
		let poll = () => getLockRequest().then(state => {
			seenActive = seenActive || state.active === true || /OPERATION_ACTIVE/.test(common.resultText(state));
			if (seenActive && !state.active && (!verify || verify(parsedLockState(state))))
				return state;
			if (seenActive && !state.active && ++inactiveAfterActive >= 3)
				throw new Error(_('The modem finished the operation but did not report the requested lock state.'));
			if (state.active) inactiveAfterActive = 0;
			if (++attempts >= 60)
				throw new Error(_('The modem operation did not finish within the verification window.'));
			return new Promise(resolve => window.setTimeout(resolve, 2000)).then(poll);
		});
		return poll();
	},
	refreshCellsAfterOperation(attempt) {
		attempt = attempt || 0;
		return new Promise(resolve => window.setTimeout(resolve, attempt ? 5000 : 20000)).then(() => getCells()).then(result => {
			let rows = parseCells(result.serving);
			if (!rows.length && attempt < 6)
				return this.refreshCellsAfterOperation(attempt + 1);
			let target = document.getElementById('m5-cell-candidates');
			if (target && rows.length)
				target.replaceChildren(candidateTable(rows));
		}).catch(() => {
			if (attempt < 6) return this.refreshCellsAfterOperation(attempt + 1);
		});
	},
	syncCandidatesToVerifiedState(result) {
		let state = parsedLockState(result);
		let target = document.getElementById('m5-cell-candidates');
		if (!target) return;
		if (state.mode == 'cell') {
			let bandNo = +state.band || 0;
			target.replaceChildren(candidateTable([ {
				serving: true,
				rat: state.rat || 'nr',
				bandNo,
				band: `${state.rat == 'lte' ? 'B' : 'n'}${bandNo}`,
				arfcn: +state.arfcn,
				pci: +state.pci,
				rsrp: null
			} ]));
		} else if (state.mode == 'none') {
			target.replaceChildren(E('p', { 'class': 'm5-note' }, _('Mobile network re-registered; refreshing actual serving and neighbor cells…')));
		}
	},
	syncLocks(result) {
		let state = parsedLockState(result);
		setChecked('m5-lte-bands', state.lte);
		setChecked('m5-nr-bands', state.nr);
		let badge = document.getElementById('m5-lock-summary');
		if (badge) badge.textContent = state.mode == 'cell' ? `${_('Cell lock')}: ${String(state.rat || '').toUpperCase()} ${state.arfcn || '-'} / PCI ${state.pci == '65535' ? _('Any') : (state.pci || '-')}` :
			state.mode == 'none' ? _('Cell lock cleared and state refreshed') :
			state.mode == 'bands' ? _('Band lock applied and verified') :
			(state.lte.length || state.nr.length) ? `${_('Current bands')}: ${state.lte.map(x => 'B' + x).join(' ')} · ${state.nr.map(x => 'n' + x).join(' ')}` : _('Lock state unavailable');
		if (state.mode == 'cell') {
			document.getElementById('m5-lock-rat').value = state.rat || 'nr';
			document.getElementById('m5-lock-band').value = state.band || 0;
			document.getElementById('m5-lock-arfcn').value = state.arfcn || '';
			document.getElementById('m5-lock-pci').value = state.pci == '65535' ? '' : (state.pci || '');
		}
	},
	runAction(fn, args, sync, verify, refresh) {
		if (this.busy) return Promise.resolve();
		let oldRpcTimeout = L.env.rpctimeout;
		L.env.rpctimeout = 40;
		document.getElementById('m5-lock-result').textContent = _('Request sent; waiting for modem verification…');
		this.setBusy(true);
		let rpcCall = fn(...(args || [])).then(result => {
			if (!result || +result.code != 0) throw new Error(common.resultText(result));
			return result;
		});
		return Promise.all([ rpcCall, this.waitForOperation(verify) ]).then(results => {
			let state = results[1];
			document.getElementById('m5-lock-state').textContent = common.resultText(state);
			this.syncLocks(state);
			this.syncCandidatesToVerifiedState(state);
			sync?.();
			this.refreshCellsAfterOperation();
			return state;
		}).catch(e => {
			common.callError(_('Band and Cell Lock'), e);
		}).finally(() => { L.env.rpctimeout = oldRpcTimeout; this.setBusy(false); });
	},
	readLocks() {
		if (this.busy) return Promise.resolve();
		this.setBusy(true);
		return getLocks().then(state => {
			document.getElementById('m5-lock-state').textContent = common.resultText(state);
			this.syncLocks(state);
			document.getElementById('m5-lock-result').textContent = _('Lock state refreshed');
		}).catch(e => common.callError(_('Band and Cell Lock'), e)).finally(() => this.setBusy(false));
	},
	showPane(name) {
		document.querySelectorAll('.m5-lock-pane').forEach(el => { el.hidden = el.id != `m5-pane-${name}`; });
		document.querySelectorAll('.m5-tabs button').forEach(el => el.classList.toggle('active', el.dataset.pane == name));
	},
	applyBands() {
		let self = this;
		let lte = selected('m5-lte-bands'), nr = selected('m5-nr-bands');
		if (!lte.length || !nr.length)
			return ui.addNotification(null, E('p', {}, _('Select at least one LTE band and one NR band.')));
		return ui.showModal(_('Confirm band lock'), [
			E('p', {}, _('Applying a band lock interrupts mobile data while the modem re-registers. Band lock and cell lock must not be enabled at the same time.')),
			E('div', { 'class': 'right' }, [
				E('button', { 'class': 'btn', click: ui.hideModal }, _('Cancel')),
				E('button', { 'class': 'btn cbi-button-negative', click: function(ev) { ev.preventDefault(); ui.hideModal(); return self.runAction(setBands, [ lte, nr ], () => { setChecked('m5-lte-bands', lte); setChecked('m5-nr-bands', nr); document.getElementById('m5-lock-result').textContent = _('Band lock applied and verified'); }, state => state.mode == 'bands' && state.status == 'verified' && sameBands(state.lte, lte) && sameBands(state.nr, nr)); } }, _('Apply band lock'))
			])
		]);
	},
	applyCell() {
		let self = this;
		let arfcn = +document.getElementById('m5-lock-arfcn').value;
		let pci = document.getElementById('m5-lock-pci').value.trim();
		if (!Number.isInteger(arfcn) || arfcn < 1 || arfcn > 3279165 || (pci && (!/^\d+$/.test(pci) || +pci > 2000)))
			return ui.addNotification(null, E('p', {}, _('Enter a valid ARFCN and an optional PCI from 0 to 2000.')));
		let rat = document.getElementById('m5-lock-rat').value, band = +document.getElementById('m5-lock-band').value;
		if (!band) return ui.addNotification(null, E('p', {}, _('Select the radio type and band for this cell.')));
		return ui.showModal(_('Confirm cell lock'), [
			E('p', {}, _('A wrong cell lock can disconnect mobile data. Verify the ARFCN and PCI in the serving/neighbor list first.')),
			E('div', { 'class': 'right' }, [
				E('button', { 'class': 'btn', click: ui.hideModal }, _('Cancel')),
				E('button', { 'class': 'btn cbi-button-negative', click: function(ev) { ev.preventDefault(); ui.hideModal(); return self.runAction(setCell, [ arfcn, pci, rat, band ], () => { document.getElementById('m5-lock-result').textContent = _('Cell lock applied and verified'); }, state => state.mode == 'cell' && state.status == 'verified' && state.rat == rat && +state.arfcn == arfcn && +state.pci == +(pci || 65535) && +state.band == band, getLockRequest); } }, _('Apply cell lock'))
			])
		]);
	},
	render(data) {
		let locks = data[0] || {}, cells = data[1] || {}, candidates = parseCells(cells.serving);
		let page = E('div', { 'class': 'm5-page', 'style': 'padding-bottom:5rem' }, [ common.style(), common.extraStyle(), E('h2', {}, _('Band and Cell Lock')),
			E('p', { 'class': 'm5-note' }, _('This page uses the modem vendor RIL. Changes may interrupt service. Restore defaults if the modem cannot register.')),
			E('p', { id: 'm5-lock-summary', 'class': 'm5-live' }, _('Reading current lock state…')),
			E('p', { id: 'm5-lock-progress', 'class': 'm5-live' }, _('Ready')),
			E('div', { 'class': 'm5-tabs' }, [
				E('button', { 'class': 'btn active', 'data-pane': 'bands', click: ui.createHandlerFn(this, function() { this.showPane('bands'); }) }, _('Band lock')),
				E('button', { 'class': 'btn', 'data-pane': 'cell', click: ui.createHandlerFn(this, function() { this.showPane('cell'); }) }, _('Cell lock'))
			]),
			E('div', { id: 'm5-pane-bands', 'class': 'm5-lock-pane' }, common.panel(_('4G / 5G band lock'), E('div', {}, [
				E('h4', {}, _('LTE bands')), checks('m5-lte-bands', 'B', LTE),
				E('h4', {}, _('NR bands')), checks('m5-nr-bands', 'n', NR),
				E('div', { 'class': 'm5-lock-footer' }, [
					E('button', { 'class': 'btn cbi-button m5-lock-action', click: ui.createHandlerFn(this, 'readLocks') }, _('Read')),
					E('button', { 'class': 'btn cbi-button cbi-button-apply m5-lock-action', click: ui.createHandlerFn(this, 'applyBands') }, _('Apply band lock')),
					E('button', { 'class': 'btn cbi-button cbi-button-negative m5-lock-action', 'style': 'grid-column:1/-1', click: ui.createHandlerFn(this, function() { return this.runAction(resetBands, [], () => { setChecked('m5-lte-bands', LTE); setChecked('m5-nr-bands', NR); document.getElementById('m5-lock-result').textContent = _('All supported bands restored and state refreshed'); }, state => state.mode == 'none'); }) }, _('Clear band lock / restore defaults'))
				])
			]))),
			E('div', { id: 'm5-pane-cell', 'class': 'm5-lock-pane', hidden: true }, [
			common.panel(_('Cell lock'), E('div', { 'class': 'm5-form' }, [
				E('label', { 'for': 'm5-lock-rat' }, _('Radio type')), E('select', { id: 'm5-lock-rat', change: function() {
					let values = this.value == 'lte' ? LTE : NR, select = document.getElementById('m5-lock-band');
					select.replaceChildren(...values.map(v => E('option', { value: v }, (this.value == 'lte' ? 'B' : 'n') + v)));
				} }, [ E('option', { value: 'nr' }, '5G NR'), E('option', { value: 'lte' }, '4G LTE') ]),
				E('label', { 'for': 'm5-lock-band' }, _('Band')), E('select', { id: 'm5-lock-band' }, NR.map(v => E('option', { value: v }, 'n' + v))),
				E('label', { 'for': 'm5-lock-arfcn' }, _('Channel number (ARFCN)')), E('input', { id: 'm5-lock-arfcn', type: 'number', min: 1, max: 3279165, placeholder: _('Select a cell below or enter its ARFCN') }),
				E('label', { 'for': 'm5-lock-pci' }, _('Physical cell ID (PCI, optional)')), E('input', { id: 'm5-lock-pci', type: 'number', min: 0, max: 2000, placeholder: _('Select a cell below or enter its PCI') }),
				E('span'), E('div', { 'class': 'm5-lock-footer' }, [
					E('button', { 'class': 'btn cbi-button cbi-button-apply m5-lock-action', click: ui.createHandlerFn(this, 'applyCell') }, _('Apply cell lock')),
					E('button', { 'class': 'btn cbi-button cbi-button-negative m5-lock-action', click: ui.createHandlerFn(this, function() { return this.runAction(clearCell, [], () => { document.getElementById('m5-lock-arfcn').value = ''; document.getElementById('m5-lock-pci').value = ''; document.getElementById('m5-lock-result').textContent = _('Cell lock cleared and state refreshed'); }, state => state.mode == 'none', getLockRequest); }) }, _('Clear cell lock'))
				]), E('span')
			])),
				common.panel(_('Serving and neighbor cell candidates'), E('div', {}, [
					E('p', { 'class': 'm5-note' }, _('Tap a serving or neighbor cell to fill ARFCN and PCI automatically. ARFCN identifies the channel; PCI identifies the physical cell.')),
					E('div', { id: 'm5-cell-candidates' }, candidateTable(candidates))
				]))]),
			common.panel(_('Current lock state and operation result'), E('pre', { id: 'm5-lock-result', 'class': 'm5-raw' }, '-')),
			E('details', { 'class': 'm5-panel' }, [ E('summary', {}, _('Current lock state and operation result')), E('pre', { id: 'm5-lock-state', 'class': 'm5-raw' }, common.resultText(locks)) ])
		]);
		requestAnimationFrame(() => this.syncLocks(locks));
		return page;
	},
	handleSaveApply: null, handleSave: null, handleReset: null
});
