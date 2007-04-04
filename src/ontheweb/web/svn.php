<?php
function get_svn_rev() {
	$rev = "$Revision$";
	return substr($rev, strlen("$Revision: "), -(strlen("$Revision$")));
}

function get_svn_url() {
	$url = "$URL$";
	return substr($url, strlen("$URL: "), -(strlen("$URL$")));
}

function get_svn_date() {
	$date = "$Date$";
	return substr($date, strlen("$Date: "), -(strlen("$Date$")));
}
?>
