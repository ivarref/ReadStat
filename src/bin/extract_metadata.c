#include "../readstat.h"
#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>

#include "../spss/readstat_sav_date.h"
#include "../stata/readstat_dta_days.h"
#include "format.h"

typedef struct context {
    int count;
    FILE* fp;
    int variable_count;
    int input_format;
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
    // fprintf(stdout, "%s:%d handling value label\n", __FILE__, __LINE__);
    if (value.type == READSTAT_TYPE_DOUBLE || value.type == READSTAT_TYPE_STRING || value.type == READSTAT_TYPE_INT32) {
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
        } else if (value.type == READSTAT_TYPE_INT32) {
            value_label->int32_key = value.v.i32_value;
        } else {
            fprintf(stderr, "%s:%d unsupported type!\n", __FILE__, __LINE__);
            exit(EXIT_FAILURE);
        }
        char *lbl = malloc(strlen(label) + 1);
        strcpy(lbl, label);
        value_label->label = lbl;
        value_label->label_len = strlen(label);
        label_set->value_labels_count++;
    } else {
        fprintf(stderr, "%s:%d Unhandled value.type %d\n", __FILE__, __LINE__, value.type);
        exit(EXIT_FAILURE);
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

void handle_missing_discrete(struct context *ctx, readstat_variable_t *variable) {
    int spss_date = variable->format && variable->format[0] && (strcmp(variable->format, "EDATE40") == 0) && variable->type == READSTAT_TYPE_DOUBLE;
    int missing_ranges_count = readstat_variable_get_missing_ranges_count(variable);
    fprintf(ctx->fp, ", \"missing\": { \"type\": \"DISCRETE\", \"values\": [");
    
    for (int i=0; i<missing_ranges_count; i++) {
        readstat_value_t lo_val = readstat_variable_get_missing_range_lo(variable, i);
        readstat_value_t hi_val = readstat_variable_get_missing_range_hi(variable, i);
        if (i>=1) {
            fprintf(ctx->fp, ", ");
        }

        if (readstat_value_type(lo_val) == READSTAT_TYPE_DOUBLE) {
            double lo = readstat_double_value(lo_val);
            double hi = readstat_double_value(hi_val);
            if (lo == hi && spss_date) {
                char buf[255];
                char *s = readstat_sav_date_string(lo, buf, sizeof(buf)-1);
                if (!s) {
                    fprintf(stderr, "Could not parse date %lf\n", lo);
                    exit(EXIT_FAILURE);
                }
                fprintf(ctx->fp, "\"%s\"", s);
            } else if (lo == hi) {
                fprintf(ctx->fp, "%g", lo);
            } else {
                fprintf(stderr, "%s:%d column %s unsupported lo %lf hi %lf\n", __FILE__, __LINE__, variable->name, lo, hi);
                exit(EXIT_FAILURE);
            }
        } else {
            fprintf(stderr, "%s:%d unsupported missing type\n", __FILE__, __LINE__);
            exit(EXIT_FAILURE);
        }
    }
    fprintf(ctx->fp, "]} ");
}

void handle_missing_range(struct context *ctx, readstat_variable_t *variable) {
    int spss_date = variable->format && variable->format[0] && (strcmp(variable->format, "EDATE40") == 0) && variable->type == READSTAT_TYPE_DOUBLE;
    int missing_ranges_count = readstat_variable_get_missing_ranges_count(variable);
    fprintf(ctx->fp, ", \"missing\": { \"type\": \"RANGE\", ");
    
    for (int i=0; i<missing_ranges_count; i++) {
        readstat_value_t lo_val = readstat_variable_get_missing_range_lo(variable, i);
        readstat_value_t hi_val = readstat_variable_get_missing_range_hi(variable, i);
        if (i>=1) {
            fprintf(ctx->fp, ", ");
        }

        if (readstat_value_type(lo_val) == READSTAT_TYPE_DOUBLE) {
            double lo = readstat_double_value(lo_val);
            double hi = readstat_double_value(hi_val);
            if (spss_date) {
                char buf[255];
                char buf2[255];
                char *s = readstat_sav_date_string(lo, buf, sizeof(buf)-1);
                char *s2 = readstat_sav_date_string(hi, buf2, sizeof(buf2)-1);
                if (!s) {
                    fprintf(stderr, "Could not parse date %lf\n", lo);
                    exit(EXIT_FAILURE);
                }
                if (!s2) {
                    fprintf(stderr, "Could not parse date %lf\n", hi);
                    exit(EXIT_FAILURE);
                }
                if (lo == hi) {
                    fprintf(ctx->fp, "\"discrete-value\": \"%s\"", s);
                } else {
                    fprintf(ctx->fp, "\"low\": \"%s\", \"high\": \"%s\"", s, s2);
                }
            } else {
                if (lo == hi) {
                    fprintf(ctx->fp, "\"discrete-value\": %lf", lo);
                } else {
                    fprintf(ctx->fp, "\"low\": %lf, \"high\": %lf", lo, hi);
                }
            }
        } else {
            fprintf(stderr, "%s:%d unsupported missing type\n", __FILE__, __LINE__);
            exit(EXIT_FAILURE);
        }
    }
    fprintf(ctx->fp, "} ");
}

void add_missing_values(struct context *ctx, readstat_variable_t *variable) {
    int missing_ranges_count = readstat_variable_get_missing_ranges_count(variable);
    if (missing_ranges_count == 0) {
        return;
    }
    
    int is_range = 0;
    int discrete = 0;
    int only_double = 1;

    for (int i=0; i<missing_ranges_count; i++) {
        readstat_value_t lo_val = readstat_variable_get_missing_range_lo(variable, i);
        readstat_value_t hi_val = readstat_variable_get_missing_range_hi(variable, i);
        if (readstat_value_type(lo_val) == READSTAT_TYPE_DOUBLE) {
            double lo = readstat_double_value(lo_val);
            double hi = readstat_double_value(hi_val);
            if (lo != hi) {
                is_range = 1;
            } else {
                discrete = 1;
            }
        } else {
            only_double = 0;
        }
    }

    if (!only_double) {
        fprintf(stderr, "%s:%d only implemented double support for missing values\n", __FILE__, __LINE__);
        exit(EXIT_FAILURE);
    }

    if (is_range || (is_range && discrete)) {
        handle_missing_range(ctx, variable);
    } else if (discrete) {
        handle_missing_discrete(ctx, variable);
    } else {
        fprintf(stderr, "%s:%d unexpected state\n", __FILE__, __LINE__);
        exit(EXIT_FAILURE);
    }
}

void add_val_labels(struct context *ctx, readstat_variable_t *variable, const char *val_labels) {
    if (!val_labels) {
        return;
    } else {
        fprintf(stdout, "extracting value labels for %s\n", val_labels);
    }
    int sav_date = variable->format && variable->format[0] && (strcmp(variable->format, "EDATE40") == 0) && variable->type == READSTAT_TYPE_DOUBLE;
    int dta_date = variable->format && variable->format[0] && (strcmp(variable->format, "%td") == 0) && variable->type == READSTAT_TYPE_INT32;
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
        if (sav_date) {
            char* lbl = quote_and_escape(value_label->label);
            char buf[255];
            char *s = readstat_sav_date_string(value_label->double_key, buf, sizeof(buf)-1);
            if (!s) {
                fprintf(stderr, "%s:%d could not parse double value %lf to date\n", __FILE__, __LINE__, value_label->double_key);
                exit(EXIT_FAILURE);
            }
            fprintf(ctx->fp, "{ \"code\": \"%s\", \"label\": %s} ", s, lbl);
            free(lbl);
        } else if (dta_date) {
            char* lbl = quote_and_escape(value_label->label);
            char buf[255];
            int k = value_label->int32_key;
            char tag = 0;
            if (k >= 2147483622) {
                tag = (k-2147483622)+'a';
            }
            char *s = readstat_dta_days_string(value_label->int32_key, buf, sizeof(buf)-1);
            if (!s) {
                fprintf(stderr, "%s:%d could not parse int32 value %d to date\n", __FILE__, __LINE__, value_label->int32_key);
                exit(EXIT_FAILURE);
            }
            if (tag) {
                fprintf(ctx->fp, "{ \"code\": \".%c\", \"label\": %s} ", tag, lbl);
            } else {
                fprintf(ctx->fp, "{ \"code\": \"%s\", \"label\": %s} ", s, lbl);
            }
            free(lbl);
        } else if (variable->type == READSTAT_TYPE_DOUBLE && ctx->input_format == RS_FORMAT_DTA) {
            char* lbl = quote_and_escape(value_label->label);
            int k = value_label->int32_key;
            char tag = 0;
            if (k >= 2147483622) {
                tag = (k-2147483622)+'a';
            }
            if (tag) {
                fprintf(ctx->fp, "{ \"code\": \".%c\", \"label\": %s} ", tag, lbl);
            } else {
                fprintf(ctx->fp, "{ \"code\": %d, \"label\": %s} ", k, lbl);
            }
            free(lbl);
        } else if (variable->type == READSTAT_TYPE_DOUBLE && ctx->input_format == RS_FORMAT_SAV) {
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

int handle_variable (int index, readstat_variable_t *variable, const char *val_labels, void *my_ctx) {
    struct context *ctx = (struct context *)my_ctx;

    char* type = "";
    int sav_date = variable->format && variable->format[0] && (strcmp(variable->format, "EDATE40") == 0) && variable->type == READSTAT_TYPE_DOUBLE;
    int dta_date = variable->format && variable->format[0] && (strcmp(variable->format, "%td") == 0) && variable->type == READSTAT_TYPE_INT32;

    if (variable->format && variable->format[0]) {
        fprintf(stdout, "format is %s\n", variable->format);
    }
    if (variable->type == READSTAT_TYPE_STRING) {
        type = "STRING";
    } else if (sav_date || dta_date) {
        type = "DATE";
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

    fprintf(stdout, "handling column %s of type %s\n", variable->name, type);
    
    if (ctx->count == 0) {
        ctx->count = 1;
        char *typ;
        if (ctx->input_format == RS_FORMAT_DTA) {
            typ = "STATA";
        } else if (ctx->input_format == RS_FORMAT_SAV) {
            typ = "SPSS";
        } else {
            fprintf(stderr, "%s:%d unsupported forma %dt\n", __FILE__, __LINE__, ctx->input_format);
            exit(EXIT_FAILURE);
        }
        fprintf(ctx->fp, "{\"%s\": \"%s\",\n  \"%s\": [\n", "type", typ, "variables");
    } else {
        fprintf(ctx->fp, ",\n");
    }

    
    fprintf(ctx->fp, "{\"type\": \"%s\", \"name\": \"%s\"", type, variable->name);
    if (variable->label && variable->label[0]) {
        char* lbl = quote_and_escape(variable->label);
        fprintf(ctx->fp, ", \"label\": %s", lbl);
        free(lbl);
    }

    add_val_labels(ctx, variable, val_labels);
    add_missing_values(ctx, variable);
    
    fprintf(ctx->fp, "}");
    return 0;
}

int pass(struct context *ctx, char *input, char *output, int pass) {
    if (pass==2) {
        FILE* fp = fopen(output, "w");
        if (fp == NULL) {
            fprintf(stderr, "Could not open %s for writing: %s\n", output, strerror(errno));
            exit(EXIT_FAILURE);
        }
        ctx->fp = fp;
    } else {
        ctx->fp = NULL;
    }

    int ret = 0;

    readstat_error_t error = READSTAT_OK;
    readstat_parser_t *parser = readstat_parser_init();
    if (pass == 1) {
        readstat_set_value_label_handler(parser, &handle_value_label);
    } else if (pass == 2) {
        readstat_set_variable_handler(parser, &handle_variable);
    }
    
    const char *filename = input;
    size_t len = strlen(filename);

    if (len < sizeof(".dta") -1) {
        fprintf(stderr, "Unknown input format\n");
        ret = 1;
        goto cleanup;
    }

    if (strncmp(filename + len - 4, ".sav", 4) == 0) {
        fprintf(stdout, "parsing sav file\n");
        error = readstat_parse_sav(parser, input, ctx);
    } else if (strncmp(filename + len - 4, ".dta", 4) == 0) {
        fprintf(stdout, "parsing dta file\n");
        error = readstat_parse_dta(parser, input, ctx);
    } else {
        fprintf(stderr, "Unsupported input format\n");
        ret = 1;
        goto cleanup;
    }

    if (error != READSTAT_OK) {
        fprintf(stderr, "Error processing %s: %d\n", input, error);
        ret = 1;
    } else {
        if (ctx->fp) {
            fprintf(ctx->fp, "]}\n");
            fprintf(ctx->fp, "\n");
        }
    }

cleanup: readstat_parser_free(parser);

    if (ctx->fp) {
        fclose(ctx->fp);
    }
    if (pass==2 && ctx->variable_count >=1) {
        for (int i=0; i<ctx->variable_count; i++) {
            readstat_label_set_t * label_set = &ctx->label_set[i];
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
        free(ctx->label_set);
    }

    fprintf(stdout, "pass %d done\n", pass);
    return ret;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Usage: %s <input-filename.(dta|sav)> <output-metadata.json>\n", argv[0]);
        return 1;
    }
    int ret = 0;
    struct context ctx;
    memset(&ctx, 0, sizeof(struct context));
    ctx.input_format = format(argv[1]);

    ret = pass(&ctx, argv[1], argv[2], 1);
    if (!ret) {
        ret = pass(&ctx, argv[1], argv[2], 2);
    }
    printf("extract_metadata exiting\n");
    return ret;
}