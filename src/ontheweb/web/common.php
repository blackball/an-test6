<?php
require_once 'MDB2.php';
require_once 'PEAR.php';

$resultdir = "/home/gmaps/ontheweb-data/";
$gmaps_url = "http://oven.cosmo.fas.nyu.edu/usnob/";
$statuspath = "status/";

// blind params
//$maxtime = 120; // Two minute max!
$totaltime = 600; // Five minute total max.
$maxtime = 0;
$maxquads = 0; // 1000000;
$maxcpu = 0;

$index_template = "index-template.html";
$index_header   = "index-header.html";
$index_tail     = "index-tail.html";
$submitted_html = "submitted.html";

$q_fn = "queue";

$input_fn = "input"; // note: this must agree with "blindscript.sh"/"watcher".
$inputtmp_fn = "input.tmp";
$start_fn = "start";
$done_fn  = "done";
$donescript_fn  = "donescript"; // note: this must agree with "blindscript.sh"
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
$objs_fn  = "objs.png";
$overlay_fn="overlay.png";
$xylsinfo_fn="xylsinfo";
$rdlsinfo_fn="rdlsinfo";
$wcsinfo_fn="wcsinfo";
$jobdata_fn = "jobdata.db";
$fits2xyout_fn = "fits2xy.out";
//$userimage_fn = "userimage";

$unitmap = array('arcsecperpix' => "arcseconds per pixel",
				 'arcminperpix' => "arcminutes per pixel",
				 'arcminwidth' => "width of the field (in arcminutes)", 
				 'degreewidth' => "width of the field (in degrees)", 
				 'focalmm' => "focal length of the lens (for 35mm film equivalent sensor)", 
				 );

