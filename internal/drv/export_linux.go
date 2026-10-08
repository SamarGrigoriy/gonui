package drv

// В файле с //export преамбула может содержать только объявления.

/*
#include "gtk3_linux.h"
*/
import "C"

import (
	"unsafe"

	"nui/internal/core"
)

// Четыре трамплина — по одному на сигнатуру колбэка (ADR-6). Все ведут в core.Dispatch.
// ud приходит числом, а не указателем: в Go не появляется «поддельных» указателей.

//export nuiSignal
func nuiSignal(obj unsafe.Pointer, ud C.uintptr_t) { core.Dispatch(uintptr(ud), 0) }

//export nuiSignalRet
func nuiSignalRet(obj, arg unsafe.Pointer, ud C.uintptr_t) C.int {
	return C.int(core.Dispatch(uintptr(ud), uintptr(arg))) // arg — указатель C (GdkEvent* и т. п.)
}

//export nuiSource
func nuiSource(ud C.uintptr_t) C.int { return C.int(core.Dispatch(uintptr(ud), 0)) }

//export nuiWatch
func nuiWatch(fd C.int, cond C.uint, ud C.uintptr_t) C.int {
	return C.int(core.Dispatch(uintptr(ud), uintptr(cond)))
}
