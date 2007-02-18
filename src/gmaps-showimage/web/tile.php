<?php

/**
  This script gets called by the client to request a map tile.  Map tiles are
  256x256 pixels and are specified in terms of the RA,DEC bounding box.
  This script just parses the values and formats a command-line for a C program.
 */

/*
Write a message to the log file.
*/
function loggit($mesg) {
	error_log($mesg, 3, "/home/gmaps/gmaps-simple/simple.log");
}

// Where to look for executables.
$path = "/home/gmaps/astrometry/src/gmaps-showimage/execs/";

// Write headers...
header("Content-type: image/png");

loggit("Tile request headers:\n");
$headers = $_REQUEST;
foreach ($headers as $header => $value) {
	loggit("  $header: $value\n");
}
	
// The variables we need...
$needed = array("WIDTH", "HEIGHT", "SRS", "BBOX");
foreach ($needed as $n => $val) {
	if (!array_key_exists($val, $_REQUEST)) {
		loggit("Request doesn't contain $val.\n");
		header("Content-type: text/html");
		printf("<html><body>Invalid request: missing $val</body></html>\n\n");
		exit;
	}
}

$ws = $_REQUEST["WIDTH"];
$hs = $_REQUEST["HEIGHT"];
$bb = $_REQUEST["BBOX"];
$epsg = $_REQUEST["SRS"];
$lay = $_REQUEST["LAYERS"];

// To make transparent tiles.
$transparent = ($_REQUEST["trans"] == "1");

loggit("W=$ws, H=$hs, BB=$bb, EPSG=$epsg, LAYERS=$lay\n");

// This identifies the projection type.
if ($epsg != "EPSG:4326") {
	loggit("Wrong EPSG: $epsg.\n");
	header("Content-type: text/html");
	printf("<html><body>Invalid request: bad EPSG $epsg.</body></html>\n\n");
	exit;
}

// Make sure the bounding box is actually numerical values...
if (sscanf($bb, "%f,%f,%f,%f", $x0, $y0, $x1, $y1) != 4) {
	loggit("Failed to parse BBOX.\n");
	header("Content-type: text/html");
	echo("<html><body>Invalid request: failed to parse BBOX.</body></html>\n\n");
	exit;
}
// Make sure the width and height are numerical...
if ((sscanf($ws, "%d", $w) != 1) ||
	(sscanf($hs, "%d", $h) != 1)) {
	loggit("Failed to parse WIDTH/HEIGHT.\n");
	header("Content-type: text/html");
	printf("<html><body>Invalid request: failed to parse WIDTH or HEIGHT.</body></html>\n\n");
	exit;
}
loggit("x0=$x0, x1=$x1, y0=$y0, y1=$y1, w=$w, h=$h.\n");

/*
$layers = explode(" ", $lay);
foreach ($layers as $l => $lval) {
  loggit("Layer $l, val $lval\n");
}
*/

// The command to run...
$cmd = "showimagetile";
$cmd = $cmd . " -F/home/gmaps/astrometry/src/gmaps-showimage/execs/test/wcs.000 ";
$cmd = $cmd . " -f/home/gmaps/astrometry/src/gmaps-showimage/execs/test/rosette.ppm ";
$cmd = $path . $cmd . sprintf(" -x %f -y %f -X %f -Y %f -w %d -h %d", $x0, $y0, $x1, $y1, $w, $h);

$cmd = $cmd . " | pnmtopng";

if ($transparent) {
	// NOTE, that space between "-transparent" and "=black" is supposed
	// to be there!
	$cmd = $cmd . " -transparent =black";
}

loggit("Command: $cmd\n");

passthru($cmd);

?> 
