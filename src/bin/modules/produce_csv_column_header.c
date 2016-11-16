#include <stdio.h>
#include <stdlib.h>

#include "../../readstat.h"
#include "../format.h"
#include "json_metadata.h"

#include "produce_value_label.h"
#include "produce_missingness.h"
#include "produce_csv_column_header.h"

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
    } else if (coltype == READSTAT_TYPE_DOUBLE) {
        var->alignment = READSTAT_ALIGNMENT_RIGHT;
    } else if (coltype == READSTAT_TYPE_INT32) {
        var->alignment = READSTAT_ALIGNMENT_RIGHT;
    } else {
        fprintf(stderr, "%s:%d unsupported column type: %x\n", __FILE__, __LINE__, coltype);
        exit(EXIT_FAILURE);
    }

    if (is_date_column && c->output_format == RS_FORMAT_SAV) {
        snprintf(var->format, sizeof(var->format)-1, "%s", "EDATE40");
    } else if (is_date_column && c->output_format == RS_FORMAT_DTA) {
        snprintf(var->format, sizeof(var->format)-1, "%s", "%td");
    } else if (is_date_column && c->output_format == RS_FORMAT_CSV) {
        // ignore
    } else if (is_date_column) {
        fprintf(stderr, "%s:%d unsupported date column for format %d\n", __FILE__, __LINE__, c->output_format);
        exit(EXIT_FAILURE);
    }

    if (c->pass == 2 && coltype == READSTAT_TYPE_STRING) {
        var->storage_width = c->column_width[c->columns];
    }
    
    var->index = c->columns;
    copy_variable_property(c->json_md, column, "label", var->label, sizeof(var->label));
    snprintf(var->name, sizeof(var->name)-1, "%.*s", (int)len, column);

    produce_missingness(c, column);
    if (c->parser->value_label_handler) {
        produce_value_label(c, column);
    }

    if (c->parser->variable_handler && c->pass == 2) {
        c->parser->variable_handler(c->columns, var, column, c->user_ctx);
    }
}
