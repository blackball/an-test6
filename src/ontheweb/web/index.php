<?php

$resultdir = "/home/gmaps/ontheweb-data/";
$indexdir = "/home/gmaps/ontheweb-indexes/";
$fits2xy = "/home/gmaps/simplexy/fits2xy";
$plotxy = "/home/gmaps/quads/plotxy";
$modhead = "/home/gmaps/quads/modhead";
$tabsort = "/home/gmaps/quads/tabsort";

$maxfilesize = 100*1024*1024;

$maxquads = 50000;

$check_xhtml = 1;
$debug = 0;

umask(0002); // in octal

function loggit($mesg) {
	error_log($mesg, 3, "/tmp/ontheweb.log");
}

/*
 // It appears you can't change these at runtime:
if (!ini_set("upload_max_filesize", $maxfilesize) ||
	!ini_set("post_max_size", $maxfilesize + 1024*1024)) {
	loggit("Failed to change the max uploaded file size.\n");
}
*/

$headers = $_REQUEST;

$got_imgtmpfile = array_key_exists("img_tmpfile", $headers);
if ($got_imgtmpfile) {
	$imgfilename = $headers["img_filename"];
	$imgtempfilename = $headers["img_tmpfile"];
}
$got_imgfile = array_key_exists("imgfile", $_FILES);
loggit("got imgfile: " . $got_imgfile . "\n");
if ($got_imgfile) {
	$imgfile = $_FILES["imgfile"];
	$got_imgfile = ($imgfile["error"] == 0);
	loggit("error: " . $imgfile["error"] . "\n");
	/*
	if ($imgfile["error"]) {
		phpinfo();
		exit;
	}
	*/
	if ($got_imgfile) {
		$imgfilename = $imgfile["name"];
		$imgtempfilename = $imgfile['tmp_name'];
		loggit("image temp file: " . filesize($imgtempfilename) . " bytes.\n");

		// Move the file right away, because otherwise it gets
		// deleted when this form completes.
		$newname = $imgtempfilename . ".tmp";
		if (!move_uploaded_file($imgtempfilename, $newname)) {
			echo "<html><body><h3>Failed to move temp file from " . $imgtempfilename . " to " . $newname . "</h3></body></html>";
			exit;
		}
		loggit("moved uploaded file " . $imgtempfilename . " to " . $newname . ".\n");
		$imgtempfilename = $newname;
		loggit("image file: " . filesize($imgtempfilename) . " bytes.\n");
	}
}

loggit("image orig filename: " . $imgfilename . "\n");
loggit("image temp filename: " . $imgtempfilename . "\n");

$ok_imgfile = $got_imgfile || $got_imgtmpfile;

$got_fitstmpfile = array_key_exists("fits_tmpfile", $headers);
if ($got_fitstmpfile) {
	$fitsfilename = $headers["fits_filename"];
	$fitstempfilename = $headers["fits_tmpfile"];
}
$got_fitsfile = array_key_exists("fitsfile", $_FILES);
if ($got_fitsfile) {
	$fitsfile = $_FILES["fitsfile"];
	$got_fitsfile = ($fitsfile["error"] == 0);
	if ($got_fitsfile) {
		$fitsfilename = $fitsfile["name"];
		$fitstempfilename = $fitsfile['tmp_name'];

		// Move the file right away, because otherwise it gets
		// deleted when this form completes.
		$newname = $fitstempfilename . ".tmp";
		if (!move_uploaded_file($fitstempfilename, $newname)) {
			echo "<html><body><h3>Failed to move temp file from " . $fitstempfilename . " to " . $newname . "</h3></body></html>";
			exit;
		}
		loggit("moved uploaded file " . $fitstempfilename . " to " . $newname . ".\n");
		$fitstempfilename = $newname;
	}
}
$ok_fitsfile = $got_fitsfile || $got_fitstmpfile;

$ok_file = $ok_fitsfile || $ok_imgfile;

