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
#include "socketC.h"

#define MAX_HANDLES		22
#define SERIAL_HANDLE	0
#define LISTEN_HANDLE	1
#define FIRST_HANDLE	2

char pidFileName[81]	=	"/var/run/trainDaemon.pid";
char serialDevice[81]	=	"/dev/ttyACM0";
int	 logOutput			=	0;
int	 infoOutput			=	0;
int	 debugOutput		=	0;
int	 goDaemon			=	0;
int	 inDaemonise		=	0;
int	 running			=	1;
int	 listenPort			=	21000;

typedef struct _handleInfo
{
	int handle;
	char localName[81];
	char remoteName[81];
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
		if (doSysLog) syslog (priority, tempBuffer);
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
	dup(i);
	dup(i);

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
	write(lfp, str, strlen(str));	/* record pid to lockfile */

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
int SerialPortSetup (char *device)
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
int SendSerial (char *buffer, int len)
{
	return write (handleInfo[SERIAL_HANDLE].handle, buffer, len);
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  R E C E I V E  S E R I A L                                                                                        *
 *  ==========================                                                                                        *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  \brief Keep buffering until we get a closing '>' and the send to all.
 *  \param buffer Received buffer.
 *  \param len Buffer length.
 *  \result None.
 */
void ReceiveSerial (char *buffer, int len)
{
	static char outBuffer[10241];
	static int outPosn = 0;
	int i = 0, j = 0;

	while (j < len)
	{
		outBuffer[outPosn++] = buffer[j];
		if (buffer[j] == '>')
		{
			outBuffer[outPosn] = 0;
			printf ("Net sending: [%s]\n", outBuffer);
			for (i = FIRST_HANDLE; i < MAX_HANDLES; ++i)
			{
				if (handleInfo[i].handle != -1)
				{
					SendSocket (handleInfo[i].handle, outBuffer, outPosn);
				}
			}
			outPosn = 0;
		}
		if (outPosn >= 10240)
		{
			outPosn = 0;
			break;
		}
		++j;
	}
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
	char inAddress[21] = "";
	struct timeval timeout;
	int i, c;

	while ((c = getopt(argc, argv, "s:l:dLID")) != -1)
	{
		switch (c)
		{
		case 'd':
			goDaemon = 1;
			break;

		case 'l':
			listenPort = atoi (optarg);
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

		case 's':
			strcpy (serialDevice, optarg);
			break;

		case '?':
			fprintf (stderr, "Usage: trainDaemon [-l port] [-s device]\n");
			fprintf (stderr, "       -l port . . . . . Listening port number.\n");
			fprintf (stderr, "       -s serial . . . . DCC++ serial device.\n");
			fprintf (stderr, "       -d  . . . . . . . Deamonise the process.\n");
			fprintf (stderr, "       -L  . . . . . . . Write messages to syslog.\n");
			fprintf (stderr, "       -I  . . . . . . . Write info messages.\n");
			fprintf (stderr, "       -D  . . . . . . . Write debug messages.\n");
			exit (1);
		}
	}
	if (goDaemon)
	{
		daemonize();
	}
	for (i = 0; i < MAX_HANDLES; ++i)
	{
		handleInfo[i].handle = -1;
	}

	handleInfo[SERIAL_HANDLE].handle = SerialPortSetup (serialDevice);
	if (handleInfo[SERIAL_HANDLE].handle == -1)
	{
		putLogMessage (LOG_ERR, "Unable to connect to serial port.");
	}
	else
	{
		putLogMessage (LOG_INFO, "Listening on serial: %s", serialDevice);

		handleInfo[LISTEN_HANDLE].handle = ServerSocketSetup (listenPort);
		if (handleInfo[LISTEN_HANDLE].handle == -1)
		{
			putLogMessage (LOG_ERR, "Unable to listen as a server");
		}
		else
		{
			putLogMessage (LOG_INFO, "Listening on port %d", listenPort);
		}
	}
	while (handleInfo[LISTEN_HANDLE].handle != -1 && running)
	{
		int selRetn;
		timeout.tv_sec = 10;
		timeout.tv_usec = 0;

		FD_ZERO(&readfds);
		for (i = 0; i < MAX_HANDLES; ++i)
		{
			if (handleInfo[i].handle != -1)
			{
				FD_SET (handleInfo[i].handle, &readfds);
			}
		}
		selRetn = select(FD_SETSIZE, &readfds, NULL, NULL, &timeout);
		if (selRetn == -1)
		{
			putLogMessage (LOG_ERR, "Select error %d:%s", errno, strerror (errno));
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
							handleInfo[i].handle = newSocket;
							strncpy (handleInfo[i].localName, inAddress, 40);
							SendSerial ("<s>", 3);
							break;
						}
					}
					if (i == MAX_HANDLES)
					{
						putLogMessage (LOG_INFO, "No free handles.");
						CloseSocket (&newSocket);
					}
				}
			}
			if (FD_ISSET(handleInfo[SERIAL_HANDLE].handle, &readfds))
			{
				int readBytes;
				char buffer[10241];
				if ((readBytes = read (handleInfo[SERIAL_HANDLE].handle, buffer, 10240)) > 0)
				{
					buffer[readBytes] = 0;
					putLogMessage (LOG_DEBUG, "Serial read %d bytes [%s]", readBytes, buffer);
					ReceiveSerial (buffer, readBytes);
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
							putLogMessage (LOG_DEBUG, "Sending %s bytes %s(%d) -> Serial", buffer,
									handleInfo[i].localName, handleInfo[i].handle);
							SendSerial (buffer, readBytes);
						}
						else if (readBytes == 0)
						{
							putLogMessage (LOG_INFO, "Socket %s(%d) closed", handleInfo[i].localName, handleInfo[i].handle);
							CloseSocket (&handleInfo[i].handle);
						}
					}
				}
			}
		}
	}
	unlink (pidFileName);
	return 0;
}

