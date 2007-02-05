<html>
<head>
<title>
Astrometry.net: Web Edition
</title>
</head>

<body>

<h2>
Astrometry.net: Web Edition
</h2>

<hr>

<form name="blindform" action="blind.php" method="PUT">

FITS file containing a table of detected objects, sorted by
brightest objects first:
<br>
<ul>
<li>File: <input type="file"   name="file" size=30>
<li>X column name: <input type="text" name="x_col" value="X" size=10>
<li>Y column name: <input type="text" name="y_col" value="Y" size=10>
<!-- li --><!-- input type="radio" name="parity" value="1" -->
</ul>
<br>
Bounds on the pixel scale of the image, in arcseconds per pixel:
<ul>
<li>Lower bound: <input type="text" name="fu_lower" size=10>
<li>Upper bound: <input type="text" name="fu_upper" size=10>
</ul>
<br>
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