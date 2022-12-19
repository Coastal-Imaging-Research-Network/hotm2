#!/usr/bin/perl

$| = 1;
$hotm = "./hotm";

$ENV{'PATH'} = '/bin:/usr/bin';
$ENV{'LD_LIBRARY_PATH'} = '/usr/local/lib:.';

foreach $in ( @ARGV ) {

	print "looking at $in\n";

	undef $c;
	$c = $1 if $in =~ /(\d+)/;
	next unless defined $c;

	# stupid pt grey, now I must determine what kind of camera I am using
	open( O, "<cameraMapping" ) || die "cannot open cameraMapping: $!\n";
	while( <O> ) {
		chomp;
		@d = split( ' ', $_ );
		if( $d[0] == $c ) {
			#$hotm = "./" . $d[2];
			printf( "changing hotm to $hotm\n" );
			last;
		}
	}

	# zero-filled-c -- 01 for cam1, 17 for cam 17
	$zfc = $c;
	$zfc = '0' . $c if $c < 10;

	unlink "/tmp/cam$zfc.log";
	system("/usr/bin/ipcrm -Q $c");

	# let's redirect stdout, stderr.
	do {
		$ENV{'HOTM_STDOUT'} = "/tmp/cam$zfc.stdout";
		$ENV{'HOTM_STDERR'} = "/tmp/cam$zfc.stderr";
	} unless defined $ENV{'HOTM_STDOUT'};

	$pid = fork();

	if( $pid == 0 ) {
		#$< = $> = 412;
		exec("$hotm -c $c cdisk cam$c");
		die "failed exec! $!\n";
	}

	print "sleeping during init ...";

	# wait for the queue to show up, I did remove it earlier ...
	$waitForMe = 1;
	while( $waitForMe ) {
		open( Q, "/usr/bin/ipcs -q |" ) || die "cannot ipcs: $!\n";
		while( <Q> ) {
			next unless /^0x(0)+(.+) /;
			$waitForMe = 0 if hex($2) == $c;
		}
		sleep(1);
	}

	system( "/usr/bin/renice -20 $pid" );

	do {
	while(<I>) {
		chomp;
		next if /^#/;
		next if /^$/;
		$i = $1 if /(.+)/;
		system("./send $c $i") if $i ne "";
	}
	close I;
	} if open(I, "<./cam0X.startup" );

	do {
	while(<I>) {
		chomp;
		next if /^#/;
		next if /^$/;
		$i = $1 if /(.+)/;
		system("./send $c $i") if $i ne "";
	}
	close I;
	} if open(I, "<./cam$zfc.startup" );


	print "start done end\n";
	#close STDOUT;

}