$formDefaults = array('x_col' => 'X',
					  'y_col' => 'Y',
					  'parity' => 2,
					  'index' => 'auto',
					  'poserr' => 1.0,
					  //'tweak' => 1,
					  'tweak' => 0,
					  'imgurl' => "http://",
					  'fsunit' => 'degreewidth',
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

if (strpos($host, "monte") === 0) {
	$sqlite = "/h/260/dstn/software/sqlite-2.8.17/bin/sqlite";
	#$resultdir = "/tmp/ontheweb-data/";
	#$indexdir = "/tmp/ontheweb-indexes/";
	$resultdir = "/h/260/dstn/local/ontheweb-results/";
	$indexdir = "/h/260/dstn/local/ontheweb-indexes/";
	$fits2xy = "~/an/simplexy/fits2xy";
	$plotxy2 = "plotxy2";
	$plotquad = "plotquad";
	$modhead = "modhead";
	$tabsort = "tabsort";
	$tabmerge = "tabmerge";
	$tablist = "tablist";
	$mergesolved = "mergesolved";
	$fitsgetext = "fitsgetext";
	$fitscopy = "fitscopy";
	$rdlsinfo = "rdlsinfo";
	$xylsinfo = "/h/260/dstn/an/quads/xylsinfo";
	$wcsinfo = "wcsinfo";
	$printsolved = "printsolved";
	$wcs_xy2rd = "wcs-xy2rd";
	$wcs_rd2xy = "wcs-rd2xy";
	$fits_guess_scale = "fits-guess-scale";
	$an_fitstopnm = "an-fitstopnm";
} else {
	$sqlite = "sqlite";
	$resultdir = "/home/gmaps/ontheweb-data/";
	$indexdir = "/home/gmaps/ontheweb-indexes/";
	$fits2xy = "/home/gmaps/simplexy/fits2xy";
	$plotxy2 = "/home/gmaps/quads/plotxy2";
	$plotquad = "/home/gmaps/quads/plotquad";
	$modhead = "/home/gmaps/quads/modhead";
	$tabsort = "/home/gmaps/quads/tabsort";
	$tablist = "/home/gmaps/quads/tablist";
	$tabmerge = "/home/gmaps/quads/tabmerge";
	$fitsgetext = "/home/gmaps/quads/fitsgetext";
	$fitscopy = "/home/gmaps/quads/fitscopy";
	$mergesolved = "/home/gmaps/quads/mergesolved";
	$rdlsinfo = "/home/gmaps/quads/rdlsinfo";
	$xylsinfo = "/home/gmaps/quads/xylsinfo";
	$wcsinfo = "/home/gmaps/quads/wcsinfo";
	$printsolved = "/home/gmaps/quads/printsolved";
	$wcs_xy2rd = "/home/gmaps/quads/wcs-xy2rd";
	$wcs_rd2xy = "/home/gmaps/quads/wcs-rd2xy";
	$fits_guess_scale = "/home/gmaps/quads/fits-guess-scale";
	$an_fitstopnm = "/home/gmaps/quads/an-fitstopnm";
	$fits_filter = "/home/gmaps/quads/fits2fits.py %s %s";
}

$headers = $_REQUEST;

$ontheweblogfile = "/tmp/ontheweb.log";
function loggit($mesg) {
	global $ontheweblogfile;
	error_log($mesg, 3, $ontheweblogfile);
}

function get_datestr($t) {
	// "c" -> 2007-03-12T22:49:53+00:00
	$datestr = date("c", $t);
	// Make Hogg happy
	if (substr($datestr, -6) == "+00:00")
		$datestr = substr($datestr, 0, strlen($datestr)-6) . "Z";
	return $datestr;
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
	switch ($vals["xysrc"]) {
	case "img":
		$imgval = $imgfile->getValue();
		if ($imgval) {
			$vals['image-origname'] = $imgval['name'];
		}
		break;
	case "fits":
		$fitsval = $fitsfile->getValue();
		if ($fitsval) {
			$vals['fits-origname'] = $fitsval['name'];
		}
		array_push($flds, "x_col");
		array_push($flds, "y_col");
		break;
	}
	$args = format_preset_url($vals, $defaults);
	return $args;
}

function format_preset_url($jd, $defaults=array()) {
	$args = "";
	$flds = array('xysrc', 'fstype', 'fsl', 'fsu', 'fse', 'fsv',
				  'fsunit', 'parity', 'poserr', 'index',
				  'uname', 'email');
	switch ($jd["xysrc"]) {
	case "url":
		array_push($flds, "imgurl");
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
		array_push($flds, "x_col");
		array_push($flds, "y_col");
		break;
	}
	foreach ($flds as $fld) {
		if ($jd[$fld] != $defaults[$fld])
			$args .= "&" . urlencode($fld) . "=" . urlencode($jd[$fld]);
	}
	if ($jd['tweak']) {
		$args = "&tweak=1" . $args;
	}
	return "?" . substr($args, 1);
}

function describe_job($jd) {
	global $unitmap;

	$strs = array();
	switch ($jd['xysrc']) {
	case 'url':
		$strs['Image URL'] =  $jd['imgurl'];
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
		$strs['Field size upper bound'] = $jd['fsu'];
		$strs['Field size lower bound'] = $jd['fsl'];
	} else {
		$strs['Field size estimate'] = $jd['fse'];
		$strs['Field size error'] = $jd['fsv'] . "%";
	}
	$strs['Tweak'] = $jd['tweak'] ? "yes" : "no";
	$strs['Field Positional Error'] = $jd['poserr'] . " pixels";
	$strs['Index'] = $jd['index'];
	switch ($jd['parity']) {
	case '0':
		$strs['Parity'] = 'Right-handed';
		break;
	case '1':
		$strs['Parity'] = 'Left-handed';
		break;
	case '2':
		$strs['Parity'] = 'Try both';
		break;
	}
	return $strs;
}

function create_random_dir($basedir) {
    while (TRUE) {
        // Generate a random string.
        $str = '';
        for ($i=0; $i<5; $i++) {
            $str .= chr(mt_rand(0, 255));
        }
        // Convert it to hex.
        $myname = bin2hex($str);

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

function create_db($dbpath) {
	global $sqlite;
	// create database.
	$cmd = $sqlite . " " . $dbpath . " .exit";
	loggit("Command: " . $cmd . "\n");
	$res = FALSE;
	$res = system($cmd, $retval);
	if ($retval) {
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
		$q = 'REPLACE INTO jobdata VALUES ("' . $key . '", "' . $val . '");';
		loggit("Query: " . $q . "\n");
		$res =& $db->exec($q);
		if (PEAR::isError($res)) {
			loggit("Database error: " . $res->getMessage() . "\n");
			return FALSE;
		}
	}
	return TRUE;
}

function download_url($url, $dest, $maxfilesize, &$errmsg) {
	$fin = fopen($url, "rb");
	if (!$fin) {
		$errmsg = "failed to open URL " . $url;
		return FALSE;
	}
	$fout = fopen($dest, "wb");
	if (!$fout) {
		$errmsg = "failed to open output file " . $dest;
		return FALSE;
	}

	$nr = 0;
	$blocksize = 1024;
	for ($i=0; $i<$maxfilesize/$blocksize; $i++) {
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