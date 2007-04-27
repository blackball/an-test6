<html>
<head>
<title>
Why Not?
</title>
</head>
<body>
<?php

function loggit($mesg) {
	error_log($mesg, 3, '/tmp/whynot.log');
}

$headers = $_REQUEST;

$quadsdir = '/h/260/dstn/an/quads/';
$printsolved = $quadsdir . 'printsolved';
$tablist     = $quadsdir . 'tablist';
$gethealpix  = $quadsdir . 'get-healpix';
$whynot      = $quadsdir . 'whynot2';
$wcs_rd2xy   = $quadsdir . 'wcs-rd2xy';
$plotxy      = $quadsdir . 'plotxy';
$plotquad    = $quadsdir . 'plotquad';

$runs = '/h/260/dstn/raid3/RUNS/';

$myrelurl = $_SERVER['PHP_SELF'];

$run = $headers['run'];
if ($run == 119) {

	$solvedfile = $runs . "119/R1-4/solved/solved.01";
	$xyls   = '/h/260/dstn/raid3/SDSS-SUBFIELDS/subfield_r01.xy.fits';
	$rdls   = '/h/260/dstn/raid3/SDSS-SUBFIELDS/subfield_r01.rd.fits';
	$index  = '/h/260/dstn/raid3/INDEXES/tiny-48/tiny-48-%02d';
	$W = 700;
	$H = 700;

	$field = $headers['field'];
	if ($field || !strcmp($field, "0")) {

		// Which Healpix contains this field?
		$cmd = $tablist . " " . $rdls . '"[' . ($field+1) . '][#row==1]" | tail -n 1';
		$radecstr = shell_exec($cmd);
		if (sscanf($radecstr, " %d %f %f ", $nil, $ra, $dec) != 3) {
			die("failed to parse radec string: \"" . $radecstr . "\", command \"" . $cmd . "\"");
		}
		$hpstr = shell_exec($gethealpix . " -d -- " . $ra . " " . $dec . " | grep ^Healpix=");
		if (sscanf($hpstr, "Healpix=%d in", $hp) != 1) {
			die("failed to parse hp string " . $hpstr);
		}

		echo "<p>Healpix " . $hp . "</p>\n";

		$thisindex = sprintf($index, $hp);

		// Build the input file for whynot.
		$wcsfile = tempnam('/tmp', 'whynot-wcs-');
		$irdlsfile = tempnam('/tmp', 'whynot-irdls-');
		$input =
			"truerdls " . $rdls . "\n" .
			"field " . $xyls . "\n" .
			"fields " . $field . "\n" .
			"index " . $thisindex . "\n" .
			"indexrdls " . $irdlsfile . "\n" .
			"verify_pix 1\n" .
			"wcs " . $wcsfile . "\n" .
			"run\n";

		$inputfile = tempnam('/tmp', 'whynot-input-');
		if (!$inputfile || !file_put_contents($inputfile, $input))
			die("failed to write input file");

		echo "<p>Input file: " . $inputfile . "</p>\n";

		$result = shell_exec($whynot . " < " . $inputfile . " 2>&1");
		echo "<p>Result:<pre>" . $result . "</pre><p>\n";

		// Project index stars to field space.
		$ixylsfile = tempnam('/tmp', 'whynot-ixyls-');
		$cmd = $wcs_rd2xy . " -w " . $wcsfile . " -i " . $irdlsfile .
			" -o " . $ixylsfile;
		loggit("Command: " . $cmd . "\n");
		if (system($cmd, $retval) === FALSE || $retval) {
			die("wcs_rd2xy failed");
		}

		// Plot index stars and field objs.
		$iplotfile = tempnam('/tmp', 'whynot-iplot-');
		if (system($plotxy . " -W " . $W . " -H " . $H .
				   " -i " . $ixylsfile . " -r 3 > " . $iplotfile,
				   $retval) === FALSE || $retval) {
			die("plotxy failed");
		}
		$fplotfile = tempnam('/tmp', 'whynot-fplot-');
		if (system($plotxy . " -W " . $W . " -H " . $H .
				   " -i " . $xyls . " -e " . $field . " > " . $fplotfile,
				   $retval) === FALSE || $retval) {
			die("plotxy failed");
		}
		// Index -> green
		// Field -> red
		$igreenplot = tempnam('/tmp', 'whynot-igreenplot-');
 		$cmd = "pgmtoppm green " . $iplotfile . " > " . $igreenplot;
		if ((system($cmd, $retval) === FALSE) || $retval) {
			die("pgmtoppm (index) failed.");
		}
		$fredplot = tempnam('/tmp', 'whynot-fredplot-');
		$cmd = "pgmtoppm red " . $fplotfile . " > " . $fredplot;
		if ((system($cmd, $retval) === FALSE) || $retval) {
			die("pgmtoppm (index) failed.");
		}
		// Composite.
		$redgreen = tempnam('/tmp', 'whynot-redgreen-');
		$cmd = "pnmcomp -alpha=" . $fplotfile . " " . $fredplot . " " .
			$igreenplot . " " . $redgreen;
		if (system($cmd, $retval) === FALSE || $retval) {
			die("pnmcomp failed.");
		}

		echo "<p>Wrote red-green " . $redgreen . "</p>";



		// Look for "Quad FieldXY" lines in the result file...
		$iquads = tempnam('/tmp', 'whynot-iquads-');
		$rlines = explode("\n", $result);
		$xystr = "";
		foreach ($rlines as $ln) {
			loggit("Line: \"" . $ln . "\"\n");
			if (strpos($ln, "Quad FieldXY ") === FALSE)
				continue;
			$words = explode(" ", rtrim($ln));
			$words = array_slice($words, -8);
			$xy = implode(" ", $words);
			loggit("xy \"" . $xy . "\"\n");
			$xystr .= " " . $xy;
		}
		if (!$xystr) {
			die("No quads in field");
		}
		$cmd = $plotquad . " -W " . $W . " -H " . $H . " -w 2 --" . $xystr .
			" | ppmtopgm > " . $iquads;
		loggit("Cmd: " . $cmd . "\n");
		if (system($cmd, $retval) === FALSE || $retval) {
			die("plotquad failed.");
		}
		
		// Composite.
		$greenquads = tempnam('/tmp', 'whynot-greenquads-');
		$cmd = "pgmtoppm green " . $iquads . " > " . $greenquads;
		if ((system($cmd, $retval) === FALSE) || $retval) {
			die("pgmtoppm (quads) failed.");
		}
		$redgreenquads = tempnam('/tmp', 'whynot-redgreenquads-');
		$cmd = "pnmcomp -alpha=" . $iquads . " " . $greenquads . " " .
			$redgreen . " " . $redgreenquads;
		if (system($cmd, $retval) === FALSE || $retval) {
			die("pnmcomp failed.");
		}

		echo "<p>Wrote red-green-quads " . $redgreenquads . "</p>";

	} else {
		$unsolved = shell_exec($printsolved . " -j -u " . $solvedfile);
		$unsolved = rtrim($unsolved);
		$unsolved = explode(' ', $unsolved);

		echo "<h3>" . count($unsolved) . " Unsolved Fields:</h3>";

		//echo '<table border="1">';
		echo '<p>';
		foreach ($unsolved as $u) {
			//echo '<tr><td><a href="' . $myrelurl . '?run=' . $run . '&field=' . $u . '">' . $u . '</a></td></tr>' . "\n";
			echo '<a href="' . $myrelurl . '?run=' . $run . '&field=' . $u . '">' . $u . '</a>' . "\n";
		}
		echo '</p>';
		//echo '</table>';

	}

	echo "<hr /></body></html>\n";
	exit;
}

?>

<hr />

<ul>
<li><a href="<?php echo $myrelurl . "?run=119"; ?>">Run 119</a>
</ul>

<hr />

</body>
</html>
