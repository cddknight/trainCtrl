/**********************************************************************************************************************
 *                                                                                                                    *
 *  P O I N T  T E S T . C                                                                                            *
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
 *  \brief Move a servo to find end points for the point config.
 */
#include <gtk/gtk.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>
#include "config.h"
#ifdef HAVE_WIRINGPI_H
#include <wiringPi.h>
#include "pca9685.h"
#endif

#define PIN_BASE 300
#define MAX_PWM 4096
#define HERTZ 50

int servoFD = -1;
int channel = 15;

typedef struct
{
	GtkWidget *windowCtrl;
	GtkWidget *labelServo;
	GtkWidget *scaleServo;
	double currentValue;
}
pointTestDef;

static void quitCallback (GSimpleAction *action, GVariant *parameter, gpointer data);

static GActionEntry appEntries[] =
{
	{ "quit", quitCallback, NULL, NULL, NULL }
};

/*--------------------------------------------------------------------------------------------------------------------*
 * If we cannot find a stock clock icon then use this built in one.                                                   *
 *--------------------------------------------------------------------------------------------------------------------*/
static GdkPixbuf *defaultIcon;
#include "train.xpm"

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
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  C L O C K  T I C K  C A L L B A C K                                                                               *
 *  ===================================                                                                               *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  \brief Called by the timer to check the current value.
 *  \param data Pointer to the point config.
 *  \result True keep the timer running.
 */
gboolean clockTickCallback (gpointer data)
{
	pointTestDef *pointTest = (pointTestDef *)data;
	if (pointTest != NULL)
	{
		double curVal = gtk_range_get_value (GTK_RANGE (pointTest -> scaleServo));
		if (curVal != pointTest -> currentValue)
		{
			pointTest -> currentValue = curVal;
#ifdef HAVE_WIRINGPI_H
			if (servoFD != -1)
			{
				pwmWrite (PIN_BASE + channel, curVal);
				printf ("Updated to: %f\n", curVal);
			}
#endif
		}
	}
	return TRUE;
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
	int j;
	char buff[41];
	GtkWidget *vbox;
	GMenu *menu;
	pointTestDef *pointTest;

	g_action_map_add_action_entries (G_ACTION_MAP (app), appEntries, G_N_ELEMENTS (appEntries), app);
	menu = g_menu_new ();
	g_menu_append (menu, "Quit", "app.quit");
	gtk_application_set_app_menu (GTK_APPLICATION (app), G_MENU_MODEL (menu));
	g_object_unref (menu);

	pointTest = (pointTestDef *)malloc (sizeof (pointTestDef));
	if (pointTest != NULL)
	{
		pointTest -> windowCtrl = gtk_application_window_new (app);
		g_signal_connect (pointTest -> windowCtrl, "destroy", G_CALLBACK (windowDestroy), pointTest);
		gtk_window_set_title (GTK_WINDOW (pointTest -> windowCtrl), "Point Test");
		if (!gtk_window_set_icon_from_file (GTK_WINDOW (pointTest -> windowCtrl),
				"/usr/share/pixmaps/traincontrol.svg", NULL))
		{
			gtk_window_set_icon_name (GTK_WINDOW (pointTest -> windowCtrl), "preferences-desktop");
		}
		gtk_window_set_default_size (GTK_WINDOW (pointTest -> windowCtrl), 150, 300);

		vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 5);
		gtk_container_set_border_width (GTK_CONTAINER(vbox), 5);
		gtk_container_add (GTK_CONTAINER (pointTest -> windowCtrl), vbox);

		sprintf (buff, "Servo[%d]", channel);
		pointTest -> labelServo = gtk_label_new (buff);
		gtk_container_add (GTK_CONTAINER (vbox), pointTest -> labelServo);

		pointTest -> scaleServo = gtk_scale_new_with_range (GTK_ORIENTATION_VERTICAL, 0, 500, 1);
		gtk_widget_set_vexpand (pointTest -> scaleServo, 1);
		gtk_scale_set_value_pos (GTK_SCALE(pointTest -> scaleServo), GTK_POS_TOP);
		gtk_scale_set_digits (GTK_SCALE(pointTest -> scaleServo), 0);
		gtk_scale_set_has_origin (GTK_SCALE(pointTest -> scaleServo), TRUE);
		for (j = 0; j <= 500; j += 125)
		{
			gtk_scale_add_mark (GTK_SCALE(pointTest -> scaleServo), j, GTK_POS_LEFT, NULL);
		}
		gtk_widget_set_halign (pointTest -> scaleServo, GTK_ALIGN_CENTER);
		gtk_range_set_value (GTK_RANGE (pointTest -> scaleServo), pointTest -> currentValue = 250);
		gtk_container_add (GTK_CONTAINER (vbox), pointTest -> scaleServo);

		gtk_widget_show_all (pointTest -> windowCtrl);
		g_timeout_add (100, clockTickCallback, pointTest);
	}
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

#ifdef HAVE_WIRINGPI_H
	wiringPiSetup();

	if ((servoFD = pca9685Setup(PIN_BASE, 0x40, HERTZ)) < 0)
	{
		return 0;
	}
	pca9685PWMReset (servoFD);
	pwmWrite (PIN_BASE + channel, 250);
#endif

	app = gtk_application_new ("Point.Test", G_APPLICATION_HANDLES_OPEN); // G_APPLICATION_FLAGS_NONE);
	g_signal_connect (app, "activate", G_CALLBACK (activate), NULL);
	g_signal_connect (app, "shutdown", G_CALLBACK (shutdown), NULL);

	defaultIcon = gdk_pixbuf_new_from_xpm_data ((const char **) &train_xpm);
	gtk_window_set_default_icon_name ("train_xpm");

	status = g_application_run (G_APPLICATION (app), argc, argv);
	g_object_unref (app);

#ifdef HAVE_WIRINGPI_H
	close (servoFD);
#endif
	return status;
}

