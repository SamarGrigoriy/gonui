package drv

/*
#include "win32_windows.h"
*/
import "C"

import "unsafe"

// Диалоги и оболочка (driver-win32.md, §9). Модальные циклы диалогов диспетчеризуют
// сообщения служебного окна, поэтому Do и таймеры работают, пока диалог открыт.

func ownerOf(parent H) C.HWND {
	if n := get(parent); n != nil {
		return n.hwnd
	}
	return nil
}

// MessageBox — TaskDialog (comctl32 v6): крупный заголовок title, под ним text, заголовок
// окна — имя приложения. Без v6 — MessageBoxW. true — OK или «Да».
func MessageBox(parent H, kind uint8, title, text string) bool {
	return C.nui_message(ownerOf(parent), C.int(kind), wstr(appTitle), wstr(title), wstr(text)) != 0
}

// FileDialog — GetOpenFileNameW / GetSaveFileNameW / SHBrowseForFolderW.
func FileDialog(parent H, mode uint8, title, name string, f []Filter) (string, bool) {
	if mode == FileFolder {
		buf := make([]uint16, 260)
		if C.nui_folder_dialog(ownerOf(parent), wstr(title), wptr(buf)) == 0 {
			return "", false
		}
		return fromUTF16(zstr(buf)), true
	}
	buf := make([]uint16, 32768) // длинные пути Windows
	copy(buf[:len(buf)-1], utf16z(name))
	ok := C.nui_file_dialog(ownerOf(parent), b2i(mode == FileSave), wstr(title), filterSpec(f), wptr(buf),
		C.int(len(buf)))
	if ok == 0 {
		return "", false
	}
	return fromUTF16(zstr(buf)), true
}

// filterSpec — фильтры в виде "Логи\0*.log;*.txt\0\0"; без фильтров — nil (все файлы).
func filterSpec(f []Filter) *C.WCHAR {
	if len(f) == 0 {
		return nil
	}
	var b []uint16
	for _, x := range f {
		b = append(b, utf16z(x.Name)...)
		pat := ""
		for i, p := range x.Patterns {
			if i > 0 {
				pat += ";"
			}
			pat += p
		}
		if pat == "" {
			pat = "*.*"
		}
		b = append(b, utf16z(pat)...)
	}
	b = append(b, 0)
	return wptr(b)
}

// zstr — буфер до первого нуля.
func zstr(b []uint16) []uint16 {
	for i, c := range b {
		if c == 0 {
			return b[:i]
		}
	}
	return b
}

// OpenURI — ShellExecuteW «open»: результат больше 32 — успех.
func OpenURI(parent H, uri string) bool {
	r := C.ShellExecuteW(ownerOf(parent), wstr("open"), wstr(uri), nil, nil, C.NUI_SW_SHOWNORMAL)
	return uintptr(unsafe.Pointer(r)) > 32
}

// ShowInFolder — Проводник с выделенным файлом: explorer.exe /select,"путь".
func ShowInFolder(path string) bool {
	r := C.ShellExecuteW(nil, wstr("open"), wstr("explorer.exe"), wstr(`/select,"`+path+`"`), nil,
		C.NUI_SW_SHOWNORMAL)
	return uintptr(unsafe.Pointer(r)) > 32
}
