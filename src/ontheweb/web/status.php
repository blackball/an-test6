<html>
<head>
<title>
Astrometry.net: Job Status
</title>
</head>
<body>
<?php
//$resultdir = "/h/260/dstn/local/ontheweb-results/";
$resultdir = "/p/learning/stars/ontheweb/";
$statuspath = "status/";
//$statuspath = "";
$headers = $_REQUEST;

if (!array_key_exists("job", $headers)) {
	echo "<h3>No \"job\" argument</h3></body></html>\n";
	exit;
}

$myname = $headers["job"];

$mydir = $resultdir . $myname . "/";

$statuspath .= $myname . "/";
$host  = $_SERVER['HTTP_HOST'];
$uri  = rtrim(dirname($_SERVER['PHP_SELF']), '/\\');
$statusurl = "http://" . $host . $uri . "/" . $statuspath;

// Make sure the path is legit...
$rp1 = realpath($resultdir);
$rp2 = realpath($mydir);
if (substr($rp2, 0, strlen($rp1)) != $rp1) {
	echo "<h3>Attempted tricky \"job\" argument.  Naughty!</h3></body></html>\n";
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

<hr>

<center>
<table border=1>

<tr><td>Job Id</td><td>
<?php
echo $myname
?>
</td></tr>

<tr><td>Submitted</td><td>
<?php
if (file_exists($inputfile)) {
	$t = filemtime($inputfile);
	$dt = dtime2str($now - $t);
	echo $dt . " ago";
} else {
	echo "(job not found!)";
}
?>
</td></tr>

<tr><td>Started</td><td>
<?php
if (file_exists($startfile)) {
	$t = filemtime($startfile);
	$dt = dtime2str($now - $t);
	echo $dt . " ago";
} else {
	echo "(job not started)";
}
?>
</td></tr>

<tr><td>Finished</td><td>
<?php
if (file_exists($donefile)) {
	$t = filemtime($donefile);
	$dt = dtime2str($now - $t);
	echo $dt . " ago";
} else {
	echo "(job not finished)";
}
?>
</td></tr>

<tr><td>Position in Queue:</td><td>
<?php
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
	echo '(not in the queue)';
} else {
	echo sprintf("%d of %d", $pos+1, count($q));
}
?>
</td></tr>

<tr><td>Field Solved:</td><td>
<?php
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
?>
</td></tr>

<tr><td>WCS file:</td><td>
<?php
print_link($wcsfile);
?>
</td></tr>


<tr><td>RA,DEC list:</td><td>
<?php
print_link($rdlist);
?>
</td></tr>

<tr><td>Log file:</td><td>
<?php
print_link($logfile);
?>
</td></tr>


</table>
</center>

<hr>

<center>
<table border=1>
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
</center>

</body>
</html>