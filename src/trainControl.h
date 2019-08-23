/**********************************************************************************************************************
 *                                                                                                                    *
 *  T R A I N  C T R L . H                                                                                            *
 *  ======================                                                                                            *
 *                                                                                                                    *
 *  This is free software; you can redistribute it and/or modify it under the terms of the GNU General Public         *
 *  License version 2 as published by the Free Software Foundation.  Note that I am not granting permission to        *
 *  redistribute or modify this under the terms of any later version of the General Public License.                   *
 *                                                                                                                    *
 *  This is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied        *
 *  warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more     *
 *  details.                                                                                                          *
 *                                                                                                                    *
 *  You should have received a copy of the GNU General Public License along with this program (in the file            *
 *  "COPYING"); if not, write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111,   *
 *  USA.                                                                                                              *
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

typedef struct _trackCell
{
	unsigned short layout;
	unsigned short point;
	unsigned short link;
	unsigned short pointState;
	unsigned short pointDefault;
	unsigned short server;
	unsigned short ident;
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
	int functions;
	int funcCount;
	int funcCustom;
	char trainDesc[41];

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
	void *xPointers[4];
#endif
}
trainCtrlDef;

typedef struct _pointCtrl
{
	char server[41];
	int port;
	int ident;
	int intHandle;
	int retry;
}
pointCtrlDef;

typedef struct _trackCtrl
{
	int powerState;
	int trainCount;
	int pServerCount;
	int serverHandle;
	int serverSession;
	int serverPort;
	int pointPort;
	int configPort;
	int ipVersion;
	int conTimeout;
	int connectRunning;
	int shownCurrent;
	int flags;
	char server[81];
	char trackName[81];
	char serialDevice[81];
	pthread_t connectHandle;

	char remoteProgMsg[111];
	int remotePowerState;
	int remoteCurrent;
	int connectionStatus[7];

	trainCtrlDef *trainCtrl;
	pointCtrlDef *pointCtrl;
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
	GtkWidget *labelProgram;			// 11
	GtkWidget *drawingArea;				// 12
	GtkWidget *statusBar;				// 13
	GtkWidget *funcSpinner;				// 14
	GtkWidget *funcLabel;				// 15
	GtkWidget *buttonStopAll;			// 16
	GtkWidget *connectionLabels[7];		// 22
#else
	void *xPointers[22];
#endif
}
trackCtrlDef;

void updatePointPosn (trackCtrlDef *trackCtrl, int server, int point, int state);
int parseMemoryXML (trackCtrlDef *trackCtrl, char *buffer);
int parseTrackXML (trackCtrlDef *trackCtrl, const char *fileName, int level);
int startConnectThread (trackCtrlDef *trackCtrl);
int trainConnectSend (trackCtrlDef *trackCtrl, char *buffer, int len);
int trainSetSpeed (trackCtrlDef *trackCtrl, trainCtrlDef *train, int speed);
int trainToggleFunction (trackCtrlDef *trackCtrl, trainCtrlDef *train, int function);
void trainUpdateFunction (trackCtrlDef *trackCtrl, int trainID, int byteOne, int byteTwo);
void stopConnectThread (trackCtrlDef *trackCtrl);

