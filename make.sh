#!/bin/sh

set -e
make $@

echo "*************************************************"
rm -f output.dta output.csv
#valgrind --leak-check=full --error-exitcode=1 ./readstat ./sample.csv output.dta
./readstat ./sample.csv output.dta
#./readstat output.dta output.csv
#echo ">>> generated:::"
#cat output.csv
echo "*** STATA ***"
/usr/local/stata/stata -q "$(pwd)/output.dta" < cmds.txt
printf "\n"