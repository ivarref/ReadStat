lib_LTLIBRARIES = libreadstat.la

libreadstat_la_SOURCES = \
	src/CKHashTable.c \
	src/readstat_bits.c \
	src/readstat_convert.c \
	src/readstat_error.c \
	src/readstat_io_unistd.c \
	src/readstat_parser.c \
	src/readstat_value.c \
	src/readstat_variable.c \
	src/readstat_writer.c \
	src/sas/readstat_sas.c \
	src/sas/readstat_sas7bcat_read.c \
	src/sas/readstat_sas7bcat_write.c \
	src/sas/readstat_sas7bdat_read.c \
	src/sas/readstat_sas7bdat_write.c \
	src/sas/readstat_sas_rle.c \
	src/spss/readstat_por.c \
	src/spss/readstat_por_parse.c \
	src/spss/readstat_por_read.c \
	src/spss/readstat_por_write.c \
	src/spss/readstat_sav.c \
	src/spss/readstat_sav_parse.c \
	src/spss/readstat_sav_parse_timestamp.c \
	src/spss/readstat_sav_read.c \
	src/spss/readstat_sav_write.c \
	src/spss/readstat_spss.c \
	src/spss/readstat_spss_parse.c \
	src/stata/readstat_dta.c \
	src/stata/readstat_dta_days.c \
	src/stata/readstat_dta_parse_timestamp.c \
	src/stata/readstat_dta_read.c \
	src/stata/readstat_dta_write.c

if HAVE_RAGEL
.rl.c:
	$(AM_V_GEN)$(RAGEL) $(RAGELFLAGS) -C $< -o $@
endif

libreadstat_la_CFLAGS = -Wall -pedantic-errors -std=c99
libreadstat_la_LIBADD = @EXTRA_LIBS@
libreadstat_la_LDFLAGS = @EXTRA_LDFLAGS@

if CODE_COVERAGE_ENABLED
libreadstat_la_CFLAGS += -O0 -fprofile-arcs -ftest-coverage
endif

include_HEADERS = src/readstat.h

noinst_HEADERS = \
       src/CKHashTable.h \
       src/readstat_bits.h \
       src/readstat_convert.h \
       src/readstat_iconv.h \
       src/readstat_io_unistd.h \
       src/readstat_writer.h \
       src/sas/readstat_sas.h \
       src/sas/readstat_sas_rle.h \
       src/spss/readstat_por.h \
       src/spss/readstat_por_parse.h \
       src/spss/readstat_sav.h \
       src/spss/readstat_sav_parse.h \
       src/spss/readstat_sav_parse_timestamp.h \
       src/spss/readstat_spss.h \
       src/spss/readstat_spss_parse.h \
       src/stata/readstat_dta.h \
       src/stata/readstat_dta_days.h \
       src/stata/readstat_dta_parse_timestamp.h \
       src/bin/module.h \
       src/bin/module_util.h \
       src/bin/modules/mod_csv.h \
       src/bin/modules/jsmn.h \
       src/bin/modules/json_metadata.h \
       src/bin/modules/produce_csv_column_value.h \
       src/bin/modules/produce_csv_column_header.h \
       src/bin/modules/mod_readstat.h \
       src/bin/modules/mod_xlsx.h \
       src/test/test_buffer.h \
       src/test/test_dta.h \
       src/test/test_error.h \
       src/test/test_read.h \
       src/test/test_readstat.h \
       src/test/test_types.h \
       src/test/test_write.h

bin_PROGRAMS = \
   readstat

readstat_SOURCES = \
	src/bin/readstat.c \
	src/bin/module_util.c \
	src/bin/modules/mod_csv.c \
	src/bin/modules/jsmn.c \
	src/bin/modules/json_metadata.c \
	src/bin/modules/produce_csv_column_value.c \
	src/bin/modules/produce_csv_column_header.c \
	src/bin/modules/mod_readstat.c

readstat_LDADD = libreadstat.la
readstat_CFLAGS = -Wall -pedantic-errors -std=c99

if HAVE_XLSXWRITER
readstat_SOURCES += src/bin/modules/mod_xlsx.c
readstat_LDADD += -lxlsxwriter
readstat_CFLAGS += -DHAVE_XLSXWRITER=1
endif

readstat_LDADD += -lcsv

check_PROGRAMS = \
	test_readstat \
	test_csv_to_dta

test_readstat_SOURCES = \
	src/test/test_buffer.c \
	src/test/test_dta.c \
	src/test/test_error.c \
	src/test/test_read.c \
	src/test/test_readstat.c \
	src/test/test_sas.c \
	src/test/test_write.c

test_readstat_LDADD = libreadstat.la
test_readstat_CFLAGS = -g

test_csv_to_dta_SOURCES = \
	src/test/test_csv_to_dta.c

test_csv_to_dta_LDADD = libreadstat.la
test_csv_to_dta_CFLAGS = -g -Wextra -Werror -pedantic-errors -std=c99

TESTS = test_readstat test_csv_to_dta

.PHONY: check-valgrind ;

check-valgrind: $(TESTS)
	@for test in $$(echo $(TESTS) | sed 's/tests\//tests\/.libs\//g') ; do \
		CK_FORK=no libtool --mode=execute valgrind --error-exitcode=1 \
		--leak-check=full $${test} ; \
		done

install-exec-hook:
	@(cd $(DESTDIR)$(libdir) && $(RM) $(lib_LTLIBRARIES))
