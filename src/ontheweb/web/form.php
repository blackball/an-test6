<?php
require_once 'HTML/QuickForm.php';
require_once 'HTML/QuickForm/Renderer/QuickHtml.php';
require_once 'svn.php';

/* Note, if you want to change the maximum uploaded file size you must also change:
  -php.ini: post_max_size
  -php.ini: upload_max_filesize
*/
$maxfilesize = 250*1024*1024;
umask(0002); // in octal

$host  = $_SERVER['HTTP_HOST'];
$myuri  = $_SERVER['PHP_SELF'];
$myuridir = rtrim(dirname($myuri), '/\\');
$debug = 0;

$sdss46paths = array();
for ($i=0; $i<12; $i++)
	 array_push($sdss46paths, sprintf('sdss-46/sdss-46-%02d', $i));

$indexdata =
array('60degree' => array('desc' => '60-degree Fields',
						  'quadsize' => array(900, 1800),
						  'paths' => array('allsky-44/allsky-44')),
	  '30degree' => array('desc' => '30-degree Fields',
						  'quadsize' => array(400, 900),
						  'paths' => array('allsky-43/allsky-43')),
	  '15degree' => array('desc' => '15-degree Fields',
						  'quadsize' => array(200, 400),
						  'paths' => array('allsky-40/allsky-40')),
	  '8degree' => array('desc' => '8-degree Fields',
						 'quadsize' => array(120, 180),
						 'paths' => array('allsky-38/allsky-38')),
	  '4degree' => array('desc' => '4-degree Fields',
						 'quadsize' => array(60, 90),
						 'paths' => array('allsky-34/allsky-34')),
	  '2degree' => array('desc' => '2-degree Fields',
						 'quadsize' => array(32, 40),
						 'paths' => array('allsky-37/allsky-37')),
	  '1degree' => array('desc' => '1-degree Fields',
						 'quadsize' => array(16, 20),
						 'paths' => array('allsky-32/allsky-32')),
	  '30arcmin' => array('desc' => '30-arcminute Fields',
						  'quadsize' => array(8, 10),
						  'paths' => array('allsky-31/allsky-31')),
	  '12arcmin' => array('desc' => '12-arcmin Fields (eg, Sloan Digital Sky Survey)',
						  'quadsize' => array(4, 5),
						  //'paths' => array('sdss-23/sdss-23-allsky')),
						  'paths' => $sdss46paths),
	  );

$largest_index = '60degree';
$smallest_index = '12arcmin';

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
	$params = get_preset($preset);
	if ($params) {
		// Redirect to this page but with params set.
		header("Location: http://" . $host . $myuri . $params);
	}
}

// a la the suggestion at http://ca.php.net/manual/en/function.uniqid.php:
$upload_id = md5(uniqid(rand(), TRUE));

// Create the HTML form object...
$form =& new HTML_QuickForm('blindform','post', '', '');

// XHTML compliance:
$form->removeAttribute('name');

$form->setDefaults($formDefaults);

$upl =& $form->addElement('hidden', 'UPLOAD_IDENTIFIER');
$upl->setValue($upload_id);

$form->addElement('hidden', 'skippreview');
$form->addElement('hidden', 'justjobid');

$xysrc_img =& $form->addElement('radio','xysrc',"img",null,'img');
$xysrc_url =& $form->addElement('radio','xysrc',"url",null,'url');
$xysrc_fits=& $form->addElement('radio','xysrc',"fits",null,'fits');

$form->setAttribute('onSubmit', 'setTimeout(\'showUploadMeter()\', 3000)');

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
$form->addElement('text', 'tweak_order', 'tweak_order',
				  array('size'=>5), null);

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

// map element name -> HTML id
$ids = array('xysrc-url-id'  => $xysrc_url->getAttribute('id'),
			 'xysrc-img-id'  => $xysrc_img->getAttribute('id'),
			 'xysrc-fits-id' => $xysrc_fits->getAttribute('id'),
			 'fstype-ul-id'  => $fs_ul->getAttribute('id'),
			 'fstype-ev-id'  => $fs_ev->getAttribute('id'),
			 );

$form->addElement('select', 'fsunit', 'units', $unitmap, null);

$form->addElement('radio', 'parity', "Try both parities", null, 2);
$form->addElement('radio', 'parity', "Positive parity image", null, 0);
$form->addElement('radio', 'parity', "Negative parity image", null, 1);

$form->addElement('text', 'poserr', "star positional error (pixels)", array('size' => 5));

// Pull out the descriptions of the indexes.
$indexes['auto'] = 'Automatic (based on image scale)';
foreach ($indexdata as $k => $v) {
	$indexes[$k] = $v['desc'];
}

$form->addElement('text', 'uname', "your name", array('size'=>40));
$form->addElement('text', 'email', "email address", array('size'=>40));

$form->addElement('select', 'index', 'index', $indexes, null);

$form->addElement('submit', 'submit', 'Submit');

$form->addElement('submit', 'linkhere', 'Link to these parameter settings');

