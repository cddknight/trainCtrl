AUTOMAKE_OPTIONS = dist-bzip2
bin_PROGRAMS = traincontrol traindaemon pointdaemon traincalc pointtest
traincontrol_SOURCES = src/trainControl.c src/trainTrack.c src/trainConnect.c src/socketC.c src/trainControl.h src/trainThrottle.c src/socketC.h buildDate.h src/train.xpm
traincontrol_LDADD = $(DEPS_LIBS) -lpthread
pointtest_SOURCES = src/pointTest.c src/pca9685.c src/pca9685.h
pointtest_LDADD = $(DEPS_LIBS) -lpthread $(WIRING_LIBS)
traindaemon_SOURCES = src/trainDaemon.c src/trainTrack.c src/socketC.c src/trainControl.h src/socketC.h buildDate.h
traindaemon_LDADD = -lxml2
pointdaemon_SOURCES = src/pointDaemon.c src/pointControl.c src/servoCtrl.c src/socketC.c src/pca9685.c src/pointControl.h src/socketC.h src/pca9685.h src/servoCtrl.h buildDate.h
pointdaemon_LDADD = -lxml2 -lpthread $(WIRING_LIBS) 
traincalc_SOURCES = src/trainCalc.c
AM_CPPFLAGS = $(DEPS_CFLAGS)
EXTRA_DIST = track.xml trackrc.xml points.xml traincontrol.desktop traincontrol.svg traincontrol.png system/pointdaemon.service system/traindaemon.service COPYING AUTHORS
Icondir = $(datadir)/pixmaps
Icon_DATA = traincontrol.svg traincontrol.png
Applicationsdir = $(datadir)/applications
Applications_DATA = traincontrol.desktop
Configdir = /etc/train
Config_DATA = trackrc.xml track.xml points.xml
systemddir = /etc/systemd/system
systemd_DATA = system/pointdaemon.service system/traindaemon.service
BUILT_SOURCES = buildDate.h
CLEANFILES = buildDate.h
buildDate.h:
	setBuildDate -c
