//go:build gui

package nui

// Интеграционные тесты, которые есть только на Linux: Watch (unix), нет дисплея,
// OpenURI и ShowInFolder с подставной программой по умолчанию (XDG, D-Bus).

import (
	"os"
	"os/exec"
	"path/filepath"
	"strings"
	"syscall"
	"testing"
	"time"
)

func TestWatchPipe(t *testing.T) {
	var p [2]int
	if err := syscall.Pipe2(p[:], syscall.O_NONBLOCK|syscall.O_CLOEXEC); err != nil {
		t.Fatal(err)
	}
	defer syscall.Close(p[0])
	var got []IOCond
	var data string
	var w *Watcher
	ui(func() {
		w = Watch(p[0], IORead, func(c IOCond) {
			got = append(got, c)
			var buf [64]byte
			if n, _ := syscall.Read(p[0], buf[:]); n > 0 {
				data += string(buf[:n])
			}
			if c&IOHup != 0 {
				w.Stop() // до close(fd) — по контракту
			}
		})
	})
	syscall.Write(p[1], []byte("привет"))
	waitUntil(t, 1000, func() bool { return data == "привет" })
	syscall.Close(p[1])
	waitUntil(t, 1000, func() bool { return len(got) > 0 && got[len(got)-1]&IOHup != 0 })
	n := 0
	ui(func() {
		n = len(got)
		if got[0]&IORead == 0 {
			t.Errorf("первое событие %b без IORead", got[0])
		}
	})
	time.Sleep(100 * time.Millisecond)
	ui(func() {
		if len(got) != n {
			t.Error("после Stop наблюдатель продолжает срабатывать")
		}
		w.Stop()
	})
}

func TestNoDisplay(t *testing.T) {
	runHelper(t, "nodisplay", 3, []string{"DISPLAY", "WAYLAND_DISPLAY"})
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
