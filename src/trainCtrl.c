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

#define TRAIN_NUM	2
#define POWER_SRT	-1
#define POWER_OFF	0
#define POWER_ON	1

int tempTrains[TRAIN_NUM][2] =
{
	{ 3, 47451 },
	{ 6, 68035 }
};

int tempTrack[10][12] = 
{
	{	4,	5,	133,5,	5,	5,	5,	21,	5,	129,0,	0 },
	{	4,	5,	133,37,	5,	5,	69,	21,	5,	129,160,0 },
	{	4,	5,	5,	37,	5,	5,	65,	0,	0,	0,	40,	40 },
	{	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	10,	26 },
	{	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	74,	10 },
	{	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	10,	10 },
	{	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	138,10 },
	{	4,	5,	5,	5,	129,0,	0,	0,	0,	0,	26,	42 },
	{	4,	5,	21,	5,	5,	161,0,	0,	0,	80,	10,	10 },
	{	4,	69,	5,	5,	5,	5,	37,	5,	65,	0,	2,	2 }
};

typedef struct _trainCtrl
{
	int trainID;
	int trainNum;
	int curSpeed;
	int reverse;
	
	GtkWidget *buttonStop;
	GtkWidget *buttonHalt;
	GtkWidget *scaleSpeed;
	GtkWidget *checkDir;
}
trainCtrlDef;

typedef struct _trackCtrl
{
	int powerState;
	GtkWidget *windowCtrl;
	GtkWidget *windowTrack;
	GtkWidget *buttonPower;
	GtkWidget *buttonTrack;
	GtkWidget *statusBar;
	GtkWidget *drawingArea;
	trainCtrlDef trainCtrl[TRAIN_NUM];
}
trackCtrlDef;

