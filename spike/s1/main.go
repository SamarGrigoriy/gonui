package main

/*
#include "s1.h"
*/
import "C"

import (
	"os"
	"runtime"
	"syscall"
	"time"
)

// init выполняется в главном потоке: главная горутина остаётся на нём (runtime.md, §1).
func init() { runtime.LockOSThread() }

// Проверки сценария S1 (docs/roadmap.md).
const (
	chkClick    = iota // 1. клик меняет текст метки
	chkDo              // 2. Do из горутины
	chkAfter           // 3. After меняет заголовок окна
	chkWatch           // 4. Watch на pipe
	chkCloseNo         // 5а. крестик → «Нет» → окно осталось
	chkCloseYes        // 5б. крестик → «Да» → окно закрыто, цикл завершён
	chkThread          // обработчики выполняются в главном потоке
	chkUTF8            // кириллица и эмодзи проходят Go → GTK → Go без потерь
	nChecks
)

var names = [nChecks]string{
	"1. клик меняет текст метки",
	"2. Do из горутины",
	"3. After меняет заголовок окна",
	"4. Watch на pipe",
	"5а. крестик → «Нет» → окно осталось",
	"5б. крестик → «Да» → окно закрыто, цикл завершён",
	"обработчики в главном потоке",
	"кириллица и эмодзи без потерь",
}

var passed [nChecks]bool

func main() {
	auto := false
	for i := 1; i < len(os.Args); i++ {
		switch os.Args[i] {
		case "-auto":
			auto = true
		case "-shots": // каталог для снимков фотоотчёта
			if i+1 < len(os.Args) {
				i++
				shotDir = os.Args[i]
			}
		}
	}
	if C.gtk_init_check(nil, nil) == 0 {
		println("s1: нет дисплея или GTK не инициализировался")
		os.Exit(2)
	}

	status := NewLabel("Нажмите кнопку")
	doLabel := NewLabel("Do: ждём горутину…")
	pipeLabel := NewLabel("Watch: ждём данные из pipe…")

	// 1. Клик меняет текст метки.
	clicks := 0
	button := NewButton("Нажми меня", func() {
		clicks++
		want := "Нажато " + itoa(clicks) + " · Привет, мир 🙂"
		status.SetText(want)
		passed[chkClick] = true
		passed[chkUTF8] = status.Text() == want
		passed[chkThread] = isMainThread()
	})

	win := NewWindow("nui · спайк S1", VBox(status, button, doLabel, pipeLabel))

	// 5. Закрытие крестиком с подтверждением: delete-event с ответом 1 / 0.
	closedByUser := false
	win.onClose = func() bool {
		yes := Ask(win, "Закрыть окно?")
		if yes {
			closedByUser = true
		} else {
			After(0, func() { passed[chkCloseNo] = win.id != 0 })
		}
		return yes
	}

	// 2. Do из горутины.
	go func() {
		time.Sleep(500 * time.Millisecond)
		Do(func() {
			doLabel.SetText("Do: выполнено в UI-потоке")
			passed[chkDo] = isMainThread()
		})
	}()

	// 3. After меняет заголовок окна.
	const title = "nui · спайк S1 · After сработал"
	After(800, func() {
		win.SetTitle(title)
		passed[chkAfter] = win.Title() == title
	})

	// 4. Watch на pipe: горутина пишет, обработчик в UI-потоке читает.
	var p [2]int
	if err := syscall.Pipe2(p[:], syscall.O_NONBLOCK|syscall.O_CLOEXEC); err != nil {
		panic(err)
	}
	const msg = "данные из горутины"
	var watcher *Watcher
	watcher = Watch(p[0], ioRead, func(got uint) {
		var buf [256]byte
		n, _ := syscall.Read(p[0], buf[:])
		if n > 0 {
			pipeLabel.SetText("Watch: " + string(buf[:n]))
			passed[chkWatch] = string(buf[:n]) == msg && isMainThread()
		}
		watcher.Stop() // до close(fd) — по контракту
		syscall.Close(p[0])
	})
	go func() {
		time.Sleep(600 * time.Millisecond)
		syscall.Write(p[1], []byte(msg))
		syscall.Close(p[1])
	}()

	win.Show()
	if auto {
		autopilot(win, button)
	}
	C.gtk_main()

	passed[chkCloseYes] = closedByUser && win.id == 0
	report()
}

// autopilot имитирует пользователя. Ответы в диалоге приходят из таймеров,
// которые срабатывают внутри вложенного цикла gtk_dialog_run.
// Снимки (тег shots) — с паузой после действия, чтобы GTK успел перерисовать окно;
// новому окну диалога нужно больше времени, чем перерисовке метки.
func autopilot(win *Window, button *Button) {
	After(250, func() { shot("start") })
	After(300, func() { C.gtk_button_clicked((*C.GtkButton)(button.h)) })
	After(450, func() { shot("click") })
	After(1000, func() { shot("do-watch-after") })
	After(1200, func() { C.gtk_window_close((*C.GtkWindow)(win.h)) }) // откроет «Закрыть окно?»
	After(1900, func() { shot("ask") })
	After(2000, func() { C.nui_test_respond(0) }) // «Нет»
	After(2200, func() { shot("after-no") })
	After(2400, func() { C.gtk_window_close((*C.GtkWindow)(win.h)) })
	After(2700, func() { C.nui_test_respond(1) }) // «Да» → окно уничтожено → gtk_main_quit
	After(10000, func() {
		println("s1: автопилот не дождался завершения")
		os.Exit(3)
	})
}

var shotDir string // -shots DIR: каталог снимков; сами снимки — только со сборкой -tags shots

func report() {
	ok := true
	for i, name := range names {
		mark := "✓"
		if !passed[i] {
			mark, ok = "✗", false
		}
		println(mark, name)
	}
	if !ok {
		println("s1: есть непройденные проверки")
		os.Exit(1)
	}
	println("s1: все проверки пройдены")
}

func itoa(n int) string {
	if n == 0 {
		return "0"
	}
	var b [20]byte
	i := len(b)
	for ; n > 0; n /= 10 {
		i--
		b[i] = byte('0' + n%10)
	}
	return string(b[i:])
}
