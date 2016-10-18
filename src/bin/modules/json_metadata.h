#include "jsmn.h"
#include "../../readstat.h"

#ifndef __JSON_METADATA_H_
#define __JSON_METADATA_H_

typedef struct json_metadata {
    char* js;
    jsmntok_t* tok;
} json_metadata;

struct json_metadata* get_json_metadata(char* filename);
readstat_type_t column_type(struct json_metadata* md, char* varname);
void free_json_metadata(struct json_metadata*);

int is_missing_double(struct json_metadata* md, char* varname, double v);
char* copy_variable_property(struct json_metadata* md, const char* varname, const char* property, char* dest, size_t maxsize);

#endif /* __JSON_METADATA_H_ */

