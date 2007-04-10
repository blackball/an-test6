<?php
require_once 'common.php';

$jobid = $headers['job'];
if ($jobid) {
	if (!verify_jobid($jobid)) {
		die("Job \"" . $jobid . "\" is not a valid job ID.\n");
	}
	$reldir = jobid_to_dir($jobid);
	$dir = $resultdir . $reldir;
	if (!file_exists($dir)) {
		die("Job \"" . $jobid . "\" does not exist.\n");
	}

	$host  = $_SERVER['HTTP_HOST'];
	$myuri  = $_SERVER['PHP_SELF'];
	$myuridir = rtrim(dirname($myuri), '/\\');

	header("Location: http://" . $host . $myuridir . "/status.php?job=" . $jobid);
	exit;
}

$myuri  = $_SERVER['PHP_SELF'];
$myscript = basename($myuri);
?>

<html>
<head>
<title>
Astrometry.net: Find job
</title>
</head>
<body>
<h1>Astrometry.net: Find job</h1>
<hr />
<form action="<?php echo $myscript; ?>" method="get" id="findjobform">
<p>
Job ID:
<input type="text" name="job" size="20" />
<input type="submit" name="submit" value="Find Job" />
</p>
<p>
Valid Job IDs have the form: <b>site-era-number</b>
</p>
<p>
Eg, <b>tor-200704-29154938</b>
</p>
</form>
<hr />
</body>
</html>
