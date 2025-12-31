#!/usr/bin/env perl
use strict;
use warnings;

die "Usage: $0 output_file 1 import1 import2 ... importN\n" if @ARGV < 2;

my $output_file = shift @ARGV;
my $generate_cpp = shift @ARGV;
my @import_files = @ARGV;

open(my $fh, '>', $output_file) or die "Cannot open $output_file: $!";

my $import_macro = $generate_cpp ? "#include" : "#import";

foreach my $file (@import_files) {
    print $fh "$import_macro \"$file\"\n";
}

close($fh);

print "Generated $output_file with $import_macro statements for provided files.\n";
