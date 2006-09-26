<?php
	function loggit($mesg) {
		error_log($mesg, 3, "/h/42/dstn/software/apache-2.2.3/logs/usnob.log");
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
	$is_index = $(src == "index");
	if ($is_sdss) {
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

	if ($is_sdss) {
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
	if (sscanf($quadstr, "%d,%d,%d,%d", $q1, $q2, $q3, $q4) != 4) {
		loggit("Failed to parse quad.\n");
		header("Content-type: text/html");
		echo("<html><body>Invalid request: failed to parse quad.</body></html>\n\n");
		exit;
	}

	loggit("file=$filenum, field=$fieldnum, hp=$hp, quad=$q1,$q2,$q3,$q4.\n");

	if ($is_sdss) {
		$cmd = sprintf("sdssquad -s %d -S %d", $filenum, $fieldnum);
	} else if ($is_index) {
		$cmd = sprintf("indexquad -H %d", $hp);
	}
	$cmd = $cmd . sprintf(" -q %d -q %d -q %d -q %d", $q1, $q2, $q3, $q4);
	loggit("Command: $cmd\n");
	passthru($cmd);
?> 
