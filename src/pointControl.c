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

#include "socketC.h"
#include "pointControl.h"

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
 *  \param count Expected number of points.
 *  \result None.
 */
void processPoints (pointCtrlDef *pointCtrl, xmlNode *inNode, int count)
{
	int loop = 0;
	xmlChar *identStr, *defaultStr, *turnoutStr;
	xmlNode *curNode = NULL;

	if ((pointCtrl -> pointStates = (pointStateDef *)malloc (count * sizeof (pointStateDef))) == NULL)
	{
		return;
	}

	memset (pointCtrl -> pointStates, 0, count * sizeof (pointStateDef));

	for (curNode = inNode; curNode && loop < count; curNode = curNode->next)
	{
		if (curNode->type == XML_ELEMENT_NODE)
		{
			if (strcmp ((char *)curNode->name, "point") == 0)
			{
				if ((identStr = xmlGetProp(curNode, (const xmlChar*)"ident")) != NULL)
				{
					if ((defaultStr = xmlGetProp(curNode, (const xmlChar*)"default")) != NULL)
					{
						if ((turnoutStr = xmlGetProp(curNode, (const xmlChar*)"turnout")) != NULL)
						{
							int ident = -1, defaultPos = -1, turnoutPos = -1;

							sscanf ((char *)identStr, "%d", &ident);
							sscanf ((char *)defaultStr, "%d", &defaultPos);
							sscanf ((char *)turnoutStr, "%d", &turnoutPos);

							if (ident != -1 && defaultPos != -1 && turnoutPos != -1)
							{
								pointCtrl -> pointStates[loop].ident = ident;
								pointCtrl -> pointStates[loop].defaultPos = defaultPos;
								pointCtrl -> pointStates[loop].turnoutPos = turnoutPos;
								++loop;
							}
							xmlFree(turnoutStr);
						}
						xmlFree(defaultStr);
					}
					xmlFree(identStr);
				}
			}
		}
	}
	pointCtrl -> pointCount = loop;
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
	int readIdent = 0, readCount = 0;
	xmlNode *curNode = NULL;

	for (curNode = inNode; curNode; curNode = curNode->next)
	{
		if (curNode->type == XML_ELEMENT_NODE)
		{
			if (level == 0 && strcmp ((char *)curNode->name, "points") == 0)
			{
				parseTree (pointCtrl, curNode -> children, 1);
			}
			else if (level == 1 && strcmp ((char *)curNode->name, "server") == 0)
			{
				xmlChar *identStr, *portStr, *countStr;
				if ((identStr = xmlGetProp(curNode, (const xmlChar*)"ident")) != NULL)
				{
					sscanf ((char *)identStr, "%d", &readIdent);
					xmlFree (identStr);
				}
				if ((portStr = xmlGetProp(curNode, (const xmlChar*)"port")) != NULL)
				{
					sscanf ((char *)portStr, "%d", &pointCtrl -> serverPort);
					xmlFree (portStr);
				}
				if ((countStr = xmlGetProp(curNode, (const xmlChar*)"count")) != NULL)
				{
					sscanf ((char *)countStr, "%d", &readCount);
					xmlFree (countStr);
				}
				if (readCount > 0 && readIdent == pointCtrl -> server)
				{
					processPoints (pointCtrl, curNode -> children, readCount);
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
			{
				parseTree (pointCtrl, rootElement, 0);
			}
			if (pointCtrl -> pointCount > 0)
			{
				retn = 1;
			}
			xmlFreeDoc(doc);
		}
		xmlFree (xmlBuffer);
	}
	xmlCleanupParser();
	return retn;
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  C H E C K  R E C V  B U F F E R                                                                                   *
 *  ===============================                                                                                   *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  \brief Process the received buffer looking for point commands.
 *  \param handle Socket handle.
 *  \param buffer Buffer received from the network.
 *  \param len Lenght of the buffer received.
 *  \result None.
 */
void checkRecvBuffer (int handle, char *buffer, int len)
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
			/* Point control */
			if (words[0][0] == 'Y' && words[0][1] == 0 && wordNum == 4)
			{
				char tempBuff[81];
				int server = atoi (words[1]);
				int point = atoi (words[2]);
				int state = atoi (words[3]);
				sprintf (tempBuff, "<y %d %d %d>", server, point, state);
				SendSocket (handle, tempBuff, strlen (tempBuff));
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


