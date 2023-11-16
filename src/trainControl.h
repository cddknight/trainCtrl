/**********************************************************************************************************************
 *                                                                                                                    *
 *  T R A I N  C O N T R O L . H                                                                                      *
 *  ============================                                                                                      *
 *                                                                                                                    *
 *  Copyright (c) 2023 Chris Knight                                                                                   *
 *                                                                                                                    *
 *  File trainControl.h part of TrainControl is free software: you can redistribute it and/or modify it under the     *
 *  terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the     *
 *  License, or (at your option) any later version.                                                                   *
 *                                                                                                                    *
 *  TrainControl is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the        *
 *  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for  *
 *  more details.                                                                                                     *
 *                                                                                                                    *
 *  You should have received a copy of the GNU General Public License along with this program. If not, see:           *
 *  <http://www.gnu.org/licenses/>                                                                                    *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  \file
 *  \brief Header file for train control.
 */
#define POWER_SRT			-1
#define POWER_OFF			0
#define POWER_ON			1
#define TRACK_FLAG_SLOW		1
#define TRACK_FLAG_SHOW		2
#define TRACK_FLAG_THRT		4

typedef struct _pointCell
{
	unsigned short point;
	unsigned short link;
	unsigned short state;
	unsigned short pointDef;
	unsigned short server;
	unsigned short ident;
}
pointCellDef;

typedef struct _signalCell
{
	unsigned short signal;
	unsigned short state;
	unsigned short server;
	unsigned short ident;
}
signalCellDef;

typedef struct _trackCell
{
	unsigned short layout;
	signalCellDef signal;
	pointCellDef point;
}
trackCellDef;

typedef struct _trackLayout
{
	unsigned int trackRows;
	unsigned int trackCols;
	unsigned int trackSize;
	trackCellDef *trackCells;
}
trackLayoutDef;

typedef struct _trainFunc
{
	int funcID;
	int trigger;
	char funcDesc[41];
#ifdef __GTK_H__
	GtkWidget *funcSwitch;
#else
	void *xPointer;
#endif
}
trainFuncDef;

typedef struct _trainCtrl
{
	int trainReg;
	int trainID;
	int trainNum;
	int curSpeed;
	int reverse;
	int slowSpeed;
	int funcCount;
	char trainDesc[41];
	char funcState[100];

	struct timeval lastChange;
	int remoteCurSpeed;
	int remoteReverse;
	trainFuncDef *trainFunc;

#ifdef __GTK_H__
	GtkWidget *buttonNum;
	GtkWidget *buttonHalt;
	GtkWidget *buttonSlow;
	GtkWidget *scaleSpeed;
	GtkWidget *checkDir;
#else
	void *xPointers[5];
#endif
}
trainCtrlDef;

typedef struct _pointCtrl
{
	char clientName[41];
	int port;
	int ident;
	int intHandle;
	int retry;
}
pointCtrlDef;

typedef struct _throttleDef
{
	int axis;
	int button;
	int defTrain;
	int zeroHigh;
	int curValue;
	int curChanged;
	int buttonPress;
	struct timeval lastChange;
	struct timeval lastButton;
	trainCtrlDef *activeTrain;

#ifdef __GTK_H__
	GtkWidget *trainSelect;
#else
	void *xPointers;
#endif
}
throttleDef;

typedef struct _trackCtrl
{
	int powerState;
	int trainCount;
	int pServerCount;
	int throttleCount;
	int serverHandle;
	int serverSession;
	int serverPort;
	int pointPort;
	int configPort;
	int ipVersion;
	int conTimeout;
	int connectRunning;
	int jStickNumber;
	int throttlesRunning;
	int shownCurrent;
	int flags;
	int idleOff;
	char server[81];
	char trackName[81];
	char serialDevice[81];
	time_t trackRepaint;
	pthread_t connectHandle;
	pthread_t throttlesHandle;
	pthread_mutex_t throttleMutex;

	char addressBuffer[111];
	char remoteProgMsg[111];
	int remotePowerState;
	int remoteCurrent;
	int connectionStatus[7];

	trainCtrlDef *trainCtrl;
	pointCtrlDef *pointCtrl;
	throttleDef *throttles;
	trackLayoutDef *trackLayout;

#ifdef __GTK_H__
	GtkWidget *windowCtrl;				//  1
	GtkWidget *windowTrack;				//  2
	GtkWidget *windowFunctions;			//  3
	GtkWidget *dialogProgram;			//  4
	GtkWidget *connectionDialog;		//  5
	GtkWidget *labelPower;				//  6
	GtkWidget *buttonPower;				//  7
	GtkWidget *buttonTrack;				//  8
	GtkWidget *buttonConnection;		//  9
	GtkWidget *buttonProgram;			// 10
	GtkWidget *entryProgram;			// 11
	GtkWidget *drawingArea;				// 12
	GtkWidget *statusBar;				// 13
	GtkWidget *funcSpinner;				// 14
	GtkWidget *funcLabel;				// 15
	GtkWidget *buttonStopAll;			// 16
	GtkWidget *connectionLabels[8];		// 22
#else
	void *xPointers[22];
#endif
}
trackCtrlDef;

void updatePointPosn (trackCtrlDef *trackCtrl, int server, int point, int state);
void updateSignalState (trackCtrlDef *trackCtrl, int server, int signal, int state);
int parseMemoryXML (trackCtrlDef *trackCtrl, char *buffer);
int parseTrackXML (trackCtrlDef *trackCtrl, const char *fileName, int level);
int startConnectThread (trackCtrlDef *trackCtrl);
int trainConnectSend (trackCtrlDef *trackCtrl, char *buffer, int len);
int trainSetSpeed (trackCtrlDef *trackCtrl, trainCtrlDef *train, int speed);
int trainToggleFunction (trackCtrlDef *trackCtrl, trainCtrlDef *train, int function, int state);
void trainUpdateFunction (trackCtrlDef *trackCtrl, int trainID, int byteOne, int byteTwo);
void stopConnectThread (trackCtrlDef *trackCtrl);
int startThrottleThread (trackCtrlDef *trackCtrl);

