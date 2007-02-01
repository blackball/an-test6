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

// Create a new Google Maps client in the "map" <div>.
var map = new GMap(document.getElementById("map"));

// Base URL of the tile and quad servers.
var BASE_URL = "http://oven.cosmo.fas.nyu.edu/usnob/";
var TILE_URL = BASE_URL + "tile.php?";
var QUAD_URL = BASE_URL + "quad.php?";

var getdata = getGetData();

// Show an overview map?
var overview = true;
if (("over" in getdata) && (getdata["over"] == "no")) {
	overview = false;
}

// Describe the tile server...
var myTile= new GTileLayer(new GCopyrightCollection("Catalog (c) USNO"),1,17);
myTile.myLayers='mylayer';
myTile.myFormat='image/png';
myTile.myBaseURL=TILE_URL;
myTile.getTileUrl=CustomGetTileUrl;

map.getMapTypes().length = 0;
map.addMapType(myType);

map.addControl(new GLargeMapControl());
map.addControl(new GMapTypeControl());

if (overview) {
	var w = 800;
	var h = 600;
	var ow = 150;
	var oh = 150;
	overviewControl = new GOverviewMapControl(new GSize(ow,oh));
	map.addControl(overviewControl);
}

GEvent.bindDom(window,"resize",map,map.onResize);

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
	document.gotoform.zoom.value = "" + newzoom;
	mapmoved();
}

/*
  This function gets called when the user stops moving the map (mouse drag).
 */
function moveended() {
	mapmoved();

	// Get the current view...
	var bounds = map.getBounds();
	var pixelsize = map.getSize();
	var zoom = map.getZoom();
	var center = map.getCenter();
	var sw = bounds.getSouthWest();
	var ne = bounds.getNorthEast();

	/*
	  url = INDEX_QUAD_URL + "&zoom=" + zoom 
	  + "&ra=" + center.lng() + "&dec=" + center.lat()
	  + "&width=" + pixelsize.width
	  + "&height=" + pixelsize.height;

	  debug("quads: " + url + "\n");
	*/

	// Contact the quad server with our current position...
	/*
	  GDownloadUrl(url, function(data, responseCode){
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
	  }
	  for (var i=0; i<indexQuads.length; i++) {
	  map.addOverlay(indexQuads[i]);
	  }
	  }
	  });
	*/
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
	var ra = document.gotoform.ra_center.value;
	var dec = document.gotoform.dec_center.value;
	var zoom = document.gotoform.zoom.value;
	map.setCenter(new GLatLng(dec, ra));
	map.setZoom(zoom);
}

// Connect up the event listeners...
GEvent.addListener(map, "move", mapmoved);
GEvent.addListener(map, "moveend", moveended);
GEvent.addListener(map, "zoomend", mapzoomed);
GEvent.addListener(map, "mousemove", mousemoved);

var ra=0;
var dec=0;
var zoom=0;

if ("ra" in getdata) {
	ra = Number(getdata["ra"]);
}
if ("dec" in getdata) {
	dec = Number(getdata["dec"];);
}
if ("zoom" in getdata) {
	zoom = Number(getdata["zoom"]);
}

map.setCenter(new GLatLng(dec, ra), zoom);


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

//var indexQuads = [];

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
		indexQuadState = on;
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

map.addControl(new IndexQuadControl());

setTimeout("moveended();", 1);
setTimeout("mapzoomed(map.getZoom(),map.getZoom());", 2);

