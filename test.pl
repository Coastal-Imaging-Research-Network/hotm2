#!/usr/bin/perl
$cam = 4;

$gam = 128;
$gain = 2048;
system("./send.pl $cam autoshutter 0 0 0");
system("./send.pl $cam changegain $gain" );
system("./send.pl $cam setcamera gamma $gam" );
sleep 1;

for( $gam=128; $gam<131; $gam++ ) {

	system("./send.pl $cam setcamera gamma $gam" );
	$gain = 2048;
	system("./send.pl $cam changegain $gain" );
	sleep 1;

	for( $sh=2040; $sh<2051; $sh++ ) {
		system("./send.pl $cam changeshutter $sh" );
		sleep 1;
		&take;
	}
	for( $sh=2051; $sh<2842; $sh+=10 ) {
		system("./send.pl $cam changeshutter $sh" );
		sleep 1;
		&take;
	}

	for( $sh=2100; $sh<2900; $sh+=100 ) {
		system("./send.pl $cam changeshutter $sh" );
		sleep 1;

		for( $gain=2048; $gain<2229; $gain+=10 ) {
			system("./send.pl $cam changegain $gain" );
			sleep 1;
			&take;
		}
	}
}

sub take {
	system("./doCam $cam 20 0 0 a.pix" );
        (@f) = <1*.ras.Z>;
        rename $f[0], "g$gain.s$sh.gam$gam.ras.Z";
}


