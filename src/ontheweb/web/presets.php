<?php
require_once 'MDB2.php';
require_once 'common.php';

$preset_db_path = $resultdir . "presets.db";

$preset_cols = array('fstype', 'fsl', 'fsu', 'fse', 'fsv', 'fsunit',
					 'poserr', 'tweak');
// jobdata => preset
$preset_special_cols = array('index'  => 'theindex',
							 'imgurl' => 'url');


function create_presets_table($db, $dbpath) {
	global $preset_cols;
	global $preset_special_cols;
	$q = 'CREATE TABLE presets (name UNIQUE';
	foreach ($preset_cols as $col) {
		$q .= "," . $col;
	}
	foreach ($preset_special_cols as $x => $col) {
		$q .= "," . $col;
	}
	$q .= ');';
	loggit("Query: " . $q . "\n");
	$res =& $db->exec($q);
	if (PEAR::isError($res)) {
		loggit("Failed to create presets table: " . $res->getMessage() . "\n");
		return FALSE;
	}
	// chmod it!
	if (!chmod($dbpath, 0664)) {
		loggit("Failed to chmod database " . $dbpath . "\n");
	}
	return TRUE;
}

function connect_presets_db() {
	global $preset_db_path;
	$create = !file_exists($preset_db_path);
	// Connect to database
	$dsn = array('phptype'  => 'sqlite', 'database' => $preset_db_path,
				 'mode'     => '0644', );
	$options = array();
	$db =& MDB2::connect($dsn, $options);
	if (PEAR::isError($db)) {
		loggit("Error connecting to presets db " . $preset_db_path . ": " . $db->getMessage() . "\n");
		return FALSE;
	}
	if ($create) {
		if (!create_presets_table($db, $preset_db_path)) {
			return FALSE;
		}
	}
	return $db;
}

function disconnect_presets_db($db) {
	$db->disconnect();
}

function add_preset($name, $jd) {
	global $preset_cols;
	global $preset_special_cols;
	$db = connect_presets_db();
	if (!$db) {
		loggit("Failed to connect to presets database.\n");
		die("Failed to connect to presets database.\n");
	}
	// Check if this preset name already exists:
	$q = "SELECT name FROM presets WHERE name = '" . $db->escape($name, TRUE) . "'";
	loggit("Query: " . $q . "\n");
	$res1 =& $db->query($q);
	if (PEAR::isError($res1)) {
		die("Failed to retrieve preset from database: " . $res1->getMessage() . "\n");
		return FALSE;
	}
	$row = $res1->fetchRow();
	if ($row) {
		// a row exists - this is a duplicate preset name!
		die("Preset name \"" . $name . "\" has already been taken.  Please choose a " .
			"new name for your preset and try again.");
	}

	$cols = "name";
	$vals = "'" . $db->escape($name, TRUE) . "'";
	foreach ($preset_cols as $col) {
		$val = $jd[$col];
		if ($val) {
			$cols .= "," . $col;
			$vals .= ",'" . $db->escape($val, TRUE) . "'";
		}
	}
	foreach ($preset_special_cols as $jdcol=>$col) {
		$val = $jd[$jdcol];
		if ($val) {
			$cols .= "," . $col;
			$vals .= ",'" . $db->escape($val, TRUE) . "'";
		}
	}
	$q = "INSERT INTO presets(" . $cols . ") VALUES (" . $vals . ");";
	loggit("Query: " . $q . "\n");
	$res =& $db->exec($q);
	if (PEAR::isError($res)) {
		print_r($res);
		die("Failed to insert preset into database: " . $res->getMessage() . "\n");
		return FALSE;
	}
	disconnect_presets_db($db);
	return TRUE;
}

