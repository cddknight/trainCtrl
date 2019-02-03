AUTOMAKE_OPTIONS = dist-bzip2
bin_PROGRAMS = traincontrol traindaemon pointdaemon traincalc
traincontrol_SOURCES = src/trainControl.c src/trainTrack.c src/trainConnect.c src/socketC.c src/trainControl.h src/socketC.h src/train.xpm
traincontrol_LDADD = $(DEPS_LIBS) -lpthread
traindaemon_SOURCES = src/trainDaemon.c src/trainTrack.c src/socketC.c src/trainControl.h src/socketC.h
traindaemon_LDADD = -lxml2
pointdaemon_SOURCES = src/pointDaemon.c src/pointControl.c src/socketC.c src/pca9685.c src/pointControl.h src/socketC.h
pointdaemon_LDADD = -lxml2 $(WIRING_LIBS)
traincalc_SOURCES = src/trainCalc.c
AM_CPPFLAGS = $(DEPS_CFLAGS)
EXTRA_DIST = traincontrolrc.xml traincontrol.desktop traincontrol.svg COPYING AUTHORS
Icondir = $(datadir)/pixmaps
Icon_DATA = traincontrol.svg
Applicationsdir = $(datadir)/applications
Applications_DATA = traincontrol.desktop
Configdir = /etc
Config_DATA = traincontrolrc.xml

