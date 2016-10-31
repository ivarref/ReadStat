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
    int variable_count;
    readstat_label_set_t *label_set;
} context;

readstat_label_set_t * get_label_set(const char *val_labels, struct context *ctx, int alloc_new) {
    for (int i=0; i<ctx->variable_count; i++) {
        readstat_label_set_t * lbl = &ctx->label_set[i];
        if (0 == strcmp(lbl->name, val_labels)) {
            return lbl;
        }
    }
    if (!alloc_new) {
        fprintf(stderr, "%s:%d could not find value labels %s\n", __FILE__, __LINE__, val_labels);
        return NULL;
    }
    ctx->variable_count++;
    ctx->label_set = realloc(ctx->label_set, ctx->variable_count*sizeof(readstat_label_set_t));
    if (!ctx->label_set) {
        fprintf(stderr, "%s:%d realloc error: %s\n", __FILE__, __LINE__, strerror(errno));
        return NULL;
    }
    readstat_label_set_t * lbl = &ctx->label_set[ctx->variable_count-1];
    memset(lbl, 0, sizeof(readstat_label_set_t));
    snprintf(lbl->name, sizeof(lbl->name)-1, "%s", val_labels);
    return lbl;
}

static int handle_value_label(const char *val_labels, readstat_value_t value, const char *label, void *c) {
    struct context *ctx = (struct context*)c;
    if (value.type == READSTAT_TYPE_DOUBLE || value.type == READSTAT_TYPE_STRING) {
        readstat_label_set_t * label_set = get_label_set(val_labels, ctx, 1);
        if (!label_set) {
            return READSTAT_ERROR_MALLOC;
        }
        long label_idx = label_set->value_labels_count;
        label_set->value_labels = realloc(label_set->value_labels, (1 + label_idx) * sizeof(readstat_value_label_t));
        if (!label_set->value_labels) {
            fprintf(stderr, "%s:%d realloc error: %s\n", __FILE__, __LINE__, strerror(errno));
            return READSTAT_ERROR_MALLOC;
        }
        readstat_value_label_t* value_label = &label_set->value_labels[label_idx];
        memset(value_label, 0, sizeof(readstat_value_label_t));
        if (value.type == READSTAT_TYPE_DOUBLE) {
            value_label->double_key = value.v.double_value;
        } else if (value.type == READSTAT_TYPE_STRING) {
            char *string_key = malloc(strlen(value.v.string_value) + 1);
            strcpy(string_key, value.v.string_value);
            value_label->string_key = string_key;
            value_label->string_key_len = strlen(value.v.string_value);
        }
        char *lbl = malloc(strlen(label) + 1);
        strcpy(lbl, label);
        value_label->label = lbl;
        value_label->label_len = strlen(label);
        label_set->value_labels_count++;
    } else {
        fprintf(stderr, "%s:%d Unhandled value.type %d\n", __FILE__, __LINE__, value.type);
    }
    return READSTAT_OK;
}

int escape(const char *s, char* dest) {
    char c = s[0];
    if (c == '\\') {
        if (dest) {
            dest[0] = '\\';
            dest[1] = '\\';
        }
        return 2 + escape(&s[1], dest ? &dest[2] : NULL);
    } else if (c == '"') {
        if (dest) {
            dest[0] = '\\';
            dest[1] = '"';
        }
        return 2 + escape(&s[1], dest ? &dest[2] : NULL);
    } else if (c) {
        if (dest) {
            dest[0] = c;
        }
        return 1 + escape(&s[1], dest ? &dest[1] : NULL);
    } else {
        if (dest) {
            dest[0] = '"';
            dest[1] = 0;
        }
        return 1;
    }
}

char* quote_and_escape(const char *src) {
    int newlen = 2 + escape(src, NULL);
    char *dest = malloc(newlen);
    dest[0] = '"';
    escape(src, &dest[1]);
    return dest;
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
    
    fprintf(ctx->fp, "{\"type\": \"%s\", \"name\": \"%s\"", type, variable->name);
    if (variable->label) {
        char* lbl = quote_and_escape(variable->label);
        fprintf(ctx->fp, ", \"label\": %s", lbl);
        free(lbl);
    }

    if (val_labels) {
        readstat_label_set_t * label_set = get_label_set(val_labels, ctx, 0);
        if (!label_set) {
            fprintf(stderr, "Could not find label set %s!\n", val_labels);
            exit(EXIT_FAILURE);
        }
        fprintf(ctx->fp, ", \"categories\": [");
        for (int i=0; i<label_set->value_labels_count; i++) {
            readstat_value_label_t* value_label = &label_set->value_labels[i];
            if (i>0) {
                fprintf(ctx->fp, ", ");
            }
            if (variable->type == READSTAT_TYPE_DOUBLE) {
                char* lbl = quote_and_escape(value_label->label);
                fprintf(ctx->fp, "{ \"code\": %lf, \"label\": %s} ", value_label->double_key, lbl);
                free(lbl);
            } else if (variable->type == READSTAT_TYPE_STRING) {
                char* lbl = quote_and_escape(value_label->label);
                char* stringkey = quote_and_escape(value_label->string_key);
                fprintf(ctx->fp, "{ \"code\": %s, \"label\": %s} ", stringkey, lbl);
                free(lbl);
                free(stringkey);
            } else {
                fprintf(stderr, "%s:%d Unsupported type %d\n", __FILE__, __LINE__, variable->type);
                exit(EXIT_FAILURE);
            }
            
            
        }
        fprintf(ctx->fp, "] ");
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
    if (ctx.variable_count >=1) {
        for (int i=0; i<ctx.variable_count; i++) {
            readstat_label_set_t * label_set = &ctx.label_set[i];
            for (int j=0; j<label_set->value_labels_count; j++) {
                readstat_value_label_t* value_label = &label_set->value_labels[j];
                if (value_label->string_key) {
                    free(value_label->string_key);
                } 
                if (value_label->label) {
                    free(value_label->label);
                }
            }
            free(label_set->value_labels);
        }
        free(ctx.label_set);
    }

    return 0;
}