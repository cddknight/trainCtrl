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
#include "buildDate.h"
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
	struct js_event event;
	int jsHandle;

	if ((jsHandle = open("/dev/input/js0", O_RDONLY)) == -1)
	{
		return NULL;
	}

	while (trackCtrl -> throttlesRunning)
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

			if (event.type == JS_EVENT_AXIS && trackCtrl -> throttles[0].axis == event.number)
			{
				int fixVal = event.value + 32767;
				fixVal *= 126;
				fixVal /= 65534;

				pthread_mutex_lock (&trackCtrl -> throttleMutex);
				if (trackCtrl -> throttles[0].curValue != fixVal)
				{
					trackCtrl -> throttles[0].curValue = fixVal;
					trackCtrl -> throttles[0].curChanged = 1;
				}
				pthread_mutex_unlock (&trackCtrl -> throttleMutex);
			}
			if (event.type == JS_EVENT_BUTTON && trackCtrl -> throttles[0].button == event.number && event.value != 0)
			{
				pthread_mutex_lock (&trackCtrl -> throttleMutex);
				trackCtrl -> throttles[0].buttonPress = 1;
				pthread_mutex_unlock (&trackCtrl -> throttleMutex);
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
		if ((jsHandle = open ("/dev/input/js0", O_RDONLY)) != -1)
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
	printf ("startThrottleThread: %d\n", retn);
	return retn;
}

