/**********************************************************************************************************************
 *                                                                                                                    *
 *  T R A I N  T R A C K . C                                                                                          *
 *  ========================                                                                                          *
 *                                                                                                                    *
 *  Copyright (c) 2023 Chris Knight                                                                                   *
 *                                                                                                                    *
 *  File trainTrack.c part of TrainControl is free software: you can redistribute it and/or modify it under the       *
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
 *  \brief Process reading the track config file.
 */
#include <gtk/gtk.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <string.h>

#include "trainControl.h"
#include "socketC.h"

/**********************************************************************************************************************
 *                                                                                                                    *
 **********************************************************************************************************************/
static const char *tmpConfig = "/tmp/trainconfig.xml";
static const char *memoryXML =
"<track name=\"Simple Track\" server=\"127.0.0.1\" port=\"30330\" ipver=\"1\" device=\"/dev/ttyACM0\" idleOff=\"15\">"\
"<trains count=\"1\">"\
"<train num=\"1234\" ident=\"3\" desc=\"Train\"/>"\
"</trains>"\
"<cells rows=\"4\" cols=\"4\" size=\"50\">"\
"<cellRow row=\"0\">"\
"<cell col=\"1\" layout=\"20\"/>"\
"<cell col=\"2\" layout=\"129\"/>"\
"</cellRow>"\
"<cellRow row=\"1\">"\
"<cell col=\"0\" layout=\"72\"/>"\
"<cell col=\"3\" layout=\"40\"/>"\
"</cellRow>"\
"<cellRow row=\"2\">"\
"<cell col=\"0\" layout=\"130\"/>"\
"<cell col=\"3\" layout=\"18\"/>"\
"</cellRow>"\
"<cellRow row=\"3\">"\
"<cell col=\"1\" layout=\"36\"/>"\
"<cell col=\"2\" layout=\"65\"/>"\
"</cellRow>"\
"</cells>"\
"</track>";

/**********************************************************************************************************************
 *                                                                                                                    *
 *  P R O C E S S  F U N C T I O N                                                                                    *
 *  ==============================                                                                                    *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  \brief Read in details of the functions.
 *  \param trackCtrl Pointer to the track config.
 *  \param inNode Current node.
 *  \param count Number of functions to read.
 *  \param train Pointer to the train.
 *  \result None.
 */
