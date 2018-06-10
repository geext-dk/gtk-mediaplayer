#include <gtk/gtk.h>
#include <gst/gst.h> 
#include "mediaplayer-app.h"
#include "mediaplayer-window.h"


struct _MediaplayerWindow
{
    GtkApplicationWindow parent;

    /* timeout id to disable it upon destruction */
    guint refresh_timer_id;

    /* playlist */
    GtkListStore *playlist_model;
    GtkWidget *playlist_view;
    GtkWidget *playlist_widget;

    /* gstreamer pipeline */
    GstElement *pipeline;
    GstState video_state;
    gint64 video_duration;

    /* video area */
    GtkWidget *video_box;
    GtkWidget *video_widget;
    GtkWidget *video_time_label;
    GtkWidget *video_button_play;
    GtkWidget *video_button_pause;
    GtkWidget *current_video_label;

    /* range */
    GtkWidget *video_scale;
    gulong scale_signal_id;

};

G_DEFINE_TYPE (MediaplayerWindow, mediaplayer_window, GTK_TYPE_APPLICATION_WINDOW);

/* list model columns */
enum {
    MEDIAFILE_OBJECT,
    MEDIAFILE_NAME,
    NUMBER_OF_COLUMNS
};

static gboolean
find_element_with_name (const gchar *current_name, GtkTreeIter *iter, MediaplayerWindow *win)
{
    gboolean not_end = TRUE;

    /* get first iterator */
    not_end = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (win->playlist_model), iter);
    if (not_end) {
        gchararray display_name;
        while (not_end) {
            /* get filename */
            gtk_tree_model_get (GTK_TREE_MODEL (win->playlist_model), iter,
                                MEDIAFILE_NAME, &display_name, -1);
            /* get the information */
            if (g_strcmp0 (display_name, current_name) == 0) {
                g_free (display_name);
                return TRUE;
            }
            not_end = gtk_tree_model_iter_next (GTK_TREE_MODEL (win->playlist_model), iter);
        }
        g_free (display_name);
    }
    return FALSE;
}

static void
start_playing (MediaplayerWindow *win)
{
    gtk_widget_set_visible (win->video_button_play, FALSE);
    gtk_widget_set_visible (win->video_button_pause, TRUE);
    gst_element_set_state (win->pipeline, GST_STATE_PLAYING);
}

static void
set_playing (GtkTreeIter *iter, MediaplayerWindow *win)
{
    GError *error = NULL;
    GFile *file;
    gchararray display_name;
    gchararray file_path;
    gchararray cat;
    gchararray uri;

    /* get file */
    gtk_tree_model_get (GTK_TREE_MODEL (win->playlist_model), iter,
                                        MEDIAFILE_NAME, &display_name,
                                        MEDIAFILE_OBJECT, &file, -1);
    gtk_label_set_text (GTK_LABEL (win->current_video_label), display_name);
    file_path = g_file_get_path (file);

    /* combine into uri string */
    cat = g_strconcat ("file://", file_path, NULL);
    uri = gst_filename_to_uri (file_path, &error);
    if (error != NULL) {
        g_printerr("Failed converting %s to URI: %s\n", cat, error->message);
        g_error_free (error);
    } else {
        /* reset variables related to previous media */
        win->video_duration = GST_CLOCK_TIME_NONE;
        win->video_state = GST_STATE_NULL;
        gtk_widget_set_visible (win->video_button_pause, FALSE);
        gtk_widget_set_visible (win->video_button_play, TRUE);
        gst_element_set_state (win->pipeline, GST_STATE_READY);

        g_object_set (G_OBJECT (win->pipeline), "uri", uri, NULL);
        g_free (uri);
    }

    /* cleanup */
    g_object_unref (file);
    g_free (display_name);
    g_free (file_path);
    g_free (cat);
}

