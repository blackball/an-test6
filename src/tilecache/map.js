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

// are we showing the overview?
var overview = true;

// args that we pass on.
var passargs = [ 'imagefn', 'wcsfn', 'cc', 'arcsinh', 'arith', 'gain',
				 'dashbox', 'cmap',
 				 'rdlsfn', 'rdlsfield', 'rdlsstyle',
 				 'rdlsfn2', 'rdlsfield2', 'rdlsstyle2',
                 'outline'
				 ]; // 'clean'

var usnobMapType;
//var cleanMapType;

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

    /*
     if (map.getCurrentMapType() == cleanMapType) {
     url = url + "&clean";
     }
     */

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
	BASE_URL = "http://oven.cosmo.fas.nyu.edu/tilecache2/";
	//BASE_URL = "http://monte.ai.toronto.edu:8080/tilecache/";
	TILE_URL = BASE_URL + "tilecache.php?";
	//QUAD_URL = BASE_URL + "quad.php?";

	// Add pass-thru args
	for (var i=0; i<passargs.length; i++) {
		if (passargs[i] in getdata) {
			TILE_URL = TILE_URL + "&" + passargs[i] + "=" + getdata[passargs[i]];
		}
	}

	// Describe the tile server...
	// USNOB underneath
	var usnobTile = new GTileLayer(new GCopyrightCollection(""), 1, 17);
	usnobTile.myLayers='usnob';
	usnobTile.myFormat='image/png';
	usnobTile.myBaseURL=TILE_URL + "&tag=usnob";
	usnobTile.getTileUrl=CustomGetTileUrl;

	lay = [];

    lay.push("solid");
	if ("tycho" in getdata) {
		lay.push('tycho') ;
	}
	if ("grid" in getdata) {
		lay.push('grid');
	}
	if (("imagefn" in getdata) && ("imwcsfn" in getdata)) {
		lay.push('image');
	}
	if ("wcsfn" in getdata) {
		lay.push('boundary');
	}
	if ("const" in getdata) {
		lay.push('constellation');
	}
	if ("rdlsfn" in getdata) {
		lay.push('rdls');
	}
	if ("clean" in getdata) {
		lay.push('clean');
	}
	if ("dirty" in getdata) {
		lay.push('dirty');
	}
	/*if ("apod" in getdata) {
	  lay.push('apod');
	  }
	*/
	layers = lay.join(",");

	var userTile = new GTileLayer(new GCopyrightCollection(""), 1, 17);
	var mylay = lay;
	mylay.push('apod');
	userTile.myLayers=mylay.join(",");
	userTile.myFormat='image/png';
	userTile.myBaseURL=TILE_URL + "&tag=apod";
	userTile.getTileUrl=CustomGetTileUrl;

	var userTile2 = new GTileLayer(new GCopyrightCollection(""), 1, 17);
	userTile2.myLayers=layers;
	userTile2.myFormat='image/png';
	userTile2.myBaseURL=TILE_URL + "&version=20070911";
	userTile2.getTileUrl=CustomGetTileUrl;

	//var userUsnobMapType = new GMapType([usnobTile, userTile], G_SATELLITE_MAP.getProjection(), "User+", G_SATELLITE_MAP);

	var userMapType = new GMapType([userTile], G_SATELLITE_MAP.getProjection(), "APOD", G_SATELLITE_MAP);
	var userMapType2 = new GMapType([userTile2], G_SATELLITE_MAP.getProjection(), "Clean", G_SATELLITE_MAP);

		usnobMapType = new GMapType([usnobTile],
									G_SATELLITE_MAP.getProjection(), "USNOB", G_SATELLITE_MAP);
        /*
         cleanMapType = new GMapType([cleanTile],
         G_SATELLITE_MAP.getProjection(), "Clean", G_SATELLITE_MAP);
         */

	map.getMapTypes().length = 0;
	/*
	  map.addMapType(myMapType);
	  map.setMapType(myMapType);
	*/
	map.addMapType(usnobMapType);
	//map.addMapType(cleanMapType);
	map.addMapType(userMapType);
	map.addMapType(userMapType2);
	//map.addMapType(userUsnobMapType);

    map.setMapType(userMapType);

	// Show an overview map?
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

	moveended();
	mapzoomed(map.getZoom(), map.getZoom());
}
