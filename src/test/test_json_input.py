#!/usr/bin/env python
# -*- coding: utf-8 -*-

from __future__ import print_function

import codecs
import json
import unittest
from glob import glob

from jsonschema import validate


class VariableMetadataValidation(unittest.TestCase):
    def setUp(self):
        with codecs.open('./variablemetadata_schema.json', encoding='utf8') as fd:
            self.schema = json.load(fd)

    def test_schema(self):
        def check_valid_file(fil):
            with codecs.open(fil, encoding='utf8') as fd:
                payload = json.load(fd)
                validate(payload, self.schema)
        for fil in glob("./src/test/csv_to_dta/**/*.json"):
            try:
                check_valid_file(fil)
            except:
                print("failed for file %s" % (fil))
                raise

if __name__=="__main__":
    unittest.main()
