package nui

import (
	"errors"
	"sync"
	"unicode/utf8"

	"nui/internal/drv"
)

// Chat — готовый чат (api.md, §18): лента сообщений, поле ввода, кнопки «Отправить»
// и «Стоп». Собран из TextView, Entry и Button, поэтому выглядит родным на каждой ОС.
// Ответ, который приходит частями (например, от ИИ), пишется через Stream — из любой
// горутины.
//
//	chat := nui.NewChat()
//	chat.OnSend(func(q string) {
//		st := chat.Stream("ИИ")
//		go func() { defer st.Close(); ask(q, st) }() // ask пишет ответ в io.Writer
//	})
type Chat struct {
	*widget // корень — VBox: общие методы виджета (SetVisible, SetMinSize, Alive …)

	log         *TextView
	input       *Entry
	send, stop  *Button
	you         string
	onSend      func(text string)
	onStop      func()
	busy        bool
	cur         *ChatStream // сообщение, которое сейчас дописывается
	tick        *Timer      // переносит текст cur в ленту
	lastNewline bool        // лента кончается переводом строки
}

// NewChat создаёт чат. Лента растягивается: задайте окну размер (SetSize) или вставьте
// чат через Expand.
func NewChat() *Chat {
	c := &Chat{you: "Вы", lastNewline: true}
	c.log = NewTextView()
	c.log.SetReadOnly(true)
	c.log.SetWrap(true)
	c.log.SetAutoScroll(true)
	c.input = NewEntry()
	c.input.OnEnter(c.submit)
	c.send = NewButton("Отправить", c.submit)
	c.stop = NewButton("Стоп", c.pressStop)
	c.stop.SetVisible(false)
	root := VBox(Expand(c.log), HBox(Expand(c.input), c.stop, c.send))
	c.widget = &root.widget
	return c
}

// OnSend: пользователь отправил непустой текст (Enter или «Отправить»). К вызову fn
// поле уже очищено, а сообщение пользователя уже в ленте. Пока чат занят (SetBusy,
// идёт Stream), отправить нельзя.
func (c *Chat) OnSend(fn func(text string)) { c.onSend = fn }

// OnStop: пользователь нажал «Стоп». Кнопка видна, только пока чат занят и fn задан.
// Текущее сообщение (Stream) к вызову fn уже закончено: дальнейшая запись в него вернёт
// ErrChatClosed — так io.Copy сам остановится.
func (c *Chat) OnStop(fn func()) {
	c.onStop = fn
	if !c.dead() {
		c.stop.SetVisible(c.busy && fn != nil)
	}
}

// Add добавляет готовое сообщение: «who:», текст и пустую строку. who == "" — без
// подписи. Сообщение, которое дописывалось (Stream), сначала заканчивается.
func (c *Chat) Add(who, text string) {
	if c.dead() {
		return
	}
	c.finish()
	c.appendLog(header(who) + text)
	c.endMessage()
}

// Stream начинает сообщение, которое приходит частями, и делает чат занятым. Писать
// в него можно из любой горутины; когда ответ закончен — Close. Предыдущее
// незаконченное сообщение заканчивается.
func (c *Chat) Stream(who string) *ChatStream {
	s := &ChatStream{}
	if c.dead() {
		s.closed = true
		return s
	}
	c.finish()
	c.appendLog(header(who))
	c.cur = s
	c.setBusy(true)
	c.tick = Every(40, c.flush)
	return s
}

// SetBusy — чат ждёт ответа: «Отправить» недоступна, «Стоп» видна (если задан OnStop).
// Stream включает это сам и выключает, когда сообщение закончено.
func (c *Chat) SetBusy(on bool) {
	if !c.dead() {
		c.setBusy(on)
	}
}

// Busy — чат ждёт ответа.
func (c *Chat) Busy() bool { return c.busy && !c.dead() }

// Clear очищает ленту; незаконченное сообщение заканчивается.
func (c *Chat) Clear() {
	if c.dead() {
		return
	}
	c.finish()
	c.log.Clear()
	c.lastNewline = true
}

// Text — вся лента.
func (c *Chat) Text() string {
	if c.dead() {
		return ""
	}
	return c.log.Text()
}

// SetPlaceholder — подсказка в пустом поле ввода.
func (c *Chat) SetPlaceholder(s string) {
	if !c.dead() {
		c.input.SetPlaceholder(s)
	}
}

// SetLabels — подпись сообщений пользователя и надписи кнопок; "" — оставить как было.
// По умолчанию «Вы», «Отправить», «Стоп».
func (c *Chat) SetLabels(you, send, stop string) {
	if c.dead() {
		return
	}
	if you != "" {
		c.you = you
	}
	if send != "" {
		c.send.SetText(send)
	}
	if stop != "" {
		c.stop.SetText(stop)
	}
}

// SetColor — цвет текста ленты; NoColor — цвет темы.
func (c *Chat) SetColor(col Color) {
	if !c.dead() {
		c.log.SetColor(col)
	}
}

