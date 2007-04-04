<?php
function get_svn_rev() {
	$rev = "$Revision$";
	return substr($rev, strlen("$Revision: "), -2);
}

function get_svn_url() {
	$url = "$URL$";
	return substr($url, strlen("$URL: "), -2);
}

function get_svn_date() {
	$date = "$Date$";
	return substr($date, strlen("$Date: "), -2);
}
?>
