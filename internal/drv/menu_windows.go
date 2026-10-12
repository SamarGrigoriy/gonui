package drv

/*
#include "win32_windows.h"
*/
import "C"

import (
	"unsafe"

	"nui/internal/core"
)

// Меню окна (driver-win32.md, §5): HMENU строки меню в неклиентской области окна,
// подменю — CreatePopupMenu. Пункты — узлы kMenuItem; выбор приходит окну WM_COMMAND
// с командой пункта (menuCmds). Сочетания клавиш — текст после табуляции, срабатывают
// они в nui (как OnKey).

const firstMenuCmd = 1000 // ниже — IDOK, IDCANCEL и прочие команды диалоговой логики

var (
	menuCmds = map[uint16]H{} // команда → пункт
	freeCmds []uint16
	nextCmd  uint16 = firstMenuCmd
)

func allocCmd() uint16 {
	if n := len(freeCmds); n > 0 {
		c := freeCmds[n-1]
		freeCmds = freeCmds[:n-1]
		return c
	}
	nextCmd++
	return nextCmd - 1
}

// menuNode — новый узел меню окна win.
func menuNode(win H, kind uint8, id uint32) (H, *node) {
	h, n := newNode(kind, id)
	n.win = win
	w := nodes[win]
	w.menuNodes = append(w.menuNodes, h)
	return h, n
}

// MenuBar — новая пустая строка меню окна; прежняя уничтожается вместе с пунктами
// (на экране — после MenuDone).
func MenuBar(win H) H {
	w := nodes[win]
	C.nui_client_size(w.hwnd, &w.menuKeep[0]) // до того, как прежняя строка снята с окна
	removeBar(win)
	h, n := menuNode(win, kMenu, 0)
	n.hmenu = C.CreateMenu()
	nodes[win].menuBar = h
	return h
}

// MenuBarRemove убирает строку меню; клиентская область окна сохраняет размер.
func MenuBarRemove(win H) {
	w := nodes[win]
	C.nui_client_size(w.hwnd, &w.menuKeep[0])
	removeBar(win)
	keepClient(win)
}

// removeBar уничтожает узлы и HMENU прежней строки меню.
func removeBar(win H) {
	w := nodes[win]
	bar := get(w.menuBar)
	if bar == nil {
		return
	}
	old := bar.hmenu
	dropMenuNodes(w)
	if C.GetMenu(w.hwnd) == old {
		C.SetMenu(w.hwnd, nil)
	}
	C.DestroyMenu(old) // вместе с подменю
}

// dropMenuNodes — EvDestroy пунктам меню окна; команды и узлы — обратно (и в destroyTree окна).
func dropMenuNodes(w *node) {
	for _, h := range w.menuNodes {
		n := get(h)
		if n == nil {
			continue
		}
		if n.id != 0 {
			core.Dispatch(core.UD(n.id, core.EvDestroy), 0)
		}
		if n.cmd != 0 {
			delete(menuCmds, n.cmd)
			freeCmds = append(freeCmds, n.cmd)
		}
		nodes[h] = nil
		freeList = append(freeList, h)
	}
	w.menuNodes, w.menuBar = nil, Nil
}

// MenuSub — пункт с подменю в parent: item — пункт (по позиции), menu — подменю.
func MenuSub(parent H, id uint32, title string) (item, menu H) {
	p := nodes[parent]
	popup := C.CreatePopupMenu()
	pos := int(C.GetMenuItemCount(p.hmenu))
	C.AppendMenuW(p.hmenu, C.NUI_MF_POPUP, C.UINT_PTR(uintptr(unsafe.Pointer(popup))), wstr(amp(title)))
	item, it := menuNode(p.win, kMenuItem, id)
	it.hmenu, it.pos = p.hmenu, pos
	menu, m := menuNode(p.win, kMenu, 0)
	m.hmenu = popup
	return item, menu
}

