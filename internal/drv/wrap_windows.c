// wrap_windows.c — то, что удобнее написать на C (docs/driver-win32.md): регистрация
// класса с указателем на трамплин nuiWndProc, цикл сообщений, шрифты под DPI, измерение
// текста, рамка окна, диалоги, Append у многострочного поля. Логика — в Go.
#include "win32_windows.h"

#define RES(id) ((LPCWSTR)(UINT_PTR)(WORD)(id))

static HINSTANCE inst;
static HWND msgWnd;  // служебное окно (HWND_MESSAGE): Do, таймеры, Enter
static HWND parking; // скрытый родитель контролов, ещё не вставленных в окно
static nui_TaskDialogIndirect_fn taskDialog;
static int dlgKind; // открытый модальный диалог — для тестов (sim_windows.go)

// Запасной путь для exe без манифеста (driver-win32.md, §8): контекст активации из
// манифеста shell32.dll (ресурс 124, Common Controls v6) — контекст процесса по умолчанию.
static void actctx_fallback(void) {
	if (FindResourceW(0, RES(1), RES(24))) // RT_MANIFEST: у exe свой манифест
		return;
	WCHAR dir[260];
	if (!GetSystemDirectoryW(dir, 260))
		return;
	ACTCTXW act = {0};
	act.cbSize = sizeof act;
	act.dwFlags = NUI_ACTCTX_FLAG_RESOURCE_NAME_VALID | NUI_ACTCTX_FLAG_SET_PROCESS_DEFAULT |
		NUI_ACTCTX_FLAG_ASSEMBLY_DIRECTORY_VALID;
	act.lpSource = L"shell32.dll";
	act.lpAssemblyDirectory = dir;
	act.lpResourceName = RES(124);
	HANDLE h = CreateActCtxW(&act);
	ULONG_PTR cookie;
	if (h != (HANDLE)(LONG_PTR)-1)
		ActivateActCtx(h, &cookie);
}

// comctl32 только динамически (ADR-16): статический импорт загрузил бы её до контекста
// активации, и у exe без манифеста осталась бы v5.
static void comctl_init(void) {
	HMODULE m = LoadLibraryW(L"comctl32.dll");
	if (!m)
		return;
	nui_InitCommonControlsEx_fn init =
		(nui_InitCommonControlsEx_fn)(void (*)(void))GetProcAddress(m, "InitCommonControlsEx");
	if (init) {
		INITCOMMONCONTROLSEX icc = {sizeof icc,
			NUI_ICC_STANDARD_CLASSES | NUI_ICC_BAR_CLASSES | NUI_ICC_PROGRESS_CLASS | NUI_ICC_LISTVIEW_CLASSES};
		init(&icc);
	}
	taskDialog = (nui_TaskDialogIndirect_fn)(void (*)(void))GetProcAddress(m, "TaskDialogIndirect");
}

int nui_init(void) {
	inst = GetModuleHandleW(0);
	// Per-Monitor v2 (§7). Если режим уже задал манифест, вызов просто вернёт ошибку.
	SetProcessDpiAwarenessContext((DPI_AWARENESS_CONTEXT)(LONG_PTR)-4);
	actctx_fallback();
	comctl_init();
	CoInitializeEx(0, NUI_COINIT_APARTMENTTHREADED); // SHBrowseForFolderW (§9)
	WNDCLASSEXW wc = {0};
	wc.cbSize = sizeof wc;
	wc.lpfnWndProc = nuiWndProc;
	wc.hInstance = inst;
	wc.hCursor = LoadCursorW(0, RES(32512)); // IDC_ARROW
	wc.hIcon = LoadIconW(inst, RES(1));      // иконка приложения, если она есть в ресурсах
	wc.hbrBackground = GetSysColorBrush(NUI_COLOR_BTNFACE);
	wc.lpszClassName = L"nuiWindow";
	if (!RegisterClassExW(&wc))
		return 0;
	msgWnd = CreateWindowExW(0, L"nuiWindow", L"", 0, 0, 0, 0, 0, (HWND)(LONG_PTR)-3, 0, inst, 0);
	parking = CreateWindowExW(0, L"nuiWindow", L"", NUI_WS_POPUP, 0, 0, 0, 0, 0, 0, inst, 0);
	if (!msgWnd || !parking)
		return 0;
	PostMessageW(msgWnd, NUI_WM_DO, 0, 0); // Do, вызванный до Run, выполнится в начале цикла
	return 1;
}

HWND nui_msg_window(void) { return msgWnd; }
HWND nui_parking(void) { return parking; }

