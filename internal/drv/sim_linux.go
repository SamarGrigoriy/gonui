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
