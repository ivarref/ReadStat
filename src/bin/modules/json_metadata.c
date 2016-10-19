#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include "jsmn.h"
#include "json_metadata.h"
#include "../../readstat.h"

/* Function realloc_it() is a wrapper function for standart realloc()
 * with one difference - it frees old memory pointer in case of realloc
 * failure. Thus, DO NOT use old data pointer in anyway after call to
 * realloc_it(). If your code has some kind of fallback algorithm if
 * memory can't be re-allocated - use standart realloc() instead.
 */
static inline void *realloc_it(void *ptrmem, size_t size) {
	void *p = realloc(ptrmem, size);
	if (!p)  {
		free (ptrmem);
		fprintf(stderr, "realloc(): errno=%d\n", errno);
	}
	return p;
}

int slurp_object(jsmntok_t *t) {
    int res = 1;
    for (int i=0; i<t->size; i++) {
        res+= slurp_object((t+res));
    }
    return res;
}

int match_token(const char *js, jsmntok_t *tok, const char* name) {
    unsigned int len = tok->end - tok->start;
    return (tok->type == JSMN_STRING) && (len == strlen(name)) && (0==strncmp(js+tok->start, name, len));
}

jsmntok_t* find_object_property(const char *js, jsmntok_t *t, const char* propname) {
    int j = 0;
    for (int i = 0; i < t->size; i++) {
        jsmntok_t* tok = t+1+j;
        if (match_token(js, tok, propname)) {
            return tok+1;
        }
        j+= slurp_object(tok);
    }
    return 0;
}

char* get_object_property(const char *js, jsmntok_t *t, const char* propname, char* dest, size_t size) {
	jsmntok_t* tok = find_object_property(js, t, propname);
	if (!tok) {
		return NULL;
	}
	snprintf(dest, size-1, "%.*s", tok->end-tok->start, js+tok->start);
	return dest;
}

jsmntok_t* find_variable_property(const char *js, jsmntok_t *t, const char* varname, const char* property) {
    if (t->type != JSMN_OBJECT) {
        fprintf(stderr, "expected root token to be OBJECT\n");
        return 0;
    }

    jsmntok_t* variables = find_object_property(js, t, "variables");
    if (!variables) {
        fprintf(stderr, "Could not find variables property\n");
        return 0;
    }
    int j = 0;
    for (int i=0; i<variables->size; i++) {
        jsmntok_t* variable = variables+1+j;
        jsmntok_t* name = find_object_property(js, variable, "name");
        if (name && match_token(js, name, varname)) {
            return find_object_property(js, variable, property);
        } else if (name == 0) {
            fprintf(stderr, "name property not found\n");
        }
        j += slurp_object(variable);
    }
    return 0;
}

char* copy_variable_property(struct json_metadata* md, const char* varname, const char* property, char* dest, size_t maxsize) {
	jsmntok_t* tok = find_variable_property(md->js, md->tok, varname, property);
	if (tok == NULL) {
		return NULL;
	}
	
	int len = tok->end - tok->start;
	if (len == 0) {
		return NULL;
	}
	snprintf(dest, maxsize-1, "%.*s", len, md->js+tok->start);

	return dest;
}

int missing_double_idx(struct json_metadata* md, char* varname, double v) {
	jsmntok_t* missing = find_variable_property(md->js, md->tok, varname, "missing");
	if (!missing) {
		return 0;
	}

	jsmntok_t* values = find_object_property(md->js, missing, "values");
	if (!values) {
		return 0;
	}

	int j = 1;
	for (int i=0; i<values->size; i++) {
		jsmntok_t* value = values+j;
		int len = value->end - value->start;
		char tmp[1024];
		snprintf(tmp, sizeof(tmp)-1, "%.*s", len, md->js + value->start);
		char *dest;
		double vv = strtod(tmp, &dest);
		if (dest == tmp) {
			fprintf(stderr, "Expected a number: %s\n", tmp);
			exit(EXIT_FAILURE);
		}
		if (vv == v) {
			return i+1;
		}
		j+= slurp_object(value);
	}
	return 0;
}

readstat_type_t column_type(struct json_metadata* md, char* varname) {
	jsmntok_t* typ = find_variable_property(md->js, md->tok, varname, "type");
	if (!typ) {
		fprintf(stderr, "Could not find type of variable %s in metadata\n", varname);
		exit(EXIT_FAILURE);
	}

	if (match_token(md->js, typ, "NUMERIC")) {
		return READSTAT_TYPE_DOUBLE;
	} else if (match_token(md->js, typ, "STRING")) {
		return READSTAT_TYPE_STRING;
	} else {
		fprintf(stderr, "Unknown metadata type for variable %s\n", varname);
		exit(EXIT_FAILURE);
	}
}

struct json_metadata* get_json_metadata(char* filename) {
    struct json_metadata* result = malloc(sizeof(struct json_metadata));
    if (result == NULL) {
        fprintf(stderr, "malloc failed\n");
        return 0;
    }
    int r;
	int eof_expected = 0;
	char *js = NULL;
	size_t jslen = 0;
	char buf[BUFSIZ];
    FILE* fd = NULL;

	jsmn_parser p;
	jsmntok_t *tok = NULL;
	size_t tokcount = 10;

	/* Prepare parser */
	jsmn_init(&p);

	/* Allocate some tokens as a start */
	tok = malloc(sizeof(*tok) * tokcount);
	if (tok == NULL) {
		fprintf(stderr, "malloc(): error:%s\n", strerror(errno));
		goto errexit;
	}
	
	fd = fopen(filename, "rb");
	if (fd == NULL) {
		fprintf(stderr, "Could not open %s: %s\n", filename, strerror(errno));
		goto errexit;
	}
	for (;;) {
		/* Read another chunk */
		r = fread(buf, 1, sizeof(buf), fd);
		if (r < 0) {
			fprintf(stderr, "fread(): %s\n", strerror(errno));
			goto errexit;
		}
		if (r == 0) {
			if (eof_expected != 0) {
                break;
			} else {
				fprintf(stderr, "fread(): unexpected EOF\n");
				goto errexit;
			}
		}

		js = realloc_it(js, jslen + r + 1);
		if (js == NULL) {
			goto errexit;
		}
		strncpy(js + jslen, buf, r);
		jslen = jslen + r;

again:
		r = jsmn_parse(&p, js, jslen, tok, tokcount);
		if (r < 0) {
			if (r == JSMN_ERROR_NOMEM) {
				tokcount = tokcount * 2;
				tok = realloc_it(tok, sizeof(*tok) * tokcount);
				if (tok == NULL) {
					goto errexit;
				}
				goto again;
			}
		} else {
			eof_expected = 1;
		}
	}
    fclose(fd);
	result->tok = tok;
	result->js = js;
    return result;

	errexit:
	fprintf(stderr, "error during json metadata parsing\n");
	if (fd) {
		fclose(fd);
		fd = NULL;
	}
	if (tok) {
		free(tok);
		tok = NULL;
	}
	if (js) {
		free(js);
		js = NULL;
	}
	if (result) {
		free(result);
		result = NULL;
	}
	return NULL;
}

void free_json_metadata(struct json_metadata* md) {
	free(md->tok);
	free(md->js);
	free(md);
}
