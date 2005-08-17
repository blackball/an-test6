#!/usr/bin/env perl

my $input = `cat $ARGV[0]`;
my $parity= 0;
my $minu= 0.0;
my $minv= 0.0;
my $maxu= 0.0;
my $maxv= 0.0;
my $minx= 0.0;
my $miny= 0.0;
my $minz= 0.0;
my $maxx= 0.0;
my $maxy= 0.0;
my $maxz= 0.0;
@lines = split "\n", $input;
foreach $line (@lines){
    if ($line =~ /min\s+uv\=\((\-?\d+.\d+)\,(\-*\d+.\d+)\)/){
	$minu= $1;
	$minv= $2;
	printf("; min u , v = %f , %f\n",$minu,$minv);
    }
    if ($line =~ /max\s+uv\=\((\-?\d+.\d+)\,(\-*\d+.\d+)\)/){
	$maxu= $1;
	$maxv= $2;
	printf("; max u , v = %f , %f\n",$maxu,$maxv);
    }
    if ($line =~ /min\s+xyz\=\((\-?\d+.\d+)\,(\-*\d+.\d+)\,(\-*\d+.\d+)\)/){
	$minx+= $1;
	$miny+= $2;
	$minz+= $3;
    }
    if ($line =~ /max\s+xyz\=\((\-?\d+.\d+)\,(\-*\d+.\d+)\,(\-*\d+.\d+)\)/){
	$maxx+= $1;
	$maxy+= $2;
	$maxz+= $3;
    }
}

# parity hack
if ($minu < 0.0){
    $minu*= -1.0;
    $maxu*= -1.0;
    $parity= 1;
}

my $minnorm= sqrt($minx*$minx+$miny*$miny+$minz*$minz);
$minx/= $minnorm;
$miny/= $minnorm;
$minz/= $minnorm;
my $maxnorm= sqrt($maxx*$maxx+$maxy*$maxy+$maxz*$maxz);
$maxx/= $maxnorm;
$maxy/= $maxnorm;
$maxz/= $maxnorm;

printf("; min x , y , z = %f , %f , %f\n",$minx,$miny,$minz);
printf("; max x , y , z = %f , %f , %f\n",$maxx,$maxy,$maxz);

printf("astr= hogg_points2astr([[%f,%f,%f,%f,%f],[%f,%f,%f,%f,%f]],%d)\n",
       $minu,$minv,$minx,$miny,$minz,
       $maxu,$maxv,$maxx,$maxy,$maxz,
       $parity);
printf("xy2ad, %f,%f,astr,mina,mind\n",
       $minu,$minv);
printf("help, mina,mind\n");
printf("xy2ad, %f,%f,astr,maxa,maxd\n",
       $maxu,$maxv);
printf("help, maxa,maxd\n");
