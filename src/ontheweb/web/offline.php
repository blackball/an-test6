<?php
/*
 Web page for offline, bulk-processing of many FITS files.

 This page will ask for the user's email address and some information
 about the images: essentially, scale and jitter.

 When submitted, it will create a directory for the files, then offer a
 page with a command-line that can be copy-n-pasted into the terminal when
 the user is in the directory that contains the images.

 This script will download the fits2xy source, build it, then run it
 on all the FITS images in the current directory tree (?), upload these
 to the target directory (via this script), and the script will trigger a
 blind solver run for each image.
*/

//phpinfo();
require_once 'MDB2.php';

$resultdir = "/home/gmaps/ontheweb-data/";
$sqlite = "sqlite";

$check_xhtml = 1;

function loggit($mesg) {
	error_log($mesg, 3, "/tmp/offline.log");
}

function getval($db, $key) {
	$res =& $db->query('SELECT val FROM forms WHERE key = "' . $key . '"');
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
	$mydir = $mydir . "/";
	return $mydir;
}

$headers = $_REQUEST;

$have_email = array_key_exists("email", $headers);
$email_val = $have_email ? $headers["email"] : "";

$have_fu_lower = array_key_exists("fu_lower", $headers);
if ($have_fu_lower) {
	$fu_lower = (float)$headers["fu_lower"];
	if ($fu_lower <= 0) {
		$have_fu_lower = 0;
		$fu_lower = 0.0;
	}
} else {
	$fu_lower = 0.0;
}

$have_fu_upper = array_key_exists("fu_upper", $headers);
if ($have_fu_upper) {
	$fu_upper = (float)$headers["fu_upper"];
	if ($fu_upper <= 0) {
		$have_fu_upper = 0;
		$fu_upper = 0.0;
	}
} else {
	$fu_upper = 0.0;
}

$have_parity = array_key_exists("parity", $headers);
if ($have_parity) {
	$parity_val = ($headers["parity"] == "1") ? 1 : 0;
} else {
	$parity_val = 0;
}

$all_ok = $have_parity && $have_fu_lower && $have_fu_upper;
if ($all_ok) {
	$mydir = create_random_dir($resultdir);
	if (!$mydir) {
		echo "<html><body><h3>Failed to create temp dir.</h3></body></html>";
		exit;
	}
	$dbpath = $mydir . "db";

	// create database.
	$cmd = $sqlite . " " . $dbpath . " .exit";
	loggit("Command: " . $cmd . "\n");
	$res = FALSE;
	$res = system($cmd, $retval);
	if ($retval) {
		loggit("Command failed: return val " . $retval . ", str " . $res . "\n");
		die("Failed to create database.");
	}

	// Connect to database
	$dsn = array('phptype'  => 'sqlite', 'database' => $dbpath,
				 'mode'     => '0644', );
	//$options = array('debug'       => 2, 'portability' => DB_PORTABILITY_ALL,);
	$db =& MDB2::connect($dsn, $options);
	if (PEAR::isError($db)) {
		die($db->getMessage());
	}

	// Create table.
	$q = "CREATE TABLE forms (key UNIQUE, val);";
	loggit("Query: " . $q . "\n");
	$res =& $db->exec($q);
	if (PEAR::isError($res)) {
		die($res->getMessage());
	}

	$vals = array("email"    => $db->quote($email_val, 'text'),
				  "fu_lower" => $fu_lower,
				  "fu_upper" => $fu_upper,
				  "parity"   => $parity_val,);

	/* Doesn't seem to support inserting multiple values...
    $q = 'REPLACE INTO forms VALUES ';
	foreach ($vals as $key => $val) {
		$q .= '("' . $key . '", "' . $val . '"), ';
	}
	$q = rtrim($q, ", ");
	$q .= ";";
	loggit("Query: " . $q . "\n");
	$res =& $db->exec($q);
	if (PEAR::isError($res)) {
		die($res->getMessage());
	}
	*/

	foreach ($vals as $key => $val) {
		$q = 'REPLACE INTO forms VALUES ("' . $key . '", "' . $val . '");';
		loggit("Query: " . $q . "\n");
		$res =& $db->exec($q);
		if (PEAR::isError($res)) {
			die($res->getMessage());
		}
	}

	$db->disconnect();
	echo "<html><body>OK.</body></html>\n";
	exit;
}
?>

<!DOCTYPE html 
PUBLIC "-//W3C//DTD XHTML 1.0 Strict//EN"
"http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd">
<html xmlns="http://www.w3.org/1999/xhtml" xml:lang="en" lang="en">
<head>
<title>
Astrometry.net: Bulk-processing Edition
</title>
<style type="text/css">
<!-- 
input.redinput {
	background-color: pink;
}
-->
</style>
</head>

<body>

<hr />

<h2>
Astrometry.net: Bulk-processing Edition
</h2>

<hr />

<form id="offlineform" action="offline.php" method="post">

<p>Your email address (optional; for notification when your job is done):
<input type="text" name="email" size="30" 
<?php
echo 'value="' . $email_val . '"';
?>
 />
</p>

<p>Scale of your images, in arcseconds per pixel:</p>
<ul>
<li>
Lower bound: <input type="text" name="fu_lower" size="10" 
<?php
echo "value=";
if ($have_fu_lower) {
	echo '"' . $fu_lower . '"';
} else {
	echo '""';
}
?>
 />
</li>
<li>
Upper bound: <input type="text" name="fu_upper" size="10" 
<?php
echo "value=";
if ($have_fu_upper) {
	echo '"' . $fu_upper . '"';
} else {
	echo '""';
}
?>
 />
</li>
</ul>

<p>
"Handedness" of your images:
</p>

<ul>
<li>
<input type="radio" name="parity" value="0" 
<?php
if (!$parity_val) {
	echo 'checked="checked"';
}
?>
/>"Right-handed": North-Up, East-Right 
</li>
<li>
<input type="radio" name="parity" value="1" 
<?php
if ($parity_val) {
	echo 'checked="checked"';
}
?>
/>"Left-handed": North-Up, West-Right
</li>
</ul>

<p>
<input type="submit" value="Submit" />
</p>

</form>

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

