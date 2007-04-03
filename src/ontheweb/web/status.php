<?php
require_once 'MDB2.php';

require 'common.php';

if (!array_key_exists("job", $headers)) {
	echo "<h3>No \"job\" argument</h3></body></html>\n";
	exit;
}

$myname = $headers["job"];
$mydir = $resultdir . $myname . "/";

$img = array_key_exists("img", $headers);
$overlay = array_key_exists("overlay", $headers);
$cancel = array_key_exists("cancel", $headers);
$goback = array_key_exists("goback", $headers);

// Make sure the path is legit...
$rp1 = realpath($resultdir);
$rp2 = realpath($mydir);
if (substr($rp2, 0, strlen($rp1)) != $rp1) {
	die("Invalid \"job\" arg.");
}

$qfile = $resultdir . $q_fn;
$inputfile = $mydir . $input_fn;
$inputtmpfile = $mydir . $inputtmp_fn;
$startfile = $mydir . $start_fn;
$donefile  = $mydir . $done_fn;
$xylist = $mydir . $xyls_fn;
$rdlist = $mydir . $rdls_fn;
$blindlogfile = $mydir . $log_fn;
$solvedfile = $mydir . $solved_fn;
$cancelfile = $mydir . $cancel_fn;
$wcsfile = $mydir . $wcs_fn;
$matchfile = $mydir . $match_fn;
$objsfile = $mydir . $objs_fn;
$overlayfile = $mydir . $overlay_fn;
$rdlsinfofile = $mydir . $rdlsinfo_fn;
$wcsinfofile = $mydir . $wcsinfo_fn;
$jobdatafile = $mydir . $jobdata_fn;
$indexxyls = $mydir . $indexxyls_fn;

$db = connect_db($jobdatafile);
if (!$db) {
	die("failed to connect jobdata db.\n");
}
$jd = getalljobdata($db);
if ($jd === FALSE) {
	die("failed to get jobdata.\n");
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
		die("Failed to rename input temp file (" . $inputtmpfile . " to " . $inputfile);
	}
	// Hack - pause a moment...
	sleep(1);
}

if ($cancel) {
	loggit("cancel requested.\n");
	if (!touch($cancelfile)) {
		die("Failed to created cancel file.");
	}
	if ($remote) {
		$cmd = "/bin/echo " . $myname . " | ssh -T c27cancel";
		if ((system($cmd, $retval) === FALSE) || $retval) {
			loggit("remote cancel failed: retval " . $retval . ", cmd " . $cmd . "\n");
		}
	}
}

