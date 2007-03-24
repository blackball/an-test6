<?php
require_once 'MDB2.php';
require_once 'HTML/QuickForm.php';
require_once 'HTML/QuickForm/Renderer/QuickHtml.php';
require_once 'PEAR.php';

require 'common.php';
require 'presets.php';

$host  = $_SERVER['HTTP_HOST'];

$debug = 0;

$maxfilesize = 100*1024*1024;

umask(0002); // in octal

function printerror($err) {
	echo $err->getMessage() . "<br>";
	echo $err->getDebugInfo() . "<br>";
	die("error.");
}

PEAR::setErrorHandling(PEAR_ERROR_CALLBACK, 'printerror');

// Copy GET variables into POST, to allow defaults to be set
// by giving a GET URL, even though this is a POST form.
foreach ($_GET as $key=>$val) {
	if (!array_key_exists($key, $_POST))
		$_POST[$key]=$val;
}

loggit("Submitted values:\n");
foreach ($_POST as $key => $val) {
	loggit("  \"" . $key . "\" = \"" . $val . "\"\n");
}

$preset = $_GET["preset"];
if ($preset) {
	$params = $blind_presets[$preset];
	if ($params) {
		// Redirect to this page but with params set.
		$host  = $_SERVER['HTTP_HOST'];
		$uri  = $_SERVER['PHP_SELF'];
		header("Location: http://" . $host . $uri . "?" . $params);
	}
}

/***    Describe the HTML form.   ***/

$form =& new HTML_QuickForm('blindform','post');

$form->removeAttribute('name');

$formDefaults = array('x_col' => 'X',
					  'y_col' => 'Y',
					  'parity' => 2,
					  'index' => 'auto',
					  'poserr' => 1.0,
					  'tweak' => 1,
					  'imgurl' => "http://",
					  'fsunit' => 'degreewidth',
					  'skippreview' => '',
					  'justjobid' => '',
					  );
$form->setDefaults($formDefaults);

$form->addElement('hidden', 'skippreview');
$form->addElement('hidden', 'justjobid');

$xysrc_img =& $form->addElement('radio','xysrc',"img",null,'img');
$xysrc_url =& $form->addElement('radio','xysrc',"url",null,'url');
$xysrc_fits=& $form->addElement('radio','xysrc',"fits",null,'fits');

$fs_ul =& $form->addElement('radio','fstype',null,null,'ul');
$fs_ev =& $form->addElement('radio','fstype',null,null,'ev');

$imgfile  =& $form->addElement('file', 'imgfile', "imgfile",
							   array('onfocus' => "setXysrcImg()",
									 'onclick' => "setXysrcImg()",));
$fitsfile =& $form->addElement('file', 'fitsfile', "fitsfile",
							   array('onfocus' => "setXysrcFits()",
									 'onclick' => "setXysrcFits()"));
$form->addElement('text', 'x_col', "x column name",
				  array('size'=>5,
						'onfocus' => "setXysrcFits()"));
$form->addElement('text', 'y_col', "y column name",
				  array('size'=>5,
						'onfocus' => "setXysrcFits()"));
$form->addElement('text', 'imgurl', "imgurl",
				  array('size' => 40,
						'onfocus' => "setXysrcUrl()"));

$form->addElement('checkbox', 'tweak', 'tweak', null, null);

$form->addElement('text', 'fsl', 'field scale (lower bound)',
				  array('size'=>5,
						'onfocus' => "setFsUl()"));
$form->addElement('text', 'fsu', 'field scale (upper bound)',
				  array('size'=>5,
						'onfocus' => "setFsUl()"));
$form->addElement('text', 'fse', 'field scale (estimate)',
				  array('size'=>5,
						'onfocus' => "setFsEv()"));
$form->addElement('text', 'fsv', 'field scale (variance (%))',
				  array('size'=>5,
						'onfocus' => "setFsEv()"));

$units = array('arcsecperpix' => "arcseconds per pixel",
			   'arcminperpix' => "arcminutes per pixel",
			   'arcminwidth' => "width of the field (in arcminutes)", 
			   'degreewidth' => "width of the field (in degrees)", 
			   'focalmm' => "focal length of the lens (for 35mm film equivalent sensor)", 
			   );

$form->addElement('select', 'fsunit', 'units', $units, null);

$form->addElement('radio', 'parity', "Try both handednesses", null, 2);
$form->addElement('radio', 'parity', "Right-handed image", null, 0);
$form->addElement('radio', 'parity', "Left-handed image", null, 1);

$form->addElement('text', 'poserr', "star positional error (pixels)", array('size' => 5));

$indexes = array('auto' => "Automatic (based on image scale)",
				 "12arcmin" => "12-arcmin Fields (eg, Sloan Digital Sky Survey)",
				 "30arcmin" => "30-arcminute Fields",
				 "1degree" => "1-degree Fields",
				 "2degree" => "2-degree Fields",
				 "4degree" => "4-degree Fields",
				 "8degree" => "8-degree Fields",
				 "15degree" => "15-degree Fields");

$form->addElement('text', 'uname', "your name", array('size'=>40));
$form->addElement('text', 'email', "email address", array('size'=>40));

$form->addElement('select', 'index', 'index', $indexes, null);

$form->addElement('submit', 'submit', 'Submit');

$form->addElement('submit', 'linkhere', 'Link to these parameter settings');

$form->addElement('submit', 'imagescale', 'Try to guess scale from image headers');

$form->addElement('reset', 'reset', "Reset Form");

$form->setMaxFileSize($maxfilesize);

$form->addRule('xysrc', 'You must provide a field to solve!', 'required');
$form->addRule('poserr', 'Star positional error must be a number!', 'numeric');
$form->addRule('fsu', 'Upper bound must be numeric.', 'numeric');
$form->addRule('fsl', 'Lower bound must be numeric.', 'numeric');
$form->addRule('fse', 'Estimate must be numeric.', 'numeric');
$form->addRule('fsv', 'Variance must be numeric.', 'numeric');
$form->addFormRule('check_xysrc');
$form->addFormRule('check_fieldscale');
$form->addFormRule('check_poserr');

