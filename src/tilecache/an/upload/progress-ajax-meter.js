var req{{name}};
var reload{{name}} = true;
var period{{name}} = 2000;
var failed{{name}} = false;
var succeeded{{name}} = false;
var id{{name}} = '';

function startProgressMeter{{name}}(id) {
	id{{name}} = id;
	reload{{name}} = true;
	failed{{name}} = false;
	succeeded{{name}} = false;
	sendRequest{{name}}();
}

function sendRequest{{name}}() {
	req = new XMLHttpRequest();
	req.onreadystatechange = contentReady{{name}};
	req.open("GET", '{{ xmlurl }}' + id{{name}}, true);
	req.send("");
}

function processIt{{name}}() {
	var req = req{{name}}
	if (!req) {
		return false;
	}
	if (req.readyState != 4) {
		return false;
	}
	if (req.status != 200) {
		return true;
	}
	xml = req.responseXML;
	if (!xml) {
		return true;
	}
	prog = xml.getElementsByTagName('progress');
	if (!prog.length) {
		return true;
	}

	// Did the upload finish with an error?
	err = prog[0].getAttribute('error');
	if (err) {
		txt = document.getElementById('meter_text{{name}}');
		while (txt.childNodes.length) {
			txt.removeChild(txt.childNodes[0]);
		}
		txt.appendChild(document.createTextNode(err));
		fore = document.getElementById('meter_fore{{name}}');
		fore.style.width = '0px';
		back = document.getElementById('meter_back{{name}}');
		back.style.background = 'pink';
		reload{{name}} = false;
		failed{{name}} = true;
		return true;
	}

	pct = prog[0].getAttribute('pct');
	txt = document.getElementById('meter_text{{name}}');
	while (txt.childNodes.length) {
		txt.removeChild(txt.childNodes[0]);
	}
	txt.appendChild(document.createTextNode('' + pct + '%'));
	fore = document.getElementById('meter_fore{{name}}');
	fore.style.width = '' + pct + '%';
	if (pct == 100) {
		reload{{name}} = false;
		succeeded{{name}} = false;
	}
	return true;
}

function contentReady{{name}}() {
	if (processIt{{name}}()) {
		if (reload{{name}}) {
			// do it again!
			setTimeout('sendRequest{{name}}()', period{{name}});
		}
	}
}

