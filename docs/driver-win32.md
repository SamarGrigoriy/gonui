# Драйвер Windows: Win32 (фаза 5)

Пакет `internal/drv`, файлы `*_windows.*`. Цель — Windows 10 1703+ и Windows 11, x64.

Контракт драйвера тот же, что в [driver-gtk3.md](driver-gtk3.md), §3. Ниже описано, чем Win32 отличается от Linux. Многие приёмы уже проверены в эталонной программе `esp32_logger.c` (Win32 на C, 223 КБ); ссылки на неё помечены «(логгер)».

## 1. Принципы и тулчейн

- FFI тот же: cgo с собственными прототипами в `win32_windows.h` и обёртки в `wrap_windows.c` (ADR-3).
  - Заголовки mingw входят в тулчейн, поэтому прототипы сверяются с `<windows.h>` тем же приёмом `NUI_CHECK` ([driver-gtk3.md](driver-gtk3.md), §10).
- Сборка gc на Debian: `CC=x86_64-w64-mingw32-gcc CGO_ENABLED=1 GOOS=windows GOARCH=amd64 go build -ldflags "-s -w -H windowsgui"`. Пакет `gcc-mingw-w64-x86-64`.
- TinyGo под Windows (встроенная цель на mingw-w64) проверен спайком **S4** ([spike/s4/RESULT.md](../spike/s4/RESULT.md)): hello — 46 КБ против 1,37 МБ у gc.
  - cgo без заголовков и `//export`-WndProc работают.
  - `.syso` TinyGo не подхватывает. Тот же объект ресурсов уходит линковщику из файла с тегом `tinygo`: `#cgo LDFLAGS: -L. -l:rsrc.o` (`${SRCDIR}` TinyGo не раскрывает, относительный `-L` отсчитывает от каталога пакета).
  - Подсистема GUI — после линковки: `x86_64-w64-mingw32-objcopy --subsystem windows --strip-all` (`-Wl,--subsystem` ld.lld получает как есть и не понимает).
  - TinyGo несёт библиотеки импорта только для kernel32, advapi32 и UCRT; `user32`, `gdi32` и прочие — из mingw-w64: `-L/usr/x86_64-w64-mingw32/lib`.
  - Под Windows в TinyGo 0.42 линкуется только планировщик `tasks`: горутины кооперативные и не выполняются, пока главная ждёт в `GetMessageW`. Программам с горутинами (рабочий поток + `Do`) на Windows нужна сборка gc.
- Под Wine 8.0 (Debian 12) программы на Go ≥ 1.22 не стартуют: нет `bcryptprimitives.dll` (`ProcessPrng`), которая есть в Windows 10+. Для тестов под Wine в префикс кладётся заглушка (`spike/s4/wine`).
- **Не используйте `-fdata-sections` в CFLAGS для mingw** (логгер, `build.ps1`): MinGW с этим флагом кладёт нулевые статические буферы не в `.bss`, а в `.data$имя`, то есть прямо в файл, и код вырастает в разы. `-ffunction-sections` безопасен.
- Флаги C-части передаются через `CGO_CFLAGS`/`CGO_LDFLAGS`, а не через `#cgo`: gc не пропускает их в `#cgo` ([driver-gtk3.md](driver-gtk3.md), §9). Для mingw: `CGO_CFLAGS=-Os -ffunction-sections`.

## 2. Хендлы и узлы

У Win32 нет контейнеров-окон для раскладки, поэтому `Box`, `Grid` и `Stretch` — **виртуальные** узлы без HWND.

```go
type H = uintptr // индекс узла во внутренней таблице драйвера (не HWND!)
var Nil H        // 0 — нулевой хендл (driver-gtk3.md, §3)

type node struct {
	hwnd     uintptr      // 0 у виртуальных узлов (Box, Grid, Stretch)
	id       uint32       // id реестра core
	kind     uint8
	lay      layout.Node  // параметры и результат раскладки (layout.md, §7)
	parent   H
	children []H
	// для Label: шрифт (обычный/жирный), цвет; для Entry: есть ли OnEnter; …
}
```

- В `GWLP_USERDATA` у каждого HWND лежит id. По `lParam` (HWND контрола) в `WM_COMMAND` и `WM_HSCROLL` драйвер находит id.
- **Идентификатор контрола (`hMenu`) не используется**: в `WM_COMMAND` он 16-битный, а наши id длиннее.

