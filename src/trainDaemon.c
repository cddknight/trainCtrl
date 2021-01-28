/**********************************************************************************************************************
 *                                                                                                                    *
 *  T R A I N  D A E M O N . C                                                                                        *
 *  ==========================                                                                                        *
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
 *  \brief Talk over the net to DCC++.
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <syslog.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <termios.h>
#include <time.h>

#include "socketC.h"
#include "trainControl.h"
#include "config.h"
#include "buildDate.h"

#define RXED_BUFF_SIZE	1024
#define MAX_HANDLES		25
#define SERIAL_HANDLE	0
#define LISTEN_HANDLE	1
#define POINTL_HANDLE	2
#define CONFIG_HANDLE	3
#define FIRST_HANDLE	4

#define SERIAL_HTYPE	1
#define LISTEN_HTYPE	2
#define POINTL_HTYPE	3
#define CONFIG_HTYPE	4
#define POINTC_HTYPE	5
#define CONTRL_HTYPE	6

char *xmlBuffer;
long xmlBufferSize;
char xmlConfigFile[81]	=	"/etc/train/track.xml";
char pidFileName[81]	=	"/run/trainDaemon.pid";
int	 logOutput			=	0;
int	 infoOutput			=	0;
int	 debugOutput		=	0;
int	 goDaemon			=	0;
int	 inDaemonise		=	0;
int	 running			=	1;
trackCtrlDef trackCtrl;

typedef struct _handleInfo
{
	int handle;
	int handleType;
	int rxedPosn;
	char localName[81];
	char remoteName[81];
	char rxedBuff[RXED_BUFF_SIZE + 1];
}
HANDLEINFO;

HANDLEINFO handleInfo[MAX_HANDLES];

/**********************************************************************************************************************
 *                                                                                                                    *
 *  P U T  L O G  M E S S A G E                                                                                       *
 *  ===========================                                                                                       *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  \brief Put a message in the log files.
 *  \param priority Which log file to add it to.
 *  \param fmt Format of the message.
 *  \param ... More arguments.
 *  \result None.
 */
