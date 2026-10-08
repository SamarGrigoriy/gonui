package drv

/*
#include "gtk3_linux.h"
*/
import "C"

import "nui/internal/core"

// adopt — нормализация каждого созданного виджета (docs/driver-gtk3.md, §6):
//  1. hexpand/vexpand явно FALSE — флаг expand не всплывает к родителям (layout.md, §6);
//  2. destroy → EvDestroy;
//  3. виджет видим с рождения, окно — нет (ADR-14).
func adopt(w *C.GtkWidget, id uint32, show bool) H {
	C.gtk_widget_set_hexpand(w, 0)
	C.gtk_widget_set_vexpand(w, 0)
	h := H(w)
	connect(h, "destroy", id, core.EvDestroy)
	if show {
		C.gtk_widget_show(w)
	}
	return h
}

func Show(h H)    { C.gtk_widget_show(widget(h)) }
func Hide(h H)    { C.gtk_widget_hide(widget(h)) }
func Destroy(h H) { C.gtk_widget_destroy(widget(h)) }

func SetEnabled(h H, on bool) { C.gtk_widget_set_sensitive(widget(h), b2i(on)) }
func Enabled(h H) bool        { return C.gtk_widget_get_sensitive(widget(h)) != 0 }
func SetVisible(h H, on bool) { C.gtk_widget_set_visible(widget(h), b2i(on)) }
func Visible(h H) bool        { return C.gtk_widget_get_visible(widget(h)) != 0 }

// SetTooltip: пустая строка убирает подсказку.
func SetTooltip(h H, s string) {
	if s == "" {
		C.gtk_widget_set_tooltip_text(widget(h), nil)
		return
	}
	C.gtk_widget_set_tooltip_text(widget(h), cstr(s))
}

// gtkAlign переводит nui.Align (Fill 0, Start 1, Center 2, End 3) в GtkAlign.
func gtkAlign(a uint8) C.GtkAlign {
	switch a {
	case 1:
		return C.NUI_GTK_ALIGN_START
	case 2:
		return C.NUI_GTK_ALIGN_CENTER
	case 3:
		return C.NUI_GTK_ALIGN_END
	}
	return C.NUI_GTK_ALIGN_FILL
}

func SetAlign(h H, hz, v uint8) {
	C.gtk_widget_set_halign(widget(h), gtkAlign(hz))
	C.gtk_widget_set_valign(widget(h), gtkAlign(v))
}

// SetMinSize: -1 — не задавать по этой оси.
func SetMinSize(h H, w, ht int) { C.gtk_widget_set_size_request(widget(h), C.gint(w), C.gint(ht)) }

// SetExpand — для Grid.ExpandCol/ExpandRow (фаза 2).
func SetExpand(h H, hz, v bool) {
	C.gtk_widget_set_hexpand(widget(h), b2i(hz))
	C.gtk_widget_set_vexpand(widget(h), b2i(v))
}

// SetPadding — Box, Grid, Window: border-width контейнера.
func SetPadding(h H, px int) {
	if px < 0 {
		px = 0
	}
	C.gtk_container_set_border_width((*C.GtkContainer)(h), C.guint(px))
}

func Focus(h H) { C.gtk_widget_grab_focus(widget(h)) }
