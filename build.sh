make

echo
echo Building thingy_scan...
gcc ThingyApp/src/thingy_scan.c -lgattlib -o thingy_scan
echo Done.

echo
echo Building thingy_name_assign...
gcc ThingyApp/src/thingy_name_assign.c -lgattlib -o thingy_name_assign
echo Done.
