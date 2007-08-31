<?php
require_once 'MDB2.php';
require_once 'PEAR.php';

require_once 'config.php';

// Are we using a remote compute server?
$remote = 1;

$gmaps_url = "http://usno.astrometry.net/";
$tiles_url = $gmaps_url . "tilecache/tilecache.php";
$upload_progress = "/tmp/upt_%s.txt";

// blind params
$totaltime = 0;
$totalcpu = 600; // Ten minute compute-time max.
$maxtime = 0;
$maxquads = 0;
$maxcpu = 0;

$index_template_simple = "index-template-simple.html";
$index_template = "index-template.html";
$index_header   = "index-header.html";
$index_header_simple = "index-header-simple.html";
$index_tail     = "index-tail.html";
$index_tail_simple = "index-tail-simple.html";
$submitted_html = "submitted.html";

$q_fn = "queue";

$input_fn = "input"; // note: this must agree with "blindscript.sh"/"watcher".
$inputtmp_fn = "input.tmp";
$start_fn = "start";
$done_fn  = "done";
$donescript_fn  = "donescript"; // note: this must agree with "blindscript.sh"
$startscript_fn  = "startscript"; // note: this must agree with "blindscript.sh"
$xyls_fn  = "field.xy.fits";
$rdls_fn  = "field.rd.fits";
$indexxyls_fn = "index.xy.fits";

$indexrdls_pat = "index-%d.rd.fits";
$match_pat = "match-%d.fits";

$indexrdls_fn = "index.rd.fits";
$match_fn = "match.fits";

$log_fn   = "log";
$solved_fn= "solved";
$cancel_fn= "cancel";
$wcs_fn   = "wcs.fits";
$newheader_fn   = "newheader.fits";
$objs_fn  = "objs.png";
$bigobjs_fn  = "objs-big.png";
$overlay_fn="overlay.png";
$bigoverlay_fn="overlay-big.png";
$const_overlay_fn="const-overlay.png";
$const_bigoverlay_fn="const-overlay-big.png";
$const_list_fn = "const-list.txt";
$xylsinfo_fn="xylsinfo";
$rdlsinfo_fn="rdlsinfo";
$wcsinfo_fn="wcsinfo";
$jobdata_fn = "jobdata.db";
$fits2xyout_fn = "fits2xy.out";

$unitmap = array('arcsecperpix' => "arcseconds per pixel",
				 'arcminperpix' => "arcminutes per pixel",
				 'arcminwidth' => "width of the field (in arcminutes)", 
				 'degreewidth' => "width of the field (in degrees)", 
				 'focalmm' => "focal length of the lens (for 35mm film equivalent sensor)", 
				 );

$formDefaults = array('x_col' => 'X',
					  'y_col' => 'Y',
					  'parity' => 2,
					  'fstype' => 'ul',
					  'fsl' => 0.1,
					  'fsu' => 180,
					  'index' => 'auto',
					  'poserr' => 1.0,
					  'tweak' => 0,
					  'tweak_order' => 2,
					  'imgurl' => "http://",
					  'fsunit' => 'degreewidth',
					  //'autoscale' => 1,
					  'skippreview' => '',
					  'justjobid' => '',
					  );

$valid_blurb = <<<END
<p>
<a href="http://validator.w3.org/check?uri=referer"><img
style="border:0"
src="valid-xhtml10.png"
alt="Valid XHTML 1.0 Strict" height="31" width="88" /></a>
<a href="http://jigsaw.w3.org/css-validator/check/referer"><img
style="border:0;width:88px;height:31px"
src="valid-css.png"
alt="Valid CSS!" /></a>
</p>
END;

$host  = $_SERVER['HTTP_HOST'];

$quads = $base . "quads/";

$resultdir = "/home/gmaps/ontheweb-data/";
$indexdir = "/home/gmaps/ontheweb-indexes/";

$sqlite = "sqlite";