if ($form->exportValue("linkhere")) {
	$host  = $_SERVER['HTTP_HOST'];
	$uri  = $_SERVER['PHP_SELF'];
	$uri .= "?";
	$vals = $form->exportValues();
	$args = "";
	$flds = array('xysrc', 'fstype', 'fsl', 'fsu', 'fse', 'fsv',
				  'fsunit', 'parity', 'poserr', 'index',
				  'uname', 'email');
	switch ($vals["xysrc"]) {
	case "url":
		array_push($flds, "imgurl");
		break;
	case "img":
		//array_push($flds, "imgfile");
		$imgval = $imgfile->getValue();
		if ($imgval) {
			//loggit("image filename: " . $imgval['name'] . "\n");
			$args .= "&imgfile=" . urlencode($imgval['name']);
		}
		break;
	case "fits":
		//array_push($flds, "fitsfile");
		$fitsval = $fitsfile->getValue();
		if ($fitsval) {
			$args .= "&fitsfile=" . urlencode($fitsval['name']);
		}
		array_push($flds, "x_col");
		array_push($flds, "y_col");
		break;
	}
	foreach ($flds as $fld) {
		if ($vals[$fld] != $formDefaults[$fld])
			$args .= "&" . urlencode($fld) . "=" . urlencode($vals[$fld]);
	}
	if ($vals["tweak"]) {
		$args = "&tweak=1" . $args;
	}

	$uri .= substr($args, 1);

	header("Location: http://" . $host . $uri);
	exit;
}

if ($form->exportValue("imagescale")) {
	loggit("Guessing image scale.\n");
	guess_image_scale($form->exportValues());
	exit;
}

/** If the form has been filled out correctly, then process it. */

//if ($form->validate()) {
if ($form->exportValue("submit") && $form->validate()) {
	$form->freeze();
	$form->process('process_data', false);
    exit;
}

/**
Else, render the form.
Do this by loading the template file and doing textual
replacement on it. 
*/

$renderer =& new HTML_QuickForm_Renderer_QuickHtml();
$form->accept($renderer);

$template = file_get_contents($index_template);

// all the "regular" fields.
$flds = array('imgfile', 'fitsfile', 'imgurl', 'x_col', 'y_col',
			  'tweak', 'fsl', 'fsu', 'fse', 'fsv', 'fsunit',
			  'poserr', 'index', 'uname', 'email',
			  'submit', 'linkhere', 'imagescale', 'reset', 'MAX_FILE_SIZE');
foreach ($flds as $fld) {
	$template = str_replace("##".$fld."##", $renderer->elementToHtml($fld), $template);
}

// xysrc radio button IDs.
$id_url = $xysrc_url ->getAttribute('id');
$id_img = $xysrc_img ->getAttribute('id');
$id_fits= $xysrc_fits->getAttribute('id');
$template = str_replace("##xysrc-url-id##",  $id_url,  $template);
$template = str_replace("##xysrc-img-id##",  $id_img,  $template);
$template = str_replace("##xysrc-fits-id##", $id_fits, $template);

$id_ul = $fs_ul ->getAttribute('id');
$id_ev = $fs_ev ->getAttribute('id');
$template = str_replace("##fstype-ul-id##", $id_ul, $template);
$template = str_replace("##fstype-ev-id##", $id_ev, $template);

// fields (and pseudo-fields) that can have errors 
$errflds = array('xysrc', 'imgfile', 'imgurl', 'fitsfile',
				 'x_col', 'y_col', 'fstype', 'fsl', 'fsu', 'fse', 'fsv',
				 'poserr', 'fs');
foreach ($errflds as $fld) {
	$template = str_replace("##".$fld."-err##", $form->getElementError($fld), $template);
}

// "special" fields (radio buttons)
$repl = array("##xysrc-img##" => $renderer->elementToHtml('xysrc', 'img'),
			  "##xysrc-url##" => $renderer->elementToHtml('xysrc', 'url'),
			  "##xysrc-fits##" => $renderer->elementToHtml('xysrc', 'fits'),
			  "##parity-both##" => $renderer->elementToHtml('parity', '2'),
			  "##parity-left##" => $renderer->elementToHtml('parity', '1'),
			  "##parity-right##" => $renderer->elementToHtml('parity', '0'),
			  "##fstype-ul##" => $renderer->elementToHtml('fstype', 'ul'),
			  "##fstype-ev##" => $renderer->elementToHtml('fstype', 'ev'),
			  );
foreach ($repl as $from => $to) {
	$template = str_replace($from, $to, $template);
}

if (array_key_exists('imgfile', $headers)) {
	// Hack - we hard-code the table row here instead of having it in the template
	// (so that if it's empty we don't get an empty table row)
	$str = "<tr><td></td><td>Previous value: <tt>" . $headers['imgfile'] . "</tt></td></tr>\n";
	$template = str_replace("##imgfile_set##", $str, $template);
} else {
	$template = str_replace("##imgfile_set##", "", $template);
}
if (array_key_exists('fitsfile', $headers)) {
	// Hack - ditto.
	$str = "<li>Previous value: <tt>" . $headers['fitsfile'] . "</tt></li\n";
	$template = str_replace("##fitsfile_set##", $str, $template);
} else {
	$template = str_replace("##fitsfile_set##", "", $template);
}

?>
<?php
// Careful with the spacing around this - it must be the first
// thing on the page - with no blank lines preceding it!
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
<link rel="stylesheet" type="text/css" href="index.css" />
<link rel="shortcut icon" type="image/x-icon" href="favicon.ico" />
<link rel="icon" type="image/png" href="favicon.png" />
<meta http-equiv="Content-Script-Type" content="text/javascript" />
</head>
<body>

<hr />
<h2>
Astrometry.net: Web Edition
</h2>
<hr />

<?php
// Here's where we render the form...
echo $renderer->toHtml($template);
?>

<hr />

<p>
Note, there is a file size limit of
<?php
printf("%d", $maxfilesize / (1024*1024));
?>
MB.  Your browser may fail silently if you try to upload a file that exceeds this.
</p>

<hr />

<?php
echo $valid_blurb;
?>

</body>
</html>


<?php

/** Parameter value checking. **/

