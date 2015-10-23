#!/usr/bin/perl
use File::Basename;

my $seed;
my $file;

my $path = "/var/nkn/dashboard/";
my $filetype = ".png";
my $type;

##############################################################
# Parsing Query_string
##############################################################
if (length ($ENV{'QUERY_STRING'}) > 0){
      $buffer = $ENV{'QUERY_STRING'};
      @pairs = split(/&/, $buffer);
      foreach $pair (@pairs){
           ($name, $value) = split(/=/, $pair);
           $value =~ s/%([a-fA-F0-9][a-fA-F0-9])/pack("C", hex($1))/eg;
           $in{$name} = $value;
      }
}
$seed = $in{'seed'};
if(  $in{'file'} eq '' ) { $file = "nokeena"; }
else { $file = $in{'file'}; }
if(  $in{'type'} eq '' ) { $type = "png"; $filetype = ".png"; }
else { $type = $in{'type'}; $filetype = "." . $type; }

##############################################################

my $filename = $path . $file . $filetype;

# should open a generic file like 'nokeena' if not found
open (MYFILE, "$filename") or die "Can't open '$filename' because $!";

flock(MYFILE, 1);

# output the chart
if ($filetype eq ".png"){
	binmode(STDOUT);
	print "Pragma: No-cache\n";
	print "Cache-Control: No-cache\n";
	print "Content-type: image/png\n\n";
	while (read(MYFILE, $buff, 8*2**10)){
		print $buff;
	}
}

if ($filetype eq ".html"){

        print "Content-type: text/html\n\n";
        while (read(MYFILE, $buff, 8*2**10)){
                print $buff;
        }
}
close(MYFILE);
