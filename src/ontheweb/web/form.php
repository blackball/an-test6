<?php
require_once 'HTML/QuickForm.php';
require_once 'HTML/QuickForm/Renderer/QuickHtml.php';
require_once 'svn.php';
require_once 'config.php';

/* Note, if you want to change the maximum uploaded file size you must also change:
  -php.ini: post_max_size
  -php.ini: upload_max_filesize
*/
$maxfilesize = 250*1024*1024;
umask(0002); // in octal

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

// If the "submit" button was pushed, fold in the default values.  This allows
// "wget" users to specify only the fields they want to change from the defaults.
if (array_key_exists('submit', $_POST)) {
    // Plug in the defaults as though the client submitted them.
    foreach ($formDefaults as $k=>$v) {
        if (!array_key_exists($k, $_POST)) {
            $_POST[$k] = $v;
        }
    }

	if (array_key_exists('imgurl', $_POST) &&
		!array_key_exists('xysrc', $_POST)) {
		$_POST['xysrc'] = 'url';
	}
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

$usepassword = array_key_exists('password', $_POST);

$target = '';
if ($usepassword) {
	$target = $myuridir . '/submit.php';
}

// Create the HTML form object...
$form =& new HTML_QuickForm('blindform','post', $target);

// XHTML compliance:
$form->removeAttribute('name');

$form->setDefaults($formDefaults);

$upl =& $form->addElement('hidden', 'UPLOAD_IDENTIFIER');
$upl->setValue($upload_id);

$form->addElement('hidden', 'skippreview');
$form->addElement('hidden', 'justjobid');

$form->addElement('radio', 'xysrc', null, null, 'img' , array('id'=>'xysrc-img') );
$form->addElement('radio', 'xysrc', null, null, 'url' , array('id'=>'xysrc-url') );
$form->addElement('radio', 'xysrc', null, null, 'fits', array('id'=>'xysrc-fits'));
$form->addElement('radio', 'xysrc', null, null, 'text', array('id'=>'xysrc-text'));

$form->setAttribute('onSubmit', 'setTimeout(\'showUploadMeter()\', 3000); checkRemember();');

$form->addElement('radio', 'fstype', null, null, 'ul', array('id'=>'fstype-ul'));
$form->addElement('radio', 'fstype', null, null, 'ev', array('id'=>'fstype-ev'));

$imgfile  =& $form->addElement('file', 'imgfile', "imgfile",
							   array('onfocus' => "setXysrcImg()",
									 'onclick' => "setXysrcImg()",));
$fitsfile =& $form->addElement('file', 'fitsfile', "fitsfile",
							   array('onfocus' => "setXysrcFits()",
									 'onclick' => "setXysrcFits()"));
$textfile =& $form->addElement('file', 'textfile', "textfile",
							   array('onfocus' => "setXysrcText()",
									 'onclick' => "setXysrcText()"));
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

$form->addElement('text', 'uname', null,
				  array('size'=>40,
						'id'=>'uname'));
$form->addElement('text', 'email', null,
				  array('size'=>40,
						'id'=>'email'));

$form->addElement('checkbox', 'remember', null, null,
                  array('id'=>'remember'));

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
$form->addElement('select', 'index', 'index', $indexes, null);

$form->addElement('submit', 'submit', 'Submit');
$form->addElement('submit', 'linkhere', 'Link to these parameter settings');
$form->addElement('submit', 'reset', "Reset Form");
$form->addElement('submit', 'logout', 'Logout');

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

if ($emailver && !$usepassword) {
	$form->addFormRule('check_email');
}

// Handle "link to here" requests, by formatting an appropriate GET URL and redirecting.
if ($form->exportValue("linkhere")) {
	$vals = $form->exportValues();
	$args = format_preset_url_from_form($vals, $formDefaults);
	loggit("Location: http://" . $host . $myuri . $args . "\n");
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

if ($form->exportValue('logout')) {
	loggit("Logout.\n");
	loggit("Form values:\n");
	foreach ($form->exportValues() as $k => $v) {
		loggit("  " . $k . " = " . $v . "\n");
	}
	header('HTTP/1.0 401 Logging you out.');
	// HACK - this must match Apache config...
	header('WWW-Authenticate: Basic realm="Astrometry.net Alpha Test"');
	echo("Logged out.\n");
	exit;
}

loggit("Form values:\n");
foreach ($form->exportValues() as $k => $v) {
	loggit("  " . $k . " = " . $v . "\n");
}

loggit("Using password: " . ($usepassword ? "yes" : "no") . "\n");

$vals = $form->exportValues();

// If the form has been filled out correctly (and the Submit button pressed), process it.
if (array_key_exists('submit', $vals) && $form->validate()) {

	if ($usepassword) {
		$submitscript = 'submit.php';
		if ($scriptname != $submitscript) {
			$loc = "http://" . $host . $myuridir . '/' . $submitscript . '?' . $_SERVER["QUERY_STRING"];
			loggit('Redirecting to ' . $loc . "\n");
			header("Location: " . $loc);
			exit;
		} else {
			loggit("Form was submitted to the password-secured page; processing it.\n");
			loggit("Request headers:\n");
			$hdrs = apache_request_headers();
			foreach ($hdrs as $k=>$v) {
				loggit("  " . $k . " = " . $v . "\n");
			}
			if (!array_key_exists('Authorization', $hdrs)) {
				loggit("No Authorization.\n");
				exit;
			} else {
				$auth = explode(' ', $hdrs['Authorization']);
				if ($auth[0] != 'Basic') {
					loggit("Not Basic\n");
					exit;
				}
				$userpass = base64_decode($auth[1]);
				loggit("Username/password: " . $userpass . "\n");
				$up = explode(':', $userpass);
				loggit("Username " . $up[0] . ", password " . $up[1] . "\n");
			}
		}
	}

    $form->freeze();
    $form->process('process_data', false);
    exit;
}

// Else render the form.
render_form($form, $headers);
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
    global $textfile;
	global $resultdir;
	global $dbfile;
	global $maxfilesize;
	global $totaltime;
	global $totalcpu;
	global $maxtime;
	global $maxquads;
	global $maxcpu;
	global $rdlsinfo;
	global $xylsinfo;
	global $indexdir;
	global $printsolved;
	global $wcs_xy2rd;
	global $wcs_rd2xy;
	global $get_wcs;
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
	global $corr_fn;
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
	global $xylist2fits;
	global $tabsort;
	global $ontheweblogfile;
	global $indexdata;
	global $largest_index;
	global $smallest_index;
	global $sqlite;
	global $webver;

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
	$flds = array('xysrc', 'imgfile', 'fitsfile', 'textfile', 'imgurl',
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
	} else if ($xysrc == 'text') {
		$textval = $textfile->getValue();
		$origname = $textval['name'];
		$jobdata['text-origname'] = $origname;
	}

	// Original submission date.
	$jobdata['submit-date'] = get_datestr(time());

	// Original subversion rev.
	$jobdata['submit-svn-rev'] = get_svn_rev();
	$jobdata['submit-svn-url'] = get_svn_url();
	$jobdata['submit-svn-date'] = get_svn_date();

	// fix double "http://"
	if (($xysrc == "url") && strstr($imgurl, 'http://http://')) {
		$imgurl = substr($imgurl, 7);
		$vals['imgurl'] = $imgurl;
		$jobdata['imgurl'] = $imgurl;
	}

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
		if (!convert_image($imgbasename, $mydir, $errstr, $W, $H, $db, $scaleguess, $inwcsfile)) {
			submit_failed($db, "Failed to run source extraction: " . $errstr);
			die($errstr);
		}
		$imgfilename = $mydir . $imgbasename;

		loggit("Got " . count($scaleguess) . " guesses about the image scale.\n");
		foreach ($scaleguess as $method => $scale) {
			loggit("  " . $method . ": " . $scale . "\n");
		}
		$uscales = array_unique(array_values($scaleguess));
		foreach ($uscales as $s) {
			// Add 1% margin.
			array_push($tryscales, array($s * 0.99, $s * 1.01));
		}
	}

	$xylist = $mydir . $xyls_fn;
	$xylsinfofile = $mydir . $xylsinfo_fn;

	$gotfits = FALSE;

	if ($xysrc == 'text') {
		// If a text xylist was uploaded:
		// -move it into place.
		$uploaded_fn = "uploaded.txt";
		$uploaded = $mydir . $uploaded_fn;
		if (!$textfile->moveUploadedFile($mydir, $uploaded_fn)) {
			submit_failed($db, "Failed to move uploaded text file into place.");
		}
		// -make it read-only.
		if (!chmod($uploaded, 0440)) {
			loggit("Failed to chmod text file " . $uploaded . "\n");
		}
		// -try to convert it to FITS.
		$cmd = $xylist2fits . " " . $uploaded . " " . $xylist .
			" > " . $mydir . "xylist2fits.out" .
			" 2> " . $mydir . "xylist2fits.err";
		loggit("Command: " . $cmd . "\n");
		$res = system($cmd, $retval);
		if (($res === FALSE) || $retval) {
			loggit("Command failed: return val " . $retval . ", str " . $res . "\n");
			submit_failed($db, "Failed to convert text XY list to FITS.");
		}
		$gotfits = TRUE;

	} else if ($xysrc == 'fits') {
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
			";Y=" . $jobdata['y_col'] . "]\" " . $xylist . " 2> " . $mydir . "fitscopy.err";
		loggit("Command: " . $cmd . "\n");
		$res = system($cmd, $retval);
		if (($res === FALSE) || $retval) {
			loggit("Command failed: return val " . $retval . ", str " . $res . "\n");
			submit_failed($db, "Failed to extract the pixel coordinate columns from your FITS file.");
		}
		$gotfits = TRUE;
		$inwcsfile = $xylist;
	}

	if ($gotfits) {
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
		// If FITS file contains "IMAGEW" and "IMAGEH" headers, use those.
		if ($infomap['imagew'] && $infomap['imageh']) {
			$W = $infomap['imagew'];
			$H = $infomap['imageh'];
		} else {
			$W = $infomap['x_max'];
			$H = $infomap['y_max'];
		}
		$shrink = get_shrink_factor($W, $H);
		// Size of image to display to the user.
		$dispW = (int)($W / $shrink);
		$dispH = (int)($H / $shrink);

		if (!setjobdata($db, array('xylsW' => $W,
								   'xylsH' => $H,
								   'imageshrink' => $shrink,
								   'displayW' => $dispW,
								   'displayH' => $dispH))) {
			submit_failed($db, "Failed to save the bounds of your field.");
		}
		loggit("found xyls width and height " . $W . ", " . $H . "\n");
	}

	// FIXME - do we need this??
	// (I think yes because apache is run by user "www-data" but watcher is
	//  run by user "gmaps")
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

	if ($fu_lower > $fu_upper) {
		loggit("Swapping reversed lower/upper bounds.\n");
		$tmp = $fu_lower;
		$fu_lower = $fu_upper;
		$fu_upper = $tmp;
	}

	array_push($tryscales, array($fu_lower, $fu_upper));

	// Merge overlapping scale estimates.
	// first sort them by the lower bound.
	foreach ($tryscales as $s) {
		//loggit("Scale estimate: [" . $s[0] . ", " . $s[1] . "]\n");
		$sorted["" . $s[0]] = $s[1];
	}
	ksort($sorted, SORT_NUMERIC);
	//loggit("Sorted:\n");
	foreach ($sorted as $lo => $hi) {
		//loggit("Scale estimate: [" . $lo . ", " . $hi . "]\n");
	}
	$merged = array();
	$sk = array_keys($sorted);
	$sv = array_values($sorted);
	for ($i=0; $i<count($sk);) {
		$thislo = $sk[$i];
		$thishi = $sv[$i];
		//loggit("Range: " . $i . ": [" . $thislo . ", " . $thishi . "]\n");
		for ($j=$i+1; $j<count($sorted); $j++) {
			$nextlo = $sk[$j];
			$nexthi = $sv[$j];
			//loggit("Next Range (" . $j . "): [" . $nextlo . ", " . $nexthi . "]\n");
			if ($nextlo <= $thishi) {
				// Merge.
				$thishi = max($thishi, $nexthi);
				//loggit("Merged to produce: [" . $thislo . ", " . $thishi . "]\n");
			} else {
				//loggit("No overlap.\n");
				break;
			}
		}
		$i = $j;
		//loggit("Adding range: [" . $thislo . ", " . $thishi . "]\n");
		array_push($merged, array($thislo, $thishi));
	}
	loggit("Merged scales:\n");
	foreach ($merged as $s) {
		loggit("  [" . $s[0] . ", " . $s[1] . "]\n");
	}
	$tryscales = $merged;

	$index = $vals['index'];

	if ($index == 'auto') {
		// Estimate size of quads we could find:
		$fmax = 1.0 * max($W, $H) * $fu_upper / 60.0;
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
	if ($webver && $imgfilename) {
		// Write to "input.tmp" instead of "input", so we don't trigger
		// the solver just yet...
		$inputfile = $inputtmpfile;
	} else {
		// MAJOR HACK - pause to let the watcher notice that the
		// new directory was created before writing to the input file.
		sleep(1);
	}

	// Write startscript and donescript before input file because they should
	// be valid before blind starts!

	// Write the startscript: executed by "watcher" (via blindscript) 
	// before blind starts.
	$str = "#! /bin/bash\n" .
		"echo Starting startscript...\n" .
		"touch " . $start_fn . "\n" .
		$sqlite . " " . $jobdata_fn . " \"REPLACE INTO jobdata VALUES('solve-start', '`date -Iseconds`');\"\n" .
		"echo Finished startscript.\n";
	if (file_put_contents($startscript, $str) === FALSE) {
		loggit("Failed to write startscript " . $startscript);
		submit_failed($db, "Failed to write the script for the blind solver.");
	}
	if (!chmod($startscript, 0775)) {
		loggit("Failed to chmod startscript " . $startscript);
		submit_failed($db, "Failed to chmod the startscript for the blind solver.");
	}

	// Write the donescript: executed by "watcher" (via blindscript) after blind completes.
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
		"  for ((s=2;; s++)); do\n" .
		"    FN=\$(printf " . $match_pat . " \$s)\n" .
		"    if [ ! -e \$FN ]; then\n" .
		"      break;\n" .
		"    fi\n" .
		"    " . $tabmerge . " \$FN+1 " . $match_fn . ".tmp+1\n" .
		"  done\n" .
		"  echo Sorting match file...\n" .
		"  " . $tabsort . " -d logodds " . $match_fn . ".tmp " . $match_fn . "\n" .
		"  rm " . $match_fn . ".tmp\n" .
		"  " . $wcs_rd2xy . " -w " . $wcs_fn . " -i " . $indexrdls_fn . " -o " . $indexxyls_fn . "\n" .
		"  echo Adding jobid to FITS headers...\n" .
		"  for x in " . implode(" ", array($match_fn, $indexxyls_fn, $indexrdls_fn, $wcs_fn, $xyls_fn, $rdls_fn)) . " ; do\n" .
		"    " . $modhead . " \$x AN_JOBID " . $myname . " \"Astrometry.net job ID\"" . "\n" .
		"  done\n" .
		"else\n" .
		"  echo \"Field did not solve.\"\n" .
		"fi\n" .
		"touch " . $done_fn . "\n" .
		$sqlite . " " . $jobdata_fn . " \"REPLACE INTO jobdata VALUES('solve-done', '`date -Iseconds`');\"\n";
	if ($emailver) {
		$str .= 
			"echo \"Sending email...\";\n" .
			"wget -q \"http://" . $host . $myuridir . "/status.php?job=" . $myname . "&send-email=1\" -O - > send-email-wget.out 2>&1\n";
	}
	$str .=	"echo Finished donescript.\n";

	if (file_put_contents($donescript, $str) === FALSE) {
		loggit("Failed to write donescript " . $donescript);
		submit_failed($db, "Failed to write the \"donescript\" for the blind solver.");
	}
	if (!chmod($donescript, 0775)) {
		loggit("Failed to chmod donescript " . $donescript);
		submit_failed($db, "Failed to chmod the donescript for the blind solver.");
	}


	// Write the input file for blind...
	$depths = array(0  => 20,
					20 => 30,
					30 => 40,
					40 => 50,
					50 => 60,
					60 => 70,
					70 => 80,
					80 => 100,
					100 => 120,
					120 => 140,
					140 => 160,
					160 => 180,
					180 => 200);

	$inparallel = 1;
	
	// If there's only one scale estimate, there's no need to do
	// striping!
	if ((count($tryscales) == 1) &&
		($inparallel || (count($indexes) == 1))) {
		$depths = array(0 => 200);
	}

	$stripenum = 1;

	$str =
		(($totaltime > 0) ? "total_timelimit " . $totaltime . "\n" : "") .
		(($totalcpu  > 0) ? "total_cpulimit " . $totalcpu . "\n" : "");

	foreach ($depths as $startdepth => $enddepth) {
		foreach ($tryscales as $range) {
			$fumin = $range[0];
			$fumax = $range[1];

			if ($inparallel)
				$str .= "indexes_inparallel\n";

			foreach ($indexes as $ind) {
				$str .= "index " . $indexdir . $ind . "\n";
			}

			$str .= 
                "verbose\n" .
                "logtostderr\n" .
				"field " . $xyls_fn . "\n" .
				"match " . sprintf($match_pat, $stripenum) . "\n" .
				"indexrdls " . sprintf($indexrdls_pat, $stripenum) . "\n" .
				## Science paper
				#"indexrdls_expand 3\n" .
                #"nsolves 2\n" .
                ##
				"solved " . $solved_fn . "\n" .
				"correspondences " . $corr_fn . "\n" .
				"cancel " . $cancel_fn . "\n" .
				"wcs " . $wcs_fn . "\n" .
				"fields 1\n" .
				"sdepth " . $startdepth . "\n" .
				"depth " . $enddepth . "\n" .
				"parity " . $parity . "\n" .
				"fieldunits_lower " . $fumin . "\n" .
				"fieldunits_upper " . $fumax . "\n" .
				"tol " . $codetol . "\n" .
				"verify_pix " . $poserr . "\n" .
				"distractors 0.25\n" .
				"ratio_toprint 1e4\n" .
				"ratio_tokeep 1e9\n" .
				"ratio_tosolve 1e9\n" .
				"ratio_tobail 1e-100\n" .
                "quadsize_min " . (0.1 * min($W, $H)) . "\n" .
				"fieldw " . $W . "\n" .
				"fieldh " . $H . "\n" .
				(($maxquads > 0) ? "maxquads " . $maxquads . "\n" : "") .
				(($cpulimit > 0) ? "cpulimit " . $maxcpu . "\n" : "") .
				(($maxtime  > 0) ? "timelimit " . $maxtime . "\n" : "");

			// uniformize into grid cells, but make them have about 1:1
			// aspect ratio.
			/*
			$ngrid = 4;
 			$nx = ceil(sqrt($ngrid) * $W / $H);
			$ny = ceil(sqrt($ngrid) * $H / $W);
			$str .= "uniformize " . $nx . " " . $ny . "\n";
			*/

			if ($tweak) {
				$str .=
					"tweak\n" .
					"tweak_aborder " . $tweak_order . "\n" . 
					"tweak_abporder " . $tweak_order . "\n" .
					"tweak_skipshift\n";
			}

			// Only do "verify-only" on the first pass.
			if (($stripenum == 1) && $inwcsfile) {
				$verify_fn = 'verify.fits';
				$cmd = $get_wcs . " -o " . $mydir . $verify_fn . " " . $inwcsfile . " >/dev/null 2>&1";
				if ((system($cmd, $retval) === FALSE) || $retval) {
					loggit("Failed to run get-wcs: " . $cmd . ", return val " . $retval . "\n");
				} else {
					$str .=
						"verify " . $verify_fn . "\n";
				}
			}

			$str .=
				"run\n" .
				"\n";
			$stripenum++;
		}
	}

	if (file_put_contents($inputfile, $str) === FALSE) {
		die("Failed to write input file " . $inputfile);
	}
	loggit("Wrote blind input file: " . $inputfile . "\n");

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
    global $textfile;

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
	case 'text':
		if (!$textfile->isUploadedFile()) {
			return array("textfile" => "You must upload a text file!");
		}
		$textval = $textfile->getValue();
		if ($textval["size"] == 0) {
			return array("textfile" => "The text file you uploaded is empty.");
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
function render_form($form, $headers) {
	global $index_template;
	global $index_template_simple;
	global $index_header;
	global $index_header_simple;
	global $index_tail;
	global $index_tail_simple;
	global $valid_blurb;
	global $emailver;
	global $webver;

	$maxfilesize = $form->getMaxFileSize();
	$renderer =& new HTML_QuickForm_Renderer_QuickHtml();
	$form->accept($renderer);

	if ($webver && !array_key_exists('advanced', $headers)) {
		$template = file_get_contents($index_template_simple);
		$replace = array();
		$flds = array('imgurl', 'submit', 'MAX_FILE_SIZE', 'skippreview');
		foreach ($flds as $fld) {
			$replace['##' . $fld . '##'] = $renderer->elementToHtml($fld);
		}
		$replace['##upload-id##'] = $form->exportValue('UPLOAD_IDENTIFIER');
		$replace['##sizelimit##'] = sprintf("%d", $maxfilesize/(1024*1024));
		$errflds = array('imgurl');
		foreach ($errflds as $fld) {
			$replace['##' . $fld . '-err##'] = $form->getElementError($fld);
		}
		$template = str_replace(array_keys($replace), array_values($replace), $template);

		// lie to the renderer and tell it we've already rendered these form
		// elements by calling "elementToHtml".
		$dummyflds = array('imgfile', 'fitsfile', 'textfile', 'x_col',
						   'y_col', 'tweak', 'tweak_order', 'fsl', 'fsu',
						   'fse', 'fsv', 'fsunit', 'poserr', 'index',
						   'uname', 'email', 'remember', 'linkhere',
						   'reset', 'justjobid');
		foreach ($dummyflds as $fld) {
			$renderer->elementToHtml($fld);
		}
		$renderer->elementToHtml('xysrc', 'img');
		$renderer->elementToHtml('xysrc', 'url');
		$renderer->elementToHtml('xysrc', 'fits');
		$renderer->elementToHtml('xysrc', 'text');
		$renderer->elementToHtml('parity', '2');
		$renderer->elementToHtml('parity', '1');
		$renderer->elementToHtml('parity', '0');
		$renderer->elementToHtml('fstype', 'ul');
		$renderer->elementToHtml('fstype', 'ev');

		echo '<' . '?xml version="1.0" encoding="UTF-8"?' . '>';
		$head = file_get_contents($index_header_simple);
		echo $head;
		// Render the form.
		echo $renderer->toHtml($template);
		$tail = file_get_contents($index_tail_simple);
		$tail = str_replace(array('##valid-blurb##', '##sizelimit##'),
							array($valid_blurb, sprintf("%d", $maxfilesize/(1024*1024))),
							$tail);
		echo $tail;
		return;
	}

	$replace['##upload-id##'] = $form->exportValue('UPLOAD_IDENTIFIER');
	$replace['##sizelimit##'] = sprintf("%d", $maxfilesize/(1024*1024));

	// fields (and pseudo-fields) that can have errors 
	$errflds = array('xysrc', 'imgfile', 'imgurl', 'fitsfile', 'textfile',
					 'x_col', 'y_col', 'fstype', 'fsl', 'fsu', 'fse', 'fsv',
					 'poserr', 'fs', 'email', 'tweak_order');
	foreach ($errflds as $fld) {
		$replace['##' . $fld . '-err##'] = $form->getElementError($fld);
	}

	// Load the index-template file and do text replacement on it.
	$template = file_get_contents($index_template);

	$replace = array();

	// all the "regular" fields.
	$flds = array('imgfile', 'fitsfile', 'textfile', 'imgurl', 'x_col', 'y_col',
				  'tweak', 'tweak_order', 'fsl', 'fsu', 'fse', 'fsv', 'fsunit',
                      'poserr', 'index', 'uname', 'email', 'remember',
				  'submit', 'linkhere',
				  'reset', 'MAX_FILE_SIZE', 'UPLOAD_IDENTIFIER',
				  'justjobid', 'skippreview');
	
	if ($emailver) {
		$replace['##email-caption##'] = "We need your email address to send you your results.";
	} else if ($webver) {
		$replace['##email-caption##'] = "These fields are optional.";
	}
	foreach ($flds as $fld) {
		$replace['##' . $fld . '##'] = $renderer->elementToHtml($fld);
	}

	$replace['##upload-id##'] = $form->exportValue('UPLOAD_IDENTIFIER');
	$replace['##sizelimit##'] = sprintf("%d", $maxfilesize/(1024*1024));

	// fields (and pseudo-fields) that can have errors 
	$errflds = array('xysrc', 'imgfile', 'imgurl', 'fitsfile', 'textfile',
					 'x_col', 'y_col', 'fstype', 'fsl', 'fsu', 'fse', 'fsv',
					 'poserr', 'fs', 'email', 'tweak_order');
	foreach ($errflds as $fld) {
		$replace['##' . $fld . '-err##'] = $form->getElementError($fld);
	}

	// Radio buttons
	$replace['##xysrc-img##']    = $renderer->elementToHtml('xysrc', 'img');
	$replace['##xysrc-url##']    = $renderer->elementToHtml('xysrc', 'url');
	$replace['##xysrc-fits##']   = $renderer->elementToHtml('xysrc', 'fits');
	$replace['##xysrc-text##']   = $renderer->elementToHtml('xysrc', 'text');
	$replace['##parity-both##']  = $renderer->elementToHtml('parity', '2');
	$replace['##parity-left##']  = $renderer->elementToHtml('parity', '1');
	$replace['##parity-right##'] = $renderer->elementToHtml('parity', '0');
	$replace['##fstype-ul##']    = $renderer->elementToHtml('fstype', 'ul');
	$replace['##fstype-ev##']    = $renderer->elementToHtml('fstype', 'ev');

	if (array_key_exists('imgfile', $headers)) {
		// Hack - we hard-code the table row here instead of having it in the template
		// (so that if it's empty we don't get an empty table row)
		$str = "<tr><td></td><td>Previous value: <tt>" . $headers['imgfile'] . "</tt></td></tr>\n";
		$replace['##imgfile_set##'] = $str;
	} else {
		$replace['##imgfile_set##'] = '';
	}
	if (array_key_exists('fitsfile', $headers)) {
		// Hack - ditto.
		$str = "<li>Previous value: <tt>" . $headers['fitsfile'] . "</tt></li\n";
		$replace['##fitsfile_set##'] = $str;
	} else {
		$replace['##fitsfile_set##'] = "";
	}
	if (array_key_exists('textfile', $headers)) {
		// Hack - ditto again.
		$str = "<li>Previous value: <tt>" . $headers['textfile'] . "</tt></li\n";
		$replace['##textfile_set##'] = $str;
	} else {
		$replace['##textfile_set##'] = "";
	}

	$template = str_replace(array_keys($replace), array_values($replace), $template);

	// Write the HTML header.
	// (for some reason there seems to be a problem echoing this first
	// line when it's in the file, so I've just plunked it here.)
	echo '<' . '?xml version="1.0" encoding="UTF-8"?' . '>';
	$head = file_get_contents($index_header);
	echo $head;

	// Render the form.
	echo $renderer->toHtml($template);

	$tail = file_get_contents($index_tail);
	$tail = str_replace(array('##valid-blurb##', '##sizelimit##'),
						array($valid_blurb, sprintf("%d", $maxfilesize/(1024*1024))),
						$tail);
	echo $tail;

}

function convert_image(&$basename, $mydir, &$errstr, &$W, &$H, $db,
					   &$scaleguess, &$wcsfile) {
	global $image2xy;
	global $modhead;
	global $plotxy;
	//global $tabsort;
	global $resort;
	global $objs_fn;
	global $bigobjs_fn;
	global $image2xyout_fn;
	global $an_fitstopnm;
	global $fits_filter;
	global $fits_guess_scale;
	global $xyls_fn;

	$filename = $mydir . $basename;
	$newjd = array();
	$todelete = array();

	loggit("image file: " . filesize($filename) . " bytes.\n");

	$addsuffix = "";

	// Handle compressed file formats.
	$newfilename = $mydir . "uncompressed.";
	if (!uncompress_file($filename, $newfilename, $suff)) {
		$errstr = "Failed to decompress image " . $filename;
		return FALSE;
	}
	if ($suff) {
		$addsuffix = $suff;
		$filename = $newfilename;
		array_push($todelete, $newfilename);
	}
	$typestr = shell_exec("file -b -N -L " . $filename);

	// Handle different image types
	$imgtypes = array("FITS image data"  => array("fits", $an_fitstopnm . " -i %s > %s"),
					  "JPEG image data"  => array("jpg",  "jpegtopnm %s > %s"),
					  "PNG image data"   => array("png",  "pngtopnm %s > %s"),
					  "GIF image data"   => array("gif",  "giftopnm %s > %s"),
					  "Netpbm PPM"       => array("pnm",  "ppmtoppm < %s > %s"),
					  "Netpbm PGM"       => array("pnm",  "pgmtoppm %s > %s"),
					  "TIFF image data"  => array("tiff", "tifftopnm %s > %s"),
					  );

	$pnmimg_orig_base = "image.pnm";
	$pnmimg = $mydir . $pnmimg_orig_base;
	$pnmimg_orig = $pnmimg;

	$gotit = FALSE;
	// Look for key phrases in the output of "file".
	foreach ($imgtypes as $phrase => $lst) {
		if (!strstr($typestr, $phrase)) {
			continue;
		}
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

		// Run *topnm on it.
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

	// Decide how much to shrink the image by for displaying results.
	$shrink = get_shrink_factor($W, $H);

	$newjd['imageW'] = $W;
	$newjd['imageH'] = $H;
	$newjd['imageshrink'] = $shrink;

	// Also use the output from "pnmfile" to decide if we need to convert to PGM (greyscale)
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
		$wcsfile = $filename;
		$fitsimg = $filename;
	} else {
		// Run pnmtofits...
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

	// The xylist filename:
	// image2xy computes the output filename by trimming .fits and adding .xy.fits.
	$xylist = substr($fitsimg, 0, strlen($fitsimg) - strlen(".fits")) . ".xy.fits";

	// Run image2xy...
	$image2xyout = $mydir . $image2xyout_fn;
	$cmd = $image2xy;
    if ($W * $H >= 5000000) {
        $cmd .= " -H";
    }
    $cmd .= " -D 4";
    $cmd .= " " . $fitsimg . " > " . $image2xyout . " 2>&1";
	loggit("Command: " . $cmd . "\n");
	$res = system($cmd, $retval);
	if (($res === FALSE) || $retval) {
		loggit("Command failed: return val " . $retval . ", str " . $res . "\n");
		$errstr = "Failed to perform source extraction: \"" . file_get_contents($image2xyout) . "\"";
		return FALSE;
	}

	// sort the xylist by FLUX.
	$tabsortout = $mydir . "tabsort.out";
	$sortedlist = $mydir . $xyls_fn;

	//$cmd = $tabsort . " -i " . $xylist . " -o " . $sortedlist . " -c FLUX -d > " . $tabsortout;
	$cmd = $resort . " " . $xylist . " " . $sortedlist . " -d > " . $tabsortout;

	loggit("Command: " . $cmd . "\n");
	$res = system($cmd, $retval);
	if (($res === FALSE) || $retval) {
		loggit("Command failed: return val " . $retval . ", str " . $res . "\n");
		$errstr = "Failed to sort the extracted sources by brightness";
		return FALSE;
	}
	$xylist = $sortedlist;

	// Create shrunken-down images to display to the user.
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
		if (($res === FALSE) || $retval) {
			loggit("Command failed: return val " . $retval . ", str " . $res . "\n");
			$errstr = "Failed to shrink image file.";
			return FALSE;
		}
	}

	// Produce small-size PNG
	$dispimgpngbase = substr($dispimgbase, 0, -4) . ".png";
	$cmd = "pnmtopng " . $dispimg . " > " . $mydir . $dispimgpngbase;
	$res = system($cmd, $retval);
	if (($res === FALSE) || $retval) {
		loggit("Command failed: return val " . $retval . ", str " . $res . "\n");
		$errstr = "Failed to convert image to PNG.";
		return FALSE;
	}

	$newjd['displayW'] = $dispW;
	$newjd['displayH'] = $dispH;
	$newjd['displayImage'] = $dispimgbase;
	$newjd['displayImagePng'] = $dispimgpngbase;

	// Produce full-size PNG.
	$imagepng = "fullsize.png";
	$newjd['imagePng'] = $imagepng;
	$cmd = "pnmtopng " . $pnmimg_orig . " > " . $mydir . $imagepng;
	$res = system($cmd, $retval);
	if (($res === FALSE) || $retval) {
		loggit("Command failed: return val " . $retval . ", str " . $res . "\n");
		$errstr = "Failed to convert full-size image to PNG.";
		return FALSE;
	}

	// Produce overlay plots.
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
        $Nmax = 500;
        $commonargs = " -i " . $xylist .
			" -x " . $xyoff . " -y " . $xyoff . " -w 2 -S " . $scale . " -C red";
        $cmd = $plotxy . " -I " . $userimg . $commonargs . " -r 6" .
            " -N " . $Nbright . " -P";
        $cmd .=  " | " . $plotxy . " -I - " . $commonargs . " -r 4" .
            " -n " . $Nbright . " -N " . $Nmax . " > " . $outimg;
        loggit("Command: " . $cmd . "\n");
        $res = system($cmd, $retval);
        if (($res === FALSE) || $retval) {
            loggit("Command failed: return val " . $retval . ", str " . $res . "\n");
            $errstr = "Failed to plot extracted sources.";
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
