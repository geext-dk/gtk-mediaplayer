#ifndef __MEDIAPLAYERWINDOW_H
#define __MEDIAPLAYERWINDOW_H
#include <gtk/gtk.h>
#include "mediaplayer-app.h"

G_BEGIN_DECLS

#define MEDIAPLAYER_WINDOW_TYPE (mediaplayer_window_get_type())
G_DECLARE_FINAL_TYPE (MediaplayerWindow, mediaplayer_window, MEDIAPLAYER, WINDOW, GtkApplicationWindow)

MediaplayerWindow *mediaplayer_window_new (MediaplayerApp *app);


G_END_DECLS

#endif  /* __MEDIAPLAYERWINDOW_H */
