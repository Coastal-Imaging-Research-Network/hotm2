#! /usr/bin/perl

# take the file named on the command line and put time in it.
# also any text after the file name
# timelabel 123456.245.c1.jpg inclmnt 
#


$in = shift || die "No input file name!\n";
do {
    die "No such file $in\n";
} unless -e $in;

while( defined( $txt = shift ) ) {
    $addnl .= $txt . " ";
}
print "processing $in with additional text $addnl\n";

@d = split(/\./,$in);
$when = shift(@d);  # epoch number
#die "file is not a time: $when\n" unless $when =~ "/d+";

$when = scalar(gmtime($when));

while(<DATA>) {
    chomp;
    $comm .= $_ . " ";
}

$comm .= sprintf " -draw \'text 32,2015 \"%s %s %s\"\'", $when, $in, $addnl;

$comm .= sprintf " $in tmp.jpg";

print $comm . "\n";
system($comm);

rename "tmp.jpg", $in;



__DATA__
/usr/bin/convert
-font courier-bold 
-pointsize 60 
-fill white

