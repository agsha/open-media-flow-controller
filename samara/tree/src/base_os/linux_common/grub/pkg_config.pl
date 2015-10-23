# Notes
#

# issue: popt and rpm don't have a ./ in their file list names!

@base_excludes=('/usr/share/doc/', '/usr/share/man/', '/usr/share/info/',
                '/usr/share/locale/', '/usr/man/', '/usr/doc/', 
                '/usr/kerberos/man/',
                '/usr/include/', '/etc/sysconfig/', '/etc/X11/', 
                '/usr/share/misc/', '/usr/lib*/debug/', '/etc/cron.d/',
                '/usr/lib/pkgconfig/'
                );

# There can only be includes or excludes for a package, not both

@excludes_grub=('/boot/', '/usr/share/grub/*/stage2_eltorito',
                '/usr/bin/mbchk');

