<?php
require_once 'common.php';
require_once 'presets.php';

if ($emailver) {
	require_once 'rfc822.php';
}

require_once 'form.php';

function allow_email($email) {
    // Note, these email address MUST be entered in lower-case.
	$allow = array(
		       'aa@astro.ex.ac.uk',
		       'aconti@stsci.edu',
               'adam.ginsburg@colorado.edu',
		       'agamick@ipac.caltech.edu',
		       'agoodman@cfa.harvard.edu',
               'ahagen@hmc.edu',
		       'akapadia@eso.org',
			   'al@kellysky.net',
			   'akelly@ghg.net',
		       'alanv@ics.mq.edu.au',
               'alexander.novikoff@gmail.com',
			   'alfredogarciajr2004@yahoo.com',
		       'align.them@gmail.com',
			   'ames@light2info.com',
               'amiller@astro.berkeley.edu',
               'andy@bettger.net',
			   'animacion@jdiez.com',
		       'anthony@astro.ufl.edu',
               'araneoantonio@yahoo.com',
               'barry.morton@virgin.net',
		       'barth@uci.edu',
			   'ben@astro.washington.edu',
		       'birkmann@mpia-hd.mpg.de',
		       'blindert@mpia-hd.mpg.de',
			   'blind_boy136@hotmail.com',
		       'blutch@google.com',
               'bradandmargie@yahoo.com',
               'brett@mensh.com',
               'burtscher@mpia-hd.mpg.de',
		       'carolc@stsci.edu',
               'christopher@cdixon7.wanadoo.co.uk',
		       'christopher@stumm.ca',
		       'christopher.lawton@esa.int',
			   'christian.perone@gmail.com',
		       'ckochanek@astronomy.ohio-state.edu',
		       'cornell@puck.as.utexas.edu',
               'dale@massapoag.org',
               'dalombardi@interact.ccsd.net',
               'dan.r.preston@gmail.com',
		       'd.sexton@homecall.co.uk',
		       'david.hogg@nyu.edu',
			   'david@12dstring.me.uk',
		       'david@davidmalin.com',
               'dbirnbau@gmail.com',
			   'ddemartin@gmail.com',
		       'dfinkbeiner@cfa.harvard.edu',
               'dinos@microsoft.com',
		       'dmink@cfa.harvard.edu',
		       'don@isc.astro.cornell.edu',
		       'dovip@wise.tau.ac.il',
               'dperley@astro.berkeley.edu',
		       'drholik@sbcglobal.net',
		       'ds@astro.columbia.edu',
		       'dsand@as.arizona.edu',
               'dschmid@mac.com',
		       'dstn@cs.toronto.edu',
               'dthilker@pha.jhu.edu',
		       'eran@astro.caltech.edu',
		       'e.schwab@gsi.de',
			   'epavlu@elliottlabs.com',
               'fbianco@cfa.harvard.edu',
               'fgj@phys.au.dk',
			   'fschorr@comcast.net',
		       'freddy@astronomy.com',
			   'fieldsde@aol.com',
			   'filipe.marques.dias@gmail.com',
		       'fitz@tucana.tuc.noao.edu',
               'fletcherisimo@gmail.com',
			   'folkerts@seanet.com',
		       'fstoehr@eso.org',
		       'fvkastro@xs4all.nl',
               'gabor.kiss@eine-vision.de',
		       'gbb@ipac.caltech.edu',
			   'gburk@rrohio.com',
			   'geoff_k@talk21.com',
               'gnhuftalen@fctvplus.net',
			   'gillcouto@cox.net',
		       'gizis@udel.edu',
               'goldston@astro.berkeley.edu',
		       'grant@bowskill.org.uk',
			   'grisanti@comcast.net',
			   'hadyramez@hotmail.com',
               'helbigh@tidewater.net',
			   'hilton@astro.washington.edu',
		       'hlorch@eso.org',
               'hos@swayback.astr.cwru.edu',
			   'hundalhh@yahoo.com',
		       'hurt@ipac.caltech.edu',
			   'iain@mrmelville.co.uk',
		       'igor.chilingarian@obspm.fr',
			   'javier_laina@yahoo.es',
               'jbedient@gmail.com',
               'jbloom@astro.berkeley.edu',
               'jbriggs@dexter-southfield.org',
			   'jd@astro.washington.edu',
		       'jdshaw@udel.edu',
               'jean-francois.hubert@space.gc.ca',
               'jeff.carlson@gmail.com',
		       'jeremy.d.brewer@gmail.com',
               'jerry.bonnell@nasa.gov',
		       'jfay@microsoft.com',
               'jfbosch@ucdavis.edu',
               'jfoster@cfa.harvard.edu',
			   'jgoldberg@minorplanet.org',
			   'jharris@noao.edu',
			   'jluisrey@teleline.es',
		       'john.moustakas@gmail.com',
               'john@pane.net',
		       'jonbarron@gmail.com',
			   'jordisolaz@gmail.com',
		       'jqshenker@gmail.com',
               'jshiode@astro.berkeley.edu',
               'kadu.barbosa@gmail.com',
               'kcc274@nyu.edu',
		       'kenwood@stsci.edu',
			   'kevinru@ipyramidia.co.uk',
               'kinnerc@gmail.com',
			   'kirby.runyon@houghton.edu',
               'koss@milkyway.gsfc.nasa.gov',
			   'kschindler@sofia.usra.edu',
               'ksmalley@cfa.harvard.edu',
               'ksokolov@mpifr-bonn.mpg.de',
               'kurk@mpia-hd.mpg.de',
		       'landsman@milkyway.gsfc.nasa.gov',
		       'lars@eso.org',
			   'lauman48@yahoo.com',
		       'leonidas@jpl.nasa.gov',
		       'lmoustakas@gmail.com',
		       'lspitler@astro.swin.edu.au',
               'mansi@astro.caltech.edu',
		       'martin@mpia-hd.mpg.de',
			   'mb123@alpenjodel.de',
			   'mbnaqvi78@hotmail.com',
               'mcorbin@nofs.navy.mil',
               'med@noao.edu',
		       'michael.dangelo@rogers.com',
			   'michael.kran@kran.com',
		       'michelle_borkin@harvard.edu',
		       'mierle@gmail.com',
               'mike@mikebutterfield.com',
               'mjg@cacr.caltech.edu',
               'mmotta@massmed.org',
		       'mobiusltd@aol.com',
		       'mperrin@ucla.edu',
               'mstaves@roadrunner.com',
               'mtrawick@richmond.edu',
               'mstaves@roadrunner.com',
		       'munz@physics.muni.cz',
			   'naveen.nanjundappa@gmail.com',
			   'n_mukkavilli@yahoo.com.au',
		       'ncarboni@att.net',
               'netastrometry@gmail.com',
               'norup@astro.ku.dk',
               'ohainaut@eso.org',
               'p.cargile@vanderbilt.edu',
		       'paul@beskeen.com',
			   'paul@vanderkouwe.net',
               'pbandj4@comcast.net',
			   'philipp@salzgeber.at',
		       'pjm@physics.ucsb.edu',
			   'rajarshi@us.ibm.com',
		       'renaud.savalle@obspm.fr',
			   'richard.alexander@usa.net',
		       'roweis@cs.toronto.edu',
               'rqfugate@comcast.net',
			   'sander@tungstentech.com',
               'scott.mcilquham@sympatico.ca',
			   'sfisher@gemini.edu',
               'sgano@nd.edu',
		       'shaw@noao.edu',
               'shawn@gano.name',
		       'simon.krughoff@gmail.com',
			   'smhsfyzix@yahoo.com',
		       'somak@star.sr.bham.ac.uk',
               'sorlando@sorlando.com',
			   'sridhar@nivarty.com',
			   'stephanie@astro.washington.edu',
               'steve@cullenaz.com',
		       'steve@skyatnightimages.co.uk',
               'stooks@engr.uga.edu',
		       'strader@ucolick.org',
		       'stubbs@physics.harvard.edu',
			   't-dinos@microsoft.com',
			   'tai@eastpole.ca',
               'tangerine_reflections@yahoo.com',
			   'themos.tsikas@googlemail.com',
               'theusner@muk.uni-hannover.de',
               'thompson.leblanc@vanderbilt.edu',
		       'tim@chaos.org.uk',
               'tjohnson@nrao.edu',
               'u5j84@ugj.keele.ac.uk',
               'u6c70@ugc.keele.ac.uk',
               'ucapfre@ucl.ac.uk',
               'v6pbs@hotmail.co.uk',
               'vg@jpl.nasa.gov',
               'vhoette@yerkes.uchicago.edu',
			   'visotcky@gmail.com',
               'williams@ap.smu.ca',
               'wmwood-vasey@cfa.harvard.edu',
			   'zorlac@syn-ack.com',
               'bzavala@nofs.navy.mil',
               'wright@astro.ucla.edu',
               'shwright50@comcast.net',
               'aemberson@ethoshousewares.com',
               'xtlin@yahoo.com',
               'keivan.stassun@vanderbilt.edu',
               'kapadia@ipac.caltech.edu',
               'jewellt@u.washington.edu',
               'kscott@es.com',
               'zibetti@mpia-hd.mpg.de',
                   );
	if (in_array(strtolower($email), $allow))
		return TRUE;
	loggit("disallowing email " . $email . "\n");
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
		if (file_exists($inputtmpfile) && !file_exists($inputfile)) {
			//if ($imgfilename) {
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

    $status_url_rel = "status.php?job=" . $myname;
    $uri  = rtrim(dirname($myuri), '/\\');
    $status_url = "http://" . $host . $uri . "/" . $status_url_rel;

	if ($webver) {
		highlevellog("Job " . $myname . ": web edition: wrote scripts.\n");

		if ($justjobid)
			exit;

		loggit("Web version.\n");
		// Redirect the client to the status page...
		if ($imgfilename && !$headers["skippreview"]) {
			$status_url .= "&img";
		}
        header("Location: " . $status_url);
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

		$args = format_preset_url($jd, $formDefaults);
		$message .= "  http://" . $host . $myuri . $args . "\n\n";

        $message .= "If you like watching the grass grow, you can go straight to the status page for this job at:\n  " .
            $status_url . "\n";

		$message .= "Thanks for using Astrometry.net!\n\n";
		$message = wordwrap($message, 70);

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
