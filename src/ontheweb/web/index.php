<?php

$resultdir = "/home/gmaps/ontheweb-data/";
$indexdir = "/home/gmaps/ontheweb-indexes/";

$maxfilesize = 10*1024*1024;

$check_xhtml = 1;
$debug = 0;

umask(0002); // in octal

function loggit($mesg) {
	error_log($mesg, 3, "/tmp/ontheweb.log");
}

$headers = $_REQUEST;

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

$all_ok = $ok_fitsfile && $ok_x_col && $ok_y_col && $ok_fu_lower && $ok_fu_upper && $ok_verify && $ok_agree && $ok_codetol && $ok_nagree & $ok_tweak;
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

	// Move the xylist into place...
	$xylist = $mydir . "field.xy"; // .fits
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

	$rdlist = $mydir . "field.rd.fits";
	$wcsfile = $mydir . "wcs.fits";
	$matchfile = $mydir . "match.fits";
	$inputfile = $mydir . "input";
	$solvedfile = $mydir . "solved";
	$startfile = $mydir . "start";
	$donefile = $mydir . "done";
	$logfile = $mydir . "log";

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
					  "marshall-30" => array("marshall-30/marshall-30c"));

	$indexes = $indexmap[$headers["index"]];

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
			//"depth 100\n" .
			"depth 60\n" .
			"parity 0\n" .
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
			($tweak_val ? "tweak\n" : "") .
			"run\n" .
			"\n");

	if (!fclose($fin)) {
		echo "<html><body><h3>Failed to close input file " . $inputfile . "</h3></body></html>";
		exit;
	}

	// Redirect the client to the status script...
	$status_url = "status.php?job=" . $myname;
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
	printf("<table border=\"1\">\n");
	printf("<tr><th>Header</th><th>Value</th></tr>\n");
	foreach ($headers as $header => $value) {
		printf("<tr><td>$header</td><td>$value</td></tr>\n");
	}
	printf("</table>\n");

	echo '<hr /><p><pre>';
	print_r($_FILES);
	echo '</pre></p>';

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

<?php
$fitsfile_ok = $ok_fitsfile && $ok_x_col && $ok_y_col;
if (!$fitsfile_ok) {
    echo $redfont_open;
}
?>

<p>
FITS file containing a table of detected objects, sorted by
brightest objects first
</p>
<?php
if (!$fitsfile_ok) {
    echo $redfont_close;
}
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
if (!$fitsfile_ok) {
    echo $redinput;
}
echo " /></li>\n";
echo "\n";
?>

<?php
printf('<li>X column name: <input type="text" name="x_col" value="%s" size="10"%s /></li>',
	   $x_col_val,
       ($ok_x_col ? "" : ' class="redinput"'));
echo "\n";
printf('<li>Y column name: <input type="text" name="y_col" value="%s" size="10"%s /></li>',
	   $y_col_val,
       ($ok_y_col ? "" : ' class="redinput"'));
?>

</ul>

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
              array("sdss-24", "SDSS: Sloan Digital Sky Survey"));
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
