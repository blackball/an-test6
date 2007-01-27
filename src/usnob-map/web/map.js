function getGetData(){
	GET_DATA=new Object();
	var getDataString=new String(window.location);
	var questionMarkLocation=getDataString.search(/\?/);
	if (questionMarkLocation!=-1){
		getDataString=getDataString.substr(questionMarkLocation+1);
		var getDataArray=getDataString.split(/&/g);
		for (var i=0;i<getDataArray.length;i++){
			var nameValuePair=getDataArray[i].split(/=/);
			GET_DATA[unescape(nameValuePair[0])]=unescape(nameValuePair[1]);
		}
	}
	return GET_DATA;
}

function debug(txt) {
	//document.debugform.debug.value += txt;
}

var map = new GMap(document.getElementById("map"));

//var BASE_URL = "http://monte.ai.toronto.edu:8080/usnob/";
var BASE_URL = "http://oven.cosmo.fas.nyu.edu/usnob/";
//var COUNT_URL = "http://monte.ai.toronto.edu:8080/usnob/count.php?map=usnob";
var COUNT_URL = BASE_URL + "/count.php?map=usnob";
var TILE_URL = BASE_URL + "tile.php?";
var USNOB_URL = TILE_URL + "map=usnob";
var INDEX_URL = TILE_URL + "map=index";
var FIELD_URL;
var SDSS_URL;
var RDLS_URL;

var gotquad = false;
var gotfieldquad = false;
var sdssobjs = [];
var fieldobjs = [];

var getdata = getGetData();
if (("SDSS_FILE" in getdata) && ("SDSS_FIELD" in getdata)) {
	var filenum = Number(getdata["SDSS_FILE"]);
	var fieldnum = Number(getdata["SDSS_FIELD"]);
	if (filenum > 0 && filenum <= 35 && fieldnum >= 0 && fieldnum < 10000) {
		var sdss = true;
		SDSS_URL = TILE_URL + "map=sdss" + "&SDSS_FILE=" + filenum + "&SDSS_FIELD=" + fieldnum;
		FIELD_URL = TILE_URL + "map=sdssfield" + "&SDSS_FILE=" + filenum + "&SDSS_FIELD=" + fieldnum;
		if ("nf" in getdata) {
			var nf = Number(getdata["nf"]);
			SDSS_URL = SDSS_URL + "&N=" + nf;
			FIELD_URL = FIELD_URL + "&N=" + nf;
		}
	}
	if ("SDSS_QUAD" in getdata) {
		var quadstr = getdata["SDSS_QUAD"];
		debug("quadstr is " + quadstr + "\n");
		if (quadstr.match(/^\d*(,\d*){3}(;\d*(,\d*){3})*$/) != null) {
			debug("quadstr matches.\n");
			sdssobjs = quadstr.match(/\d+/g);
			debug("sdssobjs is " + sdssobjs + "\n");
			if (sdssobjs != null) {
				for (var i=0; i<sdssobjs.length; i++) {
					debug("sdssobjs[" + i + "] is " + sdssobjs[i] + "\n");
				}
			}
			if (sdssobjs != null && (sdssobjs.length % 4 == 0)) {
				gotquad = true;
			}
		}
		fieldobjs = sdssobjs;
		gotfieldquad = gotquad;
	}
}
if ("RDLS_FILE" in getdata) {
	var rdls = true;
	var rdlsfile = getdata["RDLS_FILE"];
	RDLS_URL = TILE_URL + "map=rdls" + "&RDLS_FILE=" + rdlsfile;
	if ("RDLS_FIELD" in getdata) {
		RDLS_URL = RDLS_URL + "&RDLS_FIELD=" + Number(getdata["RDLS_FIELD"]);
	}
}
if ("INDEX_FILE" in getdata) {
	var index = true;
	var indexfile = getdata["INDEX_FILE"];

	if ("INDEX_QUAD" in getdata) {
		var quadstr = getdata["INDEX_QUAD"];
		debug("quadstr is " + quadstr + "\n");
		if (quadstr.match(/^\d*(,\d*){3}(;\d*(,\d*){3})*$/) != null) {
			debug("quadstr matches.\n");
			var indexobjs = quadstr.match(/\d+/g);
			debug("indexobjs is " + indexobjs + "\n");
			if (indexobjs != null) {
				for (var i=0; i<indexobjs.length; i++) {
					debug("indexobjs[" + i + "] is " + indexobjs[i] + "\n");
				}
			}
			if (indexobjs != null && (indexobjs.length % 4 == 0)) {
				var gotindexquad = true;
			}
		}
	}
}


