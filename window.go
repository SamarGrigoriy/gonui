package nui

import (
	"nui/internal/core"
	"nui/internal/drv"
)

// Window — окно верхнего уровня. Не Widget: в контейнер его не вставить.
type Window struct {
	h       drv.H
	id      uint32
	onClose func() bool
}

// NewWindow создаёт окно — скрытым, с отступом 12, по центру экрана (на Wayland
// положение выбирает композитор), с размером по содержимому. child может быть nil.
func NewWindow(title string, child Widget) *Window {
	checkCreate()
	w := &Window{}
	w.id = core.Register((*windowEv)(w))
	w.h = drv.WindowNew(w.id, title)
	wins = append(wins, w)
	if child != nil {
		w.SetChild(child)
	}
	return w
}

func (w *Window) dead() bool {
	core.CheckThread()
	if w.id != 0 {
		return false
	}
	if core.Debug {
		println("nui: вызов метода у уничтоженного окна")
	}
	return true
}

// Alive — false после уничтожения окна.
func (w *Window) Alive() bool { return w.id != 0 }

// SetTitle задаёт заголовок окна.
func (w *Window) SetTitle(s string) {
	if !w.dead() {
		drv.WindowSetTitle(w.h, s)
	}
}

// Title — заголовок окна.
func (w *Window) Title() string {
	if w.dead() {
		return ""
	}
	return drv.WindowTitle(w.h)
}

// SetChild заменяет содержимое окна; старый ребёнок уничтожается. nil — только убрать.
func (w *Window) SetChild(c Widget) {
	if w.dead() {
		return
	}
	ch := drv.Nil
	if c != nil {
		b := c.base()
		if b.dead() {
			return
		}
		b.place()
		ch = b.h
	}
	drv.WindowSetChild(w.h, ch)
}

// Show показывает окно, а если оно уже показано — поднимает наверх.
func (w *Window) Show() {
	if !w.dead() {
		drv.WindowShow(w.h)
	}
}

// Hide скрывает окно, не уничтожая его. Скрытое окно не удерживает программу:
// когда закроется последнее видимое окно, Run завершится (ADR-15).
func (w *Window) Hide() {
	if !w.dead() {
		drv.Hide(w.h)
	}
}

// Close закрывает и уничтожает окно из кода, без вызова OnClose (ADR-14).
func (w *Window) Close() {
	if !w.dead() {
		drv.Destroy(w.h)
	}
}

// OnClose: пользователь закрывает окно (крестик, Alt+F4). fn возвращает false —
// окно остаётся. Без обработчика окно закрывается. nil снимает обработчик.
func (w *Window) OnClose(fn func() bool) { w.onClose = fn }

// Focus передаёт фокус окну.
func (w *Window) Focus() {
	if !w.dead() {
		drv.Focus(w.h)
	}
}

type windowEv Window

func (w *windowEv) Event(k core.Kind, _ uintptr) uintptr {
	switch k {
	case core.EvClose:
		if w.onClose != nil && !w.onClose() {
			return 1 // окно остаётся
		}
	case core.EvDestroy:
		core.Release(w.id)
		w.id = 0
		w.h = drv.Nil
		windowGone((*Window)(w))
	}
	return 0
}

// windowGone — автовыход (ADR-15): окно уничтожено, и видимых окон не осталось.
// Скрытые окна программу не держат; Hide сам выход не вызывает.
func windowGone(w *Window) {
	for i, o := range wins {
		if o == w {
			wins = append(wins[:i], wins[i+1:]...)
			break
		}
	}
	for _, o := range wins {
		if drv.Visible(o.h) {
			return
		}
	}
	Quit()
}
