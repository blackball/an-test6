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
	$map = $_REQUEST["map"];

	$sdss_file  = $_REQUEST["SDSS_FILE"];
	$sdss_field = $_REQUEST["SDSS_FIELD"];

	$hpstr = $_REQUEST["HP"];

	loggit("W=$ws, H=$hs, BB=$bb, EPSG=$epsg, LAYERS=$lay\n");
	loggit("SDSS file $sdss_file, field $sdss_field\n");
	loggit("Healpix $hpstr\n");

	if ($epsg != "EPSG:4326") {
			loggit("Wrong EPSG: $epsg.\n");
			header("Content-type: text/html");
			printf("<html><body>Invalid request: bad EPSG $epsg.</body></html>\n\n");
			exit;
	}

	if (sscanf($bb, "%f,%f,%f,%f", $x0, $y0, $x1, $y1) != 4) {
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

	$N = 0;
	if (strlen("$sdss_file") && strlen("$sdss_field")) {
		if ($map == "sdssfield") {
			$gotsdssfield = 1;
		} else {
			$gotsdss = 1;
		}
		if ((sscanf($sdss_file, "%d", $sdssfile) != 1) ||
			(sscanf($sdss_field, "%d", $sdssfield) != 1) ||
			($sdssfile < 1) || ($sdssfile > 35) || ($sdssfield < 0) || ($sdssfield > 10000)) {
			loggit("Failed to parse SDSS FILE/FIELD.\n");
			header("Content-type: text/html");
			printf("<html><body>Invalid request: failed to parse SDSS_{FILE,FIELD}.</body></html>\n\n");
			exit;
		}
		$nstr = $_REQUEST["N"];
		if (strlen($nstr)) {
			if (sscanf($nstr, "%d", $N) != 1) {
				loggit("Failed to parse N.\n");
				header("Content-type: text/html");
				printf("<html><body>Invalid request: failed to parse.</body></html>\n\n");
				exit;
			}
		}
	}
	if (strlen("$hpstr")) {
		$gothp = 1;
		if (sscanf($hpstr, "%d", $hp) != 1) {
			loggit("Failed to parse healpix.\n");
			header("Content-type: text/html");
			printf("<html><body>Invalid request: failed to parse healpix.</body></html>\n\n");
			exit;
		}
	}

	$transparent = ($_REQUEST["trans"] == "1");

	$layers = explode(" ", $lay);
	$lines = false;
	$linesize = 0;
	//loggit("Layers: $layers");
	//$layerscmd = "";
	foreach ($layers as $l => $lval) {
		loggit("Layer $l, val $lval\n");
		/*
			if (strlen($lval) > 0) {
			$layerscmd += "-l " . escapeshellarg($lval);
			}
		*/
	}

	if (in_array("lines10", $layers)) {
		loggit("Including RA/DEC lines.\n");
		$lines = true;
		$linesize = 10;
	}

	loggit("x0=$x0, x1=$x1, y0=$y0, y1=$y1.\n");
	loggit("w=$w, h=$h.\n");
	loggit("sdss file=$sdssfile field=$sdssfield.\n");
	loggit("hp=$hp\n");
	if ($lines) {
		loggit("linesize=$linesize\n");
	}

	// http://ca.php.net/manual/en/function.escapeshellarg.php

	if ($gothp) {
		$cmd = sprintf("indextile -H %d", $hp);
	} else if ($gotsdss) {
		$cmd = sprintf("sdsstile -s %d -S %d", $sdssfile, $sdssfield);
	} else if ($gotsdssfield) {
		$cmd = sprintf("sdssfieldtile -s %d -S %d", $sdssfile, $sdssfield);
	} else {
		$cmd = "usnobtile";
		if ($lines) {
			$cmd = $cmd . sprintf(" -l %f", $linesize);
		}
	}
	if ($N > 0) {
		$cmd = $cmd . sprintf(" -N %d", $N);
	}
	$cmd = $cmd . sprintf(" -x %f -y %f -X %f -Y %f -w %d -h %d", $x0, $y0, $x1, $y1, $w, $h);
	//$cmd = $cmd . $layerscmd;
	$cmd = $cmd . " | pnmtopng";
	if (($gotsdss || $gothp) && $transparent) {
		// NOTE, that space between "-transparent" and "=black" is supposed
		// to be there!
		$cmd = $cmd . " -transparent =black";
	}

	loggit("Command: $cmd\n");

	passthru($cmd);

?> 
