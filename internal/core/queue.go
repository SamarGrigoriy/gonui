package core

import "sync"

// Очередь Do: единственное, что можно трогать из любого потока.
var (
	mu      sync.Mutex
	queue   []func()
	pending bool // idle-источник уже запланирован
	closed  bool // Run завершился
)

// Post ставит fn в очередь. true — очередь была пуста и цикл событий нужно разбудить
// (drv.Wake). Будим один раз на пачку, а не на каждый вызов.
func Post(fn func()) bool {
	mu.Lock()
	defer mu.Unlock()
	if closed {
		return false
	}
	queue = append(queue, fn)
	if pending {
		return false
	}
	pending = true
	return true
}

// Drain выполняет пачку, накопленную к моменту вызова. Только в UI-потоке.
// Post изнутри fn снова планирует idle, поэтому порядок сохраняется,
// а бесконечного цикла внутри одного Drain нет.
func Drain() {
	mu.Lock()
	q := queue
	queue, pending = nil, false
	mu.Unlock()
	for _, fn := range q {
		fn()
	}
}

// Close вызывается после завершения Run: дальнейшие Post игнорируются.
func Close() {
	mu.Lock()
	closed = true
	queue = nil
	mu.Unlock()
}
