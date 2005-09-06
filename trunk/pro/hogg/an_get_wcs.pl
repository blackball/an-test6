#!/usr/bin/env perl

my $name = $ARGV[0];
my $input = `cat $name.hits`;
my $parity= 0;
@records = split "--------------------\n", $input;
printf("set_plot, \'PS\'\n");
printf("device");
foreach $record (@records){
    if ($record =~ /field\s(\d+)\s/){
	my $field = $1;
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

	@lines = split "\n", $record;
	foreach $line (@lines){
	    if ($line =~ /min\s+uv\=\((\-?\d+.\d+)\,(\-*\d+.\d+)\)/){
		$minu= $1;
		$minv= $2;
	    }
	    if ($line =~ /max\s+uv\=\((\-?\d+.\d+)\,(\-*\d+.\d+)\)/){
		$maxu= $1;
		$maxv= $2;
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
	
	$parity= 1;
    
	my $minnorm= sqrt($minx*$minx+$miny*$miny+$minz*$minz);
	if ($minnorm > 0.99){
	    $minx/= $minnorm;
	    $miny/= $minnorm;
	    $minz/= $minnorm;
	    my $maxnorm= sqrt($maxx*$maxx+$maxy*$maxy+$maxz*$maxz);
	    $maxx/= $maxnorm;
	    $maxy/= $maxnorm;
	    $maxz/= $maxnorm;
	
	    printf("!P.TITLE=\'field %d\'\n",$field);
	    printf("; min u , v = %f , %f\n",$minu,$minv);
	    printf("; min x , y , z = %f , %f , %f\n",$minx,$miny,$minz);
	    printf("; max u , v = %f , %f\n",$maxu,$maxv);
	    printf("; max x , y , z = %f , %f , %f\n",$maxx,$maxy,$maxz);
	    printf("cat= mrdfits(\'%s.fits*\',%d)\n",$name,$field+1);
	    printf("astr= an_get_wcs([[%f,%f,%f,%f,%f], \$\n",
		   $minu,$minv,$minx,$miny,$minz);
            printf("                  [%f,%f,%f,%f,%f]], \$\n",
		   $maxu,$maxv,$maxx,$maxy,$maxz);
	    printf("                 cat.rowc,cat.colc,siporder=2, \$\n");
	    printf("                 parity=%d)\n",
		   $parity);
	    printf(";\n");
	}
    }
}
printf("device, /close\n");
