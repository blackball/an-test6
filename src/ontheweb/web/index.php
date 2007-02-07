<html>
<head>
<title>
Astrometry.net: Web Edition
</title>
<style type="text/css">
<!-- 
input.redinput {
color:black;
background-color: pink;
border:none;
}
-->
</style>
</head>

<?php
$headers = $_REQUEST;

printf("<table border=1>\n");
foreach ($headers as $header => $value) {
	printf("<tr><td>$header</td><td>$value</td></tr>\n");
}
printf("</table>\n");

echo '<hr><pre>';
print_r($_FILES);
echo '</pre><hr>';

$got_fitsfile = array_key_exists("fitsfile", $_FILES);
$got_fitstmpfile = array_key_exists("fits_tmpfile", $headers);
if ($got_fitstmpfile) {
	$fitsfilename = $headers["fits_filename"];
	$fitstempfilename = $headers["fits_tmpfile"];
}
if ($got_fitsfile) {
	$fitsfile = $_FILES["fitsfile"];
	$got_fitsfile = $got_fitsfile and ($fitsfile["error"] == 0);
	if ($got_fitsfile) {
		$fitsfilename = $fitsfile["name"];
		$fitstempfilename = $fitsfile['tmp_name'];
	}
	//echo "got_fitsfile = " . $got_fitsfile;
}
$ok_fitsfile = $got_fitsfile or $got_fitstmpfile;
echo "ok_fitsfile = " . $ok_fitsfile;
echo "fitsfile = " . $fitsfilename;

$ok_x_col = array_key_exists("x_col", $headers);
$ok_y_col = array_key_exists("y_col", $headers);
$ok_fu_lower = array_key_exists("fu_lower", $headers);
if ($ok_fu_lower) {
	$fu_lower = (int)$headers["fu_lower"];
	if ($fu_lower <= 0) {
		$ok_fu_lower = 0;
	}
}
$ok_fu_upper = array_key_exists("fu_upper", $headers);
if ($ok_fu_upper) {
	$fu_upper = (int)$headers["fu_upper"];
	if ($fu_upper <= 0) {
		$ok_fu_upper = 0;
	}
}

/*
echo "HTTP Request headers:\n";
$hdrs = apache_request_headers();
echo "<table border=1>\n";
foreach ($hdrs as $header => $value) {
	printf("<tr><td>$header</td><td>$value</td></tr>\n");
}
printf("</table>\n");
*/

?>

<body>

<h2>
Astrometry.net: Web Edition
</h2>

<hr>

<form name="blindform" action="index.php" method="POST" enctype="multipart/form-data">

<?php
$fitsfile_ok = $ok_fitsfile and $ok_x_col and $ok_y_col;
if (!$fitsfile_ok) {
	echo "<font color=red>";
}
echo "FITS file containing a table of detected objects, sorted by ";
echo "brightest objects first:\n<br>\n<ul>\n";

echo '<li>File: ';
if ($fitsfile_ok) {
	echo 'using <input type="input" name="fits_filename" readonly value="' . $fitsfilename . '">';
	echo '<input type="hidden" name="fits_tmpfile" value="' . $fitstempfilename . '">';
	echo ' or replace:';
} else {
}
echo '<input type="file" name="fitsfile" size=30>';
echo "\n";

if (!$fitsfile_ok) {
	echo "</font>";
}

echo "\n";

printf('<li>X column name: <input type="text" name="x_col" value="%s" size=10%s>',
	   ($ok_x_col ? $headers["x_col"] : "X"),
	   ($ok_x_col ? "" : " class=redinput"));

echo "\n";

printf('<li>Y column name: <input type="text" name="y_col" value="%s" size=10%s>',
	   ($ok_y_col ? $headers["y_col"] : "Y"),
	   ($ok_y_col ? "" : " class=redinput"));

echo "\n";

echo '</ul><br>';

echo "\n";

$bounds_ok = $ok_fu_lower and $ok_fu_upper;
if (!$bounds_ok) {
	 echo "<font color=red>";
}
echo 'Bounds on the pixel scale of the image, in arcseconds per pixel:';
if (!$bounds_ok) {
	 echo "</font>";
}
echo "\n";
echo "<ul>";
echo "\n";

printf('<li>Lower bound: <input type="text" name="fu_lower" size=10 %s>',
	   ($ok_fu_lower ?
		'value="' . $headers["fu_lower"] . '"' :
		"class=redinput"));
echo "\n";
printf('<li>Upper bound: <input type="text" name="fu_upper" size=10 %s>',
	   ($ok_fu_upper ?
		'value="' . $headers["fu_upper"] . '"' :
		"class=redinput"));
echo "\n";
echo "</ul><br>";
echo "\n";

?>

<!--
<ul>
<li>Lower bound: <input type="text" name="fu_lower" size=10>
<li>Upper bound: <input type="text" name="fu_upper" size=10>
</ul>
<br>
-->

Index to use:
<select name="index">
<option value="marshall-30">Wide-Field Digital Camera</option>
<option value="sdss-24">SDSS: Sloan Digital Sky Survey</option>
</select>
<br>
<br>
Star positional error (verification tolerance), in arcseconds:
<input type="text" name="verify" size=10>
<br>
<br>
Code tolerance:
<select name="codetol">
<option value="0.005">0.005 (fast)</option>
<option value="0.01" selected>0.01 (standard)</option>
<option value="0.02">0.02 (exhaustive)</option>
</select>
<br>
<br>
Number of agreeing matches required:
<select name="nagree">
<option value="1">1 (exhaustive)</option>
<option value="2" selected>2 (standard)</option>
</select>
<br>
<br>
Agreement radius (in arcseconds):
<input type="text" name="agreedist" size=10>
<br>
<br>

<!-- Number of objects to look at: -->
<!-- input type="text" name="maxobj" size=10 -->

<input type="checkbox" name="tweak" checked>
Tweak (fine-tune the WCS by computing polynomial SIP distortion)
<br>
<br>

<input type="submit" value="Submit">

</form>


<hr>

</body>

</html>