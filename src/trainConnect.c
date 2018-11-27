/**********************************************************************************************************************
 *                                                                                                                    *
 *  T R A I N  C O N N E C T . C                                                                                      *
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
 *  \brief Thread to handle the connectio to the daemon.
 */
#include <gtk/gtk.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "trainCtrl.h"
#include "socketC.h"

extern trackCtrlDef trackCtrl;

static pthread_t connectHandle;
static int connectRunning = 0;


/**********************************************************************************************************************
 *                                                                                                                    *
 *  C H E C K  R E C V  B U F F E R                                                                                   *
 *  ===============================                                                                                   *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  \brief Chech what we have received on the socket to see if we should update the display.
 *  \param buffer Buffer that was received.
 *  \param len Length of the buffer.
 *  \result None.
 */
void checkRecvBuffer (char *buffer, int len)
{
	char words[10][41];
	int word = -1, i = 0, j = 0, type = -1;

	while (i < len)
	{
		if (buffer[i] == '<' && word == -1)
		{
			word = 0;
		}
		else if (((buffer[i] >= 'a' && buffer[i] <= 'z') || (buffer[i] >= 'A' && buffer[i] <= 'Z')) && word >= 0)
		{
			if (type == 1)
			{
				++word;
				j = 0;
			}
			words[word][j++] = buffer[i];
			words[word][j] = 0;
			type = 0;
		}
		else if (buffer[i] >= '0' && buffer[i] <= '9' && word >= 0)
		{
			if (type == 0)
			{
				++word;
				j = 0;
			}
			words[word][j++] = buffer[i];
			words[word][j] = 0;
			type = 1;
		}
		else if (buffer[i] == '>' && word >= 0)
		{
			++word;
			break;
		}
		else if (word >= 0)
		{
			++word;
			j = 0;			
		}
		++i;
	}
	if (words[0][0] == 'p' && words[0][1] == 0)
	{
		trackCtrl.remotePowerState = atoi(words[1]);
	}
	else if (words[0][0] == 'T' && words[0][1] == 0)
	{
		int trainReg = atoi(words[1]), i;
		
		for (i = 0; i < trackCtrl.trainCount; ++i)
		{
			if (trackCtrl.trainCtrl[i].trainReg == trainReg)
			{
				trackCtrl.trainCtrl[i].remoteCurSpeed = atoi(words[2]);
				trackCtrl.trainCtrl[i].remoteReverse = atoi(words[3]);
			}
		}
	}
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  T R A I N  C O N N E C T  T H R E A D                                                                             *
 *  =====================================                                                                             *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  \brief Thread to handle the connection to the daemon.
 *  \param arg Not used.
 *  \result None.
 */
void *trainConnectThread (void *arg)
{
	int selRetn;
	fd_set readfds;
	struct timeval timeout;

	connectRunning = 1;
	trackCtrl.serverHandle = -1;

	while (connectRunning)
	{
		if (trackCtrl.serverHandle == -1)
		{
			trackCtrl.serverHandle = ConnectClientSocket (trackCtrl.server, trackCtrl.serverPort);
			if (trackCtrl.serverHandle == -1)
			{
				sleep (5);
			}
		}
		else
		{
			timeout.tv_sec = 0;
			timeout.tv_usec = 100000;

			FD_ZERO(&readfds);
			FD_SET (trackCtrl.serverHandle, &readfds);

			selRetn = select(FD_SETSIZE, &readfds, NULL, NULL, &timeout);
			if (selRetn == -1)
			{
				CloseSocket (&trackCtrl.serverHandle);
			}
			if (selRetn > 0)
			{
				if (FD_ISSET(trackCtrl.serverHandle, &readfds))
				{
					int readBytes;
					char buffer[10241];

					if ((readBytes = RecvSocket (trackCtrl.serverHandle, buffer, 10240)) > 0)
					{
						buffer[readBytes] = 0;
						checkRecvBuffer (buffer, readBytes);
					}
					else if (readBytes == 0)
					{
						CloseSocket (&trackCtrl.serverHandle);
					}
				}
			}
		}
	}
	if (trackCtrl.serverHandle != -1)
	{
		CloseSocket (&trackCtrl.serverHandle);
	}
	return NULL;
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  T R A I N  C O N N E C T  S E N D                                                                                 *
 *  =================================                                                                                 *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  \brief Send a message to the daemon.
 *  \param buffer Message to send.
 *  \param len Size of the message.
 *  \result Number of bytes sent.
 */
int trainConnectSend (char *buffer, int len)
{
	int retn = -1;

	if (trackCtrl.serverHandle != -1)
	{
		retn = SendSocket (trackCtrl.serverHandle, buffer, len);
	}
	return retn;
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  S T A R T  C O N N E C T  T H R E A D                                                                             *
 *  =====================================                                                                             *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  \brief Start the thread controlling the connection.
 *  \result 1 if thread started (may not be connected).
 */
int startConnectThread()
{
	int retn = pthread_create (&connectHandle, NULL, trainConnectThread, NULL);
	return (retn == 0 ? 1 : 0);
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  S T O P  C O N N E C T  T H R E A D                                                                               *
 *  ===================================                                                                               *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  \brief Stop and wait for the thread.
 *  \result None.
 */
void stopConnectThread()
{
	connectRunning = 0;
	pthread_join (connectHandle, NULL);
}