var overview = true;
if (("over" in getdata) && (getdata["over"] == "no")) {
	overview = false;
}

var usnobTile= new GTileLayer(new GCopyrightCollection("Catalog (c) USNO"),1,17);
usnobTile.myLayers='usnob';
usnobTile.myFormat='image/png';
usnobTile.myBaseURL=USNOB_URL;
usnobTile.getTileUrl=CustomGetTileUrl;

var sdssTile = new GTileLayer(new GCopyrightCollection("Catalog (c) SDSS"),1,17);
sdssTile.myLayers='sdss';
sdssTile.myFormat='image/png';
sdssTile.myBaseURL=SDSS_URL;
sdssTile.getTileUrl=CustomGetTileUrl;

var sdssTransTile = new GTileLayer(new GCopyrightCollection("Catalog (c) SDSS"),1,17);
sdssTransTile.myLayers='sdss';
sdssTransTile.myFormat='image/png';
sdssTransTile.myBaseURL=SDSS_URL + "&trans=1";
sdssTransTile.getTileUrl=CustomGetTileUrl;

var sdssFieldTile = new GTileLayer(new GCopyrightCollection("Catalog (c) SDSS"),0,17);
sdssFieldTile.myLayers='sdssfield';
sdssFieldTile.myFormat='image/png';
sdssFieldTile.myBaseURL=FIELD_URL;
sdssFieldTile.getTileUrl=CustomGetTileUrl;

var indexTile = new GTileLayer(new GCopyrightCollection("Catalog (c) SDSS"),1,17);
indexTile.myLayers='index';
indexTile.myFormat='image/png';
indexTile.myBaseURL=INDEX_URL;
indexTile.getTileUrl=CustomGetTileUrl;

var indexTransTile = new GTileLayer(new GCopyrightCollection("Catalog (c) SDSS"),1,17);
indexTransTile.myLayers='index';
indexTransTile.myFormat='image/png';
indexTransTile.myBaseURL=INDEX_URL + "&trans=1";
indexTransTile.getTileUrl=CustomGetTileUrl;

var rdlsTile = new GTileLayer(new GCopyrightCollection(""),1,17);
rdlsTile.myLayers='rdls';
rdlsTile.myFormat='image/png';
rdlsTile.myBaseURL=RDLS_URL;
rdlsTile.getTileUrl=CustomGetTileUrl;

// transparent version of the above
var rdlsTransTile = new GTileLayer(new GCopyrightCollection(""),1,17);
rdlsTransTile.myLayers='rdls';
rdlsTransTile.myFormat='image/png';
rdlsTransTile.myBaseURL=RDLS_URL + "&trans=1";
rdlsTransTile.getTileUrl=CustomGetTileUrl;

/*
  http://monte.ai.toronto.edu:8080/usnob/map.html?SDSS_FILE=1&SDSS_FIELD=6&SDSS_QUAD=0,9,13,6&ra=241.55&dec=26.93&zoom=11&HP=2&INDEX_QUAD=4763876,4763639,4763878,4732341&over=no
*/

var usnobType = new GMapType([usnobTile], G_SATELLITE_MAP.getProjection(), "USNOB", G_SATELLITE_MAP);

if (sdss) {
	var sdssField = new GMapType([sdssFieldTile], G_SATELLITE_MAP.getProjection(), "FIELD", G_SATELLITE_MAP);
	var sdssAlone = new GMapType([sdssTile], G_SATELLITE_MAP.getProjection(), "SDSS", G_SATELLITE_MAP);
}
if (rdls) {
	var rdlsUsnob = new GMapType([usnobTile, rdlsTransTile], G_SATELLITE_MAP.getProjection(), "RDLS+U", G_SATELLITE_MAP);
	var rdlsAlone = new GMapType([rdlsTile], G_SATELLITE_MAP.getProjection(), "RDLS", G_SATELLITE_MAP);
}
if (index) {
	var usnobPlusIndex = new GMapType([usnobTile, indexTransTile], G_SATELLITE_MAP.getProjection(), "U+I", G_SATELLITE_MAP);
	var indexAlone = new GMapType([indexTile], G_SATELLITE_MAP.getProjection(), "INDEX", G_SATELLITE_MAP);
	var indexPlusSDSS = new GMapType([indexTile,sdssTransTile], G_SATELLITE_MAP.getProjection(), "I+S", G_SATELLITE_MAP);
}
if (rdls && index) {
	var rdlsIndex = new GMapType([indexTile, rdlsTransTile], G_SATELLITE_MAP.getProjection(), "R+I", G_SATELLITE_MAP);
}

