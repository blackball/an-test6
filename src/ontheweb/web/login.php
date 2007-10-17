<?php
require_once 'HTML/QuickForm.php';
require_once 'HTML/QuickForm/Renderer/QuickHtml.php';

// set in php.ini : session.name instead.
//session_name('Astrometry.net');
session_start();

if ($_SESSION['logged-in'] == 1) {
    echo "Logged in.";
    exit;
 }
if (isset($_SESSION['yourname'])) {
    echo "Hello, " . $_SESSION['yourname'];
 }

echo "PHP session id: " . $PHPSESSID;

$form =& new HTML_QuickForm('loginform', 'post');
$form->removeAttribute('name');
$form->addElement('text', 'user', 'Username', array('size'=>20));
$form->addElement('password', 'pass', 'Password', array('size'=>20));
$form->addElement('submit', 'submit', 'Login');

$vals = $form->exportValues();

if ($vals['submit'] != 'Login') {
	$renderer =& new HTML_QuickForm_Renderer_QuickHtml();
	$form->accept($renderer);
    echo $renderer->toHtml();
    exit;
 }

echo '<p>Username: ' . $vals['user'] . '</p>' .
'<p>Password: ' . $vals['pass'] . '</p>' . "\n";

//$_SESSION['logged-in'] = true;
$_SESSION['logged-in'] = 1;
$_SESSION['yourname'] = $vals['user'];

//$form2 =& new HTML_QuickForm('anotherform', 'post');
//$form2->addElement('hidden',

// Remove all this user's data
// session_destroy();

?>

<a href="login.php">Return here</a>

<?php
    phpinfo();
?>