## 3. Окна, цикл, Do, таймеры

| Что | Как |
|---|---|
| Класс окна | Один класс `nuiWindow` и один `WndProc`. Сам `WndProc` — трамплин `nuiWndProc` (`//export`), указатель на него подставляет обёртка `nui_register_class` |
| Служебное окно | `HWND_MESSAGE`-окно (message-only) того же класса. На него приходят `Do` и таймеры |
| Стиль окна | Растягиваемое: `WS_OVERLAPPEDWINDOW`. Нерастягиваемое: без `WS_THICKFRAME` и `WS_MAXIMIZEBOX` (логгер). Всегда `WS_EX_CONTROLPARENT` (Tab между контролами) |
| Цикл | `GetMessageW` → `nui_pre_translate` (Enter в Entry с `OnEnter`) → `IsDialogMessageW(активное окно)` (Tab, Shift+Tab, Enter = кнопка по умолчанию, Esc) → `TranslateMessage`/`DispatchMessageW` (логгер) |
| `Quit` | `PostQuitMessage(0)` |
| `Wake` (Do) | `PostMessageW(msgWnd, WM_APP_DO, 0, 0)` — потокобезопасно (в логгере так же: `PostMessageW(g_wnd, WM_OFFER_DRIVER, …)` из рабочего потока) |
| `TimerAdd(ms, ud)` | `SetTimer(msgWnd, ud, ms, NULL)`. В `nIDEvent` (`UINT_PTR`) кладём `ud` целиком, и `WM_TIMER` сразу даёт `ud` |
| `SourceRemove` | `KillTimer(msgWnd, ud)`. Для этого драйвер хранит соответствие `src → ud`; `src` — это и есть `ud` |
| `IsMainThread` | `GetCurrentThreadId() == mainTID` (запоминается в `Init`) |
| `WatchAdd` | **нет**. `Watch` существует только в unix-сборке, на Windows — горутина + `Do` |

Модальные `MessageBox` и файловые диалоги крутят свой цикл, но сообщения служебному окну диспетчеризуют. Поэтому `Do` и таймеры продолжают работать, как и на GTK.

## 4. События и уничтожение

| Событие Win32 | Kind |
|---|---|
| `WM_COMMAND` / `BN_CLICKED` от кнопки | EvClick |
| `WM_COMMAND` / `BN_CLICKED` от флажка | EvChange |
| `WM_COMMAND` / `EN_CHANGE` | EvChange (и у Entry, и у TextView) |
| `WM_COMMAND` / `CBN_SELCHANGE`, `CBN_EDITCHANGE` | EvChange |
| `WM_HSCROLL` от trackbar | EvChange |
| `WM_KEYDOWN VK_RETURN` в Entry с `OnEnter` (до `IsDialogMessage`) | EvEnter |
| `WM_CLOSE` | EvClose. Ответ 1 — не вызывать `DestroyWindow` (логгер: подтверждение при идущей записи) |
| `WM_DESTROY` окна | EvDestroy **для окна и всех узлов его дерева**, включая виртуальные |

Стандартные контролы получают `WM_DESTROY` в своей оконной процедуре, а не в нашей. Поэтому `EvDestroy` рассылает **сам драйвер** обходом дерева узлов:
- при `WM_DESTROY` окна;
- при замене содержимого (`SetChild`): для удаляемого поддерева вызывается `DestroyWindow` каждого HWND и рассылается EvDestroy.

Программные изменения тоже присылают уведомления синхронно, например `SetWindowTextW` у Edit присылает `EN_CHANGE`. Их гасит `quiet` ([runtime.md](runtime.md), §7).

## 5. Контролы

