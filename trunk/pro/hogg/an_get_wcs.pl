#!/usr/bin/env perl
#
# ./an_get_wcs.pl ../../data/galex-field-100
#
my $parity= 0;
my $name = $ARGV[0];
my $input = `cat $name.hits`;
my $minu= 0.0;
my $minv= 0.0;
my $maxu= 0.0;
my $maxv= 0.0;
printf("set_plot,'ps'\n");
printf("device\n");
if ($input =~ /\s*parity_flip\s*=\s*True/s ){
    $parity= 1;
}
if ($input =~ /\s*min_uv_corner\s*=\(([\-\d\.]+),([\-\d\.]+)\)/s ){
    $minu= $1;
    $minv= $2;
}
if ($input =~ /\s*max_uv_corner\s*=\(([\-\d\.]+),([\-\d\.]+)\)/s ){
    $maxu= $1;
    $maxv= $2;
}

if ($input =~ /\squads\s*=\s*\[(.+)\]/s ){
    $input= $1;
    my $minx= 0.0;
    my $miny= 0.0;
    my $minz= 0.0;
    my $maxx= 0.0;
    my $maxy= 0.0;
    my $maxz= 0.0;
    while ($input =~ /\s*dict\s*\((.+)/s ){
	$input= $1;
	if ($input =~ /\s*quad\s*=\s*(\d+)\,/){
	    my $quadid= $1;
	    if ($input =~ /\s*min_xyz\s*=\(([\-\d\.]+),([\-\d\.]+),([\-\d\.]+)\)/s ){
		$minx+= $1;
		$miny+= $2;
		$minz+= $3;
	    }
	    if ($input =~ /\s*max_xyz\s*=\(([\-\d\.]+),([\-\d\.]+),([\-\d\.]+)\)/s ){
		$maxx+= $1;
		$maxy+= $2;
		$maxz+= $3;
	    }
	}
    }

    my $minnorm= sqrt($minx*$minx+$miny*$miny+$minz*$minz);
    if ($minnorm > 0.99){
	$minx/= $minnorm;
	$miny/= $minnorm;
	$minz/= $minnorm;
	my $maxnorm= sqrt($maxx*$maxx+$maxy*$maxy+$maxz*$maxz);
	$maxx/= $maxnorm;
	$maxy/= $maxnorm;
	$maxz/= $maxnorm;
	
	printf(";\n");
	printf("!P.TITLE=\'field %d\'\n",$field);
	printf("cat= mrdfits(\'%s.fits*\',%d)\n",$name,$field+1);
	printf("astr= an_get_wcs([[%f,%f,%f,%f,%f], \$\n",
	       $minu,$minv,$minx,$miny,$minz);
	printf("                  [%f,%f,%f,%f,%f]], \$\n",
	       $maxu,$maxv,$maxx,$maxy,$maxz);

# for SDSS
#	printf("                 cat.rowc,cat.colc,siporder=2, \$\n");

# for GALEX
	printf("                 cat.nuv_x,cat.nuv_y,siporder=3, \$\n");

	printf("                 parity=%d)\n",
	       $parity);
	printf(";\n");
    }
}
printf("device,/close\n");
