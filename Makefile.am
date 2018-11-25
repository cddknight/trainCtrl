AUTOMAKE_OPTIONS = dist-bzip2
bin_PROGRAMS = trainCtrl trainDaemon
trainCtrl_SOURCES = src/trainCtrl.c
trainDaemon_SOURCES = src/trainDaemon.c src/socketC.c
AM_CPPFLAGS = $(DEPS_CFLAGS)
LIBS = $(DEPS_LIBS)

