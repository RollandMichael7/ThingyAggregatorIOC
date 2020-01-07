#!/bin/sh

echo Cloning gattlib...
git clone https://github.com/labapart/gattlib
cd gattlib
git checkout tags/dev-42-g3dd0ab4
echo Done.

echo Building gattlib...
mkdir build
cd build
if cmake .. ; then
	make
	echo
	echo Done.
	echo Packaging gattlib...
	cpack ..
	echo
	echo Done.
else
	echo
	echo Done.
fi