// Цикл (§3): Enter в поле с OnEnter — до IsDialogMessage; клавиатура — IsDialogMessage
// окна верхнего уровня (Tab, Shift+Tab, Enter = кнопка по умолчанию); остальное — как обычно.
void nui_loop(void) {
	MSG m;
	while (GetMessageW(&m, 0, 0, 0) > 0) {
		if (m.message >= NUI_WM_KEYFIRST && m.message <= NUI_WM_KEYLAST) {
			if (m.message == NUI_WM_KEYDOWN && m.wParam == NUI_VK_RETURN && GetPropW(m.hwnd, L"nuiEnter")) {
				SendMessageW(msgWnd, NUI_WM_ENTER, 0, (LPARAM)m.hwnd);
				continue;
			}
			HWND root = GetAncestor(m.hwnd, NUI_GA_ROOT);
			if (root && IsDialogMessageW(root, &m))
				continue;
		}
		TranslateMessage(&m);
		DispatchMessageW(&m);
	}
}

HWND nui_window_new(LPCWSTR title, DWORD style, LONG_PTR node) {
	HWND h = CreateWindowExW(NUI_WS_EX_CONTROLPARENT, L"nuiWindow", title, style, NUI_CW_USEDEFAULT,
		NUI_CW_USEDEFAULT, NUI_CW_USEDEFAULT, NUI_CW_USEDEFAULT, 0, 0, inst, 0);
	if (h)
		SetWindowLongPtrW(h, NUI_GWLP_USERDATA, node);
	return h;
}

HWND nui_control(LPCWSTR cls, LPCWSTR text, DWORD style, DWORD exstyle, HWND parent, LONG_PTR node) {
	HWND h = CreateWindowExW(exstyle, cls, text, NUI_WS_CHILD | style, 0, 0, 0, 0, parent ? parent : parking,
		0, inst, 0);
	if (h)
		SetWindowLongPtrW(h, NUI_GWLP_USERDATA, node);
	return h;
}

// Прокрутка — своё дочернее окно того же класса: уведомления её детей идут в nuiWndProc.
HWND nui_scroll_new(HWND parent, LONG_PTR node) {
	return nui_control(L"nuiWindow", L"", NUI_WS_VISIBLE | NUI_WS_VSCROLL | NUI_WS_HSCROLL,
		NUI_WS_EX_CONTROLPARENT, parent, node);
}

// Шрифты под dpi (§7): обычный и жирный — lfMessageFont (Segoe UI), моноширинный — Consolas
// того же размера. Если SystemParametersInfoForDpi не сработал — Segoe UI 9 пт.
void nui_fonts(UINT dpi, HFONT out[3]) {
	NONCLIENTMETRICSW ncm = {0};
	ncm.cbSize = sizeof ncm;
	LOGFONTW lf;
	if (SystemParametersInfoForDpi(NUI_SPI_GETNONCLIENTMETRICS, sizeof ncm, &ncm, 0, dpi)) {
		lf = ncm.lfMessageFont;
	} else {
		static const WCHAR segoe[] = L"Segoe UI";
		lf = (LOGFONTW){0};
		lf.lfHeight = -(LONG)((9 * dpi + 36) / 72);
		lf.lfWeight = NUI_FW_NORMAL;
		for (int i = 0; segoe[i]; i++)
			lf.lfFaceName[i] = segoe[i];
	}
	out[0] = CreateFontIndirectW(&lf);
	LOGFONTW bold = lf;
	bold.lfWeight = NUI_FW_BOLD;
	out[1] = CreateFontIndirectW(&bold);
	static const WCHAR consolas[] = L"Consolas";
	LOGFONTW mono = lf;
	for (int i = 0; i < 32; i++)
		mono.lfFaceName[i] = i < 9 ? consolas[i] : 0;
	mono.lfPitchAndFamily = 0x31; // FIXED_PITCH | FF_MODERN: без Consolas — другой моноширинный
	out[2] = CreateFontIndirectW(&mono);
}

// Средняя ширина символа и высота строки шрифта.
void nui_metrics(HWND hwnd, HFONT font, int out[2]) {
	HDC dc = GetDC(hwnd);
	HGDIOBJ old = SelectObject(dc, font);
	TEXTMETRICW tm;
	GetTextMetricsW(dc, &tm);
	out[0] = tm.tmAveCharWidth;
	out[1] = tm.tmHeight;
	SelectObject(dc, old);
	ReleaseDC(hwnd, dc);
}

