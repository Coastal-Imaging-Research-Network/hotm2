#! /usr/bin/perl

$which = shift;
$howmany = shift;
$when = time;
$where = "/data/stanley/$when.c$which.raw";
$me = $$;
$SIG{'ALARM'} = \&goaway;

$| = 1;
$pipe = sprintf( "/tmp/cam%02dd", $which );
print "adding collection to $pipe\n";
alarm 10;
open( O, ">>$pipe" );
print O "add $where $howmany 1 - $me\n";
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
