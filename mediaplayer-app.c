#include <gtk/gtk.h>
#include "mediaplayer-app.h"
#include "mediaplayer-window.h"

struct _MediaplayerApp {
    GtkApplication parent;
};

G_DEFINE_TYPE(MediaplayerApp, mediaplayer_app, GTK_TYPE_APPLICATION);


/* instance init: initialize object functions */
static void mediaplayer_app_init (MediaplayerApp *app)
{
}

/* activate: invoked on launching program */
static void
mediaplayer_app_activate (GApplication *app)
{
    MediaplayerWindow *win;
    win = mediaplayer_window_new (MEDIAPLAYER_APP (app));
    gtk_window_present (GTK_WINDOW (win));
}

/* class init: initialize class functions */
static void
mediaplayer_app_class_init (MediaplayerAppClass *class)
{
    G_APPLICATION_CLASS (class)->activate = mediaplayer_app_activate;
}

/* construct object */
MediaplayerApp *
mediaplayer_app_new (void)
{
    return g_object_new (MEDIAPLAYER_APP_TYPE,
                         "application-id", "ru.petrsu.mediaplayer",
                         NULL);
}

