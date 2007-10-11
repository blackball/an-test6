/*
  This file is part of the Astrometry.net suite.
  Copyright 2006, 2007 Dustin Lang and Keir Mierle.

  The Astrometry.net suite is free software; you can redistribute
  it and/or modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation, version 2.

  The Astrometry.net suite is distributed in the hope that it will be
  useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with the Astrometry.net suite ; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
*/

/*
  This function was borrowed from the file "wms236.js" by John Deck,
  UC Berkeley, which interfaces the Google Maps client to a WMS
  (Web Mapping Service) server.
 */
CustomGetTileUrl = function(a,b,c) {
	var lULP = new GPoint(a.x*256,(a.y+1)*256);
	var lLRP = new GPoint((a.x+1)*256,a.y*256);
	var lUL = G_NORMAL_MAP.getProjection().fromPixelToLatLng(lULP,b,c);
	var lLR = G_NORMAL_MAP.getProjection().fromPixelToLatLng(lLRP,b,c);
	var lBbox=lUL.x+","+lUL.y+","+lLR.x+","+lLR.y;
	var lURL=this.myBaseURL;
	lURL+="&layers=" + this.myLayers;
	if (this.jpeg)
		lURL += "&jpeg";
	lURL+="&bb="+lBbox;
	lURL+="&w=256";
	lURL+="&h=256";
	return lURL;
}

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
	var myurl=new String(window.location);
	// Strip off trailing "#"
	if (myurl.charAt(myurl.length-1) == '#') {
		myurl = myurl.substr(0, myurl.length-1);
	}
	var questionMarkLocation=myurl.search(/\?/);
	if (questionMarkLocation!=-1){
		myurl=myurl.substr(questionMarkLocation+1);
		var getDataArray=myurl.split(/&/g);
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
	if (0) {
		GLog.write(txt);
	}
}

// The GMap2
var map;

// URL of tileserver
var TILE_URL;
var BLACK_URL;

// The arguments in the HTTP request
var getdata;