//$form->addElement('submit', 'imagescale', 'Try to guess scale from image headers');

$form->addElement('submit', 'reset', "Reset Form");

$form->setMaxFileSize($maxfilesize);

// Add rules that check the form values.
$form->addRule('xysrc', 'You must provide a field to solve!', 'required');
$form->addRule('poserr', 'Star positional error must be a number!', 'numeric');
$form->addRule('fsu', 'Upper bound must be numeric.', 'numeric');
$form->addRule('fsl', 'Lower bound must be numeric.', 'numeric');
$form->addRule('fse', 'Estimate must be numeric.', 'numeric');
$form->addRule('fsv', 'Variance must be numeric.', 'numeric');
$form->addRule('tweak_order', 'Tweak order must be numeric.', 'numeric');
$form->addFormRule('check_xycol');
$form->addFormRule('check_xysrc');
$form->addFormRule('check_fieldscale');
$form->addFormRule('check_poserr');
$form->addFormRule('check_tweak');

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

// Handle "reset" button.  (We can't just use a normal HTML reset button because if
// the URL has GET params, these become the form defaults and a normal Reset button
// just resets to those values.)
if ($form->exportValue("reset")) {
	// come back with no args!
	header("Location: http://" . $host . $myuri);
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

function submit_failed($jobdb, $msg) {
	if (!setjobdata($jobdb, array('submit-failure' => $msg))) {
		loggit("submit_failed: failed to save submit-failure in jobdb.\n");
	}
	loggit("Submit failed: " . $msg . "\n");
	die("Submit failed: " . $msg);
}

// Handle a valid submitted form.
function process_data ($vals) {
	global $form;
    global $imgfile;
    global $fitsfile;
	global $resultdir;
	global $dbfile;
	global $maxfilesize;
	global $totaltime;
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
	global $startscript_fn;
	global $indexxyls_fn;
	global $xyls_fn;
	global $xylsinfo_fn;
	global $rdls_fn;
	global $rdlsinfo_fn;
	global $match_fn;
	global $match_pat;
	global $indexrdls_fn;
	global $indexrdls_pat;
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
	global $fitsgetext;
	global $fitscopy;
	global $tabsort;
	global $ontheweblogfile;
	global $indexdata;
	global $largest_index;
	global $smallest_index;
	global $sqlite;

	$xysrc = $vals['xysrc'];
	$imgurl = $vals['imgurl'];

	if ($emailver && !allow_email($vals['email'])) {
		die("Sorry, the email address you entered (" .
			$vals['email'] . ") is not authorized.  Please contact " .
			'<a href="alpha@astrometry.net">alpha@astrometry.net</a> to be added ' .
			'to our alpha testers list.');
	}

	// Create a directory for this request.
	$myname = create_new_jobid();
	if (!$myname) {
		die("Failed to create job directory.");
	}
	// relative directory
	$myreldir = jobid_to_dir($myname) . "/";
	// full directory path
	$mydir = $resultdir . $myreldir;

	$mylogfile = $mydir . "loggit";
	loggit("Logging to " . $mylogfile . "\n");
	$ontheweblogfile = $mylogfile;

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

	// Put the submitted values into the jobdata db.
	$jobdata = array('maxquads' => $maxquads,
					 'cpulimit' => $maxcpu,
					 'timelimit' => $maxtime);
	$flds = array('xysrc', 'imgfile', 'fitsfile', 'imgurl',
				  'x_col', 'y_col', 'fstype', 'parity',
				  'tweak', 'fsl', 'fsu', 'fse', 'fsv', 'fsunit',
				  'poserr', 'index', 'submit', 'tweak_order');
	foreach ($flds as $fld) {
		$jobdata[$fld] = $vals[$fld];
	}

	// Add optional fields
	$flds = array('uname', 'email');
	foreach ($flds as $fld) {
		if ($vals[$fld])
			$jobdata[$fld] = $vals[$fld];
	}

	// Add filenames on the user's machine of uploaded files.
	if ($xysrc == 'img') {
		$imgval = $imgfile->getValue();
		$origname = $imgval['name'];
		$jobdata['image-origname'] = $origname;
	} else if ($xysrc == 'fits') {
		$fitsval = $fitsfile->getValue();
		$origname = $fitsval['name'];
		$jobdata['fits-origname'] = $origname;
	}

	// Original submission date.
	$jobdata['submit-date'] = get_datestr(time());

	// Original subversion rev.
	$jobdata['submit-svn-rev'] = get_svn_rev();
	$jobdata['submit-svn-url'] = get_svn_url();
	$jobdata['submit-svn-date'] = get_svn_date();

	if (!setjobdata($db, $jobdata)) {
		submit_failed($db, "Failed to save job parameters.");
	}

	$msg = "Job " . $myname . ": received:\n";
	$desc = describe_job($jobdata, TRUE);
	foreach ($desc as $k => $v) {
		$msg .= "  " . $k . ": " . $v . "\n";
	}
	highlevellog($msg);

	$imgfilename = "";
	$imgbasename = "";

	// Pixel scales to try.
	$tryscales = array();

	if ($xysrc == "url") {
		// fix double "http://"
		if (strstr($imgurl, 'http://http://')) {
			$imgurl = substr($imgurl, 7);
			$vals['imgurl'] = $imgurl;
		}

		// Try to retrieve the URL...
		loggit("retrieving url " . $imgurl . " ...\n");
		$imgbasename = "downloaded.";
		$downloadedimg = $mydir . $imgbasename;
		$imgfilename = $downloadedimg;
		loggit("Writing to file: " . $downloadedimg . "\n");
		if (download_url($imgurl, $downloadedimg, $maxfilesize, $errmsg,
						 $vals['UPLOAD_IDENTIFIER']) === FALSE) {
			submit_failed($db, "Failed to download image: " . $errmsg);
		}
	} else if ($xysrc == "img") {
		// If an image was uploaded, move it into place.
		$imgbasename = "uploaded.";
		$imgfilename = $mydir . $imgbasename;
		if (!$imgfile->moveUploadedFile($mydir, $imgbasename)) {
			submit_failed($db, "Failed to move uploaded image into place");
		}

	}

	// If we got an image, convert it to PNM & FITS.
	$scaleguess = array();
	if ($imgfilename) {
		if (!convert_image($imgbasename, $mydir, $errstr, $W, $H, $db, $scaleguess)) {
			submit_failed($db, "Failed to run source extraction: " . $errstr);
			die($errstr);
		}
		$imgfilename = $mydir . $imgbasename;

		loggit("Got " . count($scaleguess) . " guesses about the image scale.\n");
		foreach ($scaleguess as $method => $scale) {
			loggit("  " . $method . ": " . $scale . "\n");
		}
		$uscales = array_unique(array_values($scaleguess));
		// FIXME - merge overlapping scales...
		foreach ($uscales as $s) {
			array_push($tryscales, array($s * 0.95, $s * 1.05));
		}
	}

	$xylist = $mydir . $xyls_fn;
	$xylsinfofile = $mydir . $xylsinfo_fn;

	if ($xysrc == "fits") {
		// If a FITS bintable file was uploaded, move it into place...
		$uploaded_fn = "uploaded.fits";
		$uploaded = $mydir . $uploaded_fn;
		if (!$fitsfile->moveUploadedFile($mydir, $uploaded_fn)) {
			submit_failed($db, "Failed to move uploaded FITS file into place.");
		}

		// make original table read-only.
		if (!chmod($uploaded, 0440)) {
			loggit("Failed to chmod FITS table " . $uploaded . "\n");
		}

		// use "fitscopy" to grab the first extension and rename the
		// columns from whatever they were to X,Y.
		$cmd = $fitscopy . " " . $uploaded . "\"[1][col X=" . $jobdata['x_col'] .
			";Y=" . $jobdata['y_col'] . "]\" " . $mydir . $xyls_fn . " 2> " . $mydir . "fitscopy.err";
		loggit("Command: " . $cmd . "\n");
		$res = system($cmd, $retval);
		if ($retval) {
			loggit("Command failed: return val " . $retval . ", str " . $res . "\n");
			submit_failed($db, "Failed to extract the pixel coordinate columns from your FITS file.");
		}

		// Try to get the size of the image...
		// -run "xylsinfo", and parse the results.
		$cmd = $xylsinfo . " " . $xylist . " > " . $xylsinfofile;
		$res = system($cmd, $retval);
		if ($retval) {
			loggit("Command failed: return val " . $retval . ", str " . $res . "\n");
			submit_failed($db, "Failed to find the bounds of your field.");
		}
		if (!file_exists($xylsinfofile)) {
			loggit("xylsinfo file " . $xylsinfofile . " does not exist.");
			submit_failed($db, "Failed to find the bounds of your field (2).");
		}
		$info = file($xylsinfofile);
		foreach ($info as $str) {
			$words = explode(" ", rtrim($str, "\n"));
			$infomap[$words[0]] = implode(" ", array_slice($words, 1));
		}
		$W = $infomap["width"];
		$H = $infomap["height"];
		if (!setjobdata($db, array('xylsW'=>$W, 'xylsH'=>$H))) {
			submit_failed($db, "Failed to save the bounds of your field.");
		}
		loggit("found xyls width and height " . $W . ", " . $H . "\n");
	}

	// FIXME - do we need this??
	// (I think yes because apache is run by user "www-data" but watcher is run by
	//  user "gmaps")
	if (!chmod($xylist, 0664)) {
		loggit("Failed to chmod xylist " . $xylist . "\n");
	}

	$fstype = $vals["fstype"];
	$fsunit = $vals["fsunit"];
	$fsu = $vals["fsu"];
	$fsl = $vals["fsl"];
	$fse = $vals["fse"];
	$fsv = $vals["fsv"];

	if ($fstype == "ev") {
		// Estimate/Variance given - convert to lower/upper.
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
		submit_failed($db, "Field scale lower or upper bound is zero: " . $fu_lower . ", " . $fu_upper);
	}

	array_push($tryscales, array($fu_lower, $fu_upper));

	$index = $vals['index'];

	if ($index == 'auto') {
		// Estimate size of quads we could find:
		$fmax = 0.5  * min($W, $H) * $fu_upper / 60.0;
		$fmin = 0.1 * min($W, $H) * $fu_lower / 60.0;
		loggit("Collecting indexes with quads in range [" . $fmin . ", " . $fmax . "] arcmin.\n");

		foreach ($indexdata as $k => $v) {
			$sizemap[$k] = $v['quadsize'];
		}

		$indexes = array();
		foreach ($sizemap as $ind => $size) {
			// If they overlap...
			if (!(($fmin > $size[1]) || ($fmax < $size[0]))) {
				array_push($indexes, $ind);
			}
		}

		if (count($indexes) == 0) {
			// too big, or too little?
			if ($fmax > $sizemap[$largest_index][1]) {
				array_push($indexes, $largest_index);
			} else if ($fmin < $sizemap[$smallest_index][0]) {
				array_push($indexes, $smallest_index);
			}
		}

		loggit("Quad size limits: [" . $fmin . ", " . $fmax . "] arcmin, " .
			   "selected indexes: " . implode(", ", $indexes) . "\n");

	} else {
		$indexes = array($index);
	}
	// Compute the index path(s)
	foreach ($indexdata as $k => $v) {
		$indexmap[$k] = $v['paths'];
	}
	$indexpaths = array();
	foreach ($indexes as $i) {
		//loggit("Index " . $i . "\n");
		$indexpaths = array_merge($indexpaths, $indexmap[$i]);
	}
	$indexes = $indexpaths;

	$inputfile = $mydir . $input_fn;
	$inputtmpfile = $mydir . $inputtmp_fn;
	$donescript = $mydir . $donescript_fn;
	$startscript = $mydir . $startscript_fn;
	$donefile = $mydir . $done_fn;

	$parity = $vals["parity"];
	$tweak = $vals["tweak"];
	$tweak_order = $vals['tweak_order'];
	$poserr = $vals["poserr"];
	$codetol = 0.01;

	$inputfile_orig = $inputfile;
	if ($imgfilename) {
		// Write to "input.tmp" instead of "input", so we don't trigger
		// the solver just yet...
		$inputfile = $inputtmpfile;
	} else {
		// MAJOR HACK - pause to let the watcher notice that the
		// new directory was created before writing to the input file.
		sleep(1);
	}

	// Write the input file for blind...
	$fin = fopen($inputfile, "w");
	if (!$fin) {
		die("Failed to write input file " . $inputfile);
	}

	$depths = array(0 => 50,
					50 => 80,
					80 => 100,
					100 => 120,
					120 => 140,
					140 => 160,
					160 => 180,
					180 => 200);

	/*
	$depths = array(0 => 200);
	*/

	$stripenum = 1;

	$str = "total_timelimit " . $totaltime . "\n";

	foreach ($depths as $startdepth => $enddepth) {
		foreach ($tryscales as $range) {
			$fumin = $range[0];
			$fumax = $range[1];

			foreach ($indexes as $ind) {
				$str .= "index " . $indexdir . $ind . "\n";
			}

			$str .= 
				"field " . $xyls_fn . "\n" .
				"match " . sprintf($match_pat, $stripenum) . "\n" .
				"indexrdls " . sprintf($indexrdls_pat, $stripenum) . "\n" .
				"solved " . $solved_fn . "\n" .
				"cancel " . $cancel_fn . "\n" .
				"wcs " . $wcs_fn . "\n" .
				"fields 0\n" .
				"sdepth " . $startdepth . "\n" .
				"depth " . $enddepth . "\n" .
				"parity " . $parity . "\n" .
				"fieldunits_lower " . $fumin . "\n" .
				"fieldunits_upper " . $fumax . "\n" .
				"tol " . $codetol . "\n" .
				"verify_pix " . $poserr . "\n" .
				"nverify 25\n" .
				"nindex_tokeep 25\n" .
				"nindex_tosolve 25\n" .
				"distractors 0.25\n" .
				"ratio_toprint 10\n" .
				"ratio_tokeep 1e9\n" .
				"ratio_tosolve 1e9\n" .
				"ratio_tobail 1e-100\n" .
				"fieldw " . $W . "\n" .
				"fieldh " . $H . "\n" .
				"maxquads " . $maxquads . "\n" .
				"cpulimit " . $maxcpu . "\n" .
				"timelimit " . $maxtime . "\n" .
				($tweak ?
				 "tweak\n" .
				 "tweak_aborder " . $tweak_order . "\n" . 
				 "tweak_abporder " . $tweak_order . "\n" .
				 "tweak_skipshift\n"
				 : "") .
				"run\n" .
				"\n";
			$stripenum++;
		}
	}
	fprintf($fin, "%s", $str);

	if (!fclose($fin)) {
		submit_failed($db, "Failed to write input file for the blind solver.");
	}

	loggit("Wrote blind input file: " . $inputfile . "\n");

	// Write the startscript: executed by "watcher" (via blindscript) 
	// before blind starts.
	$fstart = fopen($startscript, "w");
	if (!$fstart) {
		loggit("Failed to write startscript " . $startscript);
		submit_failed($db, "Failed to write the script for the blind solver.");
	}
	$str = "#! /bin/bash\n" .
		"echo Starting startscript...\n" .
		"touch " . $start_fn . "\n" .
		$sqlite . " " . $jobdata_fn . " \"REPLACE INTO jobdata VALUES('solve-start', '`date -Iseconds`');\"\n" .
		"echo Finished startscript.\n";
	fprintf($fstart, "%s", $str);
	if (!fclose($fstart)) {
		loggit("Failed to close startscript " . $startscript);
		submit_failed($db, "Failed to write the script for the blind solver (2b).");
	}
	if (!chmod($startscript, 0775)) {
		loggit("Failed to chmod startscript " . $startscript);
		submit_failed($db, "Failed to write the script for the blind solver (3b).");
	}

	// Write the donescript: executed by "watcher" (via blindscript) after blind completes.
	$fdone = fopen($donescript, "w");
	if (!$fdone) {
		loggit("Failed to write donescript " . $donescript);
		submit_failed($db, "Failed to write the script for the blind solver.");
	}
	$str = 
		"#! /bin/bash\n" .
		"echo Starting donescript...\n" .
		"if [ -e " . $solved_fn . " -a " .
		"`" . $printsolved . " " . $solved_fn . " | grep -v \"File\" | wc -w` -eq 1 ]; then \n" .
		"  echo \"Field solved.\";\n" .
		"  echo Running wcs-xy2rd...;\n" .
		"  " . $wcs_xy2rd . " -w " . $wcs_fn . " -i " . $xyls_fn . " -o " . $rdls_fn . ";\n" .
		"  echo Running rdlsinfo...;\n" .
		"  " . $rdlsinfo . " " . $rdls_fn . " > " . $rdlsinfo_fn . ";\n" .
		"  echo Merging index rdls file...\n" .
		"  " . $fitsgetext . " -e 0 -e 1 -i " . sprintf($indexrdls_pat, 1) . " -o " . $indexrdls_fn . ".tmp\n" .
		"  for ((s=1;; s++)); do\n" .
		"    FN=\$(printf " . $indexrdls_pat . " \$s);\n" .
		"    if [ ! -e \$FN ]; then\n" .
		"      break;\n" .
		"    fi\n" .
		"    N=\$(( \$(" . $fitsgetext . " -i \$FN | wc -l) - 1 ))\n" .
		"    echo \"\$FN has \$N extensions.\"\n" .
		"    for ((i=1; i<N; i++)); do\n" .
		"      if [ \$i -eq 1 -a \$s -eq 1 ]; then\n" .
		"        continue;\n" .
		"      fi\n" .
		"      " . $tabmerge . " \$FN+\$i " . $indexrdls_fn . ".tmp+1\n" .
		"    done\n" .
		"  done\n" .
		"  mv " . $indexrdls_fn . ".tmp " . $indexrdls_fn . "\n" .
		"  echo Merging match files...\n" .
		"  " . $fitsgetext . " -e 0 -e 1 -i " . sprintf($match_pat, 1) . " -o " . $match_fn . ".tmp\n" .
		"  for ((s=1;; s++)); do\n" .
		"    FN=\$(printf " . $match_pat . " \$s)\n" .
		"    if [ ! -e \$FN ]; then\n" .
		"      break;\n" .
		"    fi\n" .
		"    " . $tabmerge . " \$FN+1 " . $match_fn . ".tmp+1\n" .
		"  done\n" .
		"  echo Sorting match file...\n" .
		"  " . $tabsort . " -i " . $match_fn . ".tmp -o " . $match_fn . " -c logodds -d\n" .
		"  rm " . $match_fn . ".tmp\n" .
		"  " . $wcs_rd2xy . " -w " . $wcs_fn . " -i " . $indexrdls_fn . " -o " . $indexxyls_fn . "\n" .
		"else\n" .
		"  echo \"Field did not solve.\"\n" .
		"fi\n" .
		"touch " . $done_fn . "\n" .
		$sqlite . " " . $jobdata_fn . " \"REPLACE INTO jobdata VALUES('solve-done', '`date -Iseconds`');\"\n";

	if ($emailver) {
		$str .= 
			"echo \"Sending email...\";\n" .
			"wget -q \"http://" . $host . $myuridir . "/status.php?job=" . $myname . "&email\" -O - > send-email-wget.out 2>&1\n";
	}
	$str .=	"echo Finished donescript.\n";

	fprintf($fdone, "%s", $str);

	if (!fclose($fdone)) {
		loggit("Failed to close donescript " . $donescript);
		submit_failed($db, "Failed to write the script for the blind solver (2).");
	}
	if (!chmod($donescript, 0775)) {
		loggit("Failed to chmod donescript " . $donescript);
		submit_failed($db, "Failed to write the script for the blind solver (3).");
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

function check_xycol($vals) {
	$xcol = $vals['x_col'];
	$ycol = $vals['y_col'];
	// This is only a FITS Standard "recommendation", but we enforce it
	// for security reasons.
	$pat = '/[\w-]+/';
	if (preg_match($pat, $xcol) != 1) {
		return array('x_col'=>'FITS column names must be alphanumeric.');
	}
	if (preg_match($pat, $ycol) != 1) {
		return array('y_col'=>'FITS column names must be alphanumeric.');
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
		$url = $vals['imgurl'];
		// allow double "http://"
		if (strstr($url, 'http://http://')) {
			$url = substr($url, 7);
		}
		if (validate_url($url))
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
		if ($l <= 0)
			return array("fsl"=>"Lower bound must be positive!");
		if ($u <= 0)
			return array("fsu"=>"Upper bound must be positive!");
	} else if ($type == "ev") {
		if ($e <= 0)
			return array("fse"=>"Estimate must be positive!");
		if ($v < 1 || $v > 99)
			return array("fsv"=>"% Eror must be between 1 and 99!");
		return TRUE;
	}
	return array("fstype"=>"Invalid fstype");
}

function check_tweak($vals) {
	if (!$vals['tweak'])
		return TRUE;
	if (!strlen($vals['tweak_order'])) {
		return array('tweak_order' => "Tweak order cannot be empty.");
	}
	$o = (int)$vals['tweak_order'];
	if ($o < 0) {
		return array('tweak_order' => "Tweak order must be non-negative.");
	}
	if ($o > 8) {
		return array('tweak_order' => "Tweak order must be <= 8.");
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
				  'tweak', 'tweak_order', 'fsl', 'fsu', 'fse', 'fsv', 'fsunit',
				  'poserr', 'index', 'uname', 'email',
				  'submit', 'linkhere', 
				  #'imagescale',
				  'reset', 'MAX_FILE_SIZE', 'UPLOAD_IDENTIFIER');
	
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

	$template = str_replace('##upload-id##', $form->exportValue('UPLOAD_IDENTIFIER'), $template);

	// fields (and pseudo-fields) that can have errors 
	$errflds = array('xysrc', 'imgfile', 'imgurl', 'fitsfile',
					 'x_col', 'y_col', 'fstype', 'fsl', 'fsu', 'fse', 'fsv',
					 'poserr', 'fs', 'email', 'tweak_order');
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
	$template = str_replace('##sizelimit##', sprintf("%d", $maxfilesize/(1024*1024)), $template);

	// Write the HTML header.
	// (for some reason there seems to be a problem echoing this first
	// line when it's in the file, so I've just plunked it here.)
	echo '<' . '?xml version="1.0" encoding="UTF-8"?' . '>';
	$head = file_get_contents($index_header);
	echo $head;

	// Render the form.
	echo $renderer->toHtml($template);

	$tail = file_get_contents($index_tail);
	$tail = str_replace('##valid-blurb##', $valid_blurb, $tail);
	$tail = str_replace('##sizelimit##',   sprintf("%d", $maxfilesize/(1024*1024)), $tail);
	echo $tail;

}

function convert_image(&$basename, $mydir, &$errstr, &$W, &$H, $db,
					   &$scaleguess) {
	global $fits2xy;
	global $modhead;
	global $plotxy2;
	global $tabsort;
	global $objs_fn;
	global $bigobjs_fn;
	global $fits2xyout_fn;
	global $an_fitstopnm;
	global $fits_filter;
	global $fits_guess_scale;

	$filename = $mydir . $basename;
	$newjd = array();
	$todelete = array();

	loggit("image file: " . filesize($filename) . " bytes.\n");

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
				$errstr = "Failed to decompress image (" . $phrase . ")";
				return FALSE;
			}
			$addsuffix = $suff;
			$filename = $newfilename;
			$typestr = shell_exec("file -b -N -L " . $filename);
			loggit("type: " . $typestr . " for " . $filename . "\n");
			array_push($todelete, $newfilename);
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
				// Run fits2fits.py on it...
				$filtered_fits = $mydir . "filtered.fits";
				$outfile = $mydir . "fits2fits.out";
				$cmd = sprintf($fits_filter, $filename, $filtered_fits) . " > " . $outfile . " 2>&1";
				loggit("Command: " . $cmd . "\n");
				if ((system($cmd, $retval) === FALSE) || $retval) {
					loggit("Command failed, return value " . $retval . ": " . $cmd . "\n");
					$errstr = "Failed to fix your FITS file: \"" . file_get_contents($outfile) . "\"";
					return FALSE;
				}
				$filename = $filtered_fits;

				// Run fits-guess-scale on it...
				$cmd = $fits_guess_scale . " " . $filename;
				loggit("Command: " . $cmd . "\n");
				$out = shell_exec($cmd);
				//loggit("Got: " . $out . "\n");
				$lines = explode("\n", $out);
				foreach ($lines as $ln) {
					$words = explode(" ", $ln);
					if (count($words) < 3)
						continue;
					if (!strcmp($words[0], "scale")) {
						loggit("  " . $words[1] . " => " . $words[2] . "\n");
						$scaleguess[$words[1]] = (float)$words[2];
					}
				}

			}

			$outfile = $mydir . "Xtopnm.out";
			$cmd = sprintf($command, $filename, $pnmimg) . " 2> " . $outfile;
			loggit("Command: " . $cmd . "\n");
			if ((system($cmd, $retval) === FALSE) || $retval) {
				loggit("Command failed, return value " . $retval . ": " . $cmd . "\n");
				$errstr = "Failed to convert your image: \"" . file_get_contents($outfile) . "\"";
				return FALSE;
			}
			$addsuffix = $suff . $addsuffix;
			$imgtype = $suff;
			$gotit = TRUE;
			break;
		}
	}
	if (!$gotit) {
		$errstr = "Unknown image type: the \"file\" program says: \"" . $typestr . "\"";
		return FALSE;
	}
	loggit("found image type " . $imgtype . "\n");

	// Use "pnmfile" to get the image size.
	$cmd = "pnmfile " . $pnmimg;
	loggit("Command: " . $cmd . "\n");
	$res = shell_exec($cmd);
	if (!$res) {
		$errstr = "pnmfile failed.";
		return FALSE;
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

	$newjd['imageW'] = $W;
	$newjd['imageH'] = $H;

	// choose a power-of-two shrink factor that makes the larger dimension <= 800.
	$maxsz = 800;
	$bigger = max($W,$H);
	if ($bigger > $maxsz) {
		$shrink = pow(2, ceil(log($bigger / $maxsz, 2)));
	} else {
		$shrink = 1;
	}

	$newjd['imageshrink'] = $shrink;

	// Also use the output from "pnmfile" to decide if we need to convert to PGM.
	$ss = strstr($res, "PPM");
	//loggit("strstr: " . $ss . "\n");
	if (strlen($ss)) {
		// reduce to PGM.
		$pgmimg = $mydir . "image.pgm";
		$cmd = "ppmtopgm " . $pnmimg . " > " . $pgmimg;
		loggit("Command: " . $cmd . "\n");
		$res = system($cmd, $retval);
		if (($res === FALSE) || $retval) {
			loggit("Command failed: return val " . $retval . ", str " . $res . "\n");
			$errstr = "Failed to reduce your image to grayscale.";
			return FALSE;
		}
		$pnmimg = $pgmimg;
		array_push($todelete, $pgmimg);
	}

	if ($imgtype == "fits") {
		$fitsimg = $filename;
	} else {
		$fitsimg = $mydir . "image.fits";
		$cmd = "pnmtofits " . $pnmimg . " > " . $fitsimg;
		loggit("Command: " . $cmd . "\n");
		$res = system($cmd, $retval);
		if (($res === FALSE) || $retval) {
			loggit("Command failed: return val " . $retval . ", str " . $res . "\n");
			$errstr = "Failed to convert your image to a FITS image.";
			return FALSE;
		}
		//? array_push($todelete, $fitsimg);
	}

	// fits2xy computes the output filename by trimming .fits and adding .xy.fits.
	$xylist = substr($fitsimg, 0, strlen($fitsimg) - strlen(".fits")) . ".xy.fits";

	$fits2xyout = $mydir . $fits2xyout_fn;
	$cmd = $fits2xy . " " . $fitsimg . " > " . $fits2xyout . " 2>&1";
	loggit("Command: " . $cmd . "\n");
	$res = system($cmd, $retval);
	if (($res === FALSE) || $retval) {
		loggit("Command failed: return val " . $retval . ", str " . $res . "\n");
		$errstr = "Failed to perform source extraction: \"" . file_get_contents($fits2xyout) . "\"";
		return FALSE;
	}

	// sort by FLUX.
	$tabsortout = $mydir . "tabsort.out";
	$sortedlist = $mydir . "field.xy.fits";
	$cmd = $tabsort . " -i " . $xylist . " -o " . $sortedlist . " -c FLUX -d > " . $tabsortout;
	loggit("Command: " . $cmd . "\n");
	$res = system($cmd, $retval);
	if (($res === FALSE) || $retval) {
		loggit("Command failed: return val " . $retval . ", str " . $res . "\n");
		$errstr = "Failed to sort the extracted sources by brightness";
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

	$newjd['displayW'] = $dispW;
	$newjd['displayH'] = $dispH;
	$newjd['displayImage'] = $dispimgbase;
	$newjd['displayImagePng'] = $dispimgpngbase;

	foreach (array('big','small') as $sz) {
		if ($sz == 'big') {
			$scale = 1;
			$prefix = $mydir . "big-";
			$imW = $W;
			$imH = $H;
			$xyoff = 1;
			$userimg = $pnmimg_orig;
			$outimg = $mydir . $bigobjs_fn;
		} else {
			$scale = 1 / $shrink;
			$prefix = $mydir;
			$imW = $dispW;
			$imH = $dispH;
			$xyoff = 1 / $shrink;
			$userimg = $dispimg;
			$outimg = $prefix . $objs_fn;
		}

		// Plot the extracted objects.
		$Nbright = 100;
		// -the brightest:
		$objimg1 = $prefix . "objs1.pgm";
		$cmd = $plotxy2 . " -i " . $xylist . " -W " . $imW . " -H " . $imH .
			" -x " . $xyoff . " -y " . $xyoff . " -w 1.75 -S " . $scale .
			" -N " . $Nbright . " > " . $objimg1;
		loggit("Command: " . $cmd . "\n");
		$res = system($cmd, $retval);
		if (($res === FALSE) || $retval) {
			loggit("Command failed: return val " . $retval . ", str " . $res . "\n");
			$errstr = "Failed to plot extracted sources.";
			return FALSE;
		}
		array_push($todelete, $objimg1);
		// -the rest:
		$Nmax = 500;
		$objimg2 = $prefix . "objs2.pgm";
		$cmd = $plotxy2 . " -i " . $xylist . " -W " . $imW . " -H " . $imH .
			" -x " . $xyoff . " -y " . $xyoff . " -n " . $Nbright .
			" -N " . $Nmax . " -r 3 -w 1.75" . " -S " . $scale . " > " . $objimg2;
		loggit("Command: " . $cmd . "\n");
		$res = system($cmd, $retval);
		if (($res === FALSE) || $retval) {
			loggit("Command failed: return val " . $retval . ", str " . $res . "\n");
			$errstr = "Failed to plot extracted sources.";
			return FALSE;
		}
		array_push($todelete, $objimg2);
		// -the sum:
		$objimg = $prefix . "objs.pgm";
		$objimg_orig = $objimg;
		$cmd = "pnmarith -max " . $objimg1 . " " . $objimg2 . " > " . $objimg;
		loggit("Command: " . $cmd . "\n");
		$res = system($cmd, $retval);
		if (($res === FALSE) || $retval) {
			loggit("Command failed: return val " . $retval . ", str " . $res . "\n");
			$errstr = "Failed to plot extracted sources.";
			return FALSE;
		}
		array_push($todelete, $objimg);

		$redimg = $prefix . "red.pgm";
		$cmd = "pgmtoppm red " . $objimg . " > " . $redimg;
		loggit("Command: " . $cmd . "\n");
		$res = system($cmd, $retval);
		if (($res === FALSE) || $retval) {
			loggit("Command failed: return val " . $retval . ", str " . $res . "\n");
			$errstr = "Failed to create image of extracted sources.";
			return FALSE;
		}
		$objimg = $redimg;
		array_push($todelete, $redimg);

		$sumimg = $prefix . "sum.ppm";
		$cmd = "pnmcomp -alpha=" . $objimg_orig . " " . $redimg . " " .
			$userimg . " " . $sumimg;
		loggit("Command: " . $cmd . "\n");
		$res = system($cmd, $retval);
		if (($res === FALSE) || $retval) {
			loggit("Command failed: return val " . $retval . ", str " . $res . "\n");
			$errstr = "Failed to composite image of extracted sources.";
			return FALSE;
		}
		array_push($todelete, $sumimg);

		$cmd = "pnmtopng " . $sumimg . " > " . $outimg;
		loggit("Command: " . $cmd . "\n");
		$res = system($cmd, $retval);
		if (($res === FALSE) || $retval) {
			loggit("Command failed: return val " . $retval . ", str " . $res . "\n");
			$errstr = "Failed to convert composite image of extracted sources to PNG.";
			return FALSE;
		}
	}

	// Rename the original uploaded/downloaded image file to reflect the kind of image we think it is
	$oldname = $mydir . $basename;
	$newname = $mydir . $basename . $addsuffix;
	if (!rename($oldname, $newname)) {
		$errstr = "Failed to rename image file.";
		return FALSE;
	}
	loggit("Renamed image file " . $oldname . " to " . $newname . "\n");
	$basename .= $addsuffix;
	$newjd['imagefilename'] = $basename;

	// make original image read-only.
	if (!chmod($newname, 0440)) {
		loggit("Failed to chmod image " . $newname . "\n");
	}

	// Save all the things we discovered about the img.
	if (!setjobdata($db, $newjd)) {
		$errstr = "Failed to save image parameters.";
		return FALSE;
	}

	// Delete intermediate files.
	$todelete = array_unique($todelete);
	foreach ($todelete as $del) {
		loggit("Deleting temp file " . $del . "\n");
		if (!unlink($del)) {
			loggit("Failed to unlink file: \"" . $del . "\"\n");
		}
	}

	return TRUE;
}




?>
