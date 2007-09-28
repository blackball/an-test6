/*
  Pulls entries out of the GET line and puts them in an array.
  ie, if the current URL is:
  .  http://wherever.com/map.html?a=b&x=4&z=9
  then this will return:
  .  ["a"] = "b"
  .  ["x"] = "4"
  .  ["z"] = "9"
*/
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

/*
  Prints text to the debug form.
*/
function debug(txt) {
	document.debugform.debug.value += txt;
}

// The GMap2
var map;

// Base URL of the tile and quad servers.
var BASE_URL;
var TILE_URL;

// The arguments in the HTTP request
var getdata;

// args that we pass on.
var passargs = [ 'imagefn', 'wcsfn', 'cc', 'arcsinh', 'arith', //'gain',
    'dashbox', 'cmap',
    'rdlsfn', 'rdlsfield', 'rdlsstyle',
    'rdlsfn2', 'rdlsfield2', 'rdlsstyle2',
				 'outline', 'density'
    ];

var gotoform = document.getElementById("gotoform");

function ra2long(ra) {
	return 360 - ra;
}

function long2ra(lng) {
	ra = -lng;
	if (ra < 0.0) { ra += 360.0; }
	if (ra > 360.0) { ra -= 360.0; }
	return ra;
}

/*
  This function gets called as the user moves the map.
*/
function mapmoved() {
	center = map.getCenter();
	// update the center ra,dec textboxes.
	ra = long2ra(center.lng());
	gotoform.ra_center.value  = "" + ra;
	gotoform.dec_center.value = "" + center.lat();
}

/*
  This function gets called when the user changes the zoom level.
*/
function mapzoomed(oldzoom, newzoom) {
	// update the "zoom" textbox.
	gotoform.zoomlevel.value = "" + newzoom;
	mapmoved();
}

/*
  This function gets called when the user stops moving the map (mouse drag),
  and also after it's moved programmatically (via setCenter(), etc).
*/
function moveended() {
	mapmoved();
}

/*
  This function gets called when the mouse is moved.
*/
function mousemoved(latlong) {
	var ra = long2ra(latlong.lng());
	gotoform.ra_mouse.value  = "" + ra;
	gotoform.dec_mouse.value = "" + latlong.lat();
}

/*
  This function gets called when the "Go" button is pressed, or when Enter
  is hit in one of the ra/dec/zoom textfields.
*/
function moveCenter() {
	var ra   = gotoform.ra_center.value;
	var dec  = gotoform.dec_center.value;
	var zoom = gotoform.zoomlevel.value;
	//debug("Moving map to (" + ra + ", " + dec + ") zoom " + zoom + ", old zoom " + map.getZoom() + "\n");
	map.setCenter(new GLatLng(dec, ra2long(ra)), Number(zoom));
}

/*
  Create a URL with "ra", "dec", and "zoom" GET args, and go there.
*/
function linktohere() {
	var url=new String(window.location);
	if (url.charAt(url.length - 1) == '#')
		url = url.substr(0, url.length - 1);
	var qm = url.search(/\?/);
	if (qm!=-1) {
		url = url.substr(0, qm);
	}
	center = map.getCenter();
	url += "?ra=" + long2ra(center.lng()) + "&dec=" + center.lat()
		+ "&zoom=" + map.getZoom();

	var show = [];
	if (tychoShowing)
		show.push("tycho");
	if (usnobShowing)
		show.push("usnob");
	if (apodShowing)
		show.push("apod");
	if (gridShowing)
		show.push("grid");
	if (messierShowing)
		show.push("messier");
	if (constellationShowing)
		show.push("constellation");
	url += "&show=" + show.join(",");

	for (var i=0; i<passargs.length; i++) {
		if (passargs[i] in getdata) {
			url += "&" + passargs[i];
			if (getdata[passargs[i]] != undefined) {
				url += "=" + getdata[passargs[i]];
			}
		}
	}
	window.location = url;
}

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
	button.style.color = "white";
	button.style.backgroundColor = "#000000";
	button.style.fontSize = "x-small";
	button.style.fontFamily = "monospace";
	button.style.borderStyle = "solid";
	button.style.borderWidth = "1px";
	button.style.marginBottom = "1px";
	button.style.marginTop = "1px";
	button.style.textAlign = "center";
	button.style.cursor = "pointer";
	button.style.width = "70px";
	buttonStyleOn(button);
}

var lineOverlay;
var tychoOverlay;
var usnobOverlay;
var apodOverlay;

var gridShowing = 0;
var messierShowing = 0;
var constellationShowing = 0;
var tychoShowing = 0;
var usnobShowing = 0;
var apodShowing = 0;
var apodOutlineShowing = 0;

function toggleButton(overlayName) {
	button = document.getElementById(overlayName+"ToggleButton");
	if (eval(overlayName+"Showing")) {
		eval(overlayName+"Showing = 0");
		button.style.color = "#666";
	} else {
		eval(overlayName+"Showing = 1");
		button.style.color = "white";
	}
}

function toggleOverlayRestack(overlayName) {
	toggleButton(overlayName);
	restackOverlays();
}

function toggleLineOverlay(overlayName) {
	toggleButton(overlayName);
	updateLine();
	restackOverlays();
}

function makeTile(layers, tag) {
	var newTile = new GTileLayer(new GCopyrightCollection(""), 1, 17);
	newTile.myLayers=layers;
	newTile.myFormat='image/png';
	newTile.myBaseURL=TILE_URL + tag;
	newTile.getTileUrl=CustomGetTileUrl;
	return newTile;
}

