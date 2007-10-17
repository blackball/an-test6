<?php

$login_page = 'login.php';

$unitmap = array('arcsecperpix' => "arcseconds per pixel",
				 'arcminperpix' => "arcminutes per pixel",
				 'arcminwidth' => "width of the field (in arcminutes)", 
				 'degreewidth' => "width of the field (in degrees)", 
				 'focalmm' => "focal length of the lens (for 35mm film equivalent sensor)", 
				 );

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
