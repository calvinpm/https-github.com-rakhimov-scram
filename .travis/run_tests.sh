#!/usr/bin/env bash

set -ev

# Assuming that path contains the built binaries.
# And this script is called from the root directory.
which scram
which scram_tests

scram_tests
nosetests -w ./tests/

if [[ -z "${RELEASE}" && "$CXX" = "g++" ]]; then
  nosetests --with-coverage -w scripts test/
fi

./scripts/fault_tree_generator.py -b 200 -a 5
scram --validate fault_tree.xml
