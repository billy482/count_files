#! /usr/bin/perl

use strict;
use warnings;

my ( $symbol, $filename ) = @ARGV;

my ($version) = qx/git describe/;
chomp $version;

my ($branch) = grep {/^\*/} qx/LANG=C git branch/;
chomp $branch;
if ( $branch =~ /\(detached from \w+\)/ ) {
    $branch = 'detached';
}
else {
    $branch =~ s/^\* (.*)$/$1/;
}

$version .= '-' . $branch if $branch ne 'master';

my ($git_commit) = qx/git log -1 --format=%H/;
chomp $git_commit;

open my $fd, '>', $filename;
print $fd "#define ${symbol}_VERSION \"$version\"\n";
print $fd "#define ${symbol}_GIT_COMMIT \"$git_commit\"\n";
close $fd;

