//go:build shots

package main

import "os/exec"

var shotN int

// shot снимает весь экран (под Xvfb это только наши окна) в shotDir/NN-name.png.
// Выполняется синхронно в UI-потоке: окно уже нарисовано, X-сервер хранит его содержимое.
// Снимок — через rootshot (spike/tools/rootshot.c), а не xwd -root: xwd теряет окна
// с не-умолчательным визуалом, и модальный диалог выходил чёрным.
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
	if err := exec.Command("sh", "-c", "rootshot | pnmtopng > "+path).Run(); err != nil {
		println("s1: снимок не удался:", path, err.Error())
	}
}
