/* gtk3_linux.h — собственные прототипы GTK3/GLib (ADR-3). Только объявления:
   файл подключается в преамбуле файла с //export.
   В режиме NUI_CHECK (tools/check-protos.sh) всё сверяется с настоящими заголовками. */
#ifndef NUI_GTK3_H
#define NUI_GTK3_H

/* ---------- 1. Базовые типы (в режиме проверки берутся из настоящих заголовков) ---------- */
#ifndef NUI_CHECK
typedef __UINTPTR_TYPE__ uintptr_t;
typedef __SIZE_TYPE__ size_t;
typedef int gint, gboolean;
typedef unsigned int guint;
typedef unsigned long gulong;
typedef char gchar;
typedef float gfloat;
typedef void *gpointer;
/* Перечисления: в ABI это int. В режиме проверки — настоящие enum. */
typedef int GtkWindowType, GtkWindowPosition, GtkOrientation, GtkAlign, GConnectFlags, GIOCondition,
            GtkPolicyType, GtkWrapMode, GtkJustification, PangoEllipsizeMode, PangoWeight,
            GtkDialogFlags, GtkMessageType, GtkButtonsType, GtkFileChooserAction, GBusType, GDBusCallFlags;
typedef unsigned int guint32;
typedef unsigned long GType;
typedef unsigned short guint16;
typedef double gdouble;
typedef long gssize;
typedef void (*GCallback)(void);
typedef gboolean (*GSourceFunc)(gpointer);
typedef gboolean (*GUnixFDSourceFunc)(gint, GIOCondition, gpointer);
typedef void (*GDestroyNotify)(gpointer);
typedef void (*GClosureNotify)(gpointer, void *);
/* Непрозрачные объекты */
typedef struct _GtkWidget GtkWidget;       typedef struct _GtkWindow GtkWindow;
typedef struct _GtkContainer GtkContainer; typedef struct _GtkBin GtkBin;
typedef struct _GtkBox GtkBox;             typedef struct _GtkLabel GtkLabel;
typedef struct _GtkButton GtkButton;       typedef struct _GtkToggleButton GtkToggleButton;
typedef struct _GtkEntry GtkEntry;         typedef struct _GtkEditable GtkEditable;
typedef struct _GtkComboBox GtkComboBox;   typedef struct _GtkComboBoxText GtkComboBoxText;
typedef struct _GtkRange GtkRange;         typedef struct _GtkScale GtkScale;
typedef struct _GtkProgressBar GtkProgressBar;
typedef struct _GtkGrid GtkGrid;           typedef struct _GtkFrame GtkFrame;
typedef struct _GtkScrolledWindow GtkScrolledWindow; typedef struct _GtkAdjustment GtkAdjustment;
typedef struct _GtkTextView GtkTextView;   typedef struct _GtkTextBuffer GtkTextBuffer;
typedef struct _GtkTextIter GtkTextIter;   typedef struct _GtkTextMark GtkTextMark;
typedef struct _PangoAttrList PangoAttrList; typedef struct _PangoAttribute PangoAttribute;
typedef struct _GObject GObject;           typedef struct _GTypeInstance GTypeInstance;
typedef struct _GtkDialog GtkDialog;       typedef struct _GtkMessageDialog GtkMessageDialog;
typedef struct _GtkFileChooser GtkFileChooser; typedef struct _GtkFileFilter GtkFileFilter;
typedef struct _GtkNativeDialog GtkNativeDialog;
typedef struct _GtkFileChooserNative GtkFileChooserNative;
typedef struct _GError GError;             typedef struct _GVariant GVariant;
typedef struct _GVariantType GVariantType; typedef struct _GDBusConnection GDBusConnection;
typedef struct _GCancellable GCancellable; typedef struct _GAsyncResult GAsyncResult;
typedef struct _GdkWindow GdkWindow;
typedef struct _GtkCssProvider GtkCssProvider; typedef struct _GtkStyleContext GtkStyleContext;
typedef struct _GtkStyleProvider GtkStyleProvider;
typedef void (*GAsyncReadyCallback)(GObject *, GAsyncResult *, gpointer);
typedef struct _GList GList;
struct _GList { gpointer data; GList *next; GList *prev; };
/* GtkAllocation (= GdkRectangle) — только для тестового помощника nui_allocation */
typedef struct { int x, y, width, height; } GtkAllocation;
#endif

