#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include "../../readstat.h"
#include "../module_util.h"
#include "../module.h"
#include "produce_csv_column_value.h"
#include "json_metadata.h"
#include "../../readstat.h"

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
    double vv = strtod(s, &dest);
    if (dest == s) {
        fprintf(stderr, "not a number: %s\n", s);
        exit(EXIT_FAILURE);
    }
    if (is_missing_double(c->json_md, var->name, vv)) {
        readstat_value_t value = { .type = READSTAT_TYPE_DOUBLE, .is_tagged_missing = 1, .tag = 'a', .v = { .double_value = 123 } };
        fprintf(stderr, "it's a missing value >%g<\n", vv);
        return value;
    } else {
        readstat_value_t value = {
            .is_system_missing = 0,
            .is_tagged_missing = 0,
            .v = { .double_value = vv },
            .type = READSTAT_TYPE_DOUBLE
        };
        return value;
    }
}

void produce_csv_column_value(void *s, size_t len, void *data) {
    struct csv_metadata *c = (struct csv_metadata *)data;
    readstat_variable_t *var = &c->variables[c->columns];
    int obs_index = c->rows - 1; // TODO: ???

    // typedef struct readstat_value_s {
    //     union {
    //         float       float_value;
    //         double      double_value;
    //         int8_t      i8_value;
    //         int16_t     i16_value;
    //         int32_t     i32_value;
    //         const char *string_value;
    //     } v;
    //     readstat_type_t         type;
    //     char                    tag;
    //     unsigned int            is_system_missing:1;
    //     unsigned int            is_tagged_missing:1;
    // } readstat_value_t;
    
    readstat_value_t value;
    if (len == 0) {
        value = value_sysmiss(s, len, c);
    } else if (var->type == READSTAT_TYPE_STRING) {
        value = value_string(s, len, c);
    } else if (var->type == READSTAT_TYPE_DOUBLE) {
        value = value_double(s, len, c);
    } else {
        fprintf(stderr, "unhandled column type: %x\n", var->type);
        exit(EXIT_FAILURE);
    }

    c->parser->value_handler(obs_index, var, value, c->user_ctx);
}