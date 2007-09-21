<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Strict//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
<head>
<meta http-equiv="content-type" content="text/html; charset=utf-8"/>
<link rel="shortcut icon" type="image/x-icon" href="favicon.ico">
<link rel="icon" type="image/png" href="favicon.png">
<title>Astrometry.net Stellar Browser</title>
<style type="text/css">
	.dispnumber {
		border: 1px solid black;
		color: white;
		background-color: black;
		padding: 3px;
	}
	.editnumber {
		border: 1px solid white;
		color: white;
		background-color: black;
		padding: 3px;
	}
	#gobox {
		position: absolute;
		bottom: 10px;
		left: 90px;
		font-family: monospace;
		color: white;
	}
	#map {
		position: absolute; top:0; left:0; width: 100%; height: 100%;
		background-color: black;
	}
	#toggles {
		position: absolute;
		right:10px;
		top:10px;
		color:white;
		z-index:10000;
		font-size: 12px;
		font-weight: bold;
	}
	#toggles a {
		color: #666;
	}
</style>
</head>

<?php
if (strcasecmp($_SERVER['HTTP_HOST'], "oven.cosmo.fas.nyu.edu") == 0) {
	echo '<script src="http://maps.google.com/maps?file=api&v=2&key=ABQIAAAA7dWWcc9pB-GTzZE7CvT6SRRWPaQfegoYWoxyhnGKpr3zYcSQBxSyAqZ1tFdI4Vptc_OSe3RdgwHPIA" type="text/javascript"> </script>';
} else if (strcasecmp($_SERVER['HTTP_HOST'], "explore.astrometry.net") == 0) {
	echo '<script src="http://maps.google.com/maps?file=api&v=2&key=ABQIAAAAlvYHC2LeZmCsJFCFk03fhhRnW50g22O78mhXjuWvuhZDh1wNNBQKWzt3ADML7agkUNb_99esy9S_8w" type="text/javascript"> </script>';
} else {
	echo "No Google Maps key for you!";
}
?>

<script SRC="wms236.js" TYPE="text/javascript"> </script>

<body onload="startup()" onunload="GUnload()">
<center>
<!-- The map will go here:-->
<div id="map" style=""></div>
</center>

<br>
<div id="gobox">
<FORM NAME="gotoform" ACTION="" METHOD="GET">
&nbsp;mouse ra: <INPUT class="dispnumber" TYPE="text" NAME="ra_mouse" VALUE="" readonly size=10>
dec:      <INPUT class="dispnumber" TYPE="text" NAME="dec_mouse" VALUE="" readonly size=10> degrees
<br>
center ra: <INPUT class="editnumber" TYPE="text" NAME="ra_center" VALUE="" onChange="moveCenter()" size=10>
dec:       <INPUT class="editnumber" TYPE="text" NAME="dec_center" VALUE="" onChange="moveCenter()" size=10>
degrees.
zoom level: <input class="editnumber" TYPE="text" NAME="zoomlevel" VALUE="" onChange="moveCenter()" size=4>
<INPUT class="editnumber" TYPE="button" NAME="set_center" VALUE="Go!" onClick="moveCenter()">
<input class="editnumber" type="button" name="linkhere" value="Link to this view" onClick="linktohere()">
Gain: <input type="text" name="gain" value="0" onChange="changeGain()" size=4 />
Arcsinh mapping: <input type="checkbox" name="arcsinh" checked="checked" onChange="changeArcsinh()" />
</FORM>
</div>

<div id="toggles">
	<a id="constellationToggleButton" href="#" onClick="toggleOverlay('constellation')"> Constellations</a> | 
	<a id="gridToggleButton" href="#" onClick="toggleOverlay('grid')"> Grid</a> | 
	<a id="messierToggleButton" href="#" onClick="toggleOverlay('messier')"> Messier objects</a> |
    <a id="apodToggleButton" href="#" onClick="toggleOverlayRestack('apod')">Images </a> |
    <a id="apodOutlineToggleButton" href="#" onClick="toggleApodOutline()"> Image outlines</a> |
    <a id="tychoToggleButton" href="#" onClick="toggleOverlayRestack('tycho')"> Tycho-2 </a> |
    <a id="usnobToggleButton" href="#" onClick="toggleOverlayRestack('usnob')"> USNO-B </a>
</div>

<br>

<!--
<center>
<!-- Add a textbox for printing debugging info.-->
Debug:
  <FORM name="debugform" ACTION="" METHOD="GET">
  <textarea COLS=100 ROWS=10 NAME="debug" READONLY>
  </textarea>
  </FORM>

</center>
-->

<!-- Include the map itself. -->
<script SRC="map.js" TYPE="text/javascript"> </script>

</body>
</html>
