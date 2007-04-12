<?php
/*
 This code was taken from the user comments at
    http://ca.php.net/manual/en/features.file-upload.php .

 When the form is submitted, call this javascript:
   window.open('/upload_progress.php?id=' + upload_identifier, 'Upload_Meter','width=370,height=115,status=no', true);

 Where 'upload_identifier' is the value of the hidden form element UPLOAD_IDENTIFIER.
*/

function nice_val($x) {
	$s = '';
	if ($x > 1e6) {
		$s = 'M';
		$x /= 1e6;
	} else if ($x > 1e3) {
		$s = 'k';
		$x /= 1e3;
	}

	if ($x > 100) {
		$x = sprintf("%d", round($x));
	} else if ($x > 10) {
		$x = sprintf("%.1f", round($x,1));
	} else {
		$x = sprintf("%.2f", round($x,2));
	}
	return array('val'=>$x, 'suff'=>$s);
}

$id = $_GET['id'];
$body = false; // HTML body to display.
$onload = false; // Javascript to run when the page loads.
$refresh = false; // Number of seconds to wait before refreshing the page. false/0 means don't refresh.
$info = false; // Table of information to display
$url = htmlentities($_SERVER['PHP_SELF']).'?id='.$id.'&e=1'; // URL to redirect to.
$ul_info = uploadprogress_get_info($id);
if(!$ul_info) {
   if(isset($_GET['e'])) {
	   $onload = 'window.close();';
	   $body = "Invalid upload meter ID!";
   } else {
	   $refresh = 2;   // Wait 2 seconds, give the server time to create the progress file.
	   $body = "Waiting for upload to begin.";
   }
} else {
    if($ul_info['bytes_total'] > 1 && $ul_info['bytes_uploaded'] >= $ul_info['bytes_total'] && $ul_info['est_sec'] == 0) {
        $onload = 'sleep(2); window.close();';
        $body = 'Upload complete.'; // They won't see this if the javascript runs, but just in case they've disabled it.
    } else {
        $body = "Uploading...";
        $refresh = 3;
        $percent_complete = $ul_info['bytes_uploaded'] * 100 / $ul_info['bytes_total'];
        $kb_per_sec = $ul_info['speed_last'] / 1000;
		$dt = $ul_info['time_last'] - $ul_info['time_start'];
        $info = array(
					  'meter' => round($percent_complete, 2),
					  'width' => round($percent_complete),
					  'eta' => sprintf("%02d:%02d", $ul_info['est_sec'] / 60, $ul_info['est_sec'] % 60),
					  'speed' => round($kb_per_sec, ($ul_info['speed_last'] < 10000) ? 2 : 0),
					  'upl' => nice_val($ul_info['bytes_uploaded']),
					  'total' => nice_val($ul_info['bytes_total']),
					  'time' => sprintf("%02d:%02d", $dt / 60, $dt % 60),
            );
    }
}
$pct = $info['meter'];
?>
<html>
<head>
<?php
echo '<meta http-equiv="refresh" content="';
if ($refresh)
	 echo $refresh . '; ';
echo 'URL=' . $url . '"/>';
?>
<meta http-equiv="Content-Script-Type" content="text/javascript" />
<link rel="shortcut icon" type="image/x-icon" href="favicon.ico" />
<link rel="icon" type="image/png" href="favicon.png" />
<style type="text/css">
p.c {margin-left:auto; margin-right:auto; text-align:center;}
table.c {margin-left:auto; margin-right:auto;}
div.c {margin-left:auto; margin-right:auto; text-align:center;}
#meter_frame { background-color:gray; width:300px; height:25px; padding:1px; }
#meter_back  { background-color:white; padding:5px; }
#meter_text  { text-align:center; height:0%; }
#meter_fore  { background-color:lightblue; left:0%; width:<?php echo $pct; ?>%; height:100%; margin:-5px; }
?>
</style>
<?php
if ($onload) {
	echo "<script><!--\n" .
		"onLoad=" . $onload . "\n" .
		"// -->\n" .
		"</script>\n";
}
?>
</head>
<body>
<p class="c">
<?php
echo $body;
?>
</p>
<?php
if ($info) {
?>
<p class="c">
<div class="c" id="meter_frame">
<div id="meter_back">
<div id="meter_text">
<?php
	 echo round($pct);
?>%
</div>
<div id="meter_fore">
</div>
</div>
</div>
</p>
	  
<?php
 $uv=$info['upl']['val'];
 $us=$info['upl']['suff']; 
 $tv=$info['total']['val'];
 $ts=$info['total']['suff']; 
 echo '<table class="c">';
 echo '<tr><td>';
 if ($us == $ts) {
	 echo $uv . ' / ' . $tv . ' ' . $us . 'B';
 } else {
	 $uv . $us . 'B / ' . $tv . $ts . 'B';
 }
 echo '</td></tr>';
 echo '<tr><td>';
 echo $info['time'] . " so far";
 echo '</td></tr>';
 echo '<tr><td>';
 echo $info['eta'] . " remaining";
 echo '</td></tr>';
 echo '</table>';
}
?>
</body>
</html>

