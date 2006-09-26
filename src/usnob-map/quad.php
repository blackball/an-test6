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
	
	$needed = array("file", "field", "quad");
	foreach ($needed as $n => $val) {
		if (!array_key_exists($val, $_REQUEST)) {
			loggit("Request doesn't contain $val.\n");
			header("Content-type: text/html");
			printf("<html><body>Invalid request: missing $val</body></html>\n\n");
			exit;
		}
	}

	$filestr  = $_REQUEST["file"];
	$fieldstr = $_REQUEST["field"];
	$quadstr  = $_REQUEST["quad"];

	loggit("file=$filestr, field=$fieldstr, quad=$quadstr\n");

	if ((sscanf($quadstr, "%d,%d,%d,%d", $q1, $q2, $q3, $q4) != 4) ||
	    (sscanf($filestr, "%d", $filenum) != 1) ||
	    (sscanf($fieldstr, "%d", $fieldnum) != 1)) {
			loggit("Failed to parse file, field, or quad.\n");
			header("Content-type: text/html");
			echo("<html><body>Invalid request: failed to parse values.</body></html>\n\n");
			exit;
	}

	loggit("file=$filenum, field=$fieldnum, quad=$q1,$q2,$q3,$q4.\n");

	$cmd = sprintf("sdssquad -s %d -S %d -q %d -q %d -q %d -q %d", $filenum, $fieldnum, $q1, $q2, $q3, $q4);
	loggit("Command: $cmd\n");
	passthru($cmd);
?> 