// args that we pass on.
var passargs = [ 'imagefn', 'wcsfn', 'cc', 'arcsinh', 'arith', //'gain',
    'dashbox', 'cmap',
    'rdlsfn', 'rdlsfield', 'rdlsstyle',
    'rdlsfn2', 'rdlsfield2', 'rdlsstyle2',
				 'density'
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
	if (imagesShowing)
		show.push("images");
	if (imageOutlinesShowing)
		show.push("imageOutlines");
	if (gridShowing)
		show.push("grid");
	if (messierShowing)
		show.push("messier");
	if (constellationShowing)
		show.push("constellation");
	if (userImageShowing)
		show.push('userImage');
	if (userOutlineShowing)
		show.push('userOutline');
	if (userRdlsShowing)
		show.push('userRdls');
	url += "&show=" + show.join(",");

	if ('userimage' in getdata) {
		url += "&userimage=" + getdata['userimage'];
	}

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
var imagesOverlay;
var userImageOverlay;
var selectedImageOverlay;

var gridShowing = 0;
var messierShowing = 0;
var constellationShowing = 0;
var tychoShowing = 0;
var usnobShowing = 0;
var imagesShowing = 0;
var imageOutlinesShowing = 0;
var userImageShowing = 0;
var userOutlineShowing = 0;
var userRdlsShowing = 0;
var selectedImageShowing = 0;

var selectedImages = [];

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

function makeOverlay(layers, tag) {
	var newTile = new GTileLayer(new GCopyrightCollection(""), 1, 17);
	newTile.myLayers=layers;
	newTile.myBaseURL=TILE_URL + tag;
	newTile.getTileUrl=CustomGetTileUrl;
	return new GTileLayerOverlay(newTile);
}

function makeMapType(tiles, label) {
	return new GMapType(tiles, G_SATELLITE_MAP.getProjection(), label, G_SATELLITE_MAP);
}

function getBlackUrl(tile, zoom) {
	return BLACK_URL;
}

var tychoGain = 0; // must match HTML
var tychoArcsinh = 1; // must match HTML

var usnobGain = 0; // must match HTML
var usnobArcsinh = 1; // must match HTML

var imagesGain = 0; // must match HTML

function restackOverlays() {
	map.clearOverlays();
	if (tychoShowing)
		map.addOverlay(tychoOverlay);
	if (usnobShowing)
		map.addOverlay(usnobOverlay);
	if (imagesShowing || imageOutlinesShowing)
		map.addOverlay(imagesOverlay);
	if (userImageShowing || userOutlineShowing || userRdlsShowing)
		map.addOverlay(userImageOverlay);
	if (selectedImageShowing)
		map.addOverlay(selectedImageOverlay);
	if (gridShowing || messierShowing || constellationShowing)
		map.addOverlay(lineOverlay);
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

function toggleImageOutlines() {
	toggleButton("imageOutlines");
	updateImages();
	restackOverlays();
}

function toggleImages() {
	toggleButton("images");
	updateImages();
	restackOverlays();
}

function toggleUserOutline() {
	toggleButton("userOutline");
	updateUserImage();
	restackOverlays();
}

function toggleUserRdls() {
	toggleButton("userRdls");
	updateUserImage();
	restackOverlays();
}

function toggleUserImage() {
	toggleButton("userImage");
	updateUserImage();
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
	lineOverlay = makeOverlay(layerstr, tag);
}

function updateImages() {
	var tag = "&tag=images";
	tag += "&gain=" + imagesGain;
	var lay = [];
	if (imagesShowing)
		lay.push('images');
	if (imageOutlinesShowing)
		lay.push('boundaries');
	imagesOverlay = makeOverlay(lay.join(","), tag);
}

function updateUserImage() {
	var jobid = getdata['userimage'];
	var image = jobid + '/fullsize.png';
	var wcs = jobid + '/wcs.fits';
	var rdls = jobid + '/field.rd.fits';
	var tag = "&imagefn=" + image + "&wcsfn=" + wcs + "&rdlsfn=" + rdls;
	var lay = [];
	if (userImageShowing)
		lay.push('userimage');
	if (userOutlineShowing)
		lay.push('userboundary');
	if (userRdlsShowing)
		lay.push('rdls');
	userImageOverlay = makeOverlay(lay.join(","), tag);
}

function updateTycho() {
	var tag = "&tag=tycho";
	tag += "&gain=" + tychoGain;
	if (tychoArcsinh) {
		tag += "&arcsinh";
	}
	tychoOverlay = makeOverlay('tycho', tag);
}

function updateUsnob() {
	var tag = "&tag=usnob";
	tag += "&gain=" + usnobGain;
	tag += "&cmap=rb";
	if (usnobArcsinh) {
		tag += "&arcsinh";
	}
	usnobOverlay = makeOverlay('usnob', tag);
}

function updateSelectedImage() {
	//var wcs = img + '.wcs';
	//var tag = "&wcsfn=" + wcs + "&ubstyle=y";
	var tag = "&ubstyle=y";
	/*
	  for (i=0; i<selectedImages.length; i++) {
	  tag += "&wcsfn=" + selectedImages[i] + '.wcs';
	  }
	*/
	//tag += "&wcsfn=" + selectedImages.join(',');
	tag += "&wcsfn=";
	for (i=0; i<selectedImages.length; i++) {
		tag += (i ? ',' : '') + selectedImages[i] + '.wcs';
	}
	selectedImageOverlay = makeOverlay('userboundary', tag);
}

function indexOf(arr, element) {
	ind = -1;
	var i;
	for (i=0; i<arr.length; i++) {
		if (arr[i] == element) {
			ind = i;
			break;
		}
	}
	return ind;
}
	
function toggleSelectedImage(img) {
	debug('Toggling ' + img);
	debug('Selected images: [' + selectedImages.join(', ') + ']');
	ind = indexOf(selectedImages, img);
	debug('Ind ' + ind);
	if (ind == -1) {
		selectedImages.push(img);
	} else {
		selectedImages.splice(ind, 1);
	}
	debug('After: [' + selectedImages.join(', ') + ']');
	debug('Len ' + selectedImages.length);
	if (selectedImages.length > 0) {
		selectedImageShowing = 1;
		updateSelectedImage();
	} else {
		selectedImageShowing = 0;
	}
	restackOverlays();
	showhide = document.getElementById('showhide' + img);
	removeAllChildren(showhide);
	if (ind == -1) {
		txt = '[hide]';
	} else {
		txt = '[show]';
	}
	debug('Txt ' + txt);
	showhide.appendChild(document.createTextNode(txt));
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
	imagesGain = gain;
	updateImages();
	restackOverlays();
}

function removeAllChildren(node) {
	while (node.childNodes.length) {
		node.removeChild(node.childNodes[0]);
	}
}

function emptyImageList() {
	imglist = document.getElementById('imagelist');
	removeAllChildren(imglist);
	// ??
	/*
	  if (selectedImageShowing) {
	  selectedImages = [];
	  selectedImageShowing = 0;
	  restackOverlays();
	  }
	*/
}

function imageListLoaded(txt) {
	debug('image list loaded.');
	emptyImageList();
	//debug("txt: " + txt);
	xml = GXml.parse(txt);
	//debug("xml: " + xml);
	imgtags = xml.documentElement.getElementsByTagName("image");
	debug("Found " + imgtags.length + " images.");
	//imglist.appendChild(document.createTextNode("Images in this view:"));
	//imglist.appendChild(document.createElement("br"));

	imgs = [];
	for (i=0; i<imgtags.length; i++) {
		name = imgtags[i].getAttribute('name');
		//debug("Image " + i + ": " + name);
		imgs.push(name);
	}

	debug('Selected images: [' + selectedImages.join(', ') + ']');
	debug('Visible images: [' + imgs.join(', ') + ']');

	// Remove selected images that are no longer visible.
	for (i=0; i<selectedImages.length; i++) {
		ind = indexOf(imgs, selectedImages[i]);
		if (ind == -1) {
			selectedImages.splice(ind, 1);
			i--;
		}
	}

	for (i=0; i<imgs.length; i++) {
		img = imgs[i];
		link = document.createElement("a");
		link.setAttribute('href', BASE_URL + "tile/image/?filename=" + img);
		link.appendChild(document.createTextNode(img));
		imglist.appendChild(link);

		imglist.appendChild(document.createTextNode(" "));

		link2 = document.createElement("a");
		link2.setAttribute('href', '#');
		link2.setAttribute('onclick', 'toggleSelectedImage("' + img + '")');
		link2.setAttribute('id', 'showhide' + img);

		if (indexOf(selectedImages, img) > -1) {
			txt = '[hide]';
		} else {
			txt = '[show]';
		}
		link2.appendChild(document.createTextNode(txt));
		imglist.appendChild(link2);

		imglist.appendChild(document.createElement("br"));
	}
}

function movestarted() {
	emptyImageList();
}

/*
  This function gets called when the user stops moving the map (mouse drag),
  and also after it's moved programmatically (via setCenter(), etc).
*/
function moveended() {
	mapmoved();

	debug('moveended()');

	if (imagesShowing || imageOutlinesShowing) {
		url = BASE_URL + "tile/list/?";
		bounds = map.getBounds();
		sw = bounds.getSouthWest();
		ne = bounds.getNorthEast();
		L = sw.lng();
		R = ne.lng();
		if (L > R) {
			if (L > 180) {
				L -= 360;
			} else {
				R += 360;
			}
		}
		url += "bb=" + L + "," + sw.lat() + "," + R + "," + ne.lat();
		//debug("Downloading: " + url);
		GDownloadUrl(url, imageListLoaded);
	}
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
		TILE_URL = BASE_URL + "tilecache.php?";
		BLACK_URL = BASE_URL + "black.png";
	} else {
		BASE_URL = "http://oven.cosmo.fas.nyu.edu/";
		TILE_URL = BASE_URL + "tile/get/?";
		BLACK_URL = BASE_URL + "tilecache2/black.png";
	}

	// Add pass-thru args
	firstone = true;
	for (var i=0; i<passargs.length; i++) {
		if (passargs[i] in getdata) {
			if (!firstone)
				TILE_URL += "&";
			TILE_URL += passargs[i] + "=" + getdata[passargs[i]];
			firstone = false;
		}
	}

	// Handle user images.
	if ('userimage' in getdata) {
		var holder = document.getElementById("userImageToggleButtonHolder");
		var link = document.createElement("a");
		link.setAttribute('href', '#');
		link.setAttribute('onclick', 'toggleUserImage()');
		link.setAttribute('id', 'userImageToggleButton');
		var button = document.createTextNode("My Image");
		var bar = document.createTextNode(" | ");
		link.appendChild(button);
		holder.appendChild(link);
		holder.appendChild(bar);

		var link2 = document.createElement("a");
		link2.setAttribute('href', '#');
		link2.setAttribute('onclick', 'toggleUserOutline()');
		link2.setAttribute('id', 'userOutlineToggleButton');
		var button2 = document.createTextNode("My Image Outline");
		var bar2 = document.createTextNode(" | ");
		link2.appendChild(button2);
		holder.appendChild(link2);
		holder.appendChild(bar2);

		var link3 = document.createElement("a");
		link3.setAttribute('href', '#');
		link3.setAttribute('onclick', 'toggleUserRdls()');
		link3.setAttribute('id', 'userRdlsToggleButton');
		var button3 = document.createTextNode("My Image Sources");
		var bar3 = document.createTextNode(" | ");
		link3.appendChild(button3);
		holder.appendChild(link3);
		holder.appendChild(bar3);
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

		var layers = [ 'tycho', 'usnob', 'images', 'imageOutlines', 'grid', 'constellation', 'messier', 'userImage', 'userOutline', 'userRdls' ];
		for (i=0; i<layers.length; i++)
			if (layers[i] in show)
				toggleButton(layers[i]);
	} else {
		toggleButton('tycho');
		toggleButton('images');
		if ('userimage' in getdata) {
			toggleButton('userImage');
			toggleButton('userOutline');
		}
	}
	updateLine();
	updateUserImage();
	updateImages();
	restackOverlays();

	// Connect up the event listeners...
	GEvent.addListener(map, "move", mapmoved);
	GEvent.addListener(map, "moveend", moveended);
	GEvent.addListener(map, "movestart", movestarted);
	GEvent.addListener(map, "zoomend", mapzoomed);
	GEvent.addListener(map, "mousemove", mousemoved);
	GEvent.bindDom(window, "resize", map, map.onResize);

	map.addControl(new GLargeMapControl());

	moveended();
	mapzoomed(map.getZoom(), map.getZoom());
}
