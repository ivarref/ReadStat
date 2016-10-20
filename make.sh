#!/bin/sh

set -e
make $@

rm -f output.dta output.csv
#valgrind --leak-check=full --error-exitcode=1 ./readstat ./sample.csv output.dta
#./test_json_input.py
#echo "*************************************************"
#make
./readstat ./sample.csv output.dta
#./readstat output.dta output.csv
#echo ">>> generated:::"
#cat output.csv
#echo "*** STATA ***"
/usr/local/stata/stata -q "$(pwd)/output.dta" < cmds.txt | sed '/^.\?\s*$/d'
printf "\n"

#make
#make check && cat test-suite.log || cat test-suite.log