// MenuItemNew — пункт с командой; check — с флажком (его переключает menuCommand).
func MenuItemNew(menu H, id uint32, check bool, text string, key uint32, mods uint8) H {
	p := nodes[menu]
	h, n := menuNode(p.win, kMenuItem, id)
	n.hmenu, n.cmd, n.editable = p.hmenu, allocCmd(), check
	menuCmds[n.cmd] = h
	C.AppendMenuW(p.hmenu, C.NUI_MF_STRING, C.UINT_PTR(n.cmd), wstr(menuText(text, key, mods)))
	return h
}

// menuText — текст пункта: & удвоен (иначе это мнемоника), сочетание — после табуляции,
// Windows выравнивает его по правому краю.
func menuText(text string, key uint32, mods uint8) string {
	s := amp(text)
	if key != 0 {
		s += "\t" + KeyLabel(key, mods)
	}
	return s
}

func MenuSepNew(menu H) { C.AppendMenuW(nodes[menu].hmenu, C.NUI_MF_SEPARATOR, 0, nil) }

// MenuDone ставит строку меню окну; клиентская область сохраняет размер.
func MenuDone(win H) {
	w := nodes[win]
	C.SetMenu(w.hwnd, nodes[w.menuBar].hmenu)
	C.DrawMenuBar(w.hwnd)
	keepClient(win)
}

// keepClient возвращает показанному окну клиентскую область, запомненную до смены меню:
// размер содержимого прежний, рамка растёт или уменьшается на строку меню.
func keepClient(win H) {
	w := nodes[win]
	if !w.sized {
		return // первый показ посчитает рамку уже с меню (nui_window_frame)
	}
	var r [4]C.int
	C.nui_window_frame(w.hwnd, w.menuKeep[0], w.menuKeep[1], 0, &r[0])
	C.SetWindowPos(w.hwnd, nil, r[0], r[1], r[2], r[3], C.NUI_SWP_NOZORDER|C.NUI_SWP_NOACTIVATE)
}

// menuCommand — WM_COMMAND от меню: флажок переключается, затем EvClick.
func menuCommand(cmd uint16) bool {
	h := menuCmds[cmd]
	n := get(h)
	if n == nil {
		return false
	}
	if n.editable {
		MenuItemSetChecked(h, !MenuItemChecked(h))
	}
	emit(n, core.EvClick)
	return true
}

// menuRef — как обратиться к пункту: по команде или (пункт с подменю) по позиции.
func menuRef(n *node) (C.UINT, C.UINT) {
	if n.cmd == 0 {
		return C.UINT(n.pos), C.NUI_MF_BYPOSITION
	}
	return C.UINT(n.cmd), 0
}

func MenuItemSetEnabled(h H, on bool) {
	n := nodes[h]
	item, by := menuRef(n)
	flag := C.UINT(0)
	if !on {
		flag = C.NUI_MF_GRAYED
	}
	C.EnableMenuItem(n.hmenu, item, by|flag)
	C.DrawMenuBar(nodes[n.win].hwnd)
}

func MenuItemSetChecked(h H, on bool) {
	n := nodes[h]
	flag := C.UINT(0)
	if on {
		flag = C.NUI_MF_CHECKED
	}
	C.CheckMenuItem(n.hmenu, C.UINT(n.cmd), flag)
}

func MenuItemChecked(h H) bool {
	n := nodes[h]
	return C.GetMenuState(n.hmenu, C.UINT(n.cmd), 0)&C.NUI_MF_CHECKED != 0
}

func MenuItemSetText(h H, text string, key uint32, mods uint8) {
	n := nodes[h]
	item, by := menuRef(n)
	C.nui_menu_set_text(n.hmenu, item, b2i(by != 0), wstr(menuText(text, key, mods)))
	C.DrawMenuBar(nodes[n.win].hwnd)
}

// WindowSetPadding — отступ окна: у Win32 меню вне клиентской области, ничего особого.
func WindowSetPadding(h H, px int) { SetPadding(h, px) }
