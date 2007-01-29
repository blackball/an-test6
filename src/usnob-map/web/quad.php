<?php
	function loggit($mesg) {
		error_log($mesg, 3, "/home/gmaps/usnob-map/usnob.log");
	}
	header("Content-type: text/xml");
	// DEBUG
	header("Connection: close");
	loggit("Quad request.\n");

	//  sample request: GET /usnob/quad.php?file=1&field=0&quad=0,1,2,3

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

	$needed = array("quad", "src");
	foreach ($needed as $n => $val) {
		if (!array_key_exists($val, $_REQUEST)) {
			loggit("Request doesn't contain $val.\n");
			header("Content-type: text/html");
			printf("<html><body>Invalid request: missing $val</body></html>\n\n");
			exit;
		}
	}

	$src = $_REQUEST["src"];
	$is_sdss = ($src == "sdss");
	$is_index = ($src == "index");
	$is_field = ($src == "field");
	if ($is_sdss || $is_field) {
		$needed = array("file", "field");
		foreach ($needed as $n => $val) {
			if (!array_key_exists($val, $_REQUEST)) {
				loggit("Request doesn't contain $val.\n");
				header("Content-type: text/html");
				printf("<html><body>Invalid request: missing $val</body></html>\n\n");
				exit;
			}
		}
	} else if ($is_index) {
		$needed = array("hp");
		foreach ($needed as $n => $val) {
			if (!array_key_exists($val, $_REQUEST)) {
				loggit("Request doesn't contain $val.\n");
				header("Content-type: text/html");
				printf("<html><body>Invalid request: missing $val</body></html>\n\n");
				exit;
			}
		}
	} else {
		loggit("Unknown source $src\n");
		header("Content-type: text/html");
		printf("<html><body>Invalid request: unknown source</body></html>\n\n");
		exit;
	}

	$filestr  = $_REQUEST["file"];
	$fieldstr = $_REQUEST["field"];
	$hpstr    = $_REQUEST["hp"];
	$quadstr  = $_REQUEST["quad"];

	loggit("file=$filestr, field=$fieldstr, quad=$quadstr, hp=$hpstr\n");

	if ($is_sdss || $is_field) {
		 if ((sscanf($filestr, "%d", $filenum) != 1) ||
		     (sscanf($fieldstr, "%d", $fieldnum) != 1)) {
				loggit("Failed to parse file or field.\n");
				header("Content-type: text/html");
				echo("<html><body>Invalid request: failed to parse values.</body></html>\n\n");
				exit;
		}
	} else if ($is_index) {
		 if (sscanf($hpstr, "%d", $hp) != 1) {
			loggit("Failed to parse hp.\n");
			header("Content-type: text/html");
			echo("<html><body>Invalid request: failed to parse values.</body></html>\n\n");
			exit;
		}
	}
	$quadpattern = '/^\d*(,\d*){3}(,\d*(,\d*){3})*$/';
	if (!preg_match($quadpattern, $quadstr)) {
		loggit("Failed to parse quad.\n");
		header("Content-type: text/html");
		echo("<html><body>Invalid request: failed to parse quad.</body></html>\n\n");
		exit;
	}
	$quads = explode(",", $quadstr);

	loggit("file=$filenum, field=$fieldnum, hp=$hp, quads=$quads\n");

	if ($is_sdss || $is_field) {
		$cmd = sprintf("sdssquad -s %d -S %d", $filenum, $fieldnum);
	    if ($is_field) {
			$cmd = $cmd . " -f";
		}
	} else if ($is_index) {
		$cmd = sprintf("indexquad -H %d", $hp);
	}
	//$cmd = $cmd . sprintf(" -q %d -q %d -q %d -q %d", $q1, $q2, $q3, $q4);
	foreach ($quads as $q) {
		$cmd = $cmd . sprintf(" -q %d", $q);
	}
	loggit("Command: $cmd\n");
	passthru($cmd);
?> 
