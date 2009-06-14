<?php

// Configuration info for index-700

$ip = array();
for ($i=0; $i<12; $i++)
	 array_push($ip, sprintf('700/index-700-%02d', $i));
$index700paths = $ip;

$ip = array();
for ($i=0; $i<12; $i++)
	 array_push($ip, sprintf('700/index-701-%02d', $i));
$index701paths = $ip;

$ip = array();
for ($i=0; $i<12; $i++)
	 array_push($ip, sprintf('700/index-702-%02d', $i));
$index702paths = $ip;

$ip = array();
for ($i=0; $i<12; $i++)
	 array_push($ip, sprintf('700/index-703-%02d', $i));
$index703paths = $ip;

$ip = array();
for ($i=0; $i<12; $i++)
	 array_push($ip, sprintf('700/index-704-%02d', $i));
$index704paths = $ip;

$indexdata =
array('90degree' => array('desc' => '90-degree Fields',
						  'quadsize' => array(1400, 2000),
						  'paths' => array('700/index-719')),
	  '60degree' => array('desc' => '60-degree Fields',
						  'quadsize' => array(1000, 1400),
						  'paths' => array('700/index-718')),
	  '45degree' => array('desc' => '45-degree Fields',
						  'quadsize' => array(680, 1000),
						  'paths' => array('700/index-717')),
	  '30degree' => array('desc' => '30-degree Fields',
						  'quadsize' => array(480, 680),
						  'paths' => array('700/index-716')),
	  '25degree' => array('desc' => '25-degree Fields',
						  'quadsize' => array(340, 480),
						  'paths' => array('700/index-715')),
	  '15degree' => array('desc' => '15-degree Fields',
						  'quadsize' => array(240, 340),
						  'paths' => array('700/index-714')),
	  '10degree' => array('desc' => '10-degree Fields',
						  'quadsize' => array(170, 240),
						  'paths' => array('700/index-713')),
	  '8degree' => array('desc' => '8-degree Fields',
						  'quadsize' => array(120, 170),
						  'paths' => array('700/index-712')),
	  '5degree' => array('desc' => '5-degree Fields',
						  'quadsize' => array(85, 120),
						  'paths' => array('700/index-711')),
	  '4degree' => array('desc' => '4-degree Fields',
						  'quadsize' => array(60, 85),
						  'paths' => array('700/index-710')),
	  '2.5degree' => array('desc' => '2.5-degree Fields',
						  'quadsize' => array(42, 60),
						  'paths' => array('700/index-709')),
	  '2degree' => array('desc' => '2-degree Fields',
						  'quadsize' => array(30, 42),
						  'paths' => array('700/index-708')),
	  '1.5degree' => array('desc' => '1.5-degree Fields',
						  'quadsize' => array(22, 30),
						  'paths' => array('700/index-707')),
	  '1degree' => array('desc' => '1-degree Fields',
						  'quadsize' => array(16, 22),
						  'paths' => array('700/index-706')),
	  '40arcmin' => array('desc' => '40-arcmin Fields',
						  'quadsize' => array(11, 16),
						  'paths' => array('700/index-705')),
	  '30arcmin' => array('desc' => '30-arcmin Fields',
						  'quadsize' => array(8, 11),
						  'paths' => $index704paths),
	  '20arcmin' => array('desc' => '20-arcmin Fields',
						  'quadsize' => array(5.6, 8),
						  'paths' => $index703paths),
	  '15arcmin' => array('desc' => '15-arcmin Fields (eg, Sloan Digital Sky Survey)',
						  'quadsize' => array(4, 5.6),
						  'paths' => $index702paths),
	  /*
	  '10arcmin' => array('desc' => '10-arcmin Fields',
						  'quadsize' => array(2.8, 4),
						  'paths' => $index701paths),
	  '8arcmin' => array('desc' => '8-arcmin Fields',
						 'quadsize' => array(2, 2.8),
						 'paths' => $index700paths),
	  */
	  );

$largest_index = '90degree';
//$smallest_index = '8arcmin';
$smallest_index = '15arcmin';

?>
