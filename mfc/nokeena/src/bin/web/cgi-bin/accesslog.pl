#!/usr/bin/perl
use File::Basename;

my $seed;
my $type; # type = domain, uri, file_size, resp_code
my $h; # for time interval in hour
my $m; # for time interval min
my $n; # for depth
my $D; # for domain names
# -d for domain list
# -u for uri list
# -f for file-size 
# -r for resp code

my $param;

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
if ( $in{'type'} eq '' ) { $type = "domain"; }
else { $type = $in{'type'}; }
if ( $in{'h'} eq '' ) { $h = "0"; }
else { $h = $in{'h'}; }
if ( $in{'m'} eq '' ) { $m = "0"; }
else { $m = $in{'m'}; }
if ( $in{'n'} eq '' ) { $n = "20"; }
else { $n = $in{'n'}; }
if ( $in{'D'} eq '' ) { $D = "0.0.0.0"; }
else { $D = $in{'D'}; }
########################################
#===============debug
#$h=0;
#$m=0;
#$n=10;
#$D="172.19.172.133";
#$type= "uri";

#For Domain type get only domain list
if ($type eq "domain"){
	$param = "-h ".$h." -m".$m." -n ".$n." -d"." -e";
}

# For uri type, we need the domain name
if ($type eq "uri"){
	$param = "-h ".$h." -m".$m." -n ".$n." -D ".$D." -u";
}

# For filesize type
if ($type eq "file_size"){
        $param = "-h ".$h." -m".$m." -f";
}

# For respcode type
if ($type eq "resp_code"){
        $param = "-h ".$h." -m".$m." -r";
}


print "Content-type: text/html\n\n";
#print "$param\n\n";


#$out=`/opt/tms/lib/web/cgi-bin/querylog $t $n $d`;
$out=`/opt/nkn/bin/querylog $param`;
#$out=`/opt/tms/lib/web/cgi-bin/querylog $param`;
print "$out";