map.getMapTypes().length = 0;
map.addMapType(usnobType);

if (index) {
	map.addMapType(usnobPlusIndex);
	map.addMapType(indexAlone);
	map.addMapType(indexPlusSDSS);
}
if (sdss) {
	map.addMapType(sdssAlone);
	map.addMapType(sdssField);
}
if (rdls) {
	map.addMapType(rdlsAlone);
	map.addMapType(rdlsUsnob);
}
if (rdls && index) {
	map.addMapType(rdlsIndex);
}
map.addControl(new GLargeMapControl());
map.addControl(new GMapTypeControl());
//map.addControl(new GScaleControl());

if (overview) {
	var w = 800;
	var h = 600;
	var ow = 150;
	var oh = 150;
	overviewControl = new GOverviewMapControl(new GSize(ow,oh));
	map.addControl(overviewControl);
	//map.hideControls([overviewControl]);
}

GEvent.bindDom(window,"resize",map,map.onResize);

function mapmoved() {
	center = map.getCenter();
	// update the center ra,dec textboxes.
	ra = center.lng();
	if (ra < 0.0) { ra += 360.0; }
	document.dummyform.ra_center.value  = "" + ra;
	document.dummyform.dec_center.value = "" + center.lat();
}

function mapzoomed(oldzoom, newzoom) {
	// update the "zoom" textbox.
	document.dummyform.zoom.value = "" + newzoom;
	mapmoved();
}

function moveended() {
	mapmoved();
	// update the object count textbox.
	var bounds = map.getBounds();
	var pixelsize = map.getSize();
	var zoom = map.getZoom();
	var sw = bounds.getSouthWest();
	var ne = bounds.getNorthEast();

	var url = COUNT_URL + "&center_ra=" + center.lng() + "&center_dec=" + center.lat()
		+ "&zoom=" + zoom + "&width=" + pixelsize.width + "&height=" + pixelsize.height;
	GDownloadUrl(url, function(txt) {
		document.infoform.nobjs.value = txt;
	});
}

function mousemoved(latlong) {
	var ra = latlong.lng();
	if (ra < 0.0) { ra += 360.0; }
	document.dummyform.ra_mouse.value  = "" + ra;
	document.dummyform.dec_mouse.value = "" + latlong.lat();
}

function moveCenter() {
	// "Go!" button / Enter in textfields
	var ra = document.dummyform.ra_center.value;
	var dec = document.dummyform.dec_center.value;
	var zoom = document.dummyform.zoom.value;
	// jump directly
	//map.setCenter(new GLatLng(dec, ra), zoom);
	map.setCenter(new GLatLng(dec, ra));
	map.setZoom(zoom);
}

GEvent.addListener(map, "move", mapmoved);
GEvent.addListener(map, "moveend", moveended);
GEvent.addListener(map, "zoomend", mapzoomed);
GEvent.addListener(map, "mousemove", mousemoved);

map.setCenter(new GLatLng(0,0), 0);

// if this page's URL has "ra", "dec", and "zoom" args, then go there.
function parseget() {
	debug("parseget() running.\n");
	var get = getGetData();
	if (("ra" in get) && ("dec" in get) && ("zoom" in get)) {
		var ra = Number(get["ra"]);
		var dec = Number(get["dec"]);
		var zoom = Number(get["zoom"]);
		//document.debugform.debug.value = "ra=" + ra + ", dec=" + dec + ", zoom=" + zoom;
		map.setZoom(zoom);
		map.setCenter(new GLatLng(dec,ra));
	}
}

// create a URL with "ra", "dec", and "zoom" GET args, and go there.
function linktohere() {
	var url=new String(window.location);
	var qm=url.search(/\?/);
	if (qm!=-1) {
		url = url.substr(0, qm);
	}
	center = map.getCenter();
	url = url + "?ra=" + center.lng() + "&dec=" + center.lat()
		+ "&zoom=" + map.getZoom();
    if (!overview) {
		url = url + "&over=no";
    }
    if (sdss) {
		url = url + "&SDSS_FILE=$filenum&SDSS_FIELD=$fieldnum";
	}
	window.location = url;
}

