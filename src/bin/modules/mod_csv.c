#include <csv.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include "json_metadata.h"

#include "../../readstat.h"
#include "../module_util.h"
#include "../module.h"

#define UNUSED(x) (void)(x)

typedef struct mod_csv_ctx_s {
    FILE *out_file;
    long var_count;
} mod_csv_ctx_t;

typedef struct csv_metadata {
    long rows;
    long columns;
    long _columns;
    size_t* column_width;
    int open_row;
    readstat_parser_t *parser;
    void *user_ctx;
    readstat_variable_t* variables;
    struct json_metadata* json_md;
} csv_metadata;

static int accept_file(const char *filename);
static void *ctx_init(const char *filename);
static void finish_file(void *ctx);
static int handle_info(int obs_count, int var_count, void *ctx);
static int handle_variable(int index, readstat_variable_t *variable,
                           const char *val_labels, void *ctx);
static int handle_value(int obs_index, readstat_variable_t *variable, readstat_value_t value, void *ctx);

rs_module_t rs_mod_csv = {
    accept_file, /* accept */
    ctx_init, /* init */
    finish_file, /* finish */
    handle_info, /* info */
    NULL, /* metadata */
    NULL, /* note */
    handle_variable,
    NULL, /* fweight */
    handle_value,
    NULL /* value label */
};

static int accept_file(const char *filename) {
    return rs_ends_with(filename, ".csv");
}

static void *ctx_init(const char *filename) {
    mod_csv_ctx_t *mod_ctx = malloc(sizeof(mod_csv_ctx_t));
    mod_ctx->out_file = fopen(filename, "w");
    if (mod_ctx->out_file == NULL) {
        fprintf(stderr, "Error opening %s for writing: %s\n", filename, strerror(errno));
        return NULL;
    }
    return mod_ctx;
}

static void finish_file(void *ctx) {
    mod_csv_ctx_t *mod_ctx = (mod_csv_ctx_t *)ctx;
    if (mod_ctx) {
        if (mod_ctx->out_file != NULL)
            fclose(mod_ctx->out_file);
    }
}

static int handle_info(int obs_count, int var_count, void *ctx) {
    mod_csv_ctx_t *mod_ctx = (mod_csv_ctx_t *)ctx;
    mod_ctx->var_count = var_count;
    return mod_ctx->var_count == 0;
}

static int handle_variable(int index, readstat_variable_t *variable,
                           const char *val_labels, void *ctx) {
    mod_csv_ctx_t *mod_ctx = (mod_csv_ctx_t *)ctx;
    const char *name = readstat_variable_get_name(variable);
    if (index > 0) {
        fprintf(mod_ctx->out_file, ",\"%s\"", name);
    } else {
        fprintf(mod_ctx->out_file, "\"%s\"", name);
    }
    if (index == mod_ctx->var_count - 1) {
        fprintf(mod_ctx->out_file, "\n");
    }
    return 0;
}

static int handle_value(int obs_index, readstat_variable_t *variable, readstat_value_t value, void *ctx) {
    mod_csv_ctx_t *mod_ctx = (mod_csv_ctx_t *)ctx;
    readstat_type_t type = readstat_value_type(value);
    int var_index = readstat_variable_get_index(variable);
    if (var_index > 0) {
        fprintf(mod_ctx->out_file, ",");
    }
    if (readstat_value_is_system_missing(value)) {
        /* void */
    } else if (type == READSTAT_TYPE_STRING) {
        /* TODO escape */
        fprintf(mod_ctx->out_file, "\"%s\"", readstat_string_value(value));
    } else if (type == READSTAT_TYPE_INT8) {
        fprintf(mod_ctx->out_file, "%hhd", readstat_int8_value(value));
    } else if (type == READSTAT_TYPE_INT16) {
        fprintf(mod_ctx->out_file, "%hd", readstat_int16_value(value));
    } else if (type == READSTAT_TYPE_INT32) {
        fprintf(mod_ctx->out_file, "%d", readstat_int32_value(value));
    } else if (type == READSTAT_TYPE_FLOAT) {
        fprintf(mod_ctx->out_file, "%f", readstat_float_value(value));
    } else if (type == READSTAT_TYPE_DOUBLE) {
        fprintf(mod_ctx->out_file, "%lf", readstat_double_value(value));
    }
    if (var_index == mod_ctx->var_count - 1) {
        fprintf(mod_ctx->out_file, "\n");
    }
    return 0;
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
        fprintf(stdout, "readstat_type_string\n");
    } else if (coltype == READSTAT_TYPE_DOUBLE) {
        var->alignment = READSTAT_ALIGNMENT_RIGHT;
        fprintf(stdout, "readstat_type_double\n");
    } else {
        fprintf(stderr, "unsupported column type: %x\n", coltype);
        exit(EXIT_FAILURE);
    }
    
    var->index = c->columns;
    memcpy(var->name, column, len); 
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
    const char *val_labels = NULL;
    c->parser->variable_handler(c->columns, var, val_labels, c->user_ctx);
}

