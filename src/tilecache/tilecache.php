<?

/*
 * tilecache: a generic google maps v2 tile cache 
 * generic in the sense that it's generic only for astrometry.net :)
 * you have to write your own backend for each layer type.
 *
 * Idea:
 * 	Each tile is unique given the bbox, layer backend, tag, and tilesize.
 *      So why not cache them!
 *
 * 	where:
 *
 *      bbox is the chuck of ra/dec (in merc space) of the globe we are tiling
 *      layer backend is i.e. usnob, sdss, user_image_from_blind, sdss_bounding_boxes, etc.
 *      tag is a potentially unique tag. this is needed for user image overlays 
 * 	     where the same backend will be used for multiple layers.
 * 	tilesize is always 256 as far as we can see
 *
 * Now, we glue the bounding box values together (as a STRING!) and hash them. 
 * that gives us a filename. The mapping bbox->hash may need some iteration 
 * because it may be the case that Google maps API doesn't request the exact 
 * same tiles repeatedly. Hopefully it does, however in the meantime I will 
 * code this as if that were the case.
 *
 * Then render the tile and stash it in cache/layer/tag.
 *
 * In the future, check if the file is there and serve it directly if it is 
 * already rendered.
 */

$CACHEDIR = "/data1/tilecache";
$TILERENDER = "/home/gmaps/usnob-map/execs/tilerender";

// Write a message to the log file.
function loggit($mesg) {
	//error_log($mesg, 3, "/home/gmaps/astrometry/src/tilecache/tilecache.log");
	error_log($mesg, 3, "/tmp/tilecache.log");
}

/*
  This script gets called by the client to request a map tile.  Map tiles are
  256x256 pixels and are specified in terms of the RA,DEC bounding box.
  This script just parses the values and formats a command-line for a C program.
 */

loggit("Tile request headers:\n");
$headers = $_REQUEST;
foreach ($headers as $header => $value) {
	loggit("  $header: $value\n");
}
	
// The variables we need...
$needed = array("WIDTH", "HEIGHT", "SRS", "BBOX");
foreach ($needed as $n => $val) {
	if (!array_key_exists($val, $_REQUEST)) {
		loggit("Request doesn't contain $val.\n");
		header("Content-type: text/html");
		printf("<html><body>Invalid request: missing $val</body></html>\n\n");
		exit;
	}
}

$ws = $_REQUEST["WIDTH"];
$hs = $_REQUEST["HEIGHT"];
$bb = $_REQUEST["BBOX"];
$epsg = $_REQUEST["SRS"];
$lay = $_REQUEST["LAYERS"];
$tag = $_REQUEST["tag"]; // Note that this may be blank for some layers

$cmdline = $TILERENDER;

//loggit("W=$ws, H=$hs, BB=$bb, EPSG=$epsg, LAYERS=$lay, tag=$tag\n");

// This identifies the projection type.
if ($epsg != "EPSG:4326") {
	loggit("Wrong EPSG: $epsg.\n");
	header("Content-type: text/html");
	printf("<html><body>Invalid request: bad EPSG $epsg.</body></html>\n\n");
	exit;
}

// Make sure the bounding box is actually numerical values...
if (sscanf($bb, "%f,%f,%f,%f", $x0, $y0, $x1, $y1) != 4) {
	loggit("Failed to parse BBOX.\n");
	header("Content-type: text/html");
	echo("<html><body>Invalid request: failed to parse BBOX.</body></html>\n\n");
	exit;
}
// Make sure the width and height are numerical...
if ((sscanf($ws, "%d", $w) != 1) ||
	(sscanf($hs, "%d", $h) != 1)) {
	loggit("Failed to parse WIDTH/HEIGHT.\n");
	header("Content-type: text/html");
	printf("<html><body>Invalid request: failed to parse WIDTH or HEIGHT.</body></html>\n\n");
	exit;
}
//loggit("x0=$x0, x1=$x1, y0=$y0, y1=$y1, w=$w, h=$h.\n");
$cmdline .= sprintf(" -x %f -y %f -X %f -Y %f -w %d -h %d", $x0, $y0, $x1, $y1, $w, $h);

// Layers
$layers = explode(",", $lay);
foreach ($layers as $l) {
	$cmdline .= " -l " . escapeshellarg($l);
}

// render_image layer: 
// WCS filename.
$wcs = $_REQUEST["wcsfn"];
if ($wcs) {
	$cmdline .= " -W " . escapeshellarg($wcs);
}
// Image filename.
$imgfn = $_REQUEST["imagefn"];
if ($imgfn) {
	$cmdline .= " -i " . escapeshellarg($imgfn);
}

// render_tycho layer:
// Color correction.
$cc = $_REQUEST["cc"];
if ($cc) {
	if (sscanf($cc, "%f", $ccval) == 1) {
		$cmdline .= " -c " . $ccval;
	}
}
// Arcsinh.
if (array_key_exists("arcsinh", $_REQUEST)) {
	$cmdline .= " -s";
}
// Arith.
if (array_key_exists("arith", $_REQUEST)) {
	$cmdline .= " -a";
}
// Gain.
$gain = $_REQUEST["gain"];
if ($gain) {
	if (sscanf($gain, "%f", $gainval) == 1) {
		$cmdline .= " -g " . $gainval;
	}
}

// Get the hash
$tilestring = $cmdline;
$tilehash = hash('sha256', $tilestring);

// Here we go...
header("Content-type: image/png");

if ($tag) {
	if (!preg_match("/[a-zA-Z0-9-]+/", $tag)) {
		loggit("Naughty tag: \"" . $tag . "\"\n");
		exit;
	}
	$tilecachedir = "$CACHEDIR/$tag";
	if (!file_exists($tilecachedir)) {
		if (!mkdir($tilecachedir)) {
			loggit("Failed to create cache dir " . $tilecachedir . "\n");
			exit;
		}
	}
	$tilefile = "$tilecachedir/tile-$tilehash.png";
	if (!file_exists($tilefile)) {
		$rtn = system($cmdline . " > " . $tilefile);
		if ($rtn) {
			loggit("Tilerender failed: $rtn.\n");
		}
		// Make sure the file actually rendered... paranoia?
		if (!file_exists($tilefile)) {
			$msg = "Something bad happened when rendering 'layer:$lay'  tag:$tag... ???\n";
			loggit($msg);
			header("Content-type: text/html");
			printf("<html><body>$msg</body></html>\n\n");
			exit;
		}
		loggit("Rendered $cmdline to $tilefile\n");
	} else {
		loggit("Cache hit: " . $tilefile . "\n");
	}

	// Slam the file over the wire
	$fp = fopen($tilefile, 'rb');
	fpassthru($fp);
} else {
	// No cache, just run it.
	passthru($cmdline);
}
?> 
