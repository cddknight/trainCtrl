/**********************************************************************************************************************
 *                                                                                                                    *
 *  S E R V O  C T R L . C                                                                                            *
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
 *  \brief Fuction to control the servos an slow the update.
 */

#include <stdio.h>
#include <syslog.h>
#include "config.h"

#ifdef HAVE_WIRINGPI_H
#include <wiringPi.h>
#include "pca9685.h"
#endif

#include "servoCtrl.h"
#include "pointControl.h"

/**********************************************************************************************************************
 *                                                                                                                    *
 *  S E R V O  I N I T                                                                                                *
 *  ==================                                                                                                *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  \brief Initialise the servo configuration.
 *  \param servoDef Default servo poition.
 *  \param channel Servo channel number.
 *  \param defPos Default servo poition.
 *  \result None.
 */
void servoInit (servoStateDef *servoDef, int channel, int defPos)
{
	servoDef -> state = SERVO_CHECK;
	servoDef -> channel = channel;
	servoDef -> currentPos = defPos;
	servoDef -> targetPos = defPos;
	servoDef -> count = 0;

	pthread_mutex_init (&servoDef -> updateMutex, NULL);
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  S E R V O  F R E E                                                                                                *
 *  ==================                                                                                                *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  \brief Unallocate the servo locks.
 *  \param servoDef Servo configuration.
 *  \result None.
 */
void servoFree (servoStateDef *servoDef)
{
	pthread_mutex_destroy (&servoDef -> updateMutex);
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  S E R V O  M O V E                                                                                                *
 *  ==================                                                                                                *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  \brief Move the servo to a new position.
 *  \param servoDef Servo configuration.
 *  \param newPos New servo position.
 *  \result None.
 */
void servoMove (servoStateDef *servoDef, int newPos)
{
	pthread_mutex_lock (&servoDef -> updateMutex);
	if (newPos == 0)
	{
		servoDef -> currentPos = servoDef -> targetPos;
		servoDef -> state = SERVO_SLEEP;
		servoDef -> count = SERVO_WAIT;
	}
	else if (newPos == servoDef -> currentPos)
	{
		servoDef -> state = SERVO_CHECK;
		servoDef -> count = 0;
	}
	else
	{
		servoDef -> state = SERVO_MOVE;
		servoDef -> targetPos = newPos;
		servoDef -> count = 0;
	}
	pthread_mutex_unlock (&servoDef -> updateMutex);
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  S E R V O  U P D A T E                                                                                            *
 *  ======================                                                                                            *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  \brief Called from a thread to update the servo.
 *  \param servoDef Servo configuration.
 *  \result None.
 */
int servoUpdate (servoStateDef *servoDef)
{
	int update = 0;
	pthread_mutex_lock (&servoDef -> updateMutex);
	switch (servoDef -> state)
	{
	case SERVO_CHECK:
#ifdef HAVE_WIRINGPI_H
		pwmWrite(PIN_BASE + servoDef -> channel, servoDef -> currentPos);
		update = 1;
#endif
		servoDef -> state = SERVO_SLEEP;
		servoDef -> count = SERVO_WAIT;
		break;

	case SERVO_MOVE:
		if (servoDef -> currentPos == servoDef -> targetPos)
		{
			servoDef -> state = SERVO_SLEEP;
			servoDef -> count = SERVO_WAIT;
			break;
		}
		else if (servoDef -> currentPos < servoDef -> targetPos)
		{
			servoDef -> currentPos += SERVO_STEP;
			if (servoDef -> currentPos > servoDef -> targetPos)
				servoDef -> currentPos = servoDef -> targetPos;
		}
		else if (servoDef -> currentPos > servoDef -> targetPos)
		{
			servoDef -> currentPos -= SERVO_STEP;
			if (servoDef -> currentPos < servoDef -> targetPos)
				servoDef -> currentPos = servoDef -> targetPos;
		}
#ifdef HAVE_WIRINGPI_H
		pwmWrite(PIN_BASE + servoDef -> channel, servoDef -> currentPos);
		update = 1;
#endif
		break;

	case SERVO_SLEEP:
		if (servoDef -> count)
		{
			--servoDef -> count;
		}
		else if (servoDef -> count == 0)
		{
#ifdef HAVE_WIRINGPI_H
			pwmWrite(PIN_BASE + servoDef -> channel, 0);
#endif
			servoDef -> state = SERVO_OFF;
		}
		update = 1;
		break;
	}
	pthread_mutex_unlock (&servoDef -> updateMutex);
	return update;
}

