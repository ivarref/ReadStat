#include <stdio.h>
#include <stdlib.h>

#include "../../readstat.h"
#include "json_metadata.h"

#include "produce_csv_column_value.h"
#include "produce_csv_column_header.h"

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
    } else {
        fprintf(stderr, "unsupported column type: %x\n", coltype);
        exit(EXIT_FAILURE);
    }
    
    var->index = c->columns;
    snprintf(var->name, sizeof(var->name)-1, "%.*s", (int)len, column);
    copy_variable_property(c->json_md, column, "label", var->label, sizeof(var->label));
    // static int handle_value_label(const char *val_labels, readstat_value_t value, const char *label, void *ctx);
    //fprintf(stdout, "size of var->label: %d\n", sizeof(var->label));
    // typedef struct readstat_variable_s {
    //     readstat_type_t         type;
    //     int                     index;
    //     char                    name[256];
    //     char                    format[256];
    //     char                    label[1024];
    //     readstat_label_set_t   *label_set;
    //     off_t                   offset;
    //     size_t                  storage_width;
    //     size_t                  user_width;
    //     readstat_missingness_t  missingness;
    //     readstat_measure_t      measure;
    //     readstat_alignment_t    alignment;
    //     int                     display_width;
    // } readstat_variable_t;

    // typedef struct readstat_label_set_s {
    //     readstat_type_t            type;
    //     char                        name[256];

    //     readstat_value_label_t     *value_labels;
    //     long                        value_labels_count;
    //     long                        value_labels_capacity;

    //     void                       *variables;
    //     long                        variables_count;
    //     long                        variables_capacity;
    // } readstat_label_set_t;

    // typedef struct readstat_value_label_s {
    //     double      double_key;
    //     int32_t     int32_key;
    //     char        tag;
    //     char       *string_key;
    //     size_t      string_key_len;
    //     char       *label;
    //     size_t      label_len;
    // } readstat_value_label_t;

    // (const char *val_labels, readstat_value_t value, const char *label, void *ctx)
    /*
    .label_sets = {
            {
                .name = "labels0",
                .type = READSTAT_TYPE_DOUBLE,
                .value_labels_count = 2,
                .value_labels = {
                    {
                        .value = { .type = READSTAT_TYPE_DOUBLE, .v = { .double_value = 1 } },
                        .label = "One"
                    },
                    {
                        .value = { .type = READSTAT_TYPE_DOUBLE, .v = { .double_value = 2 } },
                        .label = "Two"
                    }
                }
            },
    */

    if (c->parser->value_label_handler) {
        fprintf(stdout, "woof\n");
    }

    const char *val_labels = NULL;
    if (c->parser->variable_handler) {
        c->parser->variable_handler(c->columns, var, val_labels, c->user_ctx);
    }
}
