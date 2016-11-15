#include <stdio.h>
#include <stdlib.h>

#include "../../readstat.h"
#include "../format.h"
#include "json_metadata.h"

#include "../../stata/readstat_dta_days.h"
#include "../../spss/readstat_sav_date.h"
#include "produce_csv_column_value.h"
#include "produce_csv_column_header.h"

double get_double_date_missing_sav(const char *js, jsmntok_t* missing_value_token) {
    // SAV missing date
    char buf[255];
    char *dest;
    int len = missing_value_token->end - missing_value_token->start;
    snprintf(buf, sizeof(buf)-1, "%.*s", len, js + missing_value_token->start);
    double val = readstat_sav_date_parse(buf, &dest);
    if (buf == dest) {
        fprintf(stderr, "%s:%d failed to parse double: %s\n", __FILE__, __LINE__, buf);
        exit(EXIT_FAILURE);
    } else {
        fprintf(stdout, "added double date missing %s\n", buf);
    }
    return val;
}

readstat_value_t get_int32_date_missing_dta(const char *js, jsmntok_t* missing_value_token, int idx) {
    // DTA date missing
    char buf[255];
    int len = missing_value_token->end - missing_value_token->start;
    snprintf(buf, sizeof(buf)-1, "%.*s", len, js + missing_value_token->start);
    char* dest;
    int days = readstat_dta_num_days(buf, &dest);
    if (dest == buf) {
        fprintf(stderr, "%s:%d error parsing date %s\n", __FILE__, __LINE__, buf);
        exit(EXIT_FAILURE);
    }
    // return days;
    readstat_value_t value = {
        .type = READSTAT_TYPE_INT32,
        .is_system_missing = 0,
        .is_tagged_missing = 1,
        .tag = 'a' + idx,
        .v = {
            .i32_value = days
        }
    };
    return value;
}

double get_double_missing(const char *js, jsmntok_t* missing_value_token) {
    char buf[255];
    char *dest;
    int len = missing_value_token->end - missing_value_token->start;
    snprintf(buf, sizeof(buf)-1, "%.*s", len, js + missing_value_token->start);
    double val = strtod(buf, &dest);
    if (buf == dest) {
        fprintf(stderr, "%s:%d failed to parse double: %s\n", __FILE__, __LINE__, buf);
        exit(EXIT_FAILURE);
    }
    return val;
}

void produce_missingness_discrete(struct csv_metadata *c, jsmntok_t* missing, const char* column) {
    readstat_variable_t* var = &c->variables[c->columns];
    int is_date = c->is_date[c->columns];
    const char *js = c->json_md->js;

    jsmntok_t* values = find_object_property(js, missing, "values");
    if (!values) {
        fprintf(stderr, "%s:%d Expected to find missing 'values' property\n", __FILE__, __LINE__);
        exit(EXIT_FAILURE);
    }

    int j = 1;
    for (int i=0; i<values->size; i++) {
        jsmntok_t* missing_value_token = values + j;
        if (c->output_format == RS_FORMAT_SAV && var->type == READSTAT_TYPE_DOUBLE && is_date) {
            readstat_variable_add_missing_double_value(var, get_double_date_missing_sav(js, missing_value_token));
        } else if (c->output_format == RS_FORMAT_DTA && var->type == READSTAT_TYPE_INT32 && is_date) { 
            // TODO I think this is correct, but would be good to verify
            // So it seems that STATA does not use the hi-low ranges, and thus we only need to write once
            readstat_value_t v = get_int32_date_missing_dta(js, missing_value_token, i);
            var->missingness.missing_ranges[var->missingness.missing_ranges_count++] = v;
            //double vv = v.v.i32_value;
            //readstat_variable_add_missing_double_value(var_internal, vv);
        } else if (var->type == READSTAT_TYPE_DOUBLE) {
            readstat_variable_add_missing_double_value(var, get_double_missing(js, missing_value_token));
            // readstat_variable_add_missing_double_value(var_internal, get_double_missing(js, missing_value_token));
        } else {
            fprintf(stderr, "%s:%d Unsupported column type %d\n", __FILE__, __LINE__, var->type);
            exit(EXIT_FAILURE);
        }
        j += slurp_object(missing_value_token);
    }
}

char dta_add_missing(readstat_variable_t* var, double v) {
    int idx = var->missingness.missing_ranges_count;
    char tagg = 'a' + idx;
    readstat_value_t value = {
        .type = READSTAT_TYPE_DOUBLE,
        .is_system_missing = 0,
        .is_tagged_missing = 1,
        .tag = tagg,
        .v = {
            .double_value = v
        }
    };
    var->missingness.missing_ranges[(idx*2)] = value;
    var->missingness.missing_ranges[(idx*2)+1] = value;
    var->missingness.missing_ranges_count++;
    return tagg;
}

