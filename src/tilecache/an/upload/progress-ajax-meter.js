var req;
var reload = true;

function removeAllChildren(node) {
	while (node.childNodes.length) {
		node.removeChild(node.childNodes[0]);
	}
}

function sendRequest() {
  req = new XMLHttpRequest();
  req.onreadystatechange = contentReady;
  req.open("GET", "{{ xmlurl }}", true);
  req.send("");
}

function processIt() {
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

  err = prog[0].getAttribute('error');
  if (err) {
    txt = document.getElementById('meter_text');
	removeAllChildren(txt);
	txt.appendChild(document.createTextNode(err));
	fore = document.getElementById('meter_fore');
    fore.style.width = '0px';
	back = document.getElementById('meter_back');
	back.style.background = 'pink';
    reload = false;
	return true;
  }

  pct = prog[0].getAttribute('pct');
  txt = document.getElementById('meter_text');
  removeAllChildren(txt);
  txt.appendChild(document.createTextNode('' + pct + '%'));
  fore = document.getElementById('meter_fore');
  fore.style.width = '' + pct + '%';
  if (pct == 100) {
    reload = false;
  }
  return true;
}

function contentReady() {
  if (processIt()) {
    if (reload) {
	  // do it again!
      setTimeout('sendRequest()', 2000)
    }
  }
}

function onLoad() {
  sendRequest();
}

