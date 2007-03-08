<?php
require_once 'MDB2.php';
require_once 'HTML/QuickForm.php';
require_once 'HTML/QuickForm/Renderer/QuickHtml.php';
require_once 'PEAR.php';

require 'common.php';

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

/***    Describe the HTML form.   ***/

$form =& new HTML_QuickForm('blindform','post');

$formDefaults = array('x_col' => 'X',
					  'y_col' => 'Y',
					  'parity' => 2,
					  'index' => 'auto',
					  'poserr' => 1.0,
					  //'tweak' => TRUE,
					  'tweak' => 1,
					  'imgurl' => "http://",
					  'fsunit' => 'degreewidth',
					  );
$form->setDefaults($formDefaults);

$form->addElement('radio','xysrc',"img",null,'img');
$form->addElement('radio','xysrc',"url",null,'url');
$form->addElement('radio','xysrc',"fits",null,'fits');

$imgfile  =& $form->addElement('file', 'imgfile', "imgfile");
$fitsfile =& $form->addElement('file', 'fitsfile', "fitsfile");
$form->addElement('text', 'x_col', "x column name", array('size'=>5));
$form->addElement('text', 'y_col', "y column name", array('size'=>5));
$form->addElement('text', 'imgurl', "imgurl", array('size' => 40));

$form->addElement('checkbox', 'tweak', 'tweak', null, null);

$form->addElement('text', 'fsl', 'field scale (lower bound)', array('size'=>5));
$form->addElement('text', 'fsu', 'field scale (upper bound)', array('size'=>5));
$form->addElement('text', 'fse', 'field scale (estimate)', array('size'=>5));
$form->addElement('text', 'fsv', 'field scale (variance (%))', array('size'=>5));

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

$form->addElement('select', 'index', 'index', $indexes, null);

$form->addElement('submit', 'submit', 'Submit');

$form->addElement('submit', 'linkhere', 'Link to these parameter settings');

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

//loggit("linkhere: " . $form->exportValue("linkhere") . "\n");
if ($form->exportValue("linkhere")) {
	$host  = $_SERVER['HTTP_HOST'];
	$uri  = $_SERVER['PHP_SELF'];
	$uri .= "?";
	$vals = $form->exportValues();
	//loggit("xysrc: " . $vals["xysrc"] . "\n");
	$args = "";
	$flds = array('xysrc', 'fsl', 'fsu', 'fse', 'fsv',
				  'fsunit', 'parity', 'poserr', 'index');
	switch ($vals["xysrc"]) {
	case "url":
		array_push($flds, "imgurl");
		break;
	case "img":
		array_push($flds, "imgfile");
		break;
	case "fits":
		array_push($flds, "fitsfile");
		array_push($flds, "x_col");
		array_push($flds, "y_col");
		break;
	}
	foreach ($flds as $fld) {
		//loggit("link-here: field \"$fld\", val \"$vals[$fld]\", default \"$formDefaults[$fld]\"\n");
		if ($vals[$fld] != $formDefaults[$fld])
			$args .= "&" . urlencode($fld) . "=" . urlencode($vals[$fld]);
	}
	if ($vals["tweak"]) {
		$args .= "&tweak=1";
	}

	$uri .= substr($args, 1);

	header("Location: http://" . $host . $uri);
	exit;
}

/** If the form has been filled out correctly, then process it. */

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
// could also do a heredoc:
// $template = <<<END
// ...
// END;

// all the "regular" fields.
$flds = array('imgfile', 'fitsfile', 'imgurl', 'x_col', 'y_col',
			  'tweak', 'fsl', 'fsu', 'fse', 'fsv', 'fsunit',
			  'poserr', 'index', 'submit', 'linkhere');
foreach ($flds as $fld) {
	$template = str_replace("##".$fld."##", $renderer->elementToHtml($fld), $template);
}

// fields (and pseudo-fields) that can have errors 
$errflds = array('xysrc', 'imgfile', 'imgurl', 'fitsfile',
				 'x_col', 'y_col', 'fsl', 'fsu', 'fse', 'fsv',
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
			  );