// Размер текста: без переноса — по самой длинной строке (\n переносит), с переносом —
// высота при ширине wrapWidth. Пустой текст — ширина 0 и высота строки.
void nui_text_size(HWND hwnd, HFONT font, LPCWSTR text, int len, int wrapWidth, int out[2]) {
	HDC dc = GetDC(hwnd);
	HGDIOBJ old = SelectObject(dc, font);
	if (len == 0) {
		TEXTMETRICW tm;
		GetTextMetricsW(dc, &tm);
		out[0] = 0;
		out[1] = tm.tmHeight;
	} else {
		RECT r = {0, 0, wrapWidth > 0 ? wrapWidth : 0, 0};
		UINT f = NUI_DT_CALCRECT | NUI_DT_NOPREFIX | NUI_DT_EXPANDTABS | (wrapWidth > 0 ? NUI_DT_WORDBREAK : 0);
		DrawTextW(dc, text, len, &r, f);
		out[0] = r.right - r.left;
		out[1] = r.bottom - r.top;
	}
	SelectObject(dc, old);
	ReleaseDC(hwnd, dc);
}

// Идеальный размер кнопки и флажка по стилю системы (BCM_GETIDEALSIZE, comctl32 v6).
void nui_ideal_size(HWND button, int out[2]) {
	SIZE s = {0, 0};
	SendMessageW(button, NUI_BCM_GETIDEALSIZE, 0, (LPARAM)&s);
	out[0] = s.cx;
	out[1] = s.cy;
}

// Рамка окна для клиентской области cw × ch при его dpi; center — по центру рабочей
// области монитора. out: x, y, ширина, высота окна.
void nui_window_frame(HWND hwnd, int cw, int ch, int center, int out[4]) {
	DWORD style = (DWORD)GetWindowLongPtrW(hwnd, NUI_GWL_STYLE);
	RECT r = {0, 0, cw, ch};
	AdjustWindowRectExForDpi(&r, style, 0, NUI_WS_EX_CONTROLPARENT, GetDpiForWindow(hwnd));
	int w = r.right - r.left, h = r.bottom - r.top;
	RECT wr;
	GetWindowRect(hwnd, &wr);
	int x = wr.left, y = wr.top;
	if (center) {
		MONITORINFO mi = {0};
		mi.cbSize = sizeof mi;
		GetMonitorInfoW(MonitorFromWindow(hwnd, NUI_MONITOR_DEFAULTTONEAREST), &mi);
		// Начальный размер — не больше рабочей области монитора (на Linux это делает
		// оконный менеджер); меньше минимума окно не станет — его держит WM_GETMINMAXINFO.
		if (w > mi.rcWork.right - mi.rcWork.left)
			w = mi.rcWork.right - mi.rcWork.left;
		if (h > mi.rcWork.bottom - mi.rcWork.top)
			h = mi.rcWork.bottom - mi.rcWork.top;
		x = mi.rcWork.left + (mi.rcWork.right - mi.rcWork.left - w) / 2;
		y = mi.rcWork.top + (mi.rcWork.bottom - mi.rcWork.top - h) / 2;
		if (y < mi.rcWork.top)
			y = mi.rcWork.top;
	}
	out[0] = x;
	out[1] = y;
	out[2] = w;
	out[3] = h;
}

// Прямоугольник монитора окна целиком (для полноэкранного режима): x, y, ширина, высота.
void nui_monitor_rect(HWND hwnd, int out[4]) {
	MONITORINFO mi = {0};
	mi.cbSize = sizeof mi;
	GetMonitorInfoW(MonitorFromWindow(hwnd, NUI_MONITOR_DEFAULTTONEAREST), &mi);
	out[0] = mi.rcMonitor.left;
	out[1] = mi.rcMonitor.top;
	out[2] = mi.rcMonitor.right - mi.rcMonitor.left;
	out[3] = mi.rcMonitor.bottom - mi.rcMonitor.top;
}

// Шрифт подписи с SetFontScale: шрифт окна base в pct процентов, жирный по bold.
HFONT nui_font_scaled(HFONT base, int pct, int bold) {
	LOGFONTW lf;
	if (!GetObjectW(base, sizeof lf, &lf))
		return 0;
	lf.lfHeight = lf.lfHeight * pct / 100;
	lf.lfWeight = bold ? NUI_FW_BOLD : NUI_FW_NORMAL;
	return CreateFontIndirectW(&lf);
}

void nui_fill(HDC dc, int x, int y, int w, int h, COLORREF c) {
	RECT r = {x, y, x + w, y + h};
	HBRUSH b = CreateSolidBrush(c);
	FillRect(dc, &r, b);
	DeleteObject(b);
}

static COLORREF shade(COLORREF c, int pct) { // pct < 100 — темнее
	unsigned r = (c & 255) * pct / 100, g = ((c >> 8) & 255) * pct / 100, b = ((c >> 16) & 255) * pct / 100;
	return (r > 255 ? 255 : r) | (g > 255 ? 255 : g) << 8 | (b > 255 ? 255 : b) << 16;
}

