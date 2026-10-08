package core

import (
	"sync"
	"testing"
)

// reset возвращает ядро в начальное состояние: тесты не должны зависеть друг от друга.
func reset() {
	objs = []Receiver{nil}
	free = nil
	waker = nil
	mu.Lock()
	queue, pending, closed = nil, false, false
	mu.Unlock()
}

type rec struct {
	kinds []Kind
	args  []uintptr
	ret   uintptr
}

func (r *rec) Event(k Kind, arg uintptr) uintptr {
	r.kinds = append(r.kinds, k)
	r.args = append(r.args, arg)
	return r.ret
}

func TestRegisterReusesIDsAndNeverReturnsZero(t *testing.T) {
	reset()
	a, b := &rec{}, &rec{}
	ida, idb := Register(a), Register(b)
	if ida == 0 || idb == 0 || ida == idb {
		t.Fatalf("id: %d, %d", ida, idb)
	}
	Release(ida)
	if Lookup(ida) != nil {
		t.Fatal("после Release объект всё ещё в реестре")
	}
	c := &rec{}
	if idc := Register(c); idc != ida {
		t.Fatalf("освобождённый id не переиспользован: %d вместо %d", idc, ida)
	}
	if Lookup(idb) != b {
		t.Fatal("Lookup вернул чужой объект")
	}
}

func TestReleaseIsIdempotentAndIgnoresBadIDs(t *testing.T) {
	reset()
	id := Register(&rec{})
	Release(id)
	Release(id) // повторный — ничего не делает, id в стеке свободных один раз
	Release(0)
	Release(1000)
	if len(free) != 1 {
		t.Fatalf("free = %v", free)
	}
	a, b := Register(&rec{}), Register(&rec{})
	if a == b {
		t.Fatal("один id выдан дважды")
	}
}

func TestLookupOutOfRange(t *testing.T) {
	reset()
	if Lookup(0) != nil || Lookup(5) != nil || Lookup(^uint32(0)) != nil {
		t.Fatal("Lookup вне диапазона должен давать nil")
	}
}

func TestUDDecodeRoundTrip(t *testing.T) {
	ids := []uint32{0, 1, 2, 31, 1<<27 - 1}
	kinds := []Kind{EvClick, EvChange, EvEnter, EvClose, EvDestroy, KTimer, KWatch, KDo, 31}
	for _, id := range ids {
		for _, k := range kinds {
			gid, gk := Decode(UD(id, k))
			if gid != id || gk != k {
				t.Fatalf("UD(%d, %d) → (%d, %d)", id, k, gid, gk)
			}
		}
	}
}

func TestDispatch(t *testing.T) {
	reset()
	r := &rec{ret: 1}
	id := Register(r)
	if got := Dispatch(UD(id, EvClose), 42); got != 1 {
		t.Fatalf("ответ %d, ожидался 1", got)
	}
	if len(r.kinds) != 1 || r.kinds[0] != EvClose || r.args[0] != 42 {
		t.Fatalf("событие дошло не так: %v %v", r.kinds, r.args)
	}
	if got := Dispatch(UD(id+100, EvClick), 0); got != 0 {
		t.Fatal("неизвестный id должен давать 0")
	}
	Release(id)
	if got := Dispatch(UD(id, EvClick), 0); got != 0 || len(r.kinds) != 1 {
		t.Fatal("освобождённый объект получил событие")
	}
}

func TestDispatchKDoDrains(t *testing.T) {
	reset()
	ran := 0
	Post(func() { ran++ })
	if got := Dispatch(UD(0, KDo), 0); got != 0 || ran != 1 {
		t.Fatalf("Dispatch(KDo): ответ %d, выполнено %d", got, ran)
	}
}

func TestPostWakesOncePerBatch(t *testing.T) {
	reset()
	got := []bool{Post(func() {}), Post(func() {}), Post(func() {})}
	if !got[0] || got[1] || got[2] {
		t.Fatalf("Post ×3 = %v, ожидалось true, false, false", got)
	}
	// Drain пачки из трёх заранее будит цикл (в очереди оставались функции): idle уже
	// запланирован, и Post будить не должен. Когда этот idle отработает — снова должен.
	wakes := 0
	SetWaker(func() { wakes++ })
	Drain()
	if wakes != 1 || Post(func() {}) {
		t.Fatalf("после Drain пачки: разбужено %d раз, Post будит повторно", wakes)
	}
	Drain() // разбуженный idle
	Drain()
	if !Post(func() {}) {
		t.Fatal("когда idle отработал и очередь пуста, первый Post снова должен будить цикл")
	}
}

