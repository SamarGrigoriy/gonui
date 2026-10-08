package nui

import (
	"nui/internal/core"
	"nui/internal/drv"
)

// Label — метка: текст как есть, без разметки, выровненный по началу.
type Label struct{ widget }

// NewLabel создаёт метку с текстом.
func NewLabel(text string) *Label {
	checkCreate()
	l := &Label{}
	l.id = core.Register((*labelEv)(l))
	l.h = drv.LabelNew(l.id, text)
	return l
}

// SetText задаёт текст метки.
func (l *Label) SetText(s string) {
	if !l.dead() {
		drv.LabelSetText(l.h, s)
	}
}

// Text — текст метки.
func (l *Label) Text() string {
	if l.dead() {
		return ""
	}
	return drv.LabelText(l.h)
}

type labelEv Label

func (l *labelEv) Event(k core.Kind, _ uintptr) uintptr {
	if k == core.EvDestroy {
		l.destroyed()
	}
	return 0
}
