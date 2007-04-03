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
				 "60degree" => "60-degree Fields",
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
$form->addFormRule('check_xycol');
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

	$xysrc = $vals["xysrc"];
	$imgurl = $vals["imgurl"];

	if ($xysrc == 'url') {
		if (!validate_url($imgurl)) {
			die("Invalid url.");
		}
	}

	// Create a directory for this request.
	$myname = create_random_dir($resultdir);
	if (!$myname) {
		die("Failed to create job directory.");
	}
	$mydir = $resultdir . $myname . "/";

	$mylogfile = $mydir . "loggit";
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

	$msg = "Job " . $myname . ": received:\n";
	$desc = describe_job($jobdata, TRUE);
	foreach ($desc as $k => $v) {
		$msg .= "  " . $k . ": " . $v . "\n";
	}
	highlevellog($msg);

	$imgfilename = "";
	$imgbasename = "";
	//$origname = "";

	// Pixel scales to try.
	$tryscales = array();

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
	$scaleguess = array();
	if ($imgfilename) {
		if (!convert_image($imgbasename, $mydir, $errstr, $W, $H, $db, $scaleguess)) {
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
		// If a FITS bintable file was uploaded:
		// -grab the filename it had on the user's machine:
		$fitsval = $fitsfile->getValue();
		$origname = $fitsval["name"];
		if (!setjobdata($db, array("fits-origname"=>$origname))) {
			die("failed to update jobdata: fits-origname");
		}
		// -move it into place...
		$uploaded_fn = "uploaded.fits";
		$uploaded = $mydir . $uploaded_fn;
		if (!$fitsfile->moveUploadedFile($mydir, $uploaded_fn)) {
			die("failed to move uploaded FITS file into place.");
		}
		// use "fitscopy" to grab the first extension and rename the
		// columns from whatever they were to X,Y.
		$cmd = $fitscopy . " " . $uploaded . "\"[1][col X=" . $jobdata['x_col'] .
			";Y=" . $jobdata['y_col'] . "]\" " . $mydir . $xyls_fn . " 2> " . $mydir . "fitscopy.err";
		loggit("Command: " . $cmd . "\n");
		$res = system($cmd, $retval);
		if ($retval) {
			loggit("Command failed: return val " . $retval . ", str " . $res . "\n");
			die("failed to run command: " . $cmd . "\n");
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
	$donefile = $mydir . $done_fn;

	$index = $vals["index"];
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


	array_push($tryscales, array($fu_lower, $fu_upper));


	if ($index == "auto") {
		// Estimate size of quads we could find:
		$fmax = 0.5  * min($W, $H) * $fu_upper / 60.0;
		$fmin = 0.1 * min($W, $H) * $fu_lower / 60.0;
		loggit("W=$W, H=$H, min(W,H)=" . min($W,$H) . ", fu_upper=$fu_upper.\n");
		loggit("Collecting indexes with quads in range [" . $fmin . ", " . $fmax . "] arcmin.\n");
		// FIXME - should condense all this index data into one array!
		// (list largest-to-smallest).
		$sizemap = array("60degree" => array(900,1800),
						 "30degree" => array(400,900),
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
		$largest = "60degree";
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
					  "60degree" => array("allsky-44/allsky-44"),
					  );
	$indexpaths = array();
	foreach ($indexes as $i) {
		//loggit("Index " . $i . "\n");
		$indexpaths = array_merge($indexpaths, $indexmap[$i]);
	}
	$indexes = $indexpaths;

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
				"nverify 20\n" .
				"nindex_tokeep 25\n" .
				"nindex_tosolve 25\n" .
				"distractors 0.25\n" .
				"ratio_toprint 10\n" .
				"ratio_tokeep 1.9\n" .
				"ratio_tosolve 1e9\n" .
				"ratio_tobail 1e-100\n" .
				"fieldw " . $W . "\n" .
				"fieldh " . $H . "\n" .
				"maxquads " . $maxquads . "\n" .
				"cpulimit " . $maxcpu . "\n" .
				"timelimit " . $maxtime . "\n" .
				($tweak ? "tweak\n" : "") .
				"run\n" .
				"\n";
			$stripenum++;
		}
	}
	fprintf($fin, "%s", $str);

	if (!fclose($fin)) {
		die("failed to write blind input file.");
	}

	loggit("Wrote blind input file: " . $inputfile . "\n");

	// Write the donescript: executed by "watcher" (via blindscript) after blind completes.
	$fdone = fopen($donescript, "w");
	if (!$fdone) {
		die("Failed to write donescript " . $donescript);
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
		"  " . $wcs_rd2xy . " -w " . $wcs_fn . " -i " . $indexrdls_fn . " -o " . $indexxyls_fn . "\n" .
		"else\n" .
		"  echo \"Field did not solve.\"\n" .
		"fi\n" .
		"touch " . $donefile . "\n";
	if ($emailver) {
		$str .= 
			"echo \"Sending email...\";\n" .
			"wget -q \"http://" . $host . $myuridir . "/status.php?job=" . $myname . "&email\" -O - > send-email-wget.out 2>&1\n" .
			"echo Donescript finished.\n";
	}

	fprintf($fdone, "%s", $str);

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
		if (validate_url($vals['imgurl']))
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
				// Run fits2fits.py on it...
				$filtered_fits = $mydir . "filtered.fits";
				$outfile = $mydir . "fits2fits.out";
				$cmd = sprintf($fits_filter, $filename, $filtered_fits) . " > " . $outfile . " 2>&1";
				loggit("Command: " . $cmd . "\n");
				if ((system($cmd, $retval) === FALSE) || $retval) {
					loggit("Command failed, return value " . $retval . ": " . $cmd . "\n");
					die("failed to fix your FITS file: <pre>" . file_get_contents($outfile) . "</pre>");
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
		die("failed to rename img file.");
	}
	loggit("Renamed image file " . $oldname . " to " . $newname . "\n");
	$basename .= $addsuffix;
	$newjd['imagefilename'] = $basename;

	// Save all the things we discovered about the img.
	if (!setjobdata($db, $newjd)) {
		die("failed to save image jobdata.");
	}

	return TRUE;
}




?>
