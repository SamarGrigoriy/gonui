package nui

import (
	"nui/internal/core"
	"nui/internal/drv"
)

// Box — ряд виджетов: сверху вниз (VBox) или слева направо (HBox). Семантика —
// layout.md, §3: каждый получает естественный размер, свободное место делят поровну
// виджеты, помеченные Expand, и вставки Stretch; если таких нет — место остаётся в конце.
type Box struct{ widget }

// VBox — виджеты сверху вниз.
func VBox(items ...Widget) *Box { return newBox(true, items) }

// HBox — виджеты слева направо.
func HBox(items ...Widget) *Box { return newBox(false, items) }

func newBox(vertical bool, items []Widget) *Box {
	checkCreate()
	b := &Box{}
	b.id = core.Register((*boxEv)(b))
	b.h = drv.BoxNew(b.id, vertical)
	return b.Add(items...)
}

// Add добавляет виджеты в конец ряда. Виджет вставляется ровно в один контейнер.
func (b *Box) Add(items ...Widget) *Box {
	if b.dead() {
		return b
	}
	for _, it := range items {
		if it == nil {
			panic("nui: вставка nil вместо виджета")
		}
		w := it.base()
		if w.dead() {
			continue
		}
		w.place()
		drv.BoxPack(b.h, w.h, w.expand)
	}
	return b
}

// Spacing — расстояние между детьми, по умолчанию 6.
func (b *Box) Spacing(px int) *Box {
	if !b.dead() {
		drv.BoxSetSpacing(b.h, px)
	}
	return b
}

// Padding — отступ вокруг, по умолчанию 0.
func (b *Box) Padding(px int) *Box {
	if !b.dead() {
		drv.SetPadding(b.h, px)
	}
	return b
}

type boxEv Box

func (b *boxEv) Event(k core.Kind, _ uintptr) uintptr {
	if k == core.EvDestroy {
		b.destroyed()
	}
	return 0
}

// Expand помечает w: он забирает свободное место по главной оси Box.
// Вызывается до вставки: VBox(a, Expand(list), b). Иначе — паника.
func Expand(w Widget) Widget {
	b := w.base()
	if b.placed {
		panic("nui: Expand вызывается до вставки виджета в контейнер")
	}
	b.expand = true
	return w
}

// Stretch — пустая растягивающаяся вставка: прижимает соседей к краям.
func Stretch() Widget {
	checkCreate()
	s := &spacer{}
	s.id = core.Register((*spacerEv)(s))
	s.h = drv.SpacerNew(s.id)
	s.expand = true
	return s
}

type spacer struct{ widget }

type spacerEv spacer

func (s *spacerEv) Event(k core.Kind, _ uintptr) uintptr {
	if k == core.EvDestroy {
		s.destroyed()
	}
	return 0
}