foreach ($repl as $from => $to) {
	$template = str_replace($from, $to, $template);
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
	$l = (double)$vals["fsl"];
	$u = (double)$vals["fsu"];
	$e = (double)$vals["fse"];
	$v = (double)$vals["fsv"];
	/*
	echo $l . "<br>";
	echo $u . "<br>";
	echo $e . "<br>";
	echo $v . "<br>";
	*/
	if ($l > 0 && $u > 0)
		return TRUE;
	if ($e > 0 && $v > 0)
		return TRUE;
	if (!$l && !$u && !$e && !$v)
		//return array("fs"=>"You must provide either (lower and upper bounds) or (estimate and variance) of the field scale!");
		return array("fs"=>"You must fill in ONE of the pairs of boxes below!");
	if ($l < 0)
		return array("fsl"=>"Lower bound must be positive!");
	if ($u < 0)
		return array("fsu"=>"Upper bound must be positive!");
	if ($e < 0)
		return array("fse"=>"Estimate must be positive!");
	if ($v < 1 || $v > 99)
		return array("fsv"=>"% Eror must be between 1 and 99!");
	if (($l && !$u) || ($u && !$l)) {
		return array("fsu"=>"You must give BOTH upper and lower bounds.");
	}
	if (($e && !$v) || ($v && !$e)) {
		return array("fsu"=>"You must give BOTH an estimate and error.");
	}
	return TRUE;
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
	global $rdls_fn;
	global $rdlsinfo_fn;
	global $match_fn;
	global $solved_fn;
	global $wcs_fn;
	global $start_fn;
	global $done_fn;
	global $log_fn;

	$xysrc = $vals["xysrc"];
	$imgurl = $vals["imgurl"];

	// Create a directory for this request.
	$myname = create_random_dir($resultdir);
	if (!$myname) {
		die("Failed to create job directory.");
	}
	$mydir = $resultdir . $myname . "/";

	// Create a database...
	$dbpath = $mydir . $dbfile;
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
	/*
				  "fu_lower" => $fu_lower,
				  "fu_upper" => $fu_upper,
				  "verify" => $verify,
				  "agree" => $agree,
				  "nagree" => $nagree,
				  "tweak" => $tweak_val,
				  "parity" => $parity_val,
				  "codetol" => $codetol,
	*/

	$flds = array('xysrc', 'imgfile', 'fitsfile', 'imgurl',
				  'x_col', 'y_col',
				  'tweak', 'fsl', 'fsu', 'fse', 'fsv', 'fsunit',
				  'poserr', 'index', 'submit');

	foreach ($flds as $fld) {
		$jobdata[$fld] = $vals[$fld];
	}

	if (!setjobdata($db, $jobdata)) {
		die("failed to set job values.");
	}
	loggit("created db " . $dbpath . "\n");

	loggit("xysrc: " . $xysrc . "\n");

	$imgfilename = "";
	if ($xysrc == "url") {
		// Try to retrieve the URL...
		loggit("retrieving url " . $imgurl . " ...\n");
		$usetype = get_image_type($imgurl, $xtopnm);
		if (!$usetype) {
			loggit("unknown image type.\n");
			die("Unknown image type.");
		}
		loggit("Assuming image type: " . $usetype . "\n");

		$downloadedimg = $mydir . "downloaded." . $usetype;
		loggit("Writing to file: " . $downloadedimg . "\n");

		$fin = fopen($imgurl, "rb");
		if (!$fin) {
			die("failed to open URL " . $imgurl);
		}
		$fout = fopen($downloadedimg, "wb");
		if (!$fout) {
			die("failed to open output file " . $downloadedimg);
		}

		$nr = 0;
		$blocksize = 1024;
		for ($i=0; $i<$maxfilesize/$blocksize; $i++) {
			$block = fread($fin, $blocksize);
			if ($block === FALSE) {
				die("failed to read from URL " . $imgurl);
			}
			if (fwrite($fout, $block) === FALSE) {
				die("failed to write to output file " . $downloadedimg);
			}

			$nr += strlen($block);

			if (strlen($block) < $blocksize)
				if (feof($fin)) {
					break;
				}
		}
		if ($i == $maxfilesize/$blocksize) {
			die("URL was too big to download.\n");
		}
		fclose($fin);
		fclose($fout);
		$imgfilename = $downloadedimg;

	} else if ($xysrc == "img") {
		// If an image was uploaded, move it into place.
		// The filename on the user's machine:
		$imgval = $imgfile->getValue();
		$origname = $imgval["name"];
		$usetype = get_image_type($origname, $xtopnm);
		if (!$usetype) {
			loggit("unknown image file type.\n");
			$errstr = "Unknown image file type.";
			return FALSE;
		}
		loggit("original image file: " . $origname . ", using type: " . $usetype . "\n");

		$imgfilename = $mydir . "uploaded." . $usetype;
		if (!$imgfile->moveUploadedFile($mydir, "uploaded." . $usetype)) {
			die("failed to move uploaded file into place.");
		}
	}



	if ($imgfilename) {
		if (!convert_image($imgfilename, $mydir, $xtopnm,
						   $errstr, $W, $H)) {
			die($errstr);
		}

		if (!setjobdata($db, array("imageW"=>$W, "imageH"=>$H))) {
			die("failed to save image W,H in database.");
		}

		$imgtype = exif_imagetype($imgfilename);
		if ($imgtype === FALSE) {
			loggit("Failed to run exif_imagetype on image " . $imgfilename . "\n");
		} else {
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
				loggit("EXIF data:\n");
				foreach ($exif as $key => $section) {
					if (is_array($section)) {
						foreach ($section as $name => $val) {
							loggit("  $key.$name: $val\n");
						}
					} else {
						loggit("  $key: $section\n");
					}
				}
			}
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
	$rdlsinfofile = $mydir . $rdlsinfo_fn;
	$matchfile = $mydir . $match_fn;
	$solvedfile = $mydir . $solved_fn;
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

	if ($imgfilename) {
		// Write to "input.tmp" instead of "input", so we don't trigger
		// the solver just yet...
		$inputfile = $inputtmpfile;
	}

	// MAJOR HACK - pause to let the watcher notice that the
	// new directory was created.
	sleep(1);

	// Write the donescript for blind...
	$fdone = fopen($donescript, "w");
	if (!$fdone) {
		die("Failed to write donescript " . $donescript);
	}
	fprintf($fdone,
			"#! /bin/bash\n" .
			"touch " . $donefile . "\n" .
			"if [ `" . $printsolved . " " . $solvedfile . " | grep -v \"File\" | wc -w` -eq 1 ]; then \n" .
			"  echo \"Field solved.\";\n" .
			"  echo Running wcs-xy2rd...;\n" .
			"  " . $wcs_xy2rd . " -w " . $wcsfile . " -i " . $xylist . ".fits" . " -o " . $rdlist . " -X " . $x_col . " -Y " . $y_col . ";\n" .
			"  echo Running rdlsinfo...;\n" .
			"  " . $rdlsinfo . " " . $rdlist . " > " . $rdlsinfofile . ";\n" .
			"else\n" .
			"  echo \"Field did not solve.\";\n" .
			"fi\n"
			);
	if (!fclose($fdone)) {
		die("Failed to close donescript " . $donescript);
	}
	if (!chmod($donescript, 0775)) {
		die("Failed to chmod donescript " . $donescript);
	}

	// Write the input file for blind...
	$fin = fopen($inputfile, "w");
	if (!$fin) {
		die("Failed to write input file " . $inputfile);
	}

	$fsunit = $vals["fsunit"];
	$fsu = $vals["fsu"];
	$fsl = $vals["fsl"];
	$fse = $vals["fse"];
	$fsv = $vals["fsv"];

	// Estimate/Variance given - convert to lower/upper.
	if ($fse > 0 && $fsv > 0) {
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

	// FIXME - AUTO-SELECT INDEX!
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
					  "15degree" => array("allsky-36/allsky-36"),
					  //"15degree" => array("allsky-39/allsky-39"),
					  );
	$indexpaths = array();
	foreach ($indexes as $i) {
		$indexpaths = array_merge($indexpaths, $indexmap[$i]);
	}
	$indexes = $indexpaths;

	fprintf($fin, "log " . $logfile . "\n");
	foreach ($indexes as $ind) {
		fprintf($fin, "index " . $indexdir . $ind . "\n");
	}
	$str = "start " . $startfile . "\n" .
		"donescript " . $donescript . "\n" .
		"field " . $xylist . "\n" .
		"solved " . $solvedfile . "\n" .
		"match " . $matchfile . "\n" .
		"wcs " . $wcsfile . "\n" .
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
		"ninfield_tokeep 50\n" .
		"ninfield_tosolve 50\n" .
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


	// Redirect the client to the status page...
	$status_url = "status.php?job=" . $myname;
	if ($imgfilename) {
		$status_url .= "&img";
	}
	$host  = $_SERVER['HTTP_HOST'];
	$uri  = rtrim(dirname($_SERVER['PHP_SELF']), '/\\');
	header("Location: http://" . $host . $uri . "/" . $status_url);
	exit;
}

function get_image_type($filename, &$xtopnm) {
	// Xtopnm formats we accept:
	// normalize filenames
	$imgtypemap = array("jpg" => "jpeg",
						"jpeg" => "jpeg",
						"png" => "png",
						"fits" => "fits",
						"gif" => "gif",
						"ppm" => "ppm",
						);

	$xtopnmmap = array("jpeg" => "jpegtopnm",
					   "png" => "pngtopnm",
					   "fits" => "fitstopnm",
					   "gif" => "giftopnm",
					   "ppm" => "ppmtoppm <", // hack - ppmtoppm takes input from stdin.
					   );

	// try to figure out what kind of file it is...
	$usetype = "";
	foreach ($imgtypemap as $ending => $type) {
		if (!strcasecmp(substr($filename, -strlen($ending)), $ending)) {
			$usetype = $type;
			$xtopnm = $xtopnmmap[$type];
			break;
		}
	}
	if (!strlen($usetype)) {
		return FALSE;
	}
	return $usetype;
}

function convert_image($img, $mydir, $xtopnm, &$errstr, &$W, &$H) {
	global $fits2xy;
	global $modhead;
	global $plotxy;
	global $tabsort;
	global $objs_fn;

	loggit("image file: " . filesize($img) . " bytes.\n");

	$pnmimg = $mydir . "image.pnm";
	$pnmimg_orig = $pnmimg;
	$cmd = $xtopnm . " " . $img . " > " . $pnmimg;
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

	// Get the image size:
	$cmd = $modhead . " " . $fitsimg . " NAXIS1 | awk '{print $3}'";
	loggit("Command: " . $cmd . "\n");
	$W = (int)rtrim(shell_exec($cmd));
	loggit("naxis1 = " . $W . "\n");

	$cmd = $modhead . " " . $fitsimg . " NAXIS2 | awk '{print $3}'";
	loggit("Command: " . $cmd . "\n");
	$H = (int)rtrim(shell_exec($cmd));
	loggit("naxis2 = " . $H . "\n");

	// Plot the extracted objects.
	$Nbright = 100;
	// -the brightest:
	$objimg1 = $mydir . "objs1.pgm";
	$cmd = $plotxy . " -i " . $xylist . " -W " . $W . " -H " . $H .
		" -x 1 -y 1 " . "-N " . $Nbright . " > " . $objimg1;
	loggit("Command: " . $cmd . "\n");
	$res = FALSE;
	$res = system($cmd, $retval);
	if ($retval) {
		loggit("Command failed: return val " . $retval . ", str " . $res . "\n");
		$errstr = "Failed to plot extracted sources.";
		return FALSE;
	}
	// -the rest:
	$objimg2 = $mydir . "objs2.pgm";
	$cmd = $plotxy . " -i " . $xylist . " -W " . $W . " -H " . $H .
		" -x 1 -y 1" . " -n " . $Nbright . " -r 3" . " > " . $objimg2;
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



?>


