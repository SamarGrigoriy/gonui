package drv

/*
#include "gtk3_linux.h"
*/
import "C"

import "nui/internal/core"

// WindowNew создаёт окно: скрытое (ADR-14), по центру экрана, с отступом 12 (layout.md, §2).
// delete-event → EvClose с ответом, destroy → EvDestroy.
func WindowNew(id uint32, title string) H {
	w := C.gtk_window_new(C.NUI_GTK_WINDOW_TOPLEVEL)
	win := (*C.GtkWindow)(w)
	C.gtk_window_set_title(win, cstr(title))
	C.gtk_window_set_position(win, C.NUI_GTK_WIN_POS_CENTER)
	C.gtk_container_set_border_width((*C.GtkContainer)(w), 12)
	h := adopt(w, id, false)
	connectRet(h, "delete-event", id, core.EvClose)
	return h
}

func WindowSetTitle(h H, s string) { C.gtk_window_set_title((*C.GtkWindow)(h), cstr(s)) }
func WindowTitle(h H) string       { return gostr(C.gtk_window_get_title((*C.GtkWindow)(h))) }

// WindowSetChild заменяет содержимое окна. Старый ребёнок удаляется из контейнера,
// ссылок на него не остаётся, и GTK его уничтожает (destroy → EvDestroy).
// child == Nil — только удалить.
func WindowSetChild(h, child H) {
	if old := C.gtk_bin_get_child((*C.GtkBin)(h)); old != nil {
		C.gtk_container_remove((*C.GtkContainer)(h), old)
	}
	if child != Nil {
		C.gtk_container_add((*C.GtkContainer)(h), widget(child))
	}
}

// WindowShow показывает окно, а если оно уже показано — поднимает наверх.
func WindowShow(h H) {
	C.gtk_widget_show(widget(h))
	C.gtk_window_present((*C.GtkWindow)(h))
}
