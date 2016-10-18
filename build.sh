#!/bin/sh

clear && ./make.sh $@ && printf "\033[32;1mBuild OK\033[0m\n" || printf "\033[31;1mBuild Failed\033[0m\n";
while find . -type f -not -path "./.git/*" -not -name "*.dta" \
| inotifywait -q -q --fromfile - -e close_write; do \
clear && ./make.sh $@ \
&& printf "\033[32;1mBuild OK\033[0m\n" \
|| printf "\033[31;1mBuild Failed\033[0m\n"; done
