//go:build shots

package main

import "os/exec"

var shotN int

// shot снимает весь экран (под Xvfb это только наше окно) в shotDir/NN-name.png.
// Выполняется синхронно в UI-потоке: окно уже нарисовано, X-сервер хранит его содержимое.
// Под тегом shots, чтобы os/exec не попадал в бинарник, по которому меряют размер.
func shot(name string) {
	if shotDir == "" {
		return
	}
	shotN++
	n := itoa(shotN)
	if shotN < 10 {
		n = "0" + n
	}
	path := shotDir + "/" + n + "-" + name + ".png"
	if err := exec.Command("sh", "-c", "xwd -root -silent | xwdtopnm 2>/dev/null | pnmtopng > "+path).Run(); err != nil {
		println("s1: снимок не удался:", path, err.Error())
	}
}
