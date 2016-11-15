#ifndef __PRODUCE_VALUE_LABEL_H
#define __PRODUCE_VALUE_LABEL_H

#include "produce_csv_column_header.h"

char* produce_value_label(char* column, size_t len, struct csv_metadata *c, readstat_type_t coltype);

#endif