function check_xysrc($vals) {
    global $imgfile;
    global $fitsfile;
	if (!$vals['xysrc']) {
		return array("xysrc"=>"");
	}
	switch ($vals['xysrc']) {
	case 'img':
		if ($imgfile->isUploadedFile()) {
			return TRUE;
		}
		$imgval = $imgfile->getValue();
		/*
           Array (
             [name] => apod.xy.fits
             [type] => application/octet-stream
             [tmp_name] => /tmp/phpKBpGbt
             [error] => 0
             [size] => 0
           )
		*/
		if ($imgval["size"] == 0) {
			return array("imgfile" => "Image file is empty.");
		}
		return array("imgfile"=>"You must upload an image file!");
	case 'fits':
		if (!$fitsfile->isUploadedFile()) {
			return array("fitsfile" => "You must upload a FITS file!");
		}
		if (!$vals['x_col'] || !strlen($vals['x_col'])) {
			return array("x_col"=>"You must provide the name of the X column!");
		}
		if (!$vals['y_col'] || !strlen($vals['y_col'])) {
			return array('y_col'=>"You must provide the name of the Y column!");
		}
		$fitsval = $fitsfile->getValue();
		if ($fitsval["size"] == 0) {
			return array("fitsfile" => "FITS file is empty.");
		}
		return TRUE;
	case 'url':
		if (@parse_url($vals['imgurl']))
			return TRUE;
		return array("imgurl"=>"You must provide a valid URL.");
	}
	return array("imgfile"=>"I'M CONFUSED");
}

function check_fieldscale($vals) {
	$type = $vals["fstype"];
	$l = (double)$vals["fsl"];
	$u = (double)$vals["fsu"];
	$e = (double)$vals["fse"];
	$v = (double)$vals["fsv"];

	if (!$type) {
		return array("fstype"=>"You must select one of the checkboxes below");
	}
	if ($type == "ul") {
		if ($l > 0 && $u > 0)
			return TRUE;
		if ($l < 0)
			return array("fsl"=>"Lower bound must be positive!");
		if ($u < 0)
			return array("fsu"=>"Upper bound must be positive!");
		/*
		if (($l && !$u) || ($u && !$l)) {
			return array("fsu"=>"You must give BOTH upper and lower bounds.");
		*/
	} else if ($type == "ev") {
		if ($e > 0 && $v > 0)
			return TRUE;
		if ($e < 0)
			return array("fse"=>"Estimate must be positive!");
		if ($v < 1 || $v > 99)
			return array("fsv"=>"% Eror must be between 1 and 99!");
		/*
		if (($e && !$v) || ($v && !$e)) {
			return array("fsu"=>"You must give BOTH an estimate and error.");
		}
		*/
	}
	return array("fstype"=>"Invalid fstype");
}

function check_poserr($vals) {
	if ($vals["poserr"] <= 0) {
		return array("poserr"=>"Positional error must be positive.");
	}
	if ($vals["poserr"] >= 10) {
		return array("poserr"=>"Positional error must be less than 10 pixels.");
	}
	return TRUE;
}

/** Accept the form! **/

