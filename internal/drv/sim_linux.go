//go:build gui

package drv

// Помощники интеграционных тестов: имитируют действия пользователя — вызывают GTK
// напрямую, мимо счётчика quiet, поэтому обработчики On* срабатывают.
// #cgo CFLAGS действует на все C-файлы пакета: тестовые C-помощники в wrap_linux.c
// попадают только в сборку с тегом gui.

/*
#cgo CFLAGS: -DNUI_GUI_TEST
#include "gtk3_linux.h"
*/
import "C"

// SimClick — как нажатие кнопки мышью.
func SimClick(h H) { C.gtk_button_clicked((*C.GtkButton)(h)) }

// SimClose — как крестик: delete-event приходит не сразу, а через idle-источник.
func SimClose(h H) { C.gtk_window_close((*C.GtkWindow)(h)) }

// Allocation — прямоугольник, который GTK выделил виджету при раскладке.
func Allocation(h H) (x, y, w, ht int) {
	var a [4]C.int
	C.nui_allocation(widget(h), &a[0])
	return int(a[0]), int(a[1]), int(a[2]), int(a[3])
}

// SimType — как ввод текста в поле: changed приходит один раз.
func SimType(h H, s string) { C.gtk_entry_set_text((*C.GtkEntry)(h), cstr(s)) }

// SimComboType — ввод в поле ComboEntry.
func SimComboType(h H, s string) { SimType(comboEntry(h), s) }

// SimEnter — как Enter: activate.
func SimEnter(h H) { C.gtk_widget_activate(widget(h)) }

// SimToggle — как щелчок по флажку.
func SimToggle(h H) {
	t := (*C.GtkToggleButton)(h)
	C.gtk_toggle_button_set_active(t, b2i(C.gtk_toggle_button_get_active(t) == 0))
}

// SimSelect — как выбор пункта в списке.
func SimSelect(h H, i int) { C.gtk_combo_box_set_active((*C.GtkComboBox)(h), C.gint(i)) }

// SimSlide — как перетаскивание ползунка.
func SimSlide(h H, v float64) { C.gtk_range_set_value((*C.GtkRange)(h), C.gdouble(v)) }

// SimEdit — как правка многострочного текста: на непустом буфере changed приходит
// дважды (удаление и вставка) — так ведёт себя GTK.
func SimEdit(view H, s string) { C.gtk_text_buffer_set_text(textBuffer(view), cstr(s), -1) }

// ComboEntryForTest — поле ввода ComboEntry (чтобы имитировать Enter в нём).
func ComboEntryForTest(h H) H { return comboEntry(h) }