$exist_x_col = array_key_exists("x_col", $headers);
$ok_x_col = (!$exist_x_col) || ($exist_x_col && (strlen($headers["x_col"]) > 0));
if ($exist_x_col) {
	$x_col_val = $headers["x_col"];
} else {
	$x_col_val = "X";
}
$exist_y_col = array_key_exists("y_col", $headers);
$ok_y_col = (!$exist_y_col) || ($exist_y_col && (strlen($headers["y_col"]) > 0));
if ($exist_y_col) {
	$y_col_val = $headers["y_col"];
} else {
	$y_col_val = "Y";
}
$ok_fu_lower = array_key_exists("fu_lower", $headers);
if ($ok_fu_lower) {
	$fu_lower = (float)$headers["fu_lower"];
	if ($fu_lower <= 0) {
		$ok_fu_lower = 0;
	}
}
$ok_fu_upper = array_key_exists("fu_upper", $headers);
if ($ok_fu_upper) {
	$fu_upper = (float)$headers["fu_upper"];
	if ($fu_upper <= 0) {
		$ok_fu_upper = 0;
	}
}
$ok_verify = array_key_exists("verify", $headers);
if ($ok_verify) {
	$verify = (float)$headers["verify"];
	if ($verify <= 0) {
		$ok_verify = 0;
	}
}
$ok_agree = array_key_exists("agree", $headers);
if ($ok_agree) {
	$agree = (float)$headers["agree"];
	if ($agree <= 0) {
		$ok_agree = 0;
	}
}

$ok_codetol = array_key_exists("codetol", $headers);
$ok_nagree  = array_key_exists("nagree" , $headers);
$ok_tweak   = array_key_exists("tweak"  , $headers);
$ok_index   = array_key_exists("index"  , $headers);

$tweak_val = ($ok_tweak ? $headers["tweak"] : TRUE);
$parity_val = (array_key_exists("parity", $headers) && $headers["parity"] == "1") ? 1 : 0;

