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

/**********************************************************************************************************************
 *                                                                                                                    *
 *  C H E C K  R E C V  B U F F E R                                                                                   *
 *  ===============================                                                                                   *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  \brief Chech what we have received on the socket to see if we should update the display.
 *  \param trackCtrl Which is the active track.
 *  \param buffer Buffer that was received.
 *  \param len Length of the buffer.
 *  \result None.
 */
void checkRecvBuffer (trackCtrlDef *trackCtrl, char *buffer, int len)
{
	char words[41][41];
	int wordNum = -1, i = 0, j = 0, inType = 0;

/*------------------------------------------------------------------*
	printf ("Rxed:[%s]\n", buffer);
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
			{
				words[++wordNum][0] = 0;
			}
/*------------------------------------------------------------------*
			for (l = 0; l < wordNum; ++l)
			{
				printf ("[%s]", words[l]);
			}
			printf ("(%d)\n", wordNum);
 *------------------------------------------------------------------*/
			/* Track power status */
			if (words[0][0] == 'p' && words[0][1] == 0 && wordNum == 2)
			{
				int power = atoi(words[1]);
				trackCtrl -> remotePowerState = power;
				if (!power)
				{
					trackCtrl -> remoteCurrent = -1;
				}
			}
			/* Throttle status */
			else if (words[0][0] == 'T' && words[0][1] == 0 && wordNum == 4)
			{
				int trainReg = atoi(words[1]), t;
				for (t = 0; t < trackCtrl -> trainCount; ++t)
				{
					if (trackCtrl -> trainCtrl[t].trainReg == trainReg)
					{
						trackCtrl -> trainCtrl[t].remoteCurSpeed = atoi(words[2]);
						trackCtrl -> trainCtrl[t].remoteReverse = atoi(words[3]);
					}
				}
			}
			/* Read CV value */
			else if (words[0][0] == 'r' && words[0][1] == 0 && wordNum == 5)
			{
				if (atoi (words[1]) == trackCtrl -> serverSession)
				{
					char binary[9];
					int val = atoi (words[4]), mask = 0x80, i;
					for (i = 0; i < 8; ++i)
					{
						binary[i] = (val & mask ? '1' : '0');
						mask >>= 1;
					}
					binary[i] = 0;
					snprintf (trackCtrl -> remoteProgMsg, 110, "Read CV#%s value: %s [%s]",
								words[3], words[4], binary);
				}
			}
			/* Current monitor */
			else if (words[0][0] == 'a' && words[0][1] == 0 && wordNum == 2)
			{
				if (trackCtrl -> powerState == POWER_ON)
				{
					trackCtrl -> remoteCurrent = atoi (words[1]);
				}
			}
			else if (words[0][0] == 'V' && words[0][1] == 0 && wordNum == 2)
			{
				trackCtrl -> serverSession = atoi (words[1]);
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
		{
			j = 40;
		}
		++i;
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
	trackCtrlDef *trackCtrl = (trackCtrlDef *)arg;

	trackCtrl -> connectRunning = 1;
	trackCtrl -> serverHandle = -1;

	while (trackCtrl -> connectRunning)
	{
		if (trackCtrl -> serverHandle == -1)
		{
			trackCtrl -> serverHandle = ConnectClientSocket (trackCtrl -> server, trackCtrl -> serverPort);
			if (trackCtrl -> serverHandle == -1)
			{
				sleep (5);
			}
		}
		else
		{
			timeout.tv_sec = 0;
			timeout.tv_usec = 100000;

			FD_ZERO(&readfds);
			FD_SET (trackCtrl -> serverHandle, &readfds);

			selRetn = select(FD_SETSIZE, &readfds, NULL, NULL, &timeout);
			if (selRetn == -1)
			{
				CloseSocket (&trackCtrl -> serverHandle);
			}
			if (selRetn > 0)
			{
				if (FD_ISSET(trackCtrl -> serverHandle, &readfds))
				{
					int readBytes;
					char buffer[10241];

					if ((readBytes = RecvSocket (trackCtrl -> serverHandle, buffer, 10240)) > 0)
					{
						buffer[readBytes] = 0;
						checkRecvBuffer (trackCtrl, buffer, readBytes);
					}
					else if (readBytes == 0)
					{
						CloseSocket (&trackCtrl -> serverHandle);
					}
				}
			}
		}
	}
	if (trackCtrl -> serverHandle != -1)
	{
		CloseSocket (&trackCtrl -> serverHandle);
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
 *  \param trackCtrl Which is the active track.
 *  \param buffer Message to send.
 *  \param len Size of the message.
 *  \result Number of bytes sent.
 */
int trainConnectSend (trackCtrlDef *trackCtrl, char *buffer, int len)
{
	int retn = -1;

	if (trackCtrl -> serverHandle != -1)
	{
		retn = SendSocket (trackCtrl -> serverHandle, buffer, len);
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
 *  \param trackCtrl Which is the active track.
 *  \result 1 if thread started (may not be connected).
 */
int startConnectThread(trackCtrlDef *trackCtrl)
{
	int retn = pthread_create (&trackCtrl -> connectHandle, NULL, trainConnectThread, trackCtrl);
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
 *  \param trackCtrl Which is the active track.
 *  \result None.
 */
void stopConnectThread(trackCtrlDef *trackCtrl)
{
	trackCtrl -> connectRunning = 0;
	pthread_join (trackCtrl -> connectHandle, NULL);
}

