<?php
require_once 'HTML/QuickForm.php';
require_once 'HTML/QuickForm/Renderer/QuickHtml.php';

$maxfilesize = 100*1024*1024;
umask(0002); // in octal

$host  = $_SERVER['HTTP_HOST'];
$myuri  = $_SERVER['PHP_SELF'];
$myuridir = rtrim(dirname($myuri), '/\\');
$debug = 0;

// Set up PEAR error handling.
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

// eg "index.php"
$scriptname = basename($_SERVER['PHP_SELF']);

// debug - print out the values submitted by the client.
loggit($scriptname . " submitted values:\n");
foreach ($_POST as $key => $val) {
	loggit("  \"" . $key . "\" = \"" . $val . "\"\n");
}

// Handle "preset="... by looking up the preset and redirecting
$preset = $_GET["preset"];
if ($preset) {
	$params = $blind_presets[$preset];
	if ($params) {
		// Redirect to this page but with params set.
		header("Location: http://" . $host . $myuri . "?" . $params);
	}
}

// Create the HTML form object...
$form =& new HTML_QuickForm('blindform','post');

// XHTML compliance:
$form->removeAttribute('name');

$form->setDefaults($formDefaults);

$form->addElement('hidden', 'skippreview');
$form->addElement('hidden', 'justjobid');

$xysrc_img =& $form->addElement('radio','xysrc',"img",null,'img');
$xysrc_url =& $form->addElement('radio','xysrc',"url",null,'url');
$xysrc_fits=& $form->addElement('radio','xysrc',"fits",null,'fits');

$fs_ul =& $form->addElement('radio','fstype',null,null,'ul');
$fs_ev =& $form->addElement('radio','fstype',null,null,'ev');

// map element name -> HTML id
$ids = array('xysrc-url-id'  => $xysrc_url->getAttribute('id'),
			 'xysrc-img-id'  => $xysrc_img->getAttribute('id'),
			 'xysrc-fits-id' => $xysrc_fits->getAttribute('id'),
			 'fstype-ul-id'  => $fs_ul->getAttribute('id'),
			 'fstype-ev-id'  => $fs_ev->getAttribute('id'),
			 );

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

$form->addElement('select', 'fsunit', 'units', $unitmap, null);

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
				 "15degree" => "15-degree Fields",
				 "30degree" => "30-degree Fields",
				 );

$form->addElement('text', 'uname', "your name", array('size'=>40));
$form->addElement('text', 'email', "email address", array('size'=>40));

$form->addElement('select', 'index', 'index', $indexes, null);

$form->addElement('submit', 'submit', 'Submit');

$form->addElement('submit', 'linkhere', 'Link to these parameter settings');

//$form->addElement('submit', 'imagescale', 'Try to guess scale from image headers');

$form->addElement('reset', 'reset', "Reset Form");

$form->setMaxFileSize($maxfilesize);

// Add rules that check the form values.
$form->addRule('xysrc', 'You must provide a field to solve!', 'required');
$form->addRule('poserr', 'Star positional error must be a number!', 'numeric');
$form->addRule('fsu', 'Upper bound must be numeric.', 'numeric');
$form->addRule('fsl', 'Lower bound must be numeric.', 'numeric');
$form->addRule('fse', 'Estimate must be numeric.', 'numeric');
$form->addRule('fsv', 'Variance must be numeric.', 'numeric');
$form->addFormRule('check_xysrc');
$form->addFormRule('check_fieldscale');
$form->addFormRule('check_poserr');

if ($emailver) {
	$form->addFormRule('check_email');
}

// Handle "link to here" requests, by formatting an appropriate GET URL and redirecting.
if ($form->exportValue("linkhere")) {
	$vals = $form->exportValues();
	$args = format_preset_url_from_form($vals, $formDefaults);
	header("Location: http://" . $host . $myuri . $args);
	exit;
}

// Handle "guess image scale" requests.
/*
if ($form->exportValue("imagescale")) {
	loggit("Guessing image scale.\n");
	guess_image_scale($form->exportValues());
	exit;
}
*/

