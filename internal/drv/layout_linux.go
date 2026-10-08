package drv

/*
#include "gtk3_linux.h"
*/
import "C"

// BoxNew — GtkBox со стандартным промежутком 6 (layout.md, §2).
func BoxNew(id uint32, vertical bool) H {
	o := C.GtkOrientation(C.NUI_GTK_ORIENTATION_HORIZONTAL)
	if vertical {
		o = C.NUI_GTK_ORIENTATION_VERTICAL
	}
	return adopt(C.gtk_box_new(o, 6), id, true)
}

func BoxSetSpacing(h H, px int) {
	if px < 0 {
		px = 0
	}
	C.gtk_box_set_spacing((*C.GtkBox)(h), C.gint(px))
}

// BoxPack: ребёнок занимает свою ячейку целиком (fill = TRUE) и забирает свободное
// место, только если помечен Expand (layout.md, §6).
func BoxPack(h, child H, expand bool) {
	C.gtk_box_pack_start((*C.GtkBox)(h), widget(child), b2i(expand), 1, 0)
}

// SpacerNew — пустая вставка для Stretch: упаковывается с expand = TRUE.
func SpacerNew(id uint32) H {
	return adopt(C.gtk_box_new(C.NUI_GTK_ORIENTATION_HORIZONTAL, 0), id, true)
}
