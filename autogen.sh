#!/bin/sh

# run to generate needed files not in CVS

# this simple currently

run() {
	echo "Running $1 ..."
	$1
}

run aclocal
run autoheader
run "automake --foreign --add-missing --copy"
run autoconf
