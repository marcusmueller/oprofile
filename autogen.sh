# run to generate needed files not in CVS

# this simple currently

aclocal 
autoheader
automake --add-missing --copy
autoconf