/* opens dialog to add a file in the playlist */
static void
playlist_add_cb (GtkWidget *button, MediaplayerWindow *win)
{
    GtkWidget *dialog;
    GtkFileFilter *filter;
    gint res;
    
    dialog = gtk_file_chooser_dialog_new ("Open file",
                                          GTK_WINDOW (win),
                                          GTK_FILE_CHOOSER_ACTION_OPEN,
                                          "_Open", GTK_RESPONSE_ACCEPT,
                                          "_Cancel", GTK_RESPONSE_CANCEL,
                                          NULL);
    /* take ownership of the dialog */
    g_object_ref_sink (dialog);
    
    /* setup filter */
    filter = gtk_file_filter_new ();
    gtk_file_filter_set_name (filter, "Multimedia");
    gtk_file_filter_add_mime_type (filter, "video/*");
    gtk_file_filter_add_mime_type (filter, "audio/*");
    gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dialog), filter);

    /* show dialog */
    res = gtk_dialog_run (GTK_DIALOG (dialog));

    if (res == GTK_RESPONSE_ACCEPT) {
        GtkTreeIter iter;
        GtkFileChooser *chooser;
        gchararray filename;
        GFileInfo *file_info = NULL;
        GFile *file;
        const gchar *display_name;

        /* create file object from the chosen file path */
        chooser = GTK_FILE_CHOOSER (dialog);
        filename = gtk_file_chooser_get_filename (chooser);
        file = g_file_new_for_path (filename);

        /* query file for the information we want to display */
        file_info = g_file_query_info (file,
                            "standard::display-name",
                            G_FILE_QUERY_INFO_NONE,
                            NULL,  /* cancellable */
                            NULL); /* GError */
        if (file_info == NULL) {
            g_error ("Error retrieving information about the file: %s", filename);
        }

        /* get the information */
        display_name = g_file_info_get_attribute_string (file_info, "standard::display-name");

        /* create new row and add the file to the playlist */
        gtk_list_store_append (GTK_LIST_STORE (win->playlist_model), &iter);
        gtk_list_store_set (GTK_LIST_STORE (win->playlist_model), &iter,
                            MEDIAFILE_OBJECT, file,
                            MEDIAFILE_NAME, display_name,
                            -1);

        /* cleanup */
        g_object_unref (file_info);
        g_object_unref (file);
        g_free (filename);
    }

    /* destroy widget and remove reference */
    gtk_widget_destroy (dialog);
    g_object_unref (dialog);
}

/* remove item from the playlist */
static void
playlist_remove_cb (GtkWidget *button, MediaplayerWindow *win)
{
    GtkTreeSelection *selection;
    GtkTreeIter iter;
    GtkTreeModel *tree_model;
    tree_model = GTK_TREE_MODEL (win->playlist_model);

    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (win->playlist_view));
    if (gtk_tree_selection_get_selected (selection, &tree_model, &iter)) {
        gtk_list_store_remove (win->playlist_model, &iter);
    }
}

/* play video */
static void
play_cb (GtkWidget *button, MediaplayerWindow *win)
{
    /* play only if the video is initialized and it isn't already playing */
    if (win->video_state == GST_STATE_NULL || win->video_state == GST_STATE_PLAYING) {
        return;
    }
    start_playing (win);
}

/* pause video playback */
static void
pause_cb (GtkWidget *button, MediaplayerWindow *win)
{
    /* we want to pause only in case the video is playing */
    if (win->video_state != GST_STATE_PLAYING) {
        return;
    }
    gtk_widget_set_visible (win->video_button_play, TRUE);
    gtk_widget_set_visible (win->video_button_pause, FALSE);
    gst_element_set_state (win->pipeline, GST_STATE_PAUSED);
}

/* stop video playback */
static void
stop_cb (GtkWidget *button, MediaplayerWindow *win)
{
    if (win->video_state >= GST_STATE_PAUSED) {
        gtk_widget_set_visible (win->video_button_play, TRUE);
        gtk_widget_set_visible (win->video_button_pause, FALSE);
        gst_element_set_state (win->pipeline, GST_STATE_READY);
    }
}

static void
next_cb (GtkButton *button, MediaplayerWindow *win)
{
    GtkTreeIter iter;
    GtkTreeSelection *selection;

    /* find current */
    const gchar *current_name = gtk_label_get_text (GTK_LABEL (win->current_video_label));
    if (!find_element_with_name (current_name, &iter, win)) {
        return;
    }

    /* select next */
    if (!gtk_tree_model_iter_next (GTK_TREE_MODEL (win->playlist_model), &iter)) {
        return;
    }
    set_playing (&iter, win);
    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (win->playlist_view));
    gtk_tree_selection_select_iter (selection, &iter);
    start_playing (win);
}

static void
previous_cb (GtkButton *button, MediaplayerWindow *win)
{
    GtkTreeIter iter;
    GtkTreeSelection *selection;

    /* find current */
    const gchar *current_name = gtk_label_get_text (GTK_LABEL (win->current_video_label));
    if (!find_element_with_name (current_name, &iter, win)) {
        return;
    }

    /* select previous */
    if (!gtk_tree_model_iter_previous (GTK_TREE_MODEL (win->playlist_model), &iter)) {
        return;
    }
    set_playing (&iter, win);
    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (win->playlist_view));
    gtk_tree_selection_select_iter (selection, &iter);
    start_playing (win);
}

