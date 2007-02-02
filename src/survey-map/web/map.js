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

var map;

// Base URL of the tile and quad servers.
var BASE_URL;
var TILE_URL;
var QUAD_URL;

var getdata;

// The current polylines.
var polygons = [];
// The currently visible polylines.
var visiblePolygons = [];
// Show polygons?
var showPolygons = true;

/*
  Retrieves a new list of polygons from the server.
*/
function getNewPolygons() {
	// Get the current view...
	var bounds = map.getBounds();
	var pixelsize = map.getSize();
	var zoom = map.getZoom();
	var center = map.getCenter();
	var sw = bounds.getSouthWest();
	var ne = bounds.getNorthEast();

	// Construct the URL for the quad server.
	url = QUAD_URL
		+  "ra1=" + sw.lng() + "&dec1=" + sw.lat()
		+ "&ra2=" + ne.lng() + "&dec2=" + ne.lat()
		+ "&width=" + pixelsize.width
		+ "&height=" + pixelsize.height;
	//+ "&zoom=" + zoom + "&ra=" + center.lng() + "&dec=" + center.lat()

	debug("polygon url: " + url + "\n");

	// Contact the quad server with our current position...
	GDownloadUrl(url, function(data, responseCode){
		// Remove old polygons.
		for (var i=0; i<visiblePolygons.length; i++) {
			map.removeOverlay(visiblePolygons[i]);
		}
		visiblePolygons.length = 0;

		// Parse new polygons
		var xml = GXml.parse(data);
		var polys = xml.documentElement.getElementsByTagName("poly");
		polygons.length = 0;
		debug("parsing polygons...\n");
		for (var i=0; i<polys.length; i++) {
			var points = [];
			for (var j=0;; j++) {
				if (!(polys[i].hasAttribute("dec"+j) &&
					  polys[i].hasAttribute("ra"+j)))
					break;
				points.push(new GLatLng(parseFloat(polys[i].getAttribute("dec"+j)),
										parseFloat(polys[i].getAttribute("ra"+j))));
			}
			//points.push(points[0]);
			debug("  polygon: " + points.length + " points.\n");
			polygons.push(new GPolyline(points, "#90a8ff"));
		}
		debug("got " + polygons.length + " polygons.\n");

		if (showPolygons) {
			// Show new polygons.
			for (var i=0; i<polygons.length; i++) {
				map.addOverlay(polygons[i]);
				visiblePolygons.push(polygons[i]);
			}
		}
	});
}

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
	getNewPolygons();
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

function PolygonControl() {}
PolygonControl.prototype = new GControl();
PolygonControl.prototype.initialize = function(map) {
	var container = document.createElement("div");
	var toggleDiv = document.createElement("div");
	var txtNode = document.createTextNode("POLY");
	this.setButtonStyle_(toggleDiv);
	container.appendChild(toggleDiv);
	toggleDiv.appendChild(txtNode);
	GEvent.addDomListener(toggleDiv, "click", function(){
		if (showPolygons) {
			showPolygons = false;
			// Remove polygons.
			for (var i=0; i<visiblePolygons.length; i++) {
				map.removeOverlay(visiblePolygons[i]);
			}
			visiblePolygons.length = 0;
			buttonStyleOff(toggleDiv);
		} else {
			showPolygons = true;
			// Add polygons.
			for (var i=0; i<polygons.length; i++) {
				map.addOverlay(polygons[i]);
				visiblePolygons.push(polygons[i]);
			}
			buttonStyleOn(toggleDiv);
		}
	});
	map.getContainer().appendChild(container);
	return container;
}
PolygonControl.prototype.getDefaultPosition = function() {
	return new GControlPosition(G_ANCHOR_TOP_RIGHT, new GSize(0*78 + 7, 7));
}
PolygonControl.prototype.setButtonStyle_ = function(button) {
	buttonStyleCommon(button);
	if (!showPolygons) {
		buttonStyleOff(button);
	}
}

/*
  This function gets called when the page loads.
*/
function startup() {
	getdata = getGetData();

	// Create a new Google Maps client in the "map" <div>.
	map = new GMap(document.getElementById("map"));

	var ra=0;
	var dec=0;
	var zoom=0;

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
	BASE_URL = "http://oven.cosmo.fas.nyu.edu/survey/";
	TILE_URL = BASE_URL + "tile.php?";
	QUAD_URL = BASE_URL + "quad.php?";

	// Describe the tile server...
	var myTile = new GTileLayer(new GCopyrightCollection(""), 1, 17);
	myTile.myLayers='mylayer';
	myTile.myFormat='image/png';
	myTile.myBaseURL=TILE_URL;
	// Transparent tiles:
	//myTile.myBaseURL=TILE_URL + "&trans=1";
	myTile.getTileUrl=CustomGetTileUrl;

	var myMapType = new GMapType([myTile], G_SATELLITE_MAP.getProjection(), "MyTile", G_SATELLITE_MAP);

	map.getMapTypes().length = 0;
	map.addMapType(myMapType);
	map.setMapType(myMapType);

	// Show an overview map?
	var overview = true;
	if (("over" in getdata) && (getdata["over"] == "no")) {
		overview = false;
	}
	if (overview) {
		var ow = 150;
		var oh = 150;
		overviewControl = new GOverviewMapControl(new GSize(ow,oh));
		map.addControl(overviewControl);
	}

	// Connect up the event listeners...
	GEvent.addListener(map, "move", mapmoved);
	GEvent.addListener(map, "moveend", moveended);
	GEvent.addListener(map, "zoomend", mapzoomed);
	GEvent.addListener(map, "mousemove", mousemoved);
	GEvent.bindDom(window, "resize", map, map.onResize);

	map.addControl(new GLargeMapControl());
	map.addControl(new GMapTypeControl());
	map.addControl(new PolygonControl());

	moveended();
	mapzoomed(map.getZoom(), map.getZoom());
}
