<?php
	function loggit($mesg) {
		error_log($mesg, 3, "/h/42/dstn/software/apache-2.2.3/logs/usnob.log");
	}
	header("Content-type: image/png");
	// DEBUG
	header("Connection: close");
	loggit("Tile request.\n");

	/*
		loggit("HTTP Request headers:\n");
		$headers = apache_request_headers();
		foreach ($headers as $header => $value) {
		   loggit("  $header: $value\n");
		}
	*/
	loggit("Request headers:\n");
	$headers = $_REQUEST;
	foreach ($headers as $header => $value) {
	   loggit("  $header: $value\n");
	}
	
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

	loggit("W=$ws, H=$hs, BB=$bb, EPSG=$epsg, LAYERS=$lay\n");

	if ($epsg != "EPSG:4326") {
			loggit("Wrong EPSG: $epsg.\n");
			header("Content-type: text/html");
			printf("<html><body>Invalid request: bad EPSG $epsg.</body></html>\n\n");
			exit;
	}

	$n = sscanf($bb, "%f,%f,%f,%f", $x0, $y0, $x1, $y1);
	if ($n != 4) {
			loggit("Failed to parse BBOX.\n");
			header("Content-type: text/html");
			echo("<html><body>Invalid request: failed to parse BBOX.</body></html>\n\n");
			exit;
	}
	if ((sscanf($ws, "%d", $w) != 1) ||
		(sscanf($hs, "%d", $h) != 1)) {
			loggit("Failed to parse WIDTH/HEIGHT.\n");
			header("Content-type: text/html");
			printf("<html><body>Invalid request: failed to parse WIDTH or HEIGHT.</body></html>\n\n");
			exit;
	}

	$layers = explode(" ", $lay);
	$lines = false;
	$linesize = 0;
	//loggit("Layers: $layers");
	foreach ($layers as $l => $lval) {
		loggit("Layer $l, val $lval\n");
	}

	if (in_array("lines10", $layers)) {
		loggit("Including RA/DEC lines.\n");
		$lines = true;
		$linesize = 10;
	}

	loggit("x0=$x0, x1=$x1, y0=$y0, y1=$y1.\n");
	loggit("w=$w, h=$h.\n");
	if ($lines) {
		loggit("linesize=$linesize\n");
	}

	// http://ca.php.net/manual/en/function.escapeshellarg.php

	$cmd = sprintf("usnobtile -x %f -y %f -X %f -Y %f -w %d -h %d", $x0, $y0, $x1, $y1, $w, $h);
	if ($lines) {
		$cmd = $cmd . sprintf(" -l %f", $linesize);
	}
	$cmd = $cmd . " | pnmtopng";

	loggit("Command: $cmd\n");

	passthru($cmd);

?> 