/* select video for playing when the row in the playlist is activated */
static void
row_activated_cb (GtkTreeView *view, GtkTreePath *path, GtkTreeViewColumn *column, MediaplayerWindow *win)
{
    GtkTreeIter iter;
    gtk_tree_model_get_iter (GTK_TREE_MODEL (win->playlist_model), &iter, path);
    set_playing (&iter, win);
    start_playing (win);
}

/* stop video on widget deletion */
static gboolean
delete_cb (GtkWidget *widget, GdkEvent *event, MediaplayerWindow *win)
{
    g_source_remove (win->refresh_timer_id);
    stop_cb(NULL, win);
    return FALSE;
}

/* updates slider position and capacity */
static gboolean
refresh_ui (MediaplayerWindow *win)
{
    gint64 current = -1;
    gint64 seconds_overall = -1;
    gint64 seconds_current = -1;
    gchar current_label[8];
    gchar overall_label[8];
    gchar *label;

    if (win->video_state < GST_STATE_PAUSED) {
        return TRUE;
    }

    if (!GST_CLOCK_TIME_IS_VALID (win->video_duration)) {
        if (!gst_element_query_duration (win->pipeline, GST_FORMAT_TIME, &win->video_duration)) {
            g_error ("Error querying duration");
        } else {
            gtk_range_set_range (GTK_RANGE (win->video_scale), 0, (gdouble)win->video_duration / GST_SECOND);
        }
    }
    seconds_overall = GST_TIME_AS_SECONDS (win->video_duration);
    g_snprintf (overall_label, 8, "/%01ld:%02ld", seconds_overall / 60, seconds_overall % 60);

    if (gst_element_query_position (win->pipeline, GST_FORMAT_TIME, &current)) {
        g_signal_handler_block (win->video_scale, win->scale_signal_id);
        gtk_range_set_value (GTK_RANGE (win->video_scale), (gdouble)current / GST_SECOND);
        g_signal_handler_unblock (win->video_scale, win->scale_signal_id);

        /* set time label */
        seconds_current = GST_TIME_AS_SECONDS (current);
        g_snprintf (current_label, 7, "%01ld:%02ld", seconds_current / 60, seconds_current % 60);
    } else {
        g_snprintf (current_label, 7, " 0:00");
    }

    label = g_strconcat (current_label, overall_label, NULL);
    gtk_label_set_text (GTK_LABEL (win->video_time_label), label);

    g_free (label);
    return TRUE;
}

/* watch state when it changes */
static void
state_changed_cb (GstBus *bus, GstMessage *msg, MediaplayerWindow *win)
{
    GstState old_state, new_state, pending_state;
    gst_message_parse_state_changed (msg, &old_state, &new_state, &pending_state);
    if (GST_MESSAGE_SRC (msg) == GST_OBJECT (win->pipeline)) {
        win->video_state = new_state;
        if (old_state == GST_STATE_READY && new_state == GST_STATE_PAUSED) {
            refresh_ui (win);
        }
    }
}

/* seek video when slider moved */
static void
slider_cb (GtkRange *range, MediaplayerWindow *win)
{
    gdouble value = gtk_range_get_value (range);
    gst_element_seek_simple (win->pipeline,
                             GST_FORMAT_TIME,
                             GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT,
                             (gint64)(value * GST_SECOND));
}

/* print error messages */
static void
error_cb (GstBus *bus, GstMessage *msg, MediaplayerWindow *win)
{
    GError *err;
    gchar *debug_info;

    gst_message_parse_error (msg, &err, &debug_info);
    g_printerr ("Error recieved from element %s: %s\n", GST_OBJECT_NAME (msg->src), err->message);
    g_printerr ("Debugging information: %s\n", debug_info ? debug_info : "none");

    gst_element_set_state (win->pipeline, GST_STATE_READY);

    g_clear_error (&err);
    g_free (debug_info);
}

/* set state to READY when video has ended playing */
static void 
eos_cb (GstBus *bus, GstMessage *msg, MediaplayerWindow *win)
{
    gst_element_set_state (win->pipeline, GST_STATE_READY);
}

static void
drag_and_drop_cb (GtkWidget *widget, GdkDragContext *context, gint x, gint y,
                  GtkSelectionData *data, guint info, guint time, 
                  MediaplayerWindow *win)
{
    g_print ("Gotcha!\n");
}

