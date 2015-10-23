#!/usr/bin/perl

#
#

# This script is to help check if the PTR/doc/design/cli-commands.txt file is
# in sync with the actual CLI commands.  To use it:
#
# 1) On a product box do:
#        cli -d > cli-raw.txt
#        scp cli-raw.txt User@BuildMachine:
#
# 2) On a build machine do:
#
#   cat doc/design/cli-commands.txt | src/release/cli_doc_parse.pl | sort | uniq > cli-parsed.txt
#   cat cli-raw.txt | sed 's/ (hidden)//' | sed 's/ (terminal)//' | grep -v ' (deprecated)' | sort | uniq > cli-dump.txt
#
#   tkdiff -b cli-dump.txt cli-parsed.txt
#
#   There are expected to be two types of diff:
#   - Places where the registered cli command has a wildcard, but the docs
#     list the accepted literals
#
#   - Commands that use the | notation to specify optional, re-ordable parameters
#
#   Additionally, you could get diffs if you don't have all the features
#   turned on, so for the DEMO product you should use SAMARA_DEMO_ALL.
#

use strict;
use strict 'refs';
use warnings;
use Data::Dumper;
$Data::Dumper::Indent = 3;

sub expand_line {
    my $line = shift;
    chomp $line;

    my $pre_line = $line;

##    print "sel: .a.${pre_line}.b.\n";

    my $part_start;
    my $part_middle;
    my $part_end;
    my $line_n1;
    my $line_n2;
    my @middle_words;
    my $this_mw;

    # Convert <literalname> to <*>
    while ($line =~ /<[^>]*>/) {
        $line =~ s/<[^>]*>/\*/;
    }
##    print "elas: .a.${line}.b.\n";

    # Expand 'foo [bar] baz' to 'foo bar baz' and 'foo baz'
    if ($line =~ /\[[^\]]*\]/) {
        ($part_start, $part_middle, $part_end) = 
            ($line =~ /^(.*)[ ]*\[([^\]]*)\][ ]*(.*)[ ]*$/);

        if (!defined($part_start)) { $part_start=""; }
        if (!defined($part_middle)) { $part_middle=""; }
        if (!defined($part_end)) { $part_end=""; }

        if ($part_start ne "") {
            $part_start =~ s/^\s+|\s+$//g;
        }
        if ($part_middle ne "") {
            $part_middle =~ s/^\s+|\s+$//g;
        }
        if ($part_end ne "") {
            $part_end =~ s/^\s+|\s+$//g;
        }

##        print "p0: .a.$part_start.b. ; p1: .a.$part_middle.b. ; p2: .a.$part_end.b. .\n";
        $line_n1 = "$part_start $part_middle $part_end";
        $line_n2 = "$part_start $part_end";

        $line_n1 =~ s/^\s+|\s+$//g;   # Delete leading / trailing spaces
        $line_n1 =~ s/\s+/ /g;        # Collapse any remaing spaces to 1
        $line_n2 =~ s/^\s+|\s+$//g;   # Delete leading / trailing spaces
        $line_n2 =~ s/\s+/ /g;        # Collapse any remaing spaces to 1

        expand_line($line_n1);
        expand_line($line_n2);
        return;
    }


    # Expand 'foo {a,b, c} bar' to 'foo a bar' 'foo b bar' 'foo c bar'
    if ($line =~ /\{[^\}]*\}/) {
        ($part_start, $part_middle, $part_end) = 
            ($line =~ /^([^\{]*)[ ]*\{([^\}]*)\}[ ]*(.*)[ ]*$/);

        if (!defined($part_start)) { $part_start=""; }
        if (!defined($part_middle)) { $part_middle=""; }
        if (!defined($part_end)) { $part_end=""; }

        if ($part_start ne "") {
            $part_start =~ s/^\s+|\s+$//g;
        }
        if ($part_middle ne "") {
            $part_middle =~ s/^\s+|\s+$//g;
        }
        if ($part_end ne "") {
            $part_end =~ s/^\s+|\s+$//g;
        }

        @middle_words=split('[,|]', $part_middle);
##        print "mw:\n ", Dumper(@middle_words);
        
        for ${this_mw} (@middle_words) {
            $this_mw =~ s/^\s+|\s+$//g;
            if ($this_mw eq "") {
                next;
            }

            $line_n1="$part_start $this_mw $part_end";
            $line_n1 =~ s/^\s+|\s+$//g;   # Delete leading / trailing spaces
            $line_n1 =~ s/\s+/ /g;        # Collapse any remaing spaces to 1

##            print "cel: .a.${line_n1}.b.\n";
            expand_line($line_n1);

        }

        return;
    }


    print "$line\n";
}

my $state = 0;
my $inputline;
my $inl2;

while(<>) {
    chomp;
    $inputline = $_;
    $inputline =~ s/^\s+|\s+$//g;   # Delete leading / trailing spaces
    $inputline =~ s/\s+/ /g;        # Collapse any remaing spaces to 1

##    print "li: $inputline\n";

    if (/CLI COMMAND LIST/) {
        if ($state == 0) {
            $state++;
        }
        else {
            $state--;
        }
        next;
    }
    if ($state == 1) {
        if  (!/^[^ =]/) {
            next;
        }
        if (/-NYI-/) {
            next;
        }
        if (/-DEPR-/) {
            next;
        }

##        print "lig: $inputline\n";

        # If we encounter a \ , that means that the line is continued
        while (index($inputline, '\\') != -1) {
            $inl2 = index($inputline, '\\');
            substr($inputline, $inl2, 1, ' ');

            $_=<>;
            chomp;
            $inputline = "$inputline $_";
            $inputline =~ s/^\s+|\s+$//g;   # Delete leading / trailing spaces
            $inputline =~ s/\s+/ /g;        # Collapse any remaing spaces to 1
        }

##        print "IL: $inputline\n";

        expand_line($inputline);
        next;
    }
}
