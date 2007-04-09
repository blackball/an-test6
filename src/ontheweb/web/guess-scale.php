<?php

// Sample JPEG with lots of EXIF info:
//   http://antwrp.gsfc.nasa.gov/apod/image/0704/aurora1_wikipedia.jpg

// skeleton for guess_image_type
function get_image_type($filename, &$xtopnm) {
}

function guess_image_scale($vals) {
	global $resultdir;
    global $imgfile;
	global $maxfilesize;
	global $fits_guess_scale;

	$xysrc = $vals["xysrc"];

	// Create a directory for this request.
	$myname = create_random_dir($resultdir);
	if (!$myname) {
		die("Failed to create job directory.");
	}
	$mydir = $resultdir . $myname . "/";

	$imgbase = "";
	$imgfilename = "";
	if ($xysrc == "url") {
		$imgurl = $vals["imgurl"];
		// Try to retrieve the URL...
		loggit("retrieving url " . $imgurl . " ...\n");
		$usetype = get_image_type($imgurl, $xtopnm);
		if (!$usetype) {
			loggit("unknown image type.\n");
			die("Unknown image type.");
		}
		loggit("Assuming image type: " . $usetype . "\n");

		$imgbase = "downloaded." . $usetype;
		$downloadedimg = $mydir . $imgbase;
		loggit("Writing to file: " . $downloadedimg . "\n");

		if (download_url($imgurl, $downloadedimg, $maxfilesize, $errmsg) === FALSE) {
			die("Failed to download image: " . $errmsg);
		}
		$imgfilename = $downloadedimg;

	} else if ($xysrc == "img") {
		// If an image was uploaded, move it into place.
		// The filename on the user's machine:
		$imgval = $imgfile->getValue();
		$origname = $imgval["name"];
		$usetype = get_image_type($origname, $xtopnm);
		if (!$usetype) {
			loggit("unknown image file type.\n");
			die("Unknown image file type.");
		}
		loggit("original image file: " . $origname . ", using type: " . $usetype . "\n");

		$imgbase = "uploaded." . $usetype;
		$imgfilename = $mydir . $imgbase;
		if (!$imgfile->moveUploadedFile($mydir, "uploaded." . $usetype)) {
			die("failed to move uploaded file into place.");
		}
	}

	if (!$imgfilename) {
		die("no image file specified.");
	}

	if ($usetype == "fits") {
		$gscale = $mydir . "guess-scale";
		$cmd = $fits_guess_scale . " " . $imgfilename . " 2>/dev/null >" . $gscale;
		$res = system($cmd, $retval);
		if ($retval) {
			die("Failed to run fits-guess-scale.");
		}
		$lines = file($gscale);
		$minscale = 1e100;
		$maxscale = 0.0;

		echo "<html><head><title>Astrometry.net</title></head>\n";
		echo "<h3>Scale values found in the FITS file:</h3>\n";
		echo "<p align=\"center\">\n";
		echo "<table border=\"1\">\n";
		echo "<tr><th>Header</th><th>Scale (arcsec/pixel)</th></tr>\n";

		foreach ($lines as $line) {
			$words = explode(" ", rtrim($line, "\n"));
			if ($words[0] != "scale")
				continue;
			$scale = (float)$words[2];
			if ($scale == 0.0)
				continue;
			loggit("method " . $words[1] . " say scale is " . $scale . "\n");
			$minscale = min($scale, $minscale);
			$maxscale = max($scale, $maxscale);

			echo "<tr><td align=\"center\">" . strtoupper($words[1]) . "</td><td align=\"center\">" . $scale . "</td></tr>\n";
		}

		echo "</table>\n";
		echo "</p>\n";

		if ($maxscale == 0) {
			echo "<p align=\"center\">\n";
			echo "No scale information found.\n";
			echo "</p>\n";
		}

		$host  = $_SERVER['HTTP_HOST'];
		$uri  = rtrim(dirname($_SERVER['PHP_SELF']), '/\\');

		echo "<p align=\"center\">\n";
		echo "<form action=\"" . $_SERVER['PHP_SELF'] . "\" method=\"get\">\n" .
			"<input type=\"hidden\" name=\"xysrc\" value=\"url\" />\n" .
			"<input type=\"hidden\" name=\"tweak\" value=\"1\" />\n" .
			"<input type=\"hidden\" name=\"imgurl\" value=\"" .
			htmlentities("http://" . $host . $uri . "/status/" . $myname . "/" . $imgbase) .
			"\" />\n";

		if ($maxscale > 0) {
			if ($maxscale == $minscale) {
				echo "<input type=\"hidden\" name=\"fstype\" value=\"ev\" />\n" .
					"<input type=\"hidden\" name=\"fse\" value=\"" . $minscale . "\" />\n" .
					"<input type=\"hidden\" name=\"fsv\" value=\"" . 5 . "\" />\n";
			} else {
				// ensure 5% margin.
				if ($maxscale / $minscale < 1.05) {
					$gmean = sqrt($maxscale * $minscale);
					// FIXME - this isn't quite right - we use a different method elsewhere
					$maxscale = $gmean * sqrt(1.05);
					$minscale = $gmean / sqrt(1.05);
				}
				echo "<input type=\"hidden\" name=\"fstype\" value=\"ul\" />\n" .
					"<input type=\"hidden\" name=\"fsl\" value=\"" . $minscale . "\" />\n" .
					"<input type=\"hidden\" name=\"fsu\" value=\"" . $maxscale . "\" />\n";
			}

			echo "<input type=\"hidden\" name=\"fsunit\" value=\"arcsecperpix\" />\n" .
				"<input type=\"submit\" name=\"use-scale\" value=\"Use this scale\" />\n";
		} else {
			"<input type=\"submit\" name=\"go-back\" value=\"Go back\" />\n";
		}

		echo "</form>\n";
		echo "</p>\n";

	} else {
		$imgtype = exif_imagetype($imgfilename);
		if ($imgtype === FALSE) {
			die("Failed to run exif_imagetype on image " . $imgfilename . "\n");
		}
		$types = array(IMAGETYPE_GIF => "gif",
					   IMAGETYPE_JPEG => "jpeg",
					   IMAGETYPE_PNG => "png",
					   IMAGETYPE_SWF => "swf",
					   IMAGETYPE_PSD => "psd",
					   IMAGETYPE_BMP => "bmp",
					   IMAGETYPE_TIFF_II => "tiff(le)",
					   IMAGETYPE_TIFF_MM => "tiff(be)",
					   IMAGETYPE_JPC => "jpc",
					   IMAGETYPE_JP2 => "jp2",
					   IMAGETYPE_JPX => "jpx",
					   IMAGETYPE_JB2 => "jb2",
					   IMAGETYPE_SWC => "swc",
					   IMAGETYPE_IFF => "iff",
					   IMAGETYPE_WBMP => "wbmp",
					   IMAGETYPE_XBM => "xbm");
		loggit("Image type: " . $types[$imgtype]. "\n");
		$exif = exif_read_data($imgfilename);
		if ($exif === FALSE) {
			loggit("exif_read_data failed for file " . $imgfilename . "\n");
		} else {
			$estr = "EXIF data:\n";
			foreach ($exif as $key => $section) {
				if (is_array($section)) {
					foreach ($section as $name => $val) {
						$estr .= "  $key.$name: $val\n";
					}
				} else {
					$estr .= "  $key: $section\n";
				}
			}
			loggit($estr);

			// eg, FocalLength: 150/10 => 15 mm.
			// FocalLength: 4000/10 => 400 mm.

			// FocalLengthIn35mmFilm
			// IFD		Exif
			// Code		41989 (hex 0xA405)

			echo "<pre>$estr</pre>";
		}
	}
}

?>
