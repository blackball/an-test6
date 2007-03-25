<?php
require_once 'MDB2.php';
require 'common.php';
?>
<html>
<head>
<title>
Astrometry.net Summary Page
</title>
</head>
<body>

<h1>
Astrometry.net: Job Summary
</h1>

<hr />

<h3>
Current Job Queue:
</h3>

<?php
$q = file_get_contents($resultdir . $q_fn);
if (strlen($q)) {
	echo "<table border=1>" .
		"<tr><td>\n" .
		"<pre>" . $q . "</pre>\n" .
		"</td></tr>" .
		"</table>\n";
} else {
	echo "<p>(empty)</p>\n";
}
?>

<hr />

<h3>
Past Jobs:
</h3>

<table border=1>
<tr>
<th>Date</th>
<th>Job Id</th>
<th>Name</th>
<th>Email</th>
<th>Status</th>
<th>Status Page</th>
<th>Image</th>
</tr>

<?php
// search through the $resultdir directory tree, displaying job status
// for each job found.

$lst = scandir($resultdir);
$joblist = array();
foreach ($lst as $name) {
	$dir = $resultdir . $name;
	if ($name == "." || $name == "..")
		continue;
	if (!is_dir($dir))
		continue;
	$props = dir_status($dir . "/");
	if (!$props)
		continue;

	$st = stat($dir);

	$props['ctime'] = $st['ctime'];
	$props['jobid'] = $name;

	array_push($joblist, $props);
}

// sort
$sortlist = array();
foreach ($joblist as $job) {
	$sortlist[$job['ctime']] = $job;
}
$sortedkeys = array_keys($sortlist);
rsort($sortedkeys);
$sortedlist = array();
foreach ($sortedkeys as $k) {
	array_push($sortedlist, $sortlist[$k]);
}
$sortlist = $sortedlist;
/*
	foreach ($sortlist as $k => $job) {
	loggit($job['ctime'] . ": " . date("Y-M-d H:i:s", $job['ctime']) . "\n");
	}
*/
$joblist = array_values($sortlist);

$host  = $_SERVER['HTTP_HOST'];
$uri  = rtrim(dirname($_SERVER['PHP_SELF']), '/\\');

foreach ($joblist as $i => $job) {
	$img = $job['displayImagePng'];
	if (!$img) {
		$img = $job['displayImage'];
	}
	echo "<tr>\n" . 
		//"<td>" . date("r", $job['ctime']) . "</td>\n" .
		"<td>" . date("Y-M-d H:i:s", $job['ctime']) . "</td>\n" .
		"<td>" . $job['jobid'] . "</td>\n" .
		"<td>" . $job['uname'] . " </td>\n" .
		"<td><a href=\"mailto:" . $job['email'] . "\">" . $job['email'] . "</a> </td>\n" .
		"<td>" . $job['status'] . "</td>\n" .
		"<td><a href=\"http://" . $host . $uri . "/status.php?job=" . $job['jobid'] . "\">Status</a> </td>\n" .
		"<td><a href=\"http://" . $host . $uri . "/status/" . $job['jobid'] . "/" . $img . "\">Image</a> </td>\n" .
		//"<td>" .  . "</td>\n" .
		"</tr>\n";
}
?>

</table>
</body>
</html>

<?php
function dir_status($mydir) {
	global $jobdata_fn;
	global $solved_fn;
	global $cancel_fn;
	global $done_fn;
	global $overlay_fn;
	global $input_fn;

	$props = array();

	$jobdatafile = $mydir . $jobdata_fn;
	$solvedfile = $mydir . $solved_fn;
	$cancelfile = $mydir . $cancel_fn;
	$donefile  = $mydir . $done_fn;
	$overlayfile = $mydir . $overlay_fn;
	$inputfile = $mydir . $input_fn;

	if (!file_exists($jobdatafile))
		return FALSE;
 
	$db = connect_db($jobdatafile);
	if (!$db) {
		loggit("failed to connect jobdata db: " . $jobdatafile . "\n");
		return FALSE;
	}
	$jd = getalljobdata($db);
	if ($jd === FALSE) {
		loggit("failed to get jobdata: " . $jobdatafile . "\n");
		return FALSE;
	}
	disconnect_db($db);

	$keys = array('email', 'uname', 'displayImage', 'displayImagePng',
				  /*'xysrc', 'imgfile', 'fitsfile',
				  'imgurl', 'fstype', 'fsl', 'fsu', 'fse', 'fsv', 'fsunit',
				  'poserr', 'tweak', 'index'*/
				  );

	foreach ($keys as $k) {
		if (array_key_exists($k, $jd)) {
			$props[$k] = $jd[$k];
		}
	}

	$job_submitted = file_exists($inputfile);
	$job_started = file_exists($startfile);
	$job_done = file_exists($donefile);
	$job_queued = $job_submitted && !($job_started);
	if ($job_done) {
		$job_solved = field_solved($solvedfile, $msg);
	} else {
		$job_solved = FALSE;
	}
	$job_cancelled = file_exists($cancelfile);

	$status = "(unknown)";
	if ($job_cancelled) {
		$status = "Cancelled";
	} else if (!$job_submitted) {
		$status = "Not submitted";
	} else if ($job_done) {
		//$status = "Finished";
		if ($job_solved) {
			//$status .= " (solved)";
			$status = "Solved";
		} else {
			//$status .= " (failed)";
			$status = "Failed";
		}
	} else if ($job_started) {
		$status = "Running";
	} else if ($job_queued) {
		$status = "Submitted";
	}

	$props['status'] = $status;

	return $props;
}


?>
