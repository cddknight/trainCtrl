AUTOMAKE_OPTIONS = dist-bzip2
bin_PROGRAMS = trainCtrl trainDaemon
trainCtrl_SOURCES = src/trainCtrl.c src/trainTrack.c src/trainConnect.c src/socketC.c src/trainCtrl.h src/socketC.h src/train.xpm
trainDaemon_SOURCES = src/trainDaemon.c src/socketC.c src/trainCtrl.h src/socketC.h
AM_CPPFLAGS = $(DEPS_CFLAGS)
LIBS = $(DEPS_LIBS) -lpthread
EXTRA_DIST = trainCtrl.desktop trainCtrl.svg COPYING AUTHORS
Icondir = $(datadir)/pixmaps
Icon_DATA = trainCtrl.svg
Applicationsdir = $(datadir)/applications
Applications_DATA = trainCtrl.desktop

