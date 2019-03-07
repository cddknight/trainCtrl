/**********************************************************************************************************************
 *                                                                                                                    *
 *  T R A I N  C T R L . C                                                                                            *
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
 *  \brief Graphical DCC++ controller interface.
 */
#include <gtk/gtk.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>

#include "config.h"
#include "trainControl.h"
#include "socketC.h"

static char *notConnected = "Not connected to the train controller";
static const GdkRGBA blackCol = { 0.1, 0.1, 0.1, 1.0 };
static const GdkRGBA trackCol = { 0.6, 0.6, 0.6, 1.0 };
static const GdkRGBA trFillCol = { 0.0, 0.4, 0.0, 1.0 };
static const GdkRGBA bufferCol = { 0.6, 0.0, 0.0, 1.0 };
static const GdkRGBA inactCol = { 0.8, 0.0, 0.0, 1.0 };
static const GdkRGBA iaFillCol = { 0.4, 0.0, 0.0, 1.0 };
static const GdkRGBA circleCol = { 0.8, 0.8, 0.8, 1.0 };
static const double xChange[8] = { 0, 1, 2, 1, 0, 0, 2, 2 };
static const double yChange[8] = { 1, 0, 1, 2, 2, 0, 0, 2 };

//static void preferencesCallback (GSimpleAction *action, GVariant *parameter, gpointer data);
static void aboutCallback (GSimpleAction *action, GVariant *parameter, gpointer data);
static void quitCallback (GSimpleAction *action, GVariant *parameter, gpointer data);

static GActionEntry appEntries[] =
{
//	{ "preferences", preferencesCallback, NULL, NULL, NULL },
	{ "about", aboutCallback, NULL, NULL, NULL },
	{ "quit", quitCallback, NULL, NULL, NULL }
};

/*--------------------------------------------------------------------------------------------------------------------*
 * If we cannot find a stock clock icon then use this built in one.                                                   *
 *--------------------------------------------------------------------------------------------------------------------*/
static GdkPixbuf *defaultIcon;
#include "train.xpm"

/**********************************************************************************************************************
 *                                                                                                                    *
 *  D I F F  T I M E                                                                                                  *
 *  ================                                                                                                  *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  \brief Calculate the differenc in mili-seconds between to timevals.
 *  \param start Start time.
 *  \param end End time.
 *  \result Difference in milli seconds.
 */