var indexQuads = [];
var indexTxt  = "INDEX QUAD";
var indexQuadState = ("iq" in getdata);

var sdssQuads = [];
var sdssTxt  = "SDSS QUAD";
var sdssQuadState = ("sq" in getdata);

var sdssBoundary = null;
var sdssBoundaryTxt  = "SDSS BOX";
var pts=[];
// HACK!  Hard-code this for now...
pts.push(new GLatLng(26.884,241.39));
pts.push(new GLatLng(27.071,241.54));
pts.push(new GLatLng(26.978,241.69));
pts.push(new GLatLng(26.791,241.55));
pts.push(pts[0]);
sdssBoundary = new GPolyline(pts, "ffc800");
var sdssBoundaryState = ("sb" in getdata);
if (sdssBoundaryState) {
	map.addOverlay(sdssBoundary);
}

var sdssFieldQuads = [];
var sdssFieldTxt  = "FIELD QUAD";
var sdssFieldQuadState = ("fq" in getdata);

function buttonStyleOn(button) {
	button.style.borderTopColor    = "#b0b0b0";
	button.style.borderLeftColor   = "#b0b0b0";
	button.style.borderBottomColor = "white";
	button.style.borderRightColor  = "white";
	button.style.fontWeight = "bold";
}

function buttonStyleOff(button) {
	button.style.borderTopColor    = "white";
	button.style.borderLeftColor   = "white";
	button.style.borderBottomColor = "#b0b0b0";
	button.style.borderRightColor  = "#b0b0b0";
	button.style.fontWeight = "normal";
}

function buttonStyleCommon(button) {
	button.style.color = "#000000";
	button.style.backgroundColor = "white";
	button.style.fontSize = "x-small";
	button.style.fontFamily = "Arial";
	button.style.borderStyle = "solid";
	button.style.borderWidth = "1px";
	button.style.marginBottom = "1px";
	button.style.marginTop = "1px";
	button.style.textAlign = "center";
	button.style.cursor = "pointer";
	button.style.width = "70px";
	buttonStyleOn(button);
}

function IndexQuadControl() {}
IndexQuadControl.prototype = new GControl();
IndexQuadControl.prototype.initialize = function(map) {
	var on = indexQuadState;
	var container = document.createElement("div");
	var toggleDiv = document.createElement("div");
	var txtNode = document.createTextNode(indexTxt);
	this.setButtonStyle_(toggleDiv);
	container.appendChild(toggleDiv);
	toggleDiv.appendChild(txtNode);
	GEvent.addDomListener(toggleDiv, "click", function(){
		if (on) {
			on = false;
			for (var i=0; i<indexQuads.length; i++) {
				map.removeOverlay(indexQuads[i]);
			}
			buttonStyleOff(toggleDiv);
		} else {
			on = true;
			for (var i=0; i<indexQuads.length; i++) {
				map.addOverlay(indexQuads[i]);
			}
			buttonStyleOn(toggleDiv);
		}
	});
	map.getContainer().appendChild(container);
	return container;
}
IndexQuadControl.prototype.getDefaultPosition = function() {
	return new GControlPosition(G_ANCHOR_TOP_RIGHT, new GSize(3*78 + 7, 30));
}
IndexQuadControl.prototype.setButtonStyle_ = function(button) {
	buttonStyleCommon(button);
	if (!indexQuadState) {
		buttonStyleOff(button);
	}
}

function SdssQuadControl() {}
SdssQuadControl.prototype = new GControl();
SdssQuadControl.prototype.initialize = function(map) {
	var on = sdssQuadState;
	var container = document.createElement("div");
	var toggleDiv = document.createElement("div");
	var txtNode = document.createTextNode(sdssTxt);
	this.setButtonStyle_(toggleDiv);
	container.appendChild(toggleDiv);
	toggleDiv.appendChild(txtNode);
	GEvent.addDomListener(toggleDiv, "click", function(){
		if (on) {
			on = false;
			for (var i=0; i<sdssQuads.length; i++) {
				map.removeOverlay(sdssQuads[i]);
			}
			buttonStyleOff(toggleDiv);
		} else {
			on = true;
			for (var i=0; i<sdssQuads.length; i++) {
				map.addOverlay(sdssQuads[i]);
			}
			buttonStyleOn(toggleDiv);
		}
	});
	map.getContainer().appendChild(container);
	return container;
}
SdssQuadControl.prototype.getDefaultPosition = function() {
	return new GControlPosition(G_ANCHOR_TOP_RIGHT, new GSize(1*78 + 7, 30));
}
SdssQuadControl.prototype.setButtonStyle_ = function(button) {
	buttonStyleCommon(button);
	if (!sdssQuadState) {
		buttonStyleOff(button);
	}
}

