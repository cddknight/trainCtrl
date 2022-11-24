/**********************************************************************************************************************
 *                                                                                                                    *
 *  T R A I N  C O N T R O L . C                                                                                      *
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
 *  \brief Graphical DCC++ controller interface.
 */
#include <gtk/gtk.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>

#include "config.h"
#include "buildDate.h"
#include "trainControl.h"
#include "socketC.h"

static char *notConnected = "Not connected to the train controller";
static const GdkRGBA blackCol = { 0.1, 0.1, 0.1, 1.0 };
static const GdkRGBA trackCol = { 0.7, 0.7, 0.7, 1.0 };
static const GdkRGBA trFillCol = { 0.0, 0.5, 0.0, 1.0 };
static const GdkRGBA bufferCol = { 0.6, 0.0, 0.0, 1.0 };
static const GdkRGBA inactCol = { 0.6, 0.0, 0.0, 1.0 };
static const GdkRGBA iaFillCol = { 0.2, 0.0, 0.0, 1.0 };
static const GdkRGBA circleCol = { 0.8, 0.8, 0.8, 1.0 };
static const GdkRGBA sigRedCol = { 0.9, 0.0, 0.0, 1.0 };
static const GdkRGBA sigGrnCol = { 0.0, 0.9, 0.0, 1.0 };
static const GdkRGBA sigOffCol = { 0.2, 0.2, 0.2, 1.0 };
static const GdkRGBA sigOutCol = { 0.8, 0.8, 0.8, 1.0 };
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

#define UPDATE_HOLD	500

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
	gettimeofday (&now, NULL);
	return diffTime (start, &now);
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  C H E C K  C O N N E C T E D                                                                                      *
 *  ============================                                                                                      *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  \brief Check to see if we are connected to daemon.
 *  \param trackCtrl Track configuration.
 *  \result 1 if connected.
 */
