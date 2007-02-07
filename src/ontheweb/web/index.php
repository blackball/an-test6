<?php

//$resultdir = "/h/260/dstn/local/ontheweb-results/";
$resultdir = "/p/learning/stars/ontheweb/";
$indexdir  = "/h/260/dstn/raid3/INDEXES/";


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
        //echo 'error = ' . $fitsfile["error"] . "<br>";
	$got_fitsfile = ($fitsfile["error"] == 0);
	if ($got_fitsfile) {
		$fitsfilename = $fitsfile["name"];
		$fitstempfilename = $fitsfile['tmp_name'];
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
	if (!move_uploaded_file($fitstempfilename, $xylist . ".fits")) {
		echo "<html><body><h3>Failed to move temp file from " . $fitstempfilename . " to " . $xylist . "</h3></body></html>";
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
	//$extra = 'mypage.php';
	//header("Location: http://$host$uri/$extra");

	header("Location: http://" . $host . $uri . "/" . $status_url);
	
	exit;
}

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


<html>
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

<hr>

<?php
printf("<table border=1>\n");
foreach ($headers as $header => $value) {
	printf("<tr><td>$header</td><td>$value</td></tr>\n");
}
printf("</table>\n");

echo '<hr><pre>';
print_r($_FILES);
echo '</pre><hr>';

echo "got_fitsfile = " . $got_fitsfile . "<br>";
echo "got_fitstmpfile = " . $got_fitstmpfile . "<br>";
echo "ok_fitsfile = " . $ok_fitsfile . "<br>";
echo "fitsfilename = " . $fitsfilename . "<br>";
echo "fitstempfilename = " . $fitstempfilename . "<br>";
?>

<hr>




<h2>
Astrometry.net: Web Edition
</h2>

<hr>

<form name="blindform" action="index.php" method="POST" enctype="multipart/form-data">

<?php
$fitsfile_ok = $ok_fitsfile && $ok_x_col && $ok_y_col;
if (!$fitsfile_ok) {
    echo $redfont_open;
}
?>

FITS file containing a table of detected objects, sorted by
brightest objects first
<?php
if (!$fitsfile_ok) {
    echo $redfont_close;
}
?>

<br>
<ul>
<li>File: 
<?php
if ($fitsfile_ok) {
	echo 'using <input type="input" name="fits_filename" readonly value="' . $fitsfilename . '">';
	echo '<input type="hidden" name="fits_tmpfile" value="' . $fitstempfilename . '">';
	echo ' or replace:';
}
echo '<input type="file" name="fitsfile" size="30"';
if (!$fitsfile_ok) {
    echo $redinput;
}
echo ">\n";
echo "\n";
?>

<?php
printf('<li>X column name: <input type="text" name="x_col" value="%s" size="10"%s>',
	   $x_col_val,
       ($ok_x_col ? "" : ' class="redinput"'));
echo "\n";
printf('<li>Y column name: <input type="text" name="y_col" value="%s" size="10"%s>',
	   $y_col_val,
       ($ok_y_col ? "" : ' class="redinput"'));
?>

</ul><br>

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

<ul>
<li>Lower bound: <input type="text" name="fu_lower" size="10"
<?php
if ($ok_fu_lower) {
    echo 'value="' . $headers["fu_lower"] . '"';
} else {
    echo $redinput;
}
?>
>
<li>Upper bound: <input type="text" name="fu_upper" size="10"
<?php
if ($ok_fu_upper) {
    echo 'value="' . $headers["fu_upper"] . '"';
} else {
    echo $redinput;
}
?>
>
</ul>
<br>

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
        echo " selected";
    }
    echo '>' . $val[1] . "</option>";
}
?>
</select>
<br>
<br>

<?php
if (!$ok_verify) {
    echo $redfont_open;
}
?>

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
>
<br>
<br>
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
        echo " selected";
    }
    echo '>' . $tol[1] . "</option>";
}
?>

</select>
<br>
<br>
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
        echo " selected";
    }
    echo '>' . $na[1] . "</option>";
}
?>
</select>
<br>
<br>

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
>
<br>
<br>

<!-- Number of objects to look at: -->
<!-- input type="text" name="maxobj" size=10 -->

<input type="checkbox" name="tweak"
<?php
if ($tweak_val) {
	echo " checked";
}
?>
>
Tweak (fine-tune the WCS by computing polynomial SIP distortion)
<br>
<br>

<input type="submit" value="Submit">

</form>


<hr>

</body>

</html>