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
<th>Index RA,Dec list</th>
</tr>

<?php
// search through the $resultdir directory tree, displaying job status
// for each job found.

$ontheweblogfile = $resultdir . "summary.log";

$host  = $_SERVER['HTTP_HOST'];
$uri  = rtrim(dirname($_SERVER['PHP_SELF']), '/\\');
$statuspage = "http://" . $host . $uri . "/status.php";

function get_url($jobid, $fn) {
	global $statuspage;
	return  $statuspage . get_status_url_args($jobid, $fn);
}

$lst = scandir($resultdir);
$jobtimes = array();
foreach ($lst as $name) {
	$dir = $resultdir . $name;
	if ($name == "." || $name == "..")
		continue;
	if (!is_dir($dir))
		continue;

	/*
	$st = @stat($dir . "/" . $input_fn);
	if (!$st)
		continue;
	$jobtimes[$st['ctime']] = $name;
	*/

	$jd = $dir . "/" . $jobdata_fn;
	if (!file_exists($jd)) {
		//loggit("no jd " . $jd . "\n");
		continue;
	}
	$db = connect_db($jd, TRUE);
	if (!$db) {
		//loggit("no db\n");
		continue;
	}
	$date = getjobdata($db, 'submit-date');
	disconnect_db($db);
	if (!$date) {
		//loggit("no date\n");
		continue;
	}
	$jobtimes[$date] = $name;
}

$sortedkeys = array_keys($jobtimes);
rsort($sortedkeys);
foreach ($sortedkeys as $ctime) {
	$jobid = $jobtimes[$ctime];

	$dir = $resultdir . $jobid;
	$props = dir_status($dir . "/");
	if (!$props)
		continue;

	$img = $props['displayImagePng'];
	if (!$img) {
		$img = $props['displayImage'];
	}
	echo "<tr>\n" . 
		"<td>" . $ctime . "</td>\n" .
		"<td>" . $jobid . "</td>\n" .
		"<td>" . $props['uname'] . " </td>\n" .
		"<td><a href=\"mailto:" . $props['email'] . "\">" . $props['email'] . "</a> </td>\n" .
		"<td>" . $props['status'] . "</td>\n" .
		"<td><a href=\"" . $statuspage . "?job=" . $jobid . "\">Status</a> </td>\n";
	if (file_exists($dir . "/" . $img)) {
		echo "<td><a href=\"" . get_url($jobid, $img) . "\">Image</a> </td>\n";
	} else {
		echo "<td> </td>\n";
	}
	if (file_exists($dir . "/" . $indexrdls_fn)) {
		echo "<td><a href=\"" . get_url($jobid, $indexrdls_fn) . "\">Index RDLS</a> </td>\n";
	} else {
		echo "<td> </td>\n";
	}
	//"<td>" .  . "</td>\n" .
	echo "</tr>\n";
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

	//loggit("exists\n");

	if (!file_exists($jobdatafile))
		return FALSE;

	//loggit("connect\n");
 
	$db = @connect_db($jobdatafile, TRUE);
	if (!$db) {
		//loggit("failed to connect jobdata db: " . $jobdatafile . "\n");
		return FALSE;
	}

	//loggit("jd\n");
	$jd = getalljobdata($db, TRUE);
	if ($jd === FALSE) {
		//loggit("failed to get jobdata: " . $jobdatafile . "\n");
		return FALSE;
	}
	//loggit("disc\n");
	disconnect_db($db);

	$keys = array('email', 'uname', 'displayImage', 'displayImagePng',
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
