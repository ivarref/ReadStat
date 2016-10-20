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
#include "produce_csv_column_value.h"
#include "produce_csv_column_header.h"
#include "../../stata/readstat_dta_days.h"

#define UNUSED(x) (void)(x)

typedef struct mod_csv_ctx_s {
    FILE *out_file;
    long var_count;
} mod_csv_ctx_t;


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

static inline int is_leap(int year) {
    return ((year % 4 == 0 && year % 100 != 0) || year % 400 ==0);
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
    } else if (type == READSTAT_TYPE_INT32 && variable->format[0] && 0 == strncmp("%td", variable->format, strlen("%td"))) {
        int days = readstat_int32_value(value);
        char days_str[255];
        readstat_dta_days_string(days, days_str, sizeof(days_str)-1);
        fprintf(mod_ctx->out_file, "%s", days_str);
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

void csv_metadata_cell(void *s, size_t len, void *data)
{
    struct csv_metadata *c = (struct csv_metadata *)data;
    if (c->rows == 0) {
        c->variables = realloc(c->variables, (c->columns+1) * sizeof(readstat_variable_t));
    }
    if (c->rows == 0) {
        produce_column_header(s, len, data);
    } else if (c->rows >= 1 && c->parser->value_handler) {
        produce_csv_column_value(s, len, data);
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
