<?php
$resultdir = "/home/gmaps/ontheweb-data/";
$statuspath = "status/";
$check_xhtml = 1;

$headers = $_REQUEST;

if (!array_key_exists("job", $headers)) {
	echo "<h3>No \"job\" argument</h3></body></html>\n";
	exit;
}

$myname = $headers["job"];
$mydir = $resultdir . $myname . "/";

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
$rdlist = $mydir . "field.rd.fits";
$logfile = $mydir . "log";
$solvedfile = $mydir . "solved";
$wcsfile = $mydir . "wcs.fits";

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

function print_link($f) {
	global $statusurl;
	if (file_exists($f)) {
		echo "<a href=" . $statusurl . basename($f) . ">" .
			basename($f) . "</a>";
	} else {
		echo "(not available)";
	}
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

if ($job_done) {
	echo '<tr><td>Field Solved:</td><td>';
	if (file_exists($solvedfile)) {
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

	echo '<tr><td>WCS file:</td><td>';
	print_link($wcsfile);
	echo "</td></tr>\n";

	echo '<tr><td>RA,DEC list:</td><td>';
	print_link($rdlist);
	echo "</td></tr>\n";
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