void produce_missingness_range_dta(struct csv_metadata *c, jsmntok_t* missing, const char* column) {
    readstat_variable_t* var = &c->variables[c->columns];
    const char *js = c->json_md->js;

    jsmntok_t* low = find_object_property(js, missing, "low");
    jsmntok_t* high = find_object_property(js, missing, "high");
    jsmntok_t* discrete = find_object_property(js, missing, "discrete-value");

    jsmntok_t* categories = find_variable_property(js, c->json_md->tok, column, "categories");
    if (!categories && (low || high || discrete)) {
        fprintf(stderr, "%s:%d expected to find categories for column %s\n", __FILE__, __LINE__, column);
        exit(EXIT_FAILURE);
    } else if (!categories) {
        return;
    }
    if (low && !high) {
        fprintf(stderr, "%s:%d missing.low specified for column %s, but missing.high not specified\n", __FILE__, __LINE__, column);
        exit(EXIT_FAILURE);
    }
    if (high && !low) {
        fprintf(stderr, "%s:%d missing.high specified for column %s, but missing.low not specified\n", __FILE__, __LINE__, column);
        exit(EXIT_FAILURE);
    }

    char label_buf[1024];
    int j = 1;
    for (int i=0; i<categories->size; i++) {
        jsmntok_t* tok = categories+j;
        jsmntok_t* code = find_object_property(js, tok, "code");
        char* label = get_object_property(c->json_md->js, tok, "label", label_buf, sizeof(label_buf));
        if (!code || !label) {
            fprintf(stderr, "%s:%d bogus JSON metadata input. Missing code/label for column %s\n", __FILE__, __LINE__, column);
            exit(EXIT_FAILURE);
        }

        double cod = get_double_missing(js, code);

        if (low && high) {
            double lo = get_double_missing(js, low);
            double hi = get_double_missing(js, high);
            if (cod >= lo && cod <= hi) {
                char tag = dta_add_missing(var, cod);
                printf("%s:%d produce_missingness: Adding missing for code %lf => %c\n", __FILE__, __LINE__, cod, tag);
            }
        }
        if (discrete) {
            double v = get_double_missing(js, discrete);
            if (cod == v) {
                char tag = dta_add_missing(var, cod);
                printf("%s:%d produce_missingness: Adding missing for code %lf => %c\n", __FILE__, __LINE__, cod, tag);
            }
        }

        j += slurp_object(tok);
    }
}



void produce_missingness_range_sav(struct csv_metadata *c, jsmntok_t* missing, const char* column) {
    readstat_variable_t* var = &c->variables[c->columns];
    int is_date = c->is_date[c->columns];
    const char *js = c->json_md->js;

    jsmntok_t* low = find_object_property(js, missing, "low");
    jsmntok_t* high = find_object_property(js, missing, "high");
    jsmntok_t* discrete = find_object_property(js, missing, "discrete-value");

    if (low && !high) {
        fprintf(stderr, "%s:%d missing.low specified for column %s, but missing.high not specified\n", __FILE__, __LINE__, column);
        exit(EXIT_FAILURE);
    }
    if (high && !low) {
        fprintf(stderr, "%s:%d missing.high specified for column %s, but missing.low not specified\n", __FILE__, __LINE__, column);
        exit(EXIT_FAILURE);
    }

    if (low && high) {
        double lo = is_date ? get_double_date_missing_sav(js, low) : get_double_missing(js, low);
        double hi = is_date ? get_double_date_missing_sav(js, high) :  get_double_missing(js, high);
        readstat_variable_add_missing_double_range(var, lo, hi);
    }

    if (discrete) {
        double v = is_date ? get_double_date_missing_sav(js, discrete) : get_double_missing(js, discrete);
        readstat_variable_add_missing_double_value(var, v);
    }
}

void produce_missingness(struct csv_metadata *c, const char* column) {
    const char *js = c->json_md->js;
    readstat_variable_t* var = &c->variables[c->columns];
    var->missingness.missing_ranges_count = 0;
    
    jsmntok_t* missing = find_variable_property(js, c->json_md->tok, column, "missing");
    if (!missing) {
        return;
    }

    jsmntok_t* missing_type = find_object_property(js, missing, "type");
    if (!missing_type) {
        fprintf(stderr, "%s:%d expected to find missing.type for column %s\n", __FILE__, __LINE__, column);
        exit(EXIT_FAILURE);
    }

    if (match_token(js, missing_type, "DISCRETE")) {
        produce_missingness_discrete(c, missing, column);
    } else if (match_token(js, missing_type, "RANGE") && c->output_format == RS_FORMAT_SAV) {
        produce_missingness_range_sav(c, missing, column);
    } else if (match_token(js, missing_type, "RANGE") && c->output_format == RS_FORMAT_DTA) {
        produce_missingness_range_dta(c, missing, column);
    } else if (c->output_format == RS_FORMAT_CSV) {
        // do nothing ..
    } else {
        fprintf(stderr, "%s:%d unknown missing type %.*s\n", __FILE__, __LINE__, missing_type->end - missing_type->start, js+missing_type->start);
        exit(EXIT_FAILURE);
    }
}