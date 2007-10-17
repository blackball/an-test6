<?php
require_once 'HTML/QuickForm.php';
require_once 'HTML/QuickForm/Renderer/QuickHtml.php';
require_once 'common2.php';

main();

function main() {
	global $login_page;

	session_start();
	if (!$_SESSION['logged-in']) {
		// Back to login page.
		header('Location: http://' . $_SERVER['HTTP_HOST'] . dirname($_SERVER['PHP_SELF']) . "/" . $login_page);
		exit;
	}

	echo "<p> Logged in as <b>" . $_SESSION['yourname'] . "</b> ";
	$logout = get_logout_form();
	$renderer =& new HTML_QuickForm_Renderer_QuickHtml();
	$logout->accept($renderer);
	echo $renderer->toHtml();
	echo "</p>";

	echo "<p>Hello there.</p>";

	$urlFormDefaults = array('imgurl' => 'http://');

	$urlform = new HTML_QuickForm('urlform', 'post');
	$urlform->removeAttribute('name');
	$urlform->setDefaults($urlFormDefaults);
	$urlform->addElement('text', 'imgurl', '', array('size' => 40));
	$urlform->addElement('submit', 'submit', 'Submit');

	$vals = $urlform->exportValues();
	if (($vals['submit'] == 'Submit') && $urlform->validate()) {
		$urlform->freeze();
		$urlform->process('process_urlform', false);
	} else {
		$renderer =& new HTML_QuickForm_Renderer_QuickHtml();
		$urlform->accept($renderer);
		echo $renderer->toHtml();
	}
	exit;
}

function process_urlform($vals) {
	echo "Yep<p>";
	print_r($vals);
}

?>
