/**********************************************************************************************************************
 *                                                                                                                    *
 *  T R A I N  T H R O T T L E . C                                                                                    *
 *  ==============================                                                                                    *
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
 *  \brief Create a thread that will read the joystick.
 */
#include <gtk/gtk.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <linux/joystick.h>

#include "config.h"
#include "trainControl.h"

/**********************************************************************************************************************
 *                                                                                                                    *
 *  T H R O T T L E  T H R E A D                                                                                      *
 *  ============================                                                                                      *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  \brief Thread that reads the joystick device.
 *  \param param Track config.
 *  \result None.
 */
void *throttleThread (void *param)
{
	char devName[81];
	trackCtrlDef *trackCtrl = (trackCtrlDef *)param;
	struct timeval timeout;
	struct js_event event;
	fd_set readfds;
	int jsHandle, i;

	sprintf (devName, "/dev/input/js%d", trackCtrl -> jStickNumber);
	if ((jsHandle = open(devName, O_RDONLY)) == -1)
	{
		return NULL;
	}

	while (trackCtrl -> throttlesRunning)
	{
		timeout.tv_sec = 1;
		timeout.tv_usec = 0;

		FD_ZERO(&readfds);
		FD_SET (jsHandle, &readfds);

		if (select(FD_SETSIZE, &readfds, NULL, NULL, &timeout) > 0)
		{
			if (FD_ISSET(jsHandle, &readfds))
			{
				if (read (jsHandle, &event, sizeof(event)) == sizeof(event))
				{
					/*
					struct js_event 
					{
						__u32 time;     // event timestamp in milliseconds
						__s16 value;    // value
						__u8 type;      // event type
						__u8 number;    // axis/button number
					};
					*/
					for (i = 0; i < trackCtrl -> throttleCount; ++i)
					{
						if (event.type == JS_EVENT_AXIS && trackCtrl -> throttles[i].axis == event.number)
						{
							int fixVal = event.value + 32767;
							if (trackCtrl -> throttles[i].zeroHigh)
								fixVal = 65534 - fixVal;

							fixVal *= 126;
							fixVal /= 65534;

							pthread_mutex_lock (&trackCtrl -> throttleMutex);
							if (trackCtrl -> throttles[i].curValue != fixVal)
							{
								trackCtrl -> throttles[i].curValue = fixVal;
								trackCtrl -> throttles[i].curChanged = 1;
							}
							pthread_mutex_unlock (&trackCtrl -> throttleMutex);
						}
						if (event.type == JS_EVENT_BUTTON && trackCtrl -> throttles[i].button == event.number && event.value != 0)
						{
							pthread_mutex_lock (&trackCtrl -> throttleMutex);
							trackCtrl -> throttles[i].buttonPress = 1;
							pthread_mutex_unlock (&trackCtrl -> throttleMutex);
						}
					}
				}
			}
		}
	}

	close (jsHandle);
	return NULL;
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  S T A R T  T H R O T T L E  T H R E A D                                                                           *
 *  =======================================                                                                           *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  \brief Start the read joystick thread.
 *  \param trackCtrl Track config.
 *  \result none.
 */
int startThrottleThread (trackCtrlDef *trackCtrl)
{
	int jsHandle, retn = 0;

	if (trackCtrl -> throttles != NULL && trackCtrl -> throttleCount)
	{
		char devName[81];
		sprintf (devName, "/dev/input/js%d", trackCtrl -> jStickNumber);
		if ((jsHandle = open(devName, O_RDONLY)) != -1)
		{
			retn = 1;
			close (jsHandle);
		}
		if (retn == 1)
		{
			trackCtrl -> throttlesRunning = 1;
			if (pthread_create (&trackCtrl -> throttlesHandle, NULL, throttleThread, trackCtrl) != 0)
			{
				trackCtrl -> throttlesRunning = 0;
				printf ("startThrottleThread: create failed\n");
				retn = 0;
			}
		}
		else
			printf ("startThrottleThread: js open failed\n");
	}
	return retn;
}

