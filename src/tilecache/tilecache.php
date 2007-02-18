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

/**********************************************
 ******************* backends  ****************
 **********************************************/

/* The ghetto callback registry */
$layer_render_callbacks = array();
$getdir_callbacks = array();
function backend_register($layer_name, $render_callback, $getdir_callback)
{
	global $layer_render_callbacks, $getdir_callbacks;
	$layer_render_callbacks[$layer_name] = $render_callback;
	$getdir_callbacks[$layer_name] = $getdir_callback;
}

function backend_exists($layer_name) {
	global $layer_render_callbacks;
	return array_key_exists($layer_name, $layer_render_callbacks);
}

/* ------------------------------------------*/
/* First start with a showimagetile backend. */

$USERIMAGETILE_CACHEBIN = "/home/gmaps/astrometry/src/gmaps-showimage/execs/showimagetile";
$USERIMAGETILE_CACHEDIR = "/home/gmaps/astrometry/src/gmaps-showimage/cache";

/* each backend maps a tag to a directory where the cached images are stored */
function userimagetile_getdir($tag) {
   global $USERIMAGETILE_CACHEDIR;
	return "$USERIMAGETILE_CACHEDIR/$tag";
}

// Render a single tile to $outfname
function userimagetile_render($x0, $x1, $y0, $y1, $w, $h, $tag, $outfname) {
   global $USERIMAGETILE_CACHEDIR, $USERIMAGETILE_CACHEBIN;
	$tag = "test_tag"; // fake a tag for now
	// FIXME check for tag validity
	$wcs = "$USERIMAGETILE_CACHEDIR/$tag/wcs.000";
	$userimage = "$USERIMAGETILE_CACHEDIR/$tag/userimage.ppm";
	$cmd = "$USERIMAGETILE_CACHEBIN -f$userimage -F$wcs";
	$cmd = $cmd . sprintf(" -x %f -y %f -X %f -Y %f -w %d -h %d", $x0, $y0, $x1, $y1, $w, $h);
	$cmd = "$cmd > $outfname";
	loggit("Userimagetile backend: $cmd\n");
	system($cmd);
}

backend_register('userimagetile', 'userimagetile_render', 'userimagetile_getdir');

/* ------------------------------------------*/
/* First start with a showimagetile backend. */

$USNOB_BIN = "/home/gmaps/usnob-map/execs/usnobtile";
$USNOB_CACHEDIR = "/data1/usnobcache";

function usnob_getdir($tag) {
   global $USNOB_CACHEDIR;
	return $USNOB_CACHEDIR;
}

function usnob_render($x0, $x1, $y0, $y1, $w, $h, $tag, $outfname) {
   global $USNOB_CACHEDIR, $USNOB_BIN;
	$cmd = $USNOB_BIN . sprintf(" -x %f -y %f -X %f -Y %f -w %d -h %d", $x0, $y0, $x1, $y1, $w, $h);
	//$cmd = "$cmd | pnmtopng > $outfname";
	$cmd = "$cmd > $outfname";
	loggit("Userimagetile backend: $cmd\n");
	system($cmd);
}

backend_register('usnob', 'usnob_render', 'usnob_getdir');

/**********************************************
 ************ main tilecache entry ************
 **********************************************/

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

loggit("W=$ws, H=$hs, BB=$bb, EPSG=$epsg, LAYERS=$lay, tag=$tag\n");

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
loggit("x0=$x0, x1=$x1, y0=$y0, y1=$y1, w=$w, h=$h.\n");

// Make sure there is a backend for this layer
if (!backend_exists($lay))  {
	$msg = "Invalid backend/layer $lay\n";
	loggit($msg);
	header("Content-type: text/html");
	printf("<html><body>$msg</body></html>\n\n");
	exit;
}

// Get the hash
$tilestring = "x0=$x0, x1=$x1, y0=$y0, y1=$y1, w=$w, h=$h";
$tilehash = hash('sha256', $tilestring);

$tcb = $getdir_callbacks[$lay];
//$tilecachedir = $getdir_callbacks[$lay]($tag);
$tilecachedir = $tcb($tag);
$tilefile = "$tilecachedir/tile-$tilehash.png";
if (!file_exists($tilefile)) {
	$callback = $layer_render_callbacks[$lay];
	loggit("Missed cache, rendering to ($tilecachedir) $tilefile via $callback\n");
	$callback($x0, $x1, $y0, $y1, $w, $h, $tag, $tilefile);
} else {
	loggit("HIT THE CACHE\n");
}

// Make sure the file actually rendered
if (!file_exists($tilefile)) {
	$msg = "Something bad happened when rendering 'layer:$lay'  tag:$tag... ???\n";
	loggit($msg);
	header("Content-type: text/html");
	printf("<html><body>$msg</body></html>\n\n");
	exit;
}

// Slam the file over the wire
header("Content-type: image/png");
$fp = fopen($tilefile, 'rb');
fpassthru($fp);
exit;
?> 