// If the form has been filled out correctly (and the Submit button pressed), process it.
if ($form->exportValue("submit") && $form->validate()) {
	$form->freeze();
	$form->process('process_data', false);
    exit;
}

// Else render the form.
render_form($form, $ids, $headers);
exit;


// Handle a valid submitted form.
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
	global $xyls_fn;
	global $xylsinfo_fn;
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
	global $formDefaults;
	global $emailver;
	global $host;
	global $myuridir;

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

	// Stuff the submitted values into the jobdata db.
	$jobdata = array('maxquads' => $maxquads,
					 'cpulimit' => $maxcpu,
					 'timelimit' => $maxtime);
	$flds = array('xysrc', 'imgfile', 'fitsfile', 'imgurl',
				  'x_col', 'y_col', 'fstype', 'parity',
				  'tweak', 'fsl', 'fsu', 'fse', 'fsv', 'fsunit',
				  'poserr', 'index', 'submit');
	foreach ($flds as $fld) {
		$jobdata[$fld] = $vals[$fld];
	}

	// Stuff in optional fields
	$flds = array('uname', 'email');
	foreach ($flds as $fld) {
		if ($vals[$fld])
			$jobdata[$fld] = $vals[$fld];
	}
	if (!setjobdata($db, $jobdata)) {
		die("failed to set job values.");
	}
	loggit("created db " . $dbpath . "\n");
	//loggit("xysrc: " . $xysrc . "\n");

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

	// If we got an image, convert it to PNM & FITS.
	if ($imgfilename) {
		if (!convert_image($imgfilename, $mydir, $suffix,
						   $errstr, $W, $H, $shrink, $dispW, $dispH,
						   $dispimgbase, $dispimgpngbase)) {
			die($errstr);
		}

		// Rename our copy of the image file to reflect the kind of image
		// we think it is (based on what "file" thinks it is)
		$newname = $imgfilename . $suffix;
		if (!rename($imgfilename, $newname)) {
			die("failed to rename img file.");
		}
		loggit("Renamed image file to " . $newname . "\n");
		$imgfilename = $newname;
		$imgbasename .= $suffix;

		// Save all the things we discovered about the img.
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

	$xylist = $mydir . $xyls_fn;
	$xylsinfofile = $mydir . $xylsinfo_fn;

	if ($xysrc == "fits") {
		// If a FITS bintable file was uploaded:
		// -grab the filename it had on the user's machine:
		$fitsval = $fitsfile->getValue();
		$origname = $fitsval["name"];
		if (!setjobdata($db, array("fits-origname"=>$origname))) {
			die("failed to update jobdata: fits-origname");
		}
		// -move it into place...
		if (!$fitsfile->moveUploadedFile($mydir, $xyls_fn)) {
			die("failed to move uploaded FITS file into place.");
		}

		// Try to get the size of the image...
		// -run "xylsinfo", and parse the results.
		$cmd = $xylsinfo . " " . $xylist . " > " . $xylsinfofile;
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
		if (!setjobdata($db, array('xylsW'=>$W, 'xylsH'=>$H))) {
			die("failed to save xyls W,H in database.");
		}
		loggit("found xyls width and height " . $W . ", " . $H . "\n");
	}

	// FIXME - do we need to do this??
	if (!chmod($xylist, 0664)) {
		die("Failed to chmod xylist " . $xylist);
	}

	$inputfile = $mydir . $input_fn;
	$inputtmpfile = $mydir . $inputtmp_fn;
	$donescript = $mydir . $donescript_fn;
	$donescript_relpath = "./" . $donescript_fn;
	$donefile = $mydir . $done_fn;

	$index = $vals["index"];
	$x_col = $vals["x_col"];
	$y_col = $vals["y_col"];
	$parity = $vals["parity"];
	$tweak = $vals["tweak"];
	$poserr = $vals["poserr"];

	$codetol = 0.01;

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

	loggit("fsu $fsu, fsl $fsl, fse $fse, fsv $fsv\n");

	if ($fstype == "ev") {
		// Estimate/Variance given - convert to lower/upper.
		// FIXME - is this the right way to do this?
		$fsu = $fse * (1 + $fsv/100);
		$fsl = $fse * (1 - $fsv/100);
	}

	// Convert field scales given in other units into arcsec/pix.
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
		$sizemap = array("30degree" => array(400,900),
						 "15degree" => array(200,400),
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
		$largest = "30degree";
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
					  "30degree" => array("allsky-43/allsky-43"),
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

	foreach ($indexes as $ind) {
		fprintf($fin, "index " . $indexdir . $ind . "\n");
	}
	$str = "start " . $start_fn . "\n" .
		"donescript " . $donescript_relpath . "\n" .
		"field " . $xyls_fn . "\n" .
		"solved " . $solved_fn . "\n" .
		"cancel " . $cancel_fn . "\n" .
		"match " . $match_fn . "\n" .
		"wcs " . $wcs_fn . "\n" .
		"indexrdls " . $indexrdls_fn . "\n" .
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
		"nverify 20\n" .
		"nindex_tokeep 25\n" .
		"nindex_tosolve 25\n" .
		"distractors 0.25\n" .
		"ratio_toprint 10\n" .
		"ratio_tokeep 10\n" .
		"ratio_tosolve 1e6\n" .
		"ratio_tobail -1e100\n" .
		"fieldw " . $W . "\n" .
		"fieldh " . $H . "\n" .
		"verbose\n" .
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
			"if [ `" . $printsolved . " " . $solved_fn . " | grep -v \"File\" | wc -w` -eq 1 ]; then \n" .
			"  echo \"Field solved.\";\n" .
			"  echo Running wcs-xy2rd...;\n" .
			"  " . $wcs_xy2rd . " -w " . $wcs_fn . " -i " . $xyls_fn . " -o " . $rdls_fn . " -X " . $x_col . " -Y " . $y_col . ";\n" .
			"  echo Running rdlsinfo...;\n" .
			"  " . $rdlsinfo . " " . $rdls_fn . " > " . $rdlsinfo_fn . ";\n" .
			"  echo Merging index rdls file...\n" .
			"  cp " . $indexrdls_fn . " " . $indexrdls_fn . ".orig\n" .
			"  cp " . $indexrdls_fn . " " . $indexrdls_fn . ".tmp\n" .
			"  for ((i=2; i<=" . count($indexes) . "; i++)); do\n" .
			"    " . $tabmerge . " " . $indexrdls_fn . "+\$i " . $indexrdls_fn . ".tmp+1\n" .
			"  done\n" .
			"  mv " . $indexrdls_fn . ".tmp " . $indexrdls_fn . "\n" .
			"  " . $wcs_rd2xy . " -w " . $wcs_fn . " -i " . $indexrdls_fn . " -o " . $indexxyls_fn . ";\n" .
			#"  echo Adding image size to WCS file...\n" .
			#"  " . $modhead . " " . $wcsfile . " IMAGEW " . $W . "\n" .
			#"  " . $modhead . " " . $wcsfile . " IMAGEH " . $H . "\n" .
			"else\n" .
			"  echo \"Field did not solve.\";\n" .
			"fi\n"
			);
	if ($emailver) {
		fprintf($fdone,
				"echo \"Sending email...\";\n" .
				"wget -q \"http://" . $host . $myuridir . "/status.php?job=" . $myname . "&email\" -O - > send-email-wget.out 2>&1\n" .
				"echo Donescript finished.\n"
				);
	}

	if (!fclose($fdone)) {
		die("Failed to close donescript " . $donescript);
	}
	if (!chmod($donescript, 0775)) {
		die("Failed to chmod donescript " . $donescript);
	}

	after_submitted($imgfilename, $myname, $mydir, $vals, $db);
}