// SetBackground — фон чата: лента и полоса с полем ввода; NoColor — фон темы.
func (c *Chat) SetBackground(col Color) {
	if c.dead() {
		return
	}
	c.log.SetBackground(col)
	drv.SetBackground(c.h, uint32(col), true)
}

// Focus передаёт фокус полю ввода.
func (c *Chat) Focus() {
	if !c.dead() {
		c.input.Focus()
	}
}

func (c *Chat) submit() {
	if c.dead() || c.busy {
		return
	}
	text := c.input.Text()
	if blank(text) {
		return
	}
	c.input.SetText("")
	c.Add(c.you, text)
	if c.onSend != nil {
		c.onSend(text)
	}
}

func (c *Chat) pressStop() {
	if c.cur != nil {
		c.cur.Close()
		c.flush()
	}
	if c.onStop != nil {
		c.onStop()
	}
}

func (c *Chat) setBusy(on bool) {
	c.busy = on
	c.send.SetEnabled(!on)
	c.stop.SetVisible(on && c.onStop != nil)
}

// flush — таймер UI-потока: накопленное в cur — в ленту; закрытый cur — закончить.
func (c *Chat) flush() {
	s := c.cur
	if s == nil || !c.Alive() { // чат уничтожен вместе с окном: молча остановиться
		c.stopTick()
		c.cur = nil
		return
	}
	text, done := s.take(false)
	c.appendLog(text)
	if done {
		c.cur = nil
		c.stopTick()
		c.endMessage()
		c.setBusy(false)
	}
}

// finish заканчивает незаконченное сообщение сразу: всё накопленное — в ленту.
func (c *Chat) finish() {
	s := c.cur
	if s == nil {
		return
	}
	s.Close()
	text, _ := s.take(true)
	c.appendLog(text)
	c.cur = nil
	c.stopTick()
	c.endMessage()
	c.setBusy(false)
}

func (c *Chat) stopTick() {
	if c.tick != nil {
		c.tick.Stop()
		c.tick = nil
	}
}

func (c *Chat) appendLog(s string) {
	if s == "" {
		return
	}
	c.log.Append(s)
	c.lastNewline = s[len(s)-1] == '\n'
}

// endMessage — пустая строка после сообщения.
func (c *Chat) endMessage() {
	if c.lastNewline {
		c.appendLog("\n")
	} else {
		c.appendLog("\n\n")
	}
}

func header(who string) string {
	if who == "" {
		return ""
	}
	return who + ":\n"
}

func blank(s string) bool {
	for i := 0; i < len(s); i++ {
		switch s[i] {
		case ' ', '\t', '\n', '\r':
		default:
			return false
		}
	}
	return true
}

// ErrChatClosed — запись в законченное сообщение чата: Close, «Стоп» или новое сообщение.
var ErrChatClosed = errors.New("nui: сообщение чата уже закончено")

// ChatStream — сообщение чата, которое приходит частями (Chat.Stream). Write, WriteString
// и Close можно вызывать из любой горутины — это исключение из правила api.md, §0:
// текст копится в буфере, а в ленту его переносит таймер UI-потока (примерно 25 раз
// в секунду). Символ UTF-8, разрезанный между двумя Write, не ломается.
type ChatStream struct {
	mu     sync.Mutex
	buf    []byte
	closed bool
}

// Write дописывает p к сообщению (io.Writer). После Close — ErrChatClosed.
func (s *ChatStream) Write(p []byte) (int, error) {
	s.mu.Lock()
	defer s.mu.Unlock()
	if s.closed {
		return 0, ErrChatClosed
	}
	s.buf = append(s.buf, p...)
	return len(p), nil
}

// WriteString — то же для строки (io.StringWriter).
func (s *ChatStream) WriteString(str string) (int, error) {
	s.mu.Lock()
	defer s.mu.Unlock()
	if s.closed {
		return 0, ErrChatClosed
	}
	s.buf = append(s.buf, str...)
	return len(str), nil
}

// Close заканчивает сообщение (io.Closer): остаток уйдёт в ленту, чат перестанет быть
// занятым. Повторный Close ничего не делает.
func (s *ChatStream) Close() error {
	s.mu.Lock()
	s.closed = true
	s.mu.Unlock()
	return nil
}

// take забирает накопленное. Пока сообщение не закончено (и не all), недописанный
// последний символ UTF-8 остаётся в буфере. done — закончено и всё забрано.
func (s *ChatStream) take(all bool) (text string, done bool) {
	s.mu.Lock()
	defer s.mu.Unlock()
	n := len(s.buf)
	if !s.closed && !all {
		n = fullRunes(s.buf)
	}
	text = string(s.buf[:n])
	s.buf = append(s.buf[:0], s.buf[n:]...)
	return text, s.closed && len(s.buf) == 0
}

// fullRunes — длина начала b, которое кончается целым символом UTF-8.
func fullRunes(b []byte) int {
	i := len(b) - 1
	for i >= 0 && len(b)-i < utf8.UTFMax && !utf8.RuneStart(b[i]) {
		i--
	}
	if i < 0 || utf8.FullRune(b[i:]) {
		return len(b)
	}
	return i
}