function SdssBoundaryControl() {}
SdssBoundaryControl.prototype = new GControl();
SdssBoundaryControl.prototype.initialize = function(map) {
	var on = sdssBoundaryState;
	var container = document.createElement("div");
	var toggleDiv = document.createElement("div");
	var txtNode = document.createTextNode(sdssBoundaryTxt);
	this.setButtonStyle_(toggleDiv);
	container.appendChild(toggleDiv);
	toggleDiv.appendChild(txtNode);
	GEvent.addDomListener(toggleDiv, "click", function(){
		if (on) {
			on = false;
			if (sdssBoundary != null) {
				map.removeOverlay(sdssBoundary);
			}
			buttonStyleOff(toggleDiv);
		} else {
			on = true;
			if (sdssBoundary != null) {
				map.addOverlay(sdssBoundary);
			}
			buttonStyleOn(toggleDiv);
		}
	});
	map.getContainer().appendChild(container);
	return container;
}
SdssBoundaryControl.prototype.getDefaultPosition = function() {
	return new GControlPosition(G_ANCHOR_TOP_RIGHT, new GSize(2*78 + 7, 30));
}
SdssBoundaryControl.prototype.setButtonStyle_ = function(button) {
	buttonStyleCommon(button);
	if (!sdssBoundaryState) {
		buttonStyleOff(button);
	}
}

function FieldQuadControl() {}
FieldQuadControl.prototype = new GControl();
FieldQuadControl.prototype.initialize = function(map) {
	var on = sdssFieldQuadState;
	var container = document.createElement("div");
	var toggleDiv = document.createElement("div");
	var txtNode = document.createTextNode(sdssFieldTxt);
	this.setButtonStyle_(toggleDiv);
	container.appendChild(toggleDiv);
	toggleDiv.appendChild(txtNode);
	GEvent.addDomListener(toggleDiv, "click", function(){
		if (on) {
			on = false;
			for (var i=0; i<sdssFieldQuads.length; i++) {
				map.removeOverlay(sdssFieldQuads[i]);
			}
			buttonStyleOff(toggleDiv);
		} else {
			on = true;
			for (var i=0; i<sdssFieldQuads.length; i++) {
				map.addOverlay(sdssFieldQuads[i]);
			}
			buttonStyleOn(toggleDiv);
		}
	});
	map.getContainer().appendChild(container);
	return container;
}
FieldQuadControl.prototype.getDefaultPosition = function() {
	return new GControlPosition(G_ANCHOR_TOP_RIGHT, new GSize(7, 30));
}
FieldQuadControl.prototype.setButtonStyle_ = function(button) {
	buttonStyleCommon(button);
	if (!sdssFieldQuadState) {
		buttonStyleOff(button);
	}
}

function addquad() {
	var URL = BASE_URL + "quad.php?src=sdss&file=" + filenum + "&field=" + fieldnum +
		"&quad=";
	for (var i=0; i<sdssobjs.length; i++) {
		if (i) { URL = URL + ","; }
		URL = URL + sdssobjs[i];
	}

	GDownloadUrl(URL, function(data, responseCode){
		var xml = GXml.parse(data);
		var quads = xml.documentElement.getElementsByTagName("quad");
		for (var i = 0; i < quads.length; i++) {
			var points = [];
			for (var j=0; j<4; j++) {
				points.push(new GLatLng(parseFloat(quads[i].getAttribute("dec"+j)),
										parseFloat(quads[i].getAttribute("ra"+j))));
			}
			points.push(points[0]);
			sdssQuads.push(new GPolyline(points, "#ffc800"));
			//    sdssQuad = new GPolyline(points, "#0000ff");
			//    sdssQuad = new GPolyline(points, "#00e000");
			//sdssQuad = new GPolyline(points, "#ffc800");
		}
		if (sdssQuadState) {
			for (var i=0; i<sdssQuads.length; i++) {
				map.addOverlay(sdssQuads[i]);
			}
		}
		map.addControl(new SdssQuadControl());
	});
}

