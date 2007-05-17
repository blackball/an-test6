<?php
require_once 'MDB2.php';

require 'common.php';
require 'presets.php';

loggit("status.php: request headers:\n");
$hdrs = apache_request_headers();
foreach ($hdrs as $k=>$v) {
	loggit("  " . $k . " = " . $v . "\n");
}

loggit("status.php: args:\n");
foreach ($headers as $k=>$v) {
	loggit("  " . $k . " = " . $v . "\n");
}

if (!array_key_exists("job", $headers)) {
	die("Missing \"job\" argument.");
}
$myname = $headers["job"];

if (!verify_jobid($myname)) {
	die("Invalid \"job\" argument.");
}
$myreldir = jobid_to_dir($myname);
$mydir = $resultdir . $myreldir . "/";

$host  = $_SERVER['HTTP_HOST'];
$myreluri = $_SERVER['PHP_SELF'];
$myuri = 'http://' . $host . $myreluri;

$img = array_key_exists("img", $headers);
$cancel = array_key_exists("cancel", $headers);
$goback = array_key_exists("goback", $headers);
$addpreset = array_key_exists("addpreset", $headers);
$getfile = $headers['get'];

// Make sure the path is legit...
$rp1 = realpath($resultdir);
$rp2 = realpath($mydir);
if (substr($rp2, 0, strlen($rp1)) != $rp1) {
	die("Invalid \"job\" arg.");
}

if (jobid_split($myname, $site, $epoch, $jobnum)) {
	$qfile = $resultdir . $site . '/' . $q_fn;
}

$inputfile = $mydir . $input_fn;
$inputtmpfile = $mydir . $inputtmp_fn;
$startfile = $mydir . $start_fn;
$donefile  = $mydir . $done_fn;
$xylist = $mydir . $xyls_fn;
$rdlist = $mydir . $rdls_fn;
$indexxylist = $mydir . $indexxyls_fn;
$indexrdlist = $mydir . $indexrdls_fn;
$blindlogfile = $mydir . $log_fn;
$solvedfile = $mydir . $solved_fn;
$cancelfile = $mydir . $cancel_fn;
$wcsfile = $mydir . $wcs_fn;
$matchfile = $mydir . $match_fn;
$objsfile = $mydir . $objs_fn;
$bigobjsfile = $mydir . $bigobjs_fn;
$rdlsinfofile = $mydir . $rdlsinfo_fn;
$wcsinfofile = $mydir . $wcsinfo_fn;
$jobdatafile = $mydir . $jobdata_fn;
$indexxyls = $mydir . $indexxyls_fn;

$mylogfile = $mydir . "loggit";
$ontheweblogfile = $mylogfile;

$db = connect_db($jobdatafile);
if (!$db) {
	fail("failed to connect jobdata db.\n");
}
$jd = getalljobdata($db);
if ($jd === FALSE) {
	fail("failed to get jobdata.\n");
}

// the user's image converted to PNM.
$pnmimg = $jd["displayImage"];
if (!$pnmimg) {
	// BACK COMPAT
	$pnmimg = "image.pnm";
	loggit("No \"displayImage\" entry found: using \"" . $pnmimg . "\".\n");
}
$pnmimg = $mydir . $pnmimg;

if (!$img && !file_exists($inputfile) && file_exists($inputtmpfile)) {
	// Rename it...
	if (!rename($inputtmpfile, $inputfile)) {
		fail("Failed to rename input temp file (" . $inputtmpfile . " to " . $inputfile);
	}
	// Hack - pause a moment...
	sleep(1);
}

if ($getfile) {
	if (strstr($getfile, '/') ||
		strstr($getfile, '..')) {
		die("Invalid \"get\" filename.");
	}

	$todelete = array();

	if (!strcmp($getfile, 'overlay') ||
		!strcmp($getfile, 'overlay-big')) {
		$big = !strcmp($getfile, 'overlay-big');
		render_overlay($mydir, $big, $jd);

	} else if (!strcmp($getfile, $newheader_fn)) {
		render_newheader($fn, $mydir, $jd, $todelete);

	} else {
		$fn = $mydir . $getfile;
		if (!file_exists($fn)) {
			die("No such file.");
		}
	}
	$sz = filesize($fn);
	$attachments = array($wcs_fn, $newheader_fn,
						 );
	if (in_array($getfile, $attachments)) {
		$mimetype = 'application/octet-stream';
		$disp = 'attachment';
	} else {
		$cmd = "file -b -i " . escapeshellarg($fn);
		$mimetype = shell_exec($cmd);
		$disp = 'inline';
	}
	header('Content-Disposition: ' . $disp . '; filename="' . $getfile . '"');
	header('Content-Type: ' . $mimetype);
	header('Content-Length: ' . $sz);
	readfile($fn);

	foreach ($todelete as $del) {
		unlink($del);
	}

	exit;
}

if ($cancel) {
	loggit("cancel requested.\n");
	if (!touch($cancelfile)) {
		fail("Failed to created cancel file.");
	}
	if ($remote) {
		$cmd = "/bin/echo " . $myname . " | ssh -T c27cancel >> " . $mydir . "cancel.log 2>&1";
		if ((system($cmd, $retval) === FALSE) || $retval) {
			loggit("remote cancel failed: retval " . $retval . ", cmd " . $cmd . "\n");
		}
	}
}