$fits2xy = $quads . "fits2xy";
$plotxy = $quads . "plotxy";
$plotquad = $quads . "plotquad";
$plot_constellations = $quads . "plot-constellations";
$modhead = $quads . "modhead";
$tabsort = $quads . "tabsort";
$tablist = $quads . "tablist";
$tabmerge = $quads . "tabmerge";
$fitsgetext = $quads . "fitsgetext";
$fitscopy = $quads . "fitscopy";
$xylist2fits = $quads . "xylist2fits";
$mergesolved = $quads . "mergesolved";
$rdlsinfo = $quads . "rdlsinfo";
$xylsinfo = $quads . "xylsinfo";
$wcsinfo = $quads . "wcsinfo";
$printsolved = $quads . "printsolved";
$wcs_xy2rd = $quads . "wcs-xy2rd";
$wcs_rd2xy = $quads . "wcs-rd2xy";
$fits_guess_scale = $quads . "fits-guess-scale";
$get_wcs = $quads . "get-wcs";
$an_fitstopnm = $quads . "an-fitstopnm";
$fits_filter = $quads . "fits2fits.py %s %s";
$new_wcs = $quads . "new-wcs";

$headers = $_REQUEST;

$ontheweblogfile = $resultdir . $logfile;
function loggit($mesg) {
	global $ontheweblogfile;
	error_log($mesg, 3, $ontheweblogfile);
}

function fail($msg) {
	loggit($msg);
	die($msg);
}

$highlevellogfile = $resultdir . $highlevelfile;
function highlevellog($msg) {
	global $highlevellogfile;
	error_log($msg, 3, $highlevellogfile);
}

function create_new_jobid() {
	global $siteid;
	global $resultdir;
	$yrmonth = date("Ym");
	$dir = $resultdir . $siteid . "/" . $yrmonth;
	if (!file_exists($dir)) {
		if (!mkdir($dir, 0775, TRUE)) {
			loggit("Failed to create directory " . $dir . "\n");
			return FALSE;
		}
	}
	$num = create_random_dir($dir . "/");
	if (!$num) {
		loggit("Failed to create a random directory.  Base dir " . $dir . "\n");
		return FALSE;
	}
	return $siteid . "-" . $yrmonth . "-" . $num;
}

$sitepat = '/^\w{3,8}$/';
$epochpat = '/^\d{6}$/';
$numpat = '/^\d{8}$/';

function verify_jobid($jobid) {
	$pat_old = '/^[0-9a-f]{10}$/';
	$pat_new = '/^\w{3,8}-\d{6}-\d{8}$/';
	return ((preg_match($pat_new, $jobid) == 1) ||
			(preg_match($pat_old, $jobid) == 1));
}

function jobid_split($jobid, &$site, &$epoch, &$num) {
	$pat_new = '/^(\w{3,8})-(\d{6})-(\d{8})$/';
	if (preg_match($pat_new, $jobid, $matches) != 1)
		return FALSE;
	if (count($matches) != 4)
		return FALSE;
	$site = $matches[1];
	$epoch = $matches[1];
	$num = $matches[1];
	return TRUE;
}

function jobid_to_dir($jobid) {
	return strtr($jobid, "-", "/");
}

function dir_to_jobid($dir) {
	return strtr($dir, "/", "-");
}

function fmod_positive($val, $divisor) {
	$val = fmod($val, $divisor);
	while ($val < 0)
		$val += $divisor;
	while ($val > $divisor)
		$val -= $divisor;
	return $val;
}

function ra_deg2hms($ra, &$h, &$m, &$s) {
	$ra = fmod_positive($ra, 360);
	$hrs = $ra / 15.0;
	$h = floor($hrs);
	// remaining hours:
	$hrs -= $h;
	$mins = $hrs * 60.0;
	$m = floor($mins);
	// remaining minutes:
	$mins -= $m;
	$secs = $mins * 60.0;
	$s = $secs;
}

