//go:build gui

package nui

// Интеграционные тесты диалогов и оболочки (фаза 3). Диалог модальный и синхронный,
// поэтому он открывается через Do и ждёт ответа во вложенном цикле, а тест отвечает
// через ui() — Do работает и внутри вложенного цикла.

import (
	"os"
	"os/exec"
	"path/filepath"
	"strings"
	"testing"
	"time"

	"nui/internal/drv"
)

// openDialog запускает диалог в UI-потоке и ждёт, пока он появится на экране.
func openDialog[T any](t *testing.T, chooser bool, show func() T) <-chan T {
	t.Helper()
	res := make(chan T, 1)
	Do(func() { res <- show() })
	waitUntil(t, 5000, func() bool { return drv.DialogOpen(chooser) })
	return res
}

func receive[T any](t *testing.T, ch <-chan T) T {
	t.Helper()
	select {
	case v := <-ch:
		return v
	case <-time.After(5 * time.Second):
		t.Fatal("диалог не закрылся")
	}
	var zero T
	return zero
}

func TestMessageDialogs(t *testing.T) {
	var w *Window
	ticks := 0
	var tm *Timer
	ui(func() {
		w = NewWindow("диалоги", NewLabel("Родитель диалогов"))
		w.Show()
		tm = Every(20, func() { ticks++ })
	})

	// Ask: «Да», «Нет», Esc.
	for _, c := range []struct {
		name   string
		answer func() bool
		want   bool
	}{
		{"ask", func() bool { return drv.SimAnswer(true) }, true},
		{"ask-no", func() bool { return drv.SimAnswer(false) }, false},
		{"ask-esc", drv.SimEsc, false},
	} {
		res := openDialog(t, false, func() bool {
			return Ask(w, "Выход", "Закрыть программу?\n\nВсё сохранено. 🙂")
		})
		if c.name == "ask" {
			shot(t, "ask")
		}
		ui(func() {
			if !c.answer() {
				t.Error("диалог не найден")
			}
		})
		if got := receive(t, res); got != c.want {
			t.Fatalf("%s: Ask вернул %v, ожидалось %v", c.name, got, c.want)
		}
	}

	// Message и Alert; пока открыт Message, таймер продолжает тикать.
	before := 0
	ui(func() { before = ticks })
	done := openDialog(t, false, func() bool { Message(w, "Готово", "Файл сохранён в ~/esp32_logs"); return true })
	time.Sleep(200 * time.Millisecond)
	shot(t, "message")
	during := 0
	ui(func() {
		during = ticks
		drv.SimOK()
	})
	receive(t, done)
	if during-before < 3 {
		t.Fatalf("пока открыт Message, Every тикнул %d раз — таймеры должны работать", during-before)
	}
	done = openDialog(t, false, func() bool {
		Alert(nil, "Нет доступа к порту", "Добавьте пользователя в группу dialout.")
		return true
	})
	shot(t, "alert")
	ui(func() { drv.SimOK() })
	receive(t, done)
	ui(func() {
		tm.Stop()
		w.Close()
	})
}

// Родитель уничтожается, пока открыт диалог: DESTROY_WITH_PARENT закрывает диалог, Ask
// возвращает false, и ничего не падает (своя ссылка в nui_message).
func TestDialogParentDestroyed(t *testing.T) {
	var w *Window
	ui(func() {
		w = NewWindow("родитель", NewLabel("будет закрыт"))
		w.Show()
		After(300, w.Close)
	})
	res := openDialog(t, false, func() bool { return Ask(w, "Вопрос", "Родитель закроется сам") })
	if receive(t, res) {
		t.Fatal("Ask после уничтожения родителя должен вернуть false")
	}
}

