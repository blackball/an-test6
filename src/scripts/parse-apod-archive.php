<?php

/*
$archive = 'http://antwrp.gsfc.nasa.gov/apod/archivepix.html';
$baseurl = dirname($archive) . "/";
 */
/*
$archive = 'archivepix.html';
$baseurl = "./";
*/

$archive = 'http://antwrp.gsfc.nasa.gov/apod/archivepix.html';
$baseurl = 'http://antwrp.gsfc.nasa.gov/apod/';

$str = file_get_contents($archive);
if (!$str) {
	die("Couldn't get: " . $archive);
}

//echo "Got:\n" . substr($str, 0, 256) . "\n.....\n\n";

$pat = "|\w* \w* \w*:\s*<a href=\"(ap(\d{6}).html)\">(.*?)</a>(<br>)?|s";

if (!preg_match_all($pat, $str, $matches, PREG_SET_ORDER)) {
	die("No matches.");
}

echo "Got " . count($matches) . " matches.\n";

foreach ($matches as $m) {
	$url = $baseurl . $m[1];
	$date = $m[2];
    //echo $m[0] . "\n";
    /*
     echo "  " . $m[1] . "\n";
     echo "  " . $url . "\n";
     echo "  " . $m[2] . "\n";
     echo "  " . $m[3] . "\n";
     */

    // temp
    if (substr($date, 0, 2) != "06")
        //if (substr($date, 0, 4) != "9601")
        continue;

    /*
     $ss = substr($date, 0, 6);
     if (!in_array($ss,
     array("061227", "061208", "061006", "060825",
     "060722", "060720", "060529", "060519")))
     continue;
     */

    echo $m[0] . "\n";

	/*
	sscanf($date, "%d", $datenum);
	if ($datenum > 70522)
		continue;
	*/

	$str = file_get_contents($url);
	if (!$str) {
		die("Couldn't get: " . $url);
	}

	$pat = "|<a href=\"(image/[^ \n]*?)\"(?:.*?\")?\n?>\s*<(?i:img) SRC=\"(image/.*?\\.(.*?))\".*?>\s*</a>|s";
	if (!preg_match($pat, $str, $match)) {
		echo "-- No match found for " . $date . " --\n";
		continue;
	}

    /*
     echo "  " . $match[1] . "\n";
     echo "  " . $match[2] . "\n";
     echo "  " . $match[3] . "\n";
     */

	$bigimgurl = $baseurl . $match[1];
	$imgurl = $baseurl . $match[2];
	$suffix = $match[3];

	$img = file_get_contents($imgurl);
	if (!$img) {
		die("Couldn't get image: " . $imgurl);
	}
	if (!file_put_contents($date . "." . $suffix, $img)) {
		die("Couldn't write image: " . $imgurl);
	}


	$bigimg = file_get_contents($bigimgurl);
	if (!$bigimg) {
		die("Couldn't get bigimage: " . $bigimgurl);
	}
	if (!file_put_contents($date . "-big." . $suffix, $bigimg)) {
		die("Couldn't write image: " . $bigimgurl);
	}

}

?>