function dec_deg2dms($dec, &$d, &$m, &$s) {
	$sign = ($dec >= 0) ? 1 : -1;
	$dec = $dec * $sign;
	$d = $sign * floor($dec);
	// remaining degrees:
	$dec -= abs($d);
	$mins = $dec * 60.0;
	$m = floor($mins);
	// remaining minutes:
	$mins -= $m;
	$secs = $mins * 60.0;
	$s = $secs;
}

// Returns FALSE on error;
// 
function uncompress_file($infile, $outfile, &$suffix) {
	$typestr = shell_exec("file -b -N -L " . $infile);
	loggit("type: " . $typestr . " for " . $infile . "\n");

	// handle compressed files.
	$comptype = array("gzip compressed data"  => array(".gz",  "gunzip  -cd %s > %s"),
                      "compress'd data"       => array(".Z",   "gunzip  -cd %s > %s"),
					  "bzip2 compressed data" => array(".bz2", "bunzip2 -cd %s > %s"),
					  "Zip archive data"      => array(".zip", "unzip   -p  %s > %s"),
					  );
	// look for key phrases in the output of "file".
	foreach ($comptype as $phrase => $lst) {
		if (!strstr($typestr, $phrase)) {
			continue;
		}
		$suff    = $lst[0];
		$command = $lst[1];
		$cmd = sprintf($command, $infile, $outfile);
		loggit("Command: " . $cmd . "\n");
		if ((system($cmd, $retval) === FALSE) || $retval) {
			loggit("Command failed, return value " . $retval . ": " . $cmd);
			return FALSE;
		}
		$suffix = $suff;
		break;
	}
	return TRUE;
}

function get_status_url_args($jobid, $file) {
	return '?job=' . $jobid . "&get=" . $file;
}

function get_datestr($t) {
	// "c" -> 2007-03-12T22:49:53+00:00
	$datestr = date("c", $t);
	// Make Hogg happy
	if (substr($datestr, -6) == "+00:00")
		$datestr = substr($datestr, 0, strlen($datestr)-6) . "Z";
	return $datestr;
}

function isodate_to_timestamp($datestr) {
	$d = strptime($datestr, '%Y-%m-%dT%H:%M:%S');
	if (!$d) {
		return 0;
	}
	if ($d['unparsed'] == 'Z') {
	} else if ($d['unparsed'] == '+0000') {
	} else {
		// FIXME
		loggit("isodate_to_timestamp: unparsed part is " . $d['unparsed'] . "\n");
	}
	$t = mktime($d['tm_hour'], $d['tm_min'], $d['tm_sec'], $d['tm_mon']+1,
				$d['tm_mday'], $d['tm_year'] + 1900);
	return $t;
}

function dtime2str($secs) {
	if ($secs > 3600*24) {
		return sprintf("%.1f days", (float)$secs/(float)(3600*24));
	} else if ($secs > 3600) {
		return sprintf("%.1f hours", (float)$secs/(float)(3600));
	} else if ($secs > 60) {
		return sprintf("%.1f minutes", (float)$secs/(float)(60));
	} else {
		return sprintf("%d seconds", $secs);
	}
}

function format_preset_url_from_form($formvals, $defaults=array()) {
	global $fitsfile;
	global $imgfile;

	switch ($formvals['xysrc']) {
	case 'img':
		$imgval = $imgfile->getValue();
		if ($imgval) {
			$formvals['image-origname'] = $imgval['name'];
		}
		break;
	case 'fits':
		$fitsval = $fitsfile->getValue();
		if ($fitsval) {
			$vals['fits-origname'] = $fitsval['name'];
		}
		break;
	}
	$args = format_preset_url($formvals, $defaults);
	return $args;
}