func TestFileDialogs(t *testing.T) {
	dir := t.TempDir()
	for _, f := range []string{"журнал.log", "заметки.txt"} {
		if err := os.WriteFile(filepath.Join(dir, f), []byte("x"), 0o644); err != nil {
			t.Fatal(err)
		}
	}
	var w *Window
	ui(func() {
		w = NewWindow("файлы", NewLabel("Родитель файловых диалогов"))
		w.Show()
	})
	defer ui(func() { w.Close() })
	type result struct {
		path string
		ok   bool
	}
	pick := func(name string, show func() (string, bool), choose string, accept bool) result {
		t.Helper()
		res := openDialog(t, true, func() result { p, ok := show(); return result{p, ok} })
		if choose != "" {
			ui(func() {
				if !drv.SimChooserSet(choose) {
					t.Errorf("%s: не удалось выбрать %s", name, choose)
				}
			})
			time.Sleep(700 * time.Millisecond) // каталог загружается асинхронно
		}
		shot(t, name)
		ui(func() { drv.SimChooserDone(accept) })
		return receive(t, res)
	}

	logs := Filter{Name: "Логи", Patterns: []string{"*.log", "*.txt"}}
	want := filepath.Join(dir, "журнал.log")
	if r := pick("open-file", func() (string, bool) { return OpenFile(w, "Открыть журнал", logs) }, want, true); !r.ok || r.path != want {
		t.Fatalf("OpenFile: %q, %v; ожидалось %q", r.path, r.ok, want)
	}
	if r := pick("open-cancel", func() (string, bool) { return OpenFile(w, "Открыть журнал") }, "", false); r.ok || r.path != "" {
		t.Fatalf("отмена OpenFile: %q, %v", r.path, r.ok)
	}
	want = filepath.Join(dir, "новый.log")
	if r := pick("save-file", func() (string, bool) { return SaveFile(w, "Сохранить журнал", "новый.log", logs) }, want, true); !r.ok || r.path != want {
		t.Fatalf("SaveFile: %q, %v; ожидалось %q", r.path, r.ok, want)
	}
	if r := pick("choose-folder", func() (string, bool) { return ChooseFolder(w, "Папка для логов") }, dir, true); !r.ok || r.path != dir {
		t.Fatalf("ChooseFolder: %q, %v; ожидалось %q", r.path, r.ok, dir)
	}
}

// OpenURI и ShowInFolder — в дочернем процессе с подставной программой по умолчанию:
// она дописывает в opened.txt всё, что её попросили открыть (https и каталоги).
func TestOpenURIAndShowInFolder(t *testing.T) {
	tmp := t.TempDir()
	rec := filepath.Join(tmp, "opened.txt")
	script := filepath.Join(tmp, "record.sh")
	write := func(path, s string, mode os.FileMode) {
		if err := os.MkdirAll(filepath.Dir(path), 0o755); err != nil {
			t.Fatal(err)
		}
		if err := os.WriteFile(path, []byte(s), mode); err != nil {
			t.Fatal(err)
		}
	}
	write(script, "#!/bin/sh\necho \"$1\" >> "+rec+"\n", 0o755)
	write(filepath.Join(tmp, "data/applications/nui-record.desktop"), "[Desktop Entry]\nType=Application\nName=nui record\nExec="+
		script+" %u\nMimeType=x-scheme-handler/https;inode/directory;\n", 0o644)
	write(filepath.Join(tmp, "config/mimeapps.list"), "[Default Applications]\nx-scheme-handler/https=nui-record.desktop\ninode/directory=nui-record.desktop\n", 0o644)
	file := filepath.Join(tmp, "логи", "журнал.log")
	write(file, "x", 0o644)

	env := []string{"XDG_DATA_HOME=" + tmp + "/data", "XDG_CONFIG_HOME=" + tmp + "/config",
		"XDG_DATA_DIRS=" + tmp + "/data:/usr/share", "NUI_REC=" + rec, "NUI_FILE=" + file}
	check := func(name string, extraEnv []string, unset []string, want string) {
		t.Helper()
		os.Remove(rec)
		runHelperEnv(t, name, 0, append(env, extraEnv...), unset)
		b, _ := os.ReadFile(rec)
		if got := strings.TrimSpace(string(b)); !strings.Contains(got, want) {
			t.Fatalf("%s: открыто %q, ожидалось %q", name, got, want)
		}
	}
	check("openuri", nil, []string{"DBUS_SESSION_BUS_ADDRESS"}, "https://example.org/nui?a=1")
	// Без сессии D-Bus ShowInFolder сразу открывает папку.
	check("showinfolder", nil, []string{"DBUS_SESSION_BUS_ADDRESS"}, filepath.Dir(file))
	// С сессией D-Bus, но без FileManager1: асинхронный запрос → ошибка → запасной путь.
	if _, err := os.Stat("/usr/share/dbus-1/session.conf"); err != nil {
		t.Log("нет /usr/share/dbus-1/session.conf — путь через D-Bus не проверен")
		return
	}
	out, err := exec.Command("dbus-daemon", "--session", "--fork", "--print-address=1", "--print-pid=1").Output()
	if err != nil {
		t.Logf("dbus-daemon не запустился (%v) — путь через D-Bus не проверен", err)
		return
	}
	lines := strings.Fields(string(out))
	if len(lines) < 2 {
		t.Fatalf("dbus-daemon: %q", out)
	}
	defer exec.Command("kill", lines[1]).Run()
	check("showinfolder", []string{"DBUS_SESSION_BUS_ADDRESS=" + lines[0]}, nil, filepath.Dir(file))
}
