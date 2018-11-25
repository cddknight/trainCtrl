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
	GtkWidget *buttonPower;
	trainCtrlDef trainCtrl[TRAIN_NUM];
}
trackCtrlDef;

trackCtrlDef trackCtrl;

static void stopTrain (GtkWidget *widget, gpointer data)
{
	trainCtrlDef *train = (trainCtrlDef *)data;
	train -> curSpeed = 0;
	gtk_range_set_value (GTK_RANGE (train -> scaleSpeed), 0.0);
	g_print ("Set speed value: %d for train %d\n", -1, train -> trainNum);
}

static void haltTrain (GtkWidget *widget, gpointer data)
{
	trainCtrlDef *train = (trainCtrlDef *)data;
	train -> curSpeed = 0;
	gtk_range_set_value (GTK_RANGE (train -> scaleSpeed), 0.0);
	g_print ("Set speed value: %d for train %d\n", 0, train -> trainNum);
}

static void trackPower (GtkWidget *widget, gpointer data)
{
	trackCtrl.powerState = (trackCtrl.powerState == POWER_SRT || trackCtrl.powerState == POWER_ON) ? POWER_OFF : POWER_ON;
	g_print ("Track power: %s\n", trackCtrl.powerState == POWER_ON ? "On" : "Off");
}

static void moveTrain (GtkWidget *widget, gpointer data)
{
	double value = 0.0;
	trainCtrlDef *train = (trainCtrlDef *)data;
	value = gtk_range_get_value (GTK_RANGE (train -> scaleSpeed));
	g_print ("Set speed value: %f for train %d\n", value, train -> trainNum);
}

static void reverseTrain (GtkWidget *widget, gpointer data)
{
	trainCtrlDef *train = (trainCtrlDef *)data;
	train -> reverse = (train -> reverse == 0 ? 1 : 0);
	train -> curSpeed = 0;
	gtk_range_set_value (GTK_RANGE (train -> scaleSpeed), 0.0);
	g_print ("Set reverse: %s for train %d\n", train -> reverse ? "Yes" : "No", train -> trainNum);
}

static void activate (GtkApplication *app, gpointer user_data)
{
	int i;
	GtkWidget *grid;
	GtkWidget *vbox;

	trackCtrl.windowCtrl = gtk_application_window_new (app);
	gtk_window_set_title (GTK_WINDOW (trackCtrl.windowCtrl), "Train Control");
	gtk_window_set_default_size (GTK_WINDOW (trackCtrl.windowCtrl), 200, 400);

	vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 2);	
	gtk_container_add (GTK_CONTAINER (trackCtrl.windowCtrl), vbox);

	trackCtrl.buttonPower = gtk_button_new_with_label ("Power");
	g_signal_connect (trackCtrl.buttonPower, "clicked", G_CALLBACK (trackPower), &trackCtrl);
	gtk_widget_set_halign (trackCtrl.buttonPower, GTK_ALIGN_CENTER);
	gtk_container_add (GTK_CONTAINER (vbox), trackCtrl.buttonPower);

	grid = gtk_grid_new();
	gtk_widget_set_halign (grid, GTK_ALIGN_CENTER);
	gtk_container_add (GTK_CONTAINER (vbox), grid);

	for (i = 0; i < TRAIN_NUM; ++i)
	{
		trackCtrl.trainCtrl[i].buttonStop = gtk_button_new_with_label ("Stop!");
		g_signal_connect (trackCtrl.trainCtrl[i].buttonStop, "clicked", G_CALLBACK (stopTrain), &trackCtrl.trainCtrl[i]);
		gtk_widget_set_halign (trackCtrl.trainCtrl[i].buttonStop, GTK_ALIGN_CENTER);
		gtk_grid_attach(GTK_GRID(grid), trackCtrl.trainCtrl[i].buttonStop, i, 2, 1, 1);

		trackCtrl.trainCtrl[i].buttonHalt = gtk_button_new_with_label ("Halt");
		g_signal_connect (trackCtrl.trainCtrl[i].buttonHalt, "clicked", G_CALLBACK (haltTrain), &trackCtrl.trainCtrl[i]);
		gtk_widget_set_halign (trackCtrl.trainCtrl[i].buttonHalt, GTK_ALIGN_CENTER);
		gtk_grid_attach(GTK_GRID(grid), trackCtrl.trainCtrl[i].buttonHalt, i, 3, 1, 1);

		trackCtrl.trainCtrl[i].scaleSpeed = gtk_scale_new_with_range (GTK_ORIENTATION_VERTICAL, 0, 128, 1);
		g_signal_connect (trackCtrl.trainCtrl[i].scaleSpeed, "value-changed", G_CALLBACK (moveTrain), &trackCtrl.trainCtrl[i]);
		gtk_widget_set_vexpand (trackCtrl.trainCtrl[i].scaleSpeed, 1);
		gtk_scale_set_value_pos (GTK_SCALE(trackCtrl.trainCtrl[i].scaleSpeed), 0);
		gtk_widget_set_halign (trackCtrl.trainCtrl[i].scaleSpeed, GTK_ALIGN_CENTER);
		gtk_grid_attach(GTK_GRID(grid), trackCtrl.trainCtrl[i].scaleSpeed, i, 4, 1, 1);

		trackCtrl.trainCtrl[i].checkDir = gtk_check_button_new_with_label ("Reverse");
		g_signal_connect (trackCtrl.trainCtrl[i].checkDir, "clicked", G_CALLBACK (reverseTrain), &trackCtrl.trainCtrl[i]);
		gtk_widget_set_halign (trackCtrl.trainCtrl[i].checkDir, GTK_ALIGN_CENTER);
		gtk_grid_attach(GTK_GRID(grid), trackCtrl.trainCtrl[i].checkDir, i, 5, 1, 1);
	}
	gtk_widget_show_all (trackCtrl.windowCtrl);
}

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

	app = gtk_application_new ("org.gnome.example", G_APPLICATION_FLAGS_NONE);
	g_signal_connect (app, "activate", G_CALLBACK (activate), NULL);
	status = g_application_run (G_APPLICATION (app), argc, argv);
	g_object_unref (app);

	return status;
}