function format_preset_url($jd, $defaults=array()) {
	$args = "";
	$flds = array('xysrc', 'fstype', 'fsl', 'fsu', 'fse', 'fsv',
				  'fsunit', 'parity', 'poserr', 'index',
				  'uname', 'email');
	switch ($jd["xysrc"]) {
	case "url":
		array_push($flds, 'imgurl');
		break;
	case "img":
		$imgname = $jd['image-origname'];
		if ($imgname) {
			$args .= '&imgfile=' . $imgname;
		}
		break;
	case "fits":
		$fitsname = $jd['fits-origname'];
		if ($fitsname) {
			$args .= '&fitsfile=' . $fitsname;
		}
		array_push($flds, 'x_col');
		array_push($flds, 'y_col');
		break;
	}
	if ($jd['tweak']) {
		$args = "&tweak=1" . $args;
		array_push($flds, 'tweak_order');
	}
	foreach ($flds as $fld) {
		if ($jd[$fld] != $defaults[$fld])
			$args .= "&" . urlencode($fld) . "=" . urlencode($jd[$fld]);
	}
	return "?" . substr($args, 1);
}

function describe_job($jd, $user=FALSE) {
	global $unitmap;

	$strs = array();
	if ($user) {
		$uname = $jd['uname'];
		if ($uname) {
			$strs['Name'] = $uname;
		}
		$email = $jd['email'];
		if ($email) {
			$strs['Email'] = $email;
		}
	}
	switch ($jd['xysrc']) {
	case 'url':
		$url = $jd['imgurl'];
		if (strstr($url, 'http://http://')) {
			$url = substr($url, 7);
		}
		$strs['Image URL'] = $url;
		break;
	case 'img':
		$strs['Image file'] = $jd['image-origname'];
		break;
	case 'fits':
		$strs['FITS file'] = $jd['fits-origname'];
		break;
	}
	if ($jd['x_col'] != 'X') {
		$strs['X column name'] = $jd['x_col'];
	}
	if ($jd['y_col'] != 'Y') {
		$strs['Y column name'] = $jd['y_col'];
	}
	if ($jd['fsunit']) {
		$strs['Field size units'] = $unitmap[$jd['fsunit']];
	}
	if ($jd['fstype'] == 'ul') {
		$strs['Field size lower bound'] = $jd['fsl'];
		$strs['Field size upper bound'] = $jd['fsu'];
	} else {
		$strs['Field size estimate'] = $jd['fse'];
		$strs['Field size error'] = $jd['fsv'] . "%";
	}
	$strs['Tweak'] = $jd['tweak'] ? "yes" : "no";
	if ($jd['tweak'] && strlen($jd['tweak_order'])) {
		$strs['Tweak Polynomial Order'] = $jd['tweak_order'];
	}
	$strs['Field Positional Error'] = $jd['poserr'] . " pixels";
	$strs['Index'] = $jd['index'];
	switch ($jd['parity']) {
	case '0':
		$strs['Parity'] = 'Positive';
		break;
	case '1':
		$strs['Parity'] = 'Negative';
		break;
	case '2':
		$strs['Parity'] = 'Try both';
		break;
	}
	return $strs;
}

function get_shrink_factor($W, $H) {
	// choose a power-of-two shrink factor that makes the larger dimension <= 800.
	$maxsz = 800;
	$bigger = max($W,$H);
	if ($bigger > $maxsz) {
		$shrink = pow(2, ceil(log($bigger / $maxsz, 2)));
	} else {
		$shrink = 1;
	}
	return $shrink;
}

function create_random_dir($basedir) {
    while (TRUE) {
        // Generate a random number with up to 8 digits.
		$rnd = mt_rand(0, 99999999);
		$myname = sprintf("%08d", $rnd);
        // See if that dir already exists.
        $mydir = $basedir . $myname;
        if (!file_exists($mydir)) {
			break;
		}
    }
	if (!mkdir($mydir)) {
		return FALSE;
	}
	return $myname;
}

/*
"pecl install sqlite" is fscked.

function create_db($dbpath) {
	$db = new SQLiteDatabase($dbpath, 0664, $error);
	if ($error) {
		loggit("Error creating SQLite db: $error\n");
		return FALSE;
	}
	return TRUE;
}
*/

