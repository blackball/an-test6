<?php
$resultdir = "/home/gmaps/ontheweb-data/";
$gmaps_url = "http://oven.cosmo.fas.nyu.edu/usnob/";
$statuspath = "status/";
$check_xhtml = 1;

$headers = $_REQUEST;

if (!array_key_exists("job", $headers)) {
	echo "<h3>No \"job\" argument</h3></body></html>\n";
	exit;
}

$myname = $headers["job"];
$mydir = $resultdir . $myname . "/";

$img = array_key_exists("img", $headers);
$overlay = array_key_exists("overlay", $headers);

// Make sure the path is legit...
$rp1 = realpath($resultdir);
$rp2 = realpath($mydir);
if (substr($rp2, 0, strlen($rp1)) != $rp1) {
	echo "<h3>Attempted tricky \"job\" argument.  Naughty!</h3>";
	echo "<pre>" . $rp1 . "\n" . $rp2 . "\n";
	echo substr($rp2, 0, strlen($rp1)) . "\n</pre></body></html>\n";
	exit;
}

$qfile = $resultdir . "queue";
$inputfile = $mydir . "input";
$startfile = $mydir . "start";
$donefile  = $mydir . "done";
$xylist = $mydir . "field.xy.fits";
$rdlist = $mydir . "field.rd.fits";
$logfile = $mydir . "log";
$solvedfile = $mydir . "solved";
$wcsfile = $mydir . "wcs.fits";
$objsfile = $mydir . "objs.png";
$overlayfile = $mydir . "overlay.png";

if (!$img && !file_exists($inputfile) && file_exists($inputfile . ".tmp")) {
	// Rename it...
	if (!rename($inputfile . ".tmp", $inputfile)) {
		echo "<html><body><h3>Failed to move temp file from " . $fitstempfilename . " to " . $newname . "</h3></body></html>";
		exit;
	}
	// Hack - pause a moment...
	sleep(1);
}

$input_exists = file_exists($inputfile);
$job_submitted = $input_exists;
$job_started = file_exists($startfile);
$job_done = file_exists($donefile);
$job_queued = $job_submitted && !($job_started);

$do_refresh = !array_key_exists("norefresh", $headers);

if (!$job_submitted) {
	// input file not found; don't refresh.
	$do_refresh = 0;
}
if ($job_done) {
	// job done; don't refresh.
	$do_refresh = 0;
}
if ($img) {
	$do_refresh = 0;
}
?>

<!DOCTYPE html 
     PUBLIC "-//W3C//DTD XHTML 1.0 Strict//EN"
    "http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd">
<html xmlns="http://www.w3.org/1999/xhtml" xml:lang="en" lang="en">
<head>
<title>
Astrometry.net: Job Status
</title>
<style type="text/css">
table.c {margin-left:auto; margin-right:auto;}
p.c {margin-left:auto; margin-right:auto; text-align:center;}
form.c {margin-left:auto; margin-right:auto; text-align:center;}
</style>

<?php
if ($do_refresh) {
	echo '<meta http-equiv="refresh" content="5">';
	// content="5; URL=html-redirect.html"
}
?>

</head>
<body>


<?php
$statuspath .= $myname . "/";
$host  = $_SERVER['HTTP_HOST'];
$uri  = rtrim(dirname($_SERVER['PHP_SELF']), '/\\');
$statusurl = "http://" . $host . $uri . "/" . $statuspath;

$now = time();

function dtime2str($secs) {
	if ($secs > 3600*24) {
		return sprintf("%.1f days", (float)$secs/(float)(3600*24));
	} else if ($secs > 3600) {
		return sprintf("%.1f hours", (float)$secs/(float)(3600));
	} else if ($secs > 60) {
		return sprintf("%.1f minutes", (float)$secs/(float)(60));
	} else {
		return sprintf("%d seconds", $secs);
	}
}

function get_url($f) {
	global $statusurl;
	return $statusurl . basename($f);
}

function print_link($f) {
	if (file_exists($f)) {
		$url = get_url($f);
		echo "<a href=" . $url . ">" .
			basename($f) . "</a>";
	} else {
		echo "(not available)";
	}
}
?>

<?php
if ($img) {
	echo "<h2>Source extraction:</h2>\n";
	echo "<hr />\n";
	echo '<p class="c"><img src="' . get_url($objsfile) . '" ';
	echo 'alt="Your image, overlayed with the objects extracted" /></p>';
	echo "<hr />\n";
	echo '<form action="status.php" method="get" class="c">';
	echo "\n";
	echo "<p class=\"c\">\n";
	echo "<input type=\"hidden\" name=\"job\" value=\"" . $myname . "\" />\n";
	echo "<input type=\"submit\" value=\"Looks good - proceed!\" />\n";
	echo "</p>\n";
	echo "</form>\n";
	echo "<hr />\n";

	if ($check_xhtml) {
		print <<<END
			<p>
			<a href="http://validator.w3.org/check?uri=referer"><img
			src="http://www.w3.org/Icons/valid-xhtml10"
			alt="Valid XHTML 1.0 Strict" height="31" width="88" /></a>
			</p>
END;
	}

	echo "</body></html>\n";

	exit;
}

