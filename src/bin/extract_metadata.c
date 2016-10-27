#include "../readstat.h"
#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>

typedef struct context {
    int count;
    FILE* fp;
} context;

int handle_variable (int index, readstat_variable_t *variable, const char *val_labels, void *my_ctx) {
    struct context *ctx = (struct context *)my_ctx;
    char* type = "";
    if (variable->type == READSTAT_TYPE_STRING) {
        type = "STRING";
    } else if (variable->type == READSTAT_TYPE_DOUBLE) {
        type = "NUMERIC";
    } else if (variable->type == READSTAT_TYPE_INT8) {
        fprintf(stderr, "untested int8\n");
        exit(EXIT_FAILURE);
    } else if (variable->type == READSTAT_TYPE_INT16) {
        fprintf(stderr, "untested int16\n");
        exit(EXIT_FAILURE);
    } else if (variable->type == READSTAT_TYPE_INT32) {
        fprintf(stderr, "untested int32\n");
        exit(EXIT_FAILURE);
    } else if (variable->type == READSTAT_TYPE_FLOAT) {
        fprintf(stderr, "untested float\n");
        exit(EXIT_FAILURE);
    } else if (variable->type == READSTAT_TYPE_STRING_REF) {
        fprintf(stderr, "untested type string_ref\n");
        exit(EXIT_FAILURE);
    } else {
        fprintf(stderr, "unknown type\n");
        exit(EXIT_FAILURE);
    }
    
    if (ctx->count == 0) {
        ctx->count = 1;
        fprintf(ctx->fp, "{\"%s\": \"%s\",\n  \"%s\": [\n", "type", "STATA", "variables");
    } else {
        fprintf(ctx->fp, ",\n");
    }
    
    fprintf(ctx->fp, "{\"type\": \"%s\", \"name\": \"%s\"", type, variable->name);
    if (variable->missingness.missing_ranges_count) {
        fprintf(ctx->fp, ", \"missing\": { \"type\": \"DISCRETE\", \"values\": [");
        for (int i=0; i<variable->missingness.missing_ranges_count; i++) {
            if (variable->type == READSTAT_TYPE_DOUBLE) {
                double v = variable->missingness.missing_ranges[i].v.double_value;
                if (i>0) {
                    fprintf(ctx->fp, ", ");
                }
                fprintf(ctx->fp, "%g", v);
            } else {
                fprintf(stderr, "%s:%d Unsupported variable type %d", __FILE__, __LINE__, variable->type);
                exit(EXIT_FAILURE);
            }
        }
        fprintf(ctx->fp, "]}");
    }
    fprintf(ctx->fp, "}");
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Usage: %s <input-filename> <output-metadata.json>\n", argv[0]);
        return 1;
    }
    struct context ctx;
    ctx.count = 0;

    FILE* fp = fopen(argv[2], "w");
    if (fp == NULL) {
		fprintf(stderr, "Could not open %s for writing: %s\n", argv[2], strerror(errno));
        exit(EXIT_FAILURE);
    }
    ctx.fp = fp;
    
    readstat_error_t error = READSTAT_OK;
    readstat_parser_t *parser = readstat_parser_init();
    readstat_set_variable_handler(parser, &handle_variable);

    error = readstat_parse_sav(parser, argv[1], &ctx);

    readstat_parser_free(parser);

    if (error != READSTAT_OK) {
        fprintf(stderr, "Error processing %s: %d\n", argv[1], error);
        return 1;
    }
    fprintf(fp, "]}\n");

    fprintf(fp, "\n");

    fclose(fp);
    printf("extract_metadata exiting\n");
    return 0;
}