function makeMapType(tiles, label) {
	return new GMapType(tiles, G_SATELLITE_MAP.getProjection(), label, G_SATELLITE_MAP);
}

function getBlackUrl(tile, zoom) {
	return BASE_URL + "black.png";
}

var tychoGain = 0; // must match HTML
var tychoArcsinh = 1; // must match HTML

var usnobGain = 0; // must match HTML
var usnobArcsinh = 1; // must match HTML

var apodGain = 0; // must match HTML

function restackOverlays() {
	map.clearOverlays();
	if (tychoShowing)
		map.addOverlay(tychoOverlay);
	if (usnobShowing)
		map.addOverlay(usnobOverlay);
	if (apodShowing)
		map.addOverlay(apodOverlay);
	if (gridShowing || messierShowing || constellationShowing)
		map.addOverlay(lineOverlay);
}

function toggleApodOutline() {
	toggleButton("apodOutline");
	updateApod();
	restackOverlays();
}

function updateLine() {
	var tag = "&tag=line";
	var layers = [];
	if (constellationShowing)
		layers.push('constellation');
	if (messierShowing)
		layers.push('messier');
	if (gridShowing)
		layers.push('grid');
	layerstr = layers.join(",");
	lineOverlay = new GTileLayerOverlay(makeTile(layerstr, tag));
}

function updateApod() {
	var tag = "&tag=apod";
	if (apodOutlineShowing) {
		tag += "&outline";
	}
	tag += "&gain=" + apodGain;
	apodOverlay = new GTileLayerOverlay(makeTile('apod', tag));
}

function updateTycho() {
	var tag = "&tag=tycho";
	tag += "&gain=" + tychoGain;
	if (tychoArcsinh) {
		tag += "&arcsinh";
	}
	tychoOverlay = new GTileLayerOverlay(makeTile('tycho', tag));
}

function updateUsnob() {
	var tag = "&tag=usnob";
	tag += "&gain=" + usnobGain;
	tag += "&cmap=rb";
	if (usnobArcsinh) {
		tag += "&arcsinh";
	}
	usnobOverlay = new GTileLayerOverlay(makeTile('usnob', tag));
}

function changeArcsinh() {
	tychoArcsinh = gotoform.arcsinh.checked;
	updateTycho();
	restackOverlays();
}

function changeGain() {
	var gain = Number(gotoform.gain.value);
	tychoGain = gain;
	updateTycho();
	usnobGain = gain;
	updateUsnob();
	apodGain = gain;
	updateApod();
	restackOverlays();
}

/*
  This function gets called when the page loads.
*/
function startup() {
	getdata = getGetData();

	// Create a new Google Maps client in the "map" <div>.
	map = new GMap2(document.getElementById("map"));

	var ra=0;
	var dec=0;
	var zoom=2;

	if ("ra" in getdata) {
		ra = Number(getdata["ra"]);
	}
	if ("dec" in getdata) {
		dec = Number(getdata["dec"]);
	}
	if ("zoom" in getdata) {
		zoom = Number(getdata["zoom"]);
	}
	map.setCenter(new GLatLng(dec, ra2long(ra)), zoom);

	// Base URL of the tile and quad servers.
	var myurl = new String(window.location);
	if (myurl.toLowerCase().indexOf("explore.astrometry.net") > -1) {
		BASE_URL = "http://explore.astrometry.net/";
	} else {
		BASE_URL = "http://oven.cosmo.fas.nyu.edu/tilecache2/";
	}
	TILE_URL = BASE_URL + "tilecache.php?";

	// Add pass-thru args
	for (var i=0; i<passargs.length; i++) {
		if (passargs[i] in getdata) {
			TILE_URL = TILE_URL + "&" + passargs[i] + "=" + getdata[passargs[i]];
		}
	}

	// Clear the set of map types.
	map.getMapTypes().length = 0;
	
	var blackTile = new GTileLayer(new GCopyrightCollection(""), 1, 17);
	blackTile.getTileUrl = getBlackUrl;
	var blackMapType = makeMapType([blackTile], "Map");
	map.addMapType(blackMapType);
    map.setMapType(blackMapType);

	updateTycho();
	updateUsnob();
	updateApod();

	if ('gain' in getdata) {
		gotoform.gain.value = getdata['gain'];
		changeGain();
	}

	if ('show' in getdata) {
		var showstr = getdata['show'];
		var ss = showstr.split(',');
		var show = [];
		for (i=0; i<ss.length; i++)
			show[ss[i]] = 1;

		var layers = [ 'tycho', 'usnob', 'apod', 'grid', 'constellation', 'messier' ];
		for (i=0; i<layers.length; i++)
			if (layers[i] in show)
				toggleButton(layers[i]);
	} else {
		toggleButton('tycho');
		toggleButton('apod');
	}
	updateLine();
	restackOverlays();

	// Connect up the event listeners...
	GEvent.addListener(map, "move", mapmoved);
	GEvent.addListener(map, "moveend", moveended);
	GEvent.addListener(map, "zoomend", mapzoomed);
	GEvent.addListener(map, "mousemove", mousemoved);
	GEvent.bindDom(window, "resize", map, map.onResize);

	map.addControl(new GLargeMapControl());

	moveended();
	mapzoomed(map.getZoom(), map.getZoom());
}
