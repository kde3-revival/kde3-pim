#!/usr/bin/perl
# kdepim_combine_headers.pl [--keep-config] <header>...
#
# The ical.h and icalss.h rules of libkcal/libical: the headers, in the
# given order, inside extern "C" for C++, without the #includes of the
# other libical headers ("ical*", "pvl.h" and, unless --keep-config,
# "config*") and the RCS $Id$/$Locker$ lines:
#
#   echo '#ifdef __cplusplus' > ical.h ... cat $(COMBINEDHEADERS)
#   | egrep -v "#include.*\"ical" | egrep -v "#include.*\"config"
#   | egrep -v "#include.*\"pvl\.h\"" | egrep -v '\$(Id|Locker): .+\$'
#   >> ical.h ... echo '#endif' >> ical.h
use strict;
use warnings;

my $config = 1;
if (@ARGV && $ARGV[0] eq '--keep-config') {
    $config = 0;
    shift @ARGV;
}
print "#ifdef __cplusplus\nextern \"C\" {\n#endif\n";
for my $file (@ARGV) {
    open(my $fh, '<', $file) or die "$file: $!\n";
    while (my $line = <$fh>) {
        next if $line =~ /#include.*"ical/;
        next if $config && $line =~ /#include.*"config/;
        next if $line =~ /#include.*"pvl\.h"/;
        next if $line =~ /\$(Id|Locker): .+\$/;
        print $line;
    }
    close($fh);
}
print "#ifdef __cplusplus\n}\n#endif\n";