int checkConnected (trackCtrlDef *trackCtrl)
{
	int retn = 1;
	if (trackCtrl -> serverHandle == -1)
	{
		GtkWidget *errDialog = gtk_message_dialog_new (GTK_WINDOW (trackCtrl -> windowCtrl),
				GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
				GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE,
				"%s", notConnected);
		gtk_dialog_run (GTK_DIALOG (errDialog));
		gtk_widget_destroy (errDialog);
		retn = 0;
	}
	return retn;
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  S E N D  B U T T O N  F U N C                                                                                     *
 *  =============================                                                                                     *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  \brief Send a function that is assigned to a button.
 *  \param widget Sending button.
 *  \param data Pointer to track config.
 *  \result None.
 */
static void sendButtonFunc (GtkWidget *widget, gpointer data)
{
	trackCtrlDef *trackCtrl = (trackCtrlDef *)g_object_get_data (G_OBJECT(widget), "track");
	trainCtrlDef *train = (trainCtrlDef *)g_object_get_data (G_OBJECT(widget), "train");
	long index = (long)g_object_get_data (G_OBJECT(widget), "index");

	int funcID = train -> trainFunc[index].funcID;
	int active = gtk_switch_get_active (GTK_SWITCH (widget)) ? 1 : 0;

	if (active != train -> funcState[funcID])
	{
		trainToggleFunction (trackCtrl, train, funcID, active);
	}
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  C L O S E  F U N C T I O N S                                                                                      *
 *  ============================                                                                                      *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  \brief Function window has bee closed so clean up.
 *  \param widget Not used.
 *  \param data Pointer to the track.
 *  \result None.
 */
static void closeFunctions (GtkWidget *widget, gpointer data)
{
	int j, i;
	trackCtrlDef *trackCtrl = (trackCtrlDef *)data;

	for (i = 0; i < trackCtrl -> trainCount; ++i)
	{
		trainCtrlDef *train = &trackCtrl -> trainCtrl[i];
		for (j = 0; j < train -> funcCount; ++j)
		{
			train -> trainFunc[j].funcSwitch = NULL;
		}
	}
	trackCtrl -> windowFunctions = NULL;
	trackCtrl -> funcSpinner = NULL;
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  T R A I N  F U N C T I O N S                                                                                      *
 *  ============================                                                                                      *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  \brief Display information about the train on the status bar.
 *  \param widget Widget calling this.
 *  \param data Track data.
 *  \result None.
 */
static void trainFunctions (GtkWidget *widget, gpointer data)
{
	char tempBuff[181];
	trackCtrlDef *trackCtrl = (trackCtrlDef *)data;
	trainCtrlDef *train = (trainCtrlDef *)g_object_get_data (G_OBJECT(widget), "train");

	if (!checkConnected (trackCtrl))
		return;

	if (trackCtrl -> windowFunctions != NULL)
		gtk_widget_destroy (trackCtrl -> windowFunctions);

	if (trackCtrl -> windowFunctions == NULL && train -> funcCount)
	{
		long i;
		int row = 0;
		char tempBuff[81];
		GtkWidget *label, *grid, *vbox, *button;

		trackCtrl -> windowFunctions = gtk_window_new (GTK_WINDOW_TOPLEVEL);
		gtk_window_set_title (GTK_WINDOW (trackCtrl -> windowFunctions), "Functions");
		g_signal_connect (G_OBJECT (trackCtrl -> windowFunctions), "destroy", G_CALLBACK (closeFunctions), trackCtrl);

		sprintf (tempBuff, "Functions for %d", train -> trainNum);
		gtk_window_set_title (GTK_WINDOW (trackCtrl -> windowFunctions), tempBuff);
		gtk_window_set_transient_for (GTK_WINDOW (trackCtrl -> windowFunctions), GTK_WINDOW (trackCtrl -> windowCtrl));
		if (!gtk_window_set_icon_from_file (GTK_WINDOW (trackCtrl -> windowFunctions),
				"/usr/share/pixmaps/traincontrol.svg", NULL))
		{
			gtk_window_set_icon_from_file (GTK_WINDOW (trackCtrl -> windowFunctions),
					"/usr/share/pixmaps/traincontrol.png", NULL);
		}

		vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
		gtk_container_set_border_width (GTK_CONTAINER(vbox), 10);
		gtk_widget_set_halign (vbox, GTK_ALIGN_FILL);
		gtk_widget_set_valign (vbox, GTK_ALIGN_FILL);
		gtk_container_add (GTK_CONTAINER (trackCtrl -> windowFunctions), vbox);

		grid = gtk_grid_new();
		gtk_widget_set_halign (grid, GTK_ALIGN_FILL);
		gtk_widget_set_valign (grid, GTK_ALIGN_FILL);
		gtk_grid_set_row_spacing (GTK_GRID (grid), 5);
		gtk_grid_set_column_spacing (GTK_GRID (grid), 6);
		gtk_box_pack_start (GTK_BOX (vbox), grid, TRUE, TRUE, 0);

		for (i = 0; i < train -> funcCount; ++i)
		{
			sprintf (tempBuff, "%s", train -> trainFunc[i].funcDesc);
			label = gtk_label_new (tempBuff);
			gtk_widget_set_halign (label, GTK_ALIGN_END);
			gtk_grid_attach (GTK_GRID(grid), label, 0, row, 1, 1);

			train -> trainFunc[i].funcSwitch = button = gtk_switch_new();
			g_object_set_data (G_OBJECT(button), "track", trackCtrl);
			g_object_set_data (G_OBJECT(button), "train", train);
			g_object_set_data (G_OBJECT(button), "index", (void *)i);
			gtk_widget_set_halign (button, GTK_ALIGN_START);
			g_signal_connect (button, "notify::active", G_CALLBACK (sendButtonFunc), trackCtrl);
			gtk_grid_attach (GTK_GRID(grid), button, 1, row++, 1, 1);
		}
		gtk_widget_show_all (trackCtrl -> windowFunctions);
	}
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
		if (trainSetSpeed (trackCtrl, train, 0))
		{
			train -> curSpeed = train -> remoteCurSpeed = 0;
			gtk_range_set_value (GTK_RANGE (train -> scaleSpeed), 0.0);
		}
		else
		{
			gtk_statusbar_push (GTK_STATUSBAR (trackCtrl -> statusBar), 1, notConnected);
		}
	}
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  S L O W  T R A I N                                                                                                *
 *  ==================                                                                                                *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  \brief Set the speed of the train to it's slow speed.
 *  \param widget Calling widget.
 *  \param data Passed data, pointer to track config.
 *  \result None.
 */
static void slowTrain (GtkWidget *widget, gpointer data)
{
	trackCtrlDef *trackCtrl = (trackCtrlDef *)data;
	trainCtrlDef *train = (trainCtrlDef *)g_object_get_data (G_OBJECT(widget), "train");

	if (train -> curSpeed != train -> slowSpeed)
	{
		if (trainSetSpeed (trackCtrl, train, train -> slowSpeed))
		{
			train -> curSpeed = train -> remoteCurSpeed = train -> slowSpeed;
			gtk_range_set_value (GTK_RANGE (train -> scaleSpeed), train -> slowSpeed);
		}
		else
		{
			gtk_statusbar_push (GTK_STATUSBAR (trackCtrl -> statusBar), 1, notConnected);
		}
	}
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  A C T I V E  T R A I N                                                                                            *
 *  ======================                                                                                            *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  \brief Radio button is changing the current train.
 *  \param widget Which train is being selected.
 *  \param data Which train is being selected.
 *  \result None.
 */
static void activeTrain (GtkWidget *widget, gpointer data)
{
	trackCtrlDef *trackCtrl = (trackCtrlDef *)data;
	throttleDef *throttle = (throttleDef *)g_object_get_data (G_OBJECT(widget), "throttle");
	int selected = gtk_combo_box_get_active (GTK_COMBO_BOX (throttle -> trainSelect));
	throttle -> activeTrain = &trackCtrl -> trainCtrl[selected];
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
		trainCtrlDef *train = &trackCtrl -> trainCtrl[i];
		if (train != NULL)
		{
			if (train -> buttonNum != NULL)
				gtk_widget_set_sensitive (train -> buttonNum, state);
			if (train -> buttonHalt != NULL)
				gtk_widget_set_sensitive (train -> buttonHalt, state);
			if (train -> buttonSlow != NULL)
				gtk_widget_set_sensitive (train -> buttonSlow, state);
			if (train -> scaleSpeed != NULL)
				gtk_widget_set_sensitive (train -> scaleSpeed, state);
			if (train -> checkDir != NULL)
				gtk_widget_set_sensitive (train -> checkDir, state);
		}
	}
	for (i = 0; i < trackCtrl -> throttleCount; ++i)
	{
		if (trackCtrl -> throttles[i].trainSelect != NULL)
			gtk_widget_set_sensitive (trackCtrl -> throttles[i].trainSelect, state);
	}
	if (trackCtrl -> buttonProgram != NULL)
		gtk_widget_set_sensitive (trackCtrl -> buttonProgram, state);

	if (trackCtrl -> buttonStopAll != NULL)
		gtk_widget_set_sensitive (trackCtrl -> buttonStopAll, state);
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
	int i, newState, okSend = 0;
	char tempBuff[81];
	trackCtrlDef *trackCtrl = (trackCtrlDef *)data;

	newState = (trackCtrl -> powerState == POWER_ON ? POWER_OFF : POWER_ON);

	if (newState == POWER_ON)
		okSend = trainConnectSend (trackCtrl, "<1 MAIN>", 8) == 8;
	else
		okSend = trainConnectSend (trackCtrl, "<0>", 3) == 3;

	if (okSend)
	{
		trackCtrl -> powerState = newState;
		sprintf (tempBuff, "Track power %s", trackCtrl -> powerState == POWER_ON ? "On" : "Off");
		gtk_statusbar_push (GTK_STATUSBAR (trackCtrl -> statusBar), 1, tempBuff);

		if (trackCtrl -> powerState != trackCtrl -> remotePowerState)
		{
			trackCtrl -> remotePowerState = newState;
			for (i = 0; i < trackCtrl -> trainCount; ++i)
			{
				trainCtrlDef *train = &trackCtrl -> trainCtrl[i];
				if (train -> curSpeed > 0 && trackCtrl -> powerState == POWER_OFF)
				{
					trainSetSpeed (trackCtrl, train, -1);
					train -> reverse = train -> remoteReverse = 0;
					train -> curSpeed = train -> remoteCurSpeed = 0;
					gtk_range_set_value (GTK_RANGE (train -> scaleSpeed), 0.0);
					gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (train -> checkDir), train -> reverse);
				}
			}
		}
		checkPowerOn (trackCtrl);
	}
	else
	{
		if (newState == POWER_ON)
			gtk_switch_set_active (GTK_SWITCH(trackCtrl -> buttonPower), FALSE);

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
	trackCtrlDef *trackCtrl = (trackCtrlDef *)data;
	trainCtrlDef *train = (trainCtrlDef *)g_object_get_data (G_OBJECT(widget), "train");

	int value = (int)gtk_range_get_value (GTK_RANGE (train -> scaleSpeed));
	if (train -> curSpeed != value)
	{
		if (trainSetSpeed (trackCtrl, train, value))
		{
			gettimeofday (&train -> lastChange, NULL);
			train -> curSpeed = train -> remoteCurSpeed = value;
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
	int newState;
	trackCtrlDef *trackCtrl = (trackCtrlDef *)data;
	trainCtrlDef *train = (trainCtrlDef *)g_object_get_data (G_OBJECT(widget), "train");

	newState = (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (train -> checkDir)) == TRUE ? 1 : 0);
	if (newState != train -> reverse)
	{
		char tempBuff[81];
		train -> reverse = train -> remoteReverse = newState;
		if (trainSetSpeed (trackCtrl, train, 0))
		{
			train -> curSpeed = train -> remoteCurSpeed = 0;
			gtk_range_set_value (GTK_RANGE (train -> scaleSpeed), 0.0);
		}
		sprintf (tempBuff, "Set reverse %s for train %d", (train -> reverse ? "On" : "Off"), train -> trainNum);
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
			int lineType, xPos[5]={0,0,0,0,0}, yPos[5]={0,0,0,0,0}, posMask = 0, saveVal = 0;
			int posn = (i * cols) + j, count = 0, loop;

			for (loop = 0; loop < 8; ++loop)
			{
				if (trackCtrl -> trackLayout -> trackCells[posn].layout & (1 << loop))
				{
					++count;
					if (trackCtrl -> trackLayout -> trackCells[posn].point.point & (1 << loop))
					{
						if (!(trackCtrl -> trackLayout -> trackCells[posn].point.state & (1 << loop)))
						{
							xPos[4] = (j * cellSize) + xChangeMod[loop];
							yPos[4] = (i * cellSize) + yChangeMod[loop];
							posMask |= 16;
						}
						else if (saveVal < 4)
						{
							xPos[saveVal] = (j * cellSize) + xChangeMod[loop];
							yPos[saveVal] = (i * cellSize) + yChangeMod[loop];
							posMask |= (1 << saveVal++);
						}
					}
					else
					{
						if (saveVal < 4)
						{
							xPos[saveVal] = (j * cellSize) + xChangeMod[loop];
							yPos[saveVal] = (i * cellSize) + yChangeMod[loop];
							posMask |= (1 << saveVal++);
						}
					}
				}
				if (trackCtrl -> trackLayout -> trackCells[posn].signal.signal & (1 << loop))
				{
					cairo_save (cr);
					if (trackCtrl -> trackLayout -> trackCells[posn].signal.state == 1)
						gdk_cairo_set_source_rgba (cr, &sigRedCol);
					else if (trackCtrl -> trackLayout -> trackCells[posn].signal.state == 2)
						gdk_cairo_set_source_rgba (cr, &sigGrnCol);
					else
						gdk_cairo_set_source_rgba (cr, &sigOffCol);

					cairo_arc (cr,
							(j * cellSize) + (cellSize >> 2) + (xChangeMod[loop] >> 1),
							(i * cellSize) + (cellSize >> 2) + (yChangeMod[loop] >> 1),
							(double)cellSize / 8.5, 0, 2 * G_PI);
					cairo_fill (cr);
					cairo_stroke (cr);

					gdk_cairo_set_source_rgba (cr, &sigOutCol);
					cairo_arc (cr,
							(j * cellSize) + (cellSize >> 2) + (xChangeMod[loop] >> 1),
							(i * cellSize) + (cellSize >> 2) + (yChangeMod[loop] >> 1),
							(double)cellSize / 8.5, 0, 2 * G_PI);
					cairo_stroke (cr);

					cairo_restore (cr);
				}
			}
			if (posMask & 16)
			{
				for (lineType = 0; lineType < 2; ++lineType)
				{
					cairo_save (cr);
					cairo_set_line_join (cr, CAIRO_LINE_JOIN_ROUND);
					gdk_cairo_set_source_rgba (cr, lineType ? &iaFillCol : &inactCol);
					cairo_set_line_width (cr, lineWidths[lineType]);
					cairo_move_to (cr, (j * cellSize) + cellHalf, (i * cellSize) + cellHalf);
					cairo_line_to (cr, xPos[4], yPos[4]);
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
					cairo_set_line_width (cr, lineWidths[lineType]);
					cairo_move_to (cr, xPos[0], yPos[0]);
					cairo_line_to (cr, (j * cellSize) + cellHalf, (i * cellSize) + cellHalf);
					if (posMask & 2)
					{
						cairo_line_to (cr, xPos[1], yPos[1]);
						if (posMask & 4)
						{
							cairo_move_to (cr, xPos[2], yPos[2]);
							cairo_line_to (cr, (j * cellSize) + cellHalf, (i * cellSize) + cellHalf);
							if (posMask & 8)
							{
								cairo_line_to (cr, xPos[3], yPos[3]);
							}
						}
					}
					cairo_stroke (cr);
					cairo_restore (cr);
				}
			}
			if (count == 1)
			{
				gdk_cairo_set_source_rgba (cr, &bufferCol);
				cairo_arc (cr, (j * cellSize) + cellHalf, (i * cellSize) + cellHalf, (double)cellSize / 8.5, 0, 2 * G_PI);
				cairo_fill (cr);
				cairo_stroke (cr);

				gdk_cairo_set_source_rgba (cr, &circleCol);
				cairo_arc (cr, (j * cellSize) + cellHalf, (i * cellSize) + cellHalf, (double)cellSize / 8.5, 0, 2 * G_PI);
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

				if (trackCtrl -> trackLayout -> trackCells[posn].point.point)
				{
					char tempBuff[81];
					trackCellDef *cell = &trackCtrl -> trackLayout -> trackCells[posn];
					unsigned short newState = cell -> point.point;

					newState &= ~(cell -> point.state);
					sprintf (tempBuff, "<Y %d %d %d>", cell -> point.server, cell -> point.ident,
							cell -> point.pointDef == newState ? 0 : 1);
					if (trainConnectSend (trackCtrl, tempBuff, strlen (tempBuff)) > 0)
					{
						/* This point is linked so change the other point */
						if (trackCtrl -> trackLayout -> trackCells[posn].point.link)
						{
							int i;
							for (i = 0; i < 8; ++i)
							{
								if (cell -> point.link & (1 << i))
								{
									int newLinkState, newPosn = posn + (cols * linkRow[i]) + linkCol[i];
									if (newPosn >= 0 && newPosn < (rows * cols))
									{
										trackCellDef *newCell = &trackCtrl -> trackLayout -> trackCells[newPosn];

										/* The new point should have a link, we hope to us */
										if (newCell -> point.link)
										{
											if (cell -> point.link == newState)
											{
												/* We are setting to the link, so set other point to the link */
												newLinkState = newCell -> point.link;
											}
											else
											{
												/* We are breaking the link, so set other point away from link */
												newLinkState = newCell -> point.point & ~newCell -> point.link;
											}
											sprintf (tempBuff, "<Y %d %d %d>", newCell -> point.server, newCell -> point.ident,
													newCell -> point.pointDef == newLinkState ? 0 : 1);
											trainConnectSend (trackCtrl, tempBuff, strlen (tempBuff));
										}
									}
								}
							}
						}
					}
				}
			}
			return TRUE;

		case GDK_BUTTON_SECONDARY:
			{
				int cols = trackCtrl -> trackLayout -> trackCols;
				int cellSize = trackCtrl -> trackLayout -> trackSize;
				int posn = (((int)event -> y / cellSize) * cols) + ((int)event -> x / cellSize);

				if (trackCtrl -> trackLayout -> trackCells[posn].signal.signal)
				{
					char tempBuff[81];
					trackCellDef *cell = &trackCtrl -> trackLayout -> trackCells[posn];
					int newState = (cell -> signal.state == 1 ? 2 : 1);
					sprintf (tempBuff, "<X %d %d %d>", cell -> signal.server, cell -> signal.ident, newState);
					trainConnectSend (trackCtrl, tempBuff, strlen (tempBuff));
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
	if (!checkConnected (trackCtrl))
		return;

	if (trackCtrl -> windowTrack == NULL)
	{
		GtkWidget *eventBox;
		int cellSize = trackCtrl -> trackLayout -> trackSize;
		int width = trackCtrl -> trackLayout -> trackCols * cellSize;
		int height = trackCtrl -> trackLayout -> trackRows * cellSize;

		trackCtrl -> windowTrack = gtk_window_new (GTK_WINDOW_TOPLEVEL);
		gtk_window_set_title (GTK_WINDOW (trackCtrl -> windowTrack), "Track Control");
		gtk_window_set_icon_from_file (GTK_WINDOW (trackCtrl -> windowTrack),
				"/usr/share/pixmaps/traincontrol.svg", NULL);
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
	else
	{
		gtk_window_present_with_time (GTK_WINDOW (trackCtrl -> windowTrack), time(NULL));
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
		if (cell -> point.point)
		{
			if (cell -> point.server == server && cell -> point.ident == point)
			{
				if (state == 0)
					cell -> point.state = cell -> point.pointDef;
				else
					cell -> point.state = cell-> point.point & ~(cell -> point.pointDef);

				break;
			}
		}
	}
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  U P D A T E  S I G N A L  S T A T E                                                                               *
 *  ===================================                                                                               *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  \brief Update the state of the signal.
 *  \param trackCtrl Pointer to the config.
 *  \param server Which server to update.
 *  \param signal Which signal to update.
 *  \param state New state of the signal.
 *  \result None.
 */
void updateSignalState (trackCtrlDef *trackCtrl, int server, int signal, int state)
{
	int i, cells = trackCtrl -> trackLayout -> trackRows * trackCtrl -> trackLayout -> trackCols;

	for (i = 0; i < cells; ++i)
	{
		trackCellDef *cell = &trackCtrl -> trackLayout -> trackCells[i];
		if (cell -> signal.signal)
		{
			if (cell -> signal.server == server && cell -> signal.ident == signal)
			{
				cell -> signal.state = state;
				if (trackCtrl -> windowTrack != NULL)
					gtk_widget_queue_draw (trackCtrl -> drawingArea);
				break;
			}
		}
	}
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  C O N N E C T I O N  S T A T U S                                                                                  *
 *  ================================                                                                                  *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  \brief Display the connection status from the server.
 *  \param widget Not used.
 *  \param data Pointer to track control.
 *  \result None.
 */
static void connectionStatus (GtkWidget *widget, gpointer data)
{
	int i = 0;
	GtkWidget *contentArea;
	GtkWidget *label, *grid, *vbox;
	trackCtrlDef *trackCtrl = (trackCtrlDef *)data;

	static char *connectionText[] =
	{
		"Server Address",
		"Server Port",
		"Serial Connected",
		"Points Connected",
		"Clients Connected",
		NULL
	};

	if (trackCtrl -> connectionDialog == NULL)
	{
		trackCtrl -> connectionDialog = gtk_dialog_new_with_buttons ("Connection Status",
				GTK_WINDOW (trackCtrl -> windowCtrl),
				GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
				"Update", GTK_RESPONSE_APPLY,
				"Close", GTK_RESPONSE_CLOSE,
				NULL);

		contentArea = gtk_dialog_get_content_area (GTK_DIALOG (trackCtrl -> connectionDialog));

		vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
		gtk_container_set_border_width (GTK_CONTAINER(vbox), 10);
		gtk_widget_set_halign (vbox, GTK_ALIGN_FILL);
		gtk_widget_set_valign (vbox, GTK_ALIGN_FILL);
		gtk_box_pack_start (GTK_BOX (contentArea), vbox, TRUE, TRUE, 0);

		grid = gtk_grid_new();
		gtk_widget_set_halign (grid, GTK_ALIGN_FILL);
		gtk_widget_set_valign (grid, GTK_ALIGN_FILL);
		gtk_grid_set_row_spacing (GTK_GRID (grid), 5);
		gtk_grid_set_column_spacing (GTK_GRID (grid), 6);
		gtk_box_pack_start (GTK_BOX (vbox), grid, TRUE, TRUE, 0);

		for (i = 0; i < 5; ++i)
		{
			label = gtk_label_new (connectionText[i]);
			gtk_widget_set_halign (label, GTK_ALIGN_END);
			gtk_grid_attach (GTK_GRID(grid), label, 0, i, 1, 1);
			trackCtrl -> connectionLabels[i] = gtk_label_new ("Unknown");
			gtk_widget_set_halign (trackCtrl -> connectionLabels[i], GTK_ALIGN_START);
			gtk_grid_attach (GTK_GRID(grid), trackCtrl -> connectionLabels[i], 1, i, 1, 1);
			if (i > 1)
				trackCtrl -> connectionStatus[i - 2] = -1;
		}
		trackCtrl -> connectionStatus[6] = 0;

		gtk_widget_show_all (trackCtrl -> connectionDialog);
		do
		{
			int sendRes = trainConnectSend (trackCtrl, "<V>", 3);

			for (i = 0; i < 5; ++i)
			{
				gtk_label_set_label (GTK_LABEL (trackCtrl -> connectionLabels[i]),
						sendRes == 3 ? "Pending" :
						i < 2 ? "Not connected" : "Unknown");
			}
		}
		while (gtk_dialog_run (GTK_DIALOG (trackCtrl -> connectionDialog)) == GTK_RESPONSE_APPLY);

		gtk_widget_destroy (trackCtrl -> connectionDialog);
		trackCtrl -> connectionDialog = NULL;
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
		retn = 1;

	gtk_widget_destroy (dialog);
	return retn;
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  S T O P  A L L  T R A I N S                                                                                       *
 *  ===========================                                                                                       *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  \brief Quickly stop all the running trains.
 *  \param widget Not used.
 *  \param data Pointer to the track configuration.
 *  \result None.
 */
static void stopAllTrains (GtkWidget *widget, gpointer data)
{
	int i;
	trackCtrlDef *trackCtrl = (trackCtrlDef *)data;

	trainConnectSend (trackCtrl, "<!>", 3);
	for (i = 0; i < trackCtrl -> trainCount; ++i)
	{
		trainCtrlDef *train = &trackCtrl -> trainCtrl[i];
		if (train -> curSpeed > 0)
		{
			trainSetSpeed (trackCtrl, train, -1);
			train -> reverse = train -> remoteReverse = 0;
			train -> curSpeed = train -> remoteCurSpeed = 0;
			gtk_range_set_value (GTK_RANGE (train -> scaleSpeed), 0.0);
			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (train -> checkDir), train -> reverse);
		}
	}
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
	#define SPINCOUNT 6

	trackCtrlDef *trackCtrl = (trackCtrlDef *)data;

	static char *controlLables[] =
	{
		"DCC Address",
		"CV Number (1 - 1024)",
		"Byte Value (0 - 255)",
		"Bit number (0 - 7)",
		"Bit Value (0 - 1)",
		"ACK Limit (10 - 100)",
		"Read current value",
		"Last reply",
		"None"
	};
	static char *hintLables[] =
	{
		"Programming Hints:",
		"+ If DCC address is zero then use the programming track.",
		"+ Reads only work with a train on the programming track.",
		"+ If bit number in the range 0 to 7 then set bit value.",
		"+ If bit number is -1 then set the byte value.",
		NULL
	};
	static double defValues[SPINCOUNT] = { 0.0, 1.0, 0.0, -1.0, 0.0, 60.0 };
	static double minValues[SPINCOUNT] = { 0.0, 1.0, 0.0, -1.0, 0.0, 10.0 };
	static double maxValues[SPINCOUNT] = { 232.0, 1025.0, 256.0, 8.0, 2.0, 101.0 };

	int i = 0;
	double ackLimit = 60;
	GtkWidget *contentArea;
	GtkWidget *spinner[SPINCOUNT];
	GtkAdjustment *adjust[SPINCOUNT];
	GtkEntryBuffer *entryBuffer;
	GtkWidget *label, *grid, *vbox, *checkButton;

	if (!checkConnected (trackCtrl))
		return;

	if (trackCtrl -> dialogProgram == NULL)
	{

		if (trainConnectSend (trackCtrl, "<1 PROG>", 8) != 8)
			return;

		trackCtrl -> dialogProgram = gtk_dialog_new_with_buttons ("Program Train",
				GTK_WINDOW (trackCtrl -> windowCtrl),
				GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
				"Apply", GTK_RESPONSE_APPLY,
				"Close", GTK_RESPONSE_CLOSE,
				NULL);

		contentArea = gtk_dialog_get_content_area (GTK_DIALOG (trackCtrl -> dialogProgram));

		vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
		gtk_container_set_border_width (GTK_CONTAINER(vbox), 10);
		gtk_widget_set_halign (vbox, GTK_ALIGN_FILL);
		gtk_widget_set_valign (vbox, GTK_ALIGN_FILL);
		gtk_box_pack_start (GTK_BOX (contentArea), vbox, TRUE, TRUE, 0);

		grid = gtk_grid_new();
		gtk_widget_set_halign (grid, GTK_ALIGN_FILL);
		gtk_widget_set_valign (grid, GTK_ALIGN_FILL);
		gtk_grid_set_row_spacing (GTK_GRID (grid), 5);
		gtk_grid_set_column_spacing (GTK_GRID (grid), 6);
		gtk_box_pack_start (GTK_BOX (vbox), grid, TRUE, TRUE, 0);

		while (hintLables[i] != NULL)
		{
			label = gtk_label_new (hintLables[i]);
			gtk_widget_set_halign (label, GTK_ALIGN_START);
			gtk_widget_set_valign (label, GTK_ALIGN_START);
			gtk_box_pack_start (GTK_BOX(vbox), label, FALSE, FALSE, 3);
			++i;
		}

		gtk_box_pack_start (GTK_BOX(vbox), gtk_separator_new (GTK_ORIENTATION_HORIZONTAL), FALSE, FALSE, 3);

		for (i = 0; i < SPINCOUNT; ++i)
		{
			label = gtk_label_new (controlLables[i]);
			gtk_widget_set_halign (label, GTK_ALIGN_END);
			gtk_grid_attach (GTK_GRID(grid), label, 0, i, 1, 1);

			adjust[i] = gtk_adjustment_new (defValues[i], minValues[i], maxValues[i], 1.0, 1.0, 1.0);
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

		entryBuffer = gtk_entry_buffer_new ("None", 4);
		trackCtrl -> entryProgram = gtk_entry_new_with_buffer (entryBuffer);
		g_object_set (trackCtrl -> entryProgram, "editable", FALSE, NULL);
		gtk_entry_set_width_chars (GTK_ENTRY (trackCtrl -> entryProgram), 30);
		gtk_grid_attach (GTK_GRID(grid), trackCtrl -> entryProgram, 1, ++i - 1, 1, 1);

		gtk_widget_show_all (trackCtrl -> dialogProgram);
		while (gtk_dialog_run (GTK_DIALOG (trackCtrl -> dialogProgram)) == GTK_RESPONSE_APPLY)
		{
			int values[SPINCOUNT], allOK = 0;
			char sendBuffer[41], msgBuffer[161];
			gboolean checkRead = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (checkButton));

			sendBuffer[0] = 0;
			for (i = 0; i < SPINCOUNT; ++i)
				values[i] = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON(spinner[i]));

			/* Has ACK limit changed */
			if (values[5] != ackLimit)
			{
				ackLimit = values[5];
				sprintf (sendBuffer, "<D ACK ON><D ACK LIMIT %d>", (int)ackLimit);
				trainConnectSend (trackCtrl, sendBuffer, strlen (sendBuffer));
				printf ("Sent: [%s]\n", sendBuffer);
				sendBuffer[0] = 0;
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
					else if (values[3] >= 0)
					{
						/* Write bit value */
						if (values[3] <= 7 && (values[4] == 0 || values[4] == 1))
						{
							/* Write bit value */
							sprintf (msgBuffer, "Write to CV#%d, bit %d, value %d on the programming track?",
									values[1], values[3], values[4]);
							if (programYesNo (trackCtrl, msgBuffer))
							{
								sprintf (sendBuffer, "<B %d %d %d %d 2>", values[1], values[3], values[4],
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
							sprintf (msgBuffer, "Write to CV#%d, byte value %d on the programming track?",
									values[1], values[2]);
							if (programYesNo (trackCtrl, msgBuffer))
								sprintf (sendBuffer, "<W %d %d %d 3>", values[1], values[2], trackCtrl -> serverSession);

							allOK = 1;
						}
					}
				}
			}
			/* Main track, need a CV# */
			else if (values[1] > 0 && values[1] < 1025)
			{
				if (values[3] >= 0)
				{
					/* Write bit value */
					if (values[3] <= 7 && (values[4] == 0 || values[4] == 1))
					{
						/* Write bit value */
						sprintf (msgBuffer, "Write to address %d, CV#%d, bit %d, value %d on the main track?",
								values[0], values[1], values[3], values[4]);
						if (programYesNo (trackCtrl, msgBuffer))
							sprintf (sendBuffer, "<b %d %d %d %d>", values[0], values[1], values[3], values[4]);

						allOK = 1;
					}
				}
				else
				{
					/* Write byte value */
					if (values[2] >= 0 && values[2] <= 255)
					{
						/* Write CV byte */
						sprintf (msgBuffer, "Write to address %d, CV number %d, byte value %d on the main track?",
								values[0], values[1], values[2]);
						if (programYesNo (trackCtrl, msgBuffer))
							sprintf (sendBuffer, "<w %d %d %d>", values[0], values[1], values[2]);

						allOK = 1;
					}
				}
			}
			if (allOK == 0)
			{
				gtk_entry_set_text (GTK_ENTRY (trackCtrl -> entryProgram), "Incorrect inputs, read notes.");
			}
			else if (sendBuffer[0] != 0)
			{
				if (trainConnectSend (trackCtrl, sendBuffer, strlen (sendBuffer)) > 0)
					gtk_entry_set_text (GTK_ENTRY (trackCtrl -> entryProgram), "Command sent to daemon");
				else
					gtk_entry_set_text (GTK_ENTRY (trackCtrl -> entryProgram), notConnected);

				sendBuffer[0] = 0;
			}
		}
		gtk_widget_destroy (trackCtrl -> dialogProgram);
		trackCtrl -> dialogProgram = NULL;
		trackCtrl -> entryProgram = NULL;
		trainConnectSend (trackCtrl, "<0 PROG>", 8);
	}
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  C H E C K  T H R O T T L E  S T A T E                                                                             *
 *  =====================================                                                                             *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  \brief Check to see if the joystick has been updated by the thread.
 *  \param trackCtrl Track config.
 *  \result None.
 */
void checkThrottleState (trackCtrlDef *trackCtrl)
{
	if (trackCtrl -> powerState == POWER_ON)
	{
		if (trackCtrl -> throttles != NULL)
		{
			int i;
			for (i = 0; i < trackCtrl -> throttleCount; ++i)
			{
				int newSpeed = -1, button = 0;
				throttleDef *throttle = &trackCtrl -> throttles[i];
				trainCtrlDef *train = throttle -> activeTrain;

				if (train != NULL)
				{
					pthread_mutex_lock (&trackCtrl -> throttleMutex);
					if (throttle -> curChanged)
					{
						if (diffTimeToNow (&throttle -> lastChange) > UPDATE_HOLD)
						{
							gettimeofday (&throttle -> lastChange, NULL);
							newSpeed = throttle -> curValue;
							throttle -> curChanged = 0;
						}
					}
					if (throttle -> buttonPress)
					{
						button = 1;
						throttle -> buttonPress = 0;
					}
					pthread_mutex_unlock (&trackCtrl -> throttleMutex);

					if (newSpeed != -1)
					{
						if (train -> curSpeed != newSpeed)
						{
							if (trainSetSpeed (trackCtrl, train, newSpeed))
							{
								train -> curSpeed = train -> remoteCurSpeed = newSpeed;
								gtk_range_set_value (GTK_RANGE (train -> scaleSpeed), newSpeed);
							}
							else
							{
								gtk_statusbar_push (GTK_STATUSBAR (trackCtrl -> statusBar), 1, notConnected);
							}
						}
					}
					if (button)
					{
						char tempBuff[81];
						int newState = train -> reverse ? 0 : 1;

						train -> reverse = train -> remoteReverse = newState;
						if (trainSetSpeed (trackCtrl, train, 0))
						{
							train -> curSpeed = train -> remoteCurSpeed = 0;
							gtk_range_set_value (GTK_RANGE (train -> scaleSpeed), 0.0);
							gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (train -> checkDir), train -> reverse);
						}
						sprintf (tempBuff, "Set reverse %s for train %d", (train -> reverse ? "On" : "Off"), train -> trainNum);
						gtk_statusbar_push (GTK_STATUSBAR (trackCtrl -> statusBar), 1, tempBuff);
					}
				}
			}
		}
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

	checkThrottleState (trackCtrl);
	if (trackCtrl -> powerState != trackCtrl -> remotePowerState)
	{
		gtk_switch_set_active (GTK_SWITCH(trackCtrl -> buttonPower), trackCtrl -> remotePowerState == POWER_ON ? TRUE : FALSE);
	}
	for (i = 0; i < trackCtrl -> trainCount; ++i)
	{
		trainCtrlDef *train = &trackCtrl -> trainCtrl[i];
		if (train -> curSpeed != train -> remoteCurSpeed)
		{
			if (diffTimeToNow (&train -> lastChange) > UPDATE_HOLD)
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
		if (trackCtrl -> windowFunctions != NULL)
		{
			int j;
			for (j = 0; j < train -> funcCount; ++j)
			{
				if (train -> trainFunc[j].funcSwitch != NULL)
				{
					int funcID = train -> trainFunc[j].funcID;
					int active = gtk_switch_get_active (GTK_SWITCH (train -> trainFunc[j].funcSwitch)) ? 1 : 0;

					if (active != train -> funcState[funcID])
					{
						gtk_switch_set_active (GTK_SWITCH (train -> trainFunc[j].funcSwitch), active ? FALSE : TRUE);
					}
				}
			}
		}
	}
	if (trackCtrl -> remoteProgMsg[0] != 0)
	{
		if (trackCtrl -> entryProgram != NULL)
		{
			gtk_entry_set_text (GTK_ENTRY (trackCtrl -> entryProgram), trackCtrl -> remoteProgMsg);
			trackCtrl -> remoteProgMsg[0] = 0;
		}
	}
	if (trackCtrl -> remoteCurrent != trackCtrl -> shownCurrent)
	{
		if (trackCtrl -> labelPower != NULL)
		{
			char tempBuff[81];
			if (trackCtrl -> powerState == POWER_ON && trackCtrl -> remoteCurrent >= 0)
				sprintf (tempBuff, "Power [%0.1f%%]", ((float)trackCtrl -> remoteCurrent * 100.0) / 1024.0);
			else
				strcpy (tempBuff, "Power");

			gtk_label_set_label (GTK_LABEL (trackCtrl -> labelPower), tempBuff);
			trackCtrl -> shownCurrent = trackCtrl -> remoteCurrent;
		}
	}
	if (trackCtrl -> connectionDialog != NULL)
	{
		if (trackCtrl -> connectionStatus[6] == 1)
		{
			char tempBuff[21] = "";

			if (trackCtrl -> serverHandle != -1)
				sprintf (tempBuff, "%d", trackCtrl -> serverPort);

			gtk_label_set_label (GTK_LABEL (trackCtrl -> connectionLabels[0]),
					trackCtrl -> serverHandle == -1 ? "Not connected" : trackCtrl -> addressBuffer);
			gtk_label_set_label (GTK_LABEL (trackCtrl -> connectionLabels[1]),
					trackCtrl -> serverHandle == -1 ? "Not connected" : tempBuff);

			gtk_label_set_label (GTK_LABEL (trackCtrl -> connectionLabels[2]),
					trackCtrl -> connectionStatus[0] > 0 ? "OK" : "Not connected");
			sprintf (tempBuff, "%d", trackCtrl -> connectionStatus[4]);
			gtk_label_set_label (GTK_LABEL (trackCtrl -> connectionLabels[3]), tempBuff);
			sprintf (tempBuff, "%d", trackCtrl -> connectionStatus[5]);
			gtk_label_set_label (GTK_LABEL (trackCtrl -> connectionLabels[4]), tempBuff);
			trackCtrl -> connectionStatus[6] = 0;
		}
	}
	if (trackCtrl -> windowTrack != NULL)
	{
		time_t now = time (NULL);
		if (now - trackCtrl -> trackRepaint > 2)
		{
			gtk_widget_queue_draw (trackCtrl -> drawingArea);
			trackCtrl -> trackRepaint = now;
		}
	}
	if (trackCtrl -> flags & TRACK_FLAG_SHOW && trackCtrl -> serverHandle != -1)
	{
		displayTrack (trackCtrl -> windowCtrl, trackCtrl);
		trackCtrl -> flags &= ~TRACK_FLAG_SHOW;
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
	trackCtrl -> ipVersion = USE_ANY;
	trackCtrl -> serverHandle = -1;
	trackCtrl -> conTimeout = 5;

	g_action_map_add_action_entries (G_ACTION_MAP (app), appEntries, G_N_ELEMENTS (appEntries), app);
	menu = g_menu_new ();
	g_menu_append (menu, "About Train Control", "app.about");
	g_menu_append (menu, "Quit", "app.quit");
	gtk_application_set_app_menu (GTK_APPLICATION (app), G_MENU_MODEL (menu));
	g_object_unref (menu);

	if (userData == NULL)
	{
		char configPath[1025], *home;
		struct stat statBuff;

		home = getenv ("HOME");
		strncpy (configPath, home, 1024);
		strncat (configPath, "/.trackrc.xml", 1024 - strlen (configPath));
		if (stat (configPath, &statBuff) == 0)
		{
			parseRetn = parseTrackXML (trackCtrl, configPath, 0);
		}
		else
		{
			strcpy (configPath, "/etc/train/trackrc.xml");
			if (stat (configPath, &statBuff) == 0)
			{
				parseRetn = parseTrackXML (trackCtrl, configPath, 0);
			}
		}
	}
	else
	{
		parseRetn = parseTrackXML (trackCtrl, (char *)userData, 0);
	}
	if (!parseRetn)
		parseRetn = parseMemoryXML (trackCtrl, NULL);

	if (parseRetn)
	{
		if (startThrottleThread (trackCtrl))
		{
			trackCtrl -> flags |= TRACK_FLAG_THRT;
		}
		if (startConnectThread (trackCtrl))
		{
			int screenWidth = trackCtrl -> trainCount * 100;
			int screenHeight = trackCtrl -> trackLayout -> trackRows * trackCtrl -> trackLayout -> trackSize;

			if (screenWidth < 300)
				screenWidth = 300;
			else if (screenWidth > 1900)
				screenWidth = 1900;
			if (screenHeight < 400)
				screenHeight = 400;
			else if (screenHeight > 1060)
				screenWidth = 1060;

			sprintf (tempBuff, "Track Control - %s", trackCtrl -> trackName);
			trackCtrl -> windowCtrl = gtk_application_window_new (app);
			g_signal_connect (trackCtrl -> windowCtrl, "destroy", G_CALLBACK (windowDestroy), trackCtrl);
			gtk_window_set_title (GTK_WINDOW (trackCtrl -> windowCtrl), tempBuff);
			if (!gtk_window_set_icon_from_file (GTK_WINDOW (trackCtrl -> windowCtrl),
					"/usr/share/pixmaps/traincontrol.svg",
					NULL))
			{
				gtk_window_set_icon_name (GTK_WINDOW (trackCtrl -> windowCtrl), "preferences-desktop");
			}
			gtk_window_set_default_size (GTK_WINDOW (trackCtrl -> windowCtrl), screenWidth, screenHeight);

			vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 5);
			gtk_container_set_border_width (GTK_CONTAINER(vbox), 5);
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

			trackCtrl -> buttonConnection = gtk_button_new_with_mnemonic ("S_tatus");
			g_signal_connect (trackCtrl -> buttonConnection, "clicked", G_CALLBACK (connectionStatus), trackCtrl);
			gtk_widget_set_halign (trackCtrl -> buttonConnection, GTK_ALIGN_CENTER);
			gtk_container_add (GTK_CONTAINER (hbox), trackCtrl -> buttonConnection);

			trackCtrl -> buttonTrack = gtk_button_new_with_mnemonic ("Track _View");
			g_signal_connect (trackCtrl -> buttonTrack, "clicked", G_CALLBACK (displayTrack), trackCtrl);
			gtk_widget_set_halign (trackCtrl -> buttonTrack, GTK_ALIGN_CENTER);
			gtk_container_add (GTK_CONTAINER (hbox), trackCtrl -> buttonTrack);

			trackCtrl -> buttonProgram = gtk_button_new_with_mnemonic ("_Program");
			g_signal_connect (trackCtrl -> buttonProgram, "clicked", G_CALLBACK (programTrain), trackCtrl);
			gtk_widget_set_halign (trackCtrl -> buttonProgram, GTK_ALIGN_CENTER);
			gtk_container_add (GTK_CONTAINER (hbox), trackCtrl -> buttonProgram);

			trackCtrl -> buttonStopAll = gtk_button_new_with_mnemonic ("_STOP!");
			g_signal_connect (trackCtrl -> buttonStopAll, "clicked", G_CALLBACK (stopAllTrains), trackCtrl);
			gtk_widget_set_halign (trackCtrl -> buttonStopAll, GTK_ALIGN_CENTER);
			gtk_container_add (GTK_CONTAINER (hbox), trackCtrl -> buttonStopAll);

			gtk_container_add (GTK_CONTAINER (vbox), gtk_separator_new (GTK_ORIENTATION_HORIZONTAL));

			grid = gtk_grid_new();
			gtk_container_add (GTK_CONTAINER (vbox), grid);

			for (i = 0; i < trackCtrl -> trainCount; ++i)
			{
				int j = 0;
				trainCtrlDef *train = &trackCtrl -> trainCtrl[i];

				if (train != NULL)
				{
					int r = 0;
					GtkAdjustment *adjust = gtk_adjustment_new (0, 0, 126, 1.0, 5.0, 0.0);

					sprintf (tempBuff, "%d", train -> trainNum);
					train -> buttonNum = gtk_button_new_with_label (tempBuff);
					g_object_set_data (G_OBJECT(train -> buttonNum), "train", train);
					g_signal_connect (train -> buttonNum, "clicked", G_CALLBACK (trainFunctions), trackCtrl);
					gtk_widget_set_hexpand (train -> buttonNum, TRUE);
					gtk_widget_set_halign (train -> buttonNum, GTK_ALIGN_FILL);
					gtk_grid_attach(GTK_GRID(grid), train -> buttonNum, i, r++, 1, 1);

					train -> buttonHalt = gtk_button_new_with_label ("Halt");
					g_object_set_data (G_OBJECT(train -> buttonHalt), "train", train);
					g_signal_connect (train -> buttonHalt, "clicked", G_CALLBACK (haltTrain), trackCtrl);
					gtk_widget_set_halign (train -> buttonHalt, GTK_ALIGN_FILL);
					gtk_grid_attach(GTK_GRID(grid), train -> buttonHalt, i, r++, 1, 1);

					if (trackCtrl -> flags & TRACK_FLAG_SLOW)
					{
						train -> buttonSlow = gtk_button_new_with_label ("Slow");
						g_object_set_data (G_OBJECT(train -> buttonSlow), "train", train);
						g_signal_connect (train -> buttonSlow, "clicked", G_CALLBACK (slowTrain), trackCtrl);
						gtk_widget_set_halign (train -> buttonSlow, GTK_ALIGN_FILL);
						gtk_grid_attach(GTK_GRID(grid), train -> buttonSlow, i, r++, 1, 1);
					}
					train -> checkDir = gtk_check_button_new_with_label ("Reverse");
					g_object_set_data (G_OBJECT(train -> checkDir), "train", train);
					g_signal_connect (train -> checkDir, "clicked", G_CALLBACK (reverseTrain), trackCtrl);
					gtk_widget_set_halign (train -> checkDir, GTK_ALIGN_CENTER);
					gtk_grid_attach(GTK_GRID(grid), train -> checkDir, i, r++, 1, 1);

					train -> scaleSpeed = gtk_scale_new (GTK_ORIENTATION_VERTICAL, adjust);
					g_object_set_data (G_OBJECT(train -> scaleSpeed), "train", train);
					g_signal_connect (train -> scaleSpeed, "value-changed", G_CALLBACK (moveTrain), trackCtrl);
					gtk_widget_set_vexpand (train -> scaleSpeed, 1);
					gtk_scale_set_value_pos (GTK_SCALE(train -> scaleSpeed), GTK_POS_TOP);
					gtk_scale_set_digits (GTK_SCALE(train -> scaleSpeed), 0);
					gtk_scale_set_has_origin (GTK_SCALE(train -> scaleSpeed), TRUE);
					for (j = 0; j < 126; j += 20)
						gtk_scale_add_mark (GTK_SCALE(train -> scaleSpeed), j, GTK_POS_LEFT, NULL);

					gtk_widget_set_halign (train -> scaleSpeed, GTK_ALIGN_CENTER);
					gtk_grid_attach (GTK_GRID(grid), train -> scaleSpeed, i, r++, 1, 1);
					gettimeofday (&train -> lastChange, NULL);
				}
			}
			if (trackCtrl -> flags & TRACK_FLAG_THRT && trackCtrl -> trainCount > 0)
			{
				int trainNum = 0;
				GtkWidget *throttleHBox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 5);
				gtk_widget_set_halign (throttleHBox, GTK_ALIGN_CENTER);
				gtk_container_add (GTK_CONTAINER (throttleHBox), gtk_label_new ("Train throttle:"));
				gtk_container_add (GTK_CONTAINER (vbox), throttleHBox);
				for (i = 0; i < trackCtrl -> throttleCount; ++i)
				{
					int j;
					if (i < trackCtrl -> trainCount)
					{
						trainNum = i;
					}
					sprintf (tempBuff, " T%d ", i + 1);
					gtk_container_add (GTK_CONTAINER (throttleHBox), gtk_label_new (tempBuff));

					throttleDef *throttle = &trackCtrl -> throttles[i];
					throttle -> trainSelect = gtk_combo_box_text_new ();
					for (j = 0; j < trackCtrl -> trainCount; ++j)
					{
						if (throttle -> defTrain != -1)
						{
							if (trackCtrl -> trainCtrl[j].trainNum == throttle -> defTrain)
							{
								trainNum = j;
							}
						}
						sprintf (tempBuff, "%d", trackCtrl -> trainCtrl[j].trainNum);
						gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (throttle -> trainSelect), tempBuff);
					}
					g_object_set_data (G_OBJECT(throttle -> trainSelect), "throttle", throttle);
					g_signal_connect (throttle -> trainSelect, "changed", G_CALLBACK (activeTrain), trackCtrl);
					gtk_combo_box_set_active (GTK_COMBO_BOX (throttle -> trainSelect), trainNum);
					gtk_container_add (GTK_CONTAINER (throttleHBox), throttle -> trainSelect);
				}
			}
			checkPowerOn (trackCtrl);

			gtk_container_add (GTK_CONTAINER (vbox), gtk_separator_new (GTK_ORIENTATION_HORIZONTAL));
			trackCtrl -> statusBar = gtk_statusbar_new();
			gtk_container_add (GTK_CONTAINER (vbox), trackCtrl -> statusBar);
			if (trackCtrl -> serverHandle == -1)
			{
				gtk_statusbar_push (GTK_STATUSBAR (trackCtrl -> statusBar), 1, notConnected);
			}
			else
			{
				gtk_statusbar_push (GTK_STATUSBAR (trackCtrl -> statusBar), 1, "Train control connected");
			}
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
			"version", g_strdup_printf ("Version: %s\nBuilt: %s",
				 VERSION, buildDate),
			"copyright", "Copyright © 2018 - 2022 TheKnight",
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

