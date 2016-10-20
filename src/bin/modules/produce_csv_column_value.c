#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#define __USE_XOPEN
#include <time.h>
#include <unistd.h>
#include <errno.h>

#include "../../readstat.h"
#include "../module_util.h"
#include "../module.h"
#include "produce_csv_column_value.h"
#include "json_metadata.h"
#include "../../readstat.h"

readstat_value_t value_sysmiss(void *s, size_t len, struct csv_metadata *c) {
    readstat_variable_t *var = &c->variables[c->columns];
    readstat_value_t value = {
        .is_system_missing = 1,
        .is_tagged_missing = 0,
        .type = var->type
    };
    return value;
}

readstat_value_t value_string(void *s, size_t len, struct csv_metadata *c) {
    readstat_value_t value = {
            .is_system_missing = 0,
            .is_tagged_missing = 0,
            .v = { .string_value = s },
            .type = READSTAT_TYPE_STRING
        };
    return value;
}

readstat_value_t value_double(void *s, size_t len, struct csv_metadata *c) {
    char *dest;
    readstat_variable_t *var = &c->variables[c->columns];
    double vv = strtod(s, &dest);
    if (dest == s) {
        fprintf(stderr, "not a number: %s\n", (char*)s);
        exit(EXIT_FAILURE);
    }
    int missing_idx = missing_double_idx(c->json_md, var->name, vv);
    readstat_value_t value = {
        .type = READSTAT_TYPE_DOUBLE,
        .is_tagged_missing = 0,
        .v = { .double_value = vv }
    };
    if (missing_idx) {
        value.is_tagged_missing = 1;
        value.tag = ('a' + missing_idx - 1);
    }
    return value;
}

static inline int is_leap(int year) {
    return ((year % 4 == 0 && year % 100 != 0) || year % 400 ==0);
}

int dta_numdays(char *s) {
    struct tm tim;
    memset(&tim, 0, sizeof(struct tm));
    char *ss = strptime(s, "%Y-%m-%d", &tim);
    if (!ss) {
        fprintf(stderr, "%s:%d not a date: %s\n", __FILE__, __LINE__, (char*)s);
        exit(EXIT_FAILURE);
    } else {
        // TODO code review
        int days = 0;
        int destYear = tim.tm_year+1900;
        int daysPerMonth[] =     {31,28,31,30,31,30,31,31,30,31,30,31};
        int daysPerMonthLeap[] = {31,29,31,30,31,30,31,31,30,31,30,31};

        for (int i=destYear; i<1960; i++) {
            days -= is_leap(i) ? 366 : 365;
        }

        for (int i=1960; i<destYear; i++) {
            days += is_leap(i) ? 366 : 365;
        }
       
        for (int m=0; m<tim.tm_mon; m++) {
            days += is_leap(destYear) ? daysPerMonthLeap[m] : daysPerMonth[m];
        }

        days += tim.tm_mday-1;
        return days;
    }
}

readstat_value_t value_int32(void *s, size_t len, struct csv_metadata *c) {
    readstat_value_t value = {
        .type = READSTAT_TYPE_INT32,
        .is_tagged_missing = 0,
        .v = { .i32_value = dta_numdays(s) }
    };
    readstat_variable_t *var = &c->variables[c->columns];
    int missing_idx = missing_string_idx(c->json_md, var->name, s);
    if (missing_idx) {
        value.is_tagged_missing = 1;
        value.tag = 'a' - 1 + missing_idx;
    }
    return value;
}

void produce_csv_column_value(void *s, size_t len, void *data) {
    struct csv_metadata *c = (struct csv_metadata *)data;
    readstat_variable_t *var = &c->variables[c->columns];
    int obs_index = c->rows - 1; // TODO: ???
    readstat_value_t value;
    if (len == 0) {
        value = value_sysmiss(s, len, c);
    } else if (var->type == READSTAT_TYPE_STRING) {
        value = value_string(s, len, c);
    } else if (var->type == READSTAT_TYPE_DOUBLE) {
        value = value_double(s, len, c);
    } else if (var->type == READSTAT_TYPE_INT32) {
        value = value_int32(s, len, c);
    } else {
        fprintf(stderr, "%s:%d unsupported column type: %x\n", __FILE__, __LINE__, var->type);
        exit(EXIT_FAILURE);
    }
    c->parser->value_handler(obs_index, var, value, c->user_ctx);
}