//go:build gui

package drv

// Помощники интеграционных тестов на Windows (тот же набор, что sim_linux.go): имитируют
// действия пользователя — шлют контролам и их родителям те же сообщения, что Windows
// при вводе мышью и клавиатурой, мимо счётчика quiet, поэтому обработчики On* срабатывают.

/*
#cgo CFLAGS: -DNUI_GUI_TEST
#include "win32_windows.h"
*/
import "C"

import "unsafe"

// notifyParent — уведомление WM_COMMAND родителю контрола, как от пользователя.
func notifyParent(n *node, code uintptr) {
	send(C.GetParent(n.hwnd), C.NUI_WM_COMMAND, code<<16, uintptrOf(n.hwnd))
}

func uintptrOf(h C.HWND) uintptr { return uintptr(unsafe.Pointer(h)) }

// SimClick — как нажатие кнопки мышью: BN_CLICKED родителю.
func SimClick(h H) { notifyParent(nodes[h], C.NUI_BN_CLICKED) }

// SimClose — как крестик: WM_CLOSE приходит не сразу, а через очередь.
func SimClose(h H) { C.PostMessageW(nodes[h].hwnd, C.NUI_WM_CLOSE, 0, 0) }

// Allocation — место узла после раскладки: у окна — клиентская область, у виджета — Out
// движка, в координатах окна (как allocation в GTK).
func Allocation(h H) (x, y, w, ht int) {
	n := nodes[h]
	if n.kind == kWindow {
		var cs [2]C.int
		C.nui_client_size(n.hwnd, &cs[0])
		return 0, 0, int(cs[0]), int(cs[1])
	}
	o := n.lay.Out
	return o.X, o.Y, o.W, o.H
}

// SimType — как ввод текста в поле: EN_CHANGE приходит один раз.
func SimType(h H, s string) { C.SetWindowTextW(nodes[h].hwnd, wstr(s)) }

// SimComboType — ввод в поле ComboEntry: текст в дочерний EDIT комбобокса и, как при
// вводе с клавиатуры, CBN_EDITCHANGE родителю (на WM_SETTEXT комбобокс его не шлёт).
func SimComboType(h H, s string) {
	n := nodes[h]
	C.SetWindowTextW(C.GetWindow(n.hwnd, C.NUI_GW_CHILD), wstr(s))
	notifyParent(n, C.NUI_CBN_EDITCHANGE)
}

// SimEnter — как Enter: в поле с OnEnter — EvEnter сразу; иначе нажатие клавиши идёт
// через настоящий цикл (IsDialogMessage → кнопка по умолчанию), то есть позже.
func SimEnter(h H) {
	n := nodes[h]
	target := n.hwnd
	if n.kind == kCombo {
		target = C.GetWindow(n.hwnd, C.NUI_GW_CHILD)
	}
	if C.GetPropW(target, wstr("nuiEnter")) != nil {
		send(msg(), C.NUI_WM_ENTER, 0, uintptrOf(target))
		return
	}
	C.PostMessageW(target, C.NUI_WM_KEYDOWN, C.NUI_VK_RETURN, 0)
}

// SimToggle — как щелчок по флажку: состояние меняется, BN_CLICKED родителю.
func SimToggle(h H) {
	n := nodes[h]
	send(n.hwnd, C.NUI_BM_SETCHECK, uintptr(b2i(!CheckChecked(h))), 0)
	notifyParent(n, C.NUI_BN_CLICKED)
}

// SimSelect — как выбор пункта в списке: CBN_SELCHANGE родителю.
func SimSelect(h H, i int) {
	n := nodes[h]
	send(n.hwnd, C.NUI_CB_SETCURSEL, uintptr(i), 0)
	notifyParent(n, C.NUI_CBN_SELCHANGE)
}

// SimSlide — как перетаскивание ползунка: WM_HSCROLL родителю.
func SimSlide(h H, v int) {
	n := nodes[h]
	send(n.hwnd, C.NUI_TBM_SETPOS, 1, uintptr(v))
	send(C.GetParent(n.hwnd), C.NUI_WM_HSCROLL, 4 /* TB_THUMBPOSITION */, uintptrOf(n.hwnd))
}

// SimEdit — как правка многострочного текста: новый текст и EN_CHANGE родителю. На
// WM_SETTEXT многострочный EDIT EN_CHANGE не шлёт; пользовательская правка даёт одно
// уведомление и на непустом буфере (на GTK — два: удаление и вставка).
func SimEdit(view H, s string) {
	n := nodes[view]
	C.SetWindowTextW(n.hwnd, wstr(crlf(s)))
	notifyParent(n, C.NUI_EN_CHANGE)
}

// ComboEntryForTest — поле ComboEntry для имитации Enter в нём: на Windows SimEnter
// сам находит поле по узлу комбобокса.
func ComboEntryForTest(h H) H { return h }

// dialogButton — нажать кнопку id в открытом сообщении: TDM_CLICK_BUTTON у TaskDialog,
// WM_COMMAND у MessageBoxW.
func dialogButton(id uintptr) bool {
	d := C.nui_find_dialog()
	if d == nil || C.nui_dialog_kind() > 2 {
		return false
	}
	m := C.UINT(C.NUI_WM_COMMAND)
	if C.nui_task_dialog() != 0 {
		m = C.NUI_TDM_CLICK_BUTTON
	}
	C.PostMessageW(d, m, C.WPARAM(id), 0)
	return true
}

// SimAnswer — ответ в открытом сообщении: OK/«Да» (true) или «Нет» (false). false — диалога нет.
func SimAnswer(yes bool) bool {
	ask := C.nui_dialog_kind() == 2
	switch {
	case yes && ask:
		return dialogButton(C.NUI_IDYES)
	case ask:
		return dialogButton(C.NUI_IDNO)
	}
	return dialogButton(C.NUI_IDOK)
}

// SimOK — нажать OK в открытом сообщении.
func SimOK() bool { return dialogButton(C.NUI_IDOK) }

// SimEsc — закрыть открытое сообщение, как Esc (TDF_ALLOW_DIALOG_CANCELLATION).
func SimEsc() bool { return dialogButton(C.NUI_IDCANCEL) }

// SimChooserSet выбирает путь в открытом файловом диалоге или диалоге папки.
func SimChooserSet(path string) bool {
	d := C.nui_find_dialog()
	switch {
	case d == nil:
		return false
	case C.nui_dialog_kind() == 4: // SHBrowseForFolderW: BFFM_SETSELECTIONW
		sendStr(d, 0x400+103, 1, path)
		return true
	case C.nui_dialog_kind() == 3: // поле имени: cmb13 (проводник) или edt1
		e := C.GetDlgItem(d, 0x47C)
		if e == nil {
			e = C.GetDlgItem(d, 0x480)
		}
		if e == nil {
			return false
		}
		C.SetWindowTextW(e, wstr(path))
		return true
	}
	return false
}

// SimChooserDone — «Открыть/Сохранить/OK» (accept) или «Отмена» в файловом диалоге.
func SimChooserDone(accept bool) bool {
	d := C.nui_find_dialog()
	if d == nil || C.nui_dialog_kind() < 3 {
		return false
	}
	id := C.WPARAM(C.NUI_IDCANCEL)
	if accept {
		id = C.NUI_IDOK
	}
	C.PostMessageW(d, C.NUI_WM_COMMAND, id, 0)
	return true
}

// DialogOpen — открыт ли сейчас диалог: файловый (chooser) или сообщение.
func DialogOpen(chooser bool) bool {
	k := C.nui_dialog_kind()
	return C.nui_find_dialog() != nil && (k >= 3) == chooser
}
