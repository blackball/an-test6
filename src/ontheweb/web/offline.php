<?php
/*
 Web page for offline, bulk-processing of many FITS files.

 This page will ask for the user's email address and create
 a directory for the files.
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
    // Assuming MDB2's default fetchmode is MDB2_FETCHMODE_ORDERED
    //echo $row[0] . "=" . $row[1] . "\n";
    echo $row[0] . "\n";
}

$db->disconnect();

?>