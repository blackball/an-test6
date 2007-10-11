<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Strict//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
<head>
<meta http-equiv="content-type" content="text/html; charset=utf-8"/>
<link rel="shortcut icon" type="image/x-icon" href="favicon.ico" />
<link rel="icon" type="image/png" href="favicon.png" />
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
	background-image: url(darken.png);
	padding-left: 10px;
	padding-right: 10px;
	padding-top: 5px;
	padding-bottom: 5px;
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
	background-image: url(darken.png);
	padding-left: 10px;
	padding-right: 10px;
	padding-top: 5px;
	padding-bottom: 5px;
	}
	#toggles a {
		color: #666;
	}
	#imagelistholder {
	position: absolute;
	right:10px;
	bottom:50px;
	color:white;
	z-index:10000;
	font-size: 12px;
	font-weight: bold;
	background-image: url(darken.png);
	padding-left: 10px;
	padding-right: 10px;
	padding-top: 5px;
	padding-bottom: 5px;
	}
    #imagelist a {
	color: #666;
	}
</style>
</head>

<body onload="startup()" onunload="GUnload()">

<?php
if (strcasecmp($_SERVER['HTTP_HOST'], "oven.cosmo.fas.nyu.edu") == 0) {
	echo '<script src="http://maps.google.com/maps?file=api&amp;v=2&amp;key=ABQIAAAA7dWWcc9pB-GTzZE7CvT6SRRWPaQfegoYWoxyhnGKpr3zYcSQBxSyAqZ1tFdI4Vptc_OSe3RdgwHPIA" type="text/javascript"> </script>';
} else if (strcasecmp($_SERVER['HTTP_HOST'], "explore.astrometry.net") == 0) {
	echo '<script src="http://maps.google.com/maps?file=api&amp;v=2&amp;key=ABQIAAAAlvYHC2LeZmCsJFCFk03fhhRnW50g22O78mhXjuWvuhZDh1wNNBQKWzt3ADML7agkUNb_99esy9S_8w" type="text/javascript"> </script>';
} else {
	echo "No Google Maps key for you!";
}
?>

<!-- The map will go here:-->
<div id="map"></div>

<form id="gotoform" action="" method="get">
<div id="gobox">
&nbsp;mouse ra: <input class="dispnumber" type="text" name="ra_mouse" value="" readonly="readonly" size="10" />
dec:      <input class="dispnumber" type="text" name="dec_mouse" value="" readonly="readonly" size="10" /> degrees
<br />
center ra: <input class="editnumber" type="text" name="ra_center" value="" onchange="moveCenter()" size="10" />
dec:       <input class="editnumber" type="text" name="dec_center" value="" onchange="moveCenter()" size="10" />
degrees.
zoom level: <input class="editnumber" type="text" name="zoomlevel" value="" onchange="moveCenter()" size="4" />
<input class="editnumber" type="button" name="set_center" value="Go!" onclick="moveCenter()" />
<input class="editnumber" type="button" name="linkhere" value="Link to this view" onclick="linktohere()" />
Gain: <input type="text" name="gain" value="0" onchange="changeGain()" size="4" />
Arcsinh mapping: <input type="checkbox" name="arcsinh" checked="checked" onchange="changeArcsinh()" />
</div>
</form>

<div id="toggles">
<a id="constellationToggleButton" href="#" onclick="toggleLineOverlay('constellation')"> Constellations</a> | 
<a id="gridToggleButton" href="#" onclick="toggleLineOverlay('grid')"> Grid</a> | 
<a id="messierToggleButton" href="#" onclick="toggleLineOverlay('messier')"> Messier objects</a> |
<span id="userImageToggleButtonHolder"></span>
<a id="imagesToggleButton" href="#" onclick="toggleImages()">Images </a> |
<a id="imageOutlinesToggleButton" href="#" onclick="toggleImageOutlines()"> Image outlines</a> |
<a id="tychoToggleButton" href="#" onclick="toggleOverlayRestack('tycho')"> Tycho-2 </a> |
<a id="usnobToggleButton" href="#" onclick="toggleOverlayRestack('usnob')"> USNO-B </a>
</div>

<div id="imagelistholder">
Images in this view:
<div id="imagelist"></div></div>

<p><script src="map.js" type="text/javascript"> </script></p>

</body>
</html>
