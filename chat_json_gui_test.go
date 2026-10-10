//go:build gui

package nui

import (
	"slices"
	"strings"
	"testing"
)

// LoadJSON: свой формат и формат API моделей; ошибки не трогают ленту; JSON() — туда и
// обратно; Stream попадает в историю.
func TestChatJSON(t *testing.T) {
	ui(func() {
		c := NewChat()
		w := NewWindow("чат из JSON", c)
		w.SetSize(400, 300)
		w.Show()

		own := `[
			{"who": "Вы", "text": "Привет! Как сказать \"кот\"?"},
			{"who": "ИИ", "text": "Cat 🙂\nЕщё строка", "лишнее": 1},
			{"who": "", "text": "служебное"},
			{"who": "Вы", "text": ""}
		]`
		if err := c.LoadJSON([]byte(own)); err != nil {
			t.Fatalf("свой формат: %v", err)
		}
		want := []ChatMessage{{"Вы", `Привет! Как сказать "кот"?`}, {"ИИ", "Cat 🙂\nЕщё строка"}, {"", "служебное"}}
		if got := c.Messages(); !slices.Equal(got, want) {
			t.Errorf("Messages = %q, ожидалось %q", got, want)
		}
		if lent := "Вы:\nПривет! Как сказать \"кот\"?\n\nИИ:\nCat 🙂\nЕщё строка\n\nслужебное\n\n"; c.Text() != lent {
			t.Errorf("лента %q, ожидалось %q", c.Text(), lent)
		}
		if c.LastMessage() != "служебное" {
			t.Errorf("LastMessage = %q", c.LastMessage())
		}

		c.SetLabels("Я", "", "")
		api := `{"model": "x", "messages": [
			{"role": "system", "content": "Ты помощник."},
			{"role": "user", "content": [{"type": "text", "text": "Что "}, {"type": "image_url", "image_url": {"url": "data:..."}}, {"type": "text", "text": "на фото?"}]},
			{"role": "assistant", "content": null, "tool_calls": []},
			{"role": "tool", "content": "{\"ok\": true}"},
			{"role": "assistant", "content": "Кот."},
			{"role": "user", "name": "Гриша", "content": "Спасибо"}
		]}`
		if err := c.LoadJSON([]byte(api)); err != nil {
			t.Fatalf("формат API: %v", err)
		}
		want = []ChatMessage{{"Я", "Что на фото?"}, {"ИИ", "Кот."}, {"Гриша", "Спасибо"}}
		if got := c.Messages(); !slices.Equal(got, want) {
			t.Errorf("Messages = %q, ожидалось %q", got, want)
		}

		before := c.Text()
		for _, bad := range []string{`[{"who": "Вы", "text": "обрыв`, `{"who": "Вы"}`, `[1, 2]`, `"строка"`} {
			if err := c.LoadJSON([]byte(bad)); err == nil {
				t.Errorf("%s: ожидалась ошибка", bad)
			} else if !strings.HasPrefix(err.Error(), "nui: JSON чата: ") {
				t.Errorf("текст ошибки: %v", err)
			}
		}
		if c.Text() != before {
			t.Error("ошибка LoadJSON изменила ленту")
		}

		saved := c.JSON() // туда и обратно
		c.Clear()
		if err := c.LoadJSON(saved); err != nil || !slices.Equal(c.Messages(), want) {
			t.Errorf("LoadJSON(JSON()) = %q, %v; JSON:\n%s", c.Messages(), err, saved)
		}

		st := c.Stream("ИИ")
		st.WriteString("часть 1, ")
		st.WriteString("часть 2")
		st.Close()
		c.flush()
		if m := c.Messages(); m[len(m)-1] != (ChatMessage{"ИИ", "часть 1, часть 2"}) {
			t.Errorf("после Stream: %q", m[len(m)-1])
		}
		w.Close()
		if c.Messages() != nil || c.LoadJSON([]byte("[]")) != nil {
			t.Error("уничтоженный чат: Messages не nil или LoadJSON с ошибкой")
		}
	})
}
