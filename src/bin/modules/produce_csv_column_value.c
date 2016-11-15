#include <stdio.h>
#include <stdlib.h>

#include "../../readstat.h"
#include "../format.h"
#include "../module_util.h"
#include "../module.h"
#include "produce_csv_column_header.h"
#include "produce_csv_column_value.h"
#include "json_metadata.h"
#include "../../stata/readstat_dta_days.h"
#include "../../spss/readstat_sav_date.h"

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

readstat_value_t value_double_dta(void *s, size_t len, struct csv_metadata *c) {
    char *dest;
    readstat_variable_t *var = &c->variables[c->columns];
    double val = strtod(s, &dest);
    if (dest == s) {
        fprintf(stderr, "not a number: %s\n", (char*)s);
        exit(EXIT_FAILURE);
    }
    int missing_ranges_count = readstat_variable_get_missing_ranges_count(var);
    for (int i=0; i<missing_ranges_count; i++) {
        readstat_value_t lo_val = readstat_variable_get_missing_range_lo(var, i);
        readstat_value_t hi_val = readstat_variable_get_missing_range_hi(var, i);
        if (readstat_value_type(lo_val) == READSTAT_TYPE_DOUBLE) {
            double lo = readstat_double_value(lo_val);
            double hi = readstat_double_value(hi_val);
            if (val >= lo && val <= hi) {
                readstat_value_t value = {
                    .type = READSTAT_TYPE_DOUBLE,
                    .is_tagged_missing = 1,
                    .tag = 'a' + i,
                    .v = { .double_value = val }
                    };
                fprintf(stderr, "using missing value %lf => tag %c\n", val, value.tag);
                return value;
            }
        } else {
            fprintf(stderr, "%s:%d not implemented // should not happen (?)\n", __FILE__, __LINE__);
            exit(EXIT_FAILURE);
        }
    }

    readstat_value_t value = {
        .type = READSTAT_TYPE_DOUBLE,
        .is_tagged_missing = 0,
        .v = { .double_value = val }
    };
    return value;
}

readstat_value_t value_double_regular(void *s, size_t len, struct csv_metadata *c) {
    char *dest;
    // readstat_variable_t *var = &c->variables[c->columns];
    double val = strtod(s, &dest);
    if (dest == s) {
        fprintf(stderr, "not a number: %s\n", (char*)s);
        exit(EXIT_FAILURE);
    }
    // int missing_ranges_count = readstat_variable_get_missing_ranges_count(var);
    // for (int i=0; i<missing_ranges_count; i++) {
    //     readstat_value_t lo_val = readstat_variable_get_missing_range_lo(var, i);
    //     readstat_value_t hi_val = readstat_variable_get_missing_range_hi(var, i);
    //     if (readstat_value_type(lo_val) == READSTAT_TYPE_DOUBLE) {
    //         double lo = readstat_double_value(lo_val);
    //         double hi = readstat_double_value(hi_val);
    //         if (hi != lo) {
    //             fprintf(stderr, "%s:%d not implemented\n", __FILE__, __LINE__);
    //             exit(EXIT_FAILURE);
    //         }
    //         if (lo == val) {
    //             return lo_val;
    //         }
    //     } else {
    //         fprintf(stderr, "%s:%d not implemented // should not happen (?)\n", __FILE__, __LINE__);
    //         exit(EXIT_FAILURE);
    //     }
    // }
    readstat_value_t value = {
        .type = READSTAT_TYPE_DOUBLE,
        .v = { .double_value = val }
    };
    return value;
}

readstat_value_t value_int32_date_dta(void *s, size_t len, struct csv_metadata *c) {
    readstat_variable_t *var = &c->variables[c->columns];
    char* dest;
    int val = readstat_dta_num_days(s, &dest);
    if (dest == s) {
        fprintf(stderr, "%s:%d not a date: %s\n", __FILE__, __LINE__, (char*)s);
        exit(EXIT_FAILURE);
    }

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

readstat_value_t value_double_date_sav(void *s, size_t len, struct csv_metadata *c) {
    char *dest;
    double val = readstat_sav_date_parse(s, &dest);
    if (dest == s) {
        fprintf(stderr, "%s:%d not a valid date: %s\n", __FILE__, __LINE__, (char*)s);
        exit(EXIT_FAILURE);
    }
    readstat_value_t value = {
        .type = READSTAT_TYPE_DOUBLE,
        .v = { .double_value = val }
    };
    return value;
}

void produce_csv_column_value(void *s, size_t len, void *data) {
    struct csv_metadata *c = (struct csv_metadata *)data;
    readstat_variable_t *var = &c->variables[c->columns];
    int is_date = c->is_date[c->columns];
    int obs_index = c->rows - 1; // TODO: ???
    readstat_value_t value;
    if (len == 0) {
        value = value_sysmiss(s, len, c);
    
    // SAV support
    } else if (c->output_format == RS_FORMAT_SAV && is_date) {
        value = value_double_date_sav(s, len, c);
    } else if (c->output_format == RS_FORMAT_SAV && var->type == READSTAT_TYPE_DOUBLE) {
        value = value_double_regular(s, len, c);
    } else if (c->output_format == RS_FORMAT_SAV && var->type == READSTAT_TYPE_STRING) {
        value = value_string(s, len, c);

    // DTA support
    } else if (c->output_format == RS_FORMAT_DTA && is_date) {
        value = value_int32_date_dta(s, len, c);
    } else if (c->output_format == RS_FORMAT_DTA && var->type == READSTAT_TYPE_DOUBLE) {
        value = value_double_dta(s, len, c);
    } else if (c->output_format == RS_FORMAT_DTA && var->type == READSTAT_TYPE_STRING) {
        value = value_string(s, len, c);
    
    // CSV support
    } else if (c->output_format == RS_FORMAT_CSV && var->type == READSTAT_TYPE_DOUBLE) {
        value = value_double_regular(s, len, c);
    } else if (c->output_format == RS_FORMAT_CSV && var->type == READSTAT_TYPE_STRING) {
        value = value_string(s, len, c);
    
    // abort
    } else if (var->type == READSTAT_TYPE_INT32) {
        fprintf(stderr, "%s:%d unsupported column type: INT32\n", __FILE__, __LINE__);
        exit(EXIT_FAILURE);
    } else {
        fprintf(stderr, "%s:%d unsupported column type: %x\n", __FILE__, __LINE__, var->type);
        exit(EXIT_FAILURE);
    }
    c->parser->value_handler(obs_index, var, value, c->user_ctx);
}