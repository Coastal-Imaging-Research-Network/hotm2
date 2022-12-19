#! /usr/bin/perl

$when = time;
$where = "/d3/stanley/$when.raw";
$me = $$;

$| = 1;
print "adding collection\n";
system( "echo add $where 600 2 - $me >> /tmp/cam12d" );

print "now I'm waiting for end...\n";
kill 'STOP', $me;

print "now I'm not waiting anymore!\n";

print "do something with $where, doofus\n";
system( "./a.out $where" );
unlink $where;

