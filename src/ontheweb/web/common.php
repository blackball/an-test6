<?php
$resultdir = "/home/gmaps/ontheweb-data/";
$gmaps_url = "http://oven.cosmo.fas.nyu.edu/usnob/";
$statuspath = "status/";

// blind params
$maxtime = 120; // Two minute max!
$maxquads = 0; // 1000000;
$maxcpu = 0;

$index_template = "index-template.html";

$q_fn = "queue";

$input_fn = "input";
$inputtmp_fn = "input.tmp";
$start_fn = "start";
$done_fn  = "done";
$donescript_fn  = "donescript";
$xyls_fn  = "field.xy.fits";
$rdls_fn  = "field.rd.fits";
$indexrdls_fn = "index.rd.fits";
$indexxyls_fn = "index.xy.fits";
$match_fn = "match.fits";
$log_fn   = "log";
$solved_fn= "solved";
$cancel_fn= "cancel";
$wcs_fn   = "wcs.fits";
$objs_fn  = "objs.png";
$overlay_fn="overlay.png";
$rdlsinfo_fn="rdlsinfo";
$jobdata_fn = "jobdata.db";

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
	//$fitsgetext = "fitsgetext";
	$rdlsinfo = "rdlsinfo";
	$xylsinfo = "/h/260/dstn/an/quads/xylsinfo";
	$printsolved = "printsolved";
	$wcs_xy2rd = "wcs-xy2rd";
	$wcs_rd2xy = "wcs-rd2xy";
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
	//$fitsgetext = "/home/gmaps/quads/fitsgetext";
	$mergesolved = "/home/gmaps/quads/mergesolved";
	$rdlsinfo = "/home/gmaps/quads/rdlsinfo";
	$xylsinfo = "/home/gmaps/quads/xylsinfo";
	$printsolved = "/home/gmaps/quads/printsolved";
	$wcs_xy2rd = "/home/gmaps/quads/wcs-xy2rd";
	$wcs_rd2xy = "/home/gmaps/quads/wcs-rd2xy";
}

$headers = $_REQUEST;

$ontheweblogfile = "/tmp/ontheweb.log";
function loggit($mesg) {
	global $ontheweblogfile;
	error_log($mesg, 3, $ontheweblogfile);
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

function connect_db($dbpath) {
	// Connect to database
	$dsn = array('phptype'  => 'sqlite', 'database' => $dbpath,
				 'mode'     => '0644', );
	//$options = array('debug'       => 2, 'portability' => DB_PORTABILITY_ALL,);
	$options = array();
	$db =& MDB2::connect($dsn, $options);
	if (PEAR::isError($db)) {
		loggit("Error connecting to SQLite db $dbpath: " .
			   $db->getMessage() . "\n");
		return FALSE;
	}
	return $db;
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
	return $row[0];
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

?>