if ($goback) {
	$dir   = rtrim(dirname($_SERVER['PHP_SELF']), '/\\');
	$uri  = $dir . "/index.php";

	$quick = ($headers['quick'] == "1");
	if ($quick) {
		$userimage = $jd['imagefilename'];
		$jd['xysrc'] = 'url';
		$jd['imgurl'] = $myuri . get_status_url_args($myname, $userimage);
	}

	$args = format_preset_url($jd, $formDefaults);

	if ($headers['skippreview'] == "1") {
		$args .= "&skippreview=1";
	}

	header("Location: http://" . $host . $uri . $args);
	exit;
}

if ($addpreset) {
	$name = $headers['preset'];
	$patt = '/^[\w-]+$/';
	if (!preg_match($patt, $name)) {
		die("Preset name \"" . $name . "\" is invalid: must consist of " .
			"alphanumeric, '-' and '_' only.");
	}
	if ($jd['xysrc'] != 'url') {
		die("Can only save presets for URL images.");
	}
	add_preset($name, $jd);
	echo "Preset added.\n";
	exit;
}

$input_exists = file_exists($inputfile);
$job_submitted = $input_exists;
$job_started = file_exists($startfile);
$job_done = file_exists($donefile);
$job_queued = $job_submitted && !($job_started);
if ($job_done) {
	 $didsolve = field_solved($solvedfile, $msg);
} else {
	$didsolve = FALSE;
}
$didcancel = file_exists($cancelfile);
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

if ($didsolve) {
	if (file_exists($rdlsinfofile)) {
		$info = file($rdlsinfofile);
		foreach ($info as $str) {
			$words = explode(" ", rtrim($str, "\n"));
			$infomap[$words[0]] = implode(" ", array_slice($words, 1));
		}
		$rac_merc = $infomap["ra_center_merc"];
		$decc_merc = $infomap["dec_center_merc"];
		$zoom = $infomap["zoom_merc"];
		$rac = $infomap["ra_center"];
		$decc = $infomap["dec_center"];
		$fieldsize = $infomap["field_size"];
	}

	if (!array_key_exists("cd11", $jd)) {
		$errfile = $mydir . "wcsinfo.err";
		$cmd = $wcsinfo . " " . $wcsfile . " > " . $wcsinfofile . " 2> " . $errfile;
		loggit("Command: " . $cmd);
		if ((system($cmd, $retval) === FALSE) || $retval) {
			loggit("wcsinfo failed: retval " . $retval . ", cmd " . $cmd . "\n");
			$res = "";
		} else {
			$res = file_get_contents($wcsinfofile);
		}
		//$res = shell_exec($cmd);
		loggit("wcsinfo: " . $res . "\n");
		$lines = explode("\n", $res);
		foreach ($lines as $ln) {
			//loggit("line " . $ln . "\n");
			$words = explode(" ", $ln);
			//loggit("#words: " . count($words) . "\n");
			if (count($words) < 2)
				continue;
			//loggit("words: 0=" . $words[0] . ", 0=" . $words[1] . "\n");
			$wcsvals[$words[0]] = $words[1];
		}
		$cd11 = (float)$wcsvals["cd11"];		
		$cd12 = (float)$wcsvals["cd12"];		
		$cd21 = (float)$wcsvals["cd21"];		
		$cd22 = (float)$wcsvals["cd22"];		
		$det =  (float)$wcsvals["det"];
		$parity = (int)$wcsvals["parity"];
		$orient = (float)$wcsvals["orientation"];
		$pixscale = (float)$wcsvals["pixscale"];

		$setjd = array("cd11" => $cd11,
					   "cd12" => $cd12,
					   "cd21" => $cd21,
					   "cd22" => $cd22,
					   "det" => $det,
					   "trueparity" => $parity,
					   "orientation" => $orient,
					   "pixscale" => $pixscale);
		if (!setjobdata($db, $setjd)) {
			fail("Failed to set WCS-related jobdata entries.");
		}
	} else {
		$cd11 = (float)$jd["cd11"];		
		$cd12 = (float)$jd["cd12"];		
		$cd21 = (float)$jd["cd21"];		
		$cd22 = (float)$jd["cd22"];		
		$det =  (float)$jd["det"];
		$parity = (int)$jd["trueparity"];
		$orient = (float)$jd["orientation"];
		$pixscale = (float)$jd["pixscale"];
	}
}

