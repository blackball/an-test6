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
	
	$i=0;
	while(!feof($file)) {
		# get all 4 points
		$xs[0] = trim(fgets($file)) * $width + $originx;
		$ys[0] = trim(fgets($file)) * $height + $originy;
		$xs[1] = trim(fgets($file)) * $width + $originx;
		$ys[1] = trim(fgets($file)) * $height + $originy;
		$xs[2] = trim(fgets($file)) * $width + $originx;
		$ys[2] = trim(fgets($file)) * $height + $originy;
		$xs[3] = trim(fgets($file)) * $width + $originx;
		$ys[3] = trim(fgets($file)) * $height + $originy;
		
		# read the next empty line before going onto next loop iteration
		fgets($file);
		
		# draw boundaries
		for ($j=0; $j<4; $j++) {
			$next = ($j+1) % 4;
			
			# check if tile wraps over to the other side
			if (abs($xs[$j] - $xs[$next]) > (.75 * $width)) {
				# this tile wraps over so only draw half of it
				if ($xs[$next] > $xs[$j]) {
					ImageLine($im, $xs[$j], $ys[$j], $xs[$next]-$width, $ys[$next], $white);	
				}
				else {
					ImageLine($im, $xs[$j], $ys[$j], $xs[$next]+$width, $ys[$next], $white);
				}
			}
			else {
				# just draw a normal line between the two points
				ImageLine($im, $xs[$j], $ys[$j], $xs[$next], $ys[$next], $white);
			}
		}
	}
	
	# close file and output image
	fclose($file);
	ImagePNG($im);
?>