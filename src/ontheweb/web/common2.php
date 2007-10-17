<?php

$login_page = 'login.php';

function get_logout_form() {
	global $logout_form;
	global $login_page;

	if (!isset($logout_form)) {
		$logout_form = new HTML_QuickForm('logoutform', 'post', $login_page);
		$logout_form->removeAttribute('name');
		$logout_form->addElement('submit', 'logout', 'Logout');
	}
	return $logout_form;
}

?>
