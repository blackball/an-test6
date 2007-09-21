<?php

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

include 'config.php';

// Write a message to the log file.
function loggit($mesg) {
    global $LOGFILE;
    error_log($mesg, 3, $LOGFILE);
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
$needed = array("WIDTH", "HEIGHT", "BBOX");
foreach ($needed as $n => $val) {
    if (!array_key_exists($val, $_REQUEST)) {
        loggit("Request doesn't contain $val.\n");
        die("Invalid request: missing $val");
    }
}

$ws = $_REQUEST["WIDTH"];
$hs = $_REQUEST["HEIGHT"];
$bb = $_REQUEST["BBOX"];
$lay = $_REQUEST["LAYERS"];
$tag = $_REQUEST["tag"]; // Note that this may be blank for some layers

$cmdline = $TILERENDER;

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

$cmdline .= sprintf(" -x %f -y %f -X %f -Y %f -w %d -h %d", $x0, $y0, $x1, $y1, $w, $h);

$cmdline .= " -D " . escapeshellarg($TRCACHEDIR);

// Layers
$layers = explode(",", $lay);
foreach ($layers as $l) {
    $cmdline .= " -l " . escapeshellarg($l);
}

// ***************************************************
// Collect arguments for the renderer
// ***************************************************

// render_collection:
if (array_key_exists('outline', $_REQUEST)) {
    $cmdline .= " -O";
}

if (array_key_exists('density', $_REQUEST)) {
    $cmdline .= " -n";
}

if (in_array('apod', $layers)) {
    $files = scandir($RCDIR);
    $goodfiles = array();
    foreach ($files as $f) {
        if (is_dir($f))
            continue;
        array_push($goodfiles, $RCDIR . '/' . $f);
    }
    $filelist = implode("\n", $goodfiles);
    $fn = md5($filelist);
    $dir = sys_get_temp_dir();
    $path = $dir . '/' . $fn;
    file_put_contents($path, $filelist);
    $cmdline .= " -S " . $path;
}

//////////////////////
// render_image layer:

// WCS filename.
$wcs = $_REQUEST["imwcsfn"];
if ($wcs) {
    $cmdline .= " -I " . escapeshellarg($wcs);
}

// Image filename.
$imgfn = $_REQUEST["imagefn"];
if ($imgfn) {
    $cmdline .= " -i " . escapeshellarg($imgfn);
}

//////////////////////
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

//////////////////////
// render_usnob layer:

if (array_key_exists('version', $_REQUEST)) {
	$cmdline .= " -V " . escapeshellarg($_REQUEST['version']);
}

// color mapping:
$cmap = $_REQUEST['cmap'];
if ($cmap) {
	if (in_array($cmap, array('rb', 'i'))) {
		$cmdline .= " -C " . $cmap;
	}
}

//////////////////////
// render_rdls layer:

// rdls file
$rdlsfn = $_REQUEST['rdlsfn'];
if ($rdlsfn) {
    $cmdline .= " -r " . escapeshellarg($rdlsfn);
	$fld = $_REQUEST['rdlsfield'];
	if ($fld) {
		$cmdline .= ' -F ' . (int)$fld;
	}
	$sty = $_REQUEST['rdlsstyle'];
	if ($sty) {
		$cmdline .= ' -k ' . escapeshellarg($sty);
	}
}

$rdlsfn = $_REQUEST['rdlsfn2'];
if ($rdlsfn) {
    $cmdline .= " -r " . escapeshellarg($rdlsfn);
	$fld = $_REQUEST['rdlsfield2'];
	if ($fld) {
		$cmdline .= ' -F ' . (int)$fld;
	}
	$sty = $_REQUEST['rdlsstyle2'];
	if ($sty) {
		$cmdline .= ' -k ' . escapeshellarg($sty);
	}
}

//////////////////////
// render_boundary layer:

// WCS file.
$wcs = $_REQUEST["wcsfn"];
if ($wcs) {
    $cmdline .= " -W " . escapeshellarg($wcs);
}

// Line width.
$lw = $_REQUEST["lw"];
if ($lw) {
    if (sscanf($lw, "%f", $lwval) == 1) {
        $cmdline .= " -L " . $lwval;
    }
}

// Dashed box to show zoom-in
$box = $_REQUEST["dashbox"];
if ($box) {
    if (sscanf($box, "%f", $boxval) == 1) {
        $cmdline .= " -B " . $boxval;
    }
}

// ***************************************************
// Now render it
// ***************************************************

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
        $cmdline .= " > " . $tilefile . " 2>> " . $LOGFILE;
        loggit("cmdline: " . $cmdline . "\n");
        $rtn = system($cmdline);
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
        loggit("Rendered: $cmdline > $tilefile\n");
    } else {
        loggit("Cache hit: " . $tilefile . "\n");
    }

    // Slam the file over the wire
    $fp = fopen($tilefile, 'rb');
    fpassthru($fp);
} else {
    loggit("cmdline: " . $cmdline . "\n");
    $cmdline .= " 2>> " . $LOGFILE;
    // No cache, just run it.
    passthru($cmdline);
}
?> 