| nui | Класс и стили | Операции |
|---|---|---|
| Label | `STATIC`, `SS_NOPREFIX`, `SS_LEFT/CENTER/RIGHT`; многоточие: End → `SS_ENDELLIPSIS`, Middle и Start → `SS_PATHELLIPSIS`; без переноса → `SS_LEFTNOWORDWRAP` (логгер) | Цвет: `WM_CTLCOLORSTATIC` → `SetTextColor`, `SetBkColor(GetSysColor(COLOR_WINDOW))`, вернуть `GetSysColorBrush(COLOR_WINDOW)` (логгер). Жирный шрифт — отдельный `HFONT` (логгер, `g_bold`). Выделяемый текст — read-only `EDIT` без рамки (пересоздание контрола) |
| Button | `BUTTON`, `BS_PUSHBUTTON \| WS_TABSTOP`; кнопка по умолчанию — `BS_DEFPUSHBUTTON` + `DM_SETDEFID` | `SetWindowTextW`. Win32 считает `&` в тексте префиксом мнемоники (`&Файл` → «Файл» с подчёркнутой «Ф»), поэтому драйвер удваивает его: `&` → `&&`, и текст выглядит как на GTK. `Text()` возвращает исходную строку |
| Check | `BUTTON`, `BS_AUTOCHECKBOX \| WS_TABSTOP` | `BM_GETCHECK` / `BM_SETCHECK`; текст — с тем же удвоением `&`, что у Button |
| Entry | `EDIT`, `ES_AUTOHSCROLL \| WS_TABSTOP`, `WS_EX_CLIENTEDGE`; пароль — `ES_PASSWORD` | `EM_SETCUEBANNER` (подсказка), `EM_SETREADONLY` |
| Combo | `COMBOBOX`, `CBS_DROPDOWNLIST` (Combo) / `CBS_DROPDOWN` (ComboEntry), `WS_VSCROLL \| WS_TABSTOP` (логгер) | `CB_ADDSTRING`, `CB_RESETCONTENT`, `CB_GETCURSEL`/`CB_SETCURSEL`, `CB_SETDROPPEDWIDTH` (логгер) |
| Slider | `msctls_trackbar32`, `TBS_HORZ` | `TBM_SETRANGEMIN/MAX`, `TBM_SETPOS`/`TBM_GETPOS` |
| Progress | `msctls_progress32`, диапазон 0…1000 (`PBM_SETRANGE32`) | `PBM_SETPOS`; `Pulse` → стиль `PBS_MARQUEE` + `PBM_SETMARQUEE`. **`SetText` не отображается**: у прогресс-бара Win32 нет надписи (расхождение, указано в [api.md](api.md)) |
| TextView | `EDIT`, `ES_MULTILINE \| ES_AUTOVSCROLL \| WS_VSCROLL` (+ `WS_HSCROLL \| ES_AUTOHSCROLL` без переноса) | `EM_SETLIMITTEXT(0x7FFFFFFE)`; `Append` → запомнить выделение (`EM_GETSEL`) и первую видимую строку (`EM_GETFIRSTVISIBLELINE`), `EM_SETSEL(len, len)` + `EM_REPLACESEL`, затем вернуть выделение (со сдвигом, если `MaxLines` удалил начало). С `AutoScroll` — прокрутка в конец через `WM_VSCROLL`/`SB_BOTTOM`, без него — возврат прокрутки через `EM_LINESCROLL`: `EM_REPLACESEL` сам прокручивает к каретке, а контракт обещает, что выделение и вид не сбиваются; `MaxLines` → `EM_LINEINDEX` + `EM_SETSEL(0, i)` + `EM_REPLACESEL("")`. **Переводы строк `\n` ↔ `\r\n`** на входе и выходе. `SetWrap` пересоздаёт контрол: стиль не меняется на лету. Моноширинный шрифт — Consolas |
| Separator | `STATIC`, `SS_ETCHEDHORZ` / `SS_ETCHEDVERT` | — |
| Frame | `BUTTON`, `BS_GROUPBOX`. Это **не** родитель: дети — соседние окна внутри её прямоугольника. Рамка создаётся первой (ниже по Z-порядку) | — |
| Scroll | Своё дочернее окно класса `nuiScroll` со `WS_VSCROLL`/`WS_HSCROLL`, дети — его дочерние окна; `WM_COMMAND` от них доставляется тем же `nuiWndProc` | `SetScrollInfo`, `ScrollWindowEx` |

Всем контролам: `WM_SETFONT` с текущим шрифтом окна (логгер, `ctl()`), `GWLP_USERDATA = id`.

## 6. Измерение контролов для `internal/layout`

