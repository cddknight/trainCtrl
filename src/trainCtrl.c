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

#define CELL_SIZE	40
#define CELL_HALF	20

trackCtrlDef trackCtrl;

static char *notConnected = "Train controller not connected";

static void aboutCallback (GSimpleAction *action, GVariant *parameter, gpointer data);
static void quitCallback (GSimpleAction *action, GVariant *parameter, gpointer data);

static GActionEntry app_entries[] =
{
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
			train -> curSpeed = train -> remoteCurSpeed = 0;
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
			train -> curSpeed = train -> remoteCurSpeed = 0;
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
		trackCtrl.powerState = trackCtrl.remotePowerState = newState;

		sprintf (tempBuff, "Power %s", trackCtrl.powerState == POWER_ON ? "OFF" : "ON");
		gtk_button_set_label (GTK_BUTTON(trackCtrl.buttonPower), tempBuff);
		sprintf (tempBuff, "Track power: %s", trackCtrl.powerState == POWER_ON ? "On" : "Off");
		gtk_statusbar_push (GTK_STATUSBAR (trackCtrl.statusBar), 1, tempBuff);
		for (i = 0; i < trackCtrl.trainCount; ++i)
		{
			trainCtrlDef *train = &trackCtrl.trainCtrl[i];
			if (train -> curSpeed > 0 && trackCtrl.powerState == POWER_OFF)
			{
				trackCtrl.trainCtrl[i].curSpeed = trackCtrl.trainCtrl[i].remoteCurSpeed = 0;
				gtk_range_set_value (GTK_RANGE (trackCtrl.trainCtrl[i].scaleSpeed), 0.0);
				sprintf (tempBuff, "<t %d %d %d %d>", train -> trainReg, train -> trainID, 0, train -> reverse);
				trainConnectSend (tempBuff, strlen (tempBuff));
			}
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
			train -> curSpeed = train -> remoteCurSpeed = newValue;
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
	int newState;
	trainCtrlDef *train = (trainCtrlDef *)data;

	newState = (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (train -> checkDir)) == TRUE ? 1 : 0);
	if (newState != train -> reverse)
	{
		train -> reverse = train -> remoteReverse = newState;
		sprintf (tempBuff, "<t %d %d %d %d>", train -> trainReg, train -> trainID, 0, train -> reverse);
		if (trainConnectSend (tempBuff, strlen (tempBuff)) > 0)
		{
			train -> curSpeed = train -> remoteCurSpeed = 0;
			gtk_range_set_value (GTK_RANGE (train -> scaleSpeed), 0.0);
		}
		sprintf (tempBuff, "Set reverse: %s for train %d", (train -> reverse ? "On" : "Off"), train -> trainNum);
		gtk_statusbar_push (GTK_STATUSBAR (trackCtrl.statusBar), 1, tempBuff);
	}
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
	static const GdkRGBA trackCol = { 0.7, 0.7, 0.0, 1.0 };
	static const GdkRGBA pointCol = { 0.0, 0.0, 0.5, 1.0 };
	static const GdkRGBA bufferCol = { 0.5, 0.0, 0.0, 1.0 };
	static const GdkRGBA inactiveCol = { 0.2, 0.0, 0.0, 1.0 };
	static const GdkRGBA circleCol = { 0.7, 0.7, 0.7, 1.0 };

	static const int xChange[8] = {	0,	CELL_HALF, CELL_SIZE, CELL_HALF, 0,	0,	CELL_SIZE, CELL_SIZE	};
	static const int yChange[8] = {	CELL_HALF, 0,	CELL_HALF, CELL_SIZE, CELL_SIZE, 0,	0,	CELL_SIZE	};

	int i, j;
	int rows = trackCtrl.trackLayout -> trackRows;
	int cols = trackCtrl.trackLayout -> trackCols;
	guint width = gtk_widget_get_allocated_width (widget);
	guint height = gtk_widget_get_allocated_height (widget);
	GtkStyleContext *context = gtk_widget_get_style_context (widget);

	gtk_render_background (context, cr, 0, 0, width, height);

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
					gdk_cairo_set_source_rgba (cr, &trackCol);
					if (trackCtrl.trackLayout -> trackCells[posn].point & (1 << loop))
					{
						++points;
						if (!(trackCtrl.trackLayout -> trackCells[posn].pointState & (1 << loop)))
						{
							gdk_cairo_set_source_rgba (cr, &inactiveCol);
						}
					}
					cairo_move_to (cr, (j * CELL_SIZE) + CELL_HALF, (i * CELL_SIZE) + CELL_HALF);
					cairo_line_to (cr, (j * CELL_SIZE) + xChange[loop], (i * CELL_SIZE) + yChange[loop]);
					cairo_stroke (cr);
				}
			}
			if (count == 1)
			{
				gdk_cairo_set_source_rgba (cr, &bufferCol);
				cairo_arc (cr, (j * CELL_SIZE) + CELL_HALF, (i * CELL_SIZE) + CELL_HALF, 4, 0, 2 * G_PI);
				cairo_fill (cr);
				cairo_stroke (cr);

				gdk_cairo_set_source_rgba (cr, &circleCol);
				cairo_arc (cr, (j * CELL_SIZE) + CELL_HALF, (i * CELL_SIZE) + CELL_HALF, 4, 0, 2 * G_PI);
				cairo_stroke (cr);
			}
			if (points)
			{
				gdk_cairo_set_source_rgba (cr, &pointCol);
				cairo_arc (cr, (j * CELL_SIZE) + CELL_HALF, (i * CELL_SIZE) + CELL_HALF, 4, 0, 2 * G_PI);
				cairo_fill (cr);
				cairo_stroke (cr);

				gdk_cairo_set_source_rgba (cr, &circleCol);
				cairo_arc (cr, (j * CELL_SIZE) + CELL_HALF, (i * CELL_SIZE) + CELL_HALF, 4, 0, 2 * G_PI);
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
 *  \result TRUE if we used it.
 */
gboolean windowClickCallback (GtkWidget * widget, GdkEventButton * event)
{
	if (event->type == GDK_BUTTON_PRESS)
	{
		switch (event->button)
		{
		case GDK_BUTTON_PRIMARY:	/* left button */
			{
				int rows = trackCtrl.trackLayout -> trackRows;
				int cols = trackCtrl.trackLayout -> trackCols;
				int posn = (((int)event -> y / CELL_SIZE) * cols) + ((int)event -> x / CELL_SIZE);

				if (trackCtrl.trackLayout -> trackCells[posn].point)
				{
					unsigned short newState = trackCtrl.trackLayout -> trackCells[posn].point;
					newState &= ~(trackCtrl.trackLayout -> trackCells[posn].pointState);
					trackCtrl.trackLayout -> trackCells[posn].pointState = newState;
					gtk_widget_queue_draw (trackCtrl.drawingArea);
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
    	GtkWidget *eventBox;
		int width = trackCtrl.trackLayout -> trackCols * CELL_SIZE;
		int height = trackCtrl.trackLayout -> trackRows * CELL_SIZE;

		trackCtrl.windowTrack = gtk_window_new (GTK_WINDOW_TOPLEVEL);
		gtk_window_set_title (GTK_WINDOW (trackCtrl.windowTrack), "Track Control");
		gtk_window_set_default_size (GTK_WINDOW (trackCtrl.windowTrack), width, height);

		trackCtrl.drawingArea = gtk_drawing_area_new ();
		gtk_widget_set_size_request (trackCtrl.drawingArea, width, height);
	    eventBox = gtk_event_box_new ();
	    gtk_container_add (GTK_CONTAINER (eventBox), trackCtrl.drawingArea);

		g_signal_connect (G_OBJECT (trackCtrl.drawingArea), "draw", G_CALLBACK (draw_callback), NULL);
		g_signal_connect (G_OBJECT (trackCtrl.windowTrack), "destroy", G_CALLBACK (closeTrack), NULL);
		g_signal_connect (G_OBJECT (eventBox), "button_press_event", G_CALLBACK (windowClickCallback), NULL);

		gtk_container_add (GTK_CONTAINER (trackCtrl.windowTrack), eventBox);
		gtk_widget_show_all (trackCtrl.windowTrack);
	}
}

static void programTrain (GtkWidget *widget, gpointer data)
{
	static char *lables[4] = { "Train ID ", "CV Number ", "Byte Value ", "Bit Value " };
	static int entrySizes[4] = { 5, 4, 3, 1 };

	int reRun, i;
	GtkWidget *contentArea;
	GtkWidget *entry[4];
	GtkWidget *label;
	GtkWidget *grid;
	GtkWidget *vbox;

	if (trackCtrl.dialogProgram == NULL)
	{
		trackCtrl.dialogProgram = gtk_dialog_new_with_buttons ("Program Train", 
				GTK_WINDOW (trackCtrl.windowCtrl),
				GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
				"Close", GTK_RESPONSE_CLOSE, NULL);

		contentArea = gtk_dialog_get_content_area (GTK_DIALOG (trackCtrl.dialogProgram));

		vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 3);
		gtk_widget_set_halign (vbox, GTK_ALIGN_FILL);
		gtk_box_pack_start (GTK_BOX (contentArea), vbox, TRUE, TRUE, 0);

		grid = gtk_grid_new();
		gtk_widget_set_halign (grid, GTK_ALIGN_FILL);
		gtk_box_pack_start (GTK_BOX (vbox), grid, TRUE, TRUE, 0);

		for (i = 0; i < 4; ++i)
		{
			label = gtk_label_new (lables[i]);
			gtk_widget_set_halign (label, GTK_ALIGN_END);
			gtk_grid_attach (GTK_GRID(grid), label, 0, i, 1, 1);

			entry[i] = gtk_entry_new ();
			gtk_entry_set_max_width_chars (GTK_ENTRY (entry[i]), entrySizes[i]);
			gtk_widget_set_halign (entry[i], GTK_ALIGN_FILL);
			gtk_grid_attach (GTK_GRID(grid), entry[i], 1, i, 1, 1);
		}

		gtk_widget_show_all (trackCtrl.dialogProgram);
		gtk_dialog_run (GTK_DIALOG (trackCtrl.dialogProgram));
		gtk_widget_destroy (trackCtrl.dialogProgram);
		trackCtrl.dialogProgram = NULL;
	}
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  A B O U T  C A L L B A C K                                                                                        *
 *  ==========================                                                                                        *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  \brief Called when about selected on the menu.
 *  \param action Not used.
 *  \param parameter Not used.
 *  \param user_data Not used.
 *  \result None.
 */
static void aboutCallback (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	const gchar *authors[] =
	{
		"Chris Knight",
		NULL
	};

	gtk_show_about_dialog (GTK_WINDOW (trackCtrl.windowCtrl),
			"program-name", "Train Control",
			"version", g_strdup_printf ("%s,\nRunning against GTK+ %d.%d.%d",
			 "0.0",
				 gtk_get_major_version (),
				 gtk_get_minor_version (),
				 gtk_get_micro_version ()),
			"copyright", "(C) 2018 TheKnight",
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
	GMenu *menu;
	GtkWidget *grid;
	GtkWidget *vbox, *hbox;

	g_action_map_add_action_entries (G_ACTION_MAP (app), app_entries, G_N_ELEMENTS (app_entries), app);
	menu = g_menu_new ();
	g_menu_append (menu, "About", "app.about");
	g_menu_append (menu, "Quit", "app.quit");
	gtk_application_set_app_menu (GTK_APPLICATION (app), G_MENU_MODEL (menu));
	g_object_unref (menu);

	trackCtrl.windowCtrl = gtk_application_window_new (app);
	gtk_window_set_title (GTK_WINDOW (trackCtrl.windowCtrl), "Train Control");
	gtk_window_set_icon_name (GTK_WINDOW (trackCtrl.windowCtrl), "document-open");
	gtk_window_set_default_size (GTK_WINDOW (trackCtrl.windowCtrl), 300, 400);

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

	trackCtrl.buttonProgram = gtk_button_new_with_label ("Program");
	g_signal_connect (trackCtrl.buttonProgram, "clicked", G_CALLBACK (programTrain), &trackCtrl);
	gtk_widget_set_halign (trackCtrl.buttonProgram, GTK_ALIGN_CENTER);
	gtk_container_add (GTK_CONTAINER (hbox), trackCtrl.buttonProgram);

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
		gtk_scale_set_value_pos (GTK_SCALE(trackCtrl.trainCtrl[i].scaleSpeed), GTK_POS_TOP);
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

	if (trackCtrl.powerState != trackCtrl.remotePowerState)
	{
		char tempBuff[81];
		trackCtrl.powerState = trackCtrl.remotePowerState;
		sprintf (tempBuff, "Power %s", trackCtrl.powerState == POWER_ON ? "OFF" : "ON");
		gtk_button_set_label (GTK_BUTTON(trackCtrl.buttonPower), tempBuff);
		checkPowerOn ();
	}
	for (i = 0; i < trackCtrl.trainCount; ++i)
	{
		trainCtrlDef *train = &trackCtrl.trainCtrl[i];
		if (train -> curSpeed != train -> remoteCurSpeed)
		{
			train -> curSpeed = train -> remoteCurSpeed;
			gtk_range_set_value (GTK_RANGE (train -> scaleSpeed), (double)train -> curSpeed);
		}
		if (train -> reverse != train -> remoteReverse)
		{
			train -> reverse = train -> remoteReverse;
			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (train -> checkDir), train -> reverse);
		}
	}
	return TRUE;
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
			g_timeout_add (100, clockTickCallback, NULL);
			app = gtk_application_new ("Train.Control", G_APPLICATION_FLAGS_NONE);
			g_signal_connect (app, "activate", G_CALLBACK (activate), NULL);

			defaultIcon = gdk_pixbuf_new_from_xpm_data ((const char **) &train_xpm);
			gtk_window_set_default_icon_name ("train_xpm");

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