void putLogMessage (int priority, const char *fmt, ...)
{
	int doSysLog = 0, doConLog = 0;
	if (logOutput)
	{
		if (debugOutput)
			doSysLog = 1;
		else if (infoOutput && priority == LOG_INFO)
			doSysLog = 1;
		else if (priority == LOG_ERR)
			doSysLog = 1;
	}
	if (inDaemonise != 1)
	{
		if (debugOutput)
			doConLog = 1;
		else if (infoOutput && priority == LOG_INFO)
			doConLog = 1;
		else if (priority == LOG_ERR)
			doConLog = 1;
	}
	if (doSysLog || doConLog)
	{
		char tempBuffer[(8 * 1024) + 1];
		va_list arg_ptr;

		va_start (arg_ptr, fmt);
		vsnprintf (tempBuffer, (8 * 1024), fmt, arg_ptr);
		va_end (arg_ptr);
		if (doSysLog) syslog (priority, "%s", tempBuffer);
		if (doConLog) printf ("%s\n", tempBuffer);
	}
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  S I G  H A N D L E R                                                                                              *
 *  ====================                                                                                              *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  \brief Function called when control-C is pressed.
 *  \param signo What singnal happened.
 *  \result None.
 */
void sigHandler (int signo)
{
	switch(signo)
	{
	case SIGINT:
		running = 0;
		break;
	case SIGHUP:
		putLogMessage (LOG_INFO, "Hangup signal received");
		break;
	case SIGTERM:
		putLogMessage (LOG_INFO, "Terminate signal received");
		exit(0);
		break;
	}
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  D A E M O N I Z E                                                                                                 *
 *  =================                                                                                                 *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  \brief Convert the running program into a daemon.
 *  \result None.
 */
void daemonize(void)
{
	int i,lfp;
	char str[10];

	if (getppid() == 1)
		return; /* already a daemon */

	i = fork();
	if (i < 0)
		exit(1); /* fork error */
	if (i > 0)
		exit(0);			/* parent exits */

	/* child (daemon) continues, obtain a new process group */
	setsid();

	/* close all descriptors */
	for (i = getdtablesize(); i >= 0; --i)
		close(i);

	/* handle standard I/O */
	i = open("/dev/null",O_RDWR);
	if (dup(i) == -1) exit (1);
	if (dup(i) == -1) exit (1);

	/* set newly created file permissions */
	umask(027);

	/* Create PID file */
	if ((lfp = open(pidFileName, O_RDWR|O_CREAT, 0640)) < 0)
	{
		putLogMessage (LOG_ERR, "Cannot open PID file: %s", pidFileName);
		exit(1); /* can not open */
	}
	if (lockf(lfp,F_TLOCK,0) < 0)
	{
		putLogMessage (LOG_ERR, "Cannot lock PID file");
		exit(0); /* can not lock */
	}

	/* first instance continues */
	sprintf(str, "%d\n", getpid());
	if (write(lfp, str, strlen(str)) != strlen(str))	/* record pid to lockfile */
	{
		putLogMessage (LOG_ERR, "Cannot write to PID file");
		exit(0); /* can not lock */
	}
	signal(SIGCHLD, SIG_IGN);		/* ignore child */
	signal(SIGTSTP, SIG_IGN);		/* ignore tty signals */
	signal(SIGTTOU, SIG_IGN);
	signal(SIGTTIN, SIG_IGN);
	signal(SIGINT, sigHandler);		/* catch interupt signal */
	signal(SIGHUP, sigHandler);		/* catch hangup signal */
	signal(SIGTERM, sigHandler);	/* catch kill signal */
	inDaemonise = 1;
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  S E R I A L  P O R T  S E T U P                                                                                   *
 *  ===============================                                                                                   *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  \brief Open the serial port and set it to 115200 bps.
 *  \param device Serial port to open.
 *  \result Handle of the port.
 */
int serialPortSetup (char *device)
{
	int portFD = -1;
	struct termios options;

	portFD = open (device, O_RDWR | O_NOCTTY | O_NDELAY);
	if (portFD == -1)
	{
		putLogMessage (LOG_INFO, "Open serial port failed.");
		return -1;
	}
	memset (&options, 0, sizeof (struct termios));
	cfmakeraw (&options);
	cfsetspeed(&options, B115200);
	tcsetattr(portFD, TCSANOW, &options);
	return portFD;
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  S E N D  S E R I A L                                                                                              *
 *  ====================                                                                                              *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  \brief Send data to the serial port.
 *  \param buffer Data to send.
 *  \param len Size to send.
 *  \result The number of bytes sent.
 */
int sendSerial (char *buffer, int len)
{
	putLogMessage (LOG_DEBUG, "Sending -> Serial: %s[%d]", buffer, len);
	return write (handleInfo[SERIAL_HANDLE].handle, buffer, len);
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  S T O P  A L L  T R A I N S                                                                                       *
 *  ===========================                                                                                       *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  \brief Stop all the trains, set throttles to -1, hard stop.
 *  \result None.
 */
void stopAllTrains ()
{
	int t;
	char tempBuff[81];

	if (trackCtrl.trainCtrl != NULL)
	{
		for (t = 0; t < trackCtrl.trainCount; ++t)
		{
			trainCtrlDef *train = &trackCtrl.trainCtrl[t];
			sprintf (tempBuff, "<t %d %d %d %d>", train -> trainReg, train -> trainID, -1, 0);
			sendSerial (tempBuff, strlen (tempBuff));
		}
		usleep (100000);
	}
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  T R A I N  U P D  F U N C T I O N                                                                                 *
 *  =================================                                                                                 *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  \brief Update the function value.
 *  \param trainID Train to update.
 *  \param byteOne First byte.
 *  \param byteTwo Second byte (dependant on first).
 *  \result None.
 */
void trainUpdFunction (int trainID, int byteOne, int byteTwo)
{
	int t;
	for (t = 0; t < trackCtrl.trainCount; ++t)
	{
		if (trackCtrl.trainCtrl[t].trainID == trainID)
		{
			if ((byteOne & 0xE0) == 128)
			{
				/* Function 0 is on bit 4 and function 1 is on bit 0. */
				trackCtrl.trainCtrl[t].functions &= 0xFFFFFFE0;
				trackCtrl.trainCtrl[t].functions |= ((byteOne & 0x0F) << 1);
				trackCtrl.trainCtrl[t].functions |= ((byteOne >> 4) & 0x01);
			}
			else if ((byteOne & 0xF0) == 176)
			{
				trackCtrl.trainCtrl[t].functions &= 0xFFFFFE1F;
				trackCtrl.trainCtrl[t].functions |= ((byteOne & 0x0F) << 5);
			}
			else if ((byteOne & 0xF0) == 160)
			{
				trackCtrl.trainCtrl[t].functions &= 0xFFFFE1FF;
				trackCtrl.trainCtrl[t].functions |= ((byteOne & 0x0F) << 9);
			}
			else if (byteOne == 222)
			{
				trackCtrl.trainCtrl[t].functions &= 0xFFE01FFF;
				trackCtrl.trainCtrl[t].functions |= ((byteTwo & 0xFF) << 13);
			}
			else if (byteOne == 223)
			{
				trackCtrl.trainCtrl[t].functions &= 0xE01FFFFF;
				trackCtrl.trainCtrl[t].functions |= ((byteTwo & 0xFF) << 21);
			}
			break;
		}
	}
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  G E T  A L L  P O I N T  S T A T E S                                                                              *
 *  ====================================                                                                              *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  \brief Tell all the point controllers to output their state.
 *  \result None.
 */
void getAllPointStates ()
{
	int p;

	if (trackCtrl.pointCtrl != NULL)
	{
		for (p = 0; p < trackCtrl.pServerCount; ++p)
		{
			pointCtrlDef *point = &trackCtrl.pointCtrl[p];
			if (point -> intHandle != -1)
			{
				if (handleInfo[point -> intHandle].handle != -1)
				{
					SendSocket (handleInfo[point -> intHandle].handle, "<Y>", 3);
					SendSocket (handleInfo[point -> intHandle].handle, "<X>", 3);
				}
			}
		}
	}
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  S E T  A L L  P O I N T  S T A T E S                                                                              *
 *  ====================================                                                                              *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  \brief Send the current point states to the point server.
 *  \param pSvrIdent Server to sent to.
 *  \result None.
 */
void setAllPointStates (int pSvrIdent)
{
	int p;
	putLogMessage (LOG_DEBUG, "setAllPointStates: %d", pSvrIdent);
	if (trackCtrl.pointCtrl != NULL)
	{
		pointCtrlDef *pointSever = NULL;

		for (p = 0; p < trackCtrl.pServerCount; ++p)
		{
			if (trackCtrl.pointCtrl[p].ident == pSvrIdent)
			{
				pointSever = &trackCtrl.pointCtrl[p];
				break;
			}
		}
		if (pointSever)
		{
			if (pointSever -> intHandle != -1)
			{
				char tempBuff[80];
				int i, cells = trackCtrl.trackLayout -> trackRows * trackCtrl.trackLayout -> trackCols;

				for (i = 0; i < cells; ++i)
				{
					trackCellDef *cell = &trackCtrl.trackLayout -> trackCells[i];
					if (cell -> point.point)
					{
						if (cell -> point.server == pSvrIdent)
						{
							sprintf (tempBuff, "<Y %d %d %d>", pSvrIdent, cell -> point.ident,
									cell -> point.state == cell -> point.pointDef ? 0 : 1);
							SendSocket (handleInfo[pointSever -> intHandle].handle, tempBuff, strlen (tempBuff));
						}
					}
					if (cell -> signal.signal)
					{
						if (cell -> signal.server == pSvrIdent)
						{
							sprintf (tempBuff, "<X %d %d %d>", pSvrIdent, cell -> signal.ident, 
								cell -> signal.state == 2 ? 2 : 1);
							SendSocket (handleInfo[pointSever -> intHandle].handle, tempBuff, strlen (tempBuff));
						}

					}
				}
			}
		}
	}
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  S A V E  P O I N T  S T A T E                                                                                     *
 *  =============================                                                                                     *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  \brief Save the current state of the point as it is changed.
 *  \param pSvrIdent Server number.
 *  \param ident Point number.
 *  \param direc Point direction.
 *  \result None.
 */
void savePointState (int pSvrIdent, int ident, int direc)
{
	int i, cells = trackCtrl.trackLayout -> trackRows * trackCtrl.trackLayout -> trackCols;

	for (i = 0; i < cells; ++i)
	{
		trackCellDef *cell = &trackCtrl.trackLayout -> trackCells[i];
		if (cell -> point.point)
		{
			if (cell -> point.server == pSvrIdent && cell -> point.ident == ident)
			{
				if (direc == 0)
					cell -> point.state = cell -> point.pointDef;
				else
					cell -> point.state = cell -> point.point & ~(cell -> point.pointDef);

				break;
			}
		}
	}
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  S A V E  S I G N A L  S T A T E                                                                                   *
 *  ===============================                                                                                   *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  \brief Save the state of the signal.
 *  \param sSvrIdent Identity of the server.
 *  \param ident Identity of the signal.
 *  \param state New state of the signal.
 *  \result None.
 */
void saveSignalState (int sSvrIdent, int ident, int state)
{
	int i, cells = trackCtrl.trackLayout -> trackRows * trackCtrl.trackLayout -> trackCols;

	for (i = 0; i < cells; ++i)
	{
		trackCellDef *cell = &trackCtrl.trackLayout -> trackCells[i];
		if (cell -> signal.server == sSvrIdent && cell -> signal.ident == ident)
		{
			cell -> signal.state = state;
			break;
		}
	}
}


/**********************************************************************************************************************
 *                                                                                                                    *
 *  S E N D  P O I N T  S E R V E R                                                                                   *
 *  ===============================                                                                                   *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  \brief Send a message to the point server.
 *  \param pSvrIdent Point server identity.
 *  \param ident Identity of the point server.
 *  \param direc Direction for the point.
 *  \result None.
 */
void sendPointServer (int pSvrIdent, int ident, int direc)
{
	int p;

	if (trackCtrl.pointCtrl != NULL)
	{
		for (p = 0; p < trackCtrl.pServerCount; ++p)
		{
			pointCtrlDef *pointCtrl = &trackCtrl.pointCtrl[p];
			if (pSvrIdent == pointCtrl -> ident)
			{
				if (pointCtrl -> intHandle != -1)
				{
					if (handleInfo[pointCtrl -> intHandle].handle != -1)
					{
						char tempBuff[81];
						sprintf (tempBuff, "<Y %d %d %d>", pSvrIdent, ident, direc);
						SendSocket (handleInfo[pointCtrl -> intHandle].handle,
								tempBuff, strlen (tempBuff));
						savePointState (pSvrIdent, ident, direc);
					}
				}
				break;
			}
		}
	}
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  S E N D  S I G N A L  S E R V E R                                                                                 *
 *  =================================                                                                                 *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  \brief Send the state of a signal.
 *  \param sSvrIdent Identity of the server.
 *  \param ident Identity of the signal.
 *  \param state State of the signal.
 *  \result None.
 */
void sendSignalServer (int sSvrIdent, int ident, int state)
{
	int p;

	if (trackCtrl.pointCtrl != NULL)
	{
		for (p = 0; p < trackCtrl.pServerCount; ++p)
		{
			pointCtrlDef *pointCtrl = &trackCtrl.pointCtrl[p];
			if (sSvrIdent == pointCtrl -> ident)
			{
				if (pointCtrl -> intHandle != -1)
				{
					if (handleInfo[pointCtrl -> intHandle].handle != -1)
					{
						char tempBuff[81];
						sprintf (tempBuff, "<X %d %d %d>", sSvrIdent, ident, state);
						SendSocket (handleInfo[pointCtrl -> intHandle].handle,
								tempBuff, strlen (tempBuff));
						saveSignalState (sSvrIdent, ident, state);
					}
				}
				break;
			}
		}
	}
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  C H E C K  S E R I A L  R E C V  B U F F E R                                                                      *
 *  ============================================                                                                      *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  \brief Chech what we have received on the socket to see if we should update the display.
 *  \param buffer Buffer that was received.
 *  \param len Length of the buffer.
 *  \result None.
 */
void checkSerialRecvBuffer (char *buffer, int len)
{
	char words[41][41];
	int wordNum = -1, i = 0, j = 0, inType = 0;

/*------------------------------------------------------------------*
	printf ("Serial Rxed:[%s]\n", buffer);
 *------------------------------------------------------------------*/
	while (i < len)
	{
		if (buffer[i] == '<' && wordNum == -1)
		{
			words[wordNum = 0][0] = 0;
		}
		else if (wordNum >= 0 && ((buffer[i] >= 'a' && buffer[i] <= 'z') || (buffer[i] >= 'A' && buffer[i] <= 'Z')))
		{
			if (inType == 2 && j > 0)
			{
				words[++wordNum][0] = 0;
				j = 0;
			}
			words[wordNum][j++] = buffer[i];
			words[wordNum][j] = 0;
			inType = 1;
		}
		else if (wordNum >= 0 && ((buffer[i] >= '0' && buffer[i] <= '9') || buffer[i] == '-' || buffer[i] == '.'))
		{
			if (inType == 1 && j > 0)
			{
				words[++wordNum][0] = 0;
				j = 0;
			}
			words[wordNum][j++] = buffer[i];
			words[wordNum][j] = 0;
			inType = 2;
		}
		else if (wordNum >= 0 && buffer[i] == '>')
		{
			if (j)
				words[++wordNum][0] = 0;

			/* Track power status - power off stop trains */
			if (words[0][0] == 'p' && words[0][1] == 0 && wordNum == 2)
			{
				int power = atoi(words[1]);
				if ((trackCtrl.powerState = power) == 0)
					stopAllTrains ();
			}
			/* Throttle status */
			else if (words[0][0] == 'T' && words[0][1] == 0 && wordNum == 4)
			{
				int trainReg = atoi(words[1]), t;
				for (t = 0; t < trackCtrl.trainCount; ++t)
				{
					if (trackCtrl.trainCtrl != NULL)
					{
						if (trackCtrl.trainCtrl[t].trainReg == trainReg)
						{
							trackCtrl.trainCtrl[t].curSpeed = atoi(words[2]);
							trackCtrl.trainCtrl[t].reverse = atoi(words[3]);
						}
					}
				}
			}
			inType = 0;
			wordNum = -1;
			j = 0;
		}
		else if (wordNum >= 0 && j > 0 && (buffer[i] == ' ' || buffer[i] == '|'))
		{
			words[++wordNum][0] = 0;
			j = 0;
		}
		if (wordNum > 40)
		{
			words[wordNum = 40][0] = 0;
			j = 0;
		}
		if (j > 40)
			j = 40;

		++i;
	}
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  C H E C K  N E T W O R K  R E C V  B U F F E R                                                                    *
 *  ==============================================                                                                    *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  \brief Check the buffer received from the network.
 *  \param handle Handle to send reply to.
 *  \param buffer Received buffer.
 *  \param len Size of data in the buffer.
 *  \result 1 if data was processed locally, 0 if it should be sent to serial.
 */
int checkNetworkRecvBuffer (int handle, char *buffer, int len)
{
	char words[41][41];
	int retn = 0, wordNum = -1, i = 0, j = 0, inType = 0;

/*------------------------------------------------------------------*
	putLogMessage (LOG_INFO, "Network Rxed:[%s]", buffer);
 *------------------------------------------------------------------*/
	while (i < len)
	{
		if (buffer[i] == '<' && wordNum == -1)
		{
			words[wordNum = 0][0] = 0;
		}
		else if (wordNum >= 0 && ((buffer[i] >= 'a' && buffer[i] <= 'z') || (buffer[i] >= 'A' && buffer[i] <= 'Z')))
		{
			if (inType == 2 && j > 0)
			{
				words[++wordNum][0] = 0;
				j = 0;
			}
			words[wordNum][j++] = buffer[i];
			words[wordNum][j] = 0;
			inType = 1;
		}
		else if (wordNum >= 0 && ((buffer[i] >= '0' && buffer[i] <= '9') || buffer[i] == '-' || buffer[i] == '.'))
		{
			if (inType == 1 && j > 0)
			{
				words[++wordNum][0] = 0;
				j = 0;
			}
			words[wordNum][j++] = buffer[i];
			words[wordNum][j] = 0;
			inType = 2;
		}
		else if (wordNum >= 0 && buffer[i] == '>')
		{
			if (j)
				words[++wordNum][0] = 0;

			/* Set point state */
			if (words[0][0] == 'Y' && words[0][1] == 0 && wordNum == 4)
			{
				int server = atoi (words[1]);
				int ident = atoi (words[2]);
				int direc = atoi (words[3]);
				sendPointServer (server, ident, direc);
				retn = 1;
			}
			/* Reply point server state */
			else if (words[0][0] == 'y' && words[0][1] == 0 && wordNum == 4)
			{
				int h;
				for (h = FIRST_HANDLE; h < MAX_HANDLES; ++h)
				{
					if (handleInfo[h].handle != -1 && handleInfo[h].handleType == CONTRL_HTYPE)
						SendSocket (handleInfo[h].handle, buffer, len);
				}
				retn = 1;
			}
			/* Set signal state */
			else if (words[0][0] == 'X' && words[0][1] == 0 && wordNum == 4)
			{
				int server = atoi (words[1]);
				int ident = atoi (words[2]);
				int state = atoi (words[3]);
				sendSignalServer (server, ident, state);
				retn = 1;
			}
			/* Reply signal server state */
			else if (words[0][0] == 'x' && words[0][1] == 0 && wordNum == 4)
			{
				int h;
				for (h = FIRST_HANDLE; h < MAX_HANDLES; ++h)
				{
					if (handleInfo[h].handle != -1 && handleInfo[h].handleType == CONTRL_HTYPE)
						SendSocket (handleInfo[h].handle, buffer, len);
				}
				retn = 1;
			}
			/* Record and tell everyone about a function change */
			else if (words[0][0] == 'f' && words[0][1] == 0 && (wordNum == 3 || wordNum == 4))
			{
				int h;
				int trainID = atoi (words[1]);
				int byteOne = atoi (words[2]);
				int byteTwo = 0;

				if (wordNum == 4)
					byteTwo = atoi (words[3]);

				trainUpdFunction (trainID, byteOne, byteTwo);
				for (h = FIRST_HANDLE; h < MAX_HANDLES; ++h)
				{
					if (handleInfo[h].handle != -1 && handleInfo[h].handleType == CONTRL_HTYPE)
						SendSocket (handleInfo[h].handle, buffer, len);
				}
				retn = 0;
			}
			/* Get socket status */
			else if (words[0][0] == 'V' && words[0][1] == 0 && wordNum == 1)
			{
				char buffer[101];
				int h, conCounts[6] = { 0, 0, 0, 0, 0, 0 };

				for (h = SERIAL_HANDLE; h < MAX_HANDLES; ++h)
				{
					if (handleInfo[h].handle != -1)
					{
						if (handleInfo[h].handleType >= SERIAL_HTYPE && handleInfo[h].handleType <= CONTRL_HTYPE)
							++conCounts[handleInfo[h].handleType - 1];
					}
				}
				sprintf (buffer, "<V %d %d %d %d %d %d %d>", handleInfo[handle].handle,
						conCounts[0], conCounts[1], conCounts[2], conCounts[3], conCounts[4], conCounts[5]);
				putLogMessage (LOG_INFO, "Status: %s", buffer);
				SendSocket (handleInfo[handle].handle, buffer, strlen (buffer));
				retn = 1;
			}
			else if (words[0][0] == 'P' && words[0][1] == 0 && wordNum == 2)
			{
				if (handleInfo[handle].handleType == POINTC_HTYPE)
				{
					int p;
					for (p = 0; p < trackCtrl.pServerCount; ++p)
					{
						pointCtrlDef *point = &trackCtrl.pointCtrl[p];
						if (point -> intHandle == handle)
						{
							point -> ident = atoi (words[1]);
							setAllPointStates (point -> ident);
							break;
						}
					}
				}
				retn = 1;
			}
			inType = 0;
			wordNum = -1;
			j = 0;
		}
		else if (wordNum >= 0 && j > 0 && (buffer[i] == ' ' || buffer[i] == '|'))
		{
			words[++wordNum][0] = 0;
			j = 0;
		}
		if (wordNum > 40)
		{
			words[wordNum = 40][0] = 0;
			j = 0;
		}
		if (j > 40)
			j = 40;

		++i;
	}
	return retn;
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  R E C E I V E  S E R I A L                                                                                        *
 *  ==========================                                                                                        *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  \brief Keep buffering until we get a closing '>' and the send to all.
 *  \param handle Internal handle of the serial port.
 *  \param buffer Received buffer.
 *  \param len Buffer length.
 *  \result None.
 */
void receiveSerial (int handle, char *buffer, int len)
{
	int h, j = 0;

	while (j < len)
	{
		handleInfo[handle].rxedBuff[handleInfo[handle].rxedPosn++] = buffer[j];
		if (buffer[j] == '>')
		{
			handleInfo[handle].rxedBuff[handleInfo[handle].rxedPosn] = 0;
			checkSerialRecvBuffer (handleInfo[handle].rxedBuff, handleInfo[handle].rxedPosn);
			for (h = FIRST_HANDLE; h < MAX_HANDLES; ++h)
			{
				if (handleInfo[h].handle != -1 && handleInfo[h].handleType == CONTRL_HTYPE)
					SendSocket (handleInfo[h].handle, handleInfo[handle].rxedBuff, handleInfo[handle].rxedPosn);
			}
			handleInfo[handle].rxedPosn = 0;
		}
		if (handleInfo[handle].rxedPosn >= RXED_BUFF_SIZE)
		{
			handleInfo[handle].rxedPosn = 0;
			break;
		}
		++j;
	}
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  R E C E I V E  N E T W O R K                                                                                      *
 *  ============================                                                                                      *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  \brief Process blocks read from the network port.
 *  \param handle Internal handle that the data was received on.
 *  \param buffer Data that was received.
 *  \param len Size of data that was received.
 *  \result None.
 */
void receiveNetwork (int handle, char *buffer, int len)
{
	int j = 0;

	while (j < len)
	{
		handleInfo[handle].rxedBuff[handleInfo[handle].rxedPosn++] = buffer[j];
		if (buffer[j] == '>')
		{
			handleInfo[handle].rxedBuff[handleInfo[handle].rxedPosn] = 0;
			if (!checkNetworkRecvBuffer (handle, handleInfo[handle].rxedBuff, handleInfo[handle].rxedPosn))
				sendSerial (handleInfo[handle].rxedBuff, len);

			handleInfo[handle].rxedPosn = 0;
		}
		if (handleInfo[handle].rxedPosn >= RXED_BUFF_SIZE)
		{
			handleInfo[handle].rxedPosn = 0;
			break;
		}
		++j;
	}
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  L O A D  C O N F I G  F I L E                                                                                     *
 *  =============================                                                                                     *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  \brief Read the config file from disk and keep it in memory.
 *  \result True if it was read and parsed.
 */
int loadConfigFile ()
{
	int retn = 0;
	struct stat statbuf;

	if (stat (xmlConfigFile, &statbuf) == 0)
	{
		FILE *inFile = fopen (xmlConfigFile, "r");
		if (inFile != NULL)
		{
			xmlBufferSize = statbuf.st_size;
			if ((xmlBuffer = (char *)malloc (xmlBufferSize + 10)) != NULL)
			{
				if (fread (xmlBuffer, 1, xmlBufferSize, inFile) == xmlBufferSize)
					retn = parseMemoryXML (&trackCtrl, xmlBuffer);
			}
			fclose (inFile);
		}
	}
	return retn;
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  S E N D  C O N F I G  F I L E                                                                                     *
 *  =============================                                                                                     *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  \brief Send out the current config file".
 *  \param newSocket Socket to send to.
 *  \result None.
 */
void sendConfigFile (int newSocket)
{
	if (xmlBufferSize && xmlBuffer != NULL)
		SendSocket (newSocket, xmlBuffer, xmlBufferSize);
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  S E N D  A L L  F U N C T I O N S                                                                                 *
 *  =================================                                                                                 *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  \brief Send out the state of the functions.
 *  \param handle Where to send it, connecting client.
 *  \result None.
 */
void sendAllFunctions (int handle)
{
	int t;
	char tempBuff[81];
	if (trackCtrl.trainCtrl != NULL)
	{
		for (t = 0; t < trackCtrl.trainCount; ++t)
		{
			trainCtrlDef *train = &trackCtrl.trainCtrl[t];
			sprintf (tempBuff, "<F %d %d>", train -> trainID, train -> functions);
			SendSocket (handle, tempBuff, strlen (tempBuff));
		}
	}
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  H E L P  T H E M                                                                                                  *
 *  ================                                                                                                  *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  \brief Display command line help information.
 *  \result None (program exited).
 */
void helpThem()
{
	fprintf (stderr, "Train Daemon, Version: %s (%s)\n", PACKAGE_VERSION, buildDate);
	fprintf (stderr, "Usage: trainDaemon [-c config]\n");
	fprintf (stderr, "       -c config.xml . . Name of the config file\n");
	fprintf (stderr, "       -d  . . . . . . . Deamonise the process.\n");
	fprintf (stderr, "       -L  . . . . . . . Write messages to syslog.\n");
	fprintf (stderr, "       -I  . . . . . . . Write info messages.\n");
	fprintf (stderr, "       -D  . . . . . . . Write debug messages.\n");
	exit (1);
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  M A I N                                                                                                           *
 *  =======                                                                                                           *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  \brief The program starts here.
 *  \param argc The number of arguments passed to the program.
 *  \param argv Pointers to the arguments passed to the program.
 *  \result 0 (zero) if all processed OK.
 */
int main (int argc, char *argv[])
{
	fd_set readfds;
	struct timeval timeout;
	char inAddress[50] = "";
	int i, c, p, connectedCount = 0;
	time_t curRead = time (NULL) + 5;
	time_t lastRxed = time (NULL);

	while ((c = getopt(argc, argv, "c:dLID?")) != -1)
	{
		switch (c)
		{
		case 'c':
			strncpy (xmlConfigFile, optarg, 80);
			break;

		case 'd':
			goDaemon = 1;
			break;

		case 'D':
			debugOutput = 1;
			break;

		case 'I':
			infoOutput = 1;
			break;

		case 'L':
			logOutput = 1;
			break;

		case '?':
			helpThem();
			break;
		}
	}

	/**********************************************************************************************************************
	 * Allocate and read in the configuration.                                                                            *
	 **********************************************************************************************************************/
	if (!loadConfigFile())
		parseMemoryXML (&trackCtrl, NULL);

	for (i = 0; i < MAX_HANDLES; ++i)
		handleInfo[i].handle = -1;

	/**********************************************************************************************************************
	 * Daemonize if needed, all port will close.                                                                          *
	 **********************************************************************************************************************/
	if (goDaemon)
		daemonize();

	/**********************************************************************************************************************
	 * Setup listening serial and network ports.                                                                          *
	 **********************************************************************************************************************/
	handleInfo[SERIAL_HANDLE].handle = serialPortSetup (trackCtrl.serialDevice);
	if (handleInfo[SERIAL_HANDLE].handle == -1)
	{
		putLogMessage (LOG_ERR, "Unable to connect to: %s", trackCtrl.serialDevice);
	}
	else
	{
		handleInfo[SERIAL_HANDLE].handleType = SERIAL_HTYPE;
		putLogMessage (LOG_INFO, "Listening on serial: %s", trackCtrl.serialDevice);

		if (trackCtrl.configPort > 0)
		{
			handleInfo[CONFIG_HANDLE].handle = ServerSocketSetup (trackCtrl.configPort);
			if (handleInfo[CONFIG_HANDLE].handle == -1)
			{
				putLogMessage (LOG_ERR, "Unable to listen on config port.");
			}
			else
			{
				handleInfo[CONFIG_HANDLE].handleType = CONFIG_HTYPE;
				putLogMessage (LOG_INFO, "Listening on port: %d", trackCtrl.configPort);
			}
		}
		if (trackCtrl.pointPort > 0)
		{
			handleInfo[POINTL_HANDLE].handle = ServerSocketSetup (trackCtrl.pointPort);
			if (handleInfo[POINTL_HANDLE].handle == -1)
			{
				putLogMessage (LOG_ERR, "Unable to listen on point port.");
			}
			else
			{
				handleInfo[POINTL_HANDLE].handleType = POINTL_HTYPE;
				putLogMessage (LOG_INFO, "Listening on port: %d", trackCtrl.pointPort);
			}
		}
		handleInfo[LISTEN_HANDLE].handle = ServerSocketSetup (trackCtrl.serverPort);
		if (handleInfo[LISTEN_HANDLE].handle == -1)
		{
			putLogMessage (LOG_ERR, "Unable to listen on network port.");
		}
		else
		{
			handleInfo[LISTEN_HANDLE].handleType = LISTEN_HTYPE;
			putLogMessage (LOG_INFO, "Listening on port: %d", trackCtrl.serverPort);
		}
	}
	for (p = 0; p < trackCtrl.pServerCount; ++p)
	{
		if (trackCtrl.pointCtrl != NULL)
		{
			pointCtrlDef *point = &trackCtrl.pointCtrl[p];
			point -> intHandle = -1;
		}
	}

	/**********************************************************************************************************************
	 * Loop on select, getting and sending work.                                                                          *
	 **********************************************************************************************************************/
	while (handleInfo[LISTEN_HANDLE].handle != -1 && running)
	{
		int selRetn;
		time_t now = 0;
		timeout.tv_sec = 1;
		timeout.tv_usec = 0;

		FD_ZERO(&readfds);
		for (i = 0; i < MAX_HANDLES; ++i)
		{
			if (handleInfo[i].handle != -1)
				FD_SET (handleInfo[i].handle, &readfds);
		}
		selRetn = select(FD_SETSIZE, &readfds, NULL, NULL, &timeout);
		if (selRetn == -1)
		{
			putLogMessage (LOG_ERR, "Select error: %s[%d]", strerror (errno), errno);
			CloseSocket (&handleInfo[0].handle);
		}
		if (selRetn > 0)
		{
			if (FD_ISSET(handleInfo[LISTEN_HANDLE].handle, &readfds))
			{
				int newSocket = ServerSocketAccept (handleInfo[LISTEN_HANDLE].handle, inAddress);
				if (newSocket != -1)
				{
					for (i = FIRST_HANDLE; i < MAX_HANDLES; ++i)
					{
						if (handleInfo[i].handle == -1)
						{
							char outBuffer[41];
							handleInfo[i].handle = newSocket;
							handleInfo[i].handleType = CONTRL_HTYPE;
							strncpy (handleInfo[i].localName, inAddress, 50);
							putLogMessage (LOG_INFO, "Socket opened: %s(%d)", handleInfo[i].localName, handleInfo[i].handle);
							sprintf (outBuffer, "<V %d>", handleInfo[i].handle);
							SendSocket (handleInfo[i].handle, outBuffer, strlen (outBuffer));
							sendSerial ("<s>", 3);
							getAllPointStates ();
							sendAllFunctions (handleInfo[i].handle);
							++connectedCount;
							break;
						}
					}
					if (i == MAX_HANDLES)
					{
						putLogMessage (LOG_ERR, "No free handles.");
						CloseSocket (&newSocket);
					}
				}
			}
			if (FD_ISSET(handleInfo[POINTL_HANDLE].handle, &readfds))
			{
				int newSocket = ServerSocketAccept (handleInfo[POINTL_HANDLE].handle, inAddress);
				if (newSocket != -1)
				{
					int done = 0;
					for (i = FIRST_HANDLE; i < MAX_HANDLES && !done; ++i)
					{
						if (handleInfo[i].handle == -1)
						{
							if (trackCtrl.pointCtrl != NULL)
							{
								for (p = 0; p < trackCtrl.pServerCount && !done; ++p)
								{
									if (trackCtrl.pointCtrl[p].intHandle == -1)
									{
										trackCtrl.pointCtrl[p].intHandle = i;
										handleInfo[i].handle = newSocket;
										handleInfo[i].handleType = POINTC_HTYPE;
										strncpy (handleInfo[i].localName, inAddress, 50);
										putLogMessage (LOG_INFO, "Socket opened: %s(%d)", handleInfo[i].localName, handleInfo[i].handle);
										done = 1;
									}
								}
								if (!done)
									putLogMessage (LOG_INFO, "All point servers are already conneted: %s(%d).", inAddress, newSocket);
							}
							else
							{
								putLogMessage (LOG_INFO, "No point control configured.");
							}
						}
					}
					if (i == MAX_HANDLES)
					{
						putLogMessage (LOG_ERR, "No free handles: %s(%d).", inAddress, newSocket);
					}
					if (!done)
						CloseSocket (&newSocket);
				}
			}
			if (FD_ISSET(handleInfo[CONFIG_HANDLE].handle, &readfds))
			{
				int newSocket = ServerSocketAccept (handleInfo[CONFIG_HANDLE].handle, inAddress);
				if (newSocket != -1)
				{
					sendConfigFile (newSocket);
					CloseSocket (&newSocket);
				}
			}
			if (FD_ISSET(handleInfo[SERIAL_HANDLE].handle, &readfds))
			{
				int readBytes;
				char buffer[10241];
				if ((readBytes = read (handleInfo[SERIAL_HANDLE].handle, buffer, 10240)) > 0)
				{
					buffer[readBytes] = 0;
					putLogMessage (LOG_DEBUG, "Received <- Serial: %s[%d]", buffer, readBytes);
					receiveSerial (SERIAL_HANDLE, buffer, readBytes);
				}
			}
			for (i = FIRST_HANDLE; i < MAX_HANDLES; ++i)
			{
				if (handleInfo[i].handle != -1)
				{
					if (FD_ISSET(handleInfo[i].handle, &readfds))
					{
						int readBytes;
						char buffer[10241];

						if ((readBytes = RecvSocket (handleInfo[i].handle, buffer, 10240)) > 0)
						{
							buffer[readBytes] = 0;
							receiveNetwork (i, buffer, readBytes);
							if (handleInfo[i].handleType == CONTRL_HTYPE)
								lastRxed = time (NULL);
						}
						else if (readBytes == 0)
						{
							putLogMessage (LOG_INFO, "Socket closed: %s(%d)", handleInfo[i].localName, handleInfo[i].handle);
							CloseSocket (&handleInfo[i].handle);
							if (handleInfo[i].handleType == CONTRL_HTYPE)
							{
								if (--connectedCount == 0)
									sendSerial ("<0>", 3);
							}
							else if (handleInfo[i].handleType == POINTC_HTYPE)
							{
								for (p = 0; p < trackCtrl.pServerCount; ++p)
								{
									pointCtrlDef *point = &trackCtrl.pointCtrl[p];
									if (point -> intHandle == i)
									{
										point -> intHandle = -1;
										break;
									}
								}
							}
						}
					}
				}
			}
		}
		now = time (NULL);
		if (curRead < now && trackCtrl.powerState == POWER_ON)
		{
			sendSerial ("<c>", 3);
			curRead = now + 2;
		}
		if (trackCtrl.idleOff > 0)
		{
			if (now - lastRxed > trackCtrl.idleOff)
			{
				putLogMessage (LOG_INFO, "Idle timeout reached turning off power");
				sendSerial ("<0>", 3);
			}
		}
	}
	/**********************************************************************************************************************
	 * Killed so tidy up.                                                                                                 *
	 **********************************************************************************************************************/
	unlink (pidFileName);
	return 0;
}