| Контрол | Минимальный размер | Естественный размер |
|---|---|---|
| Label | без многоточия — как естественный; с многоточием — ширина «…» | `DrawTextW(DT_CALCRECT \| DT_SINGLELINE)`; с переносом — ширина 40 средних символов |
| Button | `BCM_GETIDEALSIZE` | `BCM_GETIDEALSIZE`, но ширина не меньше 75 DLU (стандарт Windows) |
| Check | ширина квадрата + отступ + текст (`GetSystemMetricsForDpi(SM_CXMENUCHECK)` + `GetTextExtentPoint32W`) | то же |
| Entry | 3 средних символа | `WidthChars` (по умолчанию 20) × средняя ширина символа + рамка; высота = высота шрифта + рамка |
| Combo | 3 символа + стрелка | max(ширин элементов) + стрелка (`SM_CXVSCROLL`) |
| Slider, Progress | 50 × стандартная высота | 150 × стандартная высота |
| TextView | 46 × 46 — как у GTK с Adwaita (по полосам прокрутки) | то же: размер задают `SetMinSize` или `Expand` |
| Separator | 1 px толщины | 1 px толщины |

Средняя ширина символа берётся из `GetTextMetricsW` (`tmAveCharWidth`), как в диалоговых единицах Windows.

`SetMinSize` драйвер не смешивает с измерением, а кладёт в `Floor` узла (layout.md, §7). Контейнерам с оформлением драйвер задаёт `Inset` и `Floor`:

| Узел | `Inset` | `Floor` |
|---|---|---|
| Frame | рамка `BS_GROUPBOX`; сверху — высота заголовка (в GTK с Adwaita: 1, 18, 1, 1) | ширина заголовка + поля рамки; без заголовка — только рамка |
| Scroll | рамка окна `nuiScroll` (в GTK — 0) | 46 × 46, как у GTK (полосы прокрутки) |

Естественный размер `Scroll` — по ребёнку, его считает движок. Движок раскладывает ребёнка прокрутки так, будто полосы оверлейные, как в GTK. Полосы Win32 занимают место, поэтому после `Arrange` драйвер решает, нужны ли они (ребёнок шире или выше видимой области), и при необходимости заново вызывает `Arrange` для ребёнка в уменьшенной области. Координаты детей прокрутки он пересчитывает от её клиентской области со сдвигом на позицию прокрутки.

## 7. DPI и шрифты

- Per-Monitor DPI v2: через манифест (§8) или в `Init` вызовом `SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)`.
- Физические пиксели = `round(логические × dpi / 96)`, `dpi = GetDpiForWindow(hwnd)`. Логгер считал `LOGPIXELSY` один раз при старте. Для библиотеки этого мало: окно может переехать на монитор с другим масштабом.
- Шрифт: `SystemParametersInfoForDpi(SPI_GETNONCLIENTMETRICS, …, dpi)` → `lfMessageFont` (Segoe UI) и его жирный вариант (логгер).
- `WM_DPICHANGED`:
  1. пересоздать шрифты под новый dpi;
  2. разослать `WM_SETFONT`;
  3. заново измерить листья и пересчитать раскладку;
  4. `SetWindowPos` в предложенный системой прямоугольник (`lParam`).
- `WM_GETMINMAXINFO`: минимальный размер окна = минимум раскладки + рамка (`AdjustWindowRectExForDpi`).

## 8. Манифест и ресурсы

- Обязательно: зависимость от **Common Controls v6** (визуальные стили, `TaskDialog`, `EM_SETCUEBANNER`), `dpiAwareness = PerMonitorV2`. Исходник для старта — `tools/serial_logger/app.manifest` и `app.rc` эталонного логгера.
- gc: `x86_64-w64-mingw32-windres app.rc -O coff -o rsrc_windows_amd64.syso` в каталоге **приложения**; Go подхватывает `.syso` автоматически. TinyGo — тот же объект как `rsrc.o` через `#cgo LDFLAGS: -L. -l:rsrc.o` в файле приложения с тегом `tinygo` (§1). Манифест — свойство приложения, а не библиотеки. В `examples/*` лежит готовый `.rc`, а nui даёт шаблон.
- Если у приложения нет манифеста, есть запасной путь в `Init`: `CreateActCtxW` с манифестом из ресурса 124 `shell32.dll` + `ActivateActCtx`. Под Wine 8.0 контекст создаётся, но comctl32 v6 не включает (S4). На настоящей Windows его ещё нужно проверить вручную, поэтому основной путь — манифест в exe.
- Контекст действует только на то, что загружается **после** `Init`, а статические импорты разрешаются раньше. Поэтому comctl32 не импортируется вовсе: `InitCommonControlsEx` и `TaskDialog` драйвер ищет через `LoadLibraryW` + `GetProcAddress` (ADR-16). Иначе без манифеста загрузилась бы v5, а exe со статическим импортом `TaskDialog` и вовсе не запустился бы.

