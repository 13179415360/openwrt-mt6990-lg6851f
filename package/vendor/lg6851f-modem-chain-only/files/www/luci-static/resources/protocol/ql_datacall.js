'use strict';
'require form';
'require network';

network.registerPatternVirtual(/^ccmni\d+$/);
network.registerErrorCode('MODEM_STACK_NOT_READY', _('The modem service chain is not ready.'));
network.registerErrorCode('MODEM_EXCEPTION', _('The modem is in an exception state.'));
network.registerErrorCode('SIM_NOT_INITIALIZED', _('The SIM card is not ready.'));
network.registerErrorCode('CONNECT_FAILED_RETRY_LATER', _('The mobile data call failed.'));

return network.registerProtocol('ql_datacall', {
	getI18n: function() {
		return _('MT6990 5G modem');
	},

	getIfname: function() {
		return this._ubus('l3_device') || 'ccmni';
	},

	getPackageName: function() {
		return 'lg6851f-modem-chain-only';
	},

	isFloating: function() {
		return true;
	},

	isVirtual: function() {
		return true;
	},

	getDevices: function() {
		return null;
	},

	containsDevice: function(ifname) {
		return network.getIfnameOf(ifname) == this.getIfname();
	},

	renderFormOptions: function(s) {
		var o;

		o = s.taboption('general', form.Value, 'apn', _('APN'));
		o.placeholder = 'cmnet';
		o.rmempty = false;

		o = s.taboption('general', form.ListValue, 'iptype', _('IP type'));
		o.value('1', _('IPv4'));
		o.value('2', _('IPv6'));
		o.value('3', _('IPv4/IPv6'));
		o.default = '3';

		o = s.taboption('general', form.ListValue, 'auth', _('Authentication type'));
		o.value('0', _('None'));
		o.value('1', 'PAP');
		o.value('2', 'CHAP');
		o.value('3', _('PAP/CHAP'));
		o.default = '0';

	o = s.taboption('general', form.Value, 'username', _('Username'));
	o.depends('auth', '1');
	o.depends('auth', '2');
	o.depends('auth', '3');

	o = s.taboption('general', form.Value, 'password', _('Password'));
	o.depends('auth', '1');
	o.depends('auth', '2');
	o.depends('auth', '3');
		o.password = true;

		o = s.taboption('advanced', form.Value, 'mtu', _('Override MTU'));
		o.datatype = 'range(1280,1500)';
		o.placeholder = '1400';

		o = s.taboption('advanced', form.Value, 'plmn', _('PLMN'));
		o.datatype = 'uinteger';
		o.placeholder = '46002';

		o = s.taboption('advanced', form.Flag, 'auto_conf', _('Automatic APN provisioning'));
		o.default = '1';

		o = s.taboption('advanced', form.Value, 'retry', _('Data call attempts'));
		o.datatype = 'range(1,1000)';
		o.placeholder = '5';

	s.taboption('advanced', form.Value, 'metric', _('Gateway metric'));
	}
});