func TestPostInsideDrainRunsInNextBatch(t *testing.T) {
	reset()
	wakes := 0
	SetWaker(func() { wakes++ })
	var order []int
	var woke bool
	Post(func() {
		order = append(order, 1)
		woke = Post(func() { order = append(order, 3) })
	})
	Post(func() { order = append(order, 2) })
	Drain()
	if len(order) != 2 || order[0] != 1 || order[1] != 2 {
		t.Fatalf("первая пачка: %v", order)
	}
	// Цикл должен быть разбужен для следующей пачки — либо самим Post, либо заранее Drain.
	if !woke && wakes == 0 {
		t.Fatal("после Post внутри Drain цикл никто не разбудил")
	}
	Drain()
	if len(order) != 3 || order[2] != 3 {
		t.Fatalf("вторая пачка: %v", order)
	}
}

// Функция из очереди открыла «модальный диалог» (вложенный цикл): остальные функции
// той же пачки выполняет вложенный Drain, не дожидаясь возврата. Порядок сохраняется.
func TestDrainNestedLoop(t *testing.T) {
	reset()
	nested := 0
	SetWaker(func() { nested++ })
	var order []int
	Post(func() {
		order = append(order, 1)
		if nested != 1 {
			t.Fatal("перед функцией, которая может заблокироваться, цикл должен быть разбужен")
		}
		Drain() // вложенный цикл: разбуженный idle выполняет остаток очереди
		order = append(order, 99)
	})
	Post(func() { order = append(order, 2) })
	Post(func() { order = append(order, 3) })
	Drain()
	want := []int{1, 2, 3, 99}
	if len(order) != len(want) {
		t.Fatalf("порядок %v, ожидался %v", order, want)
	}
	for i := range want {
		if order[i] != want[i] {
			t.Fatalf("порядок %v, ожидался %v", order, want)
		}
	}
}

func TestPostAfterClose(t *testing.T) {
	reset()
	Post(func() { t.Fatal("функция из очереди до Close выполнилась") })
	Close()
	if Post(func() { t.Fatal("функция после Close выполнилась") }) {
		t.Fatal("Post после Close должен давать false")
	}
	Drain()
}

// 100 горутин × 1000 Post: без гонок (-race), порядок внутри каждой горутины сохранён.
func TestPostConcurrentOrder(t *testing.T) {
	reset()
	const G, N = 100, 1000
	last := make([]int, G)
	for i := range last {
		last[i] = -1
	}
	var wg sync.WaitGroup
	for g := 0; g < G; g++ {
		wg.Add(1)
		go func(g int) {
			defer wg.Done()
			for i := 0; i < N; i++ {
				i := i
				Post(func() {
					if i != last[g]+1 {
						t.Errorf("горутина %d: %d после %d", g, i, last[g])
					}
					last[g] = i
				})
			}
		}(g)
	}
	done := make(chan struct{})
	go func() { wg.Wait(); close(done) }()
	for drained := false; !drained; {
		select {
		case <-done:
			drained = true
		default:
		}
		Drain() // «UI-поток»: только эта горутина выполняет функции
	}
	Drain()
	for g, v := range last {
		if v != N-1 {
			t.Fatalf("горутина %d: выполнено до %d из %d", g, v, N-1)
		}
	}
}

func TestItoa(t *testing.T) {
	cases := map[int]string{0: "0", 7: "7", 42: "42", -5: "-5", 1234567890: "1234567890", -9223372036854775808: "-9223372036854775808"}
	for n, want := range cases {
		if got := Itoa(n); got != want {
			t.Errorf("Itoa(%d) = %q, ожидалось %q", n, got, want)
		}
	}
}

func TestFixUTF8(t *testing.T) {
	cases := []struct{ in, want string }{
		{"", ""},
		{"Привет 🙂", "Привет 🙂"},
		{"a\xffb", "a�b"},
		{"\xc3", "�"},
		{"ok\xe2\x82", "ok��"},
		{"�", "�"}, // настоящий U+FFFD не трогаем
	}
	for _, c := range cases {
		if got := FixUTF8(c.in); got != c.want {
			t.Errorf("FixUTF8(%q) = %q, ожидалось %q", c.in, got, c.want)
		}
	}
}

func TestDir(t *testing.T) {
	cases := map[string]string{"/a/b": "/a", "/a": "/", "a": ".", "a/b/c.log": "a/b", "/": "/", "": "."}
	for in, want := range cases {
		if got := Dir(in); got != want {
			t.Errorf("Dir(%q) = %q, ожидалось %q", in, got, want)
		}
	}
}

func TestDebugHooksAreHarmlessWithoutUIThread(t *testing.T) {
	MarkUIThread(nil)
	CheckThread() // без зарегистрированной проверки — ничего не делает
	if Debug {
		MarkUIThread(func() bool { return false })
		defer MarkUIThread(nil)
		defer func() {
			if recover() == nil {
				t.Fatal("в отладочной сборке вызов не из UI-потока должен паниковать")
			}
		}()
		CheckThread()
	}
}