// FIXME - it's possible that this function doesn't actually cause the db file to be
// created!!
function create_db($dbpath) {
	global $sqlite;
	// create database.
	$cmd = $sqlite . " " . $dbpath . " .exit";
	loggit("Command: " . $cmd . "\n");
	$res = system($cmd, $retval);
	if (($res === FALSE) || $retval) {
		loggit("Command failed: return val " . $retval . ", str " . $res . "\n");
		return FALSE;
	}
	return TRUE;
}

function connect_db($dbpath, $quiet=FALSE) {
	// Connect to database
	$dsn = array('phptype'  => 'sqlite', 'database' => $dbpath,
				 'mode'     => '0644', );
	//$options = array('debug'       => 2, 'portability' => DB_PORTABILITY_ALL,);
	$options = array();
	$db =& MDB2::connect($dsn, $options);
	if (PEAR::isError($db)) {
		if (!$quiet) {
			loggit("Error connecting to SQLite db $dbpath: " . $db->getMessage() . "\n");
		}
		return FALSE;
	}

	// chmod it!
	if (!chmod($dbpath, 0664)) {
		loggit("Failed to chmod database " . $dbpath . "\n");
	}

	return $db;
}

function disconnect_db($db) {
	$db->disconnect();
}

function create_jobdata_table($db) {
	// Create table.
	$q = "CREATE TABLE jobdata (key UNIQUE, val);";
	loggit("Query: " . $q . "\n");
	$res =& $db->exec($q);
	if (PEAR::isError($res)) {
		loggit("Error creating table: " . $db->getMessage() . "\n");
		return FALSE;
	}
	//loggit("Created table.\n");
	return TRUE;
}

function getjobdata($db, $key) {
	$res =& $db->query('SELECT val FROM jobdata WHERE key = "' . $key . '"');
	if (PEAR::isError($res)) {
		loggit("Database error: " . $res->getMessage() . "\n");
		return FALSE;
	}
	$row = $res->fetchRow();
	if (!$row) {
		return FALSE;
	}
	$rtn = $row[0];
	$res->free();
	return $rtn;
}

function getalljobdata($db, $quiet=FALSE) {
	$res =& $db->query('SELECT key, val FROM jobdata');
	if (PEAR::isError($res)) {
		if (!$quiet) {
			loggit("Database error: " . $res->getMessage() . "\n");
		}
		return FALSE;
	}
	while (($row = $res->fetchRow())) {
		$jd[$row[0]] = $row[1];
	}
	return $jd;
}

function setjobdata($db, $vals) {
	foreach ($vals as $key => $val) {
		$q = "REPLACE INTO jobdata VALUES ('" . $key . "', '" . $db->escape($val, TRUE) . "');";
		loggit("Query: " . $q . "\n");
		$res =& $db->exec($q);
		if (PEAR::isError($res)) {
			loggit("Database error: " . $res->getMessage() . "\n");
			return FALSE;
		}
	}
	return TRUE;
}

function validate_url($url) {
	$parts = @parse_url($url);
	if ($parts === FALSE)
		return FALSE;
	if (!(($parts['scheme'] == 'http') || ($parts['scheme'] == 'ftp')))
		return FALSE;
	if ($parts['fragment'])
		return FALSE;
	return TRUE;
}

function write_download_status($id, $stats) {
	global $upload_progress;
	$outfile = sprintf($upload_progress, $id);
	$outfile_tmp = $outfile . '.tmp';
	$f = @fopen($outfile_tmp, "wb");
	if (!$f) {
		loggit("Failed to open download stats file " . $outfile_tmp . "\n");
		return FALSE;
	}
	//loggit("Writing stats to " . $outfile_tmp . ":\n");
	foreach ($stats as $k => $v) {
		fprintf($f, "%s", $k . "=" . $v . "\n");
		//loggit("  " . $k . " = " . $v . "\n");
	}
	fclose($f);
	if (!rename($outfile_tmp, $outfile)) {
		loggit("Failed to rename download stats file from " . $outfile_tmp . " to " . $outfile . "\n");
		return FALSE;
	}
	return TRUE;
}

