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

typedef struct _trackCell
{
	unsigned short layout;
	unsigned short point;
	unsigned short state;
}
trackCellDef;

typedef struct _trackLayout
{
	unsigned int trackRows;
	unsigned int trackCols;
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
	
	GtkWidget *buttonStop;
	GtkWidget *buttonHalt;
	GtkWidget *scaleSpeed;
	GtkWidget *checkDir;
	GtkWidget *labelNum;
}
trainCtrlDef;

typedef struct _trackCtrl
{
	int powerState;
	int trainCount;
	int serverHandle;
	int serverPort;
	char server[81];

	GtkWidget *windowCtrl;
	GtkWidget *windowTrack;
	GtkWidget *buttonPower;
	GtkWidget *buttonTrack;
	GtkWidget *statusBar;
	GtkWidget *drawingArea;
	trainCtrlDef *trainCtrl;
	trackLayoutDef *trackLayout;
}
trackCtrlDef;

int parseTrackXML (char *fileName);