/* intitialize window */
static void
mediaplayer_window_init (MediaplayerWindow *win)
{
    GstElement *sink, *gtk_gl_sink;
    GtkCellRenderer *renderer;
    GtkTreeViewColumn *column;
    GstBus *bus;

    /* initialize variables */
    win->video_duration = GST_CLOCK_TIME_NONE;
    win->video_state = GST_STATE_NULL;

    /* init template */
    gtk_widget_init_template (GTK_WIDGET (win));

    /* setup playbin */
    win->pipeline = gst_element_factory_make ("playbin", "playbin");
    gtk_gl_sink = gst_element_factory_make ("gtkglsink", "gtk_gl_sink");
    sink = gst_element_factory_make ("glsinkbin", "sink");
    g_object_set (G_OBJECT (sink), "sink", gtk_gl_sink, NULL);
    g_object_set (G_OBJECT (win->pipeline), "video-sink", sink, NULL);

    /* initialize list model with filename and length strings */
    win->playlist_model = gtk_list_store_new (NUMBER_OF_COLUMNS,
                                              G_TYPE_OBJECT,
                                              G_TYPE_STRING,
                                              G_TYPE_STRING,
                                              G_TYPE_STRING);

    gtk_tree_view_set_model (GTK_TREE_VIEW (win->playlist_view), GTK_TREE_MODEL (win->playlist_model));
    renderer = gtk_cell_renderer_text_new ();
    column = gtk_tree_view_column_new_with_attributes ("Name", renderer,
                                                       "text", MEDIAFILE_NAME,
                                                       NULL);
    gtk_tree_view_append_column (GTK_TREE_VIEW (win->playlist_view), column);

    /* add video widget to window */
    g_object_get (G_OBJECT (gtk_gl_sink), "widget", &win->video_widget, NULL);
    gtk_widget_set_size_request (win->video_widget, 400, 300);
    gtk_box_pack_start (GTK_BOX (win->video_box), win->video_widget, 1, 1, 0);

    /* set up gstreamer message handling */
    bus = gst_element_get_bus (win->pipeline);
    gst_bus_add_signal_watch (bus);
    g_signal_connect (G_OBJECT (bus), "message::error",         G_CALLBACK (error_cb),         win);
    g_signal_connect (G_OBJECT (bus), "message::eos",           G_CALLBACK (eos_cb),           win);
    g_signal_connect (G_OBJECT (bus), "message::state-changed", G_CALLBACK (state_changed_cb), win);
    gst_object_unref (bus);

    win->refresh_timer_id = g_timeout_add (200, (GSourceFunc) refresh_ui, win);

    win->scale_signal_id = g_signal_connect (G_OBJECT (win->video_scale),
                                             "value-changed", G_CALLBACK (slider_cb), win);
    g_signal_connect (win->playlist_view, "row-activated", G_CALLBACK (row_activated_cb), win);

    /* set up drag and drop */
    GtkTargetEntry entries[] = {
        {"audio/*",    GTK_TARGET_OTHER_APP, 0},
        {"video/webm", GTK_TARGET_OTHER_APP, 0}
    };
    gtk_drag_dest_set (win->playlist_widget, GTK_DEST_DEFAULT_ALL, entries, 2, GDK_ACTION_COPY);
    // gtk_drag_dest_add_uri_targets (win->playlist_widget);
    g_signal_connect (win->playlist_widget, "drag-data-received", G_CALLBACK (drag_and_drop_cb), win);
                       

    gtk_widget_show (win->video_widget);
}

/* initialize window class */
static void
mediaplayer_window_class_init (MediaplayerWindowClass *class)
{
    gtk_widget_class_set_template_from_resource (GTK_WIDGET_CLASS (class),
                                                 "/ru/petrsu/mediaplayer/window.ui");
    gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), MediaplayerWindow, video_box);
    gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), MediaplayerWindow, playlist_view);
    gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), MediaplayerWindow, playlist_widget);
    gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), MediaplayerWindow, video_scale);
    gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), MediaplayerWindow, video_time_label);
    gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), MediaplayerWindow, video_button_play);
    gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), MediaplayerWindow, video_button_pause);
    gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), MediaplayerWindow, current_video_label);

    gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (class), playlist_add_cb);
    gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (class), playlist_remove_cb);

    gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (class), play_cb);
    gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (class), stop_cb);
    gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (class), pause_cb);

    gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (class), row_activated_cb);
    gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (class), delete_cb);
    
    gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (class), next_cb);
    gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (class), previous_cb);
}

MediaplayerWindow *
mediaplayer_window_new (MediaplayerApp *app)
{
    return g_object_new (MEDIAPLAYER_WINDOW_TYPE,
                        "application", app,
                        NULL);
}

