<?php

/**
This script gets called by the client every time the map is moved.  It tells us the
new current map bounds.  We parse the arguments and format a command-line for a C
program, which will write an XML file describing the polygons that are visible in the
specified field of view.
*/

/*
Write a message to the log file.
*/
function loggit($mesg) {
	error_log($mesg, 3, "/home/gmaps/gmaps-simple/simple.log");
}

// Where to look for executables.
$path = "/home/gmaps/gmaps-simple/execs/";

// The maximum number of quads to write.
$maxquads = 200;

header("Content-type: text/plain");

loggit("Quad request headers:\n");
$headers = $_REQUEST;
foreach ($headers as $header => $value) {
	loggit("  $header: $value\n");
}
	
$needed = array("ra1", "ra2", "dec1", "dec2", "width", "height");
foreach ($needed as $n => $val) {
	if (!array_key_exists($val, $_REQUEST)) {
		loggit("Request doesn't contain $val.\n");
		header("Content-type: text/html");
		printf("<html><body>Invalid request: missing $val</body></html>\n\n");
		exit;
	}
}

$ra1s = $_REQUEST["ra1"];
$ra2s = $_REQUEST["ra2"];
$dec1s = $_REQUEST["dec1"];
$dec2s = $_REQUEST["dec2"];
$ws = $_REQUEST["width"];
$hs = $_REQUEST["height"];

if ((sscanf($ws, "%d", $w) != 1) ||
	(sscanf($hs, "%d", $h) != 1) ||
	(sscanf($ra1s, "%f", $ra1) != 1) ||
	(sscanf($ra2s, "%f", $ra2) != 1) ||
	(sscanf($dec1s, "%f", $dec1) != 1) ||
	(sscanf($dec2s, "%f", $dec2) != 1)) {
	loggit("Failed to parse width/height/ra/dec/zoom.\n");
	header("Content-type: text/html");
	printf("<html><body>Invalid request: failed to parse WIDTH or HEIGHT.</body></html>\n\n");
	exit;
}

$cmd = "simplequad -m $maxquads";
$cmd = $path . $cmd . sprintf(" -x %f -y %f -X %f -Y %f -w %d -h %d", $ra1, $dec1, $ra2, $dec2, $w, $h);

loggit("Command: $cmd\n");

passthru($cmd);

?> 