void processFunction (trackCtrlDef *trackCtrl, xmlNode *inNode, int count, trainCtrlDef *train)
{
	int loop = 0;
	xmlNode *curNode = NULL;

	if ((train -> trainFunc = (trainFuncDef *)malloc (count * sizeof (trainFuncDef))) == NULL)
		return;

	memset (train -> funcState, 0, 100);
	memset (train -> trainFunc, 0, count * sizeof (trainFuncDef));

	for (curNode = inNode; curNode; curNode = curNode->next)
	{
		if (curNode->type == XML_ELEMENT_NODE)
		{
			if (strcmp ((char *)curNode->name, "function") == 0)
			{
				xmlChar *idStr;

				if ((idStr = xmlGetProp(curNode, (const xmlChar*)"ident")) != NULL)
				{
					int trigger = 0;
					xmlChar *descStr, *triggerStr;

					if ((triggerStr = xmlGetProp(curNode, (const xmlChar*)"trigger")) != NULL)
					{
						sscanf ((char *)triggerStr, "%d", &trigger);
						if (trigger != 0)
							trigger = 1;

						xmlFree(triggerStr);
					}
					if ((descStr = xmlGetProp(curNode, (const xmlChar*)"desc")) != NULL)
					{
						int id = -1;

						sscanf ((char *)idStr, "%d", &id);

						if (id != -1 && loop < count)
						{
							train -> trainFunc[loop].funcID = id;
							train -> trainFunc[loop].trigger = trigger;
							strncpy (train -> trainFunc[loop].funcDesc, (char *)descStr, 40);
							++loop;
						}
						xmlFree(descStr);
					}
					xmlFree(idStr);
				}
			}
		}
	}
	train -> funcCount = loop;
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  P R O C E S S  F U N C T I O N S                                                                                  *
 *  ================================                                                                                  *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  \brief See if a train has functions and alloc space if it does.
 *  \param trackCtrl Pointer to the track config.
 *  \param inNode Current node.
 *  \param train Pointer to the train.
 *  \result None.
 */
void processFunctions (trackCtrlDef *trackCtrl, xmlNode *inNode, trainCtrlDef *train)
{
	xmlNode *curNode = NULL;

	for (curNode = inNode; curNode; curNode = curNode->next)
	{
		if (curNode->type == XML_ELEMENT_NODE)
		{
			if (strcmp ((char *)curNode->name, "functions") == 0)
			{
				int count = -1;
				xmlChar *tempStr;

				if ((tempStr = xmlGetProp(curNode, (const xmlChar*)"count")) != NULL)
				{
					sscanf ((char *)tempStr, "%d", &count);
					xmlFree(tempStr);

					if (count > 0)
						processFunction (trackCtrl, curNode -> children, count, train);
				}
			}
		}
	}
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  P R O C E S S  T R A I N S                                                                                        *
 *  ==========================                                                                                        *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  \brief Process the network configuration section.
 *  \param trackCtrl Which is the active track.
 *  \param inNode Pointer to the network section.
 *  \param count Process each of the train lines.
 *  \result None.
 */
void processTrains (trackCtrlDef *trackCtrl, xmlNode *inNode, int count)
{
	int loop = 0;
	xmlChar *tempStr, *descStr;
	xmlNode *curNode = NULL;

	if ((trackCtrl -> trainCtrl = (trainCtrlDef *)malloc (count * sizeof (trainCtrlDef))) == NULL)
		return;

	memset (trackCtrl -> trainCtrl, 0, count * sizeof (trainCtrlDef));

	for (curNode = inNode; curNode && loop < count; curNode = curNode->next)
	{
		if (curNode->type == XML_ELEMENT_NODE)
		{
			if (strcmp ((char *)curNode->name, "train") == 0)
			{
				int num = -1, id = -1, slow = 10;
				if ((tempStr = xmlGetProp(curNode, (const xmlChar*)"num")) != NULL)
				{
					sscanf ((char *)tempStr, "%d", &num);
					xmlFree(tempStr);
					if ((tempStr = xmlGetProp(curNode, (const xmlChar*)"ident")) != NULL)
					{
						sscanf ((char *)tempStr, "%d", &id);
						xmlFree(tempStr);
						if ((descStr = xmlGetProp(curNode, (const xmlChar*)"desc")) != NULL)
						{
							if ((tempStr = xmlGetProp(curNode, (const xmlChar*)"slow")) != NULL)
							{
								sscanf ((char *)tempStr, "%d", &slow);
								xmlFree(tempStr);
							}
							if (num != -1 && id != -1 && loop < count)
							{
								trackCtrl -> trainCtrl[loop].trainReg = loop + 1;
								trackCtrl -> trainCtrl[loop].trainID = id;
								trackCtrl -> trainCtrl[loop].trainNum = num;
								trackCtrl -> trainCtrl[loop].slowSpeed = slow;
								strncpy (trackCtrl -> trainCtrl[loop].trainDesc, (char *)descStr, 40);
								processFunctions (trackCtrl, curNode -> children, &trackCtrl -> trainCtrl[loop]);
								++loop;
							}
							xmlFree(descStr);
						}
					}
				}
			}
		}
	}
	trackCtrl -> trainCount = loop;
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  P R O C E S S  T H R O T T L E S                                                                                  *
 *  ================================                                                                                  *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  \brief Process the trottle config in the XML.
 *  \param trackCtrl Track config.
 *  \param inNode Current node.
 *  \param count Number of throttles (max).
 *  \result None.
 */
void processThrottles (trackCtrlDef *trackCtrl, xmlNode *inNode, int count)
{
	int loop = 0;
	xmlChar *tempStr;
	xmlNode *curNode = NULL;

	if ((trackCtrl -> throttles = (throttleDef *)malloc (count * sizeof (throttleDef))) == NULL)
		return;

	memset (trackCtrl -> throttles, 0, count * sizeof (throttleDef));

	for (curNode = inNode; curNode && loop < count; curNode = curNode->next)
	{
		if (curNode->type == XML_ELEMENT_NODE)
		{
			if (strcmp ((char *)curNode->name, "throttle") == 0)
			{
				int axis = -1, button = -1, train = -1, zeroHigh = 0;

				if ((tempStr = xmlGetProp(curNode, (const xmlChar*)"zero")) != NULL)
				{
					if (tempStr[0] == 'H')
						zeroHigh = 1;
					xmlFree(tempStr);
				}
				if ((tempStr = xmlGetProp(curNode, (const xmlChar*)"train")) != NULL)
				{
					sscanf ((char *)tempStr, "%d", &train);
					xmlFree(tempStr);
				}
				if ((tempStr = xmlGetProp(curNode, (const xmlChar*)"axis")) != NULL)
				{
					sscanf ((char *)tempStr, "%d", &axis);
					xmlFree(tempStr);
					if ((tempStr = xmlGetProp(curNode, (const xmlChar*)"button")) != NULL)
					{
						sscanf ((char *)tempStr, "%d", &button);
						xmlFree(tempStr);
						if (axis != -1 && button != -1 && loop < count)
						{
							trackCtrl -> throttles[loop].axis = axis;
							trackCtrl -> throttles[loop].button = button;
							trackCtrl -> throttles[loop].defTrain = train;
							trackCtrl -> throttles[loop].zeroHigh = zeroHigh;
							++loop;
						}
					}
				}
			}
		}
	}
	trackCtrl -> throttleCount = loop;
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  P R O C E S S  C E L L                                                                                            *
 *  ======================                                                                                            *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  \brief Process each of the cell lines .
 *  \param trackCtrl Which is the active track.
 *  \param inNode In node to process.
 *  \param rowNum Number of the current row.
 *  \result None.
 */
void processCell (trackCtrlDef *trackCtrl, xmlNode *inNode, int rowNum)
{
	xmlNode *curNode = NULL;

	for (curNode = inNode; curNode; curNode = curNode->next)
	{
		if (curNode->type == XML_ELEMENT_NODE)
		{
			if (strcmp ((char *)curNode->name, "cell") == 0)
			{
				xmlChar *tempStr;
				if ((tempStr = xmlGetProp(curNode, (const xmlChar*)"col")) != NULL)
				{
					int colNum = -1;
					sscanf ((char *)tempStr, "%d", &colNum);
					xmlFree(tempStr);

					if (colNum > -1)
					{
						int i, pointState = 0, point = 0;
						int posn = (rowNum * trackCtrl -> trackLayout -> trackCols) + colNum;
						memset (&trackCtrl -> trackLayout -> trackCells[posn], 0, sizeof (trackCellDef));

						if ((tempStr = xmlGetProp(curNode, (const xmlChar*)"layout")) != NULL)
						{
							sscanf ((char *)tempStr, "%hu", &trackCtrl -> trackLayout -> trackCells[posn].layout);
							xmlFree(tempStr);
						}
						if ((tempStr = xmlGetProp(curNode, (const xmlChar*)"point")) != NULL)
						{
							sscanf ((char *)tempStr, "%d", &point);
							xmlFree(tempStr);
							trackCtrl -> trackLayout -> trackCells[posn].point.point = point;
						}
						if ((tempStr = xmlGetProp(curNode, (const xmlChar*)"state")) != NULL)
						{
							sscanf ((char *)tempStr, "%d", &pointState);
							xmlFree(tempStr);
							trackCtrl -> trackLayout -> trackCells[posn].point.pointDef =
									trackCtrl -> trackLayout -> trackCells[posn].point.state = pointState;
						}
						if ((tempStr = xmlGetProp(curNode, (const xmlChar*)"link")) != NULL)
						{
							sscanf ((char *)tempStr, "%hu", &trackCtrl -> trackLayout -> trackCells[posn].point.link);
							xmlFree(tempStr);
						}
						if ((tempStr = xmlGetProp(curNode, (const xmlChar*)"server")) != NULL)
						{
							sscanf ((char *)tempStr, "%hu", &trackCtrl -> trackLayout -> trackCells[posn].point.server);
							xmlFree(tempStr);
						}
						if ((tempStr = xmlGetProp(curNode, (const xmlChar*)"ident")) != NULL)
						{
							sscanf ((char *)tempStr, "%hu", &trackCtrl -> trackLayout -> trackCells[posn].point.ident);
							xmlFree(tempStr);
						}
						if ((tempStr = xmlGetProp(curNode, (const xmlChar*)"signal")) != NULL)
						{
							sscanf ((char *)tempStr, "%hu", &trackCtrl -> trackLayout -> trackCells[posn].signal.signal);
							xmlFree(tempStr);
						}
						if ((tempStr = xmlGetProp(curNode, (const xmlChar*)"sserver")) != NULL)
						{
							sscanf ((char *)tempStr, "%hu", &trackCtrl -> trackLayout -> trackCells[posn].signal.server);
							xmlFree(tempStr);
						}
						if ((tempStr = xmlGetProp(curNode, (const xmlChar*)"sident")) != NULL)
						{
							sscanf ((char *)tempStr, "%hu", &trackCtrl -> trackLayout -> trackCells[posn].signal.ident);
							xmlFree(tempStr);
						}
						for (i = 0; i < 8 && point && pointState == 0; ++i)
						{
							if (point & (1 << i))
							{
								trackCtrl -> trackLayout -> trackCells[posn].point.pointDef =
										trackCtrl -> trackLayout -> trackCells[posn].point.state = (1 << i);
								break;
							}
						}
					}
				}
			}
		}
	}
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  P R O C E S S  C E L L S                                                                                          *
 *  ========================                                                                                          *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  \brief Process the overload table section.
 *  \param trackCtrl Which is the active track.
 *  \param inNode Pointer to the overload table section.
 *  \param rows Process each of the cells lines (made up of cells) .
 *  \param cols In node to process.
 *  \param size The size of a cell.
 *  \result None.
 */
void processCells (trackCtrlDef *trackCtrl, xmlNode *inNode, int rows, int cols, int size)
{
	xmlChar *rowStr;
	xmlNode *curNode = NULL;

	if ((trackCtrl -> trackLayout = (trackLayoutDef *)malloc (sizeof (trackLayoutDef))) == NULL)
		return;

	memset (trackCtrl -> trackLayout, 0, sizeof (trackLayoutDef));
	trackCtrl -> trackLayout -> trackRows = rows;
	trackCtrl -> trackLayout -> trackCols = cols;
	trackCtrl -> trackLayout -> trackSize = size;

	if ((trackCtrl -> trackLayout -> trackCells = (trackCellDef *)malloc (rows * cols * sizeof (trackCellDef))) == NULL)
	{
		free (trackCtrl -> trackLayout);
		trackCtrl -> trackLayout = NULL;
		return;
	}
	memset (trackCtrl -> trackLayout -> trackCells, 0, rows * cols * sizeof (trackCellDef));

	for (curNode = inNode; curNode; curNode = curNode->next)
	{
		if (curNode->type == XML_ELEMENT_NODE)
		{
			if (strcmp ((char *)curNode->name, "cellRow") == 0)
			{
				if ((rowStr = xmlGetProp(curNode, (const xmlChar*)"row")) != NULL)
				{
					int rowNum = -1;
					sscanf ((char *)rowStr, "%d", &rowNum);
					xmlFree(rowStr);

					if (rowNum > -1)
						processCell (trackCtrl, curNode -> children, rowNum);
				}
			}
		}
	}
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  P A R S E  T R E E                                                                                                *
 *  ==================                                                                                                *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  \brief Parse the pcm.
 *  \param trackCtrl Which is the active track.
 *  \param inNode Pointer to the root section.
 *  \param level Have we already seen the start of the pcm section.
 *  \result None.
 */
void parseTree(trackCtrlDef *trackCtrl, xmlNode *inNode, int level)
{
	xmlNode *curNode = NULL;

	for (curNode = inNode; curNode; curNode = curNode->next)
	{
		if (curNode->type == XML_ELEMENT_NODE)
		{
			if (level == 0 && strcmp ((char *)curNode->name, "track") == 0)
			{
				xmlChar *tempStr;
				if ((tempStr = xmlGetProp(curNode, (const xmlChar*)"name")) != NULL)
				{
					strncpy (trackCtrl -> trackName, (char *)tempStr, 80);
					xmlFree (tempStr);
				}
				if ((tempStr = xmlGetProp(curNode, (const xmlChar*)"server")) != NULL)
				{
					strncpy (trackCtrl -> server, (char *)tempStr, 80);
					xmlFree (tempStr);
				}
				if ((tempStr = xmlGetProp(curNode, (const xmlChar*)"port")) != NULL)
				{
					sscanf ((char *)tempStr, "%d", &trackCtrl -> serverPort);
					xmlFree (tempStr);
				}
				if ((tempStr = xmlGetProp(curNode, (const xmlChar*)"point")) != NULL)
				{
					sscanf ((char *)tempStr, "%d", &trackCtrl -> pointPort);
					xmlFree (tempStr);
				}
				if ((tempStr = xmlGetProp(curNode, (const xmlChar*)"config")) != NULL)
				{
					sscanf ((char *)tempStr, "%d", &trackCtrl -> configPort);
					xmlFree (tempStr);
				}
				if ((tempStr = xmlGetProp(curNode, (const xmlChar*)"timeout")) != NULL)
				{
					sscanf ((char *)tempStr, "%d", &trackCtrl -> conTimeout);
					xmlFree (tempStr);
				}
				if ((tempStr = xmlGetProp(curNode, (const xmlChar*)"ipver")) != NULL)
				{
					sscanf ((char *)tempStr, "%d", &trackCtrl -> ipVersion);
					xmlFree (tempStr);
				}
				if ((tempStr = xmlGetProp(curNode, (const xmlChar*)"device")) != NULL)
				{
					strncpy (trackCtrl -> serialDevice, (char *)tempStr, 80);
					xmlFree (tempStr);
				}
				if ((tempStr = xmlGetProp(curNode, (const xmlChar*)"flags")) != NULL)
				{
					int flags = 0;
					sscanf ((char *)tempStr, "%d", &flags);
					trackCtrl -> flags |= flags;
					xmlFree (tempStr);
				}
				if ((tempStr = xmlGetProp(curNode, (const xmlChar*)"idleOff")) != NULL)
				{
					int idleOff = 0;
					sscanf ((char *)tempStr, "%d", &idleOff);
					trackCtrl -> idleOff = idleOff * 60;
					xmlFree (tempStr);
				}
				parseTree (trackCtrl, curNode -> children, 1);
			}
			else if (level == 1 && strcmp ((char *)curNode->name, "trains") == 0)
			{
				int count = -1;
				xmlChar *countStr;
				if ((countStr = xmlGetProp(curNode, (const xmlChar*)"count")) != NULL)
				{
					sscanf ((char *)countStr, "%d", &count);
					xmlFree(countStr);
					if (count > 0)
						processTrains (trackCtrl, curNode -> children, count);
				}
			}
			else if (level == 1 && strcmp ((char *)curNode->name, "pointServers") == 0)
			{
				int count = -1;
				xmlChar *countStr;
				if ((countStr = xmlGetProp(curNode, (const xmlChar*)"count")) != NULL)
				{
					sscanf ((char *)countStr, "%d", &count);
					xmlFree(countStr);
					if (count > 0)
					{
						if ((trackCtrl -> pointCtrl = (pointCtrlDef *)malloc (count * sizeof (pointCtrlDef))) != NULL)
						{
							int i;

							memset (trackCtrl -> pointCtrl, 0, count * sizeof (pointCtrlDef));
							for (i = 0; i < count; ++i)
								trackCtrl -> pointCtrl[i].intHandle = -1;

							trackCtrl -> pServerCount = count;
						}
					}
				}
			}
			else if (level == 1 && strcmp ((char *)curNode->name, "throttles") == 0)
			{
				int count = -1, jsNum = -1;
				xmlChar *countStr, *jsNumStr;
				if ((jsNumStr = xmlGetProp(curNode, (const xmlChar*)"num")) != NULL)
				{
					sscanf ((char *)jsNumStr, "%d", &jsNum);
					xmlFree(jsNumStr);
					if (jsNum >= 0)
						trackCtrl -> jStickNumber = jsNum;
				}
				if ((countStr = xmlGetProp(curNode, (const xmlChar*)"count")) != NULL)
				{
					sscanf ((char *)countStr, "%d", &count);
					xmlFree(countStr);
					if (count > 0)
					{
						processThrottles (trackCtrl, curNode -> children, count);
						pthread_mutex_init (&trackCtrl -> throttleMutex, NULL);
					}
				}
			}
			else if (level == 1 && strcmp ((char *)curNode->name, "cells") == 0)
			{
				int rows = -1, cols = -1, size = 40;
				xmlChar *rowsStr;
				if ((rowsStr = xmlGetProp(curNode, (const xmlChar*)"rows")) != NULL)
				{
					xmlChar *colsStr;
					if ((colsStr = xmlGetProp(curNode, (const xmlChar*)"cols")) != NULL)
					{
						xmlChar *sizeStr;
						if ((sizeStr = xmlGetProp(curNode, (const xmlChar*)"size")) != NULL)
						{
							sscanf ((char *)sizeStr, "%d", &size);
							xmlFree(sizeStr);
						}
						sscanf ((char *)rowsStr, "%d", &rows);
						sscanf ((char *)colsStr, "%d", &cols);
						xmlFree(colsStr);

						if (rows > 0 && cols > 0)
							processCells (trackCtrl, curNode -> children, rows, cols, size);
					}
					xmlFree(rowsStr);
				}
			}
		}
	}
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  P A R S E  T R A C K  X M L                                                                                       *
 *  ===========================                                                                                       *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  \brief Read in and process the configuration file.
 *  \param trackCtrl Which is the active track.
 *  \param fileName File name to read in.
 *  \param level Level to stop too much recursion.
 *  \result 1 if config read OK, 0 on error.
 */
int parseTrackXML (trackCtrlDef *trackCtrl, const char *fileName, int level)
{
	int retn = 0;
	xmlDoc *doc = NULL;
	xmlNode *rootElement = NULL;

	trackCtrl -> trainCount = 0;

	if ((doc = xmlParseFile (fileName)) == NULL)
	{
		printf ("Unable to open config file: %s\n", fileName);
	}
	else
	{
		if ((rootElement = xmlDocGetRootElement(doc)) != NULL)
			parseTree (trackCtrl, rootElement, 0);

		if (trackCtrl -> trackLayout != NULL && trackCtrl -> trainCtrl != NULL)
		{
			retn = 1;
		}
		else if (trackCtrl -> configPort > 0 && trackCtrl -> server[0] && level == 0)
		{
			int cfgSocket = -1;

			if ((cfgSocket = ConnectClientSocket (trackCtrl -> server, trackCtrl -> configPort,
					trackCtrl -> conTimeout, trackCtrl -> ipVersion, NULL)) != -1)
			{
				FILE *outFile = fopen (tmpConfig, "w+");

				if (outFile != NULL)
				{
					char buffer[1024];
					int bytesRead = 0, totalRead = 0;

					sleep (1);
					while ((bytesRead = RecvSocket (cfgSocket, buffer, 1024)) > 0)
					{
						fwrite (buffer, 1, bytesRead, outFile);
						totalRead += bytesRead;
					}
					fclose (outFile);
					if (totalRead)
						retn = parseTrackXML (trackCtrl, tmpConfig, 1);

					unlink (tmpConfig);
				}
				CloseSocket (&cfgSocket);
			}
		}
	}
	xmlFreeDoc(doc);
	xmlCleanupParser();
	return retn;
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  P A R S E  M E M O R Y  X M L                                                                                     *
 *  =============================                                                                                     *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  \brief If all else fails load the default track from memory.
 *  \param trackCtrl Where to read in to.
 *  \param buffer Pointer to a buffer, NULL will use default buffer.
 *  \result 1 if track was loaded.
 */
int parseMemoryXML (trackCtrlDef *trackCtrl, char *buffer)
{
	int retn = 0;
	xmlDoc *doc = NULL;
	xmlNode *rootElement = NULL;
	xmlChar *xmlBuffer = NULL;

	if (buffer == NULL)
		xmlBuffer = xmlCharStrndup (memoryXML, strlen (memoryXML));
	else
		xmlBuffer = xmlCharStrndup (buffer, strlen (buffer));

	if (xmlBuffer != NULL)
	{
		if ((doc = xmlParseDoc (xmlBuffer)) != NULL)
		{
			if ((rootElement = xmlDocGetRootElement(doc)) != NULL)
				parseTree (trackCtrl, rootElement, 0);

			if (trackCtrl -> trackLayout != NULL && trackCtrl -> trainCtrl != NULL)
				retn = 1;

			xmlFreeDoc(doc);
		}
		xmlFree (xmlBuffer);
	}
	xmlCleanupParser();
	return retn;
}

