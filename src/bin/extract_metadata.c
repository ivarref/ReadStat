#define _BSD_SOURCE
#include "../readstat.h"
#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

typedef struct context {
    int count;
    FILE* fp;
    long variable_count;
    readstat_label_set_t *label_set;
} context;

static int handle_value_label(const char *val_labels, readstat_value_t value, const char *label, void *c) {
    struct context *ctx = (struct context*)c;
    // TODO: Will this work with anything other than SAV?
    if (value.type == READSTAT_TYPE_DOUBLE && strncmp(val_labels, "labels", strlen("labels")) == 0) {
        char* dest;
        const char *src = val_labels+strlen("labels");
        long idx = strtol(src, &dest, 10);
        if (dest == src) {
            fprintf(stderr, "%s:%d expected a number, got %s\n", __FILE__, __LINE__, src);
            exit(EXIT_FAILURE);
        }
        if (ctx->variable_count == idx) {
            long size = idx+1;
            ctx->label_set = realloc(ctx->label_set, size*sizeof(readstat_label_set_t));
            if (!ctx->label_set) {
                fprintf(stderr, "%s:%d realloc error: %s\n", __FILE__, __LINE__, strerror(errno));
                return READSTAT_ERROR_MALLOC;
            }

            memset(&ctx->label_set[idx], 0, sizeof(readstat_label_set_t));
            fprintf(stdout, "alloc for index %ld\n", idx);
            ctx->variable_count++;
        }
        readstat_label_set_t* label_set = &ctx->label_set[idx];
        snprintf(label_set->name, sizeof(label_set->name)-1, "%s", val_labels);
        long label_idx = label_set->value_labels_count;
        label_set->value_labels = realloc(label_set->value_labels, (1 + label_idx) * sizeof(readstat_value_label_t));
        readstat_value_label_t* value_label = &label_set->value_labels[label_idx];
        memset(value_label, 0, sizeof(readstat_value_label_t));
        value_label->double_key = value.v.double_value;
        value_label->label = strdup(label);
        value_label->label_len = strlen(label);

        label_set->value_labels_count++;
    }
    return 0;
}

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
    
    //fprintf(stdout, "enter column %s\n", variable->name);
    fprintf(ctx->fp, "{\"type\": \"%s\", \"name\": \"%s\"", type, variable->name);
    if (variable->label) {
        fprintf(ctx->fp, ", \"label\": \"%s\" ", variable->label);
    }

    printf("variable index is %d for %s\n", variable->index, variable->name);
    variable->label_set = &ctx->label_set[variable->index];
    if (variable->label_set) {
        //fprintf(ctx->fp, ", \"missing\": { \"type\": \"DISCRETE\", \"values\": [");
        for (int i=0; i<variable->label_set->value_labels_count; i++) {
            readstat_value_label_t* value_label = &variable->label_set->value_labels[i]; 
            printf("code: %lf label: %s \n", value_label->double_key, value_label->label);
            //fprintf(ctx->fp, ", \"categories\": [] ");
        }
    }

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
    memset(&ctx, 0, sizeof(struct context));

    FILE* fp = fopen(argv[2], "w");
    if (fp == NULL) {
		fprintf(stderr, "Could not open %s for writing: %s\n", argv[2], strerror(errno));
        exit(EXIT_FAILURE);
    }
    ctx.fp = fp;
    
    readstat_error_t error = READSTAT_OK;
    readstat_parser_t *parser = readstat_parser_init();
    readstat_set_variable_handler(parser, &handle_variable);
    readstat_set_value_label_handler(parser, &handle_value_label);

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