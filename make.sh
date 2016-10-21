#!/bin/sh

set -e

make
make test_readstat
make check TESTS='test_readstat'
make check-valgrind TESTS='test_readstat'
