#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#define __USE_XOPEN
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

static inline int is_leap(int year) {
    return ((year % 4 == 0 && year % 100 != 0) || year % 400 ==0);
}

int readstat_dta_num_days(char *s) {
    struct tm tim;
    memset(&tim, 0, sizeof(struct tm));
    char *ss = strptime(s, "%Y-%m-%d", &tim);
    if (!ss) {
        fprintf(stderr, "%s:%d not a date: %s\n", __FILE__, __LINE__, (char*)s);
        exit(EXIT_FAILURE);
    } else {
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

char* readstat_dta_days_string(int days, char* dest, int size) {
    // TODO: Candidate for clean up
    int yr = 1960;
    int month = 0;
    int day = 1;
    int daysPerMonth[] =     {31,28,31,30,31,30,31,31,30,31,30,31};
    int daysPerMonthLeap[] = {31,29,31,30,31,30,31,31,30,31,30,31};
    if (days < 0) {
        yr = 1959;
        month = 11;
        days = - days;
        while (days > 0) {
            int days_in_year = is_leap(yr) ? 366 : 365;
            if (days > days_in_year) {
                yr-=1;
                days-=days_in_year;
                continue;
            }
            int days_in_month = is_leap(yr) ? daysPerMonthLeap[month] : daysPerMonth[month];
            if (days > days_in_month) {
                month-=1;
                days-=days_in_month;
                continue;
            }
            day = days_in_month-days + 1;
            days = 0;
        }
    } else {
        while (days > 0) {
            int days_in_year = is_leap(yr) ? 366 : 365;
            if (days >= days_in_year) {
                yr+=1;
                days-=days_in_year;
                continue;
            }
            int days_in_month = is_leap(yr) ? daysPerMonthLeap[month] : daysPerMonth[month];
            if (days >= days_in_month) {
                month+=1;
                days-=days_in_month;
                continue;
            }
            day+= days;
            days = 0;
        }
    }
    snprintf(dest, size, "%04d-%02d-%02d", yr, month+1, day);
    return dest;
}
