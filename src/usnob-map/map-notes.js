function addoverlay() {
	var coarseGrid = new Array(0);
	var r;
	var bounds = map.getBounds();
	var sw = bounds.getSouthWest();
	var ne = bounds.getNorthEast();
	var rlow = sw.lng();
	var rhigh = ne.lng();
	rlow  = 10.0 * Math.floor(rlow  / 10.0);
	rhigh = 10.0 * Math.ceil(rhigh / 10.0);
	//document.write("rlow = " + rlow + ", rhigh = " + rhigh);
	var points;
	var pl;
	for (r=rlow; r<=rhigh; r+=10) {
		points = new Array(0);
		points.push(new GLatLng(-85, r));
		points.push(new GLatLng(85, r));
		pl = new GPolyline(points, '#ff0000', 0.5, 0.25);
		coarseGrid.push(pl);
		map.addOverlay(pl);
	}
	var d;
	var dlow = sw.lat();
	var dhigh = ne.lat();
	dlow  = 10.0 * Math.floor(dlow  / 10.0);
	dhigh = 10.0 * Math.ceil (dhigh / 10.0);
	for (d=dlow; d<=dhigh; d+=10) {
		points = new Array(0);
		points.push(new GLatLng(d,  0));
		points.push(new GLatLng(d,  180));
		points.push(new GLatLng(d,  0));
		points.push(new GLatLng(d,  -180));
		pl = new GPolyline(points, '#ff0000', 0.5, .25);
		coarseGrid.push(pl);
		map.addOverlay(pl);
		
		points = new Array(0);
		//points.push(new GLatLng(d,  -180));
		//points.push(new GLatLng(d,  -350));
		//points.push(new GLatLng(d,  0));
		points.push(new GLatLng(d,  -300, true));
		points.push(new GLatLng(d,  300, true));
		pl = new GPolyline(points, '#00ff00', 2, .25);
		coarseGrid.push(pl);
		map.addOverlay(pl);
	}
}




var request = GXmlHttp.create();
request.open("GET", "example.xml", true);
request.onreadystatechange = function() {
if (request.readyState == 4) {
var xmlDoc = request.responseXML;
// obtain the array of markers and loop through it
var markers = xmlDoc.documentElement.getElementsByTagName("marker");

for (var i = 0; i < markers.length; i++) {
// obtain the attribues of each marker
var lat = parseFloat(markers[i].getAttribute("lat"));
var lng = parseFloat(markers[i].getAttribute("lng"));
var point = new GLatLng(lat,lng);
var html = markers[i].getAttribute("html");
var label = markers[i].getAttribute("label");
// create the marker
var marker = createMarker(point,label,html);
map.addOverlay(marker);
}
// put the assembled sidebar_html contents into the sidebar div
document.getElementById("sidebar").innerHTML = sidebar_html;
}
}
request.send(null);




<input type="button" value="Button 1"
onclick= "document.getElementById('text1').style.visibility= 'visible';">
<div id="text1" style="visibility:hidden;">
This text is revealed by button 1 and hidden by button 3.
<input type="button" value="Button 3"
onclick="document.getElementById('text1').style.visibility = 'hidden';">
</div>
<input type="button" value="Button 2"
onclick="document.getElementById('text2').style.visibility= 'visible';">
<div id="text2" style="visibility:hidden;">
This text is revealed by button 2 and hidden by button 4.
<input type="button" value="Button 3"
onclick="document.getElementById('text2').style.visibility = 'hidden';">
</div>
