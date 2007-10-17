<?php
require_once 'HTML/QuickForm.php';
require_once 'HTML/QuickForm/Renderer/QuickHtml.php';
require_once 'HTML/QuickForm/Renderer/Default.php';
require_once 'common2.php';

// Set up PEAR error handling.
function printerror($err) {
	echo $err->getMessage() . "<br>";
	echo $err->getDebugInfo() . "<br>";
	die("error.");
}
PEAR::setErrorHandling(PEAR_ERROR_CALLBACK, 'printerror');

main();

function main() {
	global $login_page;
	global $shortform;

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

	if (isset($_SESSION['src'])) {
		$src = $_SESSION['src'];
	} else {
		$src = 'imgurl';
	}
	if (array_key_exists('src', $_REQUEST)) {
		$newsrc = $_REQUEST['src'];
		$valids = array('imgfile', 'imgurl');
		if (in_array($newsrc, $valids)) {
			$src = $newsrc;
		}
	}
	$_SESSION['src'] = $src;

	$shortformDefaults = array('imgurl' => 'http://');

	$shortform = new HTML_QuickForm('shortform', 'post');
	$shortform->removeAttribute('name');
	$shortform->setDefaults($shortformDefaults);
	if ($src == 'imgurl') {
		$shortform->addElement('text', 'imgurl', '', array('size' => 40));
	} else if ($src == 'imgfile') {
		$shortform->addElement('file', 'imgfile', '', array('size' => 40));
	}
	$shortform->addElement('submit', 'submit', 'Submit');
	$shortform->addFormRule('check_src');

	$vals = $shortform->exportValues();
	$valid = $shortform->validate();
	if (($vals['submit'] == 'Submit') && $valid) {
		$shortform->freeze();
		$shortform->process('process_shortform', false);
	} else {

		if ($src == 'imgurl') {
			echo '<p>Go to the <a href="?src=imgfile">file upload version</a> of this form</p>';
		} else if ($src == 'imgfile') {
			echo '<p>Go to the <a href="?src=imgurl">URL version</a> of this form</p>';
		}

		// DEBUG
		echo "<pre>";
		print_r($vals);
		echo "</pre>";
		switch ($src) {
		case 'imgfile':
			if (!$shortform->elementExists('imgfile')) {
				die('No imgfile');
			}
			$imgfile = $shortform->getElement('imgfile');
			if ($imgfile->isUploadedFile()) {
				echo "<p>Uploaded OK</p>";
			}
			echo "<pre>";
			print_r($imgfile->getValue());
			echo "</pre>";
		}

		$renderer =& new HTML_QuickForm_Renderer_QuickHtml();
		//$renderer =& new HTML_QuickForm_Renderer_Default();
		$shortform->accept($renderer);
		$replace = array();
		$flds = array('imgurl', 'imgfile', 'submit', 'MAX_FILE_SIZE');
		switch ($src) {
		case 'imgurl':
			$template = file_get_contents('template-simple-url.html');
			break;
		case 'imgfile':
			$template = file_get_contents('template-simple-file.html');
			break;
		}
		foreach ($flds as $fld) {
			if (!$shortform->elementExists($fld))
				continue;
			$replace['##' . $fld . '##'] = $renderer->elementToHtml($fld);
			$replace['##' . $fld . '-err##'] = $shortform->getElementError($fld);
		}
		$template = str_replace(array_keys($replace), array_values($replace), $template);
		echo '<' . '?xml version="1.0" encoding="UTF-8"?' . '>';
		$body = $renderer->toHtml($template);
		$html = file_get_contents('template-simple.html');
		$html = str_replace('##body##', $body, $html);
		echo $html;
	}
	exit;
}

function check_src($vals) {
	global $shortform;

	$src = $_SESSION['src'];
	switch ($src) {
	case 'imgfile':
		if (!$shortform->elementExists('imgfile')) {
			return array('imgfile' => 'No imgfile');
		}
		$imgfile = $shortform->getElement('imgfile');
		$val = $imgfile->getValue();
		if ($val['size'] == 0) {
			return array('imgfile' => 'Image file is empty.');
		}
		if (!$imgfile->isUploadedFile()) {
			return array('imgfile' => 'Image file was not uploaded.');
		}
		return TRUE;
	}
	return TRUE;
}

function process_shortform($vals) {
	global $shortform;

	echo "<p>Yep.</p>";
	echo "<pre>";
	print_r($vals);
	echo "</pre>";
	$src = $_SESSION['src'];
	if ($src == 'imgfile') {
		if (!$shortform->elementExists('imgfile')) {
			die('No imgfile');
		}
		$imgfile = $shortform->getElement('imgfile');
		if ($imgfile->isUploadedFile()) {
			echo "<p>Uploaded OK</p>";
		}
		echo "<pre>";
		print_r($imgfile->getValue());
		echo "</pre>";
	}
}

?>