function convert_image($imgfilename, $imgtempfilename, $mydir,
					   &$errstr) {
	global $fits2xy;
	global $modhead;
	global $plotxy;
	global $tabsort;

	// try to figure out what kind of file it is...
	// use Xtopnm:
	$typemap = array("jpg" => "jpeg",
					 "jpeg" => "jpeg",
					 "png" => "png",
					 "fits" => "fits",
					 "gif" => "gif");
	$usetype = "";
	foreach ($typemap as $ending => $type) {
		if (!strcasecmp(substr($imgfilename, -strlen($ending)), $ending)) {
			$usetype = $type;
			break;
		}
	}
	loggit("image file: " . $imgfilename . ", using type: " . $usetype . "\n");
	if (!strlen($usetype)) {
		loggit("unknown image file type.\n");
		$errstr = "Unknown image file type.";
		return FALSE;
	}

	$img = $mydir . "image." . $usetype;
	$newname = $img;
	loggit("renaming temp uploaded file " . $imgtempfilename . " to " . $newname . ".\n");
	if (!rename($imgtempfilename, $newname)) {
		$errstr = "Failed to move temp file from \"" . $imgtempfilename . "\" to \"" . $newname . "\"";
		return FALSE;
	}
	loggit("image file: " . filesize($img) . " bytes.\n");

	if (!chmod($newname, 0664)) {
		$errstr = "Failed to chmod file " . $newname;
		return FALSE;
	}

	$pnmimg = $mydir . "image.pnm";
	$pnmimg_orig = $pnmimg;
	$cmd = $usetype . "topnm " . $img . " > " . $pnmimg;
	loggit("Command: " . $cmd . "\n");
	$res = FALSE;
	$res = system($cmd, $retval);
	if ($retval) {
		loggit("Command failed: return val " . $retval . ", str " . $res . "\n");
		$errstr = "Failed to convert image file.";
		return FALSE;
	}

	$cmd = "pnmfile " . $pnmimg;
	loggit("Command: " . $cmd . "\n");
	$res = FALSE;
	$res = shell_exec($cmd);
	loggit("Pnmfile: " . $res . "\n");

	$ss = strstr($res, "PPM");
	loggit("strstr: " . $ss . "\n");
	if (strlen($ss)) {
		// reduce to PGM.
		$pgmimg = $mydir . "image.pgm";
		$cmd = "ppmtopgm " . $pnmimg . " > " . $pgmimg;
		loggit("Command: " . $cmd . "\n");
		$res = FALSE;
		$res = system($cmd, $retval);
		if ($retval) {
			loggit("Command failed: return val " . $retval . ", str " . $res . "\n");
			$errstr = "Failed to reduce image to grayscale.";
			return FALSE;
		}
		$pnmimg = $pgmimg;
	}

	$fitsimg = $mydir . "image.fits";
	$cmd = "pnmtofits " . $pnmimg . " > " . $fitsimg;
	loggit("Command: " . $cmd . "\n");
	$res = FALSE;
	$res = system($cmd, $retval);
	if ($retval) {
		loggit("Command failed: return val " . $retval . ", str " . $res . "\n");
		$errstr = "Failed to convert image to FITS.";
		return FALSE;
	}

	$fits2xyout = $mydir . "fits2xy.out";
	$cmd = $fits2xy . " " . $fitsimg . " > " . $fits2xyout . " 2>&1";
	loggit("Command: " . $cmd . "\n");
	$res = FALSE;
	$res = system($cmd, $retval);
	if ($retval) {
		loggit("Command failed: return val " . $retval . ", str " . $res . "\n");
		$errstr = "Failed to perform source extraction.";
		return FALSE;
	}
	$xylist = $mydir . "image.xy.fits";

	// sort by FLUX.
	$tabsortout = $mydir . "tabsort.out";
	$sortedlist = $mydir . "field.xy.fits";
	$cmd = $tabsort . " -i " . $xylist . " -o " . $sortedlist . " -c FLUX -d > " . $tabsortout;
	loggit("Command: " . $cmd . "\n");
	$res = FALSE;
	$res = system($cmd, $retval);
	if ($retval) {
		loggit("Command failed: return val " . $retval . ", str " . $res . "\n");
		$errstr = "Failed to perform source extraction.";
		return FALSE;
	}
	$xylist = $sortedlist;

	$cmd = $modhead . " " . $fitsimg . " NAXIS1 | awk '{print $3}'";
	loggit("Command: " . $cmd . "\n");
	$W = (int)rtrim(shell_exec($cmd));
	loggit("naxis1 = " . $W . "\n");

	$cmd = $modhead . " " . $fitsimg . " NAXIS2 | awk '{print $3}'";
	loggit("Command: " . $cmd . "\n");
	$H = (int)rtrim(shell_exec($cmd));
	loggit("naxis2 = " . $H . "\n");

	$objimg = $mydir . "objs.pgm";
	$objimg_orig = $objimg;
	$cmd = $plotxy . " -i " . $xylist . " -W " . $W . " -H " . $H .
		" -x 1 -y 1 " . " > " . $objimg;
	loggit("Command: " . $cmd . "\n");
	$res = FALSE;
	$res = system($cmd, $retval);
	if ($retval) {
		loggit("Command failed: return val " . $retval . ", str " . $res . "\n");
		$errstr = "Failed to plot extracted sources.";
		return FALSE;
	}

	$redimg = $mydir . "red.pgm";
	$cmd = "pgmtoppm red " . $objimg . " > " . $redimg;
	loggit("Command: " . $cmd . "\n");
	$res = FALSE;
	$res = system($cmd, $retval);
	if ($retval) {
		loggit("Command failed: return val " . $retval . ", str " . $res . "\n");
		$errstr = "Failed to create image of extracted sources.";
		return FALSE;
	}
	$objimg = $redimg;

	$sumimg = $mydir . "sum.ppm";
	//$cmd = "pnmarith -max " . $objimg . " " . $pnmimg_orig . " > " . $sumimg;
	$cmd = "pnmcomp -alpha=" . $objimg_orig . " " . $redimg . " " . $pnmimg_orig . " " . $sumimg;
	loggit("Command: " . $cmd . "\n");
	$res = FALSE;
	$res = system($cmd, $retval);
	if ($retval) {
		loggit("Command failed: return val " . $retval . ", str " . $res . "\n");
		$errstr = "Failed to composite image of extracted sources.";
		return FALSE;
	}

	$sumimgpng = $mydir . "objs.png";
	$cmd = "pnmtopng " . $sumimg . " > " . $sumimgpng;
	loggit("Command: " . $cmd . "\n");
	$res = FALSE;
	$res = system($cmd, $retval);
	if ($retval) {
		loggit("Command failed: return val " . $retval . ", str " . $res . "\n");
		$errstr = "Failed to convert composite image of extracted sources to PNG.";
		return FALSE;
	}

	return TRUE;
}

