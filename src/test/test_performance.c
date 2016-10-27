#define _BSD_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/time.h>
#include <string.h>

int main(int argc, char *argv[])
{
    int write_csv = 1;
    int row_count = 1000;
    int v_count = 1000;
    if (write_csv)
    {
        printf("start creating tmp.csv ...\n");
        FILE *f;
        f = fopen("tmp.csv", "w");
        for (int i = 0; i < row_count; i++)
        {
            for (int v = 0; v < v_count; v++)
            {
                if (v > 0)
                {
                    fprintf(f, ",");
                }
                if (i == 0)
                {
                    fprintf(f, "var%d", v);
                }
                else
                {
                    fprintf(f, "%d", v);
                }
            }
            fprintf(f, "\n");
        }
        fclose(f);
        printf("done creating tmp.csv ...\n");
    }

    int write_meta = 1;
    if (write_meta)
    {
        FILE *f;
        f = fopen("tmp.json", "w");
        fprintf(f, "{ \"type\" : \"STATA\", \"variables\" : [ \n");
        for (int v = 0; v < v_count; v++)
        {
            if (v > 0)
            {
                fprintf(f, ",\n");
            }
            fprintf(f, "{ \"name\": \"var%d\", \"type\": \"NUMERIC\" }", v);
        }
        fprintf(f, "\n]\n}\n");
        fclose(f);
    }

    printf("hello\n");
    return 0;
}