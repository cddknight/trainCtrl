/**********************************************************************************************************************
 *                                                                                                                    *
 *  P O I N T  C O N T R O L . H                                                                                      *
 *  ============================                                                                                      *
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
 *  \brief Control the points taking commands from the network.
 */
typedef struct _pointState
{
	int ident;
	int state;
	int defaultPos;
	int turnoutPos;
	int servoChannel;
	servoStateDef servoState;
}
pointStateDef;

typedef struct _signalState
{
	int ident;
	int state;
	int type;
	int channelRed;
	int channelGreen;
	int redOut;
	int greenOut;
	int servoChannel;
	servoStateDef servoState;
}
signalStateDef;

typedef struct _pointCtrl
{
	int clientID;
	int serverPort;
	int conTimeout;
	int ipVersion;
	int pointCount;
	int signalCount;
	char clientName[41];
	char serverName[81];
	pointStateDef *pointStates;
	signalStateDef *signalStates;
}
pointCtrlDef;

int parseMemoryXML (pointCtrlDef *pointCtrl, char *buffer);
void checkRecvBuffer (pointCtrlDef *pointCtrl, int handle, char *buffer, int len);
void checkPointsOff (pointCtrlDef *pointCtrl, int handle);
void putLogMessage (int priority, const char *fmt, ...);
int pointControlSetup (pointCtrlDef *pointCtrl);

