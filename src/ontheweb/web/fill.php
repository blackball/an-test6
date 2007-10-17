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
		header('Location: http://' . $_SERVER['HTTP_HOST'] . dirname($_SERVER['PHP_SELF']) . "/" . $loginpage);
		exit;
	}

	echo "<p> Logged in as " . $_SESSION['yourname'] . "</p>";
	$logout = get_logout_form();
	$renderer =& new HTML_QuickForm_Renderer_QuickHtml();
	$logout->accept($renderer);
	echo $renderer->toHtml();

	echo "<p>Hello there.</p>";
}

?>
