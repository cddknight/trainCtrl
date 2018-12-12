AUTOMAKE_OPTIONS = dist-bzip2
bin_PROGRAMS = traincontrol traindaemon
traincontrol_SOURCES = src/trainControl.c src/trainTrack.c src/trainConnect.c src/socketC.c src/trainControl.h src/socketC.h src/train.xpm
traindaemon_SOURCES = src/trainDaemon.c src/trainTrack.c src/socketC.c src/trainControl.h src/socketC.h
AM_CPPFLAGS = $(DEPS_CFLAGS)
LIBS = $(DEPS_LIBS) -lpthread
EXTRA_DIST = trainconfig.xml traincontrol.desktop traincontrol.svg COPYING AUTHORS
Icondir = $(datadir)/pixmaps
Icon_DATA = traincontrol.svg
Applicationsdir = $(datadir)/applications
Applications_DATA = traincontrol.desktop
Configdir = /etc
Config_DATA = trainconfig.xml

