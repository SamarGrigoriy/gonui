package drv

/*
#include "win32_windows.h"
*/
import "C"

// Show, Hide — окна верхнего уровня (у остальных виджетов — SetVisible).
func Show(h H) { WindowShow(h) }
func Hide(h H) { C.ShowWindow(nodes[h].hwnd, C.NUI_SW_HIDE) }

// Destroy уничтожает виджет и его поддерево (EvDestroy каждому узлу); окно — DestroyWindow,
// и тогда EvDestroy приходят из WM_DESTROY.
func Destroy(h H) {
	n := get(h)
	if n == nil {
		return
	}
	if n.kind == kWindow {
		C.DestroyWindow(n.hwnd)
		return
	}
	detach(h)
	destroyTree(h, true)
}

// SetEnabled: как в GTK, выключенный контейнер выключает всё поддерево, а свой флаг
// каждого виджета сохраняется.
func SetEnabled(h H, on bool) {
	n := nodes[h]
	n.disabled = !on
	applyEnabled(h, effectiveDisabled(h))
}

func applyEnabled(h H, disabled bool) {
	n := nodes[h]
	disabled = disabled || n.disabled
	if n.hwnd != nil {
		C.EnableWindow(n.hwnd, b2i(!disabled))
	}
	for _, k := range n.kids {
		applyEnabled(k, disabled)
	}
}

func Enabled(h H) bool { return !nodes[h].disabled }

func SetVisible(h H, on bool) {
	n := nodes[h]
	if n.kind == kWindow {
		if on {
			WindowShow(h)
		} else {
			Hide(h)
		}
		return
	}
	n.hidden = !on
	invalidate(n.parent)
	if n.parent == Nil && n.hwnd != nil && !on {
		C.ShowWindow(n.hwnd, C.NUI_SW_HIDE)
	}
}

func Visible(h H) bool {
	n := nodes[h]
	if n.kind == kWindow {
		return C.IsWindowVisible(n.hwnd) != 0
	}
	return !n.hidden
}

// SetTooltip — всплывающая подсказка (tooltips_class32); "" убирает её. У Box и Grid
// подсказка видна над детьми без своей.
func SetTooltip(h H, s string) {
	nodes[h].tooltip = osText(s)
	applyTooltipTree(h)
}

func SetAlign(h H, hz, v uint8) {
	n := nodes[h]
	n.lay.HAlign, n.lay.VAlign = hz, v
	invalidate(n.parent)
}

func SetMinSize(h H, w, ht int) {
	n := nodes[h]
	n.minW, n.minH = w, ht
	invalidate(h)
}

func SetExpand(h H, hz, v bool) {
	n := nodes[h]
	n.expandH, n.expandV = hz, v
	invalidate(n.parent)
}

func SetPadding(h H, px int) {
	n := nodes[h]
	n.padding = max(px, 0)
	invalidate(h)
}

func Focus(h H) {
	if n := get(h); n != nil && n.hwnd != nil {
		C.SetFocus(n.hwnd)
	}
}