## 9. Диалоги и оболочка

| nui | Win32 |
|---|---|
| Message / Alert / Ask | `TaskDialogIndirect` (comctl32 v6): `pszMainInstruction = title`, `pszContent = text`, заголовок окна — имя приложения, `TDF_ALLOW_DIALOG_CANCELLATION` — Esc и крестик закрывают и вопрос (ответ «нет», как на GTK), `TDF_POSITION_RELATIVE_TO_WINDOW`. Функция ищется в `Init`: `LoadLibraryW(L"comctl32.dll")` + `GetProcAddress` (ADR-16). Статический импорт не годится: без манифеста exe не запустится. Не нашлась — `MessageBoxW(caption = title)` (логгер). Значки: INFORMATION / ERROR / без значка с кнопками YES/NO. Если родителя уничтожили, пока диалог открыт, — ответ «нет», второй диалог не открывается |
| OpenFile / SaveFile | `GetOpenFileNameW` / `GetSaveFileNameW`, `OFN_EXPLORER \| OFN_FILEMUSTEXIST` / `OFN_OVERWRITEPROMPT`. Фильтр — строка `"Логи\0*.log;*.txt\0\0"` |
| ChooseFolder | `SHBrowseForFolderW` (`BIF_NEWDIALOGSTYLE \| BIF_RETURNONLYFSDIRS`) → `SHGetPathFromIDListW` → `CoTaskMemFree`. Для этого `Init` вызывает `CoInitializeEx(NULL, COINIT_APARTMENTTHREADED)` |
| OpenURI | `ShellExecuteW(NULL, L"open", uri, …)`; результат > 32 — успех |
| ShowInFolder | `ShellExecuteW(…, L"explorer.exe", L"/select,\"путь\"", …)` (логгер, `open_folder`) |
| SetClipboard / Clipboard | `nui_clip_set` / `nui_clip_get`: `OpenClipboard(msgWnd)` (до 10 попыток через 10 мс: буфер может держать другая программа) → `EmptyClipboard` + `SetClipboardData(CF_UNICODETEXT, GlobalAlloc(GMEM_MOVEABLE))` / `GetClipboardData` + `GlobalLock` → `CloseClipboard`. После успешного `SetClipboardData` память принадлежит системе. `\n` ↔ `\r\n` переводит Go-часть |

## 10. Отличия от Linux (документированные расхождения)

| Что | Windows |
|---|---|
| `Watch` | отсутствует (unix-only API) |
| `Progress.SetText` | не отображается |
| `EllipsisStart` | работает как `EllipsisMiddle` (`SS_PATHELLIPSIS`) |
| Тёмная тема | стандартные контролы Win32 её не поддерживают; окна светлые |
| `SetIconName` | ищет иконку в ресурсах exe по имени, иначе берёт иконку приложения (ресурс 1, как в логгере) |
| Положение окна | центр экрана работает (на Wayland его выбирает композитор) |
| `TextView.OnChange` | замена выделенного текста — один вызов, а не два (Windows шлёт один `EN_CHANGE`) |
| `Label.SetSelectable` | read-only EDIT без рамки: многоточие (`SetEllipsis`) у выделяемой подписи не рисуется |
| Цветная кнопка (`Button.SetColor`, `SetBackground`) | `BS_OWNERDRAW`, рисует драйвер: плоская, без визуального стиля системы (ADR-19). Фон Box и Grid — `WM_ERASEBKGND` окна; подписи, флажки, ползунки берут его через `WM_CTLCOLORSTATIC` |

## 11. Порядок работ в фазе 5

1. **S4** (1–2 дня): hello на Win32 под gc и TinyGo. Проверяем: `.syso`, подсистему GUI, `//export`-WndProc, размер. **Пройден** ([spike/s4/RESULT.md](../spike/s4/RESULT.md)): под Wine — оба тулчейна; настоящая Windows — вручную.
2. `internal/layout` и golden-тесты (они уже могут быть готовы: движок — чистый Go, его можно писать параллельно).
3. Окно, цикл, `Do`, таймеры, Label/Button/Box.
4. Остальные контролы, DPI, диалоги.
5. `examples/form` и `examples/logger` (в Windows-версии логгера порт читает горутина + `Do`, а сборка — gc или TinyGo `threads`).