/* Место под GtkTextIter на стеке C: настоящий размер ~80 байт, берём с запасом.
   В режиме проверки размер сверяется (раздел 3). Go к итераторам не прикасается. */
typedef struct { void *p[16]; } NuiTextIter;

/* ---------- 2. Макросы ---------- */
#ifdef NUI_CHECK
#define NUI_FN(ret, name, params) \
	_Static_assert(__builtin_types_compatible_p(__typeof__(&name), ret (*) params), #name);
#define NUI_CONST(name, val) \
	_Static_assert((val) == (name), #name); enum { NUI_##name = (val) };
/* Функции без аргументов с G_GNUC_CONST (*_get_type): атрибут const у GCC — часть типа
   функции, поэтому сверяется тип результата. */
#define NUI_FN_CONST(ret, name) \
	_Static_assert(__builtin_types_compatible_p(__typeof__(name()), ret), #name);
#else
#define NUI_FN(ret, name, params) ret name params;
#define NUI_CONST(name, val) enum { NUI_##name = (val) };
#define NUI_FN_CONST(ret, name) ret name(void) __attribute__((const));
#endif

/* ---------- 3. Константы ---------- */
NUI_CONST(GTK_WINDOW_TOPLEVEL, 0)
NUI_CONST(GTK_WIN_POS_CENTER, 1)
NUI_CONST(GTK_ORIENTATION_HORIZONTAL, 0)
NUI_CONST(GTK_ORIENTATION_VERTICAL, 1)
NUI_CONST(GTK_ALIGN_FILL, 0)
NUI_CONST(GTK_ALIGN_START, 1)
NUI_CONST(GTK_ALIGN_END, 2)
NUI_CONST(GTK_ALIGN_CENTER, 3)
NUI_CONST(G_PRIORITY_DEFAULT, 0)
NUI_CONST(G_IO_IN, 1)
NUI_CONST(G_IO_PRI, 2)
NUI_CONST(G_IO_OUT, 4)
NUI_CONST(G_IO_ERR, 8)
NUI_CONST(G_IO_HUP, 16)
NUI_CONST(G_IO_NVAL, 32)
NUI_CONST(GTK_POLICY_AUTOMATIC, 1)
NUI_CONST(GTK_POLICY_NEVER, 2)
NUI_CONST(GTK_WRAP_NONE, 0)
NUI_CONST(GTK_WRAP_WORD_CHAR, 3)
NUI_CONST(GTK_STYLE_PROVIDER_PRIORITY_APPLICATION, 600)
NUI_CONST(GTK_JUSTIFY_LEFT, 0)
NUI_CONST(GTK_JUSTIFY_RIGHT, 1)
NUI_CONST(GTK_JUSTIFY_CENTER, 2)
NUI_CONST(PANGO_ELLIPSIZE_NONE, 0)
NUI_CONST(PANGO_ELLIPSIZE_START, 1)
NUI_CONST(PANGO_ELLIPSIZE_MIDDLE, 2)
NUI_CONST(PANGO_ELLIPSIZE_END, 3)
NUI_CONST(PANGO_WEIGHT_NORMAL, 400)
NUI_CONST(PANGO_WEIGHT_BOLD, 700)
NUI_CONST(GTK_DIALOG_MODAL, 1)
NUI_CONST(GTK_DIALOG_DESTROY_WITH_PARENT, 2)
NUI_CONST(GTK_MESSAGE_INFO, 0)
NUI_CONST(GTK_MESSAGE_QUESTION, 2)
NUI_CONST(GTK_MESSAGE_ERROR, 3)
NUI_CONST(GTK_BUTTONS_OK, 1)
NUI_CONST(GTK_BUTTONS_YES_NO, 4)
NUI_CONST(GTK_RESPONSE_ACCEPT, -3)
NUI_CONST(GTK_RESPONSE_DELETE_EVENT, -4)
NUI_CONST(GTK_RESPONSE_OK, -5)
NUI_CONST(GTK_RESPONSE_CANCEL, -6)
NUI_CONST(GTK_RESPONSE_YES, -8)
NUI_CONST(GTK_RESPONSE_NO, -9)
NUI_CONST(GTK_FILE_CHOOSER_ACTION_OPEN, 0)
NUI_CONST(GTK_FILE_CHOOSER_ACTION_SAVE, 1)
NUI_CONST(GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER, 2)
NUI_CONST(G_BUS_TYPE_SESSION, 2)
NUI_CONST(G_DBUS_CALL_FLAGS_NONE, 0)
#ifdef NUI_CHECK
_Static_assert(sizeof(NuiTextIter) >= sizeof(GtkTextIter), "NuiTextIter меньше GtkTextIter");
_Static_assert(sizeof(GType) == sizeof(unsigned long), "GType");
#endif

/* ---------- 4. Прототипы ---------- */
/* GLib, GObject, цикл событий. (w) — только из обёрток. */
NUI_FN(gboolean, gtk_init_check, (int *argc, char ***argv))
NUI_FN(void, gtk_main, (void))
NUI_FN(void, gtk_main_quit, (void))
NUI_FN(void, g_set_prgname, (const gchar *prgname))
NUI_FN(gulong, g_signal_connect_data, (gpointer instance, const gchar *detailed_signal,
       GCallback c_handler, gpointer data, GClosureNotify destroy_data, GConnectFlags connect_flags)) /* (w) */
NUI_FN(guint, g_timeout_add_full, (gint priority, guint interval, GSourceFunc function,
       gpointer data, GDestroyNotify notify))                                                       /* (w) */
NUI_FN(guint, g_idle_add_full, (gint priority, GSourceFunc function, gpointer data, GDestroyNotify notify)) /* (w) */
NUI_FN(guint, g_unix_fd_add_full, (gint priority, gint fd, GIOCondition condition,
       GUnixFDSourceFunc function, gpointer user_data, GDestroyNotify notify))                      /* (w) */
NUI_FN(gboolean, g_source_remove, (guint tag))
NUI_FN(void, g_free, (gpointer mem))
NUI_FN(void, g_object_set_data, (GObject *object, const gchar *key, gpointer data))   /* (w) */
NUI_FN(gpointer, g_object_get_data, (GObject *object, const gchar *key))              /* (w) */
NUI_FN(gpointer, g_object_ref, (gpointer object))                                      /* (w) */
NUI_FN(void, g_object_unref, (gpointer object))
NUI_FN(void, g_object_set_data_full, (GObject *object, const gchar *key, gpointer data, GDestroyNotify destroy)) /* (w) */
NUI_FN(gchar *, g_strdup, (const gchar *str))                                          /* (w) */
NUI_FN(void, g_error_free, (GError *error))
NUI_FN(gchar *, g_filename_to_uri, (const gchar *filename, const gchar *hostname, GError **error)) /* g_free */
NUI_FN(gboolean, g_type_check_instance_is_a, (GTypeInstance *instance, GType iface_type))  /* тесты */
NUI_FN(void, g_list_free, (GList *list))                                                   /* тесты */
NUI_FN(size_t, strlen, (const char *s))
NUI_FN(int, gettid, (void)) /* pid_t; glibc ≥ 2.30 — ниже фактического минимума 2.34 */
NUI_FN(int, getpid, (void))

/* GtkWidget, GtkContainer, GtkBin */
NUI_FN(void, gtk_widget_show, (GtkWidget *widget))
NUI_FN(void, gtk_widget_hide, (GtkWidget *widget))
NUI_FN(void, gtk_widget_destroy, (GtkWidget *widget))
NUI_FN(void, gtk_widget_set_sensitive, (GtkWidget *widget, gboolean sensitive))
NUI_FN(gboolean, gtk_widget_get_sensitive, (GtkWidget *widget))
NUI_FN(void, gtk_widget_set_visible, (GtkWidget *widget, gboolean visible))
NUI_FN(gboolean, gtk_widget_get_visible, (GtkWidget *widget))
NUI_FN(void, gtk_widget_set_tooltip_text, (GtkWidget *widget, const gchar *text))
NUI_FN(void, gtk_widget_set_halign, (GtkWidget *widget, GtkAlign align))
NUI_FN(void, gtk_widget_set_valign, (GtkWidget *widget, GtkAlign align))
NUI_FN(void, gtk_widget_set_hexpand, (GtkWidget *widget, gboolean expand))
NUI_FN(void, gtk_widget_set_vexpand, (GtkWidget *widget, gboolean expand))
NUI_FN(void, gtk_widget_set_size_request, (GtkWidget *widget, gint width, gint height))
NUI_FN(void, gtk_widget_grab_focus, (GtkWidget *widget))
NUI_FN(void, gtk_widget_get_allocation, (GtkWidget *widget, GtkAllocation *allocation)) /* (w), тесты */
NUI_FN(void, gtk_widget_get_preferred_width, (GtkWidget *widget, gint *minimum_width, gint *natural_width))     /* тесты */
NUI_FN(void, gtk_widget_get_preferred_height, (GtkWidget *widget, gint *minimum_height, gint *natural_height))  /* тесты */
NUI_FN(gboolean, gtk_widget_translate_coordinates, (GtkWidget *src_widget, GtkWidget *dest_widget,
       gint src_x, gint src_y, gint *dest_x, gint *dest_y))                                                     /* тесты */
NUI_FN(GtkWidget *, gtk_widget_get_toplevel, (GtkWidget *widget))                                              /* тесты */
NUI_FN(GdkWindow *, gtk_widget_get_window, (GtkWidget *widget))                                                /* тесты */
NUI_FN(gint, gdk_window_get_origin, (GdkWindow *window, gint *x, gint *y))                                     /* тесты */
NUI_FN(void, gtk_container_add, (GtkContainer *container, GtkWidget *widget))
NUI_FN(void, gtk_container_remove, (GtkContainer *container, GtkWidget *widget))
NUI_FN(void, gtk_container_set_border_width, (GtkContainer *container, guint border_width))
NUI_FN(guint, gtk_container_get_border_width, (GtkContainer *container))                     /* (w) */
NUI_FN(GtkStyleContext *, gtk_widget_get_style_context, (GtkWidget *widget))                  /* (w) */
NUI_FN(void, gtk_style_context_add_provider, (GtkStyleContext *context, GtkStyleProvider *provider, guint priority)) /* (w) */
NUI_FN(GtkCssProvider *, gtk_css_provider_new, (void))                                       /* (w) */
NUI_FN(gboolean, gtk_css_provider_load_from_data, (GtkCssProvider *css_provider, const gchar *data, gssize length,
       GError **error))                                                                     /* (w) */
NUI_FN(GtkWidget *, gtk_bin_get_child, (GtkBin *bin))

/* GtkWindow */
NUI_FN(GtkWidget *, gtk_window_new, (GtkWindowType type))
NUI_FN(void, gtk_window_set_title, (GtkWindow *window, const gchar *title))
NUI_FN(const gchar *, gtk_window_get_title, (GtkWindow *window))
NUI_FN(void, gtk_window_set_position, (GtkWindow *window, GtkWindowPosition position))
NUI_FN(void, gtk_window_present, (GtkWindow *window))
NUI_FN(void, gtk_window_set_default_size, (GtkWindow *window, gint width, gint height))
NUI_FN(void, gtk_window_resize, (GtkWindow *window, gint width, gint height))
NUI_FN(void, gtk_window_set_resizable, (GtkWindow *window, gboolean resizable))
NUI_FN(gboolean, gtk_window_get_resizable, (GtkWindow *window))
NUI_FN(void, gtk_window_get_default_size, (GtkWindow *window, gint *width, gint *height))
NUI_FN(void, gtk_window_set_default, (GtkWindow *window, GtkWidget *default_widget))
NUI_FN(void, gtk_window_set_icon_name, (GtkWindow *window, const gchar *name))
NUI_FN(void, gtk_window_maximize, (GtkWindow *window))
NUI_FN(void, gtk_window_unmaximize, (GtkWindow *window))
NUI_FN(void, gtk_window_fullscreen, (GtkWindow *window))
NUI_FN(void, gtk_window_unfullscreen, (GtkWindow *window))
NUI_FN(void, gtk_widget_set_can_default, (GtkWidget *widget, gboolean can_default))
NUI_FN(gboolean, gtk_widget_activate, (GtkWidget *widget)) /* тесты: SimEnter */
NUI_FN(void, gtk_window_close, (GtkWindow *window)) /* тесты: SimClose */

/* GtkLabel, GtkButton */
NUI_FN(GtkWidget *, gtk_label_new, (const gchar *str))
NUI_FN(void, gtk_label_set_text, (GtkLabel *label, const gchar *str))
NUI_FN(const gchar *, gtk_label_get_text, (GtkLabel *label))
NUI_FN(void, gtk_label_set_xalign, (GtkLabel *label, gfloat xalign))
NUI_FN(void, gtk_label_set_justify, (GtkLabel *label, GtkJustification jtype))
NUI_FN(void, gtk_label_set_ellipsize, (GtkLabel *label, PangoEllipsizeMode mode))
NUI_FN(void, gtk_label_set_line_wrap, (GtkLabel *label, gboolean wrap))
NUI_FN(void, gtk_label_set_selectable, (GtkLabel *label, gboolean setting))
NUI_FN(void, gtk_label_set_attributes, (GtkLabel *label, PangoAttrList *attrs))            /* (w) */
NUI_FN(PangoAttrList *, pango_attr_list_new, (void))                                        /* (w) */
NUI_FN(void, pango_attr_list_insert, (PangoAttrList *list, PangoAttribute *attr))           /* (w) */
NUI_FN(void, pango_attr_list_unref, (PangoAttrList *list))                                  /* (w) */
NUI_FN(PangoAttribute *, pango_attr_weight_new, (PangoWeight weight))                       /* (w) */
NUI_FN(PangoAttribute *, pango_attr_foreground_new, (guint16 red, guint16 green, guint16 blue)) /* (w) */
NUI_FN(PangoAttribute *, pango_attr_scale_new, (double scale_factor))                        /* (w) */
NUI_FN(GtkWidget *, gtk_button_new_with_label, (const gchar *label))
NUI_FN(void, gtk_button_set_label, (GtkButton *button, const gchar *label))
NUI_FN(const gchar *, gtk_button_get_label, (GtkButton *button))
NUI_FN(void, gtk_button_clicked, (GtkButton *button)) /* тесты: SimClick */
NUI_FN(GtkWidget *, gtk_check_button_new_with_label, (const gchar *label))
NUI_FN(void, gtk_toggle_button_set_active, (GtkToggleButton *toggle_button, gboolean is_active))
NUI_FN(gboolean, gtk_toggle_button_get_active, (GtkToggleButton *toggle_button))

/* GtkEntry */
NUI_FN(GtkWidget *, gtk_entry_new, (void))
NUI_FN(void, gtk_entry_set_text, (GtkEntry *entry, const gchar *text))
NUI_FN(const gchar *, gtk_entry_get_text, (GtkEntry *entry))
NUI_FN(void, gtk_entry_set_placeholder_text, (GtkEntry *entry, const gchar *text))
NUI_FN(void, gtk_entry_set_visibility, (GtkEntry *entry, gboolean visible))
NUI_FN(void, gtk_editable_set_editable, (GtkEditable *editable, gboolean is_editable))
NUI_FN(void, gtk_entry_set_width_chars, (GtkEntry *entry, gint n_chars))
NUI_FN(void, gtk_entry_set_activates_default, (GtkEntry *entry, gboolean setting))

/* GtkComboBoxText */
NUI_FN(GtkWidget *, gtk_combo_box_text_new, (void))
NUI_FN(GtkWidget *, gtk_combo_box_text_new_with_entry, (void))
NUI_FN(void, gtk_combo_box_text_append_text, (GtkComboBoxText *combo_box, const gchar *text))
NUI_FN(void, gtk_combo_box_text_remove_all, (GtkComboBoxText *combo_box))
NUI_FN(gint, gtk_combo_box_get_active, (GtkComboBox *combo_box))
NUI_FN(void, gtk_combo_box_set_active, (GtkComboBox *combo_box, gint index_))
NUI_FN(gchar *, gtk_combo_box_text_get_active_text, (GtkComboBoxText *combo_box)) /* g_free */

/* GtkScale, GtkRange, GtkProgressBar */
NUI_FN(GtkWidget *, gtk_scale_new_with_range, (GtkOrientation orientation, gdouble min, gdouble max, gdouble step))
NUI_FN(void, gtk_scale_set_digits, (GtkScale *scale, gint digits))
NUI_FN(void, gtk_range_set_value, (GtkRange *range, gdouble value))
NUI_FN(gdouble, gtk_range_get_value, (GtkRange *range))
NUI_FN(GtkWidget *, gtk_progress_bar_new, (void))
NUI_FN(void, gtk_progress_bar_set_fraction, (GtkProgressBar *pbar, gdouble fraction))
NUI_FN(gdouble, gtk_progress_bar_get_fraction, (GtkProgressBar *pbar))
NUI_FN(void, gtk_progress_bar_pulse, (GtkProgressBar *pbar))
NUI_FN(void, gtk_progress_bar_set_text, (GtkProgressBar *pbar, const gchar *text))
NUI_FN(void, gtk_progress_bar_set_show_text, (GtkProgressBar *pbar, gboolean show_text))

/* GtkTextView, GtkScrolledWindow */
NUI_FN(GtkWidget *, gtk_scrolled_window_new, (GtkAdjustment *hadjustment, GtkAdjustment *vadjustment))
NUI_FN(void, gtk_scrolled_window_set_policy, (GtkScrolledWindow *scrolled_window,
       GtkPolicyType hscrollbar_policy, GtkPolicyType vscrollbar_policy))
NUI_FN(void, gtk_scrolled_window_set_propagate_natural_width, (GtkScrolledWindow *scrolled_window, gboolean propagate))
NUI_FN(void, gtk_scrolled_window_set_propagate_natural_height, (GtkScrolledWindow *scrolled_window, gboolean propagate))
NUI_FN(GtkWidget *, gtk_text_view_new, (void))
NUI_FN(GtkTextBuffer *, gtk_text_view_get_buffer, (GtkTextView *text_view))
NUI_FN(void, gtk_text_buffer_set_text, (GtkTextBuffer *buffer, const gchar *text, gint len))
NUI_FN(void, gtk_text_view_set_editable, (GtkTextView *text_view, gboolean setting))
NUI_FN(void, gtk_text_view_set_cursor_visible, (GtkTextView *text_view, gboolean setting))
NUI_FN(void, gtk_text_view_set_monospace, (GtkTextView *text_view, gboolean monospace))
NUI_FN(void, gtk_text_view_set_wrap_mode, (GtkTextView *text_view, GtkWrapMode wrap_mode))
NUI_FN(void, gtk_text_view_set_left_margin, (GtkTextView *text_view, gint left_margin))
NUI_FN(void, gtk_text_view_set_right_margin, (GtkTextView *text_view, gint right_margin))
NUI_FN(void, gtk_text_view_set_top_margin, (GtkTextView *text_view, gint top_margin))
NUI_FN(void, gtk_text_view_set_bottom_margin, (GtkTextView *text_view, gint bottom_margin))
NUI_FN(void, gtk_text_buffer_get_start_iter, (GtkTextBuffer *buffer, GtkTextIter *iter))           /* (w) */
NUI_FN(void, gtk_text_buffer_get_end_iter, (GtkTextBuffer *buffer, GtkTextIter *iter))             /* (w) */
NUI_FN(void, gtk_text_buffer_get_iter_at_line, (GtkTextBuffer *buffer, GtkTextIter *iter, gint line_number)) /* (w) */
NUI_FN(gchar *, gtk_text_buffer_get_text, (GtkTextBuffer *buffer, const GtkTextIter *start,
       const GtkTextIter *end, gboolean include_hidden_chars))                                    /* (w) */
NUI_FN(void, gtk_text_buffer_insert, (GtkTextBuffer *buffer, GtkTextIter *iter, const gchar *text, gint len)) /* (w) */
NUI_FN(void, gtk_text_buffer_delete, (GtkTextBuffer *buffer, GtkTextIter *start, GtkTextIter *end)) /* (w) */
NUI_FN(gint, gtk_text_buffer_get_line_count, (GtkTextBuffer *buffer))                               /* (w) */
NUI_FN(gboolean, gtk_text_iter_starts_line, (const GtkTextIter *iter))                              /* (w) */
NUI_FN(GtkTextMark *, gtk_text_buffer_get_mark, (GtkTextBuffer *buffer, const gchar *name))         /* (w) */
NUI_FN(GtkTextMark *, gtk_text_buffer_create_mark, (GtkTextBuffer *buffer, const gchar *mark_name,
       const GtkTextIter *where, gboolean left_gravity))                                          /* (w) */
NUI_FN(void, gtk_text_buffer_move_mark, (GtkTextBuffer *buffer, GtkTextMark *mark, const GtkTextIter *where)) /* (w) */
NUI_FN(void, gtk_text_view_scroll_mark_onscreen, (GtkTextView *text_view, GtkTextMark *mark))       /* (w) */

/* Контейнеры */
NUI_FN(GtkWidget *, gtk_box_new, (GtkOrientation orientation, gint spacing))
NUI_FN(void, gtk_box_set_spacing, (GtkBox *box, gint spacing))
NUI_FN(void, gtk_box_pack_start, (GtkBox *box, GtkWidget *child, gboolean expand, gboolean fill, guint padding))
NUI_FN(GtkWidget *, gtk_grid_new, (void))
NUI_FN(void, gtk_grid_attach, (GtkGrid *grid, GtkWidget *child, gint left, gint top, gint width, gint height))
NUI_FN(void, gtk_grid_set_row_spacing, (GtkGrid *grid, guint spacing))
NUI_FN(void, gtk_grid_set_column_spacing, (GtkGrid *grid, guint spacing))
NUI_FN(GtkWidget *, gtk_separator_new, (GtkOrientation orientation))
NUI_FN(GtkWidget *, gtk_frame_new, (const gchar *label))
NUI_FN(void, gtk_frame_set_label, (GtkFrame *frame, const gchar *label))

/* Диалоги и оболочка */
NUI_FN(GtkWidget *, gtk_message_dialog_new, (GtkWindow *parent, GtkDialogFlags flags, GtkMessageType type,
       GtkButtonsType buttons, const gchar *message_format, ...))                                        /* (w) */
NUI_FN(void, gtk_message_dialog_format_secondary_text, (GtkMessageDialog *message_dialog,
       const gchar *message_format, ...))                                                                /* (w) */
NUI_FN(gint, gtk_dialog_run, (GtkDialog *dialog))                                                         /* (w) */
NUI_FN(void, gtk_dialog_response, (GtkDialog *dialog, gint response_id))                                  /* тесты */
NUI_FN_CONST(GType, gtk_dialog_get_type)                                                                  /* тесты */
NUI_FN_CONST(GType, gtk_file_chooser_get_type)                                                            /* тесты */
NUI_FN(GList *, gtk_window_list_toplevels, (void))                                                        /* тесты */
NUI_FN(GtkFileChooserNative *, gtk_file_chooser_native_new, (const gchar *title, GtkWindow *parent,
       GtkFileChooserAction action, const gchar *accept_label, const gchar *cancel_label))
NUI_FN(gint, gtk_native_dialog_run, (GtkNativeDialog *self))
NUI_FN(void, gtk_native_dialog_set_modal, (GtkNativeDialog *self, gboolean modal))
NUI_FN(gchar *, gtk_file_chooser_get_filename, (GtkFileChooser *chooser)) /* g_free */
NUI_FN(gboolean, gtk_file_chooser_set_filename, (GtkFileChooser *chooser, const char *filename))          /* тесты */
NUI_FN(void, gtk_file_chooser_set_current_name, (GtkFileChooser *chooser, const gchar *name))
NUI_FN(void, gtk_file_chooser_set_do_overwrite_confirmation, (GtkFileChooser *chooser, gboolean do_overwrite_confirmation))
NUI_FN(GtkFileFilter *, gtk_file_filter_new, (void))
NUI_FN(void, gtk_file_filter_set_name, (GtkFileFilter *filter, const gchar *name))
NUI_FN(void, gtk_file_filter_add_pattern, (GtkFileFilter *filter, const gchar *pattern))
NUI_FN(void, gtk_file_chooser_add_filter, (GtkFileChooser *chooser, GtkFileFilter *filter))
NUI_FN(gboolean, gtk_show_uri_on_window, (GtkWindow *parent, const char *uri, guint32 timestamp, GError **error))
NUI_FN(GDBusConnection *, g_bus_get_sync, (GBusType bus_type, GCancellable *cancellable, GError **error))  /* (w) */
NUI_FN(GVariant *, g_variant_new, (const gchar *format_string, ...))                                       /* (w) */
NUI_FN(void, g_dbus_connection_call, (GDBusConnection *connection, const gchar *bus_name,
       const gchar *object_path, const gchar *interface_name, const gchar *method_name,
       GVariant *parameters, const GVariantType *reply_type, GDBusCallFlags flags, gint timeout_msec,
       GCancellable *cancellable, GAsyncReadyCallback callback, gpointer user_data))                      /* (w) */
NUI_FN(GVariant *, g_dbus_connection_call_finish, (GDBusConnection *connection, GAsyncResult *res, GError **error)) /* (w) */
NUI_FN(void, g_variant_unref, (GVariant *value))                                                           /* (w) */

/* ---------- 5. Наши обёртки (wrap_linux.c) и трамплины (export_linux.go) ----------
   Объявляются в ОБОИХ режимах: в режиме проверки wrap_linux.c компилируется против
   настоящих заголовков (tools/check-protos.sh, шаг 2), и без этих объявлений трамплины
   остались бы необъявленными. */
gulong nui_connect(gpointer obj, const gchar *sig, uintptr_t ud);
gulong nui_connect_ret(gpointer obj, const gchar *sig, uintptr_t ud);
guint  nui_timeout(guint ms, uintptr_t ud);
guint  nui_idle(uintptr_t ud);
guint  nui_watch(int fd, unsigned cond, uintptr_t ud);
void   nui_label_style(GtkLabel *l, int set_bold, int bold, int set_color, unsigned color, int scale_pct);
void   nui_css(GtkWidget *w, int what, unsigned value);
void   nui_css_background(GtkWidget *w, unsigned color, int panel);
void   nui_set_padding(GtkWidget *w, unsigned px);
char  *nui_text_get(GtkTextBuffer *b);
void   nui_text_append(GtkTextView *v, const gchar *s, int len, int autoscroll, int max_lines);
int    nui_message(GtkWindow *parent, int type, int buttons, const gchar *title, const gchar *text);
int    nui_show_items(const gchar *uri, const gchar *dir_uri);
void   nui_allocation(GtkWidget *w, int out[4]); /* только с NUI_GUI_TEST */
void   nui_preferred(GtkWidget *w, int out[4]);  /* только с NUI_GUI_TEST */
void   nui_window_pos(GtkWidget *w, int out[4]); /* только с NUI_GUI_TEST */
int    nui_sim_dialog_response(int chooser, int response); /* только с NUI_GUI_TEST */
int    nui_sim_file_chooser_set(const gchar *path);         /* только с NUI_GUI_TEST */
int    nui_dialog_open(int chooser);                        /* только с NUI_GUI_TEST */

void nuiSignal(void *obj, uintptr_t ud);
int  nuiSignalRet(void *obj, void *arg, uintptr_t ud);
int  nuiSource(uintptr_t ud);
int  nuiWatch(int fd, unsigned cond, uintptr_t ud);

#endif
