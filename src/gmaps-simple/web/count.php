<?php
	function loggit($mesg) {
		error_log($mesg, 3, "/home/gmaps/usnob-map/usnob.log");
	}
	header("Content-type: text/plain");
	// DEBUG
	header("Connection: close");
	loggit("Count request.\n");

	loggit("Request headers:\n");
	$headers = $_REQUEST;
	foreach ($headers as $header => $value) {
	   loggit("  $header: $value\n");
	}
	
	$needed = array("center_ra", "center_dec", "zoom", "width", "height");
	foreach ($needed as $n => $val) {
		if (!array_key_exists($val, $_REQUEST)) {
			loggit("Request doesn't contain $val.\n");
			header("Content-type: text/html");
			printf("<html><body>Invalid request: missing $val</body></html>\n\n");
			exit;
		}
	}

	$ras = $_REQUEST["center_ra"];
	$decs = $_REQUEST["center_dec"];
	$ws = $_REQUEST["width"];
	$hs = $_REQUEST["height"];
	$zooms = $_REQUEST["zoom"];

	loggit("RA=$ras, DEC=$decs, W=$ws, H=$hs, Zoom=$zooms\n");

	if ((sscanf($ws, "%d", $w) != 1) ||
		(sscanf($hs, "%d", $h) != 1) ||
		(sscanf($ras, "%f", $ra) != 1) ||
		(sscanf($decs, "%f", $dec) != 1) ||
		(sscanf($zooms, "%d", $zoom) != 1)) {
			loggit("Failed to parse width/height/ra/dec/zoom.\n");
			header("Content-type: text/html");
			printf("<html><body>Invalid request: failed to parse WIDTH or HEIGHT.</body></html>\n\n");
			exit;
	}

	loggit("ra=$ra, dec=$dec, w=$w, h=$h, z=$zoom\n");

	$cmd = sprintf("usnobcount -r %f -d %f -w %d -h %d -z %d", $ra, $dec, $w, $h, $zoom);

	loggit("Command: $cmd\n");

	passthru($cmd);

?> 
