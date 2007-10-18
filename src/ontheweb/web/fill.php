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

	if ($_REQUEST['long']) {
		global $unitmap;

		$longformDefaults = array('x_col' => 'X',
								  'y_col' => 'Y',
								  'parity' => 2,
								  'fstype' => 'ul',
								  'fsl' => 0.1,
								  'fsu' => 180,
								  'tweak' => 1,
								  'tweak_order' => 2,
								  'imgurl' => "http://",
								  'scaleunits' => 'degreewidth',
								  );

		$longform = new HTML_QuickForm('longform', 'post', '', '',
									   array('onsubmit'=>'return submitForm();'));
		$longform->removeAttribute('name');
		$longform->setDefaults($longformDefaults);
		$longform->addElement('text', 'imgurl', '', array('size' => 40));
		$longform->addElement('file', 'imgfile', '', array('size' => 40));
		$longform->addElement('file', 'fitsfile', '', array('size' => 40));
		$longform->addElement('file', 'textfile', '', array('size' => 40));

		$srcmap = array('imgurl' => 'URL of an image file',
						'imgfile' => 'Image file',
						'fitsfile' => 'FITS table of sources',
						'textfile' => 'Text list of sources');
		$longform->addElement('select', 'src', '', $srcmap,
							  array('onchange' =>'sourceChanged()',
									'onkeyup' =>'sourceChanged()',
									'id' => 'selectsource'));

		$longform->addElement('select', 'scaleunits', '', $unitmap,
							  array('onchange' =>'unitsChanged()',
									'onkeyup' =>'unitsChanged()',
									'id' => 'selectunits'));
		$longform->addElement('radio', 'fstype', null, null, 'ul',
							  array('id'=>'fstype-ul',
									'onclick'=>'scalechanged()'));
		$longform->addElement('radio', 'fstype', null, null, 'ev',
							  array('id'=>'fstype-ev',
									'onclick'=>'scalechanged()'));
		$longform->addElement('text', 'fsl', '',
							  array('size'=>5,
									'id'=>'fsl',
									'onkeyup' => 'scalechanged()',
									'onfocus' => 'setFsUl()'));
		$longform->addElement('text', 'fsu', '',
							  array('size'=>5,
									'id'=>'fsu',
									'onkeyup' => 'scalechanged()',
									'onfocus' => 'setFsUl()'));
		$longform->addElement('text', 'fse', '',
							  array('size'=>5,
									'id'=>'fse',
									'onkeyup' => 'scalechanged()',
									'onfocus' => 'setFsEv()'));
		$longform->addElement('text', 'fsv', '',
							  array('size'=>5,
									'id'=>'fsv',
									'onkeyup' => 'scalechanged()',
									'onfocus' => 'setFsEv()',));

		$longform->addElement('submit', 'submit', 'Submit');

		$longform->validate();

		//$longform->addElement('select', 'fsunit', 'units', $unitmap, array('onchange=);
		$renderer =& new HTML_QuickForm_Renderer_QuickHtml();
		$longform->accept($renderer);

		$replace = array();
		$flds = array('imgurl', 'imgfile', 'fitsfile', 'textfile',
					  'src', 'scaleunits', 'fsl', 'fsu', 'fse', 'fsv',
					  'submit');
		foreach ($flds as $fld) {
			$replace['##' . $fld . '##'] = $renderer->elementToHtml($fld);
			$replace['##' . $fld . '-err##'] = $longform->getElementError($fld);
		}
		// pseudo-fields that can have errors.
		$flds = array('fstype');
		foreach ($flds as $fld) {
			$replace['##' . $fld . '-err##'] = $longform->getElementError($fld);
		}

		$replace['##fstype-ul##']    = $renderer->elementToHtml('fstype', 'ul');
		$replace['##fstype-ev##']    = $renderer->elementToHtml('fstype', 'ev');
		$template = file_get_contents('template-long-body.html');
		$template = str_replace(array_keys($replace), array_values($replace), $template);
		echo '<' . '?xml version="1.0" encoding="UTF-8"?' . '>';
		$body = $renderer->toHtml($template);

		$html = file_get_contents('template-long.html');
		$html = str_replace('##body##', $body, $html);
		echo $html;

		//echo $renderer->toHtml();
		exit;
	}

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

		// DEBUG
		/*
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
		*/

		$logout = get_logout_form();
		$renderer =& new HTML_QuickForm_Renderer_QuickHtml();
		$logout->accept($renderer);
		$replace = array();
		$replace['##user##'] = $_SESSION['yourname'];
		$replace['##logout##'] = $renderer->elementToHtml('logout');
		$template = file_get_contents('template-logout.html');
		$template = str_replace(array_keys($replace), array_values($replace), $template);
		$logout_html = $renderer->toHtml($template);


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
		$html = str_replace('##logout##', $logout_html, $html);
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