function get_preset($name) {
	global $preset_cols;
	global $preset_special_cols;
	$db = connect_presets_db();
	if (!$db) {
		die("Failed to connect to presets database.\n");
	}
	$jdcols = array();
	$cols = "";
	foreach ($preset_cols as $c) {
		array_push($jdcols, $c);
		$cols .= "," . $c;
	}
	foreach ($preset_special_cols as $jdc=>$c) {
		array_push($jdcols, $jdc);
		$cols .= "," . $c;
	}
	// pull off the initial ",".
	$cols = substr($cols, 1);

	$q = "SELECT " . $cols . " FROM presets WHERE name = '" . 
		$db->escape($name, TRUE) . "';";
	loggit("Query: " . $q . "\n");
	$res =& $db->query($q);
	if (PEAR::isError($res)) {
		die("Failed to retrieve preset from database: " . $res->getMessage() . "\n");
		return FALSE;
	}
	$row = $res->fetchRow();
	if (!$row) {
		return FALSE;
	}
	$jd = array();
	foreach ($row as $i => $v) {
		$jd[$jdcols[$i]] = $v;
	}
	$res->free();
	disconnect_presets_db($db);
	loggit("Got preset vals:\n");
	foreach ($jd as $k=>$v) {
		loggit("  " . $k . " = " . $v . "\n");
	}
	$jd['xysrc']='url';
	$args = format_preset_url($jd);
	loggit("Formatted: " . $args . "\n");
	return $args;
}

