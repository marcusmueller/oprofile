#! /usr/bin/perl

# stupid script to calc averages out of time reports in a file
# usage: time-aves.pl baseline.out result1.out result2.out ... 

# 499.63user 52.49system 9:15.71elapsed 99%CPU (0avgtext+0avgdata 0maxresident)k
 
# use --gnuplot option for errorbars input to gnuplot, e.g. :
# set grid
# set ytics 1
# set term fig big color
# set output 'filename.fig'
#  plot [0:7] 'ccuk.gnuplot' with yerrorbars

if ($ARGV[0] eq "--gnuplot") {
	$gnuplot = 1;
	shift;
}

$fcount = 1;
$first = 1;
while ($_ = $ARGV[0]) {
	$file = $_;
	shift(@ARGV);
	open(FILE, $file) || die "Couldn't open $file\n"; 
	$count = 0; 
	$totuser = 0;
	$totsystem = 0;
	$totelapsed = 0;
	$minelapsed = 9999999999999990;
	$maxelapsed = 0;
	while (<FILE>) {
		next if ! /elapsed/ ;
		s/user//;
		s/system//;
		s/elapsed//;
		($user, $system, $elapsed) = split;
		($min, $sec) = split(':', $elapsed); 
		$totuser += $user;
		$totsystem += $system;
		$elapsed = (60*$min) + $sec;
		$totelapsed += $elapsed;
		if ($elapsed < $minelapsed) {
			$minelapsed = $elapsed;
		}
		if ($elapsed > $maxelapsed) {
			$maxelapsed = $elapsed;
		}
		$count++; 
	}
	# if not that format
	if ($totelapsed == 0) {
		seek(FILE, 0, 0); 
		while (<FILE>) {
			if (/real/) {
				($a,$b) = split(' ',$_);
				($min,$sec) = split('m',$b);
				$sec =~ s/s//;
				$elapsed = (60*$min) + $sec;
				$totelapsed += $elapsed;
				if ($elapsed < $minelapsed) {
					$minelapsed = $elapsed;
				}
				if ($elapsed > $maxelapsed) {
					$maxelapsed = $elapsed;
				}
				$count++;
			} elsif (/user/) {
				($a,$b) = split(' ',$_);
				($min,$sec) = split('m',$b);
				$sec =~ s/s//;
				$totuser += (60*$min) + $sec;
			} elsif (/sys/) {
				($a,$b) = split(' ',$_);
				($min,$sec) = split('m',$b);
				$sec =~ s/s//; 
				$totsystem += (60*$min) + $sec;
			}
		}
	}
	if ($first) {
		$first = 0;
		$origuser = $totuser/$count;
		$origsys = $totsystem/$count;
		$origelapsed = $totelapsed/$count;
	}
	if (defined($gnuplot)) {
		printf("# %s\n",$file); 
		printf("%d %.2f %.2f %.2f\n", $fcount, (( ($totelapsed/$count) - $origelapsed) / $origelapsed ) * 100.0,
			(( ($minelapsed) - $origelapsed) / $origelapsed ) * 100.0,
			(( ($maxelapsed) - $origelapsed) / $origelapsed ) * 100.0);
	} else {
		printf("%.2f (%.2f%%)\t| ", $totuser/$count, (( ($totuser/$count) - $origuser) / $origuser ) * 100.0);
		printf("%.2f (%.2f%%) \t| ", $totsystem/$count, (( ($totsystem/$count) - $origsys) / $origsys ) * 100.0);
		printf("%.2f (%.2f%%) \t| %s\n", $totelapsed/$count, (( ($totelapsed/$count) - $origelapsed) / $origelapsed ) * 100.0, $file);
	}
	close(FILE);
	$fcount ++;
}
