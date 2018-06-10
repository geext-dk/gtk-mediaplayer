#ifndef __MEDIAPLAYERAPP_H
#define __MEDIAPLAYERAPP_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define MEDIAPLAYER_APP_TYPE (mediaplayer_app_get_type ())
G_DECLARE_FINAL_TYPE (MediaplayerApp, mediaplayer_app, MEDIAPLAYER, APP, GtkApplication)
MediaplayerApp *mediaplayer_app_new (void);

G_END_DECLS

#endif  /* __MEDIAPLAYERAPP_H  */
