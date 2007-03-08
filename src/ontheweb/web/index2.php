<?php
require_once 'MDB2.php';
require_once 'HTML/QuickForm.php';
require_once 'HTML/QuickForm/Renderer/QuickHtml.php';
require_once 'PEAR.php';

$maxfilesize = 100*1024*1024;
$check_xhtml = 1;

function printerror($err) {
	echo $err->getMessage() . "<br>";
	echo $err->getDebugInfo() . "<br>";
	//print_r($err);
	die("error.");
}

//PEAR::setErrorHandling(PEAR_ERROR_DIE);
PEAR::setErrorHandling(PEAR_ERROR_CALLBACK, 'printerror');

function process_data ($values) {
    global $imgfile;
    global $fitsfile;
	echo "<html><body>";
	if ($imgfile->isUploadedFile()) {
		//$imgfile->moveUploadedFile($path);
		print "Image file was uploaded";
	} else {
		print "No image file uploaded";
	}
	if ($fitsfile->isUploadedFile()) {
		print "FITS file was uploaded";
	} else {
		print "No FITS file was uploaded";
	}
	echo "<table border=1>\n";
	foreach ($values as $key=>$value) {
		echo "<tr><td>" . $key . "</td><td>" . $value . "</td></tr>";
	}
	echo "</table></body></html>\n";
}

$form =& new HTML_QuickForm('blindform','post');

$form->setDefaults(array('x_col' => 'X',
						 'y_col' => 'Y',
						 'parity' => 2,
						 'index' => 'auto',
						 'poserr' => 1.0,
						 'tweak' => TRUE,
						 'imgurl' => "http://",
						 'fsunit' => 'degreewidth',
						 ));

$form->addElement('radio','xysrc',"img",null,'img');
$form->addElement('radio','xysrc',"url",null,'url');
$form->addElement('radio','xysrc',"fits",null,'fits');

$imgfile  =& $form->addElement('file', 'imgfile', "imgfile");
$fitsfile =& $form->addElement('file', 'fitsfile', "fitsfile");
$form->addElement('text', 'x_col', "x column name", array('size'=>5));
$form->addElement('text', 'y_col', "y column name", array('size'=>5));
$form->addElement('text', 'imgurl', "imgurl", array('size' => 40));

$form->addElement('checkbox', 'tweak', 'tweak', null, null);

$form->addElement('text', 'fsl', 'field scale (lower bound)', array('size'=>5));
$form->addElement('text', 'fsu', 'field scale (upper bound)', array('size'=>5));
$form->addElement('text', 'fse', 'field scale (estimate)', array('size'=>5));
$form->addElement('text', 'fsv', 'field scale (variance (%))', array('size'=>5));

$units = array('arcsecperpix' => "arcseconds per pixel",
			   'arcminperpix' => "arcminutes per pixel",
			   'arcminwidth' => "width of the field (in arcminutes)", 
			   'degreewidth' => "width of the field (in degrees)", 
			   'focalmm' => "focal length of the lens (for 35mm film equivalent sensor)", 
			   );

$form->addElement('select', 'fsunit', 'units', $units, null);

$form->addElement('radio', 'parity', "Try both handednesses", null, 2);
$form->addElement('radio', 'parity', "Right-handed image", null, 0);
$form->addElement('radio', 'parity', "Left-handed image", null, 1);

$form->addElement('text', 'poserr', "star positional error (pixels)", array('size' => 5));

$indexes = array('auto' => "Automatic (based on image scale)",
				 "sdss-23" => "12-arcmin Fields (eg, Sloan Digital Sky Survey)",
				 "allsky-31" => "30-arcminute Fields",
				 "allsky-32" => "1-degree Fields",
				 "allsky-33" => "2-degree Fields",
				 "allsky-34" => "4-degree Fields",
				 "allsky-35" => "8-degree Fields",
				 "allsky-36" => "15-degree Fields");

$form->addElement('select', 'index', 'index', $indexes, null);

$form->addElement('submit', 'submit', 'Submit');

$form->setMaxFileSize($maxfilesize);

$form->addRule('xysrc', 'You must provide a field to solve!', 'required');
$form->addRule('poserr', 'Star positional error must be a number!', 'numeric');
$form->addRule('fsu', 'Upper bound must be numeric.', 'numeric');
$form->addRule('fsl', 'Lower bound must be numeric.', 'numeric');
$form->addRule('fse', 'Estimate must be numeric.', 'numeric');
$form->addRule('fsv', 'Variance must be numeric.', 'numeric');
$form->addFormRule('check_xysrc');
$form->addFormRule('check_fieldscale');
$form->addFormRule('check_poserr');
/*
More validation rules
    * required
    * maxlength
    * rangelength
    * regex
    * email
    * emailorblank
    * lettersonly
    * alphanumeric
    * numeric
    * nopunctuation
    * nonzero
Note that some rules (for example, maxlength) take an additional argument:
  $form->addRule('postcode', 'Maximum postcode 8 characters', 'maxlength', 8, 'client');

  $uploadForm->addRule('filename', 'The file you selected is too large', 'maxfilesize', 524288);
  $uploadForm->addRule('filename', 'Must be a jpeg', 'mimetype', array('image/jpeg', 'image/jpeg') );
*/

/*
$form->applyFilter('name', 'trim');
*/


