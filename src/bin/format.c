#include <string.h>

#include "format.h"

int format(const char *filename) {
    size_t len = strlen(filename);
    if (len < sizeof(".dta")-1)
        return RS_FORMAT_UNKNOWN;

    if (strncmp(filename + len - 4, ".dta", 4) == 0)
        return RS_FORMAT_DTA;

    if (strncmp(filename + len - 4, ".sav", 4) == 0)
        return RS_FORMAT_SAV;

    if (strncmp(filename + len - 4, ".por", 4) == 0)
        return RS_FORMAT_POR;

    #if HAVE_CSVREADER
    if (strncmp(filename + len - 4, ".csv", 4) == 0)
        return RS_FORMAT_CSV;
    #endif

    if (strncmp(filename + len - 4, ".xpt", 4) == 0)
        return RS_FORMAT_XPORT;

    if (len < sizeof(".json")-1)
        return RS_FORMAT_UNKNOWN;
    
    if (strncmp(filename + len - 5, ".json", 5) == 0)
        return RS_FORMAT_JSON;

    if (len < sizeof(".sas7bdat")-1)
        return RS_FORMAT_UNKNOWN;

    if (strncmp(filename + len - 9, ".sas7bdat", 9) == 0)
        return RS_FORMAT_SAS_DATA;

    if (strncmp(filename + len - 9, ".sas7bcat", 9) == 0)
        return RS_FORMAT_SAS_CATALOG;

    return RS_FORMAT_UNKNOWN;
}
