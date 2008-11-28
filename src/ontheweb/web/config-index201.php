<?php

// Configuration info for index-200 through index-219.

/*
200+
0: nside 1760, quad size 2 to 2.8, dedup 8
1: nside 1245, quad size 2.8 to 4, dedup 8
2: nside 880, quad size 4 to 5.6, dedup 8
3: nside 623, quad size 5.6 to 8, dedup 8
4: nside 440, quad size 8 to 11, dedup 8
5: nside 312, quad size 11 to 16, dedup 9.6
6: nside 220, quad size 16 to 22, dedup 13.2
7: nside 156, quad size 22 to 30, dedup 18
8: nside 110, quad size 30 to 42, dedup 25.2
9: nside 78, quad size 42 to 60, dedup 36
10: nside 55, quad size 60 to 85, dedup 51
11: nside 39, quad size 85 to 120, dedup 72
12: nside 28, quad size 120 to 170, dedup 102
13: nside 20, quad size 170 to 240, dedup 144
14: nside 14, quad size 240 to 340, dedup 204
15: nside 10, quad size 340 to 480, dedup 288
16: nside 7, quad size 480 to 680, dedup 408
17: nside 5, quad size 680 to 1000, dedup 600
18: nside 4, quad size 1000 to 1400, dedup 840
19: nside 3, quad size 1400 to 2000, dedup 1200
*/

/*
$ip = array();
for ($i=0; $i<12; $i++)
	 array_push($ip, sprintf('200/index-200-%02d', $i));
$index200paths = $ip;
 */

$ip = array();
for ($i=0; $i<12; $i++)
	 array_push($ip, sprintf('200/index-201-%02d', $i));
$index201paths = $ip;

$ip = array();
for ($i=0; $i<12; $i++)
	 array_push($ip, sprintf('200/index-202-%02d', $i));
$index202paths = $ip;

$ip = array();
for ($i=0; $i<12; $i++)
	 array_push($ip, sprintf('200/index-203-%02d', $i));
$index203paths = $ip;

$ip = array();
for ($i=0; $i<12; $i++)
	 array_push($ip, sprintf('200/index-204-%02d', $i));
$index204paths = $ip;

$indexdata =
array('90degree' => array('desc' => '90-degree Fields',
						  'quadsize' => array(1400, 2000),
						  'paths' => array('200/index-219')),
	  '60degree' => array('desc' => '60-degree Fields',
						  'quadsize' => array(1000, 1400),
						  'paths' => array('200/index-218')),
	  '45degree' => array('desc' => '45-degree Fields',
						  'quadsize' => array(680, 1000),
						  'paths' => array('200/index-217')),
	  '30degree' => array('desc' => '30-degree Fields',
						  'quadsize' => array(480, 680),
						  'paths' => array('200/index-216')),
	  '25degree' => array('desc' => '25-degree Fields',
						  'quadsize' => array(340, 480),
						  'paths' => array('200/index-215')),
	  '15degree' => array('desc' => '15-degree Fields',
						  'quadsize' => array(240, 340),
						  'paths' => array('200/index-214')),
	  '10degree' => array('desc' => '10-degree Fields',
						  'quadsize' => array(170, 240),
						  'paths' => array('200/index-213')),
	  '8degree' => array('desc' => '8-degree Fields',
						  'quadsize' => array(120, 170),
						  'paths' => array('200/index-212')),
	  '5degree' => array('desc' => '5-degree Fields',
						  'quadsize' => array(85, 120),
						  'paths' => array('200/index-211')),
	  '4degree' => array('desc' => '4-degree Fields',
						  'quadsize' => array(60, 85),
						  'paths' => array('200/index-210')),
	  '2.5degree' => array('desc' => '2.5-degree Fields',
						  'quadsize' => array(42, 60),
						  'paths' => array('200/index-209')),
	  '2degree' => array('desc' => '2-degree Fields',
						  'quadsize' => array(30, 42),
						  'paths' => array('200/index-208')),
	  '1.5degree' => array('desc' => '1.5-degree Fields',
						  'quadsize' => array(22, 30),
						  'paths' => array('200/index-207')),
	  '1degree' => array('desc' => '1-degree Fields',
						  'quadsize' => array(16, 22),
						  'paths' => array('200/index-206')),
	  '40arcmin' => array('desc' => '40-arcmin Fields',
						  'quadsize' => array(11, 16),
						  'paths' => array('200/index-205')),
	  '30arcmin' => array('desc' => '30-arcmin Fields',
						  'quadsize' => array(8, 11),
						  'paths' => $index204paths),
	  '20arcmin' => array('desc' => '20-arcmin Fields',
						  'quadsize' => array(5.6, 8),
						  'paths' => $index203paths),
	  '15arcmin' => array('desc' => '15-arcmin Fields (eg, Sloan Digital Sky Survey)',
						  'quadsize' => array(4, 5.6),
						  'paths' => $index202paths),
	  '10arcmin' => array('desc' => '10-arcmin Fields',
						  'quadsize' => array(2.8, 4),
						  'paths' => $index201paths),
      /*
	  '8arcmin' => array('desc' => '8-arcmin Fields',
						 'quadsize' => array(2, 2.8),
						 'paths' => $index200paths),
       */
	  );

$largest_index = '90degree';
//$smallest_index = '8arcmin';
$smallest_index = '10arcmin';

?>
