#ifndef __PRODUCE_CSV_COLUMN_HEADER_H
#define __PRODUCE_CSV_COLUMN_HEADER_H

void produce_column_header(void *s, size_t len, void *data);

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

#endif