$all_ok = $ok_file && $ok_x_col && $ok_y_col && $ok_fu_lower && $ok_fu_upper && $ok_verify && $ok_agree && $ok_codetol && $ok_nagree & $ok_tweak;
if ($all_ok) {
    // Launch!

	// Create a directory for this request.
    while (TRUE) {
        // Generate a random string.
        $str = '';
        for ($i=0; $i<5; $i++) {
            $str .= chr(mt_rand(0, 255));
        }
        // Convert it to hex.
        $myname = bin2hex($str);

        // See if that dir already exists.
        $mydir = $resultdir . $myname;
        if (!file_exists($mydir)) {
			break;
		}
    }

	if (!mkdir($mydir)) {
		echo "<html><body><h3>Failed to create dir " . $mydir . "</h3></body></html>";
		exit;
	}
	$mydir = $mydir . "/";

	$xylist = $mydir . "field.xy"; // .fits

	// If an image was uploaded, move it into place, and run object
	// extraction on it.
	if ($ok_imgfile) {
		$errstr = "";
		if (!convert_image($imgfilename, $imgtempfilename, $mydir, $errstr)) {
			echo "<html><body><h3>" . $errstr . "</h3></body></html>\n";
			exit;
		}
	} else {
		// Move the xylist into place...
		$newname = $xylist . ".fits";
		loggit("renaming temp uploaded file " . $fitstempfilename . " to " . $newname . ".\n");
		if (!rename($fitstempfilename, $newname)) {
			echo "<html><body><h3>Failed to move temp file from " . $fitstempfilename . " to " . $newname . "</h3></body></html>";
			exit;
		}
		if (!chmod($xylist . ".fits", 0664)) {
			echo "<html><body><h3>Failed to chmod xylist " . $xylist . "</h3></body></html>";
			exit;
		}
	}

	$rdlist = $mydir . "field.rd.fits";
	$wcsfile = $mydir . "wcs.fits";
	$matchfile = $mydir . "match.fits";
	$inputfile = $mydir . "input";
	$solvedfile = $mydir . "solved";
	$startfile = $mydir . "start";
	$donefile = $mydir . "done";
	$logfile = $mydir . "log";

	if ($ok_imgfile) {
		// Write to "input.tmp" instead of "input", so we don't trigger
		// the solver just yet...
		$inputfile = $inputfile . ".tmp";
	}

	// MAJOR HACK - pause to let the watcher notice the new directory
	// was created.
	sleep(1);

	// Write the input file for blind...
	$fin = fopen($inputfile, "w");
	if (!$fin) {
		echo "<html><body><h3>Failed to write input file " . $inputfile . ".fits" . "</h3></body></html>";
		exit;
	}

	for ($i=0; $i<12; $i++) {
		$sdssfiles[$i] = sprintf("sdss-24/sdss-24-%02d", $i);
	}
	$indexmap = array("sdss-24" => $sdssfiles,
					  "sdss-23" => array("sdss-23/sdss-23-allsky"),
					  "allsky-31" => array("allsky-31/allsky-31"),
					  "marshall-30" => array("marshall-30/marshall-30c"));

	$indexes = $indexmap[$headers["index"]];

	// Try both parities.
	for ($ip=0; $ip<2; $ip++) {
		fprintf($fin, "log " . $logfile . "\n");
		foreach ($indexes as $ind) {
			fprintf($fin, "index " . $indexdir . $ind . "\n");
		}
		fprintf($fin,
				"field " . $xylist . "\n" .
				"match " . $matchfile . "\n" .
				"start " . $startfile . "\n" .
				"done " . $donefile . "\n" .
				"solved " . $solvedfile . "\n" .
				"wcs " . $wcsfile . "\n" .
				"rdls " . $rdlist . "\n" .
				"fields 0\n" .
				"xcol " . $x_col_val . "\n" .
				"ycol " . $y_col_val . "\n" .
				"sdepth 0\n" .
				"depth 200\n" .
				//"depth 150\n" .
				//"depth 100\n" .
				//"depth 60\n" .
				($ip == 0 ?
				 "parity " . $parity_val . "\n" :
				 "parity " . (1 - $parity_val) . "\n") .
				"fieldunits_lower " . $fu_lower . "\n" .
				"fieldunits_upper " . $fu_upper . "\n" .
				"tol " . $headers["codetol"] . "\n" .
				"verify_dist " . $verify . "\n" .
				"agreetol " . $agree . "\n" .
				"nagree_toverify " . $nagree . "\n" .
				"ninfield_tokeep 50\n" .
				"ninfield_tosolve 50\n" .
				"overlap_tokeep 0.25\n" .
				"overlap_tosolve 0.25\n" .
				"maxquads " . $maxquads . "\n" .
				($tweak_val ? "tweak\n" : "") .
				"run\n" .
				"\n");
	}

	if (!fclose($fin)) {
		echo "<html><body><h3>Failed to close input file " . $inputfile . "</h3></body></html>";
		exit;
	}

	// Redirect the client to the status script...
	$status_url = "status.php?job=" . $myname;
	if ($ok_imgfile) {
		$status_url .= "&img";
	}
	$host  = $_SERVER['HTTP_HOST'];
	$uri  = rtrim(dirname($_SERVER['PHP_SELF']), '/\\');
	header("Location: http://" . $host . $uri . "/" . $status_url);
	
	exit;
}