function addindexquad() {
	var URL = BASE_URL + "quad.php?src=index&hp=" + hp +
		"&quad=";
	for (var i=0; i<indexobjs.length; i++) {
		if (i) { URL = URL + ","; }
		URL = URL + indexobjs[i];
	}
	GDownloadUrl(URL, function(data, responseCode){
		var xml = GXml.parse(data);
		var quads = xml.documentElement.getElementsByTagName("quad");
		for (var i = 0; i < quads.length; i++) {
			var points = [];
			for (var j=0; j<4; j++) {
				points.push(new GLatLng(parseFloat(quads[i].getAttribute("dec"+j)),
										parseFloat(quads[i].getAttribute("ra"+j))));
			}
			points.push(points[0]);
			indexQuads.push(new GPolyline(points, "#90a8ff"));
			//indexQuad = new GPolyline(points, "#ff0000");
			//indexQuad = new GPolyline(points, "#90a8ff");
		}
		if (indexQuadState) {
			for (var i=0; i<indexQuads.length; i++) {
				map.addOverlay(indexQuads[i]);
			}
		}
		map.addControl(new IndexQuadControl());
	});
}

function addfieldquad() {
	var URL = BASE_URL + "quad.php?src=field&file=" + filenum + "&field=" + fieldnum +
		"&quad=";
	for (var i=0; i<fieldobjs.length; i++) {
		if (i) { URL = URL + ","; }
		URL = URL + fieldobjs[i];
	}
	GDownloadUrl(URL, function(data, responseCode){
		var xml = GXml.parse(data);
		var quads = xml.documentElement.getElementsByTagName("quad");
		for (var i = 0; i < quads.length; i++) {
			var points = [];
			for (var j=0; j<4; j++) {
				points.push(new GLatLng(parseFloat(quads[i].getAttribute("dec"+j)),
										parseFloat(quads[i].getAttribute("ra"+j))));
			}
			points.push(points[0]);
			sdssFieldQuads.push(new GPolyline(points, "#ffc800"));
		}
		if (sdssFieldQuadState) {
			for (var i=0; i<sdssFieldQuads.length; i++) {
				map.addOverlay(sdssFieldQuads[i]);
			}
		}
		map.addControl(new FieldQuadControl());
	});
}

debug("here 1\n");
setTimeout("moveended();", 1);
setTimeout("mapzoomed(map.getZoom(),map.getZoom());", 2);
setTimeout("parseget()", 3);
if (gotquad) {
	setTimeout("addquad();", 5);
}
if (gotindexquad) {
	setTimeout("addindexquad();", 6);
}
if (gotfieldquad) {
	setTimeout("addfieldquad();", 7);
}
if (sdss) {
	if (sdssBoundaryState) {
		map.addOverlay(sdssBoundary);
	}
	map.addControl(new SdssBoundaryControl());
}

var type="";
if ("view" in getdata) {
	var view = getdata["view"];
	if (view == "usnob") {
		type = "usnobType";
	} else if (view == "u+i") {
		type = "usnobPlusIndex";
	} else if (view == "index") {
		type = "indexAlone";
	} else if (view == "i+s") {
		type = "indexPlusSDSS";
	} else if (view == "sdss") {
		type = "sdssAlone";
	} else if (view == "field") {
		type = "sdssField";
	}
}
if (type.length) {
	setTimeout("map.setMapType(" + type + ");", 8);
} else {
	if (gothp) {
		setTimeout("map.setMapType(indexPlusSDSS);", 4);
	} else if (sdss) {
		setTimeout("map.setMapType(usnobPlusSDSS);", 4);
	}
}

// DEBUG
//setTimeout("map.setMapType(sdssField); map.setZoom(2); map.setCenter(new GLatLng(8.41,179.65));", 8);
//setTimeout("map.setMapType(sdssAlone); map.setZoom(12); map.setCenter(new GLatLng(26.92,241.51));", 8);
//setTimeout("map.setMapType(indexAlone); map.setZoom(12); map.setCenter(new GLatLng(26.92,241.51));", 8);

/*
  info_html = "<b>testing</b>";
  document.getElementById("info").innerHTML += info_html;
*/