if (array_key_exists("send-email", $headers)) {
	if ($jd['sent-email'] == 'yes') {

		loggit("Already sent email: headers:\n");
		foreach ($headers as $k => $v) {
			loggit("  " . $k . " = " . $v . "\n");
		}

		fail("already sent email.");
	}
	if (!setjobdata($db, array('sent-email'=>'yes'))) {
		fail("failed to update jobdata : sent-email.\n");
	}

	highlevellog("Job " . $myname . ": email edition: job " .
				 ($didsolve ? "solved" : "failed") . ".\n");

	$host  = $_SERVER['HTTP_HOST'];
	$uri  = rtrim(dirname($_SERVER['PHP_SELF']), '/\\');
	$staturl = "http://" . $host . $uri . "/status.php?job=" . $myname;

	$email = $jd['email'];
	$uname = $jd['uname'];
	$headers = 'From: Astrometry.net <alpha@astrometry.net>' . "\r\n" .
		'Reply-To: dstn@cs.toronto.edu' . "\r\n" .
		'X-Mailer: PHP/' . phpversion() . "\r\n";
	$subject = 'Astrometry.net job ' . $myname;

	if ($didsolve) {
		$message = "Hello again,\n\n" .
			"We're pleased to tell you that we solved your field.\n\n" .
			"You can get the results here:\n" .
			"  " . $staturl . "\n\n" .
			"Please let us know if you have any problems retrieving your results " .
			"or if the solution is wrong.\n\n";
	} else {
		$headers .= 'Cc: Dustin Lang <dstn@cs.toronto.edu>' . "\r\n";
		$message = "Hello again,\n\n" .
			"Sadly, we were unable to solve your field automatically.\n\n" .
			"We may take a look at it and see if we can get it to solve; in this case " .
			"we'll send you an email to let you know.  We may contact you to get more " .
			"details about your field.\n\n";

		$message .= "You can get the details about your job here:\n" .
			"  " . $staturl . "\n\n";
	}

	$message .= "Just to remind you, here are the parameters of your job:\n";
	$jobdesc = describe_job($jd);
	foreach ($jobdesc as $key => $val) {
		$message .= "  " . $key . ": " . $val . "\n";
	}
	$message .= "\n";

	$message .= "If you want to return to the form with the values you entered " .
		"already filled in, you can use this link.  Note, however, that your web browser " .
		"won't let us set a default value for the file upload field, so you may have to " .
		"cut-n-paste the filename.\n";
	$host  = $_SERVER['HTTP_HOST'];
	$uri  = rtrim(dirname($_SERVER['PHP_SELF']), '/\\') . "/index.php";
	$args = format_preset_url($jd, $formDefaults);
	$message .= "  http://" . $host . $uri . $args . "\n\n";

	$message .= "Thanks for using Astrometry.net!\n\n";


	if ($uname) {
		$headers .= 'To: ' . $uname . ' <' . $email . '>' . "\r\n";
	} else {
		$headers .= 'To: ' . $email . "\r\n";
	}

	if (!mail("", $subject, $message, $headers)) {
		phpinfo();
		fail("Failed to send email.\n");
	}
	echo "Email sent.\n";
	exit;
}


if ($didsolve) {
	// The size of the full-sized original image.
	$fullW = $jd["imageW"];
	$fullH = $jd["imageH"];
	$exact = TRUE;
	if (!($fullW && $fullH)) {
		$exact = FALSE;
		$fullW = $jd["xylsW"];
		$fullH = $jd["xylsH"];
		if (!($fullW && $fullH)) {
			loggit("Couldn't get xylsW/H.\n");
		}
	}
}


$status = "(unknown)";
if ($didcancel) {
	$status = "Cancelled";
} else if (!$job_submitted) {
	$status = "Not submitted";
} else if ($job_done) {
	$status = "Finished";
} else if ($job_started) {
	$status = "Running";
} else if ($job_queued) {
	$status = "Submitted";
}

if ($job_done || $didcancel) {
	if (!$jd['checked-done']) {
		if (!setjobdata($db, array('checked-done'=>$status))) {
			loggit("failed to set checked-done jobdata\n");
		}
		if ($status == "Finished") {
			if ($didsolve) {
				$status = "Solved";
			} else {
				$status = "Failed";
			}
		}
		highlevellog("Job " . $myname . ": status " . $status . "\n");
	}
}

?>

<!DOCTYPE html 
     PUBLIC "-//W3C//DTD XHTML 1.0 Strict//EN"
    "http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd">
<html xmlns="http://www.w3.org/1999/xhtml" xml:lang="en" lang="en">
<head>
<title>
Astrometry.net 
<?php
if ($img) {
	echo "(Source extraction for";
} else {
	echo "(" . $status;
}
echo " job " . $myname . ")";
?>
</title>
<style type="text/css">
table.c {margin-left:auto; margin-right:auto;}
p.c {margin-left:auto; margin-right:auto; text-align:center;}
form.c {margin-left:auto; margin-right:auto; text-align:center;}
h3.c {text-align:center;}
#fits2xylog {margin-left:auto; margin-right:auto; border-color:gray; border-style: double;}
#overlay { margin-left:auto; margin-right:auto; text-align:center; }
#overlay > a:link { color:white; }
#overlay > a:visited { color:white; }
#overlay > a:hover { color:gray; }
#overlay > a:active { color:yellow; }
#onsky { margin-left:auto; margin-right:auto; text-align:center; }
#onsky > a:link { color:white; }
#onsky > a:visited { color:white; }
#onsky > a:hover { color:gray; }
#onsky > a:active { color:yellow; }
#sources { margin-left:auto; margin-right:auto; text-align:center; }
</style>

<?php
if ($do_refresh) {
	echo '<meta http-equiv="refresh" content="5; URL=status.php?job=' . $myname . '"/>';
}
?>

<link rel="shortcut icon" type="image/x-icon" href="favicon.ico" />
<link rel="icon" type="image/png" href="favicon.png" />

</head>
<body>


<?php
$now = time();

function get_url($f) {
	global $myname;
	global $myuri;
	return $myuri . get_status_url_args($myname, $f);
}