// Don't highlight the missing elements in red if this is the
// first time the page has been loaded.
$newform = (count($headers) == 0);
if ($newform) {
	$redinput = "";
	$redfont_open   = "";
	$redfont_close  = "";
} else {
	$redinput = ' class="redinput"';
	$redfont_open   = '<font color="red">';
	$redfont_close  = "</font>";
}
?>

<?php
echo '<?xml version="1.0" encoding="UTF-8"?>';
?>

<!DOCTYPE html 
PUBLIC "-//W3C//DTD XHTML 1.0 Strict//EN"
"http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd">
<html xmlns="http://www.w3.org/1999/xhtml" xml:lang="en" lang="en">
<head>
<title>
Astrometry.net: Web Edition
</title>
<style type="text/css">
<!-- 
input.redinput {
	background-color: pink;
}
-->
</style>
</head>

<body>

<hr />

<?php
if ($debug) {
	//phpinfo();

	printf("<table border=\"1\">\n");
	printf("<tr><th>Header</th><th>Value</th></tr>\n");
	foreach ($headers as $header => $value) {
		printf("<tr><td>$header</td><td>$value</td></tr>\n");
	}
	printf("</table>\n");

	echo '<hr /><pre>';
	print_r($_FILES);
	echo '</pre>';

	echo "<hr /><p>";
	echo "got_fitsfile = " . $got_fitsfile . "<br />";
	echo "got_fitstmpfile = " . $got_fitstmpfile . "<br />";
	echo "ok_fitsfile = " . $ok_fitsfile . "<br />";
	echo "fitsfilename = " . $fitsfilename . "<br />";
	echo "fitstempfilename = " . $fitstempfilename . "<br />";
	echo "</p>";
	echo "<hr />";
}
?>





