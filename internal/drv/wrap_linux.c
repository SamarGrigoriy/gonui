/* wrap_linux.c — то, что Go не может вызвать сам: макрос g_signal_connect,
   функции с переменным числом аргументов, структуры на стеке, указатели на колбэки.
   Трамплины приводятся к типам колбэков GLib: по ABI uintptr_t и gpointer
   передаются одинаково (на том же держится макрос G_CALLBACK). */
#include "gtk3_linux.h"

gulong nui_connect(gpointer obj, const gchar *sig, uintptr_t ud) {
	return g_signal_connect_data(obj, sig, (GCallback)nuiSignal, (gpointer)ud, 0, 0);
}

gulong nui_connect_ret(gpointer obj, const gchar *sig, uintptr_t ud) {
	return g_signal_connect_data(obj, sig, (GCallback)nuiSignalRet, (gpointer)ud, 0, 0);
}

guint nui_timeout(guint ms, uintptr_t ud) {
	return g_timeout_add_full(NUI_G_PRIORITY_DEFAULT, ms, (GSourceFunc)nuiSource, (gpointer)ud, 0);
}

/* Потокобезопасна: GLib разрешает g_idle_add_full из любого потока. */
guint nui_idle(uintptr_t ud) {
	return g_idle_add_full(NUI_G_PRIORITY_DEFAULT, (GSourceFunc)nuiSource, (gpointer)ud, 0);
}

/* HUP, ERR и NVAL подписываются всегда, как у poll (api.md, §3). */
guint nui_watch(int fd, unsigned cond, uintptr_t ud) {
	cond |= NUI_G_IO_HUP | NUI_G_IO_ERR | NUI_G_IO_NVAL;
	return g_unix_fd_add_full(NUI_G_PRIORITY_DEFAULT, fd, (GIOCondition)cond,
	                          (GUnixFDSourceFunc)nuiWatch, (gpointer)ud, 0);
}

/* Жирный шрифт и цвет метки — атрибуты Pango. Состояние хранится в самой метке
   (g_object_set_data, число, а не указатель Go), поэтому у nui.Label нет лишних полей
   и приложения без оформления не растут ни на байт. Упаковка:
   биты 0–23 — цвет 0xRRGGBB, бит 24 — без цвета (цвет темы), бит 25 — жирный,
   бит 26 — значение задано. color == 1<<24 (nui.NoColor) — цвет темы. */
#define NUI_STYLE_NOCOLOR (1u << 24)
#define NUI_STYLE_BOLD    (1u << 25)
#define NUI_STYLE_SET     (1u << 26)
void nui_label_style(GtkLabel *l, int set_bold, int bold, int set_color, unsigned color) {
	unsigned st = (unsigned)(uintptr_t)g_object_get_data((GObject *)l, "nui-style");
	if (!st)
		st = NUI_STYLE_SET | NUI_STYLE_NOCOLOR;
	if (set_bold)
		st = bold ? (st | NUI_STYLE_BOLD) : (st & ~NUI_STYLE_BOLD);
	if (set_color)
		st = (st & (NUI_STYLE_SET | NUI_STYLE_BOLD)) | (color & (NUI_STYLE_NOCOLOR | 0xffffffu));
	g_object_set_data((GObject *)l, "nui-style", (gpointer)(uintptr_t)st);

	PangoAttrList *a = pango_attr_list_new();
	pango_attr_list_insert(a, pango_attr_weight_new((st & NUI_STYLE_BOLD) ? NUI_PANGO_WEIGHT_BOLD : NUI_PANGO_WEIGHT_NORMAL));
	if (!(st & NUI_STYLE_NOCOLOR)) {
		unsigned r = (st >> 16) & 255, g = (st >> 8) & 255, b = st & 255;
		pango_attr_list_insert(a, pango_attr_foreground_new(r * 257, g * 257, b * 257));
	}
	gtk_label_set_attributes(l, a);
	pango_attr_list_unref(a);
}

