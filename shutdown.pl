#! /usr/bin/perl

use IPC::SysV qw(IPC_PRIVATE IPC_CREAT S_IRWXU S_IRWXG S_IRWXO);
use IPC::Msg;

$SIG{'ALRM'} = \&goaway;

$| = 1;
$what = "exit";

alarm 15;

$me = $$;
$msgIn = new IPC::Msg( $me, IPC_CREAT|S_IRWXU|S_IRWXG|S_IRWXO);
die "recvmsg create: $!\n" if !defined $msgIn;

foreach $c ( @ARGV ) {

	$msgOut = new IPC::Msg( $c, IPC_CREAT|S_IRWXU|S_IRWXG|S_IRWXO);
	die "sendmsg create: $!\n" if !defined $msgOut;

	$msgOut->snd( 1, pack( "L a*", $me, $what ));
	$in = $msgIn->rcv( $buf, 256 );

	($code, $resp) = unpack( 'L a*', $buf );
	print "cam $c says $resp ($code)\n";

	$msgOut->remove;

}

$msgIn->remove;

sub goaway {
	print "dies due to signal\n";
	$msgIn->remove;
	exit;
}
