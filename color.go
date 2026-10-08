package nui

// Color — цвет 0xRRGGBB. Нужен только меткам: строки состояния (ADR-1).
type Color uint32

// RGB собирает цвет из компонент.
func RGB(r, g, b uint8) Color { return Color(r)<<16 | Color(g)<<8 | Color(b) }

// NoColor — цвет темы (по умолчанию).
const NoColor Color = 1 << 24
