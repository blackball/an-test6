<?php
require 'common.php';
require 'presets.php';

$emailver = 0;
$webver = 1;

/*
$emailver = 1;
$webver = 0;
*/

if ($emailver) {
	require 'rfc822.php';
}

require 'form.php';

// Called after the blind input file is written... do user interface stuff.
function after_submitted($imgfilename, $myname, $mydir, $vals, $db) {
	global $emailver;
	global $webver;
	global $headers;
	global $input_fn;
	global $inputtmp_fn;
	global $host;
	global $myuri;
	global $submitted_html;

	$inputfile = $mydir . $input_fn;
	$inputtmpfile = $mydir . $inputtmp_fn;

	if ($webver) {
		loggit("Web version.\n");
		if ($headers["justjobid"]) {
			// skip the "source extraction" preview, just start crunching!
			loggit("justjobid set.  imgfilename=" . $imgfilename . "\n");
			if ($imgfilename) {
				if (!rename($inputtmpfile, $inputfile)) {
					die("Failed to rename input temp file " . $inputtmpfile . " to " . $inputfile);
				}
				loggit("renamed $inputtmpfile to $inputfile_orig.\n");
			}
			// Just write the jobid.
			header('Content-type: text/plain');
			echo $myname . "\n";
			exit;
		}

		// Redirect the client to the status page...
		$status_url = "status.php?job=" . $myname;

		if ($imgfilename && !$headers["skippreview"]) {
			$status_url .= "&img";
		}
		$uri  = rtrim(dirname($myuri), '/\\');
		header("Location: http://" . $host . $uri . "/" . $status_url);
		exit;
	}

	if ($emailver) {
		loggit("Email version.\n");

		// Rename the input file so the watcher grabs it.
		if (!rename($inputtmpfile, $inputfile)) {
			die("Failed to rename input temp file " . $inputtmpfile . " to " . $inputfile);
		}
		loggit("Renamed $inputtmpfile to $inputfile.\n");

		// Tell the client we've received their image and will start crunching...
		$txt = file_get_contents($submitted_html);
		$txt = str_replace('##jobid##', $myname, $txt);
		echo $txt;

		$to = $vals['email'];
		$name = $vals['uname'];

		$message = "Hi,\n\n" .
			"Astrometry.net has received your field and we are trying to solve it.\n\n" .
			"Your job id is " . $myname . "\n\n" .
			"If we solve it successfully, we will send you the results.  " .
			"If not, we will send you an email to let you know, and we may contact you to ask for more details about the field.\n\n";

		$message .= "Just to remind you, here are the parameters of your job:\n";
		$jd = getalljobdata($db);
		$jobdesc = describe_job($jd);
		foreach ($jobdesc as $key => $val) {
			$message .= "  " . $key . ": " . $val . "\n";
		}
		$message .= "\n";

		$message .= "If you want to return to the form with the values you entered " .
			"already filled in, you can use this link.  Note, however, that your web browser " .
			"won't let us set a default value for the file upload field, so you may have to " .
			"cut-n-paste the filename.\n";
		$message = wordwrap($message, 70);

		$args = format_preset_url($jd, $formDefaults);
		$message .= "  http://" . $host . $myuri . $args . "\n\n";

		$message .= "Thanks for using Astrometry.net!\n\n";

		$subject = 'Astrometry.net job ' . $myname . ' submitted';
		$headers = 'From: Astrometry.net <alpha@astrometry.net>' . "\r\n" .
			'Reply-To: dstn@cs.toronto.edu' . "\r\n" .
			'X-Mailer: PHP/' . phpversion() . "\r\n";
		if ($name) {
			$headers .= 'To: ' . $name . ' <' . $to . '>' . "\r\n";
		} else {
			$headers .= 'To: ' . $to . "\r\n";
		}

		if (!mail("", $subject, $message, $headers)) {
			die("Failed to send email.\n");
		}
		loggit("Sent 'submitted' email.\n");
		exit;
	}
}

?>
