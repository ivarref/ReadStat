#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#define __USE_XOPEN
#include <time.h>
#include <unistd.h>
#include <errno.h>

#include "../../readstat.h"
#include "../module_util.h"
#include "../module.h"
#include "produce_csv_column_header.h"
#include "produce_csv_column_value.h"
#include "json_metadata.h"
#include "../../stata/readstat_dta_days.h"

readstat_value_t value_sysmiss(void *s, size_t len, struct csv_metadata *c) {
    readstat_variable_t *var = &c->variables[c->columns];
    readstat_value_t value = {
        .is_system_missing = 1,
        .is_tagged_missing = 0,
        .type = var->type
    };
    return value;
}

readstat_value_t value_string(void *s, size_t len, struct csv_metadata *c) {
    readstat_value_t value = {
            .is_system_missing = 0,
            .is_tagged_missing = 0,
            .v = { .string_value = s },
            .type = READSTAT_TYPE_STRING
        };
    return value;
}

readstat_value_t value_double(void *s, size_t len, struct csv_metadata *c) {
    char *dest;
    readstat_variable_t *var = &c->variables[c->columns];
    double val = strtod(s, &dest);
    if (dest == s) {
        fprintf(stderr, "not a number: %s\n", (char*)s);
        exit(EXIT_FAILURE);
    }
    for (int i=0; i<var->missingness.missing_ranges_count; i++) {
        if (val == var->missingness.missing_ranges[i].v.double_value) {
            return var->missingness.missing_ranges[i];
        }
    }
    readstat_value_t value = {
        .type = READSTAT_TYPE_DOUBLE,
        .is_tagged_missing = 0,
        .v = { .double_value = val }
    };
    return value;
}

readstat_value_t value_int32(void *s, size_t len, struct csv_metadata *c) {
    readstat_variable_t *var = &c->variables[c->columns];
    int val = readstat_dta_num_days(s);

    for (int i=0; i<var->missingness.missing_ranges_count; i++) {
        if (val == var->missingness.missing_ranges[i].v.i32_value) {
            return var->missingness.missing_ranges[i];
        }
    }
    readstat_value_t value = {
        .type = READSTAT_TYPE_INT32,
        .is_tagged_missing = 0,
        .v = { .i32_value = val }
    };
    return value;
}

void produce_csv_column_value(void *s, size_t len, void *data) {
    struct csv_metadata *c = (struct csv_metadata *)data;
    readstat_variable_t *var = &c->variables[c->columns];
    int obs_index = c->rows - 1; // TODO: ???
    readstat_value_t value;
    if (len == 0) {
        value = value_sysmiss(s, len, c);
    } else if (var->type == READSTAT_TYPE_STRING) {
        value = value_string(s, len, c);
    } else if (var->type == READSTAT_TYPE_DOUBLE) {
        value = value_double(s, len, c);
    } else if (var->type == READSTAT_TYPE_INT32) {
        value = value_int32(s, len, c);
    } else {
        fprintf(stderr, "%s:%d unsupported column type: %x\n", __FILE__, __LINE__, var->type);
        exit(EXIT_FAILURE);
    }
    c->parser->value_handler(obs_index, var, value, c->user_ctx);
}