void produce_column_value(void *s, size_t len, void *data) {
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
    if (len == 0) {
        readstat_value_t value = {
            .is_system_missing = 1,
            .is_tagged_missing = 0,
            .type = var->type
        };
        c->parser->value_handler(obs_index, var, value, c->user_ctx);
    }
    else if (var->type == READSTAT_TYPE_STRING) {
        readstat_value_t value = {
            .is_system_missing = 0,
            .is_tagged_missing = 0,
            .v = { .string_value = s },
            .type = READSTAT_TYPE_STRING
        };
        c->parser->value_handler(obs_index, var, value, c->user_ctx);
    } else if (var->type == READSTAT_TYPE_DOUBLE) {
        double vv = strtod(s, NULL); // TODO handle malformatted data
        if (is_missing_double(c->json_md, var->name, vv)) {
            fprintf(stderr, "missing value %g\n", vv);
            readstat_value_t value = {
                .is_system_missing = 0,
                .is_tagged_missing = 1,
                .v = { .double_value = vv },
                .type = READSTAT_TYPE_DOUBLE
            };
            c->parser->value_handler(obs_index, var, value, c->user_ctx);
        } else {
            readstat_value_t value = {
                .is_system_missing = 0,
                .is_tagged_missing = 0,
                .v = { .double_value = vv },
                .type = READSTAT_TYPE_DOUBLE
            };
            c->parser->value_handler(obs_index, var, value, c->user_ctx);
        }
    } else {
        fprintf(stderr, "unhandled column type: %x\n", var->type);
        exit(EXIT_FAILURE);
    }
}

void csv_metadata_cell(void *s, size_t len, void *data)
{
    struct csv_metadata *c = (struct csv_metadata *)data;
    if (c->rows == 0) {
        c->variables = realloc(c->variables, (c->columns+1) * sizeof(readstat_variable_t));
    }
    if (c->rows == 0 && c->parser->variable_handler) {
        produce_column_header(s, len, data);
    } else if (c->rows >= 1 && c->parser->value_handler) {
        produce_column_value(s, len, data);
    }
    if (c->rows >= 1) {
        size_t w = c->column_width[c->columns];
        c->column_width[c->columns] = (len>w) ? len : w;
    }
    c->open_row = 1;
    c->columns++;
}

void csv_metadata_row(int cc, void *data)
{
    UNUSED(cc);
    struct csv_metadata *c = (struct csv_metadata *)data;
    c->rows++;
    if (c->rows == 1) {
        c->column_width = calloc(c->columns, sizeof(size_t));
        c->_columns = c->columns;
    }
    c->columns = 0;
    c->open_row = 0;
}

readstat_error_t readstat_parse_csv(readstat_parser_t *parser, const char *path, void *user_ctx) {
    readstat_error_t retval = READSTAT_OK;
    readstat_io_t *io = parser->io;
    size_t file_size = 0;
    size_t bytes_read;
    struct csv_parser csvparser;
    struct csv_parser* p = &csvparser;
    char buf[BUFSIZ];
    struct csv_metadata metadata;
    struct csv_metadata* md = &metadata;
    memset(md, 0, sizeof(csv_metadata));
    md->parser = parser;
    md->user_ctx = user_ctx;
    md->json_md = NULL;

    char* jsonpath = calloc(strlen(path)+2, sizeof(char));
    int len = strlen(path) - strlen(".csv");
    sprintf(jsonpath, "%.*s.json", len, path);
    //fprintf(stdout, "json file is >%s<\n", jsonpath);
    if ((md->json_md = get_json_metadata(jsonpath)) == NULL) {
        fprintf(stderr, "Could not get JSON metadata\n");
    }

    if (io->open(path, io->io_ctx) == -1) {
        retval = READSTAT_ERROR_OPEN;
        goto cleanup;
    }

    file_size = io->seek(0, READSTAT_SEEK_END, io->io_ctx);
    if (file_size == -1) {
        retval = READSTAT_ERROR_SEEK;
        goto cleanup;
    }

    if (io->seek(0, READSTAT_SEEK_SET, io->io_ctx) == -1) {
        retval = READSTAT_ERROR_SEEK;
        goto cleanup;
    }

    if (csv_init(p, CSV_APPEND_NULL) != 0)
    {
        retval = READSTAT_ERROR_OPEN;
        goto cleanup;
    }

    while ((bytes_read = io->read(buf, sizeof(buf), io->io_ctx)) > 0)
    {
        if (csv_parse(p, buf, bytes_read, csv_metadata_cell, csv_metadata_row, md) != bytes_read)
        {
            fprintf(stderr, "Error while parsing file: %s\n", csv_strerror(csv_error(p)));
            retval = READSTAT_ERROR_PARSE;
            goto cleanup;
        }
    }
    csv_fini(p, csv_metadata_cell, csv_metadata_row, md);
    if (!md->open_row) {
        md->rows--;
    }
    if (parser->info_handler) {
        parser->info_handler(md->rows, md->_columns, user_ctx);
    }

cleanup:
    if (md->column_width) {
        free(md->column_width);
        md->column_width = NULL;
    }
    if (md->variables) {
        free(md->variables);
        md->variables = NULL;
    }
    if (jsonpath) {
        free(jsonpath);
        jsonpath = NULL;
    }
    if (md->json_md) {
        free_json_metadata(md->json_md);
        md->json_md = NULL;
    }
    csv_free(p);
    io->close(io->io_ctx);
    return retval;
}