<h2>
Astrometry.net: Web Edition
</h2>

<hr />

<form id="blindform" action="index.php" method="post" enctype="multipart/form-data">

<p>
Field to solve: upload one of the following:
</p>
<ul>
<li>

<p>
<?php
$fitsfile_ok = $ok_fitsfile && $ok_x_col && $ok_y_col;
$file_ok = $ok_imgfile || $fitsfile_ok;

if (!$file_ok) {
    echo $redfont_open;
}
?>
FITS file containing a table of detected objects, sorted by
brightest objects first
<?php
if (!$file_ok) {
    echo $redfont_close;
}
?>
</p>

<?php
/*
echo "<p>ok_fitsfile: " . $ok_fitsfile . "</p>\n";
echo "<p>ok_imgfile: " . $ok_imgfile . "</p>\n";
echo "<p>ok_x_col: " . $ok_x_col . "</p>\n";
echo "<p>ok_y_col: " . $ok_y_col . "</p>\n";
echo "<p>fitsfile_ok: " . $fitsfile_ok . "</p>\n";
echo "<p>file_ok: " . $file_ok . "</p>\n";
*/
?>

<ul>
<li>File: 
<?php
if ($ok_fitsfile) {
	echo 'using <input type="input" name="fits_filename" readonly value="' . $fitsfilename . '" />';
	echo '<input type="hidden" name="fits_tmpfile" value="' . $fitstempfilename . '">';
	echo ' or replace:';
}
printf('<input type="hidden" name="MAX_FILE_SIZE" value="%d" />', $maxfilesize);
echo '<input type="file" name="fitsfile" size="30"';
if (!$file_ok && !$ok_fitsfile) {
    echo $redinput;
}
echo " />\n";
echo "</li>\n";
echo "\n";
?>

<?php
printf('<li>X column name: <input type="text" name="x_col" value="%s" size="10"%s /></li>',
	   $x_col_val,
       ($file_ok || $ok_x_col ? "" : ' class="redinput"'));
echo "\n";
printf('<li>Y column name: <input type="text" name="y_col" value="%s" size="10"%s /></li>',
	   $y_col_val,
       ($file_ok || $ok_y_col ? "" : ' class="redinput"'));
?>

</ul>

</li>

<li>
<p>
Image file:
<?php
if ($ok_imgfile) {
	echo 'using <input type="input" name="img_filename" readonly value="' . $imgfilename . '" />';
	echo '<input type="hidden" name="img_tmpfile" value="' . $imgtempfilename . '">';
	echo ' or replace:';
}
printf('<input type="hidden" name="MAX_FILE_SIZE" value="%d" />', $maxfilesize);
echo '<input type="file" name="imgfile" size="30"';
if (!$ok_imgfile && !($ok_fitsfile && $ok_x_col && $ok_y_col)) {
    echo $redinput;
}
echo " />\n";
?>
</p>
</li>
</ul>

<p>
<input type="radio" name="parity" value="0" 
<?php
if (!$parity_val) {
	echo 'checked="checked"';
}
?>
/>"Right-handed" image: North-Up, East-Right <br />
<input type="radio" name="parity" value="1" 
<?php
if ($parity_val) {
	echo 'checked="checked"';
}
?>
/>"Left-handed" image: North-Up, West-Right
</p>

<p>
<?php
$bounds_ok = $ok_fu_lower && $ok_fu_upper;
if (!$bounds_ok) {
    echo $redfont_open;
}
?>
Bounds on the pixel scale of the image, in arcseconds per pixel:
<?php
if (!$bounds_ok) {
    echo $redfont_close;

}
?>
</p>

