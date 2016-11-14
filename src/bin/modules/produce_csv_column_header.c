#include <stdio.h>
#include <stdlib.h>

#include "../../readstat.h"
#include "../format.h"
#include "json_metadata.h"

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
    int missing_idx = missing_double_idx(c->json_md, column, v);
    if (missing_idx) {
        value.is_tagged_missing = 1;
        value.tag = 'a' + (missing_idx-1);
    }
    fprintf(stdout, "adding value label %lf with label %s. missing_idx => %d\n", v, label, missing_idx);
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
        if (c->output_format == RS_FORMAT_SAV && coltype == READSTAT_TYPE_DOUBLE && is_date) {
            produce_value_label_double_date_sav(column, c, code, label);
        } else if (c->output_format == RS_FORMAT_DTA && coltype == READSTAT_TYPE_INT32 && is_date) {
            produce_value_label_int32_date_dta(column, c, code, label);
        } else if (c->output_format == RS_FORMAT_SAV && coltype == READSTAT_TYPE_DOUBLE) {
            produce_value_label_double_sav(column, c, code, label);
        } else if (c->output_format == RS_FORMAT_DTA && coltype == READSTAT_TYPE_DOUBLE) {
            produce_value_label_double_dta(column, c, code, label);
        } else if (coltype == READSTAT_TYPE_STRING) {
            produce_value_label_string(column, c, code, label);
        } else {
            fprintf(stderr, "%s:%d unsupported column type for value label\n", __FILE__, __LINE__);
            exit(EXIT_FAILURE);
        }
        j += slurp_object(tok);
    }
    return column;
}

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
        } else if (var->type == READSTAT_TYPE_DOUBLE) {
            readstat_variable_add_missing_double_value(var, get_double_missing(js, missing_value_token));
        } else {
            fprintf(stderr, "%s:%d Unsupported column type %d\n", __FILE__, __LINE__, var->type);
            exit(EXIT_FAILURE);
        }
        j += slurp_object(missing_value_token);
    }
}

void produce_missingness_range(struct csv_metadata *c, jsmntok_t* missing, const char* column) {
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

    if (low && high && c->output_format == RS_FORMAT_SAV) {
        double lo = is_date ? get_double_date_missing_sav(js, low) : get_double_missing(js, low);
        double hi = is_date ? get_double_date_missing_sav(js, high) :  get_double_missing(js, high);
        readstat_variable_add_missing_double_range(var, lo, hi);
    } else if (low && high) {
        fprintf(stderr, "%s:%d unsupported output format for missing range\n", __FILE__, __LINE__);
        exit(EXIT_FAILURE);
    }

    if (discrete && c->output_format == RS_FORMAT_SAV) {
        double v = is_date ? get_double_date_missing_sav(js, discrete) : get_double_missing(js, discrete);
        readstat_variable_add_missing_double_value(var, v);
    } else if (discrete) {
        fprintf(stderr, "%s:%d missing.discrete-value specified, but unsupported output format\n", __FILE__, __LINE__);
        exit(EXIT_FAILURE);
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
    } else if (match_token(js, missing_type, "RANGE")) {
        produce_missingness_range(c, missing, column);
    } else {
        fprintf(stderr, "%s:%d unknown missing type %.*s\n", __FILE__, __LINE__, missing_type->end - missing_type->start, js+missing_type->start);
        exit(EXIT_FAILURE);
    }
}

void produce_column_header(void *s, size_t len, void *data) {
    struct csv_metadata *c = (struct csv_metadata *)data;
    char* column = (char*)s;
    readstat_variable_t* var = &c->variables[c->columns];
    memset(var, 0, sizeof(readstat_variable_t));
    
    readstat_type_t coltype = column_type(c->json_md, column, c->output_format);
    int is_date_column = is_date(c->json_md, column);
    c->is_date[c->columns] = is_date_column;
    var->type = coltype;
    if (coltype == READSTAT_TYPE_STRING) {
        var->alignment = READSTAT_ALIGNMENT_LEFT;
        fprintf(stdout, "column %s is of type STRING\n", column);
    } else if (coltype == READSTAT_TYPE_DOUBLE) {
        var->alignment = READSTAT_ALIGNMENT_RIGHT;
        fprintf(stdout, "column %s is of type DOUBLE\n", column);
    } else if (coltype == READSTAT_TYPE_INT32) {
        var->alignment = READSTAT_ALIGNMENT_RIGHT;
        fprintf(stdout, "column %s is of type INT32\n", column);
    } else {
        fprintf(stderr, "%s:%d unsupported column type: %x\n", __FILE__, __LINE__, coltype);
        exit(EXIT_FAILURE);
    }

    if (is_date_column && c->output_format == RS_FORMAT_SAV) {
        snprintf(var->format, sizeof(var->format)-1, "%s", "EDATE40");
    } else if (is_date_column && c->output_format == RS_FORMAT_DTA) {
        snprintf(var->format, sizeof(var->format)-1, "%s", "%td");
    } else if (is_date_column) {
        fprintf(stderr, "%s:%d unsupported date column for format %d\n", __FILE__, __LINE__, c->output_format);
    }

    if (c->pass == 2 && coltype == READSTAT_TYPE_STRING) {
        var->storage_width = c->column_width[c->columns];
    }
    
    var->index = c->columns;
    copy_variable_property(c->json_md, column, "label", var->label, sizeof(var->label));
    snprintf(var->name, sizeof(var->name)-1, "%.*s", (int)len, column);

    produce_missingness(c, column);
    if (c->parser->value_label_handler) {
        produce_value_label(column, len, c, coltype);
    }

    if (c->parser->variable_handler && c->pass == 2) {
        c->parser->variable_handler(c->columns, var, column, c->user_ctx);
    }
}
