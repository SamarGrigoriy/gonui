package drv

// Общие для всех ОС константы и типы контракта драйвера (docs/driver-gtk3.md, §3).
// Функции контракта — в файлах *_linux.* и *_windows.*.

// Виды сообщений MessageBox.
const (
	MsgInfo  = 0 // информация, кнопка OK
	MsgAlert = 1 // ошибка или предупреждение, кнопка OK
	MsgAsk   = 2 // вопрос, кнопки Да/Нет
)

// Режимы FileDialog.
const (
	FileOpen   = 0
	FileSave   = 1
	FileFolder = 2
)

// Filter — фильтр файлового диалога: имя и шаблоны вида "*.log".
type Filter struct {
	Name     string
	Patterns []string
}