long diffTime (struct timeval *start, struct timeval *end)
{
	struct timeval inStartStore;
	struct timeval inEndStore;
	struct timeval *inStart = &inStartStore;
	struct timeval *inEnd = &inEndStore;

	inStartStore.tv_sec = start -> tv_sec;
	inStartStore.tv_usec = start -> tv_usec;
	inEndStore.tv_sec = end -> tv_sec;
	inEndStore.tv_usec = end -> tv_usec;

	inStart -> tv_usec /= 1000;
	inEnd -> tv_usec /= 1000;

	if (inStart -> tv_sec > inEnd -> tv_sec || 
			(inStart -> tv_sec == inEnd -> tv_sec && inStart -> tv_usec > inEnd -> tv_usec))
	{
		inStart = &inEndStore;
		inEnd = &inStartStore;
	}
	if (inStart -> tv_usec > inEnd -> tv_usec)
	{
		inEnd -> tv_usec += 1000;
		inEnd -> tv_sec -= 1;
	}
	return (inEnd -> tv_usec - inStart -> tv_usec) + 
			((inEnd -> tv_sec - inStart -> tv_sec) * 1000);
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  D I F F  T I M E  T O  N O W                                                                                      *
 *  ============================                                                                                      *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  \brief Different between a time and now.
 *  \param start The start time.
 *  \result Different between a time and now.
 */
long diffTimeToNow (struct timeval *start)
{
    struct timeval now;
    gettimeofday(&now, NULL);
	return diffTime (start, &now);
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  S T O P  T R A I N                                                                                                *
 *  ==================                                                                                                *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  \brief Send an emergency stop command.
 *  \param widget Which button was pressed.
 *  \param data Which train to stop.
 *  \result None.
 */
static void stopTrain (GtkWidget *widget, gpointer data)
{
	trackCtrlDef *trackCtrl = (trackCtrlDef *)data;
	trainCtrlDef *train = (trainCtrlDef *)g_object_get_data (G_OBJECT(widget), "train");

	if (train -> curSpeed != 0)
	{
		char tempBuff[81];
		sprintf (tempBuff, "<t %d %d %d %d>", train -> trainReg, train -> trainID, -1, train -> reverse);
		if (trainConnectSend (trackCtrl, tempBuff, strlen (tempBuff)) > 0)
		{
			train -> curSpeed = train -> remoteCurSpeed = 0;
			gtk_range_set_value (GTK_RANGE (train -> scaleSpeed), 0.0);
			sprintf (tempBuff, "Set speed: STOP for train %d", train -> trainNum);
			gtk_statusbar_push (GTK_STATUSBAR (trackCtrl -> statusBar), 1, tempBuff);
		}
		else
		{
			gtk_statusbar_push (GTK_STATUSBAR (trackCtrl -> statusBar), 1, notConnected);
		}
	}
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  T R A I N  I N F O                                                                                                *
 *  ==================                                                                                                *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  \brief Display information about the train on the status bar.
 *  \param widget Widget calling this.
 *  \param data Track data.
 *  \result None.
 */
static void trainInfo (GtkWidget *widget, gpointer data)
{
	char tempBuff[181];
	trackCtrlDef *trackCtrl = (trackCtrlDef *)data;
	trainCtrlDef *train = (trainCtrlDef *)g_object_get_data (G_OBJECT(widget), "train");
	snprintf (tempBuff, 180, "%d, %s, DCC %d", train -> trainNum, train -> trainDesc, train -> trainID);
	gtk_statusbar_push (GTK_STATUSBAR (trackCtrl -> statusBar), 1, tempBuff);
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  H A L T  T R A I N                                                                                                *
 *  ==================                                                                                                *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  \brief Send a standard halt.
 *  \param widget Which button was pressed.
 *  \param data Which train to halt.
 *  \result None.
 */
static void haltTrain (GtkWidget *widget, gpointer data)
{
	trackCtrlDef *trackCtrl = (trackCtrlDef *)data;
	trainCtrlDef *train = (trainCtrlDef *)g_object_get_data (G_OBJECT(widget), "train");

	if (train -> curSpeed != 0)
	{
		char tempBuff[81];
		sprintf (tempBuff, "<t %d %d %d %d>", train -> trainReg, train -> trainID, 0, train -> reverse);
		if (trainConnectSend (trackCtrl, tempBuff, strlen (tempBuff)) > 0)
		{
			train -> curSpeed = train -> remoteCurSpeed = 0;
			gtk_range_set_value (GTK_RANGE (train -> scaleSpeed), 0.0);
			sprintf (tempBuff, "Set speed: 0 for train %d", train -> trainNum);
			gtk_statusbar_push (GTK_STATUSBAR (trackCtrl -> statusBar), 1, tempBuff);
		}
		else
		{
			gtk_statusbar_push (GTK_STATUSBAR (trackCtrl -> statusBar), 1, notConnected);
		}
	}
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  C H E C K  P O W E R  O N                                                                                         *
 *  =========================                                                                                         *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  \brief Check the state of the power and disable if it is off.
 *  \param trackCtrl Which is the active track.
 *  \result None.
 */
void checkPowerOn (trackCtrlDef *trackCtrl)
{
	int i;
	gboolean state = (trackCtrl -> powerState == POWER_ON ? TRUE : FALSE);
	for (i = 0; i < trackCtrl -> trainCount; ++i)
	{
		if (trackCtrl -> trainCtrl[i].buttonNum != NULL)
			gtk_widget_set_sensitive (trackCtrl -> trainCtrl[i].buttonNum, state);
		if (trackCtrl -> trainCtrl[i].buttonHalt != NULL)
			gtk_widget_set_sensitive (trackCtrl -> trainCtrl[i].buttonHalt, state);
		if (trackCtrl -> trainCtrl[i].scaleSpeed != NULL)
			gtk_widget_set_sensitive (trackCtrl -> trainCtrl[i].scaleSpeed, state);
		if (trackCtrl -> trainCtrl[i].checkDir != NULL)
			gtk_widget_set_sensitive (trackCtrl -> trainCtrl[i].checkDir, state);
		if (trackCtrl -> trainCtrl[i].buttonStop != NULL)
			gtk_widget_set_sensitive (trackCtrl -> trainCtrl[i].buttonStop, state);
	}
	if (trackCtrl -> buttonProgram != NULL)
	{
		gtk_widget_set_sensitive (trackCtrl -> buttonProgram, state);
	}
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  T R A C K  P O W E R                                                                                              *
 *  ====================                                                                                              *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  \brief Turn on/off track power.
 *  \param widget Which button was pressed.
 *  \param pspec Not used.
 *  \param data Which track to power.
 *  \result None.
 */
static gboolean trackPower (GtkWidget *widget, GParamSpec *pspec, gpointer data)
{
	int i, newState;
	char tempBuff[81];
	trackCtrlDef *trackCtrl = (trackCtrlDef *)data;

	newState = (trackCtrl -> powerState == POWER_ON ? POWER_OFF : POWER_ON);
	if (trainConnectSend (trackCtrl, newState == POWER_ON ? "<1>" : "<0>", 3) == 3)
	{
		trackCtrl -> powerState = newState;
		sprintf (tempBuff, "Track power: %s", trackCtrl -> powerState == POWER_ON ? "On" : "Off");
		gtk_statusbar_push (GTK_STATUSBAR (trackCtrl -> statusBar), 1, tempBuff);

		if (trackCtrl -> powerState != trackCtrl -> remotePowerState)
		{
			trackCtrl -> remotePowerState = newState;
			for (i = 0; i < trackCtrl -> trainCount; ++i)
			{
				trainCtrlDef *train = &trackCtrl -> trainCtrl[i];
				if (train -> curSpeed > 0 && trackCtrl -> powerState == POWER_OFF)
				{
					trackCtrl -> trainCtrl[i].curSpeed = trackCtrl -> trainCtrl[i].remoteCurSpeed = 0;
					gtk_range_set_value (GTK_RANGE (trackCtrl -> trainCtrl[i].scaleSpeed), 0.0);
					sprintf (tempBuff, "<t %d %d %d %d>", train -> trainReg, train -> trainID, 0, train -> reverse);
				}
			}
		}
		checkPowerOn (trackCtrl);
	}
	else
	{
		if (newState == POWER_ON)
		{
			gtk_switch_set_active (GTK_SWITCH(trackCtrl -> buttonPower), FALSE);
		}
		gtk_statusbar_push (GTK_STATUSBAR (trackCtrl -> statusBar), 1, notConnected);
	}
	return TRUE;
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  M O V E  T R A I N                                                                                                *
 *  ==================                                                                                                *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  \brief Send a speed change to a train.
 *  \param widget Which button was pressed.
 *  \param data Which train to control.
 *  \result None.
 */
static void moveTrain (GtkWidget *widget, gpointer data)
{
	double value = 0.0;
	trackCtrlDef *trackCtrl = (trackCtrlDef *)data;
	trainCtrlDef *train = (trainCtrlDef *)g_object_get_data (G_OBJECT(widget), "train");

	value = gtk_range_get_value (GTK_RANGE (train -> scaleSpeed));
	if (train -> curSpeed != (int)value)
	{
		char tempBuff[81];
		int newValue = (int)value;

		sprintf (tempBuff, "<t %d %d %d %d>", train -> trainReg, train -> trainID, newValue, train -> reverse);
		if (trainConnectSend (trackCtrl, tempBuff, strlen (tempBuff)) > 0)
		{
		    gettimeofday(&train -> lastChange, NULL);
			train -> curSpeed = train -> remoteCurSpeed = newValue;
			sprintf (tempBuff, "Set speed: %d for train %d", newValue, train -> trainNum);
			gtk_statusbar_push (GTK_STATUSBAR (trackCtrl -> statusBar), 1, tempBuff);
		}
		else
		{
			gtk_statusbar_push (GTK_STATUSBAR (trackCtrl -> statusBar), 1, notConnected);
		}
	}
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  R E V E R S E  T R A I N                                                                                          *
 *  ========================                                                                                          *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  \brief Send a change direction to a train.
 *  \param widget Which button was pressed.
 *  \param data Which train to control.
 *  \result None.
 */
static void reverseTrain (GtkWidget *widget, gpointer data)
{
	char tempBuff[81];
	int newState;
	trackCtrlDef *trackCtrl = (trackCtrlDef *)data;
	trainCtrlDef *train = (trainCtrlDef *)g_object_get_data (G_OBJECT(widget), "train");

	newState = (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (train -> checkDir)) == TRUE ? 1 : 0);
	if (newState != train -> reverse)
	{
		train -> reverse = train -> remoteReverse = newState;
		sprintf (tempBuff, "<t %d %d %d %d>", train -> trainReg, train -> trainID, 0, train -> reverse);
		if (trainConnectSend (trackCtrl, tempBuff, strlen (tempBuff)) > 0)
		{
			train -> curSpeed = train -> remoteCurSpeed = 0;
			gtk_range_set_value (GTK_RANGE (train -> scaleSpeed), 0.0);
		}
		sprintf (tempBuff, "Set reverse: %s for train %d", (train -> reverse ? "On" : "Off"), train -> trainNum);
		gtk_statusbar_push (GTK_STATUSBAR (trackCtrl -> statusBar), 1, tempBuff);
	}
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  D R A W  T R A C K  C A L L B A C K                                                                               *
 *  ===================================                                                                               *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  \brief Draw the track.
 *  \param widget Button that was pressed.
 *  \param cr Cairo context.
 *  \param data Track to draw.
 *  \result FALSE.
 */
gboolean drawTrackCallback (GtkWidget *widget, cairo_t *cr, gpointer data)
{
	trackCtrlDef *trackCtrl = (trackCtrlDef *)data;

	double lineWidths[2];
	int i, j, xChangeMod[8], yChangeMod[8];
	int rows = trackCtrl -> trackLayout -> trackRows;
	int cols = trackCtrl -> trackLayout -> trackCols;
	int cellSize = trackCtrl -> trackLayout -> trackSize;
	int cellHalf = cellSize >> 1;
	guint width = gtk_widget_get_allocated_width (widget);
	guint height = gtk_widget_get_allocated_height (widget);
	GtkStyleContext *context = gtk_widget_get_style_context (widget);

	lineWidths[0] = (double)cellSize / 4;
	lineWidths[1] = (double)cellSize / 8;
	for (i = 0; i < 8; ++i)
	{
		xChangeMod[i] = xChange[i] * cellHalf;
		yChangeMod[i] = yChange[i] * cellHalf;
	}

	cairo_save (cr);
	gtk_render_background (context, cr, 0, 0, width, height);
	gdk_cairo_set_source_rgba (cr, &blackCol);
	cairo_set_line_width (cr, 1.0);

	for (i = 1; i < rows; ++i)
	{
		cairo_move_to (cr, 0, i * cellSize);
		cairo_line_to (cr, cols * cellSize, i * cellSize);
	}
	cairo_stroke (cr);
	for (j = 1; j < cols; ++j)
	{
		cairo_move_to (cr, j * cellSize, 0);
		cairo_line_to (cr, j * cellSize, rows * cellSize);
	}
	cairo_stroke (cr);
	cairo_restore (cr);

	for (i = 0; i < rows; ++i)
	{
		for (j = 0; j < cols; ++j)
		{
			int lineType, xPos[3]={0,0,0}, yPos[3]={0,0,0}, posMask = 0;
			int posn = (i * cols) + j, count = 0, loop;

			for (loop = 0; loop < 8; ++loop)
			{
				if (trackCtrl -> trackLayout -> trackCells[posn].layout & (1 << loop))
				{
					++count;
					if (trackCtrl -> trackLayout -> trackCells[posn].point & (1 << loop))
					{
						if (!(trackCtrl -> trackLayout -> trackCells[posn].pointState & (1 << loop)))
						{
							xPos[2] = (j * cellSize) + xChangeMod[loop];
							yPos[2] = (i * cellSize) + yChangeMod[loop];
							posMask |= 4;
						}
						else
						{
							xPos[1] = (j * cellSize) + xChangeMod[loop];
							yPos[1] = (i * cellSize) + yChangeMod[loop];
							posMask |= 2;
						}
					}
					else
					{
						if (!(posMask & 1))
						{
							xPos[0] = (j * cellSize) + xChangeMod[loop];
							yPos[0] = (i * cellSize) + yChangeMod[loop];
							posMask |= 1;
						}
						else
						{
							xPos[1] = (j * cellSize) + xChangeMod[loop];
							yPos[1] = (i * cellSize) + yChangeMod[loop];
							posMask |= 2;
						}
					}
				}
			}
			if (posMask & 4)
			{
				for (lineType = 0; lineType < 2; ++lineType)
				{
					cairo_save (cr);
					cairo_set_line_join (cr, CAIRO_LINE_JOIN_ROUND);
					gdk_cairo_set_source_rgba (cr, lineType ? &iaFillCol : &inactCol);
					cairo_set_line_width (cr, lineType ? lineWidths[1] : lineWidths[0]);
					cairo_move_to (cr, (j * cellSize) + cellHalf, (i * cellSize) + cellHalf);
					cairo_line_to (cr, xPos[2], yPos[2]);
					cairo_stroke (cr);
					cairo_restore (cr);
				}
			}
			if (posMask & 1)
			{
				for (lineType = 0; lineType < 2; ++lineType)
				{
					cairo_save (cr);
					cairo_set_line_join (cr, CAIRO_LINE_JOIN_ROUND);
					gdk_cairo_set_source_rgba (cr, lineType ? &trFillCol : &trackCol);
					cairo_set_line_width (cr, lineType ? lineWidths[1] : lineWidths[0]);
					cairo_move_to (cr, xPos[0], yPos[0]);
					cairo_line_to (cr, (j * cellSize) + cellHalf, (i * cellSize) + cellHalf);
					if (posMask & 2)
					{
						cairo_line_to (cr, xPos[1], yPos[1]);
					}
					cairo_stroke (cr);
					cairo_restore (cr);
				}
			}
			if (count == 1)
			{
				gdk_cairo_set_source_rgba (cr, &bufferCol);
				cairo_arc (cr, (j * cellSize) + cellHalf, (i * cellSize) + cellHalf, (double)cellSize / 7.5, 0, 2 * G_PI);
				cairo_fill (cr);
				cairo_stroke (cr);

				gdk_cairo_set_source_rgba (cr, &circleCol);
				cairo_arc (cr, (j * cellSize) + cellHalf, (i * cellSize) + cellHalf, (double)cellSize / 7.5, 0, 2 * G_PI);
				cairo_stroke (cr);
			}
		}
	}
	return FALSE;
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  W I N D O W  C L I C K  C A L L B A C K                                                                           *
 *  =======================================                                                                           *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  \brief Called when the track window is clicked.
 *  \param widget Not used.
 *  \param event Where it was clicked.
 *  \param data Which is the active track.
 *  \result TRUE if we used it.
 */
gboolean windowClickCallback (GtkWidget * widget, GdkEventButton * event, gpointer data)
{
	trackCtrlDef *trackCtrl = (trackCtrlDef *)data;
	static int linkRow[] = {	0,	-1, 0,	1,	1,	-1, -1, 1	};
	static int linkCol[] = {	-1, 0,	1,	0,	-1, -1, 1,	1	};

	if (event->type == GDK_BUTTON_PRESS)
	{
		switch (event->button)
		{
		case GDK_BUTTON_PRIMARY:	/* left button */
			{
				int rows = trackCtrl -> trackLayout -> trackRows;
				int cols = trackCtrl -> trackLayout -> trackCols;
				int cellSize = trackCtrl -> trackLayout -> trackSize;
				int posn = (((int)event -> y / cellSize) * cols) + ((int)event -> x / cellSize);

				if (trackCtrl -> trackLayout -> trackCells[posn].point)
				{
					char tempBuff[81];
					trackCellDef *cell = &trackCtrl -> trackLayout -> trackCells[posn];
					unsigned short newState = cell -> point;

					newState &= ~(cell -> pointState);
					sprintf (tempBuff, "<Y %d %d %d>", cell -> server, cell -> ident, 
							cell -> pointDefault == newState ? 0 : 1);
					if (trainConnectSend (trackCtrl, tempBuff, strlen (tempBuff)) > 0)
					{
						/* This point is linked so change the other point */
						if (trackCtrl -> trackLayout -> trackCells[posn].link)
						{
							int i;
							for (i = 0; i < 8; ++i)
							{
								if (cell -> link & (1 << i))
								{
									int newLinkState, newPosn = posn + (cols * linkRow[i]) + linkCol[i];
									if (newPosn >= 0 && newPosn < (rows * cols))
									{
										trackCellDef *newCell = &trackCtrl -> trackLayout -> trackCells[newPosn];

										/* The new point should have a link, we hope to us */
										if (newCell -> link)
										{
											if (cell -> link == newState)
											{
												/* We are setting to the link, so set other point to the link */
												newLinkState = newCell -> link;
											}
											else
											{
												/* We are breaking the link, so set other point away from link */
												newLinkState = newCell -> point & ~newCell -> link;
											}
											sprintf (tempBuff, "<Y %d %d %d>", newCell -> server, newCell -> ident, 
													newCell -> pointDefault == newLinkState ? 0 : 1);
										}
									}
								}
							}
						}
					}
				}
			}
			return TRUE;
		}
	}
	return FALSE;
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  C L O S E  T R A C K                                                                                              *
 *  ====================                                                                                              *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  \brief Function call on window destroy to close window pointer.
 *  \param widget Not used.
 *  \param data Not used.
 *  \result None.
 */
static void closeTrack (GtkWidget *widget, gpointer data)
{
	trackCtrlDef *trackCtrl = (trackCtrlDef *)data;
	trackCtrl -> windowTrack = NULL;
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  D I S P L A Y  T R A C K                                                                                          *
 *  ========================                                                                                          *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  \brief Display the track layout, later for track control.
 *  \param widget What called the display.
 *  \param data What track to draw.
 *  \result None.
 */
static void displayTrack (GtkWidget *widget, gpointer data)
{
	trackCtrlDef *trackCtrl = (trackCtrlDef *)data;
	if (trackCtrl -> serverHandle == -1)
	{
		GtkWidget *errDialog = gtk_message_dialog_new (GTK_WINDOW (trackCtrl -> windowCtrl),
				GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
				GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE,
				"%s", notConnected);
		gtk_dialog_run (GTK_DIALOG (errDialog));
		gtk_widget_destroy (errDialog);
		return;
	}
	if (trackCtrl -> windowTrack == NULL)
	{
		GtkWidget *eventBox;
		int cellSize = trackCtrl -> trackLayout -> trackSize;
		int width = trackCtrl -> trackLayout -> trackCols * cellSize;
		int height = trackCtrl -> trackLayout -> trackRows * cellSize;

		trackCtrl -> windowTrack = gtk_window_new (GTK_WINDOW_TOPLEVEL);
		gtk_window_set_title (GTK_WINDOW (trackCtrl -> windowTrack), "Track Control");
		gtk_window_set_default_size (GTK_WINDOW (trackCtrl -> windowTrack), width, height);

		trackCtrl -> drawingArea = gtk_drawing_area_new ();
		gtk_widget_set_size_request (trackCtrl -> drawingArea, width, height);
		eventBox = gtk_event_box_new ();
		gtk_container_add (GTK_CONTAINER (eventBox), trackCtrl -> drawingArea);

		g_signal_connect (G_OBJECT (trackCtrl -> drawingArea), "draw", G_CALLBACK (drawTrackCallback), trackCtrl);
		g_signal_connect (G_OBJECT (trackCtrl -> windowTrack), "destroy", G_CALLBACK (closeTrack), trackCtrl);
		g_signal_connect (G_OBJECT (eventBox), "button_press_event", G_CALLBACK (windowClickCallback), trackCtrl);

		gtk_container_add (GTK_CONTAINER (trackCtrl -> windowTrack), eventBox);
		gtk_widget_show_all (trackCtrl -> windowTrack);
	}
}


/**********************************************************************************************************************
 *                                                                                                                    *
 *  U P D A T E  P O I N T  P O S N                                                                                   *
 *  ===============================                                                                                   *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  \brief Update the current state of the point.
 *  \param trackCtrl Track config.
 *  \param server Server that is sending the update.
 *  \param point Point to change.
 *  \param state New point state.
 *  \result None.
 */
void updatePointPosn (trackCtrlDef *trackCtrl, int server, int point, int state)
{
	int i, cells = trackCtrl -> trackLayout -> trackRows * trackCtrl -> trackLayout -> trackCols;
	
	for (i = 0; i < cells; ++i)
	{
		trackCellDef *cell = &trackCtrl -> trackLayout -> trackCells[i];
		if (cell -> point)
		{
			if (cell -> server == server && cell -> ident == point)
			{
				if (state == 0)
				{
					cell -> pointState = cell -> pointDefault;
				}
				else
				{
					cell -> pointState = cell-> point & ~(cell -> pointDefault);
				}
				if (trackCtrl -> windowTrack != NULL)
				{
					gtk_widget_queue_draw (trackCtrl -> drawingArea);
				}
				break;
			}
		}
	}
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  C O N N E C T  S T A T U S                                                                                        *
 *  ==========================                                                                                        *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  \brief Display the connection status from the server.
 *  \param widget Not used.
 *  \param data Pointer to track control.
 *  \result None.
 */
static void connectStatus (GtkWidget *widget, gpointer data)
{
	int i = 0;
	GtkWidget *contentArea;
	GtkWidget *label, *grid, *vbox;
	trackCtrlDef *trackCtrl = (trackCtrlDef *)data;

	static char *statusLables[] =
	{
		"Server:",
		"Serial port:",
		"Listen socket:",
		"Configuration:",
		"Point control:",
		"Clients:",
		NULL
	};

	if (trackCtrl -> dialogStatus == NULL)
	{
		trackCtrl -> dialogStatus = gtk_dialog_new_with_buttons ("Server Status",
				GTK_WINDOW (trackCtrl -> windowCtrl),
				GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
				"Update", GTK_RESPONSE_APPLY,
				"Close", GTK_RESPONSE_CLOSE,
				NULL);

		contentArea = gtk_dialog_get_content_area (GTK_DIALOG (trackCtrl -> dialogStatus));

		vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
		gtk_widget_set_halign (vbox, GTK_ALIGN_FILL);
		gtk_widget_set_valign (vbox, GTK_ALIGN_FILL);
		gtk_box_pack_start (GTK_BOX (contentArea), vbox, TRUE, TRUE, 0);

		grid = gtk_grid_new();
		gtk_widget_set_halign (grid, GTK_ALIGN_FILL);
		gtk_widget_set_valign (grid, GTK_ALIGN_FILL);
		gtk_grid_set_row_spacing (GTK_GRID (grid), 3);
		gtk_grid_set_column_spacing (GTK_GRID (grid), 6);
		gtk_box_pack_start (GTK_BOX (vbox), grid, TRUE, TRUE, 6);

		for (i = 0; i < 6; ++i)
		{
			label = gtk_label_new (statusLables[i]);
			gtk_widget_set_halign (label, GTK_ALIGN_END);
			gtk_grid_attach (GTK_GRID(grid), label, 0, i, 1, 1);

			if (i == 0)
			{
				trackCtrl -> statusLabels[i] = gtk_label_new (trackCtrl -> serverHandle == -1 
						? "Not connected" : "Connected");
				gtk_widget_set_halign (trackCtrl -> statusLabels[i], GTK_ALIGN_START);
				gtk_grid_attach (GTK_GRID(grid), trackCtrl -> statusLabels[i], 1, i, 1, 1);
			}
			else
			{
				trackCtrl -> statusLabels[i] = gtk_label_new ("Unknown");
				gtk_widget_set_halign (trackCtrl -> statusLabels[i], GTK_ALIGN_START);
				gtk_grid_attach (GTK_GRID(grid), trackCtrl -> statusLabels[i], 1, i, 1, 1);
				trackCtrl -> serverStatus[i - 1] = -1;
			}
		}
		trackCtrl -> serverStatus[5] = 0;

		gtk_widget_show_all (trackCtrl -> dialogStatus);
		do
		{
			int sendRes = trainConnectSend (trackCtrl, "<V>", 3);

			gtk_label_set_label (GTK_LABEL (trackCtrl -> statusLabels[0]),
					sendRes == 3 ?  "Connected" :  "Not connected");

			for (i = 1; i < 6; ++i)
			{
				gtk_label_set_label (GTK_LABEL (trackCtrl -> statusLabels[i]), 
						 sendRes == 3 ? "Pending" : "Unknown");
			}
		}
		while (gtk_dialog_run (GTK_DIALOG (trackCtrl -> dialogStatus)) == GTK_RESPONSE_APPLY);

		gtk_widget_destroy (trackCtrl -> dialogStatus);
		trackCtrl -> dialogStatus = NULL;
	}
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  P R O G R A M  Y E S  N O                                                                                         *
 *  =========================                                                                                         *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  \brief Dialog to warn user before sending a programming command.
 *  \param trackCtrl Which is the active track.
 *  \param question Question to ask them.
 *  \result 1 and the command can go ahead.
 */
static int programYesNo (trackCtrlDef *trackCtrl, char *question)
{
	int retn = 0;
	GtkWidget *dialog = gtk_message_dialog_new (GTK_WINDOW (trackCtrl -> dialogProgram),
			GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_MESSAGE_WARNING, GTK_BUTTONS_OK_CANCEL,
			"%s", question);

	if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_OK)
	{
		retn = 1;
	}
	gtk_widget_destroy (dialog);
	return retn;
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  P R O G R A M  T R A I N                                                                                          *
 *  ========================                                                                                          *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  \brief A dialog that allow simple programming commands.
 *  \param widget Button that called it.
 *  \param data Not used.
 *  \result None.
 */
static void programTrain (GtkWidget *widget, gpointer data)
{
	trackCtrlDef *trackCtrl = (trackCtrlDef *)data;

	static char *controlLables[] =
	{
		"DCC Address", "CV Number", "Byte Value (0 - 255)", "Bit number (1 - 8)", "Bit Value (0 - 1)",
		"Read current value", "Last reply:", "-"
	};
	static char *hintLables[] =
	{
		"Hints:",
		"* If DCC address is zero then use programming track.",
		"* Reads only work with train on the programming track.",
		"* If bit number in the range 1 to 8 then set bit value.",
		"* If the bit number is zero then set the byte value.",
		NULL
	};
	static double maxValues[] = { 10294.0, 1025.0, 256.0, 9.0, 2.0 };

	int i = 0;
	GtkWidget *contentArea;
	GtkWidget *spinner[5];
	GtkAdjustment *adjust[5];
	GtkWidget *label, *grid, *vbox, *checkButton;

	if (trackCtrl -> serverHandle == -1)
	{
		GtkWidget *errDialog = gtk_message_dialog_new (GTK_WINDOW (trackCtrl -> windowCtrl),
				GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
				GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE,
				"%s", notConnected);
		gtk_dialog_run (GTK_DIALOG (errDialog));
		gtk_widget_destroy (errDialog);
		return;
	}

	if (trackCtrl -> dialogProgram == NULL)
	{
		trackCtrl -> dialogProgram = gtk_dialog_new_with_buttons ("Program Train",
				GTK_WINDOW (trackCtrl -> windowCtrl),
				GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
				"Apply", GTK_RESPONSE_APPLY,
				"Close", GTK_RESPONSE_CLOSE,
				NULL);

		contentArea = gtk_dialog_get_content_area (GTK_DIALOG (trackCtrl -> dialogProgram));

		vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
		gtk_widget_set_halign (vbox, GTK_ALIGN_FILL);
		gtk_widget_set_valign (vbox, GTK_ALIGN_FILL);
		gtk_box_pack_start (GTK_BOX (contentArea), vbox, TRUE, TRUE, 0);

		while (hintLables[i] != NULL)
		{
			label = gtk_label_new (hintLables[i]);
			gtk_widget_set_halign (label, GTK_ALIGN_START);
			gtk_widget_set_valign (label, GTK_ALIGN_START);
			gtk_box_pack_start (GTK_BOX(vbox), label, FALSE, FALSE, 3);
			++i;
		}

		gtk_box_pack_start (GTK_BOX(vbox), gtk_separator_new (GTK_ORIENTATION_HORIZONTAL), FALSE, FALSE, 3);

		grid = gtk_grid_new();
		gtk_widget_set_halign (grid, GTK_ALIGN_FILL);
		gtk_widget_set_valign (grid, GTK_ALIGN_FILL);
		gtk_grid_set_row_spacing (GTK_GRID (grid), 3);
		gtk_grid_set_column_spacing (GTK_GRID (grid), 6);
		gtk_box_pack_start (GTK_BOX (vbox), grid, TRUE, TRUE, 3);

		for (i = 0; i < 5; ++i)
		{
			label = gtk_label_new (controlLables[i]);
			gtk_widget_set_halign (label, GTK_ALIGN_END);
			gtk_grid_attach (GTK_GRID(grid), label, 0, i, 1, 1);

			adjust[i] = gtk_adjustment_new (0.0, 0.0, maxValues[i], 1.0, 1.0, 1.0);
			spinner[i] = gtk_spin_button_new (adjust[i], 10, 0);
			gtk_widget_set_halign (GTK_WIDGET (spinner[i]), GTK_ALIGN_FILL);
			gtk_grid_attach (GTK_GRID(grid), spinner[i], 1, i, 1, 1);
		}

		checkButton = gtk_check_button_new_with_label (controlLables[i]);
		gtk_widget_set_halign (checkButton, GTK_ALIGN_START);
		gtk_grid_attach (GTK_GRID(grid), checkButton, 1, i, 1, 1);

		label = gtk_label_new (controlLables[++i]);
		gtk_widget_set_halign (label, GTK_ALIGN_END);
		gtk_grid_attach (GTK_GRID(grid), label, 0, i, 1, 1);

		trackCtrl -> labelProgram = gtk_label_new (controlLables[++i]);
		gtk_widget_set_halign (trackCtrl -> labelProgram, GTK_ALIGN_START);
		gtk_grid_attach (GTK_GRID(grid), trackCtrl -> labelProgram, 1, i - 1, 1, 1);

		gtk_widget_show_all (trackCtrl -> dialogProgram);
		while (gtk_dialog_run (GTK_DIALOG (trackCtrl -> dialogProgram)) == GTK_RESPONSE_APPLY)
		{
			int values[5], allOK = 0;
			char sendBuffer[41], msgBuffer[161];
			gboolean checkRead = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (checkButton));

			sendBuffer[0] = 0;
			for (i = 0; i < 5; ++i)
			{
				values[i] = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON(spinner[i]));
			}
			/* Programming track, no train ID */
			if (values[0] == 0)
			{
				/* Programming track, need a CV# */
				if (values[1] > 0 && values[1] < 1025)
				{
					if (checkRead == TRUE)
					{
						/* Read CV number */
						sprintf (sendBuffer, "<R %d %d 1>", values[1], trackCtrl -> serverSession);
						allOK = 1;
					}
					else if (values[3])
					{
						/* Write bit value */
						if (values[3] <= 8 && (values[4] == 0 || values[4] == 1))
						{
							/* Write bit value */
							sprintf (msgBuffer, "Write to CV#%d, bit %d, value %d on the programming track?",
									values[1], values[3], values[4]);
							if (programYesNo (trackCtrl, msgBuffer))
							{
								sprintf (sendBuffer, "<B %d %d %d %d 2>", values[1], values[3] - 1, values[4],
										trackCtrl -> serverSession);
							}
							allOK = 1;
						}
					}
					else
					{
						/* Write byte value */
						if (values[2] >= 0 && values[2] <= 255)
						{
							/* Write CV byte */
							sprintf (msgBuffer, "Write to CV#%d, value %d on the programming track?",
									values[1], values[2]);
							if (programYesNo (trackCtrl, msgBuffer))
							{
								sprintf (sendBuffer, "<W %d %d %d 3>", values[1], values[2], trackCtrl -> serverSession);
							}
							allOK = 1;
						}
					}
				}
			}
			/* Main track, need a CV# */
			else if (values[1] > 0 && values[1] < 1025)
			{
				if (values[3])
				{
					/* Write bit value */
					if (values[3] <= 8 && (values[4] == 0 || values[4] == 1))
					{
						/* Write bit value */
						sprintf (msgBuffer, "Write to address %d, CV#%d, bit %d, value %d on the main track?",
								values[0], values[1], values[3], values[4]);
						if (programYesNo (trackCtrl, msgBuffer))
						{
							sprintf (sendBuffer, "<b %d %d %d %d>", values[0], values[1], values[3] - 1, values[4]);
						}
						allOK = 1;
					}
				}
				else
				{
					/* Write byte value */
					if (values[2] >= 0 && values[2] <= 255)
					{
						/* Write CV byte */
						sprintf (msgBuffer, "Write to address %d, CV number %d, value %d on the main track?",
								values[0], values[1], values[2]);
						if (programYesNo (trackCtrl, msgBuffer))
						{
							sprintf (sendBuffer, "<w %d %d %d>", values[0], values[1], values[2]);
						}
						allOK = 1;
					}
				}
			}
			if (allOK == 0)
			{
				gtk_label_set_label (GTK_LABEL (trackCtrl -> labelProgram), "Incorrect inputs, read notes.");
			}
			else if (sendBuffer[0] != 0)
			{
				if (trainConnectSend (trackCtrl, sendBuffer, strlen (sendBuffer)) > 0)
				{
					gtk_label_set_label (GTK_LABEL (trackCtrl -> labelProgram), "Command sent to daemon");
				}
				else
				{
					gtk_label_set_label (GTK_LABEL (trackCtrl -> labelProgram), notConnected);
				}
				sendBuffer[0] = 0;
			}
		}
		gtk_widget_destroy (trackCtrl -> dialogProgram);
		trackCtrl -> dialogProgram = NULL;
		trackCtrl -> labelProgram = NULL;
	}
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  C L O C K  T I C K  C A L L B A C K                                                                               *
 *  ===================================                                                                               *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  \brief To update the display there is a timer.
 *  \param data Not used.
 *  \result Always true so it is called again.
 */
gboolean clockTickCallback (gpointer data)
{
	int i;
	trackCtrlDef *trackCtrl = (trackCtrlDef *)data;

	if (trackCtrl -> powerState != trackCtrl -> remotePowerState)
	{
		gtk_switch_set_active (GTK_SWITCH(trackCtrl -> buttonPower), trackCtrl -> remotePowerState == POWER_ON ? TRUE : FALSE);
	}
	for (i = 0; i < trackCtrl -> trainCount; ++i)
	{
		trainCtrlDef *train = &trackCtrl -> trainCtrl[i];
		if (train -> curSpeed != train -> remoteCurSpeed)
		{
			if (diffTimeToNow (&train -> lastChange) > 750)
			{
				train -> curSpeed = train -> remoteCurSpeed;
				gtk_range_set_value (GTK_RANGE (train -> scaleSpeed), (double)train -> curSpeed);
			}
		}
		if (train -> reverse != train -> remoteReverse)
		{
			train -> reverse = train -> remoteReverse;
			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (train -> checkDir), train -> reverse);
		}
		if (trackCtrl -> remoteProgMsg[0] != 0)
		{
			if (trackCtrl -> labelProgram != NULL)
			{
				gtk_label_set_label (GTK_LABEL (trackCtrl -> labelProgram), trackCtrl -> remoteProgMsg);
				trackCtrl -> remoteProgMsg[0] = 0;
			}
		}
		if (trackCtrl -> remoteCurrent != trackCtrl -> shownCurrent)
		{
			if (trackCtrl -> labelPower != NULL)
			{
				char tempBuff[81];
				if (trackCtrl -> powerState == POWER_ON && trackCtrl -> remoteCurrent >= 0)
				{
					sprintf (tempBuff, "Power [%d%%]", (trackCtrl -> remoteCurrent * 100) / 1024);
				}
				else
				{
					strcpy (tempBuff, "Power");
				}
				gtk_label_set_label (GTK_LABEL (trackCtrl -> labelPower), tempBuff);
				trackCtrl -> shownCurrent = trackCtrl -> remoteCurrent;
			}
		}
	}
	if (trackCtrl -> dialogStatus != NULL)
	{
		if (trackCtrl -> serverStatus[5] == 1)
		{
			char tempBuff[81];

			gtk_label_set_label (GTK_LABEL (trackCtrl -> statusLabels[0]), 
					trackCtrl -> serverHandle == -1 ? "Not connected" : "Connected");
			gtk_label_set_label (GTK_LABEL (trackCtrl -> statusLabels[1]), 
					trackCtrl -> serverStatus[0] > 0 ? "OK" : "Not connected");
			gtk_label_set_label (GTK_LABEL (trackCtrl -> statusLabels[2]), 
					trackCtrl -> serverStatus[1] > 0 ? "OK" : "Not listening");
			gtk_label_set_label (GTK_LABEL (trackCtrl -> statusLabels[3]), 
					trackCtrl -> serverStatus[2] > 0 ? "OK" : "Not listening");
			sprintf (tempBuff, "%d Connected", trackCtrl -> serverStatus[3]);
			gtk_label_set_label (GTK_LABEL (trackCtrl -> statusLabels[4]), tempBuff);
			sprintf (tempBuff, "%d Connected", trackCtrl -> serverStatus[4]);
			gtk_label_set_label (GTK_LABEL (trackCtrl -> statusLabels[5]), tempBuff);
			trackCtrl -> serverStatus[5] = 0;
		}
	}
	return TRUE;
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  W I N D O W  D E S T R O Y                                                                                        *
 *  ==========================                                                                                        *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  \brief Close the window.
 *  \param window Closing window.
 *  \param userData Which is the active track.
 *  \result none.
 */
static void windowDestroy (GtkWidget *window, gpointer userData)
{
	trackCtrlDef *trackCtrl = (trackCtrlDef *)userData;
	stopConnectThread (trackCtrl);
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  A C T I V A T E                                                                                                   *
 *  ===============                                                                                                   *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  \brief Activate a new control window.
 *  \param app Application making the request.
 *  \param userData Not used.
 *  \result None.
 */
static void activate (GtkApplication *app, gpointer userData)
{
	int i, parseRetn = 0;
	char tempBuff[161];
	GtkWidget *grid;
	GtkWidget *vbox, *hbox;
	GMenu *menu;
	trackCtrlDef *trackCtrl = (trackCtrlDef *)malloc (sizeof (trackCtrlDef));
	memset (trackCtrl, 0, sizeof (trackCtrlDef));

	g_action_map_add_action_entries (G_ACTION_MAP (app), appEntries, G_N_ELEMENTS (appEntries), app);
	menu = g_menu_new ();
//	g_menu_append (menu, "Preferences", "app.preferences");
	g_menu_append (menu, "About", "app.about");
	g_menu_append (menu, "Quit", "app.quit");
	gtk_application_set_app_menu (GTK_APPLICATION (app), G_MENU_MODEL (menu));
	g_object_unref (menu);

	if (userData != NULL)
	{
		parseRetn = parseTrackXML (trackCtrl, (char *)userData, 0);
	}
	else
	{
		char configPath[1025], *home;
		struct stat statBuff;

		home = getenv ("HOME");
		strncpy (configPath, home, 1024);
		strncat (configPath, "/.traincontrolrc.xml", 1024 - strlen (configPath));
		if (stat (configPath, &statBuff) == 0)
		{
			parseRetn = parseTrackXML (trackCtrl, configPath, 0);
		}
		else
		{
			strcpy (configPath, "/etc/traincontrolrc.xml");
			if (stat (configPath, &statBuff) == 0)
			{
				parseRetn = parseTrackXML (trackCtrl, configPath, 0);
			}
		}
	}
	if (!parseRetn)
	{
		parseRetn = parseMemoryXML (trackCtrl, NULL);
	}
	if (parseRetn)
	{
		if (startConnectThread (trackCtrl))
		{
			sprintf (tempBuff, "Track Control - %s", trackCtrl -> trackName);
			trackCtrl -> windowCtrl = gtk_application_window_new (app);
			g_signal_connect (trackCtrl -> windowCtrl, "destroy", G_CALLBACK (windowDestroy), trackCtrl);
			gtk_window_set_title (GTK_WINDOW (trackCtrl -> windowCtrl), tempBuff);
			gtk_window_set_icon_name (GTK_WINDOW (trackCtrl -> windowCtrl), "preferences-desktop");
			gtk_window_set_default_size (GTK_WINDOW (trackCtrl -> windowCtrl), 300, 600);

			vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 5);
			gtk_container_add (GTK_CONTAINER (trackCtrl -> windowCtrl), vbox);

			hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 5);
			gtk_widget_set_halign (hbox, GTK_ALIGN_CENTER);
			gtk_container_add (GTK_CONTAINER (vbox), hbox);

			trackCtrl -> labelPower = gtk_label_new ("Power");
			gtk_container_add (GTK_CONTAINER (hbox), trackCtrl -> labelPower);
			trackCtrl -> buttonPower = gtk_switch_new();
			gtk_switch_set_active (GTK_SWITCH (trackCtrl -> buttonPower), FALSE);
			g_signal_connect (trackCtrl -> buttonPower, "notify::active", G_CALLBACK (trackPower), trackCtrl);
			gtk_widget_set_halign (trackCtrl -> buttonPower, GTK_ALIGN_CENTER);
			gtk_container_add (GTK_CONTAINER (hbox), trackCtrl -> buttonPower);

			hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 5);
			gtk_widget_set_halign (hbox, GTK_ALIGN_CENTER);
			gtk_container_add (GTK_CONTAINER (vbox), hbox);

			trackCtrl -> buttonTrack = gtk_button_new_with_label ("Track View");
			g_signal_connect (trackCtrl -> buttonTrack, "clicked", G_CALLBACK (displayTrack), trackCtrl);
			gtk_widget_set_halign (trackCtrl -> buttonTrack, GTK_ALIGN_CENTER);
			gtk_container_add (GTK_CONTAINER (hbox), trackCtrl -> buttonTrack);

			trackCtrl -> buttonStatus = gtk_button_new_with_label ("Status");
			g_signal_connect (trackCtrl -> buttonStatus, "clicked", G_CALLBACK (connectStatus), trackCtrl);
			gtk_widget_set_halign (trackCtrl -> buttonStatus, GTK_ALIGN_CENTER);
			gtk_container_add (GTK_CONTAINER (hbox), trackCtrl -> buttonStatus);

			trackCtrl -> buttonProgram = gtk_button_new_with_label ("Program");
			g_signal_connect (trackCtrl -> buttonProgram, "clicked", G_CALLBACK (programTrain), trackCtrl);
			gtk_widget_set_halign (trackCtrl -> buttonProgram, GTK_ALIGN_CENTER);
			gtk_container_add (GTK_CONTAINER (hbox), trackCtrl -> buttonProgram);
			gtk_container_add (GTK_CONTAINER (vbox), gtk_separator_new (GTK_ORIENTATION_HORIZONTAL));



			grid = gtk_grid_new();
			gtk_container_add (GTK_CONTAINER (vbox), grid);

			for (i = 0; i < trackCtrl -> trainCount; ++i)
			{
				sprintf (tempBuff, "%d", trackCtrl -> trainCtrl[i].trainNum);
				trackCtrl -> trainCtrl[i].buttonNum = gtk_button_new_with_label (tempBuff);
				g_object_set_data (G_OBJECT(trackCtrl -> trainCtrl[i].buttonNum), "train", &trackCtrl -> trainCtrl[i]);
				g_signal_connect (trackCtrl -> trainCtrl[i].buttonNum, "clicked", G_CALLBACK (trainInfo), trackCtrl);
				gtk_widget_set_hexpand (trackCtrl -> trainCtrl[i].buttonNum, TRUE);
				gtk_widget_set_halign (trackCtrl -> trainCtrl[i].buttonNum, GTK_ALIGN_FILL);
				gtk_grid_attach(GTK_GRID(grid), trackCtrl -> trainCtrl[i].buttonNum, i, 0, 1, 1);

				trackCtrl -> trainCtrl[i].buttonHalt = gtk_button_new_with_label ("Halt");
				g_object_set_data (G_OBJECT(trackCtrl -> trainCtrl[i].buttonHalt), "train", &trackCtrl -> trainCtrl[i]);
				g_signal_connect (trackCtrl -> trainCtrl[i].buttonHalt, "clicked", G_CALLBACK (haltTrain), trackCtrl);
				gtk_widget_set_halign (trackCtrl -> trainCtrl[i].buttonHalt, GTK_ALIGN_FILL);
				gtk_grid_attach(GTK_GRID(grid), trackCtrl -> trainCtrl[i].buttonHalt, i, 1, 1, 1);

				trackCtrl -> trainCtrl[i].scaleSpeed = gtk_scale_new_with_range (GTK_ORIENTATION_VERTICAL, 0, 126, 0.5);
				g_object_set_data (G_OBJECT(trackCtrl -> trainCtrl[i].scaleSpeed), "train", &trackCtrl -> trainCtrl[i]);
				g_signal_connect (trackCtrl -> trainCtrl[i].scaleSpeed, "value-changed", G_CALLBACK (moveTrain), trackCtrl);
				gtk_widget_set_vexpand (trackCtrl -> trainCtrl[i].scaleSpeed, 1);
				gtk_scale_set_value_pos (GTK_SCALE(trackCtrl -> trainCtrl[i].scaleSpeed), 0);
				gtk_scale_set_value_pos (GTK_SCALE(trackCtrl -> trainCtrl[i].scaleSpeed), GTK_POS_TOP);
				gtk_scale_set_digits (GTK_SCALE(trackCtrl -> trainCtrl[i].scaleSpeed), 0);
				gtk_widget_set_halign (trackCtrl -> trainCtrl[i].scaleSpeed, GTK_ALIGN_CENTER);
				gtk_grid_attach(GTK_GRID(grid), trackCtrl -> trainCtrl[i].scaleSpeed, i, 2, 1, 1);
			    gettimeofday(&trackCtrl -> trainCtrl[i].lastChange, NULL);

				trackCtrl -> trainCtrl[i].checkDir = gtk_check_button_new_with_label ("Reverse");
				g_object_set_data (G_OBJECT(trackCtrl -> trainCtrl[i].checkDir), "train", &trackCtrl -> trainCtrl[i]);
				g_signal_connect (trackCtrl -> trainCtrl[i].checkDir, "clicked", G_CALLBACK (reverseTrain), trackCtrl);
				gtk_widget_set_halign (trackCtrl -> trainCtrl[i].checkDir, GTK_ALIGN_CENTER);
				gtk_grid_attach(GTK_GRID(grid), trackCtrl -> trainCtrl[i].checkDir, i, 3, 1, 1);

				trackCtrl -> trainCtrl[i].buttonStop = gtk_button_new_with_label ("STOP");
				g_object_set_data (G_OBJECT(trackCtrl -> trainCtrl[i].buttonStop), "train", &trackCtrl -> trainCtrl[i]);
				g_signal_connect (trackCtrl -> trainCtrl[i].buttonStop, "clicked", G_CALLBACK (stopTrain), trackCtrl);
				gtk_widget_set_halign (trackCtrl -> trainCtrl[i].buttonStop, GTK_ALIGN_FILL);
				gtk_grid_attach(GTK_GRID(grid), trackCtrl -> trainCtrl[i].buttonStop, i, 4, 1, 1);

			}
			checkPowerOn (trackCtrl);

			gtk_container_add (GTK_CONTAINER (vbox), gtk_separator_new (GTK_ORIENTATION_HORIZONTAL));
			trackCtrl -> statusBar = gtk_statusbar_new();
			gtk_container_add (GTK_CONTAINER (vbox), trackCtrl -> statusBar);
			gtk_statusbar_push (GTK_STATUSBAR (trackCtrl -> statusBar), 1, "Train control started");

			gtk_widget_show_all (trackCtrl -> windowCtrl);
			g_timeout_add (100, clockTickCallback, trackCtrl);
		}
		else
		{
			fprintf (stderr, "Unable to start connection thread.\n");
		}
	}
	else
	{
		fprintf (stderr, "Unable to read configuration.\n");
	}
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  O P E N  F I L E S                                                                                                *
 *  ==================                                                                                                *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  \brief Called if the command line has a file on it.
 *  \param application Current application.
 *  \param files Files to be read.
 *  \param n_files Number of files.
 *  \param hint Not used.
 *  \result None.
 */
void openFiles (GtkApplication *application, GFile **files, gint n_files, const gchar *hint)
{
	gchar *uri = g_file_get_uri (files[0]);
	activate (application, uri);
	g_free (uri);
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  S H U T D O W N                                                                                                   *
 *  ===============                                                                                                   *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  \brief Shut down the whole application.
 *  \param app Not used.
 *  \param userData Not Used.
 *  \result None.
 */
static void shutdown (GtkApplication *app, gpointer userData)
{
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  A B O U T  C A L L B A C K                                                                                        *
 *  ==========================                                                                                        *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  \brief Currently not used.
 *  \param action Not used.
 *  \param parameter Not used.
 *  \param userData Not used.
 *  \result None.
 */
static void aboutCallback (GSimpleAction *action, GVariant *parameter, gpointer userData)
{
	const gchar *authors[] =
	{
		"Chris Knight",
		NULL
	};

	gtk_show_about_dialog (GTK_WINDOW (NULL),
			"program-name", "Train Control",
			"version", g_strdup_printf ("Version: %s\nRunning against GTK+ %d.%d.%d",
			 VERSION,
				 gtk_get_major_version (),
				 gtk_get_minor_version (),
				 gtk_get_micro_version ()),
			"copyright", "Copyright © 2018 - 2019 TheKnight",
			"license-type", GTK_LICENSE_LGPL_2_1,
			"website", "http://www.theknight.co.uk",
			"comments", "Program to control trains with DCC++.",
			"authors", authors,
			"logo", defaultIcon,
			"title", "About Train Control",
			NULL);
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  Q U I T  C A L L B A C K                                                                                          *
 *  ========================                                                                                          *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  \brief Called when quit selected on the menu.
 *  \param action Not used.
 *  \param parameter Not used.
 *  \param data Not used.
 *  \result None.
 */
void
quitCallback (GSimpleAction *action, GVariant *parameter, gpointer data)
{
	g_application_quit (data);
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  M A I N                                                                                                           *
 *  =======                                                                                                           *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  \brief The program starts here.
 *  \param argc The number of arguments passed to the program.
 *  \param argv Pointers to the arguments passed to the program.
 *  \result 0 (zero) if all processed OK.
 */
int main (int argc, char **argv)
{
	int status = 1;
	GtkApplication *app;

	app = gtk_application_new ("Train.Control", G_APPLICATION_HANDLES_OPEN); // G_APPLICATION_FLAGS_NONE);
	g_signal_connect (app, "activate", G_CALLBACK (activate), NULL);
	g_signal_connect (app, "open", G_CALLBACK (openFiles), NULL);
	g_signal_connect (app, "shutdown", G_CALLBACK (shutdown), NULL);

	defaultIcon = gdk_pixbuf_new_from_xpm_data ((const char **) &train_xpm);
	gtk_window_set_default_icon_name ("train_xpm");

	status = g_application_run (G_APPLICATION (app), argc, argv);
	g_object_unref (app);
	return status;
}

