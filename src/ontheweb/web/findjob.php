<?php
require_once 'common.php';
require_once 'rfc822.php';

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

$email = $headers['email'];
if ($email) {
	if (!is_valid_email_address($email)) {
		die("Invalid email address.");
	}
	echo "<html><head><title>Astrometry.net: jobs for " . $email . "</title></head>" .
		'<body><h1>Astrometry.net: jobs for ' . $email . '</h1><hr />' .
		'<table border="1" cellpadding="3">';
	$sites = scandir($resultdir);
	foreach ($sites as $site) {
		if ($site == "." || $site == "..")
			continue;
		if (!preg_match($sitepat, $site))
			continue;
		$sitedir = $resultdir . $site . "/";
		$eras = scandir($sitedir);
		foreach ($eras as $era) {
			if ($era == "." || $era == "..")
				continue;
			if (!preg_match($erapat, $era))
				continue;
			$eradir = $sitedir . $era . "/";
			$nums = scandir($eradir);
			foreach ($nums as $num) {
				if ($num == "." || $num == "..")
					continue;
				if (!preg_match($numpat, $num))
					continue;
				$numdir = $eradir . $num . "/";

				$jd = $numdir . "/" . $jobdata_fn;
				
				if (!file_exists($jd))
					continue;
				$db = connect_db($jd, TRUE);
				if (!$db) {
					//loggit("no db\n");
					continue;
				}
				$jobemail = getjobdata($db, 'email');
				disconnect_db($db);
				if (!$jobemail) {
					//loggit("no email\n");
					continue;
				}
				if (strcmp($jobemail, $email) == 0) {
					$reldir = $site . "/" . $era . "/" . $num;
					$jobid = dir_to_jobid($reldir);
					echo '<tr><td><a href="status.php?job=' . $jobid . '">' .
						$jobid . "</a></td></tr>\n";
				}
			}
		}
	}
	echo "</table><hr /></body></html>\n";
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
<h3>By job ID</h3>
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
<h3>By your email address:</h3>
<form action="<?php echo $myscript; ?>" method="get" id="findemailform">
<p>
Email address:
<input type="text" name="email" size="30" />
<input type="submit" name="submit" value="Find Jobs" />
</p>
</form>
<hr />
</body>
</html>
