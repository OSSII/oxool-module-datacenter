#!/bin/sh

if [ -f Makefile ] ; then
    make distclean
fi

find -name "Makefile.in" -exec rm -f {} \;
rm -f stamp-h1
rm -f  config.*
rm -f *.log
rm -f .in
rm -f aclocal.m4
rm -f install-sh
rm -f libtool
rm -f ltmain.sh
rm -f missing
rm -f configure
rm -f depcomp
rm -fr autom4te.cache
rm -f *.tar.*
