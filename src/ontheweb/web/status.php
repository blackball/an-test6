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
$jobdatafile = $mydir . $jobdata_fn;
$indexxyls = $mydir . $indexxyls_fn;

// the user's image.
$pnmimg = $mydir . "image.pnm";

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
}

if ($overlay) {

	loggit("overlay image: " . $overlayfile . "\n");
	loggit("exists? " . (file_exists($overlayfile) ? "yes" : "no") . "\n");

	if (!file_exists($overlayfile)) {
		// render it!
		if (!$didsolve) {
			die("Field didn't solve.");
		}
		$db = connect_db($jobdatafile);
		if (!$db) {
			die("failed to connect jobdata db.\n");
		}
		$W = getjobdata($db, "imageW");
		$H = getjobdata($db, "imageH");
		if (!($W && $H)) {
			die("failed to find image width and height.\n");
		}

		$cmd = $tablist . " " . $matchfile . "\"[col fieldobjs]\" | tail -n 1";
		//loggit("cmd: " . $cmd . "\n");
		$output = shell_exec($cmd);
		loggit("output: " . $output . "\n");
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
			$fldxy[] = $x;
			$fldxy[] = $y;
		}

		$quadimg = $mydir . "quad.pgm";
		$redquad = $mydir . "redquad.pgm";
		$xypgm = $mydir . "index.xy.pgm";
		$fldxy1pgm = $mydir . "fldxy1.pgm";
		//$fldxy2pgm = $mydir . "fldxy2.pgm";
		$redimg = $mydir . "red.pgm";
		$sumimg = $mydir . "sum.ppm";
		$sumimg2 = $mydir . "sum2.ppm";
		$dimimg = $mydir . "dim.ppm";

		$cmd = $plotquad . " -W " . $W . " -H " . $H . " -w 3 " . implode(" ", $fldxy) . " | ppmtopgm > " . $quadimg;
		loggit("command: $cmd\n");
		if (system($cmd, $retval) === FALSE) {
			die("plotquad failed.");
		}

		$cmd = "pgmtoppm red " . $quadimg . " > " . $redquad;
		loggit("command: $cmd\n");
		if ((system($cmd, $retval) === FALSE) || $retval) {
			die("pgmtoppm (quad) failed.");
		}

		$cmd = $plotxy2 . " -i " . $indexxyls . " -W " . $W . " -H " . $H . " -x 1 -y 1 -w 1.5 > " . $xypgm;
		loggit("Command: " . $cmd . "\n");
		$res = system($cmd, $retval);
		if ($retval) {
			die("plotxy2 failed. retval $retval, res \"" . $res . "\"");
		}

		$cmd = $plotxy2 . " -i " . $xylist . " -W " . $W . " -H " . $H . " -N " . (1+$fldobjs) . " -r 3 -x 1 -y 1 -w 1.5 > " . $fldxy1pgm;
		loggit("Command: " . $cmd . "\n");
		$res = system($cmd, $retval);
		if ($retval) {
			die("plotxy2 (fld1) failed. retval $retval, res \"" . $res . "\"");
		}

		/*
		$cmd = $plotxy2 . " -i " . $xylist . " -W " . $W . " -H " . $H . " -n " . $fldobjs . " > " . $fldxy2pgm;
		loggit("Command: " . $cmd . "\n");
		$res = system($cmd, $retval);
		if ($retval) {
			die("plotxy (fld2) failed. retval $retval, res \"" . $res . "\"");
		}
		*/

		$cmd = "pgmtoppm red " . $xypgm . " > " . $redimg;
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

		$cmd = "pgmtoppm white " . $fldxy1pgm . " > " . $redimg;
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
		/*
		$cmd = "pgmtoppm white " . $fldxy2pgm . " > " . $redimg;
		loggit("Command: " . $cmd . "\n");
		$res = system($cmd, $retval);
		if ($retval) {
			die("pgmtoppm failed.");
		}
		$cmd = "pnmcomp -alpha=" . $fldxy2pgm . " " . $redimg . " " . $sumimg2 . " " . $sumimg;
 		loggit("Command: " . $cmd . "\n");
		$res = system($cmd, $retval);
		if ($retval) {
			die("pnmcomp failed.");
		}
		*/
		$cmd = "mv " . $sumimg2 . " " . $sumimg;
		loggit("Command: " . $cmd . "\n");
		$res = system($cmd, $retval);

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
p.c {margin-left:auto; margin-right:auto; text-align:center;}
form.c {margin-left:auto; margin-right:auto; text-align:center;}
h3.c {text-align:center;}
#overlay { margin-left:auto; margin-right:auto; text-align:center; }
#onsky { margin-left:auto; margin-right:auto; text-align:center; }
#onsky > a:link { color:white; }
#onsky > a:visited { color:white; }
#onsky > a:hover { color:gray; }
#onsky > a:active { color:yellow; }
</style>

<?php
if ($do_refresh) {
	echo '<meta http-equiv="refresh" content="5" />';
	// content="5; URL=html-redirect.html"
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

	print <<<END
		<p>
		<a href="http://validator.w3.org/check?uri=referer"><img
		style="border:0"
		src="http://www.w3.org/Icons/valid-xhtml10"
		alt="Valid XHTML 1.0 Strict" height="31" width="88" /></a>
		<a href="http://jigsaw.w3.org/css-validator/check/referer">
		<img style="border:0;width:88px;height:31px"
		src="http://jigsaw.w3.org/css-validator/images/vcss" 
		alt="Valid CSS!" /></a>
		</p>
END;

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

	echo "<a href=\"" . htmlentities($url .
		"&WIDTH=1024&HEIGHT=1024&lw=5") .
		"\">";
	echo "<img src=\"" . htmlentities($url . 
		"&WIDTH=512&HEIGHT=512&lw=3") .
		"\" alt=\"An image of your field shown in an image of the whole sky.\"/></a>\n";
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
	echo "<br />Red circles: stars from the index, projected to image coordinates.\n";
	echo "<br />White circles: field objects that were examined.\n";
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
if ($didcancel) {
	echo "Cancelled.";
} else if (!$job_submitted) {
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
function get_datestr($t) {
	// "c" -> 2007-03-12T22:49:53+00:00
	$datestr = date("c", $t);
	// Make Hogg happy
	if (substr($datestr, -6) == "+00:00")
		$datestr = substr($datestr, 0, strlen($datestr)-6) . "Z";
	return $datestr;
}

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

function field_solved($solvedfile, &$msg) {
	if (!file_exists($solvedfile)) {
		return FALSE;
	}
	$contents = file_get_contents($solvedfile);
	return (strlen($contents) && (ord($contents[0]) == 1));
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

	if ($didsolve) {
		echo '<tr><td>(RA, DEC) center:</td><td>';
		echo "(" . $rac . ", " . $decc . ") degrees\n";
		echo "</td></tr>\n";

		$cmd = $modhead . " " . $wcsfile . " CD1_1 | awk '{print $3}'";
		//loggit("Command: " . $cmd . "\n");
		$cd11 = (float)rtrim(shell_exec($cmd));
		$cmd = $modhead . " " . $wcsfile . " CD1_2 | awk '{print $3}'";
		//loggit("Command: " . $cmd . "\n");
		$cd12 = (float)rtrim(shell_exec($cmd));
		$cmd = $modhead . " " . $wcsfile . " CD2_1 | awk '{print $3}'";
		//loggit("Command: " . $cmd . "\n");
		$cd21 = (float)rtrim(shell_exec($cmd));
		$cmd = $modhead . " " . $wcsfile . " CD2_2 | awk '{print $3}'";
		//loggit("Command: " . $cmd . "\n");
		$cd22 = (float)rtrim(shell_exec($cmd));
		loggit("CD: $cd11, $cd12, $cd21, $cd22\n");

		$det = $cd11 * $cd22 - $cd12 * $cd21;
		$par = ($det > 0) ? 1 : -1;
		$T = $par * $cd11 + $cd22;
		$A = $par * $cd21 - $cd12;
		$orient = -(180/M_PI) * atan2($A, $T);
		$pixscale = 3600 * sqrt($par * $det);

		/*
		loggit("det(CD): $det.\n");
		loggit("parity: $par.\n");
		loggit("T: $T.\n");
		loggit("A: $A.\n");
		loggit("orient: $orient\n");
		loggit("pixscale: $pixscale\n");
		*/

		echo '<tr><td>Orientation:</td><td>';
		printf("%.2f deg E of N\n", $orient);
		echo "</td></tr>\n";

		echo '<tr><td>Pixel scale:</td><td>';
		printf("%.2f arcsec/pixel\n", $pixscale);
		echo "</td></tr>\n";

		echo '<tr><td>Parity:</td><td>';
		if ($par == 1) {
			echo "Reverse (\"Left-handed\")";
		} else {
			echo "Normal (\"Right-handed\")";
		}
		echo "</td></tr>\n";

		$db = connect_db($jobdatafile);
		if (!$db) {
			loggit("failed to connect jobdata db.\n");
		} else {
			$W = getjobdata($db, "imageW");
			$H = getjobdata($db, "imageH");
			$exact = TRUE;
			if (!($W && $H)) {
				$exact = FALSE;
				$W = getjobdata($db, "xylsW");
				$H = getjobdata($db, "xylsH");
				if (!($W && $H)) {
					loggit("Couldn't get xylsW/H.\n");
				}
			}
			if ($W && $H) {
				$fldw = $pixscale * $W;
				$fldh = $pixscale * $H;
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
		}

		/*
		echo '<tr><td>Field size (approx):</td><td>';
		echo $fieldsize;
		echo "</td></tr>\n";
		*/

		echo '<tr><td>WCS file:</td><td>';
		print_link($wcsfile);
		echo "</td></tr>\n";

		echo '<tr><td>RA,DEC list:</td><td>';
		print_link($rdlist);
		echo "</td></tr>\n";

		/*
		if (file_exists($objsfile)) {
			echo '<tr><td>Graphical representation:</td><td>';
			echo "<a href=\"";
			echo "http://" . $host . $uri . "/status.php?job=" . $myname . "&overlay";
			echo "\">overlay.png</a>\n";
			echo "</td></tr>\n";
		}
		*/

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

<?php
/* Boo hoo, no grass.
<table border="1" class="c">
<tr><td>Log File</td></tr>
<tr><td>
<pre>
<?php
if (file_exists($blindlogfile)) {
	echo file_get_contents($blindlogfile);
} else {
	echo "(not available)";
}
?>
</pre>
</td></tr>
</table>

<hr />
*/
?>

<?php
if (!($job_done || $didcancel)) {
?>

<form id="dummyform" action="status.php" method="get">
<p class="c">
<input type="submit" name="cancel" value="Cancel Job" />
<input type="hidden" name="job" value="<?php echo $myname; ?>" />
</p>
</form>
<hr />

<?php
}
?>

<p>
<a href="http://validator.w3.org/check?uri=referer"><img
style="border:0"
src="http://www.w3.org/Icons/valid-xhtml10"
alt="Valid XHTML 1.0 Strict" height="31" width="88" /></a>
<a href="http://jigsaw.w3.org/css-validator/check/referer">
<img style="border:0;width:88px;height:31px"
src="http://jigsaw.w3.org/css-validator/images/vcss" 
alt="Valid CSS!" /></a>
</p>

</body>
</html>
