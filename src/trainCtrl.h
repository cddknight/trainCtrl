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

	int remoteCurSpeed;
	int remoteReverse;

#ifdef __GTK_H__
	GtkWidget *buttonStop;
	GtkWidget *buttonHalt;
	GtkWidget *scaleSpeed;
	GtkWidget *checkDir;
	GtkWidget *labelNum;
#endif
}
trainCtrlDef;

typedef struct _trackCtrl
{
	int powerState;
	int trainCount;
	int serverHandle;
	int serverSession;
	int serverPort;
	int connectRunning;
	int rxedCurrent;
	int showCurrent;
	char server[81];
	char trackName[81];
	char serialDevice[81];
	pthread_t connectHandle;

	char remoteProgMsg[111];
	int remotePowerState;

#ifdef __GTK_H__
	GtkWidget *windowCtrl;
	GtkWidget *windowTrack;
	GtkWidget *dialogProgram;
	GtkWidget *labelPower;
	GtkWidget *buttonPower;
	GtkWidget *buttonTrack;
	GtkWidget *buttonProgram;
	GtkWidget *labelProgram;
	GtkWidget *drawingArea;
	GtkWidget *statusBar;
#endif

	trainCtrlDef *trainCtrl;
	trackLayoutDef *trackLayout;
}
trackCtrlDef;

int parseTrackXML (trackCtrlDef *trackCtrl, char *fileName);
int startConnectThread (trackCtrlDef *trackCtrl);
int trainConnectSend (trackCtrlDef *trackCtrl, char *buffer, int len);
void stopConnectThread (trackCtrlDef *trackCtrl);

