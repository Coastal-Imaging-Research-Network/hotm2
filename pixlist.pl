#!/usr/bin/perl

for( $x=110; $x<1024; $x+=85 ) {

	for( $xx=0; $xx<20; $xx++ ) {

		print $x+$xx . " 210\n";
	}
}

