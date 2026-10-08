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
typedef int GtkWindowType, GtkWindowPosition, GtkOrientation, GtkAlign, GConnectFlags, GIOCondition;
typedef void (*GCallback)(void);
typedef gboolean (*GSourceFunc)(gpointer);
typedef gboolean (*GUnixFDSourceFunc)(gint, GIOCondition, gpointer);
typedef void (*GDestroyNotify)(gpointer);
typedef void (*GClosureNotify)(gpointer, void *);
/* Непрозрачные объекты */
typedef struct _GtkWidget GtkWidget;       typedef struct _GtkWindow GtkWindow;
typedef struct _GtkContainer GtkContainer; typedef struct _GtkBin GtkBin;
typedef struct _GtkBox GtkBox;             typedef struct _GtkLabel GtkLabel;
typedef struct _GtkButton GtkButton;
/* GtkAllocation (= GdkRectangle) — только для тестового помощника nui_allocation */
typedef struct { int x, y, width, height; } GtkAllocation;
#endif

/* ---------- 2. Макросы ---------- */
#ifdef NUI_CHECK
#define NUI_FN(ret, name, params) \
	_Static_assert(__builtin_types_compatible_p(__typeof__(&name), ret (*) params), #name);
#define NUI_CONST(name, val) \
	_Static_assert((val) == (name), #name); enum { NUI_##name = (val) };
#else
#define NUI_FN(ret, name, params) ret name params;
#define NUI_CONST(name, val) enum { NUI_##name = (val) };
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
NUI_FN(void, gtk_container_add, (GtkContainer *container, GtkWidget *widget))
NUI_FN(void, gtk_container_remove, (GtkContainer *container, GtkWidget *widget))
NUI_FN(void, gtk_container_set_border_width, (GtkContainer *container, guint border_width))
NUI_FN(GtkWidget *, gtk_bin_get_child, (GtkBin *bin))

/* GtkWindow */
NUI_FN(GtkWidget *, gtk_window_new, (GtkWindowType type))
NUI_FN(void, gtk_window_set_title, (GtkWindow *window, const gchar *title))
NUI_FN(const gchar *, gtk_window_get_title, (GtkWindow *window))
NUI_FN(void, gtk_window_set_position, (GtkWindow *window, GtkWindowPosition position))
NUI_FN(void, gtk_window_present, (GtkWindow *window))
NUI_FN(void, gtk_window_close, (GtkWindow *window)) /* тесты: SimClose */

/* GtkLabel, GtkButton */
NUI_FN(GtkWidget *, gtk_label_new, (const gchar *str))
NUI_FN(void, gtk_label_set_text, (GtkLabel *label, const gchar *str))
NUI_FN(const gchar *, gtk_label_get_text, (GtkLabel *label))
NUI_FN(void, gtk_label_set_xalign, (GtkLabel *label, gfloat xalign))
NUI_FN(GtkWidget *, gtk_button_new_with_label, (const gchar *label))
NUI_FN(void, gtk_button_set_label, (GtkButton *button, const gchar *label))
NUI_FN(const gchar *, gtk_button_get_label, (GtkButton *button))
NUI_FN(void, gtk_button_clicked, (GtkButton *button)) /* тесты: SimClick */

/* Контейнеры */
NUI_FN(GtkWidget *, gtk_box_new, (GtkOrientation orientation, gint spacing))
NUI_FN(void, gtk_box_set_spacing, (GtkBox *box, gint spacing))
NUI_FN(void, gtk_box_pack_start, (GtkBox *box, GtkWidget *child, gboolean expand, gboolean fill, guint padding))

/* ---------- 5. Наши обёртки (wrap_linux.c) и трамплины (export_linux.go) ----------
   Объявляются в ОБОИХ режимах: в режиме проверки wrap_linux.c компилируется против
   настоящих заголовков (tools/check-protos.sh, шаг 2), и без этих объявлений трамплины
   остались бы необъявленными. */
gulong nui_connect(gpointer obj, const gchar *sig, uintptr_t ud);
gulong nui_connect_ret(gpointer obj, const gchar *sig, uintptr_t ud);
guint  nui_timeout(guint ms, uintptr_t ud);
guint  nui_idle(uintptr_t ud);
guint  nui_watch(int fd, unsigned cond, uintptr_t ud);
void   nui_allocation(GtkWidget *w, int out[4]); /* только с NUI_GUI_TEST */

void nuiSignal(void *obj, uintptr_t ud);
int  nuiSignalRet(void *obj, void *arg, uintptr_t ud);
int  nuiSource(uintptr_t ud);
int  nuiWatch(int fd, unsigned cond, uintptr_t ud);

#endif
