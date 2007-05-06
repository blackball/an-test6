<?php
require_once 'common.php';
require_once 'presets.php';

$emailver = 0;
$webver = 1;

if ($emailver) {
	require_once 'rfc822.php';
}

require_once 'form.php';

function allow_email($email) {
	// $mode = 'deny';
	$mode = 'allow';

	$allow = array(
		       'aconti@stsci.edu',
		       'alanv@ics.mq.edu.au',
		       'barth@uci.edu',
		       'blutch@google.com',
		       'carolc@stsci.edu',
		       'christopher@stumm.ca',
		       'ckochanek@astronomy.ohio-state.edu',
		       'cornell@puck.as.utexas.edu',
		       'david.hogg@nyu.edu',
		       'dfinkbeiner@cfa.harvard.edu',
		       'dmink@cfa.harvard.edu',
		       'dovip@wise.tau.ac.il',
			   'dstn@cs.toronto.edu',
		       'eran@astro.caltech.edu',
			   'fitz@tucana.tuc.noao.edu',
		       'john.moustakas@gmail.com',
		       'jqshenker@gmail.com',
		       'kenwood@stsci.edu',
		       'landsman@milkyway.gsfc.nasa.gov',
		       'leonidas@jpl.nasa.gov',
		       'lmoustakas@gmail.com',
		       'munz@physics.muni.cz',
		       'pjm@physics.ucsb.edu',
		       'roweis@cs.toronto.edu',
		       'strader@ucolick.org',
		       'stubbs@physics.harvard.edu',
		       'tim@chaos.org.uk');

	$deny = array('badguy@somewhere.com');

	if ($mode == 'deny') {
		if (in_array(strtolower($email), $deny)) {
			loggit("denying email " . $email . "\n");
			return FALSE;
		}
		return TRUE;
	} else if ($mode == 'allow') {
		if (in_array(strtolower($email), $allow))
			return TRUE;
		loggit("disallowing email " . $email . "\n");
		return FALSE;
	} else {
		loggit("allow_email: invalid mode " . $mode . "\n");
	}
	return FALSE;
}

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

	$justjobid = $vals['justjobid'];

	if ($justjobid) {
		// skip the "source extraction" preview, just start crunching!
		loggit("justjobid set.  imgfilename=" . $imgfilename . "\n");
		if ($imgfilename) {
			// HACK - pause for watcher...
			sleep(1);
			if (!rename($inputtmpfile, $inputfile)) {
				loggit("Failed to rename input temp file " . $inputtmpfile . " to " . $inputfile);
				submit_failed($db, "Failed to rename input file for the blind solver.");
			}
			//loggit("renamed $inputtmpfile to $inputfile_orig.\n");
		}
		// Just write the jobid.
		header('Content-type: text/plain');
		echo $myname . "\n";
		highlevellog("Job " . $myname . ": skipped source extraction preview; job started.\n");
	}

	if ($webver) {
		highlevellog("Job " . $myname . ": web edition: wrote scripts.\n");

		if ($justjobid)
			exit;

		loggit("Web version.\n");
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

		// HACK - is this necessary?
		if (file_exists($inputtmpfile) && !file_exists($inputfile)) {
			// Rename the input file so the watcher grabs it...
			// HACK - after waiting for the watcher.
			sleep(1);
			if (!rename($inputtmpfile, $inputfile)) {
				loggit("Failed to rename input temp file " . $inputtmpfile . " to " . $inputfile);
				submit_failed($db, "Failed to rename input file for the blind solver.");
			}
			loggit("Renamed $inputtmpfile to $inputfile.\n");
		}

		highlevellog("Job " . $myname . ": email edition; job started.\n");

		// Send email
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

		$subject = 'Astrometry.net job ' . $myname;
		$headers = 'From: Astrometry.net <alpha@astrometry.net>' . "\r\n" .
			'Reply-To: dstn@cs.toronto.edu' . "\r\n" .
			'X-Mailer: PHP/' . phpversion() . "\r\n";
		if ($name) {
			$headers .= 'To: ' . $name . ' <' . $to . '>' . "\r\n";
		} else {
			$headers .= 'To: ' . $to . "\r\n";
		}

		if (!mail("", $subject, $message, $headers)) {
			submit_failed($db, "Failed to send email.");
		}
		loggit("Sent 'submitted' email.\n");

		if ($justjobid)
			exit;

		// Tell the client we've received their image and will start crunching...
		$txt = file_get_contents($submitted_html);
		$txt = str_replace('##jobid##', $myname, $txt);
		echo $txt;

		exit;
	}
}

?>
