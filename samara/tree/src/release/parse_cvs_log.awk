#!/usr/bin/awk

{ if (after_minus_bar == 0)
   if (/^RCS file: .*,v$/) {
      sub(/^RCS file: /,"")
      sub(/,v$/,"")
      printf ""  $0 "  ---  "
   } 
}

/^---------/ { if (after_minus_bar != 0)
                    print "minus bar after minus bar", NR
                    after_minus_bar = 1 
                    next
             }
/^=========/ { if (after_minus_bar != 1)
                  print "equal bar not after minus bar", NR
                  after_minus_bar = 0
                  print $0
             }
{ if (after_minus_bar == 1)
   print $0
}