trackCtrlDef trackCtrl;

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
	char tempBuff[81];

	trainCtrlDef *train = (trainCtrlDef *)data;
	train -> curSpeed = 0;
	gtk_range_set_value (GTK_RANGE (train -> scaleSpeed), 0.0);
	sprintf (tempBuff, "Set speed: STOP for train %d", train -> trainNum);
	gtk_statusbar_push (GTK_STATUSBAR (trackCtrl.statusBar), 1, tempBuff);
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
	char tempBuff[81];

	trainCtrlDef *train = (trainCtrlDef *)data;
	train -> curSpeed = 0;
	gtk_range_set_value (GTK_RANGE (train -> scaleSpeed), 0.0);
	sprintf (tempBuff, "Set speed: 0 for train %d", train -> trainNum);
	gtk_statusbar_push (GTK_STATUSBAR (trackCtrl.statusBar), 1, tempBuff);
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
	char tempBuff[81];

	trackCtrl.powerState = (trackCtrl.powerState == POWER_ON ? POWER_OFF : POWER_ON);
	sprintf (tempBuff, "Power %s", trackCtrl.powerState == POWER_ON ? "OFF" : "ON");
	gtk_button_set_label (GTK_BUTTON(trackCtrl.buttonPower), tempBuff); 
	sprintf (tempBuff, "Track power: %s", trackCtrl.powerState == POWER_ON ? "On" : "Off");
	gtk_statusbar_push (GTK_STATUSBAR (trackCtrl.statusBar), 1, tempBuff);
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
	char tempBuff[81];
	double value = 0.0;

	trainCtrlDef *train = (trainCtrlDef *)data;
	value = gtk_range_get_value (GTK_RANGE (train -> scaleSpeed));
	train -> curSpeed = (int)value;
	sprintf (tempBuff, "Set speed: %0.0f for train %d", value, train -> trainNum);
	gtk_statusbar_push (GTK_STATUSBAR (trackCtrl.statusBar), 1, tempBuff);
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
	if (train -> curSpeed > 0)
	{
		train -> curSpeed = 0;
		gtk_range_set_value (GTK_RANGE (train -> scaleSpeed), 0.0);
	}
	train -> reverse = (train -> reverse == 0 ? 1 : 0);
	sprintf (tempBuff, "Set reverse: %s for train %d", train -> reverse ? "On" : "Off", train -> trainNum);
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
	int i, j;
	guint width, height;
	GdkRGBA color;
	GtkStyleContext *context;

	context = gtk_widget_get_style_context (widget);

	width = gtk_widget_get_allocated_width (widget);
	height = gtk_widget_get_allocated_height (widget);
	gtk_render_background (context, cr, 0, 0, width, height);

	for (i = 0; i < 10; ++i)
	{
		for (j = 0; j < 12; ++j)
		{
			if (tempTrack[i][j] & 0x01)
			{ 
				cairo_move_to (cr, (j * 40) + 20, (i * 40) + 20);
				cairo_line_to (cr, (j * 40), (i * 40) + 20);
			}
			if (tempTrack[i][j] & 0x02)
			{ 
				cairo_move_to (cr, (j * 40) + 20, (i * 40) + 20);
				cairo_line_to (cr, (j * 40) + 20, (i * 40));
			}
			if (tempTrack[i][j] & 0x04)
			{ 
				cairo_move_to (cr, (j * 40) + 20, (i * 40) + 20);
				cairo_line_to (cr, (j * 40) + 40, (i * 40) + 20);
			}
			if (tempTrack[i][j] & 0x08)
			{ 
				cairo_move_to (cr, (j * 40) + 20, (i * 40) + 20);
				cairo_line_to (cr, (j * 40) + 20, (i * 40) + 40);
			}
			if (tempTrack[i][j] & 0x10)
			{ 
				cairo_move_to (cr, (j * 40) + 20, (i * 40) + 20);
				cairo_line_to (cr, (j * 40), (i * 40) + 40);
			}
			if (tempTrack[i][j] & 0x20)
			{ 
				cairo_move_to (cr, (j * 40) + 20, (i * 40) + 20);
				cairo_line_to (cr, (j * 40), (i * 40));
			}
			if (tempTrack[i][j] & 0x40)
			{ 
				cairo_move_to (cr, (j * 40) + 20, (i * 40) + 20);
				cairo_line_to (cr, (j * 40) + 40, (i * 40));
			}
			if (tempTrack[i][j] & 0x80)
			{ 
				cairo_move_to (cr, (j * 40) + 20, (i * 40) + 20);
				cairo_line_to (cr, (j * 40) + 40, (i * 40) + 40);
			}
		}
	}

	cairo_stroke (cr);

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
	GtkWidget *vbox, *hbox, *labels[TRAIN_NUM];

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

	for (i = 0; i < TRAIN_NUM; ++i)
	{
		sprintf (tempBuff, "%d", tempTrains[i][1]);
		labels[i] = gtk_label_new (tempBuff);
		gtk_widget_set_halign (labels[i], GTK_ALIGN_CENTER);
		gtk_grid_attach(GTK_GRID(grid), labels[i], i, 0, 1, 1);

		trackCtrl.trainCtrl[i].buttonHalt = gtk_button_new_with_label ("Halt");
		g_signal_connect (trackCtrl.trainCtrl[i].buttonHalt, "clicked", G_CALLBACK (haltTrain), &trackCtrl.trainCtrl[i]);
		gtk_widget_set_halign (trackCtrl.trainCtrl[i].buttonHalt, GTK_ALIGN_CENTER);
		gtk_grid_attach(GTK_GRID(grid), trackCtrl.trainCtrl[i].buttonHalt, i, 1, 1, 1);

		trackCtrl.trainCtrl[i].scaleSpeed = gtk_scale_new_with_range (GTK_ORIENTATION_VERTICAL, 0, 128, 1);
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
	int i, status;
	GtkApplication *app;

	trackCtrl.powerState = POWER_SRT;
	for (i = 0; i < TRAIN_NUM; ++i)
	{	
		trackCtrl.trainCtrl[i].trainID = tempTrains[i][0];
		trackCtrl.trainCtrl[i].trainNum = tempTrains[i][1];
		trackCtrl.trainCtrl[i].curSpeed = 0;
	}

	app = gtk_application_new ("Train.Control", G_APPLICATION_FLAGS_NONE);
	g_signal_connect (app, "activate", G_CALLBACK (activate), NULL);
	status = g_application_run (G_APPLICATION (app), argc, argv);
	g_object_unref (app);

	return status;
}

