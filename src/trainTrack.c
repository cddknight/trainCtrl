/**********************************************************************************************************************
 *                                                                                                                    *
 *  T R A I N  T R A C K . C                                                                                          *
 *  ========================                                                                                          *
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
"<track name=\"Simple Track\" server=\"127.0.0.1\" port=\"30330\" device=\"/dev/ttyACM0\">"\
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
	{
		return;
	}

	memset (train -> trainFunc, 0, count * sizeof (trainFuncDef));

	for (curNode = inNode; curNode; curNode = curNode->next)
	{
		if (curNode->type == XML_ELEMENT_NODE)
		{
			if (strcmp ((char *)curNode->name, "function") == 0)
			{
				xmlChar *idStr, *descStr, *slowStr;

				if ((idStr = xmlGetProp(curNode, (const xmlChar*)"ident")) != NULL)
				{
					if ((descStr = xmlGetProp(curNode, (const xmlChar*)"desc")) != NULL)
					{
						int id = -1;

						sscanf ((char *)idStr, "%d", &id);

						if (id != -1 && loop < count)
						{
							train -> trainFunc[loop].funcID = id;
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
				int count = -1, custom = 0;;
				xmlChar *countStr, *customStr;
				if ((countStr = xmlGetProp(curNode, (const xmlChar*)"count")) != NULL)
				{
					sscanf ((char *)countStr, "%d", &count);
					xmlFree(countStr);

					if ((customStr = xmlGetProp(curNode, (const xmlChar*)"custom")) != NULL)
					{
						sscanf ((char *)customStr, "%d", &custom);
						xmlFree(customStr);
					}
					if (custom != 0)
					{
						train -> funcCustom = 1;
					}
					if (count > 0)
					{
						processFunction (trackCtrl, curNode -> children, count, train);
					}
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
	xmlChar *numStr, *idStr, *descStr, *slowStr;
	xmlNode *curNode = NULL;

	if ((trackCtrl -> trainCtrl = (trainCtrlDef *)malloc (count * sizeof (trainCtrlDef))) == NULL)
	{
		return;
	}

	memset (trackCtrl -> trainCtrl, 0, count * sizeof (trainCtrlDef));

	for (curNode = inNode; curNode && loop < count; curNode = curNode->next)
	{
		if (curNode->type == XML_ELEMENT_NODE)
		{
			if (strcmp ((char *)curNode->name, "train") == 0)
			{
				if ((numStr = xmlGetProp(curNode, (const xmlChar*)"num")) != NULL)
				{
					if ((idStr = xmlGetProp(curNode, (const xmlChar*)"ident")) != NULL)
					{
						if ((descStr = xmlGetProp(curNode, (const xmlChar*)"desc")) != NULL)
						{
							int num = -1, id = -1, slow = 10;

							if ((slowStr = xmlGetProp(curNode, (const xmlChar*)"slow")) != NULL)
							{
								sscanf ((char *)slowStr, "%d", &slow);
								xmlFree(slowStr);
							}

							sscanf ((char *)numStr, "%d", &num);
							sscanf ((char *)idStr, "%d", &id);

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
						xmlFree(idStr);
					}
					xmlFree(numStr);
				}
			}
		}
	}
	trackCtrl -> trainCount = loop;
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
	xmlChar *colStr, *layoutStr, *pointStr, *linkStr, *pointStateStr, *serverStr, *identStr;

	for (curNode = inNode; curNode; curNode = curNode->next)
	{
		if (curNode->type == XML_ELEMENT_NODE)
		{
			if (strcmp ((char *)curNode->name, "cell") == 0)
			{
				if ((colStr = xmlGetProp(curNode, (const xmlChar*)"col")) != NULL)
				{
					int posn, i;
					int colNum = -1, layout = 0, point = 0, link = 0, pointState = 0, server = 0, ident = 0;
					sscanf ((char *)colStr, "%d", &colNum);

					if (colNum > -1)
					{
						posn = (rowNum * trackCtrl -> trackLayout -> trackCols) + colNum;

						if ((layoutStr = xmlGetProp(curNode, (const xmlChar*)"layout")) != NULL)
						{
							sscanf ((char *)layoutStr, "%d", &layout);
							xmlFree(layoutStr);
						}
						if ((pointStr = xmlGetProp(curNode, (const xmlChar*)"point")) != NULL)
						{
							sscanf ((char *)pointStr, "%d", &point);
							xmlFree(pointStr);
						}
						if ((linkStr = xmlGetProp(curNode, (const xmlChar*)"link")) != NULL)
						{
							sscanf ((char *)linkStr, "%d", &link);
							xmlFree(linkStr);
						}
						if ((pointStateStr = xmlGetProp(curNode, (const xmlChar*)"state")) != NULL)
						{
							sscanf ((char *)pointStateStr, "%d", &pointState);
							xmlFree(pointStateStr);
						}
						if ((serverStr = xmlGetProp(curNode, (const xmlChar*)"server")) != NULL)
						{
							sscanf ((char *)serverStr, "%d", &server);
							xmlFree(serverStr);
						}
						if ((identStr = xmlGetProp(curNode, (const xmlChar*)"ident")) != NULL)
						{
							sscanf ((char *)identStr, "%d", &ident);
							xmlFree(identStr);
						}
						trackCtrl -> trackLayout -> trackCells[posn].layout = layout;
						trackCtrl -> trackLayout -> trackCells[posn].point = point;
						trackCtrl -> trackLayout -> trackCells[posn].link = link;
						trackCtrl -> trackLayout -> trackCells[posn].pointDefault =
						trackCtrl -> trackLayout -> trackCells[posn].pointState = pointState;
						trackCtrl -> trackLayout -> trackCells[posn].server = server;
						trackCtrl -> trackLayout -> trackCells[posn].ident = ident;
						for (i = 0; i < 8 && point && pointState == 0; ++i)
						{
							if (point & (1 << i))
							{
								trackCtrl -> trackLayout -> trackCells[posn].pointDefault =
								trackCtrl -> trackLayout -> trackCells[posn].pointState = (1 << i);
								break;
							}
						}
					}
					xmlFree(colStr);
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
	{
		return;
	}
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
					{
						processCell (trackCtrl, curNode -> children, rowNum);
					}
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
				xmlChar *nameStr, *serverStr, *portStr, *cfgPointStr, *cfgPortStr, *serialStr;
				if ((nameStr = xmlGetProp(curNode, (const xmlChar*)"name")) != NULL)
				{
					strncpy (trackCtrl -> trackName, (char *)nameStr, 80);
					xmlFree (nameStr);
				}
				if ((serverStr = xmlGetProp(curNode, (const xmlChar*)"server")) != NULL)
				{
					strncpy (trackCtrl -> server, (char *)serverStr, 80);
					xmlFree (serverStr);
				}
				if ((portStr = xmlGetProp(curNode, (const xmlChar*)"port")) != NULL)
				{
					sscanf ((char *)portStr, "%d", &trackCtrl -> serverPort);
					xmlFree (portStr);
				}
				if ((cfgPointStr = xmlGetProp(curNode, (const xmlChar*)"point")) != NULL)
				{
					sscanf ((char *)cfgPointStr, "%d", &trackCtrl -> pointPort);
					xmlFree (cfgPointStr);
				}
				if ((cfgPortStr = xmlGetProp(curNode, (const xmlChar*)"config")) != NULL)
				{
					sscanf ((char *)cfgPortStr, "%d", &trackCtrl -> configPort);
					xmlFree (cfgPortStr);
				}
				if ((serialStr = xmlGetProp(curNode, (const xmlChar*)"device")) != NULL)
				{
					strncpy (trackCtrl -> serialDevice, (char *)serialStr, 80);
					xmlFree (serialStr);
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
					{
						processTrains (trackCtrl, curNode -> children, count);
					}
				}
			}
			else if (level == 1 && strcmp ((char *)curNode->name, "pointServers") == 0)
			{
				int count = -1, i;
				xmlChar *countStr;
				if ((countStr = xmlGetProp(curNode, (const xmlChar*)"count")) != NULL)
				{
					sscanf ((char *)countStr, "%d", &count);
					xmlFree(countStr);
					if (count > 0)
					{
						if ((trackCtrl -> pointCtrl = (pointCtrlDef *)malloc (count * sizeof (pointCtrlDef))) != NULL)
						{
							memset (trackCtrl -> pointCtrl, 0, count * sizeof (pointCtrlDef));
							for (i = 0; i < count; ++i)
							{
								trackCtrl -> pointCtrl[i].intHandle = -1;
							}
							trackCtrl -> pServerCount = count;
						}
					}
				}
			}
			else if (level == 1 && strcmp ((char *)curNode->name, "cells") == 0)
			{
				int rows = -1, cols = -1, size = 40;
				xmlChar *rowsStr, *colsStr, *sizeStr;
				if ((rowsStr = xmlGetProp(curNode, (const xmlChar*)"rows")) != NULL)
				{
					if ((colsStr = xmlGetProp(curNode, (const xmlChar*)"cols")) != NULL)
					{
						if ((sizeStr = xmlGetProp(curNode, (const xmlChar*)"size")) != NULL)
						{
							sscanf ((char *)sizeStr, "%d", &size);
							xmlFree(sizeStr);
						}
						sscanf ((char *)rowsStr, "%d", &rows);
						sscanf ((char *)colsStr, "%d", &cols);
						xmlFree(colsStr);

						if (rows > 0 && cols > 0)
						{
							processCells (trackCtrl, curNode -> children, rows, cols, size);
						}
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
		{
			parseTree (trackCtrl, rootElement, 0);
		}
		if (trackCtrl -> trackLayout != NULL && trackCtrl -> trainCtrl != NULL)
		{
			retn = 1;
		}
		else if (trackCtrl -> configPort > 0 && trackCtrl -> server[0] && level == 0)
		{
			int cfgSocket = -1;
			char addrBuffer[81];

			if (!GetAddressFromName (trackCtrl -> server, addrBuffer))
			{
				strcpy (addrBuffer, trackCtrl -> server);
			}
			if ((cfgSocket = ConnectClientSocket (addrBuffer, trackCtrl -> configPort)) != -1)
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
					{
						retn = parseTrackXML (trackCtrl, tmpConfig, 1);
					}
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
	{
		xmlBuffer = xmlCharStrndup (memoryXML, strlen (memoryXML));
	}
	else
	{
		xmlBuffer = xmlCharStrndup (buffer, strlen (buffer));
	}
	if (xmlBuffer != NULL)
	{
		if ((doc = xmlParseDoc (xmlBuffer)) != NULL)
		{
			if ((rootElement = xmlDocGetRootElement(doc)) != NULL)
			{
				parseTree (trackCtrl, rootElement, 0);
			}
			if (trackCtrl -> trackLayout != NULL && trackCtrl -> trainCtrl != NULL)
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

