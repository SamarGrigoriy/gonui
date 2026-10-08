package drv

/*
#include "gtk3_linux.h"
*/
import "C"

import "nui/internal/core"

// LabelNew: у GtkLabel по умолчанию xalign = 0.5; nui выравнивает текст по началу.
func LabelNew(id uint32, text string) H {
	w := C.gtk_label_new(cstr(text))
	C.gtk_label_set_xalign((*C.GtkLabel)(w), 0)
	return adopt(w, id, true)
}

func LabelSetText(h H, s string) { C.gtk_label_set_text((*C.GtkLabel)(h), cstr(s)) }
func LabelText(h H) string       { return gostr(C.gtk_label_get_text((*C.GtkLabel)(h))) }

// ButtonNew: clicked → EvClick. Подчёркивания в тексте не становятся мнемониками.
func ButtonNew(id uint32, text string) H {
	h := adopt(C.gtk_button_new_with_label(cstr(text)), id, true)
	connect(h, "clicked", id, core.EvClick)
	return h
}

func ButtonSetText(h H, s string) { C.gtk_button_set_label((*C.GtkButton)(h), cstr(s)) }
func ButtonText(h H) string       { return gostr(C.gtk_button_get_label((*C.GtkButton)(h))) }
