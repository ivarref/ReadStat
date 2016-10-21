#!/bin/sh

set -e

make test_dta_dates
make check TESTS='test_dta_dates' || /bin/sh -c 'cat test_dta_dates.log; exit 1' && cat test_dta_dates.log

#./test_json_input.py
#valgrind --leak-check=full --error-exitcode=1 .libs/readstat ./sample.csv ./sample.json output.dta
#valgrind .libs/readstat ./src/test/csv_to_dta/input.csv ./src/test/csv_to_dta/input.json ./src/test/csv_to_dta/output.dta
#./readstat ./sample.csv ./sample.json output.dta
#./readstat output.dta output.csv
#echo ">>> generated:::"
#cat output.csv
#echo "*** STATA ***"
#/usr/local/stata/stata -q "$(pwd)/output.dta" < cmds.txt | sed '/^.\?\s*$/d'
#printf "\n"

#make
#make check && cat test-suite.log || cat test-suite.log