/* Old-fashioned presets:
$blind_presets = 
array("apod-2007-03-09" =>
	  "xysrc=url&fstype=ul&fsl=30&fsu=60&poserr=2&" .
	  "imgurl=http://antwrp.gsfc.nasa.gov/apod/image/0703/tle070303_legault_legend.jpg",

	  "apod-2007-03-08" =>
	  "xysrc=url&fstype=ev&fse=1.5&fsv=50&poserr=1&tweak=1&" .
	  "imgurl=http://antwrp.gsfc.nasa.gov/apod/image/0703/tle_03032007_60_schedler720.jpg",

	  "apod-2007-03-08-fast" =>
	  "xysrc=url&fstype=ev&fse=1.5&fsv=10&poserr=1&index=2degree&tweak=1&" .
	  "imgurl=http://antwrp.gsfc.nasa.gov/apod/image/0703/tle_03032007_60_schedler720.jpg",

	  "apod-2007-02-26" =>
	  "xysrc=url&fstype=ul&fsl=10&fsu=30&tweak=1&" .
	  "imgurl=http://antwrp.gsfc.nasa.gov/apod/image/0702/debrismovie_mcnaught.jpg",

	  "apod-2007-02-26-fast" =>
	  "xysrc=url&fstype=ul&fsl=10&fsu=30&tweak=1&index=8degree&" .
	  "imgurl=http://antwrp.gsfc.nasa.gov/apod/image/0702/debrismovie_mcnaught.jpg",

	  "apod-2007-02-16" =>
	  "xysrc=url&fstype=ev&fse=20&fsv=50&fsunit=arcminwidth&tweak=1&" .
	  "imgurl=http://antwrp.gsfc.nasa.gov/apod/image/0702/NGC2685_crawford720.jpg",

	  "apod-2007-02-28" =>
	  "xysrc=url&fstype=ul&fsl=30&fsu=45&fsunit=arcminwidth&tweak=1&" .
	  "imgurl=http://antwrp.gsfc.nasa.gov/apod/image/0702/ngc2170_croman.jpg",

	  "apod-2007-02-28-fast" =>
	  "xysrc=url&fstype=ul&fsl=35&fsu=40&fsunit=arcminwidth&tweak=1&index=12arcmin&" .
	  "imgurl=http://antwrp.gsfc.nasa.gov/apod/image/0702/ngc2170_croman.jpg",

	  "apod-2007-02-14" =>
	  "xysrc=url&fstype=ul&fsl=1&fsu=2&tweak=1&" .
	  "imgurl=http://antwrp.gsfc.nasa.gov/apod/image/0702/rosette_gendler.jpg",

	  "apod-2007-02-14-fast" =>
	  "xysrc=url&fstype=ul&fsl=1.8&fsu=1.9&index=1degree&tweak=1&" .
	  "imgurl=http://antwrp.gsfc.nasa.gov/apod/image/0702/rosette_gendler.jpg",

	  "apod-2007-01-04" =>
	  "xysrc=url&fstype=ev&fse=3&fsv=50&tweak=1&" .
	  "imgurl=http://antwrp.gsfc.nasa.gov/apod/image/0701/gammacyg_parkercarboni720.jpg",

	  "apod-2007-01-06" =>
	  "xysrc=url&fstype=ev&fse=5&fsv=50&tweak=1&" .
	  "imgurl=http://antwrp.gsfc.nasa.gov/apod/image/0701/oriondeepwide_gendler720.jpg",

	  "apod-2007-01-06-fast" =>
	  "xysrc=url&fstype=ev&fse=7&fsv=10&index=4degree&tweak=1&" .
	  "imgurl=http://antwrp.gsfc.nasa.gov/apod/image/0701/oriondeepwide_gendler720.jpg",

	  "apod-2007-01-08" =>
	  "xysrc=url&fstype=ul&fsl=20&fsu=90&poserr=1&" .
	  "imgurl=http://antwrp.gsfc.nasa.gov/apod/image/0701/dippers_lodriguss_labels.jpg",

	  "apod-2007-01-08-fast" =>
	  "xysrc=url&fstype=ul&fsl=60&fsu=75&poserr=1&" .
	  "imgurl=http://antwrp.gsfc.nasa.gov/apod/image/0701/dippers_lodriguss_labels.jpg",

	  "apod-2007-03-19" =>
	  "xysrc=url&fstype=ul&fsl=20&fsu=30&poserr=1&" .
	  "imgurl=http://antwrp.gsfc.nasa.gov/apod/image/0703/hickson44_masil.jpg",

 	  "apod-2007-03-21-fast" =>
	  "xysrc=url&fstype=ul&fsl=30&fsu=40&fsunit=arcminwidth&poserr=1&index=30arcmin&tweak=1&" .
	  "imgurl=http://antwrp.gsfc.nasa.gov/apod/image/0703/barnard163_noao.jpg",

	  "apod-2007-03-21" =>
	  "xysrc=url&fstype=ul&fsl=30&fsu=60&fsunit=arcminwidth&poserr=1&tweak=1&" .
	  "imgurl=http://antwrp.gsfc.nasa.gov/apod/image/0703/barnard163_noao.jpg",

	  "apod-2007-01-12" =>
	  "xysrc=url&fstype=ev&fse=2&fsv=50&poserr=1&tweak=1&" .
	  "imgurl=http://antwrp.gsfc.nasa.gov/apod/image/0701/coathanger_barrett.jpg",

	  "apod-2007-01-22" =>
	  "xysrc=url&fstype=ul&fsl=20&fsu=60&poserr=1&tweak=1&" .
	  "imgurl=http://antwrp.gsfc.nasa.gov/apod/image/0701/comet_mcnaught.jpg",

	  "apod-2007-01-25" =>
	  "xysrc=url&fstype=ev&fse=13&fsv=20&poserr=1&tweak=0&" .
	  "imgurl=http://antwrp.gsfc.nasa.gov/apod/image/0701/orioncradle_hallas_r780.jpg",
	  );
*/

	  /* HST:
           http://antwrp.gsfc.nasa.gov/apod/ap070305.html
           http://antwrp.gsfc.nasa.gov/apod/ap070218.html
           http://antwrp.gsfc.nasa.gov/apod/ap070208.html
           http://antwrp.gsfc.nasa.gov/apod/ap070126.html
           http://antwrp.gsfc.nasa.gov/apod/ap070110.html
           
	  */

	  /* Xray:
           http://antwrp.gsfc.nasa.gov/apod/ap070224.html
           
	  */
	  /* Infrared:
           http://antwrp.gsfc.nasa.gov/apod/ap070210.html
           http://antwrp.gsfc.nasa.gov/apod/ap070202.html
           http://antwrp.gsfc.nasa.gov/apod/ap070121.html
           http://antwrp.gsfc.nasa.gov/apod/ap070111.html
           
	  */

	  /* Should be doable:
           http://antwrp.gsfc.nasa.gov/apod/ap070228.html
           http://antwrp.gsfc.nasa.gov/apod/ap070223.html
           http://antwrp.gsfc.nasa.gov/apod/ap070222.html
           http://antwrp.gsfc.nasa.gov/apod/ap070221.html
           http://antwrp.gsfc.nasa.gov/apod/ap070219.html
           http://antwrp.gsfc.nasa.gov/apod/ap070213.html
           http://antwrp.gsfc.nasa.gov/apod/ap070203.html
           http://antwrp.gsfc.nasa.gov/apod/ap070124.html

         Tried:
           http://antwrp.gsfc.nasa.gov/apod/ap070130.html
           http://antwrp.gsfc.nasa.gov/apod/ap070315.html
           
	   */
	  /* A stretch?:
           http://antwrp.gsfc.nasa.gov/apod/ap070215.html
           http://antwrp.gsfc.nasa.gov/apod/ap070212.html
           http://antwrp.gsfc.nasa.gov/apod/ap070123.html
           
	   */


?>