<ul>
<li>Lower bound: <input type="text" name="fu_lower" size="10"
<?php
if ($ok_fu_lower) {
    echo 'value="' . $headers["fu_lower"] . '"';
} else {
    echo $redinput;
}
?>
 /></li>
<li>Upper bound: <input type="text" name="fu_upper" size="10"
<?php
if ($ok_fu_upper) {
    echo 'value="' . $headers["fu_upper"] . '"';
} else {
    echo $redinput;
}
?>
 /></li>
</ul>

<p>
Index to use:
<select name="index">

<?php
$vals = array(array("marshall-30", "Wide-Field Digital Camera"),
			  array("allsky-31", "25-arcminute Fields"),
              array("sdss-23", "SDSS: Sloan Digital Sky Survey"));
if ($ok_index) {
    $sel = $headers["index"];
} else {
    $sel = "marshall-30"; // default
}
foreach ($vals as $val) {
    echo '<option value="' . $val[0] . '"';
    if ($val[0] == $sel) {
        echo ' selected="selected"';
    }
    echo '>' . $val[1] . "</option>";
}
?>
</select>
</p>

<?php
if (!$ok_verify) {
    echo $redfont_open;
}
?>

<p>
Star positional error (verification tolerance), in arcseconds:

<?php
if (!$ok_verify) {
    echo $redfont_close;
}
?>

<input type="text" name="verify" size="10"
<?php
if ($ok_verify) {
    echo 'value="' . $verify . '"';
} else {
    echo $redinput;
}
?>
 />
</p>

<p>
Code tolerance:
<select name="codetol">
<?php
$tols = array(array("0.005", "0.005 (fast)"),
              array("0.01" , "0.01  (standard)"),
              array("0.02" , "0.02  (exhaustive)"));
if ($ok_codetol) {
    $codesel = $headers["codetol"];
} else {
    $codesel = "0.01"; // default
}
foreach ($tols as $tol) {
    echo '<option value="' . $tol[0] . '"';
    if ($tol[0] == $codesel) {
        echo ' selected="selected"';
    }
    echo '>' . $tol[1] . "</option>";
}
?>
</select>
</p>

<p>
Number of agreeing matches required:
<select name="nagree">
<?php
$ns = array(array("1", "1 (exhaustive)"),
              array("2", "2 (standard)"));
if ($ok_nagree) {
    $nagreesel = $headers["nagree"];
} else {
    $nagreesel = "2"; // default
}
foreach ($ns as $na) {
    echo '<option value="' . $na[0] . '"';
    if ($na[0] == $nagreesel) {
        echo ' selected="selected"';
    }
    echo '>' . $na[1] . "</option>";
}
?>
</select>
</p>

<p>
<?php
if (!$ok_agree) {
    echo $redfont_open;
}
?>
Agreement radius (in arcseconds):
<?php
if (!$ok_agree) {
    echo $redfont_close;
}
?>

<input type="text" name="agree" size="10"
<?php
if ($ok_agree) {
    echo 'value="' . $agree . '"';
} else {
    echo $redinput;
}
?>
 />
</p>

<!-- Number of objects to look at: -->
<!-- input type="text" name="maxobj" size=10 -->

<p>
<input type="checkbox" name="tweak"
<?php
if ($tweak_val) {
	echo ' checked="checked"';
}
?>
 />
Tweak (fine-tune the WCS by computing polynomial SIP distortion)
</p>

<p>
<input type="submit" value="Submit" />
</p>

</form>

<?php
echo "<hr /><p>Note, there is a file size limit of ";
printf("%d", $maxfilesize / (1024*1024));
echo " MB.  Your browser may fail silently if you try to upload a file that exceeds this.</p>\n";
?>

<hr />



<?php
if ($check_xhtml) {
print <<<END
<p>
    <a href="http://validator.w3.org/check?uri=referer"><img
        src="http://www.w3.org/Icons/valid-xhtml10"
        alt="Valid XHTML 1.0 Strict" height="31" width="88" /></a>
</p>
END;
}
?>  

</body>

</html>
