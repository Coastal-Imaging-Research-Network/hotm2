#! /usr/bin/perl

$which = shift;
$when = time;
$where = "/data/stanley/$when.raw";
$me = $$;
$SIG{'ALARM'} = \&goaway;

$| = 1;
alarm 10;
$pipe = sprintf( "/tmp/cam%02dd", $which );
print "adding collection snap to $pipe\n";
open( O, ">>$pipe" );
print O "add $where 1 0 - $me\n";
close O;

alarm 0;

print "now I'm waiting for end...\n";
kill 'STOP', $me;

print "now I'm not waiting anymore!\n";

print "do something with $where, doofus\n";
system( "./src/postproc $where" );
unlink $where;

sub goaway {
	print "dies due to alarm\n";
	exit;
}