/* Весь текст буфера. Строку освобождает вызывающий (g_free). GtkTextIter — только на стеке C. */
char *nui_text_get(GtkTextBuffer *b) {
	NuiTextIter s, e;
	gtk_text_buffer_get_start_iter(b, (GtkTextIter *)&s);
	gtk_text_buffer_get_end_iter(b, (GtkTextIter *)&e);
	return gtk_text_buffer_get_text(b, (GtkTextIter *)&s, (GtkTextIter *)&e, 0);
}

/* Дописать в конец; при max_lines > 0 удалить старые строки сверх max_lines; при autoscroll —
   прокрутить в конец. GTK считает пустую строку после завершающего \n отдельной строкой,
   поэтому она не учитывается. Прокрутка — через свою метку "nui-end": курсор и выделение
   пользователя не трогаются (place_cursor не используется). */
void nui_text_append(GtkTextView *v, const gchar *s, int len, int autoscroll, int max_lines) {
	GtkTextBuffer *b = gtk_text_view_get_buffer(v);
	NuiTextIter end;
	gtk_text_buffer_get_end_iter(b, (GtkTextIter *)&end);
	gtk_text_buffer_insert(b, (GtkTextIter *)&end, s, len);
	if (max_lines > 0) {
		gtk_text_buffer_get_end_iter(b, (GtkTextIter *)&end);
		int n = gtk_text_buffer_get_line_count(b) - (gtk_text_iter_starts_line((GtkTextIter *)&end) ? 1 : 0);
		if (n > max_lines) {
			NuiTextIter start, cut;
			gtk_text_buffer_get_start_iter(b, (GtkTextIter *)&start);
			gtk_text_buffer_get_iter_at_line(b, (GtkTextIter *)&cut, n - max_lines);
			gtk_text_buffer_delete(b, (GtkTextIter *)&start, (GtkTextIter *)&cut);
		}
	}
	if (autoscroll) {
		GtkTextMark *m = gtk_text_buffer_get_mark(b, "nui-end");
		gtk_text_buffer_get_end_iter(b, (GtkTextIter *)&end);
		if (m)
			gtk_text_buffer_move_mark(b, m, (GtkTextIter *)&end);
		else
			m = gtk_text_buffer_create_mark(b, "nui-end", (GtkTextIter *)&end, 0);
		gtk_text_view_scroll_mark_onscreen(v, m);
	}
}

/* Модальное сообщение: title — крупный заголовок, text — пояснение под ним.
   Внутри gtk_dialog_run крутится вложенный цикл: таймеры, Watch и Do продолжают работать.
   Своя ссылка на диалог: если родитель уничтожится, пока диалог открыт (DESTROY_WITH_PARENT),
   gtk_dialog_run вернётся, а объект ещё будет жив — destroy и unref безопасны. */
int nui_message(GtkWindow *parent, int type, int buttons, const gchar *title, const gchar *text) {
	GtkWidget *d = gtk_message_dialog_new(parent, NUI_GTK_DIALOG_MODAL | NUI_GTK_DIALOG_DESTROY_WITH_PARENT,
	                                      type, buttons, "%s", title);
	if (text && *text)
		gtk_message_dialog_format_secondary_text((GtkMessageDialog *)d, "%s", text);
	g_object_ref(d);
	int r = gtk_dialog_run((GtkDialog *)d);
	gtk_widget_destroy(d);
	g_object_unref(d);
	return r;
}

/* Ответ файлового менеджера на ShowItems: при ошибке (службы нет и т. п.) — открыть папку. */
static void nui_show_items_done(GObject *src, GAsyncResult *res, gpointer dir_uri) {
	GError *err = 0;
	GVariant *v = g_dbus_connection_call_finish((GDBusConnection *)src, res, &err);
	if (v) {
		g_variant_unref(v);
	} else {
		g_error_free(err);
		gtk_show_uri_on_window(0, (const char *)dir_uri, 0, 0);
	}
	g_free(dir_uri);
	g_object_unref(src);
}