function download_url($url, $dest, $maxfilesize, &$errmsg, $id=FALSE) {
	$fin = @fopen($url, "rb");
	if (!$fin) {
		$errmsg = "failed to open URL " . $url;
		return FALSE;
	}
	$fout = fopen($dest, "wb");
	if (!$fout) {
		$errmsg = "failed to open output file " . $dest;
		return FALSE;
	}

	// For http:// URLs, look for the "Content-Length:" header.
	$meta_data = stream_get_meta_data($fin);
	if ($meta_data) {
		$pat = '/^Content-Length: (\d+)$/';
		foreach ($meta_data['wrapper_data'] as $k=>$v) {
			if (preg_match($pat, $v, $matches)) {
				$length = (int)$matches[1];
				break;
			}
		}
	}
	if ($length) {
		loggit("Found content-length " . $length . "\n");
	}
	if ($id) {
		loggit("Id " . $id . "\n");
		$tstart = time();
		$tlast = $tstart;
		$blast = 0;
		$stats['time_start'] = $tstart;
	}

	$nr = 0;
	$blocksize = 1024;
	for ($i=0; $i<$maxfilesize/$blocksize; $i++) {

		if ($id) {
			$tnow = time();
			if (($tnow - $tlast) > 1) {
				// update stats.
				$bnow = $nr;
				$stats['time_last'] = $tnow;
				$stats['bytes_uploaded'] = $bnow;
				if ($length)
					$stats['bytes_total'] = $length;
				if ($length && $bnow)
					$stats['est_sec'] = ($length - $bnow) * ($tnow - $tstart) / $bnow;
				$stats['speed_average'] = $bnow / ($tnow - $tstart);
				$stats['speed_last'] = ($bnow - $blast) / ($tnow - $tlast);
				write_download_status($id, $stats);
				$tlast = $tnow;
			}
		}

		$block = fread($fin, $blocksize);
		if ($block === FALSE) {
			$errmsg = "failed to read from URL " . $url;
			return FALSE;
		}
		if (fwrite($fout, $block) === FALSE) {
			$errmsg = "failed to write to output file " . $dest;
			return FALSE;
		}
		$nr += strlen($block);
		if (strlen($block) < $blocksize)
			if (feof($fin)) {
				break;
			}
	}
	if ($i == $maxfilesize/$blocksize) {
		$errmsg = "URL was too big to download.\n";
		return FALSE;
	}
	fclose($fin);
	fclose($fout);

	if ($id) {
		// update stats.
		$tnow = time();
		$bnow = $nr;
		$stats['time_last'] = $tnow;
		$stats['bytes_uploaded'] = $bnow;
		if ($length)
			$stats['bytes_total'] = $length;
		if ($length && $bnow)
			$stats['est_sec'] = ($length - $bnow) * ($tnow - $tstart) / $bnow;
		if ($tnow != $tstart)
			$stats['speed_average'] = $bnow / ($tnow - $tstart);
		if ($tnow != $tlast)
			$stats['speed_last'] = ($bnow - $blast) / ($tnow - $tlast);
		$stats['files_uploaded'] = 1;
		write_download_status($id, $stats);
		$tlast = $tnow;
	}

	return $nr;
}

// RA in degrees
function ra2merc($ra) {
	return $ra / 360.0;
}
// DEC in degrees
function dec2merc($dec) {
	return 0.5 + (asinh(tan($dec * M_PI/180.0)) / (2.0*M_PI));
}
// Returns RA in degrees.
function merc2ra($mx) {
	return $mx * 360.0;
}
// Returns DEC in degrees.
function merc2dec($my) {
	return atan(sinh(($my - 0.5) * (2.0*M_PI))) * 180.0/M_PI;
}

function field_solved($solvedfile, &$msg) {
	if (!file_exists($solvedfile)) {
		return FALSE;
	}
	$contents = file_get_contents($solvedfile);
	return (strlen($contents) && (ord($contents[0]) == 1));
}

?>