function process_data ($vals) {
	global $form;
    global $imgfile;
    global $fitsfile;
	global $resultdir;
	global $dbfile;
	global $maxfilesize;
	global $maxtime;
	global $maxquads;
	global $maxcpu;
	global $rdlsinfo;
	global $xylsinfo;
	global $indexdir;
	global $printsolved;
	global $wcs_xy2rd;
	global $wcs_rd2xy;
	global $input_fn;
	global $inputtmp_fn;
	global $donescript_fn;
	global $indexrdls_fn;
	global $indexxyls_fn;
	global $rdls_fn;
	global $rdlsinfo_fn;
	global $match_fn;
	global $solved_fn;
	global $cancel_fn;
	global $wcs_fn;
	global $start_fn;
	global $done_fn;
	global $log_fn;
	global $jobdata_fn;
	global $tabmerge;
	global $modhead;
	global $headers;

	$xysrc = $vals["xysrc"];
	$imgurl = $vals["imgurl"];

	// Create a directory for this request.
	$myname = create_random_dir($resultdir);
	if (!$myname) {
		die("Failed to create job directory.");
	}
	$mydir = $resultdir . $myname . "/";

	// Create a database...
	$dbpath = $mydir . $jobdata_fn;
	if (!create_db($dbpath)) {
		die("failed to create db.");
	}
	$db = connect_db($dbpath);
	if (!$db) {
		die("failed to connect db.");
	}
	if (!create_jobdata_table($db)) {
		die("failed to create table.");
	}

	$jobdata = array("maxquads" => $maxquads,
					 "cpulimit" => $cpulimit,
					 "timelimit" => $timelimit);

	$flds = array('xysrc', 'imgfile', 'fitsfile', 'imgurl',
				  'x_col', 'y_col', 'fstype',
				  'tweak', 'fsl', 'fsu', 'fse', 'fsv', 'fsunit',
				  'poserr', 'index', 'submit');

	foreach ($flds as $fld) {
		$jobdata[$fld] = $vals[$fld];
	}

	// optional fields
	$flds = array('uname', 'email');
	foreach ($flds as $fld) {
		if ($vals[$fld])
			$jobdata[$fld] = $vals[$fld];
	}

	if (!setjobdata($db, $jobdata)) {
		die("failed to set job values.");
	}
	loggit("created db " . $dbpath . "\n");

	loggit("xysrc: " . $xysrc . "\n");

	$imgfilename = "";
	$imgbasename = "";
	//$origname = "";
	if ($xysrc == "url") {
		// Try to retrieve the URL...
		loggit("retrieving url " . $imgurl . " ...\n");

		$imgbasename = "downloaded.";
		$downloadedimg = $mydir . $imgbasename;
		$imgfilename = $downloadedimg;
		loggit("Writing to file: " . $downloadedimg . "\n");
		if (download_url($imgurl, $downloadedimg, $maxfilesize, $errmsg) === FALSE) {
			die("Failed to download image: " . $errmsg);
		}

	} else if ($xysrc == "img") {
		// If an image was uploaded, move it into place.
		// The filename on the user's machine:
		$imgval = $imgfile->getValue();
		$origname = $imgval["name"];
		if (!setjobdata($db, array("image-origname"=>$origname))) {
			die("failed to update jobdata: image-origname");
		}

		$imgbasename = "uploaded.";
		$imgfilename = $mydir . $imgbasename;
		if (!$imgfile->moveUploadedFile($mydir, $imgbasename)) {
			die("failed to move uploaded file into place.");
		}
	}

	if ($imgfilename) {
		if (!convert_image($imgfilename, $mydir, $suffix,
						   $errstr, $W, $H, $shrink, $dispW, $dispH,
						   $dispimgbase, $dispimgpngbase)) {
			die($errstr);
		}

		$newname = $imgfilename . $suffix;
		if (!rename($imgfilename, $newname)) {
			die("failed to rename img file.");
		}
		loggit("Renamed image file to " . $newname . "\n");
		$imgfilename = $newname;
		$imgbasename .= $suffix;

		if (!setjobdata($db, array("imageW" => $W,
								   "imageH" => $H,
								   "imageshrink" => $shrink,
								   "displayW" => $dispW,
								   "displayH" => $dispH,
								   "displayImage" => $dispimgbase,
								   "displayImagePng" => $dispimgpngbase,
								   "imagefilename" => $imgbasename))) {
			die("failed to save image {filename,W,H} in database.");
		}
	}

	$xylistfilename = "field.xy"; // without .fits
	$xylist = $mydir . $xylistfilename;
	$xylsinfofile = $mydir . "xylsinfo";

	if ($xysrc == "fits") {
		// If a FITS bintable file was uploaded, move it into
		// place...
		if (!$fitsfile->moveUploadedFile($mydir, $xylistfilename . ".fits")) {
			die("failed to move uploaded FITS file into place.");
		}

		// Try to get the size of the image...
		$cmd = $xylsinfo . " " . $xylist . ".fits > " . $xylsinfofile;
		$res = system($cmd, $retval);
		if ($retval) {
			loggit("Command failed: return val " . $retval . ", str " . $res . "\n");
			die("failed to run command: " . $cmd . "\n");
		}
		if (!file_exists($xylsinfofile)) {
			die("xylsinfo file " . $xylsinfofile . " does not exist.");
		}
		$info = file($xylsinfofile);
		foreach ($info as $str) {
			$words = explode(" ", rtrim($str, "\n"));
			$infomap[$words[0]] = implode(" ", array_slice($words, 1));
		}
		$W = $infomap["width"];
		$H = $infomap["height"];
		if (!setjobdata($db, array("xylsW"=>$W, "xylsH"=>$H))) {
			die("failed to save xyls W,H in database.");
		}

		loggit("found xyls width and height " . $W . ", " . $H . "\n");
	}

	if (!chmod($xylist . ".fits", 0664)) {
		die("Failed to chmod xylist " . $xylist);
	}

	$inputfile = $mydir . $input_fn;
	$inputtmpfile = $mydir . $inputtmp_fn;
	$donescript = $mydir . $donescript_fn;
	$rdlist = $mydir . $rdls_fn;
	$indexrdls = $mydir . $indexrdls_fn;
	$indexxyls = $mydir . $indexxyls_fn;
	$rdlsinfofile = $mydir . $rdlsinfo_fn;
	$matchfile = $mydir . $match_fn;
	$solvedfile = $mydir . $solved_fn;
	$cancelfile = $mydir . $cancel_fn;
	$wcsfile = $mydir . $wcs_fn;
	$startfile = $mydir . $start_fn;
	$donefile = $mydir . $done_fn;
	$logfile = $mydir . $log_fn;

	$index = $vals["index"];
	$x_col = $vals["x_col"];
	$y_col = $vals["y_col"];
	$parity = $vals["parity"];
	$tweak = $vals["tweak"];
	$poserr = $vals["poserr"];

	$codetol = 0.01;
	$nagree = 0;

	$inputfile_orig = $inputfile;
	if ($imgfilename) {
		// Write to "input.tmp" instead of "input", so we don't trigger
		// the solver just yet...
		$inputfile = $inputtmpfile;
	}

	// MAJOR HACK - pause to let the watcher notice that the
	// new directory was created.
	sleep(1);

	$fstype = $vals["fstype"];
	$fsunit = $vals["fsunit"];
	$fsu = $vals["fsu"];
	$fsl = $vals["fsl"];
	$fse = $vals["fse"];
	$fsv = $vals["fsv"];

	if ($fstype == "ev") {
		// Estimate/Variance given - convert to lower/upper.
		// FIXME - is this the right way to do this?
		$fsu = $fse * (1 + $fsv/100);
		$fsl = $fse * (1 - $fsv/100);
	}

	$fu_lower = 0.0;
	$fu_upper = 0.0;
	switch ($fsunit) {
	case "arcsecperpix":
 		$fu_lower = $fsl;
		$fu_upper = $fsu;
		break;
	case "arcminperpix":
 		$fu_lower = 60.0 * $fsl;
		$fu_upper = 60.0 * $fsu;
		break;
	case "arcminwidth":
		$fu_lower = (60.0 * $fsl) / $W;
		$fu_upper = (60.0 * $fsu) / $W;
		break;
	case "degreewidth":
		$fu_lower = (3600.0 * $fsl) / $W;
		$fu_upper = (3600.0 * $fsu) / $W;
		break;
	case "focalmm":
		$arc = 2 * atan(35 / (2 * $fsu));
		$fu_lower = $arc * 180/M_PI * 3600 / $W;
		$arc = 2 * atan(35 / (2 * $fsl));
		$fu_upper = $arc * 180/M_PI * 3600 / $W;
	}

	loggit("Computed field unit bounds [" . $fu_lower . ", " . $fu_upper . "]\n");

	if (($fu_upper == 0.0) || ($fu_lower == 0.0)) {
		die("Field scale lower or upper bound is zero: " . $fu_lower . ", " . $fu_upper . "\n");
	}

	if ($index == "auto") {
		// Estimate size of quads we could find:
		$fmax = 0.5  * min($W, $H) * $fu_upper / 60.0;
		$fmin = 0.1 * min($W, $H) * $fu_lower / 60.0;
		loggit("W=$W, H=$H, min(W,H)=" . min($W,$H) . ", fu_upper=$fu_upper.\n");
		loggit("Collecting indexes with quads in range [" . $fmin . ", " . $fmax . "] arcmin.\n");
		// FIXME - should condense all this index data into one array!
		// (list largest-to-smallest).
		$sizemap = array("15degree" => array(200,400),
						 "8degree" => array(120,180),
						 "4degree" => array(60, 90),
						 "2degree" => array(32, 40),
						 "1degree" => array(16, 20),
						 "30arcmin" => array(8, 10),
						 "12arcmin" => array(4, 5),);
		/*
		$sizemap = array("12arcmin" => array(4, 5),
						 "30arcmin" => array(8, 10),
						 "1degree" => array(16, 20),
						 "2degree" => array(32, 40),
						 "4degree" => array(60, 90),
						 "8degree" => array(120,180),
						 "15degree" => array(200,400),
						 );
		*/
		$largest = "15degree";
		$smallest = "12arcmin";

		$indexes = array();
		foreach ($sizemap as $ind => $size) {
			// If they overlap...
			if (!(($fmin > $size[1]) || ($fmax < $size[0]))) {
				array_push($indexes, $ind);
			}
		}

		if (count($indexes) == 0) {
			// too big, or too little?
			if ($fmax > $sizemap[$largest][1]) {
				array_push($indexes, $largest);
			} else if ($fmin < $sizemap[$smallest][0]) {
				array_push($indexes, $smallest);
			}
		}

		loggit("Quad size limits: [" . $fmin . ", " . $fmax . "] arcmin, " .
			   "selected indexes: " . implode(", ", $indexes) . "\n");

	} else {
		$indexes = array($index);
	}
	// Compute the index path(s)
	$indexmap = array("12arcmin" => array("sdss-23/sdss-23-allsky"),
					  "30arcmin" => array("allsky-31/allsky-31"),
					  "1degree" => array("allsky-32/allsky-32"),
					  "2degree" => array("allsky-37/allsky-37"),
					  "4degree" => array("allsky-34/allsky-34"),
					  //"8degree" => array("allsky-35/allsky-35"),
					  "8degree" => array("allsky-38/allsky-38"),
					  //"15degree" => array("allsky-36/allsky-36"),
					  //"15degree" => array("allsky-39/allsky-39"),
					  "15degree" => array("allsky-40/allsky-40"),
					  );
	$indexpaths = array();
	foreach ($indexes as $i) {
		$indexpaths = array_merge($indexpaths, $indexmap[$i]);
	}
	$indexes = $indexpaths;

	// Write the input file for blind...
	$fin = fopen($inputfile, "w");
	if (!$fin) {
		die("Failed to write input file " . $inputfile);
	}

	fprintf($fin, "log " . $logfile . "\n");
	foreach ($indexes as $ind) {
		fprintf($fin, "index " . $indexdir . $ind . "\n");
	}
	$str = "start " . $startfile . "\n" .
		"donescript " . $donescript . "\n" .
		"field " . $xylist . "\n" .
		"solved " . $solvedfile . "\n" .
		"cancel " . $cancelfile . "\n" .
		"match " . $matchfile . "\n" .
		"wcs " . $wcsfile . "\n" .
		"indexrdls " . $indexrdls . "\n" .
		"fields 0\n" .
		"xcol " . $x_col . "\n" .
		"ycol " . $y_col . "\n" .
		"sdepth 0\n" .
		"depth 200\n" .
		"parity " . $parity . "\n" .
		"fieldunits_lower " . $fu_lower . "\n" .
		"fieldunits_upper " . $fu_upper . "\n" .
		"tol " . $codetol . "\n" .
		"verify_pix " . $poserr . "\n" .
		"nagree_toverify " . $nagree . "\n" .
		//"ninfield_tokeep 50\n" .
		//"ninfield_tosolve 50\n" .
		"ninfield_tokeep 25\n" .
		"ninfield_tosolve 25\n" .
		"overlap_tokeep 0.25\n" .
		"overlap_tosolve 0.25\n" .
		"maxquads " . $maxquads . "\n" .
		"cpulimit " . $maxcpu . "\n" .
		"timelimit " . $maxtime . "\n" .
		($tweak ? "tweak\n" : "") .
		"run\n" .
		"\n";
	fprintf($fin, $str);

	if (!fclose($fin)) {
		die("failed to write blind input file.");
	}

	loggit("Wrote blind input file: " . $inputfile . "\n");

	// Write the donescript for blind...
	$fdone = fopen($donescript, "w");
	if (!$fdone) {
		die("Failed to write donescript " . $donescript);
	}
	fprintf($fdone,
			"#! /bin/bash\n" .
			"echo Starting donescript...\n" .
			"touch " . $donefile . "\n" .
			"if [ `" . $printsolved . " " . $solvedfile . " | grep -v \"File\" | wc -w` -eq 1 ]; then \n" .
			"  echo \"Field solved.\";\n" .
			"  echo Running wcs-xy2rd...;\n" .
			"  " . $wcs_xy2rd . " -w " . $wcsfile . " -i " . $xylist . ".fits" . " -o " . $rdlist . " -X " . $x_col . " -Y " . $y_col . ";\n" .
			"  echo Running rdlsinfo...;\n" .
			"  " . $rdlsinfo . " " . $rdlist . " > " . $rdlsinfofile . ";\n" .
			"  echo Merging index rdls file...\n" .
			"  cp " . $indexrdls . " " . $indexrdls . ".orig\n" .
			"  cp " . $indexrdls . " " . $indexrdls . ".tmp\n" .
			"  for ((i=2; i<=" . count($indexes) . "; i++)); do\n" .
			"    " . $tabmerge . " " . $indexrdls . "+\$i " . $indexrdls . ".tmp+1\n" .
			"  done\n" .
			"  mv " . $indexrdls . ".tmp " . $indexrdls . "\n" .
			"  " . $wcs_rd2xy . " -w " . $wcsfile . " -i " . $indexrdls . " -o " . $indexxyls . ";\n" .
			"  echo Adding image size to WCS file...\n" .
			"  " . $modhead . " " . $wcsfile . " IMAGEW " . $W . "\n" .
			"  " . $modhead . " " . $wcsfile . " IMAGEH " . $H . "\n" .
			"else\n" .
			"  echo \"Field did not solve.\";\n" .
			"fi\n" .
			"echo Donescript finished.\n"
			);
	if (!fclose($fdone)) {
		die("Failed to close donescript " . $donescript);
	}
	if (!chmod($donescript, 0775)) {
		die("Failed to chmod donescript " . $donescript);
	}

	if ($headers["justjobid"]) {
		// skip the "source extraction" preview, just start crunching!
		loggit("justjobid set.  imgfilename=" . $imgfilename . "\n");
		if ($imgfilename) {
			if (!rename($inputtmpfile, $inputfile_orig)) {
				die("Failed to rename input temp file " . $inputtmpfile . " to " . $inputfile_orig);
			}
			loggit("renamed $inputtmpfile to $inputfile_orig.\n");
		}

		header('Content-type: text/plain');
		echo $myname . "\n";
		exit;
	}

	// Redirect the client to the status page...
	$status_url = "status.php?job=" . $myname;

	if ($imgfilename && !$headers["skippreview"]) {
		$status_url .= "&img";
	}
	$host  = $_SERVER['HTTP_HOST'];
	$uri  = rtrim(dirname($_SERVER['PHP_SELF']), '/\\');
	header("Location: http://" . $host . $uri . "/" . $status_url);
	exit;
}