// Цветная кнопка (SetBackground, SetColor): стандартную кнопку Windows перекрасить нельзя,
// поэтому BS_OWNERDRAW — плоская заливка, рамка темнее заливки (у кнопки по умолчанию —
// двойная), текст по центру; нажатая — темнее и сдвинута, недоступная — серый текст.
void nui_draw_button(const DRAWITEMSTRUCT *d, unsigned fg, unsigned bg, int isDefault) {
	RECT r = d->rcItem;
	int pressed = d->itemState & NUI_ODS_SELECTED;
	COLORREF face = bg == ~0u ? GetSysColor(NUI_COLOR_BTNFACE) : bg;
	if (pressed)
		face = shade(face, 85);
	HBRUSH b = CreateSolidBrush(face);
	FillRect(d->hDC, &r, b);
	DeleteObject(b);
	HBRUSH e = CreateSolidBrush(shade(face, 65));
	FrameRect(d->hDC, &r, e);
	if (isDefault) {
		RECT in = {r.left + 1, r.top + 1, r.right - 1, r.bottom - 1};
		FrameRect(d->hDC, &in, e);
	}
	DeleteObject(e);
	WCHAR t[256];
	int n = GetWindowTextW(d->hwndItem, t, 256);
	HGDIOBJ old = SelectObject(d->hDC, (HGDIOBJ)SendMessageW(d->hwndItem, NUI_WM_GETFONT, 0, 0));
	SetBkMode(d->hDC, NUI_TRANSPARENT);
	SetTextColor(d->hDC, d->itemState & NUI_ODS_DISABLED ? GetSysColor(NUI_COLOR_GRAYTEXT)
		: fg == ~0u ? GetSysColor(NUI_COLOR_BTNTEXT) : fg);
	RECT tr = r;
	if (pressed) {
		tr.left += 1;
		tr.top += 1;
		tr.right += 1;
		tr.bottom += 1;
	}
	DrawTextW(d->hDC, t, n, &tr, NUI_DT_CENTER | NUI_DT_VCENTER | NUI_DT_SINGLELINE);
	SelectObject(d->hDC, old);
	if ((d->itemState & NUI_ODS_FOCUS) && !(d->itemState & NUI_ODS_NOFOCUSRECT)) {
		RECT f = {r.left + 3, r.top + 3, r.right - 3, r.bottom - 3};
		DrawFocusRect(d->hDC, &f);
	}
}

void nui_client_size(HWND hwnd, int out[2]) {
	RECT r = {0, 0, 0, 0};
	GetClientRect(hwnd, &r);
	out[0] = r.right - r.left;
	out[1] = r.bottom - r.top;
}

// Сообщение (§9): TaskDialogIndirect из comctl32 v6, если нашлась, иначе MessageBoxW.
// kind: 0 — информация, 1 — ошибка, 2 — вопрос «Да/Нет». 1 — OK или «Да».
// Esc и крестик закрывают и вопрос (TDF_ALLOW_DIALOG_CANCELLATION) — ответ «нет», как на GTK.
// Если родителя уничтожили, пока диалог открыт, TaskDialogIndirect вернётся с ошибкой или
// без нажатой кнопки — это «нет»; второй диалог (MessageBoxW) тогда не открывается.
int nui_message(HWND parent, int kind, LPCWSTR app, LPCWSTR title, LPCWSTR text) {
	int r = 0;
	dlgKind = kind == 2 ? 2 : 1;
	if (taskDialog) {
		TASKDIALOGCONFIG c = {0};
		c.cbSize = sizeof c;
		c.hwndParent = parent;
		c.hInstance = inst;
		c.dwFlags = NUI_TDF_ALLOW_DIALOG_CANCELLATION | (parent ? NUI_TDF_POSITION_RELATIVE_TO_WINDOW : 0);
		c.dwCommonButtons = kind == 2 ? NUI_TDCBF_YES_BUTTON | NUI_TDCBF_NO_BUTTON : NUI_TDCBF_OK_BUTTON;
		c.pszWindowTitle = app;
		c.pszMainIcon = kind == 0 ? RES(-3) : kind == 1 ? RES(-2) : 0; // TD_INFORMATION_ICON, TD_ERROR_ICON
		c.pszMainInstruction = title;
		c.pszContent = text;
		int pressed = 0;
		if (taskDialog(&c, &pressed, 0, 0) >= 0)
			r = pressed == NUI_IDOK || pressed == NUI_IDYES;
	} else {
		UINT t = kind == 2 ? NUI_MB_YESNO | NUI_MB_ICONQUESTION : NUI_MB_OK | (kind == 1 ? NUI_MB_ICONERROR : NUI_MB_ICONINFORMATION);
		int b = MessageBoxW(parent, text, title, t);
		r = b == NUI_IDOK || b == NUI_IDYES;
	}
	dlgKind = 0;
	return r;
}

