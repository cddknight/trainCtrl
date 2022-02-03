/**********************************************************************************************************************
 *                                                                                                                    *
 *  P O I N T  C O N T R O L . C                                                                                      *
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
 *  \brief Control the points taking commands from the network.
 */
#include "config.h"
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
#include <libxml/parser.h>
#include <libxml/tree.h>
#ifdef HAVE_WIRINGPI_H
#include <wiringPi.h>
#include "pca9685.h"
#endif

#include "socketC.h"
#include "servoCtrl.h"
#include "pointControl.h"

int servoFD = -1;
int curPriority = 0;
extern int running;
pthread_t threadHandle;
pthread_mutex_t priorityMutex;

/**********************************************************************************************************************
 *                                                                                                                    *
 *  P R O C E S S  P O I N T S                                                                                        *
 *  ==========================                                                                                        *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  \brief Process reading in all the ports.
 *  \param pointCtrl Save data hear.
 *  \param inNode Current node under this you will find the points.
 *  \param pCount Number of points to expect.
 *  \param sCount Number od signals to expect.
 *  \result None.
 */
void processPoints (pointCtrlDef *pointCtrl, xmlNode *inNode, int pCount, int sCount)
{
	int pFound = 0, sFound = 0;
	xmlChar *tempStr;
	xmlNode *curNode = NULL;

	if (pCount > 0)
	{
		if ((pointCtrl -> pointStates = (pointStateDef *)malloc (pCount * sizeof (pointStateDef))) == NULL)
			return;
		memset (pointCtrl -> pointStates, 0, pCount * sizeof (pointStateDef));
	}
	if (sCount > 0)
	{
		if ((pointCtrl -> signalStates = (signalStateDef *)malloc (sCount * sizeof (signalStateDef))) == NULL)
			return;
		memset (pointCtrl -> signalStates, 0, sCount * sizeof (signalStateDef));
	}

	for (curNode = inNode; curNode; curNode = curNode->next)
	{
		if (curNode->type == XML_ELEMENT_NODE)
		{
			if (strcmp ((char *)curNode->name, "point") == 0 && pFound < pCount)
			{
				int ident = -1, channel = -1, defaultPos = -1, turnoutPos = -1;

				if ((tempStr = xmlGetProp(curNode, (const xmlChar*)"ident")) != NULL)
				{
					sscanf ((char *)tempStr, "%d", &ident);
					xmlFree (tempStr);
				}
				if ((tempStr = xmlGetProp(curNode, (const xmlChar*)"channel")) != NULL)
				{
					sscanf ((char *)tempStr, "%d", &channel);
					xmlFree (tempStr);
				}
				if ((tempStr = xmlGetProp(curNode, (const xmlChar*)"default")) != NULL)
				{
					sscanf ((char *)tempStr, "%d", &defaultPos);
					xmlFree (tempStr);
				}
				if ((tempStr = xmlGetProp(curNode, (const xmlChar*)"turnout")) != NULL)
				{
					sscanf ((char *)tempStr, "%d", &turnoutPos);
					xmlFree (tempStr);
				}								
				if (ident != -1 && channel != -1 && defaultPos != -1 && turnoutPos != -1)
				{
					pointCtrl -> pointStates[pFound].ident = ident;
					pointCtrl -> pointStates[pFound].servoChannel = channel;
					pointCtrl -> pointStates[pFound].defaultPos = defaultPos;
					pointCtrl -> pointStates[pFound].turnoutPos = turnoutPos;
					++pFound;
				}
			}
			else if (strcmp ((char *)curNode->name, "signal") == 0 && sFound < sCount)
			{
				int ident = -1, cRed = -1, cGreen = -1, channel = -1, redOut = -1, greenOut = -1, sType = -1;

				if ((tempStr = xmlGetProp(curNode, (const xmlChar*)"ident")) != NULL)
				{
					sscanf ((char *)tempStr, "%d", &ident);
					xmlFree (tempStr);
				}
				if ((tempStr = xmlGetProp(curNode, (const xmlChar*)"type")) != NULL)
				{
					sscanf ((char *)tempStr, "%d", &sType);
					xmlFree (tempStr);
				}
				if ((tempStr = xmlGetProp(curNode, (const xmlChar*)"channel")) != NULL)
				{
					sscanf ((char *)tempStr, "%d", &channel);
					xmlFree (tempStr);
				}
				if ((tempStr = xmlGetProp(curNode, (const xmlChar*)"channelRed")) != NULL)
				{
					sscanf ((char *)tempStr, "%d", &cRed);
					xmlFree (tempStr);
				}
				if ((tempStr = xmlGetProp(curNode, (const xmlChar*)"channelGreen")) != NULL)
				{
					sscanf ((char *)tempStr, "%d", &cGreen);
					xmlFree (tempStr);
				}
				if ((tempStr = xmlGetProp(curNode, (const xmlChar*)"redOut")) != NULL)
				{
					sscanf ((char *)tempStr, "%d", &redOut);
					xmlFree (tempStr);
				}
				if ((tempStr = xmlGetProp(curNode, (const xmlChar*)"greenOut")) != NULL)
				{
					sscanf ((char *)tempStr, "%d", &greenOut);
					xmlFree (tempStr);
				}
				if (ident != -1 && sType != -1 && redOut != -1 && greenOut != -1)
				{
					pointCtrl -> signalStates[sFound].ident = ident;
					pointCtrl -> signalStates[sFound].type = sType;
					pointCtrl -> signalStates[sFound].servoChannel = channel;
					pointCtrl -> signalStates[sFound].channelRed = cRed;
					pointCtrl -> signalStates[sFound].channelGreen = cGreen;
					pointCtrl -> signalStates[sFound].redOut = redOut;
					pointCtrl -> signalStates[sFound].greenOut = greenOut;
					pointCtrl -> signalStates[sFound].state = 0;
					++sFound;
				}
			}
		}
	}
	pointCtrl -> pointCount = pFound;
	pointCtrl -> signalCount = sFound;
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  P A R S E  T R E E                                                                                                *
 *  ==================                                                                                                *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  \brief Parse the xml to read the config.
 *  \param pointCtrl Save data hear.
 *  \param inNode The root node.
 *  \param level Current level, starts at root.
 *  \result None.
 */
void parseTree(pointCtrlDef *pointCtrl, xmlNode *inNode, int level)
{
	xmlNode *curNode = NULL;

	for (curNode = inNode; curNode; curNode = curNode->next)
	{
		if (curNode->type == XML_ELEMENT_NODE)
		{
			xmlChar *tempStr;
			char clientName[41] = "";
			if (level == 0 && strcmp ((char *)curNode->name, "pointControl") == 0)
			{
				if ((tempStr = xmlGetProp(curNode, (const xmlChar*)"server")) != NULL)
				{
					strcpy (pointCtrl -> serverName, (char *)tempStr);
					xmlFree (tempStr);
				}
				if ((tempStr = xmlGetProp(curNode, (const xmlChar*)"port")) != NULL)
				{
					sscanf ((char *)tempStr, "%d", &pointCtrl -> serverPort);
					xmlFree (tempStr);
				}
				if ((tempStr = xmlGetProp(curNode, (const xmlChar*)"ipver")) != NULL)
				{
					sscanf ((char *)tempStr, "%d", &pointCtrl -> ipVersion);
					xmlFree (tempStr);
				}
				if ((tempStr = xmlGetProp(curNode, (const xmlChar*)"timeout")) != NULL)
				{
					sscanf ((char *)tempStr, "%d", &pointCtrl -> conTimeout);
					xmlFree (tempStr);
				}
				if ((tempStr = xmlGetProp(curNode, (const xmlChar*)"clientIdent")) != NULL)
				{
					sscanf ((char *)tempStr, "%d", &pointCtrl -> clientID);
					xmlFree (tempStr);
				}
				parseTree (pointCtrl, curNode -> children, 1);
			}
			else if (level == 1 && strcmp ((char *)curNode->name, "pointDaemon") == 0)
			{
				int readIdent = -1, pointCount = 0, signalCount = 0;

				if ((tempStr = xmlGetProp(curNode, (const xmlChar*)"ident")) != NULL)
				{
					sscanf ((char *)tempStr, "%d", &readIdent);
					xmlFree (tempStr);
				}
				if ((tempStr = xmlGetProp(curNode, (const xmlChar*)"pCount")) != NULL)
				{
					sscanf ((char *)tempStr, "%d", &pointCount);
					xmlFree (tempStr);
				}
				if ((tempStr = xmlGetProp(curNode, (const xmlChar*)"sCount")) != NULL)
				{
					sscanf ((char *)tempStr, "%d", &signalCount);
					xmlFree (tempStr);
				}
				if ((tempStr = xmlGetProp(curNode, (const xmlChar*)"client")) != NULL)
				{
					strncpy (clientName, (char *)tempStr, 40);
					xmlFree (tempStr);
				}
				if (readIdent == pointCtrl -> clientID)
				{
					strncpy (pointCtrl -> clientName, clientName, 41);
					processPoints (pointCtrl, curNode -> children, pointCount, signalCount);
				}
			}
		}
	}
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  P A R S E  M E M O R Y  X M L                                                                                     *
 *  =============================                                                                                     *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  \brief If all else fails load the default track from memory.
 *  \param pointCtrl Parse the contents of memory.
 *  \param buffer Pointer to a buffer, NULL will use default buffer.
 *  \result 1 if track was loaded.
 */
int parseMemoryXML (pointCtrlDef *pointCtrl, char *buffer)
{
	int retn = 0;
	xmlDoc *doc = NULL;
	xmlNode *rootElement = NULL;
	xmlChar *xmlBuffer = xmlCharStrndup (buffer, strlen (buffer));

	if (xmlBuffer != NULL)
	{
		if ((doc = xmlParseDoc (xmlBuffer)) != NULL)
		{
			if ((rootElement = xmlDocGetRootElement(doc)) != NULL)
				parseTree (pointCtrl, rootElement, 0);
			if (pointCtrl -> pointCount > 0)
				retn = 1;

			xmlFreeDoc(doc);
		}
		xmlFree (xmlBuffer);
	}
	xmlCleanupParser();
	return retn;
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  U P D A T E  P O I N T                                                                                            *
 *  ======================                                                                                            *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  \brief Update the state of a point.
 *  \param pointCtrl Current point states.
 *  \param handle Socket handle to send reply.
 *  \param server Selected server (check it is for us).
 *  \param point Identity of the point to change.
 *  \param state New state for the point.
 *  \result None.
 */
void updatePoint (pointCtrlDef *pointCtrl, int handle, int server, int point, int state)
{
	if (server == pointCtrl -> clientID && servoFD != -1)
	{
		int i;
		for (i = 0; i < pointCtrl -> pointCount; ++i)
		{
			if (pointCtrl -> pointStates[i].ident == point)
			{
				char tempBuff[81];

				pthread_mutex_lock (&priorityMutex);
				servoMove (&pointCtrl -> pointStates[i].servoState, state ?
						pointCtrl -> pointStates[i].turnoutPos :
						pointCtrl -> pointStates[i].defaultPos,
						++curPriority);
				pthread_mutex_unlock (&priorityMutex);
				pointCtrl -> pointStates[i].state = state;
				sprintf (tempBuff, "<y %d %d %d>", server, point, state);
				SendSocket (handle, tempBuff, strlen (tempBuff));
				break;
			}
		}
	}
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  U P D A T E  S I G N A L                                                                                          *
 *  ========================                                                                                          *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  \brief Update the state of a signal.
 *  \param pointCtrl Pointer to the config.
 *  \param handle Hande of the controller.
 *  \param server Number of the server.
 *  \param signal Number of the signal.
 *  \param state New signal state.
 *  \result None.
 */
void updateSignal (pointCtrlDef *pointCtrl, int handle, int server, int signal, int state)
{
	if (server == pointCtrl -> clientID && servoFD != -1)
	{
		int i;
		for (i = 0; i < pointCtrl -> signalCount; ++i)
		{
			if (pointCtrl -> signalStates[i].ident == signal)
			{
				char tempBuff[81];

				if (pointCtrl -> signalStates[i].type == 0)
				{
#ifdef HAVE_WIRINGPI_H
					if (state == 1)
					{
						pwmWrite(PIN_BASE + pointCtrl -> signalStates[i].channelGreen, 0);
						delay(PWM_DELAY);
						pwmWrite(PIN_BASE + pointCtrl -> signalStates[i].channelRed, pointCtrl -> signalStates[i].redOut);
					}
					else if (state == 2)
					{
						pwmWrite(PIN_BASE + pointCtrl -> signalStates[i].channelRed, 0);
						delay(PWM_DELAY);
						pwmWrite(PIN_BASE + pointCtrl -> signalStates[i].channelGreen, pointCtrl -> signalStates[i].greenOut);
					}
					else
					{
						pwmWrite(PIN_BASE + pointCtrl -> signalStates[i].channelRed, 0);
						delay(PWM_DELAY);
						pwmWrite(PIN_BASE + pointCtrl -> signalStates[i].channelGreen, 0);
					}
					delay(PWM_DELAY);
#endif
				}
				else if (pointCtrl -> signalStates[i].type == 1)
				{

					pthread_mutex_lock (&priorityMutex);
					servoMove (&pointCtrl -> signalStates[i].servoState, state == 0 ? 0 : state == 1 ?
							pointCtrl -> signalStates[i].redOut :
							pointCtrl -> signalStates[i].greenOut,
							++curPriority);
					pthread_mutex_unlock (&priorityMutex);
				}
				pointCtrl -> signalStates[i].state = state;
				sprintf (tempBuff, "<x %d %d %d>", server, signal, state);
				SendSocket (handle, tempBuff, strlen (tempBuff));
				break;
			}
		}
	}
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  U P D A T E  A L L  P O I N T S                                                                                   *
 *  ===============================                                                                                   *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  \brief Send out a message with the state of all the points.
 *  \param pointCtrl Current point states.
 *  \param handle Socket handle to send reply.
 *  \result None.
 */
void updateAllPoints (pointCtrlDef *pointCtrl, int handle)
{
	int i;
	char tempBuff[81];

	for (i = 0; i < pointCtrl -> pointCount; ++i)
	{
		sprintf (tempBuff, "<y %d %d %d>", pointCtrl -> clientID,
				pointCtrl -> pointStates[i].ident,
				pointCtrl -> pointStates[i].state);
		SendSocket (handle, tempBuff, strlen (tempBuff));
	}
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  U P D A T E  A L L  S I G N A L S                                                                                 *
 *  =================================                                                                                 *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  \brief Send out the state of all the signals.
 *  \param pointCtrl Pointer to the config.
 *  \param handle Hande of the controller.
 *  \result None.
 */
void updateAllSignals (pointCtrlDef *pointCtrl, int handle)
{
	int i;
	char tempBuff[81];

	for (i = 0; i < pointCtrl -> signalCount; ++i)
	{
		sprintf (tempBuff, "<x %d %d %d>", pointCtrl -> clientID,
				pointCtrl -> signalStates[i].ident,
				pointCtrl -> signalStates[i].state);
		SendSocket (handle, tempBuff, strlen (tempBuff));
	}
}


/**********************************************************************************************************************
 *                                                                                                                    *
 *  C H E C K  R E C V  B U F F E R                                                                                   *
 *  ===============================                                                                                   *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  \brief Process the received buffer looking for point commands.
 *  \param pointCtrl Current point states.
 *  \param handle Socket handle.
 *  \param buffer Buffer received from the network.
 *  \param len Lenght of the buffer received.
 *  \result None.
 */
void checkRecvBuffer (pointCtrlDef *pointCtrl, int handle, char *buffer, int len)
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
				words[++wordNum][0] = 0;

/*------------------------------------------------------------------*
			for (l = 0; l < wordNum; ++l)
			{
				printf ("[%s]", words[l]);
			}
			printf ("(%d)\n", wordNum);
 *------------------------------------------------------------------*/
			/* Point control */
			if (words[0][0] == 'Y' && words[0][1] == 0)
			{
				if (wordNum == 4)
				{
					int server = atoi (words[1]);
					int point = atoi (words[2]);
					int state = atoi (words[3]);
					updatePoint (pointCtrl, handle, server, point, state);
				}
				else
				{
					updateAllPoints (pointCtrl, handle);
				}
			}
			else if (words[0][0] == 'X' && words[0][1] == 0)
			{
				if (wordNum == 4)
				{
					int server = atoi (words[1]);
					int signal = atoi (words[2]);
					int state = atoi (words[3]);
					updateSignal (pointCtrl, handle, server, signal, state);
				}
				else
				{
					updateAllSignals (pointCtrl, handle);
				}
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
			j = 40;

		++i;
	}
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  C H E C K  P O I N T S  S T A T E                                                                                 *
 *  =================================                                                                                 *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  \brief Turn off the point after it moves.
 *  \param pointPtr Point configuration pointer.
 *  \result None.
 */
void *checkPointsState (void *pointPtr)
{
	pointCtrlDef *pointCtrl = (pointCtrlDef *)pointPtr;

	while (running)
	{
		int i, selServo = -1, selServoPrio = -1, selType = -1;

		for (i = 0; i < pointCtrl -> pointCount; ++i)
		{
			int curPrio = pointCtrl -> pointStates[i].servoState.priority;
			if (curPrio > 0)
			{
				if (selServo == -1 || curPrio < selServoPrio)
				{
					selType = 0;
					selServo = i;
					selServoPrio = curPrio;
				}
			}
		}
		for (i = 0; i < pointCtrl -> signalCount; ++i)
		{
			int curPrio = pointCtrl -> signalStates[i].servoState.priority;
			if (curPrio > 0)
			{
				if (selServo == -1 || curPrio < selServoPrio)
				{
					selType = 1;
					selServo = i;
					selServoPrio = curPrio;
				}
			}
		}
		if (selType != -1)
		{
			if (selType == 0)
				servoUpdate (&pointCtrl -> pointStates[selServo].servoState);
			else
				servoUpdate (&pointCtrl -> signalStates[selServo].servoState);
		}
		else
		{
			pthread_mutex_lock (&priorityMutex);
			curPriority = 0;
			pthread_mutex_unlock (&priorityMutex);
		}
		usleep (50000);
	}
	return NULL;
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  P O I N T  C O N T R O L  S E T U P                                                                               *
 *  ===================================                                                                               *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  \brief Set up the I2C interface for controlling the servos.
 *  \param pointCtrl Point configuration.
 *  \result 1 if all went OK.
 */
int pointControlSetup (pointCtrlDef *pointCtrl)
{
	int i;

#ifdef HAVE_WIRINGPI_H
	wiringPiSetup();
	if ((servoFD = pca9685Setup(PIN_BASE, 0x40, HERTZ)) < 0)
	{
		putLogMessage (LOG_ERR, "Error setting up point control");
		return 0;
	}
	pca9685PWMReset (servoFD);
#endif

	for (i = 0; i < pointCtrl -> pointCount; ++i)
	{
		servoInit (&pointCtrl -> pointStates[i].servoState, pointCtrl -> pointStates[i].servoChannel, 
				pointCtrl -> pointStates[i].defaultPos);
	}
	for (i = 0; i < pointCtrl -> signalCount; ++i)
	{
		if (pointCtrl -> signalStates[i].type == 0)
		{
#ifdef HAVE_WIRINGPI_H
			pwmWrite (PIN_BASE + pointCtrl -> signalStates[i].channelRed, pointCtrl -> signalStates[i].redOut);
			delay (PWM_DELAY);
			pwmWrite (PIN_BASE + pointCtrl -> signalStates[i].channelGreen, 0);
			delay (PWM_DELAY);
#endif
		}
		else if (pointCtrl -> signalStates[i].type == 1)
		{
			servoInit (&pointCtrl -> signalStates[i].servoState, pointCtrl -> signalStates[i].servoChannel, 
					pointCtrl -> signalStates[i].redOut);
		}		
		pointCtrl -> signalStates[i].state = 1;
	}

	pthread_mutex_init (&priorityMutex, NULL);
	if (pthread_create (&threadHandle, NULL, checkPointsState, pointCtrl) != 0)
	{
		return 0;
	}
	return 1;
}

