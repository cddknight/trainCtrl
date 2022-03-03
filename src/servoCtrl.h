/**********************************************************************************************************************
 *                                                                                                                    *
 *  S E R V O  C T R L . H                                                                                            *
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

#include <pthread.h>

#define SERVO_OFF		0
#define SERVO_MOVE		1
#define SERVO_SLEEP		2
#define SERVO_CHECK		3

#define SERVO_STEP		5
#define SERVO_WAIT		6

#define PIN_BASE		300
#define MAX_PWM			4096
#define HERTZ			50
#define PWM_DELAY		100

typedef struct _servoState
{
	int state;
	int channel;
	int currentPos;
	int targetPos;
	int count;
	int priority;
	pthread_mutex_t updateMutex;
}
servoStateDef;

void servoInit (servoStateDef *servoDef, int channel, int defPos);
void servoFree (servoStateDef *servoDef);
void servoMove (servoStateDef *servoDef, int newPos, int priority);
int servoUpdate (servoStateDef *servoDef);

