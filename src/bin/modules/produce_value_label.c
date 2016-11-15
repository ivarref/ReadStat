#include <stdio.h>
#include <stdlib.h>

#include "../../readstat.h"
#include "../format.h"
#include "json_metadata.h"

#include "produce_value_label.h"
#include "produce_csv_column_value.h"
#include "produce_csv_column_header.h"
#include "../../stata/readstat_dta_days.h"
#include "../../spss/readstat_sav_date.h"

void produce_value_label_double_date_sav(char* column, struct csv_metadata *c, char *code, char *label) {
    char *endptr;
    double v = readstat_sav_date_parse(code, &endptr);
    if (endptr == code) {
        fprintf(stderr, "%s:%d not a valid date: %s\n", __FILE__, __LINE__, code);
        exit(EXIT_FAILURE);
    }
    readstat_value_t value = {
        .v = { .double_value = v },
        .type = READSTAT_TYPE_DOUBLE,
    };
    c->parser->value_label_handler(column, value, label, c->user_ctx);
}

void produce_value_label_double_sav(char* column, struct csv_metadata *c, char *code, char *label) {
    char *endptr;
    double v = strtod(code, &endptr);
    if (endptr == code) {
        fprintf(stderr, "%s:%d not a number: %s\n", __FILE__, __LINE__, code);
        exit(EXIT_FAILURE);
    }
    readstat_value_t value = {
        .v = { .double_value = v },
        .type = READSTAT_TYPE_DOUBLE,
    };
    c->parser->value_label_handler(column, value, label, c->user_ctx);
}

void produce_value_label_double_dta(char* column, struct csv_metadata *c, char *code, char *label) {
    readstat_variable_t* variable = &c->variables[c->columns];
    char *endptr;
    double v = strtod(code, &endptr);
    if (endptr == code) {
        fprintf(stderr, "%s:%d not a number: %s\n", __FILE__, __LINE__, code);
        exit(EXIT_FAILURE);
    }
    readstat_value_t value = {
        .v = { .double_value = v },
        .type = READSTAT_TYPE_DOUBLE,
    };
    int missing_ranges_count = readstat_variable_get_missing_ranges_count(variable);
    for (int i=0; i<missing_ranges_count; i++) {
        readstat_value_t lo_val = readstat_variable_get_missing_range_lo(variable, i);
        readstat_value_t hi_val = readstat_variable_get_missing_range_hi(variable, i);
        if (readstat_value_type(lo_val) == READSTAT_TYPE_DOUBLE) {
            double lo = readstat_double_value(lo_val);
            double hi = readstat_double_value(hi_val);
            if (v >= lo && v <= hi) {
                value.is_tagged_missing = 1;
                value.tag = 'a' + i;
            }
        }
    }
    fprintf(stdout, "adding value label %lf with label %s. tag => %c\n", v, label, value.tag);
    c->parser->value_label_handler(column, value, label, c->user_ctx);
}

void produce_value_label_int32_date_dta(char* column, struct csv_metadata *c, char *code, char *label) {
    char *dest;
    int days = readstat_dta_num_days(code, &dest);
    if (dest == code) {
        fprintf(stderr, "%s:%d not a valid date: %s\n", __FILE__, __LINE__, code);
        exit(EXIT_FAILURE);
    }
    readstat_value_t value = {
        .v = { .i32_value = days },
        .type = READSTAT_TYPE_INT32,
    };
    int missing_idx = missing_string_idx(c->json_md, column, code);
    if (missing_idx) {
        value.is_tagged_missing = 1;
        value.tag = 'a' + (missing_idx-1);
    }
    c->parser->value_label_handler(column, value, label, c->user_ctx);
}

void produce_value_label_string(char* column, struct csv_metadata *c, char *code, char *label) {
    readstat_value_t value = {
        .v = { .string_value = code },
        .type = READSTAT_TYPE_STRING,
    };
    c->parser->value_label_handler(column, value, label, c->user_ctx);
}

char* produce_value_label(char* column, size_t len, struct csv_metadata *c, readstat_type_t coltype) {
    jsmntok_t* categories = find_variable_property(c->json_md->js, c->json_md->tok, column, "categories");
    if (categories==NULL) {
        return NULL;
    }
    int is_date = c->is_date[c->columns];
    int j = 1;
    char code_buf[1024];
    char label_buf[1024];
    for (int i=0; i<categories->size; i++) {
        jsmntok_t* tok = categories+j;
        char* code = get_object_property(c->json_md->js, tok, "code", code_buf, sizeof(code_buf));
        char* label = get_object_property(c->json_md->js, tok, "label", label_buf, sizeof(label_buf));
        if (!code || !label) {
            fprintf(stderr, "%s:%d bogus JSON metadata input. Missing code/label for column %s\n", __FILE__, __LINE__, column);
            exit(EXIT_FAILURE);
        }

        // format SAV
        if (c->output_format == RS_FORMAT_SAV && coltype == READSTAT_TYPE_DOUBLE && is_date) {
            produce_value_label_double_date_sav(column, c, code, label);
        } else if (c->output_format == RS_FORMAT_SAV && coltype == READSTAT_TYPE_DOUBLE) {
            produce_value_label_double_sav(column, c, code, label);
        } else if (c->output_format == RS_FORMAT_SAV && coltype == READSTAT_TYPE_STRING) {
            produce_value_label_string(column, c, code, label);

        // format DTA
        } else if (c->output_format == RS_FORMAT_DTA && coltype == READSTAT_TYPE_INT32 && is_date) {
            produce_value_label_int32_date_dta(column, c, code, label);
        } else if (c->output_format == RS_FORMAT_DTA && coltype == READSTAT_TYPE_DOUBLE) {
            produce_value_label_double_dta(column, c, code, label);

        // format CSV
        } else if (c->output_format == RS_FORMAT_CSV) {
            // do nothing for CSV
        } else {
            fprintf(stderr, "%s:%d unsupported column type for value label\n", __FILE__, __LINE__);
            exit(EXIT_FAILURE);
        }
        j += slurp_object(tok);
    }
    return column;
}