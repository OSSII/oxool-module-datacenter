/* -*- js-indent-level: 8 -*- */
/*
	Socket to be intialized on opening the config settings in Admin console
*/
/* global $ vex AdminSocketBase Admin */
var AdminSocketConfigSettings = Admin.SocketBase.extend({
	constructor: function(host) {
		this.base(host);
	},

	setConfig: function (settings) {
		this.socket.send('module datacenter setConfig ' + settings);
	},

	onSocketOpen: function() {
		// Base class' onSocketOpen handles authentication
		this.base.call(this);
		var form = $('#mainform').find('input, select, textarea');
		var cmd = 'module datacenter getConfig';
		for (var i= 0 ; i < form.length ; i++)
		{
			cmd += ' ' + form[i].id;
		}
		this.socket.send(cmd);
	},

	onSocketMessage: function(e) {
		//console.log(e.data);
		var textMsg;
		if (typeof e.data === 'string')
		{
			textMsg = e.data;
		}
		else
		{
			textMsg = '';
		}

		if (textMsg.startsWith('settings')) {
			var json = JSON.parse(textMsg.substring(textMsg.indexOf('{')));
			for (var key in json)
			{
				if (json[key] === null) continue; // null : 表示 xml 中找不到對應的 key

				var input = document.getElementById(key);
				if (input)
				{
					switch (input.type)
					{
					case 'text':
					case 'textarea':
					case 'number':
					case 'hidden':
						input.value = json[key];
						break;
					case 'checkbox':
					case 'radio':
						input.checked = json[key];
						break;
					case 'select-one':	// 下拉選項(單選)
						for (var i = 0 ; i < input.length ; i++)
						{
							if (input.options[i].value == json[key])
							{
								input.selectedIndex = i;
								break;
							}
						}
						break;
					default:
						input.value = json[key];
						//console.log('未知的 input type -> ' + input.type);
						//console.log(input);
						break;
					}
				}
			}
		}
		else if (textMsg == 'setConfigOk')	// 設定更新成功
		{
			vex.dialog.alert('<p>設定更新成功</p><p>這些設定，會在下次啟動服務後生效</p>');
		}
		else if (textMsg == 'setConfigNothing')
		{
			vex.dialog.alert('設定未更新！');
		}
		$('#saveConfig').attr('disabled', false);
	},

	onSocketClose: function()
	{

	}
});

Admin.ConfigSettings = function(host)
{
	return new AdminSocketConfigSettings(host);
};
