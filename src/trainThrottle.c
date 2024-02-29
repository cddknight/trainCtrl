/**********************************************************************************************************************
 *                                                                                                                    *
 *  T R A I N  T H R O T T L E . C                                                                                    *
 *  ==============================                                                                                    *
 *                                                                                                                    *
 *  Copyright (c) 2023 Chris Knight                                                                                   *
 *                                                                                                                    *
 *  File trainThrottle.c part of TrainControl is free software: you can redistribute it and/or modify it under the    *
 *  terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the     *
 *  License, or (at your option) any later version.                                                                   *
 *                                                                                                                    *
 *  TrainControl is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the        *
 *  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for  *
 *  more details.                                                                                                     *
 *                                                                                                                    *
 *  You should have received a copy of the GNU General Public License along with this program. If not, see:           *
 *  <http://www.gnu.org/licenses/>                                                                                    *
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
#include <libevdev-1.0/libevdev/libevdev.h>

//#include <linux/joystick.h>

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
	trackCtrlDef *trackCtrl = (trackCtrlDef *)param;
	struct libevdev *dev = NULL;
	struct timeval timeout;
	char openDev[256];
	fd_set readfds;
	int jsHandle, rc;

	sprintf (openDev, "/dev/input/by-id/usb-%s-event-joystick", trackCtrl -> throttleName);
	if ((jsHandle = open (openDev, O_RDONLY|O_NONBLOCK)) != -1)
	{
		rc = libevdev_new_from_fd(jsHandle, &dev);
	}
	if (jsHandle == -1 || rc < 0)
	{
		return NULL;
	}
	trackCtrl -> throttlesRunning = 1;

	do
	{
		timeout.tv_sec = 1;
		timeout.tv_usec = 0;

		FD_ZERO(&readfds);
		FD_SET (jsHandle, &readfds);

		if (select(FD_SETSIZE, &readfds, NULL, NULL, &timeout) > 0)
		{
			if (FD_ISSET(jsHandle, &readfds))
			{
				struct input_event event;
				rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_NORMAL, &event);
				if (rc == 0)
				{
					int i;

/*					printf("Event: Type(%d):%s Code(%d):%s Value(%d): %s\n",
							event.type, libevdev_event_type_get_name(event.type),
							event.code, libevdev_event_code_get_name(event.type, event.code),
							event.value, libevdev_event_value_get_name(event.type, event.code, event.value));
*/
					for (i = 0; i < trackCtrl -> throttleCount; ++i)
					{
						if (event.type == 3 && trackCtrl -> throttles[i].axis == event.code)
						{
							int fixVal = (event.value * 126 / 255);

							if (trackCtrl -> throttles[i].zeroHigh)
							{
								fixVal = 126 - fixVal;
							}
							pthread_mutex_lock (&trackCtrl -> throttleMutex);
							if (trackCtrl -> throttles[i].curValue != fixVal)
							{
								trackCtrl -> throttles[i].curValue = fixVal;
								trackCtrl -> throttles[i].curChanged = 1;
							}
							pthread_mutex_unlock (&trackCtrl -> throttleMutex);
						}
						if (event.type == 1 && trackCtrl -> throttles[i].button == event.code && event.value != 0)
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
	while ((rc == 1 || rc == 0 || rc == -11) && trackCtrl -> throttlesRunning);

	trackCtrl -> throttlesRunning = 0;
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
	int retn = 0;

	if (trackCtrl -> throttles != NULL && trackCtrl -> throttleCount)
	{
		if (pthread_create (&trackCtrl -> throttlesHandle, NULL, throttleThread, trackCtrl) != 0)
		{
			trackCtrl -> throttlesRunning = 0;
			printf ("startThrottleThread: create failed\n");
			retn = 0;
		}
		else
			retn = 1;
	}
	return retn;
}

