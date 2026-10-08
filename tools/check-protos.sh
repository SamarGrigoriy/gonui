#!/bin/sh
# Сверка собственных прототипов и констант драйвера с настоящими заголовками GTK
# (docs/driver-gtk3.md, §10). Нужны libgtk-3-dev и pkg-config — только здесь, не для сборки.
# Ошибка в прототипе — неопределённое поведение без единого предупреждения компилятора.
set -eu
cd "$(dirname "$0")/.."
if ! pkg-config --exists gtk+-3.0; then
	echo "check-protos: нужен libgtk-3-dev (apt install libgtk-3-dev pkg-config)" >&2
	exit 2
fi
CFLAGS="-std=c11 -D_GNU_SOURCE -DNUI_CHECK -DNUI_GUI_TEST -Wall -Werror $(pkg-config --cflags gtk+-3.0)"

# 1. Каждый NUI_FN и NUI_CONST превращается в _Static_assert против настоящего объявления.
#    Исходник из stdin: «#include "x.h"» ищется сначала в текущем каталоге — поэтому cd.
(cd internal/drv && printf '#include <gtk/gtk.h>\n#include <glib-unix.h>\n#include <stdint.h>\n#include <string.h>\n#include <unistd.h>\n#include "gtk3_linux.h"\n' |
	cc $CFLAGS -x c -fsyntax-only -)

# 2. Обёртки (включая тестовые) компилируются против настоящих заголовков.
#    В режиме проверки NUI_FN ничего не объявляет, поэтому всё нужное — через -include.
cc $CFLAGS -I internal/drv -include gtk/gtk.h -include glib-unix.h -include stdint.h -include string.h \
	-fsyntax-only internal/drv/wrap_linux.c

echo "check-protos: ok"