// Form parameter value checking.
function check_email($vals) {
	$email = $vals['email'];
	if (!$email) {
		return array('email'=>'You must enter a valid email address.');
	}
	if (!is_valid_email_address($email)) {
		return array('email'=>'A valid email address is required.');
	}
	return TRUE;
}

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
	} else if ($type == "ev") {
		if ($e < 0)
			return array("fse"=>"Estimate must be positive!");
		if ($v < 1 || $v > 99)
			return array("fsv"=>"% Eror must be between 1 and 99!");
		return TRUE;
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

// Form rendering
function render_form($form, $ids, $headers) {
	global $index_template;
	global $index_header;
	global $index_tail;
	global $valid_blurb;
	global $emailver;
	global $webver;

	$maxfilesize = $form->getMaxFileSize();
	$renderer =& new HTML_QuickForm_Renderer_QuickHtml();
	$form->accept($renderer);

	// Load the index-template file and do text replacement on it.
	$template = file_get_contents($index_template);

	// all the "regular" fields.
	$flds = array('imgfile', 'fitsfile', 'imgurl', 'x_col', 'y_col',
				  'tweak', 'fsl', 'fsu', 'fse', 'fsv', 'fsunit',
				  'poserr', 'index', 'uname', 'email',
				  'submit', 'linkhere', 
				  #'imagescale',
				  'reset', 'MAX_FILE_SIZE');
	
	if ($emailver) {
		$template = str_replace('##email-caption##',
								"We need your email address to send you your results.",
								$template);
	} else if ($webver) {
		$template = str_replace('##email-caption##',
								"These fields are optional.",
								$template);
	}
	foreach ($flds as $fld) {
		$template = str_replace("##".$fld."##", $renderer->elementToHtml($fld), $template);
	}

	// Replace -id tokens.
	foreach ($ids as $fld=>$id) {
		$template = str_replace("##" . $fld . "##",  $id,  $template);
	}

	// fields (and pseudo-fields) that can have errors 
	$errflds = array('xysrc', 'imgfile', 'imgurl', 'fitsfile',
					 'x_col', 'y_col', 'fstype', 'fsl', 'fsu', 'fse', 'fsv',
					 'poserr', 'fs', 'email');
	foreach ($errflds as $fld) {
		$template = str_replace("##".$fld."-err##", $form->getElementError($fld), $template);
	}

	// Radio buttons
	$repl = array("##xysrc-img##"    => $renderer->elementToHtml('xysrc', 'img'),
				  "##xysrc-url##"    => $renderer->elementToHtml('xysrc', 'url'),
				  "##xysrc-fits##"   => $renderer->elementToHtml('xysrc', 'fits'),
				  "##parity-both##"  => $renderer->elementToHtml('parity', '2'),
				  "##parity-left##"  => $renderer->elementToHtml('parity', '1'),
				  "##parity-right##" => $renderer->elementToHtml('parity', '0'),
				  "##fstype-ul##"    => $renderer->elementToHtml('fstype', 'ul'),
				  "##fstype-ev##"    => $renderer->elementToHtml('fstype', 'ev'),
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

	// Write the HTML header.
	$head = file_get_contents($index_header);
	echo $head;

	// Render the form.
	echo $renderer->toHtml($template);

	$tail = file_get_contents($index_tail);
	$tail = str_replace('##valid-blurb##', $valid_blurb, $tail);
	$tail = str_replace('##sizelimit##',   sprintf("%d", $maxfilesize/(1024*1024)), $tail);
	echo $tail;

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
	global $fits_filter;

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

			if ($suff == "fits") {
				$filtered_fits = $mydir . "filtered.fits";
				$outfile = $mydir . "fits2fits.out";
				$cmd = sprintf($fits_filter, $filename, $filtered_fits) . " > " . $outfile . " 2>&1";
				loggit("Command: " . $cmd . "\n");
				if ((system($cmd, $retval) === FALSE) || $retval) {
					loggit("Command failed, return value " . $retval . ": " . $cmd . "\n");
					die("failed to fix your FITS file: <pre>" . file_get_contents($outfile) . "</pre>");
				}
				$filename = $filtered_fits;
			}

			$outfile = $mydir . "Xtopnm.out";
			$cmd = sprintf($command, $filename, $pnmimg) . " 2> " . $outfile;
			loggit("Command: " . $cmd . "\n");
			if ((system($cmd, $retval) === FALSE) || $retval) {
				loggit("Command failed, return value " . $retval . ": " . $cmd . "\n");
				die("failed to convert image: <pre>" . file_get_contents($outfile) . "</pre>");
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
	loggit("found image type " . $imgtype . "\n");

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
		$fitsimg = $filename;
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




?>
