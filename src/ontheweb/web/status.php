<html>
<head>
<title>
Astrometry.net: Status
</title>
</head>
<body>
<?php
$resultdir = "/h/260/dstn/local/ontheweb-results/";
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
	echo "<h3>Attempted tricky \"job\" argument.  Naughty!</h3></body></html>\n";
	exit;
}

$inputfile = $mydir . "input";
$startfile = $mydir . "start";


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
?>

<h2>
Astrometry.net Job Status
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
$found = FALSE;
$files = array($startfile,
			   $startfile . "0");
foreach ($files as $f) {
	if (file_exists($f)) {
		$t = filemtime($f);
		$dt = dtime2str($now - $t);
		echo $dt . " ago";
		$found = TRUE;
		break;
	}
}
if (!$found) {
	echo "(job not started)";
}
?>
</td></tr>

</table>
</center>

<hr>

</body>
</html>