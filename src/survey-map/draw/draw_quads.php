<?php

	#usage: php draw_quads.php > output.png
	#need to manually uncomment one of the following lines to specifiy input file

	$filename = "Possi.boundaries.merc";
	#$filename = "Possii.boundaries.merc";
	#$filename = "2mass.boundaries.merc";
	#$filename = "South.boundaries.merc";
	#$filename = "Possi+South.boundaries.merc";
	#$filename = "AAOR.boundaries.merc";

	Header("Content-type: image/png");
	$height = pow(2, 12);
	$width = $height;
	$im = ImageCreate($width, $height);
	
	#for testing#
	$factor = 1; #please use an even number. use 1 for no scaling
	$originx = $width * ($factor-1)/($factor * 2);
	$originy = $height * ($factor-1)/($factor * 2);
	$width = $width/$factor;
	$height = $height/$factor;
	#for testing#
	
	$bck = ImageColorAllocate($im, 0,0,50);
	$white = ImageColorAllocate($im, 255, 255, 255);
	ImageFill($im, 0, 0, $bck);
	
	$file = fopen($filename, 'r');
	
	$i = 0;
	$startx=0;
	$starty=0;
	$endx = trim(fgets($file)) * $width + $originx;
	$endy = trim(fgets($file)) * $height + $originy;
	$firstx= $endx;
	$firsty= $endy;
	while(!feof($file)) {
		if ($i == 3) {
			ImageLine($im, $endx, $endy, $firstx, $firsty, $white);
		}
		else {
			if ($i == 4) {
				#break;
				fgets($file);
				$i = 0;
				$startx = trim(fgets($file)) * $width + $originx;
				$starty = trim(fgets($file)) * $height + $originy;
				$firstx = $startx;
				$firsty = $starty;
			}
			else {
				$startx = $endx;
				$starty = $endy;
			}
			$endx = trim(fgets($file)) * $width + $originx;
			$endy = trim(fgets($file)) * $height + $originy;
			ImageLine($im, $startx, $starty, $endx, $endy, $white);
		}
		$i = $i + 1;
	}
	
	fclose($file);
	ImagePNG($im);
?>