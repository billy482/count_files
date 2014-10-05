#! /usr/bin/perl

use strict;
use warnings;

my $ref;

open my $fd, '<', '.git/HEAD';
while (<$fd>) {
    if (/^ref: (.+)$/) {
        $ref = $1;
    }
    else {
        exit 0;
    }
}
close $fd;

my $filename = ".git/$ref";

if ( -e $filename ) {
    print "$filename\n";
    exit 0;
}

$filename = ".git/logs/$ref";

if ( -e $filename ) {
    print "$filename\n";
    exit 0;
}