/* Выделить файл в файловом менеджере: D-Bus org.freedesktop.FileManager1.ShowItems, асинхронно —
   интерфейс не ждёт. Таймаут D-Bus по умолчанию: при коротком файловый менеджер, запускавшийся
   дольше, всё равно выполнил бы ShowItems, и вместе с запасным путём открылись бы два окна.
   Нет сессии D-Bus — сразу открыть папку. 1 — запрос отправлен или папка открыта. */
int nui_show_items(const gchar *uri, const gchar *dir_uri) {
	GDBusConnection *c = g_bus_get_sync(NUI_G_BUS_TYPE_SESSION, 0, 0);
	if (!c)
		return gtk_show_uri_on_window(0, dir_uri, 0, 0);
	const gchar *uris[2] = {uri, 0};
	g_dbus_connection_call(c, "org.freedesktop.FileManager1", "/org/freedesktop/FileManager1",
	                       "org.freedesktop.FileManager1", "ShowItems", g_variant_new("(^ass)", uris, ""),
	                       0, NUI_G_DBUS_CALL_FLAGS_NONE, -1, 0, nui_show_items_done, g_strdup(dir_uri));
	return 1;
}

#ifdef NUI_GUI_TEST
/* Тестовый помощник: GtkAllocation живёт на стеке C, Go к нему не прикасается. */
void nui_allocation(GtkWidget *w, int out[4]) {
	GtkAllocation a;
	gtk_widget_get_allocation(w, &a);
	out[0] = a.x;
	out[1] = a.y;
	out[2] = a.width;
	out[3] = a.height;
}

/* Минимальный и естественный размер: ширина (out[0], out[1]), высота (out[2], out[3]). */
void nui_preferred(GtkWidget *w, int out[4]) {
	gtk_widget_get_preferred_width(w, &out[0], &out[1]);
	gtk_widget_get_preferred_height(w, &out[2], &out[3]);
}

/* Положение виджета в его окне верхнего уровня — сквозь GtkViewport, где allocation
   отсчитывается от собственного GdkWindow, — и начало этого окна на экране. */
void nui_window_pos(GtkWidget *w, int out[4]) {
	GtkWidget *top = gtk_widget_get_toplevel(w);
	GdkWindow *win = gtk_widget_get_window(top);
	out[0] = out[1] = out[2] = out[3] = 0;
	gtk_widget_translate_coordinates(w, top, 0, 0, &out[0], &out[1]);
	if (win)
		gdk_window_get_origin(win, &out[2], &out[3]);
}

/* Видимый диалог среди окон верхнего уровня: chooser — файловый (GtkFileChooserNative
   без портала показывает GtkFileChooserDialog), иначе — любой другой GtkDialog. */
static GtkWidget *nui_find_dialog(int chooser) {
	GList *all = gtk_window_list_toplevels(), *it;
	GtkWidget *found = 0;
	for (it = all; it; it = it->next) {
		GtkWidget *w = it->data;
		if (!gtk_widget_get_visible(w) || !g_type_check_instance_is_a((GTypeInstance *)w, gtk_dialog_get_type()))
			continue;
		if (!!g_type_check_instance_is_a((GTypeInstance *)w, gtk_file_chooser_get_type()) == !!chooser)
			found = w;
	}
	g_list_free(all);
	return found;
}

/* Открыт ли сейчас диалог (ничего в нём не нажимая). */
int nui_dialog_open(int chooser) { return nui_find_dialog(chooser) != 0; }

/* Ответить в открытом диалоге, как будто нажали кнопку. 0 — диалога нет. */
int nui_sim_dialog_response(int chooser, int response) {
	GtkWidget *d = nui_find_dialog(chooser);
	if (!d)
		return 0;
	gtk_dialog_response((GtkDialog *)d, response);
	return 1;
}

/* Выбрать путь в открытом файловом диалоге (каталог загружается асинхронно — дайте ему время). */
int nui_sim_file_chooser_set(const gchar *path) {
	GtkWidget *d = nui_find_dialog(1);
	if (!d)
		return 0;
	return gtk_file_chooser_set_filename((GtkFileChooser *)d, path);
}
#endif
