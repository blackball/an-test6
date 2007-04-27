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
$ontheweblogfile = $resultdir . "summary.log";

$host  = $_SERVER['HTTP_HOST'];
$uri  = rtrim(dirname($_SERVER['PHP_SELF']), '/\\');
$myuri  = $_SERVER['PHP_SELF'];

$site = $headers['site'];
$epoch = $headers['epoch'];

if (!$site || !preg_match($sitepat, $site)) {
	// Show sites
	echo "<hr />\n" .
		"<h3>Sites:</h3>\n" .
		'<table border="1">' . "\n";

	$lst = scandir($resultdir);
	foreach ($lst as $name) {
		$dir = $resultdir . $name;
		if ($name == "." || $name == "..")
			continue;
		if (!is_dir($dir))
			continue;
		if (!preg_match($sitepat, $name))
			continue;

		echo '<tr><td><a href="' . $myuri . "?site=" . $name . '">' .
			$name . "</a></td></tr>\n";
	}

	echo "</table>\n" .
		"<hr />\n" .
		"</body></html>\n";
	exit;
}

$q = file_get_contents($resultdir . $site . '/' . $q_fn);
if (strlen($q)) {
	echo "<table border=1>" .
		"<tr><td>\n" .
		"<pre>" . $q . "</pre>\n" .
		"</td></tr>" .
		"</table>\n";
} else {
	echo "<p>(empty)</p>\n";
}

if (!$epoch || !preg_match($epochpat, $epoch)) {
	// Show epochs for this site.
	echo "<hr />\n" .
		"<h3>Epochs:</h3>\n" .
		'<table border="1">' . "\n";

	$sitedir = $resultdir . $site;
	loggit("scandir " . $sitedir . "\n");
	$lst = scandir($sitedir);
	foreach ($lst as $name) {
		loggit("  dir " . $name . "\n");
		$dir = $sitedir . "/" . $name;
		if ($name == "." || $name == "..")
			continue;
		if (!is_dir($dir))
			continue;
		if (!preg_match($epochpat, $name))
			continue;

		echo '<tr><td><a href="' . $myuri . "?site=" . $site . "&epoch=" . $name . '">' .
			$name . "</a></td></tr>\n";
	}

	echo "</table>\n" .
		"<hr />\n" .
		"</body></html>\n";
	exit;
}

?>

<hr />

<h3>
Past Jobs:
</h3>

<table border="1" cellpadding="3">
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

$statuspage = "http://" . $host . $uri . "/status.php";
function get_url($jobid, $fn) {
	global $statuspage;
	return  $statuspage . get_status_url_args($jobid, $fn);
}

$basedir = $resultdir . $site . "/" . $epoch . "/";
loggit("Getting status in dir " . $basedir . "\n");
$lst = scandir($basedir);
//$jobtimes = array();
$alljobs = array();
foreach ($lst as $name) {
	$dir = $basedir . $name;
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

	loggit("  " . $name . "\n");

	$dbfile = $dir . "/" . $jobdata_fn;
	if (!file_exists($dbfile)) {
		loggit("no db " . $dbfile . "\n");
		continue;
	}
	$db = connect_db($dbfile, TRUE);
	if (!$db) {
		loggit("no db\n");
		continue;
	}
	//$date = getjobdata($db, 'submit-date');
	$jd = getalljobdata($db, TRUE);
	disconnect_db($db);
	//if (!$date) {
	if (!$jd) {
		loggit("no date\n");
		continue;
	}
	//$jobtimes[$date] = $name;
	$jd['dir'] = $name;
	array_push($alljobs, $jd);
}

$sort1 = $headers['sort'];
$sort2 = $headers['sort2'];
$dir1  = $headers['sortdir'];
$dir2  = $headers['sortdir2'];

if (!$sort1) {
	$sort1 = 'submit-date';
	$sort2 = FALSE;
	$dir1 = 'r';
}

$keys1 = array();
$keys2 = array();
foreach ($alljobs as $j) {
	$k1 = $j[$sort1];
	array_push($keys1, $k1);
	if ($sort2) {
		$k2 = $j[$sort2];
		array_push($keys2, $k2);
	}
}

if ($dir1 && ($dir1 == 'r')) {
	$d1 = SORT_DESC;
} else {
	$d1 = SORT_ASC;
}
if ($dir2 && ($dir2 == 'r')) {
	$d2 = SORT_DESC;
} else {
	$d2 = SORT_ASC;
}

if ($sort2) {
	array_multisort($keys1, $d1, $keys2, $d2, $alljobs);
} else {
	array_multisort($keys1, $d1, $alljobs);
}

foreach ($alljobs as $j) {
	$jobnum = $j['dir'];
	$jobid = $site . "-" . $epoch . "-" . $jobnum;
	$jobdir = jobid_to_dir($jobid);

	$dir = $basedir . $jobnum;
	$props = dir_status($dir . "/");
	if (!$props)
		continue;

	$date = $props['submit-date'];

	$img = $props['displayImagePng'];
	if (!$img) {
		$img = $props['displayImage'];
	}
	echo "<tr>\n" . 
		"<td>" . $date . "</td>\n" .
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
	global $xyls_fn;

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

	if (!$jd['xylist-hash']) {
		if (file_exists($mydir . $xyls_fn)) {
			$hash = md5(file_get_contents($mydir . $xyls_fn));
			$jd['xylist-hash'] = $hash;
			if (!setjobdata($db, array('xylist-hash'=>$hash))) {
				loggit("Failed to save xylist-hash: " . $mydir . "\n");
			}
		}
	}

	disconnect_db($db);

	$keys = array('email', 'uname', 'displayImage', 'displayImagePng',
				  'submit-date', 'xylist-hash',
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
