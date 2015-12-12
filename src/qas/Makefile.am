bin_PROGRAMS = qas
qas_CFLAGS = -I. -I../../util -I../../libq -I../../libqcircuit @GLOBAL_CFLAGS@
qas_LDFLAGS = @GLOBAL_LDFLAGS@

qas_LDADD = ../../util/libutil.la ../../libqcircuit/libqcircuit.la ../../libq/libq.la @GLOBAL_LDFLAGS@

qas_SOURCES = include.c main.c parser.c qas.h qas.c