if ($goback) {
	$host  = $_SERVER['HTTP_HOST'];
	$dir   = rtrim(dirname($_SERVER['PHP_SELF']), '/\\');
	$uri  = $dir . "/index.php";

	$imgdir = "http://" . $host . $dir . "/status/" . $myname . "/";

	$quick = ($headers["quick"] == "1");
	if ($quick) {
		$userimage = $jd["imagefilename"];
		$jd['xysrc'] = 'url';
		$jd['imgurl'] = $imgdir . $userimage;
	}

	$args = format_preset_url($jd, $formDefaults);

	if ($headers["skippreview"] == "1") {
		$args .= "&skippreview=1";
	}

	header("Location: http://" . $host . $uri . $args);
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
			die("Failed to set WCS-related jobdata entries.");
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

if (array_key_exists("email", $headers)) {
	if ($jd['sent-email'] == 'yes') {
		die("already sent email.");
	}
	if (!setjobdata($db, array('sent-email'=>'yes'))) {
		die("failed to update jobdata : sent-email.\n");
	}

	$host  = $_SERVER['HTTP_HOST'];
	$uri  = rtrim(dirname($_SERVER['PHP_SELF']), '/\\');
	$staturl = "http://" . $host . $uri . "/status.php?job=" . $myname;

	$email = $jd['email'];
	$uname = $jd['uname'];
	$headers = 'From: Astrometry.net <alpha@astrometry.net>' . "\r\n" .
		'Reply-To: dstn@cs.toronto.edu' . "\r\n" .
		'X-Mailer: PHP/' . phpversion() . "\r\n";
	if ($didsolve) {
		$subject = 'Astrometry.net job ' . $myname . ' solved';
		$message = "Hello again,\n\n" .
			"We're please to tell you that we solved your field.\n\n" .
			"You can get the results here:\n" .
			"  " . $staturl . "\n\n" .
			"Please let us know if you have any problems retrieving your results " .
			"or if the solution is wrong.\n\n";
	} else {
		$headers .= 'Cc: Dustin Lang <dstn@cs.toronto.edu>' . "\r\n";
		$subject = 'Astrometry.net job ' . $myname . ' failed';
		$message = "Hello again,\n\n" .
			"Sadly, we were unable to solve your field automatically.\n\n" .
			"We may take a look at it and see if we can get it to solve; in this case " .
			"we'll send you an email to let you know.  We may contact you to get more " .
			"details about your field.\n\n";
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
		die("Failed to send email.\n");
	}
	echo "Email sent.\n";
	exit;
}

if ($overlay) {

	//loggit("overlay image: " . $overlayfile . "\n");
	//loggit("exists? " . (file_exists($overlayfile) ? "yes" : "no") . "\n");

	if (!file_exists($overlayfile)) {
		// render it!
		if (!$didsolve) {
			die("Field didn't solve.");
		}
		$W = $jd["displayW"];
		$H = $jd["displayH"];
		if (!($W && $H)) {
			// BACKWARDS COMPATIBILITY.
			loggit("failed to find image display width and height.\n");
			$W = $jd["imageW"];
			$H = $jd["imageH"];
			if (!($W && $H)) {
				die("failed to find image width and height.\n");
			}
		}
		$shrink = $jd["imageshrink"];
		if (!$shrink)
			$shrink = 1;

		$cmd = $tablist . " " . $matchfile . "\"[col fieldobjs]\" | tail -n 1";
		loggit("Command: " . $cmd . "\n");
		$output = shell_exec($cmd);
		//loggit("output: " . $output . "\n");
		if (sscanf($output, " %d %d %d %d %d ", $nil, $fA, $fB, $fC, $fD) != 5) {
			die("failed to parse field objs.");
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
				die("failed to parse field objs coords: \"" . $lines[$i] . "\"");
			}
			// Here's where we scale down the size of the quad:
			$fldxy[] = $x / $shrink;
			$fldxy[] = $y / $shrink;
		}

		$quadimg = $mydir . "quad.pgm";
		$redquad = $mydir . "redquad.pgm";
		$xypgm = $mydir . "index.xy.pgm";
		$fldxy1pgm = $mydir . "fldxy1.pgm";
		$fldxy2pgm = $mydir . "fldxy2.pgm";
		$redimg = $mydir . "red.pgm";
		$sumimg = $mydir . "sum.ppm";
		$sumimg2 = $mydir . "sum2.ppm";
		$dimimg = $mydir . "dim.ppm";

		$cmd = $plotquad . " -W " . $W . " -H " . $H . " -w 3 " . implode(" ", $fldxy) . " | ppmtopgm > " . $quadimg;
		loggit("command: $cmd\n");
		if (system($cmd, $retval) === FALSE) {
			die("plotquad failed.");
		}

		$cmd = "pgmtoppm green " . $quadimg . " > " . $redquad;
		loggit("command: $cmd\n");
		if ((system($cmd, $retval) === FALSE) || $retval) {
			die("pgmtoppm (quad) failed.");
		}

		$cmd = $plotxy2 . " -i " . $indexxyls . " -S " . (1/$shrink) . " -W " . $W . " -H " . $H .
			" -x 1 -y 1 -w 1.5 -r 4 > " . $xypgm;
		loggit("Command: " . $cmd . "\n");
		$res = system($cmd, $retval);
		if ($retval) {
			die("plotxy2 failed. retval $retval, res \"" . $res . "\"");
		}

		$cmd = $plotxy2 . " -i " . $xylist . " -S " . (1/$shrink) . " -W " . $W . " -H " . $H .
			" -N " . (1+$fldobjs) . " -r 5 -x 1 -y 1 -w 1.5 > " . $fldxy1pgm;
		loggit("Command: " . $cmd . "\n");
		$res = system($cmd, $retval);
		if ($retval) {
			die("plotxy2 (fld1) failed. retval $retval, res \"" . $res . "\"");
		}

		$cmd = $plotxy2 . " -i " . $xylist . " -S " . (1/$shrink) . " -W " . $W . " -H " . $H .
			" -n " . (1+$fldobjs) . " -N 200 -r 3 -x 1 -y 1 -w 1.5 > " . $fldxy2pgm;
		loggit("Command: " . $cmd . "\n");
		$res = system($cmd, $retval);
		if ($retval) {
			die("plotxy2 (fld2) failed. retval $retval, res \"" . $res . "\"");
		}

		$cmd = "pgmtoppm green " . $xypgm . " > " . $redimg;
		loggit("Command: " . $cmd . "\n");
		$res = system($cmd, $retval);
		if ($retval) {
			die("pgmtoppm (xy) failed.");
		}

		$cmd = "ppmdim 0.75 " . $pnmimg . " > " . $dimimg;
		$res = system($cmd, $retval);
		if ($retval) {
			die("ppmdim failed.");
		}

		$cmd = "pnmcomp -alpha=" . $xypgm . " " . $redimg . " " . $dimimg . " " . $sumimg;
 		loggit("Command: " . $cmd . "\n");
		$res = system($cmd, $retval);
		if ($retval) {
			die("pnmcomp failed.");
		}

		$cmd = "pgmtoppm red " . $fldxy1pgm . " > " . $redimg;
		loggit("Command: " . $cmd . "\n");
		$res = system($cmd, $retval);
		if ($retval) {
			die("pgmtoppm (fldxy1) failed.");
		}
		$cmd = "pnmcomp -alpha=" . $fldxy1pgm . " " . $redimg . " " . $sumimg . " " . $sumimg2;
 		loggit("Command: " . $cmd . "\n");
		$res = system($cmd, $retval);
		if ($retval) {
			die("pnmcomp failed.");
		}

		$cmd = "pgmtoppm red " . $fldxy2pgm . " > " . $redimg;
		loggit("Command: " . $cmd . "\n");
		$res = system($cmd, $retval);
		if ($retval) {
			die("pgmtoppm (fldxy1) failed.");
		}
		$cmd = "pnmcomp -alpha=" . $fldxy2pgm . " " . $redimg . " " . $sumimg2 . " " . $sumimg;
 		loggit("Command: " . $cmd . "\n");
		$res = system($cmd, $retval);
		if ($retval) {
			die("pnmcomp failed.");
		}

		/*
		$cmd = "mv " . $sumimg2 . " " . $sumimg;
		loggit("Command: " . $cmd . "\n");
		$res = system($cmd, $retval);
		*/

		$cmd = "pnmcomp -alpha=" . $quadimg . " " . $redquad . " " . $sumimg . " " . $sumimg2;
 		loggit("Command: " . $cmd . "\n");
		$res = system($cmd, $retval);
		if ($retval) {
			die("pnmcomp (2) failed.");
		}

		$cmd = "pnmtopng " . $sumimg2 . " > " . $overlayfile;
 		loggit("Command: " . $cmd . "\n");
		$res = system($cmd, $retval);
		if ($retval) {
			die("pnmtopng failed.");
		}
	}

	if (file_exists($overlayfile)) {
		header('Content-type: image/png');
		readfile($overlayfile);
		exit;
	} else
		die("overlay file does not exist.");
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
#onsky { margin-left:auto; margin-right:auto; text-align:center; }
#onsky > a:link { color:white; }
#onsky > a:visited { color:white; }
#onsky > a:hover { color:gray; }
#onsky > a:active { color:yellow; }
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
$statuspath .= $myname . "/";
$host  = $_SERVER['HTTP_HOST'];
$uri  = rtrim(dirname($_SERVER['PHP_SELF']), '/\\');
$statusurl = "http://" . $host . $uri . "/" . $statuspath;

$now = time();

function get_url($f) {
	global $statusurl;
	return $statusurl . basename($f);
}

function print_link($f) {
	if (file_exists($f)) {
		$url = get_url($f);
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
	echo "<h3 class=\"c\">Your field is at RA,DEC = (" . $rac . ", " . $decc . ") degrees.</h3>\n";
	echo "<hr />\n";
}

if ($didsolve) {
	echo "<div id=\"onsky\">\n";
	echo "<p>Your field on the sky (click for larger image):</p>\n";

	$url = "http://oven.cosmo.fas.nyu.edu/tilecache/tilecache.php?" .
		// "tag=test-tag&" .
		"LAYERS=tycho,grid,boundary&FORMAT=image/png" .
		"&arcsinh&gain=-0.5" .
		"&BBOX=0,-85,360,85" .
		"&wcsfn=" . $myname . "/wcs.fits";

	$fldsz = $pixscale * max($fullW, $fullH);
	loggit("Field size: " . $fldsz . "\n");
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

		$url = "http://oven.cosmo.fas.nyu.edu/tilecache/tilecache.php?" .
			// "tag=test-tag&" .
			"LAYERS=tycho,grid,boundary&FORMAT=image/png" .
			"&arcsinh&gain=-0.25" .
			"&BBOX=" . $ralo . "," . $declo . "," . $rahi . "," . $dechi .
			"&wcsfn=" . $myname . "/wcs.fits";
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

			$url = "http://oven.cosmo.fas.nyu.edu/tilecache/tilecache.php?" .
				// "tag=test-tag&" .
				"LAYERS=tycho,grid,boundary&FORMAT=image/png" .
				"&arcsinh&gain=0.5" .
				"&BBOX=" . $ralo . "," . $declo . "," . $rahi . "," . $dechi .
				"&wcsfn=" . $myname . "/wcs.fits";

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

//<?php
//loggit("didsolve: $didsolve.  pnmimg? " . file_exists($pnmimg) . "\n");
if ($didsolve && file_exists($pnmimg)) {
	$host  = $_SERVER['HTTP_HOST'];
	$uri  = rtrim(dirname($_SERVER['PHP_SELF']), '/\\');
	echo "<div id=\"overlay\">\n";
	echo "<p>Your field plus our index objects:\n";
	echo "<br />Green circles: stars from the index, projected to image coordinates.\n";
	echo "<br />Large red circles: field objects that were examined.\n";
	echo "<br />Small red circles: field objects that were not examined.\n";
	echo "</p>\n";
	//Your field, overplotted with objects from the index.</p>\n";
	echo "<img src=\"" .
		"http://" . $host . $uri . htmlentities("/status.php?job=" . $myname . "&overlay") .
		"\" alt=\"An image of your field, showing sources we extracted from the image and objects from our index.\"/>";
	echo "</div>\n";
	echo "<hr />\n";
}
?>

<table cellpadding="3" border="1" class="c">

<tr><td>Job Id:</td><td>
<?php
echo $myname
?>
</td></tr>

<tr><td>Status:</td><td>
<?php
echo $status . "</td></tr>\n";

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
	$t = filemtime($inputfile);
	echo get_datestr($t);
	$dt = dtime2str($now - $t);
	echo "<br />(" . $dt . " ago)";
	echo "</td></tr>\n";
}

if ($job_started) {
	echo '<tr><td>Started:</td><td>';
	$t = filemtime($startfile);
	echo get_datestr($t);
	$dt = dtime2str($now - $t);
	echo "<br />(" . $dt . " ago)";
	echo "</td></tr>\n";
}

if ($job_done) {
	echo '<tr><td>Finished:</td><td>';
	$t = filemtime($donefile);
	echo get_datestr($t);
	$dt = dtime2str($now - $t);
	echo "<br />" . $dt . " ago)";
	echo "</td></tr>\n";
}

if ($job_queued) {
	echo '<tr><td>Position in Queue:</td><td>';
	$q = @file($qfile);
	$pos = -1;
	for ($i=0; $i<count($q); $i++) {
		$entry = explode("/", rtrim($q[$i]));
		if ($entry[count($entry)-2] == $myname) {
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
	if ($didsolve) {
		echo '<tr><td>(RA, DEC) center:</td><td>';
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

		echo '<tr><td>RA,DEC list:</td><td>';
		print_link($rdlist);
		echo "</td></tr>\n";

		echo '<tr><td>Google Maps view:</td><td>';
		echo "<a href=\"";
		echo htmlentities($gmaps_url . 
						  "?zoom=" . $zoom . 
						  "&ra=" . $rac_merc .
						  "&dec=" . $decc_merc .
						  "&over=no" .
						  "&rdls=" . $myname . "/field.rd.fits" .
						  "&view=r+u&nr=200");
		echo "\">USNOB</a>\n";
		echo "<a href=\"";
		echo htmlentities($gmaps_url .
						  "?gain=" . (($zoom <= 6) ? "-2" : "0") .
						  "&zoom=" . $zoom .
						  "&ra=" . $rac_merc . "&dec=" . $decc_merc .
						  "&over=no" .
						  "&rdls=" . $myname . "/field.rd.fits" .
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

<hr />

<table border="1" class="c">
<tr><td>Tail of the Log File</td></tr>
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
	 <td>Quick (don't re-upload the image)</td>
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

<?php
	  }
echo $valid_blurb;
?>

</body>
</html>
