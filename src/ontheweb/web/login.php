<?php
require_once 'HTML/QuickForm.php';
require_once 'HTML/QuickForm/Renderer/QuickHtml.php';

require_once 'common2.php';

$sessionvars = array('logged-in', 'yourname');
$formpage = 'fill.php';
main();
exit;

function show_login_form() {
	global $login_form;

	$template = file_get_contents('template-login-form.html');
	$replace = array();
	$flds = array('user', 'pass', 'login');
	$renderer =& new HTML_QuickForm_Renderer_QuickHtml();
	$login_form->accept($renderer);
	foreach ($flds as $fld) {
		$replace['##' . $fld . '##'] = $renderer->elementToHtml($fld);
	}
	$template = str_replace(array_keys($replace), array_values($replace), $template);
	$form_html = $renderer->toHtml($template);

	$template = file_get_contents('template-login.html');
	$template = str_replace('##form##', $form_html, $template);

	echo '<' . '?xml version="1.0" encoding="UTF-8"?' . '>';
	echo $template;
}

function try_login_user($user, $pass) {
	return TRUE;
}

function show_login_failed($user) {
	echo "<p>Tough luck.</p>";
}

function login_user($user) {
	global $formpage;
	// redirect to here so that it's not a POST...
	// (redirect to next page...)
	header('Location: http://' . $_SERVER['HTTP_HOST'] . dirname($_SERVER['PHP_SELF']) . "/" . $formpage);
}

function logged_in() {
	login_user($_SESSION['yourname']);
	/*
	global $logout_form;
	echo "<p>Logged in.</p>";
	echo "<p>Hello, " . $_SESSION['yourname'] . "</p>";
	$renderer =& new HTML_QuickForm_Renderer_QuickHtml();
	$logout_form->accept($renderer);
	echo $renderer->toHtml();
	*/
}

function logout_session() {
	global $sessionvars;

	foreach ($sessionvars as $v) {
		unset($_SESSION[$v]);
	}
	session_destroy();
}

function main() {
	global $login_form;
	global $logout_form;

	session_start();
	if ($_SESSION['logged-in']) {
		$logout_form = get_logout_form();
		$vals = $logout_form->exportValues();
		if ($vals['logout'] == 'Logout') {
			logout_session();
			// redirect here so that when the user hits "reload" it isn't a POST
			// resubmission.
			header('Location: http://' . $_SERVER['HTTP_HOST'] . $_SERVER['PHP_SELF']);
		} else {
			logged_in();
		}
		exit;
	}

	$login_form = new HTML_QuickForm('loginform', 'post');
	$login_form->removeAttribute('name');
	$login_form->addElement('text', 'user', 'Username', array('size'=>20));
	$login_form->addElement('password', 'pass', 'Password', array('size'=>20));
	$login_form->addElement('submit', 'login', 'Login');
	$vals = $login_form->exportValues();
	if ($vals['login'] != 'Login') {
		show_login_form();
	} else {
		$user = $vals['user'];
		$pass = $vals['pass'];
		if (try_login_user($user, $pass)) {
			$_SESSION['logged-in'] = true;
			$_SESSION['yourname'] = $user;
			login_user($user);
		} else {
			show_login_failed($user);
		}
	}
	exit;
}

?>
