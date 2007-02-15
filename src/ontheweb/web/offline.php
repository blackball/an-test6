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
$dbname = "test2.db";
$dbpath = $resultdir . $dbname;

function loggit($mesg) {
	error_log($mesg, 3, "/tmp/ontheweb.log");
}

$dsn = array(
    'phptype'  => 'sqlite',
    'database' => $dbpath,
    'mode'     => '0644',
);

$options = array(
    'debug'       => 2,
    //'portability' => DB_PORTABILITY_ALL,
);

$db =& MDB2::connect($dsn, $options);
if (PEAR::isError($db)) {
    die($db->getMessage());
}

$res =& $db->query('SELECT val FROM forms WHERE key = "key1"');
if (PEAR::isError($res)) {
    die($res->getMessage());
}

while (($row = $res->fetchRow())) {
    echo $row[0] . "\n";
}

$db->disconnect();

?>