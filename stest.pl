#! /usr/bin/perl

use IPC::SysV qw(IPC_PRIVATE IPC_CREAT S_IRWXU S_IRWXG S_IRWXO);
use IPC::Msg;

$which = shift;
$howmany = shift;
($skip = shift) || ($skip = 0);
($delay = shift) || ($delay = 0);
($quiet = shift) || ($quiet = '');
$now = $when = time;
$when = $when+20;
$where = "/home/users/stanley/hotm/$when.c$which.stack.ras";
$me = $$;
$SIG{'ALARM'} = \&goaway;
$width = 400;

$what = "add $where $howmany $skip $when 400 bogusaoifilename";
@U = ( 1 .. $width );
@V = ( 1 .. $width );
foreach $Ut (@U) {
	push( @UV, $Ut );
	push( @UV, shift(@V) );
} 

$| = 1;
print "$now: adding collection: $what\n";
alarm 10;

$msgOut = new IPC::Msg( $which, IPC_CREAT|S_IRWXU|S_IRWXG|S_IRWXO);
die "sendmsg create: $!\n" if !defined $msgOut;
$msgIn = new IPC::Msg( $me, IPC_CREAT|S_IRWXU|S_IRWXG|S_IRWXO);
die "recvmsg create: $!\n" if !defined $msgIn;

$msgOut->snd( 1, pack( "L a*", $me, $what ));

# here we wait for immediate ok 
$in = $msgIn->rcv( $buf, 256 );

($code, $resp) = unpack( "L a*", $buf );
print "$in: $code: $resp\n";
do {
	print "error response from hotm $code, type $in\n";
	print "answer was: $resp\n";
	$msgIn->remove;
	exit;
} if $in != 1;

system("./send.pl $which status");

# now send the pixlist -- type 5
$msgOut->snd( 5, pack( "L S*", $me, @UV ));

$in = $msgIn->rcv( $buf, 256 );
($code, $resp) = unpack( "L a*", $buf );
print "$in: $code: $resp\n";
do {
	print "error response from hotm $code, type $in\n";
	print "answer was: $resp\n";
	$msgIn->remove;
	exit;
} if $in > 10;

$msgOut->snd( 5, pack( "L", $me ));

$in = $msgIn->rcv( $buf, 256 );
($code, $resp) = unpack( "L a*", $buf );
print "$in: $code: $resp\n";
do {
	print "error response from hotm $code, type $in\n";
	print "answer was: $resp\n";
	$msgIn->remove;
	exit;
} if $in > 10;

$msgIn->remove;
exit;
 
sub goaway {
	print "dies due to alarm\n";
	$msgIn->remove;
	exit;
}
