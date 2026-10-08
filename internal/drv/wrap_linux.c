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
#endif