// Открыть/сохранить (§9). filter — пары «имя\0шаблоны\0», в конце ещё \0. buf — на входе
// предлагаемое имя, на выходе путь.
// mode — FileOpen, FileSave или FileOpenMany (contract.go). При выборе нескольких файлов
// в buf «каталог\0имя1\0имя2\0\0», одного — полный путь.
int nui_file_dialog(HWND parent, int mode, LPCWSTR title, LPCWSTR filter, LPWSTR buf, int cap) {
	OPENFILENAMEW o = {0};
	o.lStructSize = sizeof o;
	o.hwndOwner = parent;
	o.lpstrFilter = filter;
	o.nFilterIndex = 1;
	o.lpstrFile = buf;
	o.nMaxFile = (DWORD)cap;
	o.lpstrTitle = title;
	o.Flags = NUI_OFN_EXPLORER | NUI_OFN_NOCHANGEDIR | NUI_OFN_HIDEREADONLY | NUI_OFN_PATHMUSTEXIST;
	dlgKind = 3;
	int ok;
	if (mode == 3)
		o.Flags |= NUI_OFN_ALLOWMULTISELECT;
	if (mode == 1) {
		o.Flags |= NUI_OFN_OVERWRITEPROMPT;
		ok = GetSaveFileNameW(&o);
	} else {
		o.Flags |= NUI_OFN_FILEMUSTEXIST;
		ok = GetOpenFileNameW(&o);
	}
	dlgKind = 0;
	return ok;
}

// Выбор папки (§9). buf — не меньше MAX_PATH (260) символов.
int nui_folder_dialog(HWND parent, LPCWSTR title, LPWSTR buf) {
	BROWSEINFOW bi = {0};
	bi.hwndOwner = parent;
	bi.pszDisplayName = buf;
	bi.lpszTitle = title;
	bi.ulFlags = NUI_BIF_RETURNONLYFSDIRS | NUI_BIF_NEWDIALOGSTYLE;
	dlgKind = 4;
	PIDLIST_ABSOLUTE id = SHBrowseForFolderW(&bi);
	dlgKind = 0;
	if (!id)
		return 0;
	int ok = SHGetPathFromIDListW(id, buf);
	CoTaskMemFree(id);
	return ok;
}

// Строк в поле: последняя пустая строка после завершающего перевода не считается
// (api.md, §13 — как на GTK).
static int text_lines(HWND e) {
	int n = (int)SendMessageW(e, NUI_EM_GETLINECOUNT, 0, 0);
	if (n > 0 && (int)SendMessageW(e, NUI_EM_LINEINDEX, (WPARAM)(n - 1), 0) == GetWindowTextLengthW(e))
		n--;
	return n;
}

// Append (§5, TextView): дописать в конец, не сбивая выделение и вид пользователя;
// MaxLines удаляет начало; AutoScroll — прокрутка в конец. s — уже с \r\n.
void nui_text_append(HWND e, LPCWSTR s, int autoscroll, int maxLines) {
	DWORD a = 0, b = 0;
	SendMessageW(e, NUI_EM_GETSEL, (WPARAM)&a, (LPARAM)&b);
	int first = (int)SendMessageW(e, NUI_EM_GETFIRSTVISIBLELINE, 0, 0);
	int len = GetWindowTextLengthW(e);
	SendMessageW(e, NUI_EM_SETSEL, (WPARAM)len, (LPARAM)len);
	SendMessageW(e, NUI_EM_REPLACESEL, 0, (LPARAM)s);
	int cut = 0, cutLines = 0;
	if (maxLines > 0) {
		int extra = text_lines(e) - maxLines;
		if (extra > 0) {
			cut = (int)SendMessageW(e, NUI_EM_LINEINDEX, (WPARAM)extra, 0);
			cutLines = extra;
			SendMessageW(e, NUI_EM_SETSEL, 0, (LPARAM)cut);
			SendMessageW(e, NUI_EM_REPLACESEL, 0, (LPARAM)L"");
		}
	}
	a = (int)a > cut ? a - (DWORD)cut : 0;
	b = (int)b > cut ? b - (DWORD)cut : 0;
	SendMessageW(e, NUI_EM_SETSEL, a, b);
	if (autoscroll) {
		SendMessageW(e, NUI_WM_VSCROLL, NUI_SB_BOTTOM, 0);
		return;
	}
	// EM_REPLACESEL прокручивает к каретке — возвращаем вид пользователя.
	int want = first - cutLines;
	if (want < 0)
		want = 0;
	int now = (int)SendMessageW(e, NUI_EM_GETFIRSTVISIBLELINE, 0, 0);
	if (now != want)
		SendMessageW(e, NUI_EM_LINESCROLL, 0, (LPARAM)(want - now));
}