if ($form->validate()) {
	$form->freeze();
    //echo '<h1>Hello, ' . htmlspecialchars($form->exportValue('name')) . '!</h1>';
	/*
	$params = array('xysrc', 'imgfile', 'imgurl', 'fitsfile', 'xcol', 'ycol',
					'ful', 'fub', 'fue', 'fuv', 'fuu', 'parity', 'index',
					'poserr', 'tweak');
	echo "<html><body><table border=1>\n";
	foreach ($params as $p) {
		$val = $form->exportValue($p);
		echo '<tr><th>' . $p . '</th><td>' . gettype($val) . '</td><td>' . 
			$form->getElementError($p) . '</td><td>' . 
			//htmlspecialchars($val)
			//$val
print_r($val, TRUE) . "</td></tr>\n";
	}
	echo "</table></body></html>\n";
	*/

	$form->process('process_data', false);
    exit;
}

$renderer =& new HTML_QuickForm_Renderer_QuickHtml();
$form->accept($renderer);

$template = file_get_contents("index-template.html");
// could also do a heredoc:
// $template = <<<END
// ...
// END;

$flds = array('imgfile', 'fitsfile', 'imgurl', 'x_col', 'y_col',
			  'tweak', 'fsl', 'fsu', 'fse', 'fsv', 'fsunit',
			  'poserr', 'index', 'submit');

foreach ($flds as $fld) {
	$template = str_replace("##".$fld."##", $renderer->elementToHtml($fld), $template);
}

$errflds = array('xysrc', 'imgfile', 'imgurl', 'fitsfile',
				 'x_col', 'y_col', 'fsl', 'fsu', 'fse', 'fsv',
				 'poserr', 'fs');

foreach ($errflds as $fld) {
	$template = str_replace("##".$fld."-err##", $form->getElementError($fld), $template);
}

$repl = array("##xysrc-img##" => $renderer->elementToHtml('xysrc', 'img'),
			  "##xysrc-url##" => $renderer->elementToHtml('xysrc', 'url'),
			  "##xysrc-fits##" => $renderer->elementToHtml('xysrc', 'fits'),
			  "##parity-both##" => $renderer->elementToHtml('parity', '2'),
			  "##parity-left##" => $renderer->elementToHtml('parity', '1'),
			  "##parity-right##" => $renderer->elementToHtml('parity', '0'),
			  );

foreach ($repl as $from => $to) {
	$template = str_replace($from, $to, $template);
}
?>
<?php
echo '<?xml version="1.0" encoding="UTF-8"?>';
?>
<!DOCTYPE html 
PUBLIC "-//W3C//DTD XHTML 1.0 Strict//EN"
"http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd">
<html xmlns="http://www.w3.org/1999/xhtml" xml:lang="en" lang="en">
<head>
<title>
Astrometry.net: Web Edition
</title>
<link rel="stylesheet" type="text/css" href="index.css" />
</head>
<body>

<hr />
<h2>
Astrometry.net: Web Edition
</h2>
<hr />

<?php
echo $renderer->toHtml($template);
//$form->display();
?>

<p>
Note, there is a file size limit of
<?php
printf("%d", $maxfilesize / (1024*1024));
?>
MB.  Your browser may fail silently if you try to upload a file that exceeds this.
</p>

<hr />

<?php
if ($check_xhtml) {
print <<<END
<p>
    <a href="http://validator.w3.org/check?uri=referer"><img
        src="http://www.w3.org/Icons/valid-xhtml10"
        alt="Valid XHTML 1.0 Strict" height="31" width="88" /></a>
</p>
END;
}
?>  

</body>
</html>


<?php
function check_xysrc($vals) {
    global $imgfile;
    global $fitsfile;
	if (!$vals['xysrc']) {
		return array("xysrc"=>"");
	}
	switch ($vals['xysrc']) {
	case 'img':
		if ($imgfile->isUploadedFile()) {
			return TRUE;
		}
		return array("imgfile"=>"You must upload an image file!");
	case 'fits':
		if (!$fitsfile->isUploadedFile()) {
			return array("fitsfile" => "You must upload a FITS file!");
		}
		if (!$vals['x_col'] || !strlen($vals['x_col'])) {
			return array("x_col"=>"You must provide the name of the X column!");
		}
		if (!$vals['y_col'] || !strlen($vals['y_col'])) {
			return array('y_col'=>"You must provide the name of the Y column!");
		}
		return TRUE;
	case 'url':
		if (@parse_url($vals['imgurl']))
			return TRUE;
		return array("imgurl"=>"You must provide a valid URL.");
	}
	return array("imgfile"=>"I'M CONFUSED");
}

function check_fieldscale($vals) {
	$l = (double)$vals["fsl"];
	$u = (double)$vals["fsu"];
	$e = (double)$vals["fse"];
	$v = (double)$vals["fsv"];
	/*
	echo $l . "<br>";
	echo $u . "<br>";
	echo $e . "<br>";
	echo $v . "<br>";
	*/
	if ($l > 0 && $u > 0)
		return TRUE;
	if ($e > 0 && $v > 0)
		return TRUE;
	if (!$l && !$u && !$e && !$v)
		//return array("fs"=>"You must provide either (lower and upper bounds) or (estimate and variance) of the field scale!");
		return array("fs"=>"You must fill in ONE of the pairs of boxes below!");
	if ($l < 0)
		return array("fsl"=>"Lower bound must be positive!");
	if ($u < 0)
		return array("fsu"=>"Upper bound must be positive!");
	if ($e < 0)
		return array("fse"=>"Estimate must be positive!");
	if ($v < 0)
		return array("fsv"=>"Variance must be positive!");
	if (($l && !$u) || ($u && !$l)) {
		return array("fsu"=>"You must give BOTH upper and lower bounds.");
	}
	if (($e && !$v) || ($v && !$e)) {
		return array("fsu"=>"You must give BOTH an estimate and error.");
	}
	return TRUE;
}

function check_poserr($vals) {
	if ($vals["poserr"] <= 0) {
		return array("poserr"=>"Positional error must be positive.");
	}
	return TRUE;
}

?>
