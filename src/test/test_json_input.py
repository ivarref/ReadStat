#!/usr/bin/env python
# -*- coding: utf-8 -*-

from __future__ import print_function

import codecs
import json
import sys
import unittest
from glob import glob

from jsonschema import validate


def get_schema():
    with codecs.open('./variablemetadata_schema.json', encoding='utf8') as fd:
        return json.load(fd)

def check_valid_file(fil):
    with codecs.open(fil, encoding='utf8') as fd:
        payload = json.load(fd)
        validate(payload, get_schema())

class VariableMetadataValidation(unittest.TestCase):
    def test_schema(self):
        for fil in glob("./src/test/csv_to_dta/**/*.json"):
            try:
                check_valid_file(fil)
            except:
                print("failed for file %s" % (fil))
                raise

if __name__=="__main__":
    if len(sys.argv) == 2:
        check_valid_file(sys.argv[-1])
    else:
        unittest.main()