// Подсказки (SetTooltip): один tooltips_class32 на окно, контрол — «инструмент» по своему
// HWND (TTF_IDISHWND | TTF_SUBCLASS: наведение мыши отслеживает сам tooltip).
HWND nui_tooltip_new(HWND owner) {
	HWND t = CreateWindowExW(NUI_WS_EX_TOPMOST, L"tooltips_class32", 0,
		NUI_WS_POPUP | NUI_TTS_ALWAYSTIP | NUI_TTS_NOPREFIX, NUI_CW_USEDEFAULT, NUI_CW_USEDEFAULT,
		NUI_CW_USEDEFAULT, NUI_CW_USEDEFAULT, owner, 0, inst, 0);
	if (t)
		SendMessageW(t, NUI_TTM_SETMAXTIPWIDTH, 0, 400); // длинный текст переносится
	return t;
}

void nui_tooltip_set(HWND tip, HWND ctl, LPCWSTR text) {
	TOOLINFOW ti = {0};
	ti.cbSize = 64; // TTTOOLINFOW_V2_SIZE: годится и для comctl32 v5 (без манифеста)
	ti.uFlags = NUI_TTF_IDISHWND | NUI_TTF_SUBCLASS;
	ti.hwnd = GetParent(ctl);
	ti.uId = (UINT_PTR)ctl;
	SendMessageW(tip, NUI_TTM_DELTOOLW, 0, (LPARAM)&ti);
	if (text) {
		ti.lpszText = (LPWSTR)text;
		SendMessageW(tip, NUI_TTM_ADDTOOLW, 0, (LPARAM)&ti);
	}
}

// Буфер обмена (CF_UNICODETEXT). Его может держать открытым другая программа —
// несколько попыток с паузой.
static int clip_open(void) {
	for (int i = 0; i < 10; i++) {
		if (OpenClipboard(msgWnd))
			return 1;
		Sleep(10);
	}
	return 0;
}

int nui_clip_set(LPCWSTR s, int len) {
	HANDLE m = GlobalAlloc(NUI_GMEM_MOVEABLE, (SIZE_T)(len + 1) * sizeof(WCHAR));
	if (!m)
		return 0;
	WCHAR *p = GlobalLock(m);
	for (int i = 0; i < len; i++)
		p[i] = s[i];
	p[len] = 0;
	GlobalUnlock(m);
	if (!clip_open()) {
		GlobalFree(m);
		return 0;
	}
	EmptyClipboard();
	int ok = SetClipboardData(NUI_CF_UNICODETEXT, m) != 0; // память теперь принадлежит системе
	if (!ok)
		GlobalFree(m);
	CloseClipboard();
	return ok;
}

int nui_clip_get(LPWSTR buf, int cap) {
	if (!clip_open())
		return 0;
	int n = 0;
	HANDLE m = GetClipboardData(NUI_CF_UNICODETEXT);
	const WCHAR *p = m ? GlobalLock(m) : 0;
	if (p) {
		while (p[n])
			n++;
		if (buf)
			for (int i = 0; i < n && i < cap; i++)
				buf[i] = p[i];
		GlobalUnlock(m);
	}
	CloseClipboard();
	return n;
}

// --- Картинки (Image, driver-win32.md, §5) ---
// GDI+ (gdiplus.dll) грузится при первой картинке: программы без Image её не трогают.
// GdiplusShutdown не нужен — библиотека живёт до конца процесса.
static struct {
	int state; // 0 — не загружали, 1 — готов, -1 — нет GDI+
	nui_GdipCreateBitmapFromStream_fn fromStream;
	nui_GdipCreateBitmapFromScan0_fn fromScan0;
	nui_GdipDisposeImage_fn dispose;
	nui_GdipGetImageWidth_fn width;
	nui_GdipGetImageHeight_fn height;
	nui_GdipImageRotateFlip_fn rotateFlip;
	nui_GdipGetImageGraphicsContext_fn graphics;
	nui_GdipSetInterpolationMode_fn interpolation;
	nui_GdipSetPixelOffsetMode_fn pixelOffset;
	nui_GdipDrawImageRectRectI_fn draw;
	nui_GdipDeleteGraphics_fn deleteGraphics;
	nui_GdipCreateImageAttributes_fn newAttrs;
	nui_GdipSetImageAttributesWrapMode_fn wrapMode;
	nui_GdipDisposeImageAttributes_fn disposeAttrs;
} gp;

