#!/usr/bin/perl
use strict;
use Getopt::Long;
use Data::Dumper;

my ($dir,$debug,$mytokens);
$dir = '.';
GetOptions(
	   'd|dir:s' => \$dir,
           'debug:s' => \$debug,
           't|tokens:s' => \$mytokens,
          );

my $models;

my @tokens = (
              'MXMX','MXDS','_DSM',
              '0xA6, 0xFA, 0xDD, 0x3D, 0x1B, 0x36, 0xB4, 0x4E',
              '0xA4, 0x24, 0x8D, 0x10, 0x08, 0x9D, 0x16, 0x53');

if (defined $mytokens) {
    @tokens = ();
    foreach my $mytoken (split(":",$mytokens)) {
        push @tokens, $mytoken;
    }
}

opendir DIR,$dir or die $!;
my $file;
my $count = 0;
while (( defined($file = readdir(DIR)) )) {
    if ($file =~ /\.dsl$/) {
        analyse_file($dir,$file);
    }
    last if ($count++ > $debug && defined($debug));
}

foreach my $file (sort keys %$models) {
    my $num = scalar keys %{$models->{$file}}; $num = sprintf("%02d",$num);
    print "$num $file \{";
    foreach my $token (sort keys %{$models->{$file}}) {
        my $value = $models->{$file}{$token};
        print "\t$token => $value ";
    }
    print "\}\n";
}
$DB::single=1;1;
print STDERR Dumper($models) if ($debug);

########################################

sub analyse_file {
    my $dir  = shift;
    my $file  = shift;
    my $param = shift;

    open FILE, "$dir/$file" or die $!;
    while (<FILE>) {
        foreach my $token (@tokens) {
            my $regexp = qr/${token}/;
            if ($_ =~ /$regexp/g) {
                $models->{$file}{$token}++;
            }
        }
    }
    close FILE;
}
