package main

import "github.com/SamarGrigoriy/gonui"

func main() {
	err := nui.Run(func() {
		selftest()
		nui.NewWindow("github.com/SamarGrigoriy/gonui", nui.VBox(nui.NewLabel("Привет, мир!"), nui.NewButton("Закрыть", nui.Quit))).Show()
	})
	if err != nil {
		println(err.Error())
	}
}