#define GP_LOAD(field, name) \
	if (!(gp.field = (nui_##name##_fn)(void (*)(void))GetProcAddress(m, #name))) \
		return 0;

static int gp_load(void) {
	HMODULE m = LoadLibraryW(L"gdiplus.dll");
	if (!m)
		return 0;
	nui_GdiplusStartup_fn startup = (nui_GdiplusStartup_fn)(void (*)(void))GetProcAddress(m, "GdiplusStartup");
	GP_LOAD(fromStream, GdipCreateBitmapFromStream)
	GP_LOAD(fromScan0, GdipCreateBitmapFromScan0)
	GP_LOAD(dispose, GdipDisposeImage)
	GP_LOAD(width, GdipGetImageWidth)
	GP_LOAD(height, GdipGetImageHeight)
	GP_LOAD(rotateFlip, GdipImageRotateFlip)
	GP_LOAD(graphics, GdipGetImageGraphicsContext)
	GP_LOAD(interpolation, GdipSetInterpolationMode)
	GP_LOAD(pixelOffset, GdipSetPixelOffsetMode)
	GP_LOAD(draw, GdipDrawImageRectRectI)
	GP_LOAD(deleteGraphics, GdipDeleteGraphics)
	GP_LOAD(newAttrs, GdipCreateImageAttributes)
	GP_LOAD(wrapMode, GdipSetImageAttributesWrapMode)
	GP_LOAD(disposeAttrs, GdipDisposeImageAttributes)
	GdiplusStartupInput in = {1, 0, 0, 0};
	ULONG_PTR token;
	return startup && startup(&token, &in, 0) == 0;
}

static int gp_ready(void) {
	if (!gp.state)
		gp.state = gp_load() ? 1 : -1;
	return gp.state > 0;
}

// Декодированная картинка. GDI+ читает поток лениво, поэтому поток живёт вместе с ней.
typedef struct {
	IStream *stream;
	GpBitmap *bmp;
	UINT w, h;
} NuiPicture;

void nui_image_free(void *p) {
	NuiPicture *pic = p;
	if (!pic)
		return;
	gp.dispose(pic->bmp);
	pic->stream->lpVtbl->Release(pic->stream);
	CoTaskMemFree(pic);
}

// orient — тег Orientation из EXIF (1…8; его читает Go, core.Orientation): GDI+ сам
// картинку не поворачивает, а Wine и не отдаёт этот тег.
void *nui_image_decode(const BYTE *data, int len, int orient, int wh[2]) {
	static const int rotateFlip[9] = {0, 0, 4, 2, 6, 5, 1, 7, 3}; // → RotateFlipType
	if (len <= 0 || !gp_ready())
		return 0;
	HGLOBAL mem = GlobalAlloc(NUI_GMEM_MOVEABLE, (SIZE_T)len);
	if (!mem)
		return 0;
	BYTE *p = GlobalLock(mem);
	for (int i = 0; i < len; i++)
		p[i] = data[i];
	GlobalUnlock(mem);
	NuiPicture *pic = CoTaskMemAlloc(sizeof *pic);
	if (!pic || CreateStreamOnHGlobal(mem, 1, &pic->stream) < 0) {
		GlobalFree(mem);
		CoTaskMemFree(pic);
		return 0;
	}
	if (gp.fromStream(pic->stream, &pic->bmp) != 0) {
		pic->stream->lpVtbl->Release(pic->stream); // освобождает и mem
		CoTaskMemFree(pic);
		return 0;
	}
	if (orient >= 2 && orient <= 8)
		gp.rotateFlip(pic->bmp, rotateFlip[orient]);
	gp.width(pic->bmp, &pic->w);
	gp.height(pic->bmp, &pic->h);
	if (!pic->w || !pic->h) {
		nui_image_free(pic);
		return 0;
	}
	wh[0] = (int)pic->w;
	wh[1] = (int)pic->h;
	return pic;
}

// Картинка w×h точек: DIB 32 бита сверху вниз с умноженной альфой (её ждёт AlphaBlend).
// Уменьшение — бикубическое высокого качества; WrapModeTileFlipXY убирает полупрозрачную
// кайму, которую GDI+ иначе рисует по краям.
HBITMAP nui_image_render(void *p, int w, int h) {
	NuiPicture *pic = p;
	BITMAPINFO bi = {{sizeof(BITMAPINFOHEADER), w, -h, 1, 32, NUI_BI_RGB, 0, 0, 0, 0, 0}, {{0, 0, 0, 0}}};
	void *bits = 0;
	HBITMAP dib = CreateDIBSection(0, &bi, NUI_DIB_RGB_COLORS, &bits, 0, 0);
	if (!dib)
		return 0;
	GpBitmap *dst = 0;
	GpGraphics *g = 0;
	GpImageAttributes *attrs = 0;
	if (gp.fromScan0(w, h, w * 4, NUI_PixelFormat32bppPARGB, bits, &dst) == 0) {
		if (gp.graphics(dst, &g) == 0) {
			gp.interpolation(g, NUI_InterpolationModeHighQualityBicubic);
			gp.pixelOffset(g, NUI_PixelOffsetModeHighQuality);
			if (gp.newAttrs(&attrs) == 0)
				gp.wrapMode(attrs, NUI_WrapModeTileFlipXY, 0, 0);
			gp.draw(g, pic->bmp, 0, 0, w, h, 0, 0, (INT)pic->w, (INT)pic->h, NUI_UnitPixel, attrs, 0, 0);
			if (attrs)
				gp.disposeAttrs(attrs);
			gp.deleteGraphics(g);
		}
		gp.dispose(dst);
	}
	GdiFlush();
	return dib;
}

// WM_DRAWITEM картинки: фон под ней (панель или окно), картинка — по центру места,
// как у GtkImage.
void nui_draw_image(const DRAWITEMSTRUCT *d, HBITMAP bmp, int w, int h, HBRUSH bg) {
	FillRect(d->hDC, &d->rcItem, bg);
	if (!bmp)
		return;
	int x = d->rcItem.left + (d->rcItem.right - d->rcItem.left - w) / 2;
	int y = d->rcItem.top + (d->rcItem.bottom - d->rcItem.top - h) / 2;
	HDC m = CreateCompatibleDC(d->hDC);
	HGDIOBJ old = SelectObject(m, bmp);
	BLENDFUNCTION bf = {NUI_AC_SRC_OVER, 0, 255, NUI_AC_SRC_ALPHA};
	GdiAlphaBlend(d->hDC, x, y, w, h, m, 0, 0, w, h, bf);
	SelectObject(m, old);
	DeleteDC(m);
}

// --- List (driver-win32.md, §5): ListView с LVS_OWNERDATA ---

void nui_list_column(HWND lv, int index, LPCWSTR title) {
	LVCOLUMNW c = {0};
	c.mask = NUI_LVCF_TEXT | NUI_LVCF_WIDTH | NUI_LVCF_SUBITEM;
	c.cx = 100;
	c.pszText = (LPWSTR)title;
	c.iSubItem = index;
	SendMessageW(lv, NUI_LVM_INSERTCOLUMNW, (WPARAM)index, (LPARAM)&c);
}

// Выбрать строку, как щелчком: снять выделение со всех (-1), выделить и сделать текущей
// строку row и прокрутить к ней. row < 0 — только снять.
void nui_list_select(HWND lv, int row) {
	LVITEMW it = {0};
	it.stateMask = NUI_LVIS_SELECTED;
	SendMessageW(lv, NUI_LVM_SETITEMSTATE, (WPARAM)-1, (LPARAM)&it);
	if (row < 0)
		return;
	it.stateMask = it.state = NUI_LVIS_SELECTED | NUI_LVIS_FOCUSED;
	SendMessageW(lv, NUI_LVM_SETITEMSTATE, (WPARAM)row, (LPARAM)&it);
	SendMessageW(lv, NUI_LVM_ENSUREVISIBLE, (WPARAM)row, 0);
}

#ifdef NUI_GUI_TEST
// Текст ячейки так, как его видит ListView: LVM_GETITEMTEXTW → LVN_GETDISPINFOW родителю.
int nui_list_item_text(HWND lv, int row, int col, LPWSTR buf, int cap) {
	LVITEMW it = {0};
	it.iSubItem = col;
	it.pszText = buf;
	it.cchTextMax = cap;
	return (int)SendMessageW(lv, NUI_LVM_GETITEMTEXTW, (WPARAM)row, (LPARAM)&it);
}
#endif

#ifdef NUI_GUI_TEST
// Помощники интеграционных тестов (sim_windows.go): открытый модальный диалог потока.
int nui_dialog_kind(void) { return dlgKind; }
int nui_task_dialog(void) { return taskDialog != 0; }

static BOOL find_dialog(HWND h, LPARAM out) {
	WCHAR cls[16];
	if (IsWindowVisible(h) && GetClassNameW(h, cls, 16) == 6 && cls[0] == '#' && cls[1] == '3' && cls[2] == '2' &&
		cls[3] == '7' && cls[4] == '7' && cls[5] == '0') {
		*(HWND *)out = h;
		return 0;
	}
	return 1;
}

HWND nui_find_dialog(void) {
	HWND h = 0;
	if (dlgKind)
		EnumThreadWindows(GetCurrentThreadId(), find_dialog, (LPARAM)&h);
	return h;
}
#endif
