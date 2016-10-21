#!/usr/bin/env python
# -*- coding: utf-8 -*-

import unittest
from jsonschema import validate
import json
import codecs

class VariableMetadataValidation(unittest.TestCase):
    def setUp(self):
        with codecs.open('./variablemetadata_schema.json', encoding='utf8') as fd:
            self.schema = json.load(fd)

    def test_schema(self):
        def check_valid_file(fil):
            with codecs.open(fil, encoding='utf8') as fd:
                payload = json.load(fd)
                validate(payload, self.schema)
        check_valid_file('./sample.json')
        # check_valid_file('./src/test/csv_to_dta/input.json')

if __name__=="__main__":
    unittest.main()
    