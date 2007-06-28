#!/usr/bin/perl
######################################################################
# grab_alpha_addresses
#
# harvest addresses from index.php
######################################################################
my $file= './index.php';
open(INDEX, $file);
@lines= <INDEX>;
foreach $line (@lines){
    if ($line =~ /^.*\'([\w\.]+\@[\w\.]+)\'.*$/){
	print "$1,\n";
    }
}
