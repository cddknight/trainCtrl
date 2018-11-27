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
#include <string.h>

#include "trainCtrl.h"
#include "socketC.h"

#define POWER_SRT	-1
#define POWER_OFF	0
#define POWER_ON	1

trackCtrlDef trackCtrl;
static char *notConnected = "Train controller not connected";

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
	trainCtrlDef *train = (trainCtrlDef *)data;
	if (train -> curSpeed != 0)
	{
		char tempBuff[81];
		sprintf (tempBuff, "<t %d %d %d %d>", train -> trainReg, train -> trainID, -1, train -> reverse);
		if (trainConnectSend (tempBuff, strlen (tempBuff)) > 0)
		{
			train -> curSpeed = 0;
			gtk_range_set_value (GTK_RANGE (train -> scaleSpeed), 0.0);
			sprintf (tempBuff, "Set speed: STOP for train %d", train -> trainNum);
			gtk_statusbar_push (GTK_STATUSBAR (trackCtrl.statusBar), 1, tempBuff);
		}
		else
		{
			gtk_statusbar_push (GTK_STATUSBAR (trackCtrl.statusBar), 1, notConnected);
		}
	}
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
	trainCtrlDef *train = (trainCtrlDef *)data;
	if (train -> curSpeed != 0)
	{
		char tempBuff[81];
		sprintf (tempBuff, "<t %d %d %d %d>", train -> trainReg, train -> trainID, 0, train -> reverse);
		if (trainConnectSend (tempBuff, strlen (tempBuff)) > 0)
		{
			train -> curSpeed = 0;
			gtk_range_set_value (GTK_RANGE (train -> scaleSpeed), 0.0);
			sprintf (tempBuff, "Set speed: 0 for train %d", train -> trainNum);
			gtk_statusbar_push (GTK_STATUSBAR (trackCtrl.statusBar), 1, tempBuff);
		}
		else
		{
			gtk_statusbar_push (GTK_STATUSBAR (trackCtrl.statusBar), 1, notConnected);
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
 *  \result None.
 */
void checkPowerOn ()
{
	int i;
	gboolean state = (trackCtrl.powerState == POWER_ON ? TRUE : FALSE);
	for (i = 0; i < trackCtrl.trainCount; ++i)
	{
		if (trackCtrl.trainCtrl[i].labelNum != NULL)
			gtk_widget_set_sensitive (trackCtrl.trainCtrl[i].labelNum, state);
		if (trackCtrl.trainCtrl[i].buttonHalt != NULL)
			gtk_widget_set_sensitive (trackCtrl.trainCtrl[i].buttonHalt, state);
		if (trackCtrl.trainCtrl[i].scaleSpeed != NULL)
			gtk_widget_set_sensitive (trackCtrl.trainCtrl[i].scaleSpeed, state);
		if (trackCtrl.trainCtrl[i].checkDir != NULL)
			gtk_widget_set_sensitive (trackCtrl.trainCtrl[i].checkDir, state);
		if (trackCtrl.trainCtrl[i].buttonStop != NULL)
			gtk_widget_set_sensitive (trackCtrl.trainCtrl[i].buttonStop, state);
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
 *  \param data Which track to power.
 *  \result None.
 */
static void trackPower (GtkWidget *widget, gpointer data)
{
	int i, newState;
	char tempBuff[81];

	newState = (trackCtrl.powerState == POWER_ON ? POWER_OFF : POWER_ON);
	if (trainConnectSend (newState == POWER_ON ? "<1>" : "<0>", 3) == 3)
	{
		trackCtrl.powerState = newState;

		sprintf (tempBuff, "Power %s", trackCtrl.powerState == POWER_ON ? "OFF" : "ON");
		gtk_button_set_label (GTK_BUTTON(trackCtrl.buttonPower), tempBuff);
		sprintf (tempBuff, "Track power: %s", trackCtrl.powerState == POWER_ON ? "On" : "Off");
		gtk_statusbar_push (GTK_STATUSBAR (trackCtrl.statusBar), 1, tempBuff);
		for (i = 0; i < trackCtrl.trainCount; ++i)
		{
			trackCtrl.trainCtrl[i].curSpeed = 0;
			gtk_range_set_value (GTK_RANGE (trackCtrl.trainCtrl[i].scaleSpeed), 0.0);
		}
		checkPowerOn ();
	}
	else
	{
		gtk_statusbar_push (GTK_STATUSBAR (trackCtrl.statusBar), 1, notConnected);
	}
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
	trainCtrlDef *train = (trainCtrlDef *)data;
	value = gtk_range_get_value (GTK_RANGE (train -> scaleSpeed));

	if (train -> curSpeed != (int)value)
	{
		char tempBuff[81];
		int newValue = (int)value;

		sprintf (tempBuff, "<t %d %d %d %d>", train -> trainReg, train -> trainID, newValue, train -> reverse);
		if (trainConnectSend (tempBuff, strlen (tempBuff)) > 0)
		{
			train -> curSpeed = newValue;
			sprintf (tempBuff, "Set speed: %d for train %d", newValue, train -> trainNum);
			gtk_statusbar_push (GTK_STATUSBAR (trackCtrl.statusBar), 1, tempBuff);
		}
		else
		{
			gtk_statusbar_push (GTK_STATUSBAR (trackCtrl.statusBar), 1, notConnected);
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
	trainCtrlDef *train = (trainCtrlDef *)data;
	if (train -> curSpeed != 0)
	{
		sprintf (tempBuff, "<t %d %d %d %d>", train -> trainReg, train -> trainID, 0, train -> reverse);
		if (trainConnectSend (tempBuff, strlen (tempBuff)) > 0)
		{
			train -> curSpeed = 0;
			gtk_range_set_value (GTK_RANGE (train -> scaleSpeed), 0.0);
		}
	}
	train -> reverse = (train -> reverse == 0 ? 1 : 0);
	sprintf (tempBuff, "Set reverse: %s for train %d", (train -> reverse ? "On" : "Off"), train -> trainNum);
	gtk_statusbar_push (GTK_STATUSBAR (trackCtrl.statusBar), 1, tempBuff);
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  D R A W _ C A L L B A C K                                                                                         *
 *  =========================                                                                                         *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  \brief Draw the track.
 *  \param widget Button that was pressed.
 *  \param cr Cairo context.
 *  \param data Track to draw.
 *  \result FALSE.
 */
gboolean draw_callback (GtkWidget *widget, cairo_t *cr, gpointer data)
{
	int i, j, rows, cols;
	int xChange[8] = {	0,	20, 40, 20, 0,	0,	40, 40	};
	int yChange[8] = {	20, 0,	20, 40, 40, 0,	0,	40	};

	guint width, height;
	GtkStyleContext *context;
	GdkRGBA trackCol = { 0.5, 0.5, 0.5, 1.0 };
	GdkRGBA bufferCol = { 0.5, 0.0, 0.0, 1.0 };
	GdkRGBA inactiveCol = { 0.0, 0.0, 0.5, 1.0 };

	width = gtk_widget_get_allocated_width (widget);
	height = gtk_widget_get_allocated_height (widget);
	context = gtk_widget_get_style_context (widget);
	gtk_render_background (context, cr, 0, 0, width, height);

	rows = trackCtrl.trackLayout -> trackRows;
	cols = trackCtrl.trackLayout -> trackCols;

	for (i = 0; i < rows; ++i)
	{
		for (j = 0; j < cols; ++j)
		{
			int posn = (i * cols) + j, count = 0, points = 0, loop;

			for (loop = 0; loop < 8; ++loop)
			{
				if (trackCtrl.trackLayout -> trackCells[posn].layout & (1 << loop))
				{
					++count;
					if (trackCtrl.trackLayout -> trackCells[posn].point & (1 << loop))
					{
						if (++points > 1)
							gdk_cairo_set_source_rgba (cr, &inactiveCol);
					}
					else
					{
						gdk_cairo_set_source_rgba (cr, &trackCol);
					}
					cairo_move_to (cr, (j * 40) + 20, (i * 40) + 20);
					cairo_line_to (cr, (j * 40) + xChange[loop], (i * 40) + yChange[loop]);
					cairo_stroke (cr);
				}
			}
			if (count == 1)
			{
				gdk_cairo_set_source_rgba (cr, &bufferCol);
				cairo_arc (cr, (j * 40) + 20, (i * 40) + 20, 5, 0, 2 * G_PI);
				cairo_fill (cr);
				cairo_stroke (cr);

				gdk_cairo_set_source_rgba (cr, &trackCol);
				cairo_arc (cr, (j * 40) + 20, (i * 40) + 20, 5, 0, 2 * G_PI);
				cairo_stroke (cr);
			}
			if (points)
			{
				gdk_cairo_set_source_rgba (cr, &inactiveCol);
				cairo_arc (cr, (j * 40) + 20, (i * 40) + 20, 5, 0, 2 * G_PI);
				cairo_fill (cr);
				cairo_stroke (cr);

				gdk_cairo_set_source_rgba (cr, &trackCol);
				cairo_arc (cr, (j * 40) + 20, (i * 40) + 20, 5, 0, 2 * G_PI);
				cairo_stroke (cr);
			}
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
	trackCtrl.windowTrack = NULL;
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
	if (trackCtrl.windowTrack == NULL)
	{
		trackCtrl.windowTrack = gtk_window_new (GTK_WINDOW_TOPLEVEL);
		gtk_window_set_title (GTK_WINDOW (trackCtrl.windowTrack), "Track Control");
		gtk_window_set_default_size (GTK_WINDOW (trackCtrl.windowTrack), 480, 400);

		trackCtrl.drawingArea = gtk_drawing_area_new ();
		gtk_widget_set_size_request (trackCtrl.drawingArea, 480, 400);
		g_signal_connect (G_OBJECT (trackCtrl.drawingArea), "draw", G_CALLBACK (draw_callback), NULL);
		g_signal_connect (G_OBJECT (trackCtrl.windowTrack), "destroy", G_CALLBACK (closeTrack), NULL);

		gtk_container_add (GTK_CONTAINER (trackCtrl.windowTrack), trackCtrl.drawingArea);
		gtk_widget_show_all (trackCtrl.windowTrack);
	}
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
 *  \param user_data Which track to control.
 *  \result None.
 */
static void activate (GtkApplication *app, gpointer user_data)
{
	int i;
	char tempBuff[21];
	GtkWidget *grid;
	GtkWidget *vbox, *hbox;

	trackCtrl.windowCtrl = gtk_application_window_new (app);
	gtk_window_set_title (GTK_WINDOW (trackCtrl.windowCtrl), "Train Control");
	gtk_window_set_default_size (GTK_WINDOW (trackCtrl.windowCtrl), 260, 400);

	vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 5);
	gtk_container_add (GTK_CONTAINER (trackCtrl.windowCtrl), vbox);

	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 5);
	gtk_widget_set_halign (hbox, GTK_ALIGN_CENTER);
	gtk_container_add (GTK_CONTAINER (vbox), hbox);

	trackCtrl.buttonPower = gtk_button_new_with_label ("Power ON");
	g_signal_connect (trackCtrl.buttonPower, "clicked", G_CALLBACK (trackPower), &trackCtrl);
	gtk_widget_set_halign (trackCtrl.buttonPower, GTK_ALIGN_CENTER);
	gtk_container_add (GTK_CONTAINER (hbox), trackCtrl.buttonPower);

	trackCtrl.buttonTrack = gtk_button_new_with_label ("Track View");
	g_signal_connect (trackCtrl.buttonTrack, "clicked", G_CALLBACK (displayTrack), &trackCtrl);
	gtk_widget_set_halign (trackCtrl.buttonTrack, GTK_ALIGN_CENTER);
	gtk_container_add (GTK_CONTAINER (hbox), trackCtrl.buttonTrack);

	gtk_container_add (GTK_CONTAINER (vbox), gtk_separator_new (GTK_ORIENTATION_HORIZONTAL));

	grid = gtk_grid_new();
	gtk_widget_set_halign (grid, GTK_ALIGN_CENTER);
	gtk_container_add (GTK_CONTAINER (vbox), grid);

	for (i = 0; i < trackCtrl.trainCount; ++i)
	{
		sprintf (tempBuff, "%d", trackCtrl.trainCtrl[i].trainNum);
		trackCtrl.trainCtrl[i].labelNum = gtk_label_new (tempBuff);
		gtk_widget_set_halign (trackCtrl.trainCtrl[i].labelNum, GTK_ALIGN_CENTER);
		gtk_grid_attach(GTK_GRID(grid), trackCtrl.trainCtrl[i].labelNum, i, 0, 1, 1);

		trackCtrl.trainCtrl[i].buttonHalt = gtk_button_new_with_label ("Halt");
		g_signal_connect (trackCtrl.trainCtrl[i].buttonHalt, "clicked", G_CALLBACK (haltTrain), &trackCtrl.trainCtrl[i]);
		gtk_widget_set_halign (trackCtrl.trainCtrl[i].buttonHalt, GTK_ALIGN_CENTER);
		gtk_grid_attach(GTK_GRID(grid), trackCtrl.trainCtrl[i].buttonHalt, i, 1, 1, 1);

		trackCtrl.trainCtrl[i].scaleSpeed = gtk_scale_new_with_range (GTK_ORIENTATION_VERTICAL, 0, 126, 1);
		g_signal_connect (trackCtrl.trainCtrl[i].scaleSpeed, "value-changed", G_CALLBACK (moveTrain), &trackCtrl.trainCtrl[i]);
		gtk_widget_set_vexpand (trackCtrl.trainCtrl[i].scaleSpeed, 1);
		gtk_scale_set_value_pos (GTK_SCALE(trackCtrl.trainCtrl[i].scaleSpeed), 0);
		gtk_widget_set_halign (trackCtrl.trainCtrl[i].scaleSpeed, GTK_ALIGN_CENTER);
		gtk_grid_attach(GTK_GRID(grid), trackCtrl.trainCtrl[i].scaleSpeed, i, 2, 1, 1);

		trackCtrl.trainCtrl[i].checkDir = gtk_check_button_new_with_label ("Reverse");
		g_signal_connect (trackCtrl.trainCtrl[i].checkDir, "clicked", G_CALLBACK (reverseTrain), &trackCtrl.trainCtrl[i]);
		gtk_widget_set_halign (trackCtrl.trainCtrl[i].checkDir, GTK_ALIGN_CENTER);
		gtk_grid_attach(GTK_GRID(grid), trackCtrl.trainCtrl[i].checkDir, i, 3, 1, 1);

		trackCtrl.trainCtrl[i].buttonStop = gtk_button_new_with_label ("STOP");
		g_signal_connect (trackCtrl.trainCtrl[i].buttonStop, "clicked", G_CALLBACK (stopTrain), &trackCtrl.trainCtrl[i]);
		gtk_widget_set_halign (trackCtrl.trainCtrl[i].buttonStop, GTK_ALIGN_CENTER);
		gtk_grid_attach(GTK_GRID(grid), trackCtrl.trainCtrl[i].buttonStop, i, 4, 1, 1);

	}
	checkPowerOn ();

	gtk_container_add (GTK_CONTAINER (vbox), gtk_separator_new (GTK_ORIENTATION_HORIZONTAL));
	trackCtrl.statusBar = gtk_statusbar_new();
	gtk_container_add (GTK_CONTAINER (vbox), trackCtrl.statusBar);
	gtk_statusbar_push (GTK_STATUSBAR (trackCtrl.statusBar), 1, "Train control started");

	gtk_widget_show_all (trackCtrl.windowCtrl);
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
	int i, status = 1;
	GtkApplication *app;

	if (parseTrackXML ("track.xml"))
	{
		if (startConnectThread ())
		{
			app = gtk_application_new ("Train.Control", G_APPLICATION_FLAGS_NONE);
			g_signal_connect (app, "activate", G_CALLBACK (activate), NULL);
			status = g_application_run (G_APPLICATION (app), argc, argv);
			g_object_unref (app);
			stopConnectThread ();
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
	return status;
}

