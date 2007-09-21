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
var passargs = [ 'imagefn', 'wcsfn', 'cc', 'arcsinh', 'arith', 'gain',
    'dashbox', 'cmap',
    'rdlsfn', 'rdlsfield', 'rdlsstyle',
    'rdlsfn2', 'rdlsfield2', 'rdlsstyle2',
				 'outline', 'density'
    ];

/*
  This function gets called as the user moves the map.
*/
function mapmoved() {
	center = map.getCenter();
	// update the center ra,dec textboxes.
	ra = center.lng();
	if (ra < 0.0) { ra += 360.0; }
	document.gotoform.ra_center.value  = "" + ra;
	document.gotoform.dec_center.value = "" + center.lat();
}

/*
  This function gets called when the user changes the zoom level.
*/
function mapzoomed(oldzoom, newzoom) {
	// update the "zoom" textbox.
	document.gotoform.zoomlevel.value = "" + newzoom;
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
	var ra = latlong.lng();
	if (ra < 0.0) { ra += 360.0; }
	document.gotoform.ra_mouse.value  = "" + ra;
	document.gotoform.dec_mouse.value = "" + latlong.lat();
}

/*
  This function gets called when the "Go" button is pressed, or when Enter
  is hit in one of the ra/dec/zoom textfields.
*/
function moveCenter() {
	var ra   = document.gotoform.ra_center.value;
	var dec  = document.gotoform.dec_center.value;
	var zoom = document.gotoform.zoomlevel.value;
	//debug("Moving map to (" + ra + ", " + dec + ") zoom " + zoom + ", old zoom " + map.getZoom() + "\n");
	map.setCenter(new GLatLng(dec, ra), Number(zoom));
}

/*
  Create a URL with "ra", "dec", and "zoom" GET args, and go there.
*/
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
	for (var i=0; i<passargs.length; i++) {
		if (passargs[i] in getdata) {
			url = url + "&" + passargs[i];
			if (getdata[passargs[i]] != undefined) {
				url = url + "=" + getdata[passargs[i]];
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

function toggleOverlay(overlayName) {
	overlay = eval(overlayName+"Overlay");
	button = document.getElementById(overlayName+"ToggleButton");
	if (eval(overlayName+"Showing")) {
		map.removeOverlay(overlay);
		eval(overlayName+"Showing = 0");
		button.style.color = "#666";
	} else {
		map.addOverlay(overlay);
		eval(overlayName+"Showing = 1");
		button.style.color = "white";
	}
}

var gridOverlay;
var gridShowing = 0;
var messierOverlay;
var messierShowing = 0;
var constellationOverlay;
var constellationShowing = 0;


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
	map.setCenter(new GLatLng(dec, ra), zoom);

	// Base URL of the tile and quad servers.
	//BASE_URL = "http://oven.cosmo.fas.nyu.edu/tilecache2/";
	BASE_URL = "http://explore.astrometry.net/";
	TILE_URL = BASE_URL + "tilecache.php?";

	// Add pass-thru args
	for (var i=0; i<passargs.length; i++) {
		if (passargs[i] in getdata) {
			TILE_URL = TILE_URL + "&" + passargs[i] + "=" + getdata[passargs[i]];
		}
	}

	// Clear the set of map types.
	map.getMapTypes().length = 0;

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

	/////////////////////////////////// LAYERS /////////////////////////

	// overlays
	var gridTile = makeTile('grid', "&tag=grid");
	gridOverlay = new GTileLayerOverlay(gridTile);
	var messierTile = makeTile('messier', "&tag=messier");
	messierOverlay = new GTileLayerOverlay(messierTile);
	var constellationTile = makeTile('constellation', "&tag=constellation");
	constellationOverlay = new GTileLayerOverlay(constellationTile);

	var usnobTile = makeTile('usnob', "&tag=usnob");
	var usnobMapType = makeMapType([usnobTile], "USNOB");

	var tychoTile = makeTile('tycho', "&tag=tycho&arcsinh&gain=-2");
	var tychoMapType = makeMapType([tychoTile], "Tycho");

	var apodTile = makeTile('apod', "&tag=apod");
	var apodMapType = makeMapType([tychoTile, apodTile], "APOD");

	map.addMapType(apodMapType);
	map.addMapType(usnobMapType);
	map.addMapType(tychoMapType);

	/////////////////////////////////// END LAYERS /////////////////////////

    map.setMapType(apodMapType);

	// Connect up the event listeners...
	GEvent.addListener(map, "move", mapmoved);
	GEvent.addListener(map, "moveend", moveended);
	GEvent.addListener(map, "zoomend", mapzoomed);
	GEvent.addListener(map, "mousemove", mousemoved);
	GEvent.bindDom(window, "resize", map, map.onResize);

	map.addControl(new GLargeMapControl());
	map.addControl(new GMapTypeControl());

	moveended();
	mapzoomed(map.getZoom(), map.getZoom());
}
