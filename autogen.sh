# run to generate needed files not in CVS

# this simple currently

aclocal 
autoheader
automake --foreign --add-missing --copy
autoconf
