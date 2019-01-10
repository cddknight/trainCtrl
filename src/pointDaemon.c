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
//#include "trainControl.h"
#include "pointControl.h"

#define MAX_HANDLES		5
#define LISTEN_HANDLE	0
#define FIRST_HANDLE	1

char xmlConfigFile[81]	=	"points.xml";
char pidFileName[81]	=	"/var/run/pointDaemon.pid";
int	 logOutput			=	0;
int	 infoOutput			=	0;
int	 debugOutput		=	0;
int	 goDaemon			=	0;
int	 inDaemonise		=	0;
int	 running			=	1;
int	 serverIdent		=	1;
pointCtrlDef pointCtrl;

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
	fprintf (stderr, "Usage: pointDaemon -l <port>\n");
    fprintf (stderr, "       -l <port> . . . . Listen port number.\n");
	fprintf (stderr, "       -d  . . . . . . . Deamonise the process.\n");
	fprintf (stderr, "       -L  . . . . . . . Write messages to syslog.\n");
	fprintf (stderr, "       -I  . . . . . . . Write info messages.\n");
	fprintf (stderr, "       -D  . . . . . . . Write debug messages.\n");
	exit (1);
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
	long xmlBufferSize = 0;
	char *xmlBuffer = NULL;

	putLogMessage (LOG_INFO, "P:Config file: %s", xmlConfigFile);
	if (stat (xmlConfigFile, &statbuf) == 0)
	{
		FILE *inFile = fopen (xmlConfigFile, "r");
		if (inFile != NULL)
		{
			xmlBufferSize = statbuf.st_size;
			if ((xmlBuffer = (char *)malloc (xmlBufferSize + 10)) != NULL)
			{
				if (fread (xmlBuffer, 1, xmlBufferSize, inFile) == xmlBufferSize)
				{
					retn = parseMemoryXML (&pointCtrl, xmlBuffer);
				}
				free (xmlBuffer);
			}
			fclose (inFile);
		}
	}
	return retn;
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
	char inAddress[21] = "";
	int i, c, connectedCount = 0;
	time_t curRead = time(NULL) + 5;

	while ((c = getopt(argc, argv, "c:s:dLID")) != -1)
	{
		switch (c)
		{
		case 'c':
			strncpy (xmlConfigFile, optarg, 80);
			break;

		case 's':
			serverIdent = atoi (optarg);
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

	pointCtrl.server = serverIdent;
	loadConfigFile ();

	for (i = 0; i < MAX_HANDLES; ++i)
	{
		handleInfo[i].handle = -1;
	}

	/**********************************************************************************************************************
	 * Daemonize if needed, all port will close.                                                                          *
	 **********************************************************************************************************************/
	if (goDaemon)
	{
		daemonize();
	}

	/**********************************************************************************************************************
	 * Setup listening network ports.                                                                                     *
	 **********************************************************************************************************************/
	handleInfo[LISTEN_HANDLE].handle = ServerSocketSetup (pointCtrl.serverPort);
	if (handleInfo[LISTEN_HANDLE].handle == -1)
	{
		putLogMessage (LOG_ERR, "P:Unable to listen on network port.");
	}
	else
	{
		putLogMessage (LOG_INFO, "P:Listening on port: %d", pointCtrl.serverPort);
	}

	/**********************************************************************************************************************
	 * Loop on select, getting and sending work.                                                                          *
	 **********************************************************************************************************************/
	while (handleInfo[LISTEN_HANDLE].handle != -1 && running)
	{
		int selRetn;
		timeout.tv_sec = 2;
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
			putLogMessage (LOG_ERR, "P:Select error: %s[%d]", strerror (errno), errno);
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
							strncpy (handleInfo[i].localName, inAddress, 40);
							putLogMessage (LOG_INFO, "P:Socket opened: %s(%d)", handleInfo[i].localName, handleInfo[i].handle);
							++connectedCount;
							break;
						}
					}
					if (i == MAX_HANDLES)
					{
						putLogMessage (LOG_ERR, "P:No free handles.");
						CloseSocket (&newSocket);
					}
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
							putLogMessage (LOG_INFO, "P:Socket rxed: %s(%d)", buffer, handleInfo[i].handle);
							checkRecvBuffer (&pointCtrl, handleInfo[i].handle, buffer, readBytes);
						}
						else if (readBytes == 0)
						{
							putLogMessage (LOG_INFO, "P:Socket closed: %s(%d)", handleInfo[i].localName, handleInfo[i].handle);
							CloseSocket (&handleInfo[i].handle);
							--connectedCount;
						}
					}
				}
			}
		}
	}
	/**********************************************************************************************************************
	 * Killed so tidy up.                                                                                                 *
	 **********************************************************************************************************************/
	unlink (pidFileName);
	return 0;
}

