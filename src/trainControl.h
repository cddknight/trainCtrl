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

#define POWER_SRT	-1
#define POWER_OFF	0
#define POWER_ON	1

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

typedef struct _trainCtrl
{
	int trainReg;
	int trainID;
	int trainNum;
	int curSpeed;
	int reverse;
	char trainDesc[41];

	struct timeval lastChange;
	int remoteCurSpeed;
	int remoteReverse;

#ifdef __GTK_H__
	GtkWidget *buttonNum;
	GtkWidget *buttonStop;
	GtkWidget *buttonHalt;
	GtkWidget *scaleSpeed;
	GtkWidget *checkDir;
#else
	void *xPointers[5];
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
	int configPort;
	int connectRunning;
	int shownCurrent;
	char server[81];
	char trackName[81];
	char serialDevice[81];
	pthread_t connectHandle;

	char remoteProgMsg[111];
	int remotePowerState;
	int remoteCurrent;
	int serverStatus[6];

	trainCtrlDef *trainCtrl;
	pointCtrlDef *pointCtrl;
	trackLayoutDef *trackLayout;

#ifdef __GTK_H__
	GtkWidget *windowCtrl;				//  1
	GtkWidget *windowTrack;				//  2
	GtkWidget *dialogProgram;			//  3
	GtkWidget *dialogStatus;			//  4
	GtkWidget *labelPower;				//  5
	GtkWidget *buttonPower;				//  6
	GtkWidget *buttonTrack;				//  7
	GtkWidget *buttonStatus;			//  8
	GtkWidget *buttonProgram;			//  9
	GtkWidget *labelProgram;			// 10
	GtkWidget *drawingArea;				// 11
	GtkWidget *statusBar;				// 12
	GtkWidget *statusLabels[6];			// 18
#else
	void *xPointers[18];
#endif
}
trackCtrlDef;

void updatePointPosn (trackCtrlDef *trackCtrl, int server, int point, int state);
int parseMemoryXML (trackCtrlDef *trackCtrl, char *buffer);
int parseTrackXML (trackCtrlDef *trackCtrl, const char *fileName, int level);
int startConnectThread (trackCtrlDef *trackCtrl);
int trainConnectSend (trackCtrlDef *trackCtrl, char *buffer, int len);
int trainSetSpeed (trackCtrlDef *trackCtrl, trainCtrlDef *train, int speed);
void stopConnectThread (trackCtrlDef *trackCtrl);