function print_link($f, $dynamic=FALSE) {
	if (file_exists($f) || $dynamic) {
		$url = get_url(basename($f));
		echo "<a href=\"" . $url . "\">" .
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
	echo '<p class="c">';
	$big = (file_exists($bigobjsfile));
	if ($big) {
		echo '<a href="' . get_url(basename($bigobjsfile)) . '">';
	}
	echo '<img src="' . get_url(basename($objsfile)) . '" ';
	echo 'alt="Your image, overlayed with the objects extracted" />';
	if ($big) {
		echo '</a>';
	}
	echo "</p><hr />\n";
	echo '<form action="status.php" method="get" class="c">';
	echo "\n";
	echo "<p class=\"c\">\n";
	echo "<input type=\"hidden\" name=\"job\" value=\"" . $myname . "\" />\n";
	echo "<input type=\"submit\" value=\"Looks good - proceed!\" />\n";
	echo "</p>\n";
	echo "</form>\n";
	echo "<hr />\n";
	echo "<h3>Log file:</h3>\n";
	echo "<div id=\"fits2xylog\">\n";
	echo "<pre>";
	echo file_get_contents($mydir . $fits2xyout_fn);
	echo "</pre>";
	echo "</div>\n";

	echo $valid_blurb;

	echo "</body></html>\n";

	exit;
}

?>

<h2>
Astrometry.net: Job Status
</h2>

<hr />

<?php
if ($didsolve) {
	echo "<h3 class=\"c\">Your field is at RA,Dec = (" . $rac . ", " . $decc . ") degrees.</h3>\n";
	echo "<hr />\n";

	echo "<div id=\"onsky\">\n";
	echo "<p>Your field on the sky (click for larger image):</p>\n";

	$tileurl = $tiles_url . "?" .
		"LAYERS=tycho,grid,boundary" .
		"&FORMAT=image/png" .
		"&arcsinh" .
		"&wcsfn=" . $myreldir . "/wcs.fits";
	// "tag=test-tag&" .

	$url = $tileurl .
		"&gain=-0.5" .
		"&BBOX=0,-85,360,85";

	$fldsz = $pixscale * max($fullW, $fullH);
	//loggit("Field size: " . $fldsz . "\n");
	$zoomin = ($fldsz < (3600*18));
	$zoomin2 = ($fldsz < (3600*1.8));

	if ($zoomin) {
		echo "<p>(Your field is small so we have drawn a dashed box" .
			" around your field and zoomed in on that region.)</p>\n";
		$url .= "&dashbox=0.1";
	}

	echo "<a href=\"" . htmlentities($url .
		"&WIDTH=1024&HEIGHT=1024&lw=5") .
		"\">";
	echo "<img src=\"" . htmlentities($url . 
		"&WIDTH=300&HEIGHT=300&lw=3") .
		"\" alt=\"An image of your field shown in an image of the whole sky.\" /></a>\n";


	if ($zoomin) {
		//$rac_merc
		$xmerc = ra2merc($rac_merc);
		$ymerc = dec2merc($decc_merc);
		$dm = 0.05;
		$ymerc = max($dm, min(1-$dm, $ymerc));
		$ralo = merc2ra($xmerc - $dm);
		$rahi = merc2ra($xmerc + $dm);
		$declo = merc2dec($ymerc - $dm);
		$dechi = merc2dec($ymerc + $dm);

		$url = $tileurl .
			"&gain=-0.25" .
			"&BBOX=" . $ralo . "," . $declo . "," . $rahi . "," . $dechi;
		if ($zoomin2) {
			$url .= "&dashbox=0.01";
		}

		echo "<a href=\"" . htmlentities($url .
										 "&WIDTH=1024&HEIGHT=1024&lw=5") .
			"\">";
		echo "<img src=\"" . htmlentities($url . 
										  "&WIDTH=300&HEIGHT=300&lw=3") .
			"\" alt=\"A zoomed-in image of your field on the sky.\" /></a>\n";

		if ($zoomin2) {
			$dm = 0.005;
			$ymerc = max($dm, min(1-$dm, $ymerc));
			$ralo = merc2ra($xmerc - $dm);
			$rahi = merc2ra($xmerc + $dm);
			$declo = merc2dec($ymerc - $dm);
			$dechi = merc2dec($ymerc + $dm);

			$url = $tileurl .
				"&gain=0.5" .
				"&BBOX=" . $ralo . "," . $declo . "," . $rahi . "," . $dechi;

			echo "<a href=\"" . htmlentities($url .
											 "&WIDTH=1024&HEIGHT=1024&lw=5") .
				"\">";
			echo "<img src=\"" . htmlentities($url . 
											  "&WIDTH=300&HEIGHT=300&lw=3") .
				"\" alt=\"A really zoomed-in image of your field on the sky.\" /></a>\n";
		}

	}

	echo "</div>\n";
}

//http://oven.cosmo.fas.nyu.edu/tilecache/tilecache.php?&tag=test-tag&REQUEST=GetMap&SERVICE=WMS&VERSION=1.1.1&LAYERS=tycho,image,grid,rdls&STYLES=&FORMAT=image/png&BGCOLOR=0xFFFFFF&TRANSPARENT=TRUE&SRS=EPSG:4326&BBOX=-180,0,0,85.0511287798066&WIDTH=256&HEIGHT=256&reaspect=false

if ($didsolve && file_exists($pnmimg)) {
	$host  = $_SERVER['HTTP_HOST'];
	$uri  = rtrim(dirname($_SERVER['PHP_SELF']), '/\\');
	echo "<div id=\"overlay\">\n";
	echo "<p>Your field plus our index objects:\n";
	echo "<br />Green circles: stars from the index, projected to image coordinates.\n";
	echo "<br />Large red circles: field objects that were examined.\n";
	echo "<br />Small red circles: field objects that were not examined.\n";
	echo "<br />(click for full-size version)\n";
	echo "</p>\n";
	//Your field, overplotted with objects from the index.</p>\n";
	echo "<a href=\"" .
		get_url('overlay-big') . 
		"\">\n";
	echo "<img src=\"" .
		get_url('overlay') . 
		"\" alt=\"An image of your field, showing sources we extracted from the image and objects from our index.\"/>";
	echo "</a>\n";
	echo "</div>\n";
	echo "<hr />\n";
}
?>

<table cellpadding="3" border="1" class="c">

<tr><th colspan="2">Job Status</th></tr>

<tr><td>Job Id:</td><td>
<?php
echo $myname
?>
</td></tr>

<tr><td>Status:</td><td>
<?php
echo $status . "</td></tr>\n";

if (!$job_submitted) {
	// Why?
	$reason = $jd['submit-failure'];
	if ($reason) {
		echo '<tr><td>Failure reason:</td><td>' . $reason . '</td></tr>' . "\n";
	}
}

if ($job_done) {
	echo '<tr><td>Field Solved:</td><td>';
	if (strlen($msg)) {
		echo $msg;
	} else if ($didsolve) {
		echo "Yes";
	} else {
		echo "No";
	}
	echo "</td></tr>\n";
}
?>

<?php
if ($job_submitted) {
	echo '<tr><td>Submitted:</td><td>';
	$date = $jd['submit-date'];
	$t = isodate_to_timestamp($date);
	if (!$t) {
		echo '(error)';
	} else {
		echo get_datestr($t);
		$dt = dtime2str($now - $t);
		echo "<br />(" . $dt . " ago)";
	}
	echo "</td></tr>\n";
}

if ($job_started) {
	$date = $jd['solve-start'];
	$t = isodate_to_timestamp($date);
	if (!$t) {
		//echo '(error)';
	} else {
		echo '<tr><td>Started:</td><td>';
		echo get_datestr($t);
		$dt = dtime2str($now - $t);
		echo "<br />(" . $dt . " ago)";
		echo "</td></tr>\n";
	}
}

if ($job_done) {
	echo '<tr><td>Finished:</td><td>';
	$date = $jd['solve-done'];
	$t = isodate_to_timestamp($date);
	if (!$t) {
		echo '(error)';
	} else {
		echo get_datestr($t);
		$dt = dtime2str($now - $t);
		echo "<br />(" . $dt . " ago)";
	}
	echo "</td></tr>\n";
}

if ($job_queued && $qfile) {
	echo '<tr><td>Position in Queue:</td><td>';
	$q = @file($qfile);
	$pos = -1;
	for ($i=0; $i<count($q); $i++) {
		if (strstr($q[$i], $myreldir)) {
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

if ($job_submitted) {
	$imgs = array('imagePng', 'imagefilename', 'displayImagePng');
	foreach ($imgs as $i) {
		$fn = $jd[$i];
		loggit("file: " . $fn . "\n");
		if ($fn && file_exists($mydir . $fn)) {
			echo "<tr><td>Image:</td><td>\n";
			print_link($mydir . $fn);
			echo "</td></tr>\n";
			break;
		}
	}
}

if ($job_submitted && (file_exists($xylist))) {
	if (file_exists($objsfile)) {
		echo "<tr><td>Extracted sources:</td><td>\n";
		print_link($xylist);
		echo "</td></tr>\n";
		echo "<tr><td>Source extraction image:</td><td>\n";
		print_link($objsfile);
		echo "</td></tr>\n";
		if (file_exists($bigobjsfile)) {
			echo "<tr><td>Source extraction image (full size):</td><td>\n";
			print_link($bigobjsfile);
			echo "</td></tr>\n";
		}
	} else {
		echo "<tr><td>XY list (FITS table):</td><td>\n";
		print_link($xylist);
		echo "</td></tr>\n";
	}
}

if ($job_done) {
	if ($didsolve) {
		echo '<tr><td>(RA, Dec) center:</td><td>';
		echo "(" . $rac . ", " . $decc . ") degrees\n";
		echo "</td></tr>\n";

		echo '<tr><td>Orientation:</td><td>';
		printf("%.2f deg E of N\n", $orient);
		echo "</td></tr>\n";

		echo '<tr><td>Pixel scale:</td><td>';
		printf("%.2f arcsec/pixel\n", $pixscale);
		echo "</td></tr>\n";

		echo '<tr><td>Parity:</td><td>';
		if ($parity == 1) {
			echo "Reverse (\"Left-handed\")";
		} else {
			echo "Normal (\"Right-handed\")";
		}
		echo "</td></tr>\n";

		if ($fullW && $fullH) {
			$fldw = $pixscale * $fullW;
			$fldh = $pixscale * $fullH;
			$units = "arcseconds";
			if (min($fldw, $fldh) > 3600.0) {
				$fldw /= 3660.0;
				$fldh /= 3660.0;
				$units = "degrees";
			} else if (min($fldw, $fldh) > 60.0) {
				$fldw /= 60.0;
				$fldh /= 60.0;
				$units = "arcminutes";
			}
			echo '<tr><td>Field size ';
			if (!$exact) {
				echo "(approximately)";
			}
			echo ':</td><td>';
			printf("%.2f x %.2f %s\n", $fldw, $fldh, $units);
			echo "</td></tr>\n";
		}

		echo '<tr><td>WCS file:</td><td>';
		print_link($wcsfile);
		echo "</td></tr>\n";

		echo '<tr><td>New FITS header:</td><td>';
		print_link($mydir . $newheader_fn, TRUE);
		echo "</td></tr>\n";

		echo '<tr><td>RA, Dec for extracted sources:</td><td>';
		print_link($rdlist);
		echo "</td></tr>\n";

		echo '<tr><td>RA, Dec for index sources:</td><td>';
		print_link($indexrdlist);
		echo "</td></tr>\n";

		echo '<tr><td>Image x, y for index sources:</td><td>';
		print_link($indexxylist);
		echo "</td></tr>\n";

		echo '<tr><td>Google Maps view:</td><td>';
		echo "<a href=\"";
		echo htmlentities($gmaps_url . 
						  "?zoom=" . $zoom . 
						  "&ra=" . $rac_merc .
						  "&dec=" . $decc_merc .
						  "&over=no" .
						  "&rdls=" . $myreldir . "/field.rd.fits" .
						  "&view=r+u&nr=200");
		echo "\">USNOB</a>\n";
		echo "<a href=\"";
		echo htmlentities($gmaps_url .
						  "?gain=" . (($zoom <= 6) ? "-2" : "0") .
						  "&zoom=" . $zoom .
						  "&ra=" . $rac_merc . "&dec=" . $decc_merc .
						  "&over=no" .
						  "&rdls=" . $myreldir . "/field.rd.fits" .
						  "&view=r+t&nr=200&arcsinh");
		echo "\">Tycho-2</a>\n";
		echo "</td></tr>\n";
	}
}


?>

<tr><td>Log file:</td><td>
<?php
print_link($blindlogfile);
?>
</td></tr>


</table>

<?php
if ($job_done && !$didsolve) {
	// Failure
	if (file_exists($objsfile)) {
		echo "<hr />\n";
		echo '<div id="sources">';
		echo "<p>Source extraction:</p>\n";
		echo '<p>';
		if (file_exists($bigobjsfile)) {
			echo '<a href="' . get_url(basename($bigobjsfile)) . '">';
		}
		echo '<img src="' . get_url(basename($objsfile)) .
			'" alt="Source extraction image" />';
		if (file_exists($bigobjsfile)) {
			echo '</a>';
		}
		echo "</p></div>\n";
	}
}
?>

<hr />

<table border="1" class="c">
<tr><th>Tail of the Log File</th></tr>
<tr><td>
<pre>
<?php
if (file_exists($blindlogfile)) {
	passthru("tail " . $blindlogfile);
} else {
	echo "(not available)";
}
?>
</pre>
</td></tr>
</table>

<hr />

<?php
$job = describe_job($jd);
echo '<table border="1" cellpadding="3" class="c">' . "\n";
echo '<tr><th colspan=2>Job Parameters</th></tr>' . "\n";
foreach ($job as $k => $v) {
	echo '<tr><td>' . $k . '</td><td>';
	$makelink = ($k == 'Image URL');
	if ($makelink) {
		echo '<a href="' . $v . '">';
	}
	echo $v;
	if ($makelink) {
		echo '</a>';
	}
	echo '</td></tr>' . "\n";
}
echo '</table>' . "\n";
?>

<hr />

<?php
if (!($job_done || $didcancel)) {
?>

<form id="cancelform" action="status.php" method="get">
<p class="c">
<input type="submit" name="cancel" value="Cancel Job" />
<input type="hidden" name="job" value="<?php echo $myname; ?>" />
</p>
</form>
<hr />

<?php
	 } else {
?>

<form id="gobackform" action="status.php" method="get">
<table class="c">
<tr>
<td rowspan="3">
<input type="hidden" name="job" value="<?php echo $myname; ?>" />
<input type="submit" name="goback" value="Return to Form" />
</td>
<td><input type="radio" name="quick" value="1" checked="checked" /></td>
	 <td>Quick (don<?php echo "'";?>t re-upload the image)</td>
</tr>
<tr>
<td><input type="radio" name="quick" value="0" /></td>
<td>Permalink (re-upload the image)</td>
</tr>
<tr>
<td><input type="checkbox" name="skippreview" value="1" checked="checked" /></td>
<td>Skip source extraction preview</td>
</tr>
</table>
</form>
<hr />

<form id="presetform" action="status.php" method="get">
<table class="c">
<tr>
<td>
<input type="text" name="preset" size="20" />
<input type="hidden" name="job" value="<?php echo $myname; ?>" />
</td>
<td>
<input type="submit" name="addpreset" value="Save Preset" />
</td>
</tr>
</table>
</form>

<hr />

<?php
	  }
echo $valid_blurb;
?>

</body>
</html>

<?php
function render_newheader(&$fn, $mydir, $jd, &$todelete) {
	global $wcs_fn;
	global $fits_filter;
	global $new_wcs;

	$imgfile = $jd['imagefilename'];
	if (!$imgfile) {
		//die("No imagefilename");
		$fn = $mydir . $wcs_fn;
		return;
	}
	$imgfile = $mydir . $imgfile;

	$newfile = tempnam('/tmp', 'uncompressed');
	if (!$newfile) {
		die("Failed to create temporary file.");
	}

	$suff = "";
	if (!uncompress_file($imgfile, $newfile, $suff)) {
		die("Failed to decompress image " . $imgfile);
	}
	if ($suff) {
		$imgfile = $newfile;
		array_push($todelete, $newfile);
	}

	$typestr = shell_exec("file -b -N -L " . $imgfile);
	if (!strstr($typestr, "FITS image data")) {
		//die("Not a FITS image: " . $typestr);
		$fn = $mydir . $wcs_fn;
		return;
	}

	// Run fits2fits.py on it...
	$filtered = tempnam('/tmp', 'filtered');
	$cmd = sprintf($fits_filter, $imgfile, $filtered) . " > /dev/null 2> /dev/null";
	loggit("Command: " . $cmd . "\n");
	if ((system($cmd, $retval) === FALSE) || $retval) {
		loggit("Command failed, return value " . $retval . ": " . $cmd . "\n");
		die("Failed to fix your FITS file.");
	}
	array_push($todelete, $filtered);

	// Merge WCS
	$merged = tempnam('/tmp', 'newheader');
	$cmd = $new_wcs . " -i " . $filtered . " -w " . $mydir . $wcs_fn .
		" -o " . $merged . " > /dev/null 2> /dev/null";
	if ((system($cmd, $retval) === FALSE) || $retval) {
		loggit("Command failed, return value " . $retval . ": " . $cmd . "\n");
		die("Failed to merge your FITS file.");
	}
	array_push($todelete, $merged);

	$fn = $merged;
}

function render_overlay($mydir, $big, $jd) {
	global $overlay_fn;
	global $bigoverlay_fn;
	global $match_fn;
	global $xyls_fn;
	global $indexxyls_fn;

	global $tablist;
	global $plotquad;
	global $plotxy2;

	global $pnmimg;

	$overlayfile = $mydir . $overlay_fn;
	$bigoverlayfile = $mydir . $bigoverlay_fn;
	$matchfile = $mydir . $match_fn;
	$xylist = $mydir . $xyls_fn;
	$indexxyls = $mydir . $indexxyls_fn;

	$todelete = array();

	if ((!$big && !file_exists($overlayfile)) ||
		($big  && !file_exists($bigoverlayfile))) {
		if ($big) {
			$W = $jd['imageW'];
			$H = $jd['imageH'];
			$shrink = 1;
			$userimg = $mydir . "image.pnm";
		} else {
			$W = $jd['displayW'];
			$H = $jd['displayH'];
			$shrink = $jd["imageshrink"];
			if (!$shrink)
				$shrink = 1;
			$userimg = $pnmimg;
		}
		if (!($W && $H)) {
			// BACKWARDS COMPATIBILITY.
			loggit("failed to find image display width and height.\n");
			$W = $jd['imageW'];
			$H = $jd['imageH'];
			if (!($W && $H)) {
				fail("failed to find image width and height.\n");
			}
		}

		$cmd = $tablist . " " . $matchfile . "\"[col fieldobjs]\" | tail -n 1";
		loggit("Command: " . $cmd . "\n");
		$output = shell_exec($cmd);
		//loggit("output: " . $output . "\n");
		if (sscanf($output, " %d %d %d %d %d ", $nil, $fA, $fB, $fC, $fD) != 5) {
			fail("failed to parse field objs.");
		}
		$flds = array($fA, $fB, $fC, $fD);
		$fldobjs = max($flds);
		loggit("field objs: " . implode(" ", $flds) . "\n");

		for ($i=0; $i<4; $i++)
			$fldor[$i] = "#row==" . (1+$flds[$i]);
		$cmd = $tablist . " " . $xylist . "\"[" . implode("||", $fldor) . "][col X;Y]\" | tail -n 4";
		$output = shell_exec($cmd);
		$lines = explode("\n", $output);
		for ($i=0; $i<4; $i++) {
			if (sscanf($lines[$i], " %d %f %f ", $nil, $x, $y) != 3) {
				fail("failed to parse field objs coords: \"" . $lines[$i] . "\"");
			}
			// Here's where we scale down the size of the quad:
			$fldxy[] = $x / $shrink;
			$fldxy[] = $y / $shrink;
		}

		$prefix = $mydir;
		if ($big) {
			$prefix .= "big-";
		}

		$quadimg = $prefix . "quad.pgm";
		$redquad = $prefix . "redquad.pgm";
		$xypgm = $prefix . "index.xy.pgm";
		$fldxy1pgm = $prefix . "fldxy1.pgm";
		$fldxy2pgm = $prefix . "fldxy2.pgm";
		$redimg = $prefix . "red.pgm";
		$sumimg = $prefix . "sum.ppm";
		$sumimg2 = $prefix . "sum2.ppm";
		$dimimg = $prefix . "dim.ppm";

		array_push($todelete, $quadimg);
		array_push($todelete, $redquad);
		array_push($todelete, $xypgm);
		array_push($todelete, $fldxy1pgm);
		array_push($todelete, $fldxy2pgm);
		array_push($todelete, $redimg);
		array_push($todelete, $sumimg);
		array_push($todelete, $sumimg2);
		array_push($todelete, $dimimg);

		$cmd = $plotquad . " -W " . $W . " -H " . $H . " -w 3 " . implode(" ", $fldxy) . " | ppmtopgm > " . $quadimg;
		loggit("command: $cmd\n");
		if ((system($cmd, $retval) === FALSE) || $retval) {
			fail("plotquad failed: return value " . $retval);
		}

		$cmd = "pgmtoppm green " . $quadimg . " > " . $redquad;
		loggit("command: $cmd\n");
		if ((system($cmd, $retval) === FALSE) || $retval) {
			fail("pgmtoppm (quad) failed.");
		}

		$cmd = $plotxy2 . " -i " . $indexxyls . " -S " . (1/$shrink) .
			" -W " . $W . " -H " . $H .
			" -x " . (1/$shrink) . " -y " . (1/$shrink) .
			" -w 2 -r 8 -s + > " . $xypgm;
		loggit("Command: " . $cmd . "\n");
		$res = system($cmd, $retval);
		if ($res === FALSE || $retval) {
			fail("plotxy2 failed. retval $retval, res \"" . $res . "\"");
		}

		$cmd = $plotxy2 . " -i " . $xylist . " -S " . (1/$shrink) . " -W " . $W . " -H " . $H .
			" -N " . (1+$fldobjs) . " -r 6 " .
			"-x " . (1/$shrink) . " -y " . (1/$shrink) . " -w 2 > " . $fldxy1pgm;
		loggit("Command: " . $cmd . "\n");
		$res = system($cmd, $retval);
		if ($res === FALSE || $retval) {
			fail("plotxy2 (fld1) failed. retval $retval, res \"" . $res . "\"");
		}

		$cmd = $plotxy2 . " -i " . $xylist . " -S " . (1/$shrink) . " -W " . $W . " -H " . $H .
			" -n " . (1+$fldobjs) . " -N 200 -r 4 " .
			"-x " . (1/$shrink) . " -y " . (1/$shrink) . " -w 2 > " . $fldxy2pgm;
		loggit("Command: " . $cmd . "\n");
		$res = system($cmd, $retval);
		if ($res === FALSE || $retval) {
			fail("plotxy2 (fld2) failed. retval $retval, res \"" . $res . "\"");
		}

		$cmd = "pgmtoppm green " . $xypgm . " > " . $redimg;
		loggit("Command: " . $cmd . "\n");
		$res = system($cmd, $retval);
		if ($res === FALSE || $retval) {
			fail("pgmtoppm (xy) failed.");
		}

		$cmd = "ppmdim 0.75 " . $userimg . " > " . $dimimg;
		loggit("Command: " . $cmd . "\n");
		$res = system($cmd, $retval);
		if ($res === FALSE || $retval) {
			fail("ppmdim failed: " . $res);
		}

		$cmd = "pnmcomp -alpha=" . $xypgm . " " . $redimg . " " . $dimimg . " " . $sumimg;
 		loggit("Command: " . $cmd . "\n");
		$res = system($cmd, $retval);
		if ($res === FALSE || $retval) {
			fail("pnmcomp failed.");
		}

		$cmd = "pgmtoppm red " . $fldxy1pgm . " > " . $redimg;
		loggit("Command: " . $cmd . "\n");
		$res = system($cmd, $retval);
		if ($res === FALSE || $retval) {
			fail("pgmtoppm (fldxy1) failed.");
		}
		$cmd = "pnmcomp -alpha=" . $fldxy1pgm . " " . $redimg . " " . $sumimg . " " . $sumimg2;
 		loggit("Command: " . $cmd . "\n");
		$res = system($cmd, $retval);
		if ($res === FALSE || $retval) {
			fail("pnmcomp failed.");
		}

		$cmd = "pgmtoppm red " . $fldxy2pgm . " > " . $redimg;
		loggit("Command: " . $cmd . "\n");
		$res = system($cmd, $retval);
		if ($res === FALSE || $retval) {
			fail("pgmtoppm (fldxy1) failed.");
		}

		$cmd = "pnmcomp -alpha=" . $fldxy2pgm . " " . $redimg . " " . $sumimg2 . " " . $sumimg;
 		loggit("Command: " . $cmd . "\n");
		$res = system($cmd, $retval);
		if ($res === FALSE || $retval) {
			fail("pnmcomp failed.");
		}

		$cmd = "pnmcomp -alpha=" . $quadimg . " " . $redquad . " " . $sumimg . " " . $sumimg2;
 		loggit("Command: " . $cmd . "\n");
		$res = system($cmd, $retval);
		if ($res === FALSE || $retval) {
			fail("pnmcomp (2) failed.");
		}

		$cmd = "pnmtopng " . $sumimg2 . " > ";
		if ($big) {
			$cmd .= $bigoverlayfile;
		} else {
			$cmd .= $overlayfile;
		}
 		loggit("Command: " . $cmd . "\n");
		$res = system($cmd, $retval);
		if ($res === FALSE || $retval) {
			fail("pnmtopng failed.");
		}

		// Delete intermediate files.
		$todelete = array_unique($todelete);
		foreach ($todelete as $del) {
			loggit("Deleting temp file " . $del . "\n");
			if (!unlink($del)) {
				loggit("Failed to unlink file: \"" . $del . "\"\n");
			}
		}

	}

	if (!$big && file_exists($overlayfile)) {
		header('Content-type: image/png');
		readfile($overlayfile);
		exit;
	} else if ($big && file_exists($bigoverlayfile)) {
		header('Content-type: image/png');
		readfile($bigoverlayfile);
		exit;
	} else
		fail("(big)overlay file does not exist.");
}
?>
