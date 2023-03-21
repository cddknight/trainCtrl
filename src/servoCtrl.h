/**********************************************************************************************************************
 *                                                                                                                    *
 *  S E R V O  C T R L . H                                                                                            *
 *  ======================                                                                                            *
 *                                                                                                                    *
 *  Copyright (c) 2023 Chris Knight                                                                                   *
 *                                                                                                                    *
 *  File servoCtrl.h part of TrainControl is free software: you can redistribute it and/or modify it under the terms  *
 *  of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License,  *
 *  or (at your option) any later version.                                                                            *
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

