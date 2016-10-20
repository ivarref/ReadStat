#include <stdio.h>
#include <stdlib.h>

#include "../../readstat.h"
#include "json_metadata.h"

#include "produce_csv_column_value.h"
#include "produce_csv_column_header.h"

int produce_missingness(char* column, size_t len, struct csv_metadata *c, readstat_type_t coltype, readstat_variable_t* var) {
    jsmntok_t* missing = find_variable_property(c->json_md->js, c->json_md->tok, column, "missing");
    if (missing==NULL) {
        return 1;
    }

    char type_buf[1024];
    char* type = get_object_property(c->json_md->js, missing, "type", type_buf, sizeof(type_buf));
    if (!type) {
        fprintf(stderr, "expected to find type property inside missing definition\n");
        exit(EXIT_FAILURE);
    }

    if (0 == strcmp(type, "DISCRETE")) {
        jsmntok_t* values = find_object_property(c->json_md->js, missing, "values");
        if (!values) {
            fprintf(stderr, "expected to find values property inside missing definition\n");
            exit(EXIT_FAILURE);
        }
        int j = 1;
        char double_buf[1024];
        for (int i=0; i<values->size; i++) {
            jsmntok_t* value = values+j;
            snprintf(double_buf, sizeof(double_buf)-1, "%.*s", value->end - value->start, c->json_md->js+value->start);
            char *dest;
            double v = strtod(double_buf, &dest);
            if (dest == double_buf) {
                fprintf(stderr, "not a number: %s\n", double_buf);
                exit(EXIT_FAILURE);
            }
            fprintf(stdout, "adding missing value metadata %g for column %s\n", v, column);
            readstat_variable_add_missing_double_value(var, v);
            j += slurp_object(value);
        }
    } else {
        fprintf(stderr, "unsupported missing definition %s\n", type);
        exit(EXIT_FAILURE);
    }
    return 0;
}

char* produce_value_label(char* column, size_t len, struct csv_metadata *c, readstat_type_t coltype) {
    jsmntok_t* categories = find_variable_property(c->json_md->js, c->json_md->tok, column, "categories");
    if (categories==NULL) {
        return NULL;
    }
    int j = 1;
    char code_buf[1024];
    char label_buf[1024];
    for (int i=0; i<categories->size; i++) {
        jsmntok_t* tok = categories+j;
        char* code = get_object_property(c->json_md->js, tok, "code", code_buf, sizeof(code_buf));
        char* label = get_object_property(c->json_md->js, tok, "label", label_buf, sizeof(label_buf));
        if (!code || !label) {
            fprintf(stderr, "bogus JSON metadata input\n");
            exit(EXIT_FAILURE);
        }
        if (coltype == READSTAT_TYPE_DOUBLE) {
            char * endptr;
            double v = strtod(code, &endptr);
            if (endptr == code) {
                fprintf(stderr, "not a number: %s\n", code);
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
            c->parser->value_label_handler(column, value, label, c->user_ctx);
        } else if (coltype == READSTAT_TYPE_INT32) {
            int days = dta_numdays(code);
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
        } else {
            fprintf(stderr, "%s:%d unsupported column type for value label\n", __FILE__, __LINE__);
            exit(EXIT_FAILURE);
        }
        j += slurp_object(tok);
    }
    return column;
}

void produce_column_header(void *s, size_t len, void *data) {
    struct csv_metadata *c = (struct csv_metadata *)data;
    char* column = (char*)s;
    readstat_variable_t* var = &c->variables[c->columns];
    memset(var, 0, sizeof(readstat_variable_t));
    
    readstat_type_t coltype = column_type(c->json_md, column);
    var->type = coltype;
    if (coltype == READSTAT_TYPE_STRING) {
        var->alignment = READSTAT_ALIGNMENT_LEFT;
    } else if (coltype == READSTAT_TYPE_DOUBLE) {
        var->alignment = READSTAT_ALIGNMENT_RIGHT;
    } else if (coltype == READSTAT_TYPE_INT32) {
        var->alignment = READSTAT_ALIGNMENT_RIGHT;
        snprintf(var->format, sizeof(var->format)-1, "%s", "%td");
    } else {
        fprintf(stderr, "%s:%d unsupported column type: %x\n", __FILE__, __LINE__, coltype);
        exit(EXIT_FAILURE);
    }
    
    var->index = c->columns;
    copy_variable_property(c->json_md, column, "label", var->label, sizeof(var->label));
    snprintf(var->name, sizeof(var->name)-1, "%.*s", (int)len, column);

    if (c->parser->value_label_handler) {
        produce_value_label(column, len, c, coltype);
    }

    if (c->parser->variable_handler) {
        //produce_missingness(column, len, c, coltype, var);
        c->parser->variable_handler(c->columns, var, column, c->user_ctx);
    }
}