if ($overlay) {
	if (!file_exists($overlayfile)) {
		// render it!
		// FIXME
	}
	header('Content-type: image/png');
	readfile($overlayfile);
	exit;
}

?>

<h2>
Astrometry.net: Job Status
</h2>

<hr />

<table border="1" class="c">

<tr><td>Job Id</td><td>
<?php
echo $myname
?>
</td></tr>

<tr><td>Status</td><td>
<?php
if (!$job_submitted) {
	echo "Not submitted";
} else if ($job_done) {
	echo "Finished";
} else if ($job_started) {
	echo "Running";
} else if ($job_queued) {
	echo "Submitted";
} else {
	echo "(unknown)";
}
?>
</td></tr>

<?php
if ($job_submitted) {
	echo '<tr><td>Submitted</td><td>';
	$t = filemtime($inputfile);
	$dt = dtime2str($now - $t);
	echo $dt . " ago";
	echo "</td></tr>\n";
}

if ($job_started) {
	echo '<tr><td>Started</td><td>';
	$t = filemtime($startfile);
	$dt = dtime2str($now - $t);
	echo $dt . " ago";
	echo "</td></tr>\n";
}

if ($job_done) {
	echo '<tr><td>Finished</td><td>';
	$t = filemtime($donefile);
	$dt = dtime2str($now - $t);
	echo $dt . " ago";
	echo "</td></tr>\n";
}

if ($job_queued) {
	echo '<tr><td>Position in Queue:</td><td>';
	$q = @file($qfile);
	$pos = -1;
	for ($i=0; $i<count($q); $i++) {
		$entry = rtrim($q[$i]);
		if ($entry == $myname) {
			$pos = $i;
			break;
		}
	}
	if ($pos == -1) {
		echo '(job not found)';
	} else {
		echo sprintf("%d of %d", $pos+1, count($q));
	}
	echo "</td></tr>\n";
}

if ($job_submitted && file_exists($objsfile) && (file_exists($xylist))) {
	echo "<tr><td>Extracted objects:</td><td>\n";
	print_link($xylist);
	echo "</td></tr>\n";
}

if ($job_done) {
	echo '<tr><td>Field Solved:</td><td>';
	$didsolve = file_exists($solvedfile);
	if ($didsolve) {
		$fin = fopen($solvedfile, "rb");
		if (!$fin) {
			echo "(failed to open file)";
		} else {
			$s = '';
			while (!feof($fin)) {
				$rd = fread($fin, 1024);
				if ($rd == FALSE) {
					echo "(error reading file)";
					break;
				}
				$s .= $rd;
			}
			//echo 'Read ' . strlen($s) . ' entries.';
			if (ord($s[0]) == 1) {
				echo "yes";
			} else if (ord($s[0]) == 0) {
				echo "no";
			} else {
				echo "(unexpected value " . ord($s[0]) . ")";
			}
			fclose($fin);
		}
	} else {
		echo "no";
	}
	echo "</td></tr>\n";

	if ($didsolve) {
		echo '<tr><td>WCS file:</td><td>';
		print_link($wcsfile);
		echo "</td></tr>\n";

		echo '<tr><td>RA,DEC list:</td><td>';
		print_link($rdlist);
		echo "</td></tr>\n";

		if (file_exists($objsfile)) {
			echo '<tr><td>Graphical representation:</td><td>';
			echo "<a href=\"";
			echo "http://" . $host . $uri . "/status.php?job=" . $myname . "&overlay";
			echo "\">overlay.png</a>\n";
			echo "</td></tr>\n";
		}

		echo '<tr><td>Google Maps view:</td><td>';
		echo "<a href=\"";
		echo $gmaps_url . "?ra=" . "&dec=" . "&zoom=" . "&over=no" .
			"&rdls=" . $myname . "/field.rd.fits" .
			"&view=r+i";
		echo "\">browse</a>\n";
		echo "</td></tr>\n";
	}
}
?>

<tr><td>Log file:</td><td>
<?php
print_link($logfile);
?>
</td></tr>


</table>

<hr />

<table border="1" class="c">
<tr><td>Log File</td></tr>
<tr><td>
<pre>
<?php
if (file_exists($logfile)) {
	echo file_get_contents($logfile);
} else {
	echo "(not available)";
}
?>
</pre>
</td></tr>
</table>

<hr />

<?php
if ($check_xhtml) {
print <<<END
<p>
    <a href="http://validator.w3.org/check?uri=referer"><img
        src="http://www.w3.org/Icons/valid-xhtml10"
        alt="Valid XHTML 1.0 Strict" height="31" width="88" /></a>
</p>
END;
}
?>  

</body>
</html>
