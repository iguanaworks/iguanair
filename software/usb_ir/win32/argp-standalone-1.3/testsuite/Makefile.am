AM_CPPFLAGS = -I$(top_srcdir)

noinst_PROGRAMS = ex1 ex3 ex4

TS_PROGS =
TS_SH = ex1-test permute-test

TS_ALL = $(TS_PROGS) $(TS_SH)

LDADD = ../libargp.a

EXTRA_DIST = $(TS_SH) run-tests
CLEANFILES = test.out

.PHONY: check 
check: $(TS_ALL) $(srcdir)/run-tests
	srcdir="$(srcdir)" $(srcdir)/run-tests $(TS_ALL)