// skeleton for guess_image_type
function get_image_type($filename, &$xtopnm) {
}

function convert_image($filename, $mydir,
					   &$addsuffix, &$errstr, &$W, &$H, &$shrink,
					   &$dispW, &$dispH, &$dispimgbase, &$dispimgpngbase) {
	global $fits2xy;
	global $modhead;
	global $plotxy2;
	global $tabsort;
	global $objs_fn;
	global $fits2xyout_fn;
	global $an_fitstopnm;

	loggit("image file: " . filesize($img) . " bytes.\n");

	$addsuffix = "";

	$typestr = shell_exec("file -b -N " . $filename);
	loggit("type: " . $typestr . " for " . $filename . "\n");

	// handle compressed files.
	$comptype = array("gzip compressed data"  => array(".gz",  "gunzip  -cd %s > %s"),
					  "bzip2 compressed data" => array(".bz2", "bunzip2 -cd %s > %s"),
					  "Zip archive data"      => array(".zip", "unzip   -p  %s > %s"),
					  );
	// look for key phrases in the output of "file".
	foreach ($comptype as $phrase => $lst) {
		if (strstr($typestr, $phrase)) {
			$suff    = $lst[0];
			$command = $lst[1];
			$newfilename = $mydir . "uncompressed.";
			$cmd = sprintf($command, $filename, $newfilename);
			loggit("Command: " . $cmd . "\n");
			if (system($cmd, $retval)) {
				loggit("Command failed, return value " . $retval . ": " . $cmd);
				die("failed to decompress image");
			}
			$addsuffix = $suff;
			$filename = $newfilename;
			$typestr = shell_exec("file -b -N -L " . $filename);
			loggit("type: " . $typestr . " for " . $filename . "\n");
			break;
		}
	}

	// Handle different image types
	$imgtypes = array("FITS image data"  => array("fits", $an_fitstopnm . " -i %s > %s"),
					  "JPEG image data"  => array("jpg",  "jpegtopnm %s > %s"),
					  "PNG image data"   => array("png",  "pngtopnm %s > %s"),
					  "GIF image data"   => array("gif",  "giftopnm %s > %s"),
					  "Netpbm PPM"       => array("pnm",  "ppmtoppm < %s > %s"),
					  "Netpbm PGM"       => array("pnm",  "pgmtoppm %s > %s"),
					  );

	$pnmimg_orig_base = "image.pnm";
	$pnmimg = $mydir . $pnmimg_orig_base;
	$pnmimg_orig = $pnmimg;

	$gotit = FALSE;
	// Look for key phrases in the output of "file".
	foreach ($imgtypes as $phrase => $lst) {
		if (strstr($typestr, $phrase)) {
			$suff    = $lst[0];
			$command = $lst[1];
			$cmd = sprintf($command, $filename, $pnmimg);
			loggit("Command: " . $cmd . "\n");
			if (system($cmd, $retval)) {
				loggit("Command failed, return value " . $retval . ": " . $cmd);
				die("failed to convert image");
			}
			$addsuffix = $suff . $addsuffix;
			$imgtype = $suff;
			$gotit = TRUE;
			break;
		}
	}
	if (!$gotit) {
		die("Unknown image type: " . $typestr);
	}

	// Use "pnmfile" to get the image size.
	$cmd = "pnmfile " . $pnmimg;
	loggit("Command: " . $cmd . "\n");
	$res = shell_exec($cmd);
	if (!$res) {
		die("pnmfile failed.");
	}
	//loggit("Pnmfile: " . $res . "\n");

	// eg, "/home/gmaps/ontheweb-data/13a732d8ff/image.pnm: PGM raw, 4096 by 4096  maxval 255"
	$pat = '/.*P.M .*, ([[:digit:]]*) by ([[:digit:]]*) *maxval [[:digit:]]*/';
	if (!preg_match($pat, $res, &$matches)) {
		die("preg_match failed: string is \"" . $res . "\"\n");
	}
	$W = (int)$matches[1];
	$H = (int)$matches[2];
	/*
		for ($i=0; $i<count($matches); $i++) {
			loggit("matches[$i]=" . $matches[$i] . "\n");
		}
	*/

	// choose a power-of-two shrink factor that makes the larger dimension <= 800.
	$maxsz = 800;
	$bigger = max($W,$H);
	if ($bigger > $maxsz) {
		$shrink = pow(2, ceil(log($bigger / $maxsz, 2)));
	} else {
		$shrink = 1;
	}

	// Also use the output from "pnmfile" to decide if we need to convert to PGM.
	$ss = strstr($res, "PPM");
	//loggit("strstr: " . $ss . "\n");
	if (strlen($ss)) {
		// reduce to PGM.
		$pgmimg = $mydir . "image.pgm";
		$cmd = "ppmtopgm " . $pnmimg . " > " . $pgmimg;
		loggit("Command: " . $cmd . "\n");
		$res = FALSE;
		$res = system($cmd, $retval);
		if ($retval) {
			loggit("Command failed: return val " . $retval . ", str " . $res . "\n");
			die("Failed to reduce image to grayscale.");
		}
		$pnmimg = $pgmimg;
	}

	if ($imgtype == "fits") {
		$fitsimg = $img;
	} else {
		$fitsimg = $mydir . "image.fits";
		$cmd = "pnmtofits " . $pnmimg . " > " . $fitsimg;
		loggit("Command: " . $cmd . "\n");
		$res = FALSE;
		$res = system($cmd, $retval);
		if ($retval) {
			loggit("Command failed: return val " . $retval . ", str " . $res . "\n");
			die("Failed to convert image to FITS.");
		}
	}

	// fits2xy computes the output filename by trimming .fits and adding .xy.fits.
	$xylist = substr($fitsimg, 0, strlen($fitsimg) - strlen(".fits")) . ".xy.fits";

	$fits2xyout = $mydir . $fits2xyout_fn;
	$cmd = $fits2xy . " " . $fitsimg . " > " . $fits2xyout . " 2>&1";
	loggit("Command: " . $cmd . "\n");
	$res = FALSE;
	$res = system($cmd, $retval);
	if ($retval) {
		loggit("Command failed: return val " . $retval . ", str " . $res . "\n");
		$errstr = "Failed to perform source extraction.";
		return FALSE;
	}

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

	// Size of image to display to the user.
	$dispW = (int)($W / $shrink);
	$dispH = (int)($H / $shrink);
	if ($shrink == 1) {
		$dispimg = $pnmimg_orig;
		$dispimgbase = $pnmimg_orig_base;
	} else {
		$dispimgbase = "shrink.pnm";
		$dispimg = $mydir . $dispimgbase;
		$cmd = "pnmscale -reduce " . $shrink . " " . $pnmimg_orig . " > " . $dispimg;
		$res = system($cmd, $retval);
		if ($retval) {
			loggit("Command failed: return val " . $retval . ", str " . $res . "\n");
			$errstr = "Failed to shrink image file.";
			return FALSE;
		}
	}

	$dispimgpngbase = substr($dispimgbase, 0, -4) . ".png";
	$cmd = "pnmtopng " . $dispimg . " > " . $mydir . $dispimgpngbase;
	$res = system($cmd, $retval);
	if ($retval) {
		loggit("Command failed: return val " . $retval . ", str " . $res . "\n");
		$errstr = "Failed to convert image to PNG.";
		return FALSE;
	}

	// Plot the extracted objects.
	$Nbright = 100;
	// -the brightest:
	$objimg1 = $mydir . "objs1.pgm";
	$cmd = $plotxy2 . " -i " . $xylist . " -W " . $dispW . " -H " . $dispH .
		" -x 1 -y 1 -w 1.75 -S " . (1/$shrink) . " -N " . $Nbright . " > " . $objimg1;
	loggit("Command: " . $cmd . "\n");
	$res = FALSE;
	$res = system($cmd, $retval);
	if ($retval) {
		loggit("Command failed: return val " . $retval . ", str " . $res . "\n");
		$errstr = "Failed to plot extracted sources.";
		return FALSE;
	}
	// -the rest:
	$Nmax = 500;
	$objimg2 = $mydir . "objs2.pgm";
	// FIXME - interaction between (1,1) offset and scaling is wrong -
	// we need to offset then scale, not vice versa.
	$cmd = $plotxy2 . " -i " . $xylist . " -W " . $dispW . " -H " . $dispH .
		" -x 1 -y 1" . " -n " . $Nbright . " -N " . $Nmax . " -r 3 -w 1.75" .
		" -S " . (1/$shrink) . " > " . $objimg2;
	loggit("Command: " . $cmd . "\n");
	$res = FALSE;
	$res = system($cmd, $retval);
	if ($retval) {
		loggit("Command failed: return val " . $retval . ", str " . $res . "\n");
		$errstr = "Failed to plot extracted sources.";
		return FALSE;
	}
	// -the sum:
	$objimg = $mydir . "objs.pgm";
	$objimg_orig = $objimg;
	$cmd = "pnmarith -max " . $objimg1 . " " . $objimg2 . " > " . $objimg;
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
	$cmd = "pnmcomp -alpha=" . $objimg_orig . " " . $redimg . " " . $dispimg . " " . $sumimg;
	loggit("Command: " . $cmd . "\n");
	$res = FALSE;
	$res = system($cmd, $retval);
	if ($retval) {
		loggit("Command failed: return val " . $retval . ", str " . $res . "\n");
		$errstr = "Failed to composite image of extracted sources.";
		return FALSE;
	}

	$sumimgpng = $mydir . $objs_fn;
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


function guess_image_scale($vals) {
	global $resultdir;
    global $imgfile;
	global $maxfilesize;
	global $fits_guess_scale;

	$xysrc = $vals["xysrc"];

	// Create a directory for this request.
	$myname = create_random_dir($resultdir);
	if (!$myname) {
		die("Failed to create job directory.");
	}
	$mydir = $resultdir . $myname . "/";

	$imgbase = "";
	$imgfilename = "";
	if ($xysrc == "url") {
		$imgurl = $vals["imgurl"];
		// Try to retrieve the URL...
		loggit("retrieving url " . $imgurl . " ...\n");
		$usetype = get_image_type($imgurl, $xtopnm);
		if (!$usetype) {
			loggit("unknown image type.\n");
			die("Unknown image type.");
		}
		loggit("Assuming image type: " . $usetype . "\n");

		$imgbase = "downloaded." . $usetype;
		$downloadedimg = $mydir . $imgbase;
		loggit("Writing to file: " . $downloadedimg . "\n");

		if (download_url($imgurl, $downloadedimg, $maxfilesize, $errmsg) === FALSE) {
			die("Failed to download image: " . $errmsg);
		}
		$imgfilename = $downloadedimg;

	} else if ($xysrc == "img") {
		// If an image was uploaded, move it into place.
		// The filename on the user's machine:
		$imgval = $imgfile->getValue();
		$origname = $imgval["name"];
		$usetype = get_image_type($origname, $xtopnm);
		if (!$usetype) {
			loggit("unknown image file type.\n");
			die("Unknown image file type.");
		}
		loggit("original image file: " . $origname . ", using type: " . $usetype . "\n");

		$imgbase = "uploaded." . $usetype;
		$imgfilename = $mydir . $imgbase;
		if (!$imgfile->moveUploadedFile($mydir, "uploaded." . $usetype)) {
			die("failed to move uploaded file into place.");
		}
	}

	if (!$imgfilename) {
		die("no image file specified.");
	}

	if ($usetype == "fits") {
		$gscale = $mydir . "guess-scale";
		$cmd = $fits_guess_scale . " " . $imgfilename . " 2>/dev/null >" . $gscale;
		$res = system($cmd, $retval);
		if ($retval) {
			die("Failed to run fits-guess-scale.");
		}
		$lines = file($gscale);
		$minscale = 1e100;
		$maxscale = 0.0;

		echo "<html><head><title>Astrometry.net</title></head>\n";
		echo "<h3>Scale values found in the FITS file:</h3>\n";
		echo "<p align=\"center\">\n";
		echo "<table border=\"1\">\n";
		echo "<tr><th>Header</th><th>Scale (arcsec/pixel)</th></tr>\n";

		foreach ($lines as $line) {
			$words = explode(" ", rtrim($line, "\n"));
			if ($words[0] != "scale")
				continue;
			$scale = (float)$words[2];
			if ($scale == 0.0)
				continue;
			loggit("method " . $words[1] . " say scale is " . $scale . "\n");
			$minscale = min($scale, $minscale);
			$maxscale = max($scale, $maxscale);

			echo "<tr><td align=\"center\">" . strtoupper($words[1]) . "</td><td align=\"center\">" . $scale . "</td></tr>\n";
		}

		echo "</table>\n";
		echo "</p>\n";

		if ($maxscale == 0) {
			echo "<p align=\"center\">\n";
			echo "No scale information found.\n";
			echo "</p>\n";
		}

		$host  = $_SERVER['HTTP_HOST'];
		$uri  = rtrim(dirname($_SERVER['PHP_SELF']), '/\\');

		echo "<p align=\"center\">\n";
		echo "<form action=\"" . $_SERVER['PHP_SELF'] . "\" method=\"get\">\n" .
			"<input type=\"hidden\" name=\"xysrc\" value=\"url\" />\n" .
			"<input type=\"hidden\" name=\"tweak\" value=\"1\" />\n" .
			"<input type=\"hidden\" name=\"imgurl\" value=\"" .
			htmlentities("http://" . $host . $uri . "/status/" . $myname . "/" . $imgbase) .
			"\" />\n";

		if ($maxscale > 0) {
			if ($maxscale == $minscale) {
				echo "<input type=\"hidden\" name=\"fstype\" value=\"ev\" />\n" .
					"<input type=\"hidden\" name=\"fse\" value=\"" . $minscale . "\" />\n" .
					"<input type=\"hidden\" name=\"fsv\" value=\"" . 5 . "\" />\n";
			} else {
				// ensure 5% margin.
				if ($maxscale / $minscale < 1.05) {
					$gmean = sqrt($maxscale * $minscale);
					// FIXME - this isn't quite right - we use a different method elsewhere
					$maxscale = $gmean * sqrt(1.05);
					$minscale = $gmean / sqrt(1.05);
				}
				echo "<input type=\"hidden\" name=\"fstype\" value=\"ul\" />\n" .
					"<input type=\"hidden\" name=\"fsl\" value=\"" . $minscale . "\" />\n" .
					"<input type=\"hidden\" name=\"fsu\" value=\"" . $maxscale . "\" />\n";
			}

			echo "<input type=\"hidden\" name=\"fsunit\" value=\"arcsecperpix\" />\n" .
				"<input type=\"submit\" name=\"use-scale\" value=\"Use this scale\" />\n";
		} else {
			"<input type=\"submit\" name=\"go-back\" value=\"Go back\" />\n";
		}

		echo "</form>\n";
		echo "</p>\n";

	} else {
		$imgtype = exif_imagetype($imgfilename);
		if ($imgtype === FALSE) {
			die("Failed to run exif_imagetype on image " . $imgfilename . "\n");
		}
		$types = array(IMAGETYPE_GIF => "gif",
					   IMAGETYPE_JPEG => "jpeg",
					   IMAGETYPE_PNG => "png",
					   IMAGETYPE_SWF => "swf",
					   IMAGETYPE_PSD => "psd",
					   IMAGETYPE_BMP => "bmp",
					   IMAGETYPE_TIFF_II => "tiff(le)",
					   IMAGETYPE_TIFF_MM => "tiff(be)",
					   IMAGETYPE_JPC => "jpc",
					   IMAGETYPE_JP2 => "jp2",
					   IMAGETYPE_JPX => "jpx",
					   IMAGETYPE_JB2 => "jb2",
					   IMAGETYPE_SWC => "swc",
					   IMAGETYPE_IFF => "iff",
					   IMAGETYPE_WBMP => "wbmp",
					   IMAGETYPE_XBM => "xbm");
		loggit("Image type: " . $types[$imgtype]. "\n");
		$exif = exif_read_data($imgfilename);
		if ($exif === FALSE) {
			loggit("exif_read_data failed for file " . $imgfilename . "\n");
		} else {
			$estr = "EXIF data:\n";
			foreach ($exif as $key => $section) {
				if (is_array($section)) {
					foreach ($section as $name => $val) {
						$estr .= "  $key.$name: $val\n";
					}
				} else {
					$estr .= "  $key: $section\n";
				}
			}
			loggit($estr);

			// eg, FocalLength: 150/10 => 15 mm.
			// FocalLength: 4000/10 => 400 mm.

			// FocalLengthIn35mmFilm
			// IFD		Exif
			// Code		41989 (hex 0xA405)

			echo "<pre>$estr</pre>";
		}
	}
}

?>


