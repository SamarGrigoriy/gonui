# Сборка на Debian

## 1. Что нужно

| Назначение | Пакеты / инструменты | Debian 12 | Debian 13 |
|---|---|---|---|
| Запуск приложений | GTK3 (обычно уже стоит с десктопом) | `libgtk-3-0` | `libgtk-3-0t64` |
| Сборка gc | Go ≥ 1.22, gcc, make, git | `build-essential git` + Go с go.dev | `build-essential git golang` (1.24) или go.dev |
| Малая сборка | выпущенный TinyGo (проверено 0.42.0) + target-файл nui (§3) | — | — |
| Тесты GUI | Xvfb | `xvfb xauth` | то же |
| Проверка прототипов (CI) | заголовки GTK | `libgtk-3-dev pkg-config` | то же |
| Кросс-сборка под Windows (фаза 5) | mingw | `gcc-mingw-w64-x86-64` | то же |
| Необязательно | UPX | `upx-ucl` | `upx-ucl` |

> Пакеты `-dev` для **сборки** nui не нужны: прототипы свои, линковка по soname (ADR-3). `libgtk-3-dev` нужен только для `make check-protos` и генератора golden-файлов раскладки ([layout.md](layout.md), §8).

> **Релизные бинарники собираются на Debian 12** (контейнер `debian:bookworm`). Бинарник требует glibc не старее той, на которой собран: загрузчик сверяет версии символов `GLIBC_*`. Собранный на Debian 13 (glibc 2.41) может не запуститься на Debian 12 (2.36). Для разработки годится любая система; проверка релиза — `make abi` ([driver-gtk3.md](driver-gtk3.md), §9).

Go в apt Debian 12 — версии 1.19, для nui это слишком старая версия. Поставьте Go с go.dev:

```sh
wget https://go.dev/dl/go1.XX.Y.linux-amd64.tar.gz       # актуальная версия с go.dev/dl
sudo rm -rf /usr/local/go && sudo tar -C /usr/local -xzf go1.XX.Y.linux-amd64.tar.gz
echo 'export PATH=$PATH:/usr/local/go/bin:$HOME/go/bin' >> ~/.profile
```

## 2. Быстрый старт (gc)

```sh
git clone <репозиторий> nui && cd nui
make dev            # bin/hello, bin/form, bin/logger
./bin/hello
make test           # юнит-тесты + интеграционные под xvfb-run
```

## 3. TinyGo с glibc

Штатный TinyGo на Linux линкуется со своей статической musl, а с ней системный GTK загрузить нельзя ([decisions.md](decisions.md), ADR-4).

**Основной путь (ADR-17, спайк S3 пройден): выпущенный TinyGo + target-файл nui.** Всё, что делает [PR #5838](https://github.com/tinygo-org/tinygo/pull/5838) (флаг `-libc=glibc`, на 08.10.2026 не влит), задаёт файл `targets/linux-glibc-amd64.json`. Образец — [spike/s3/linux-glibc-amd64.json](../spike/s3/linux-glibc-amd64.json).

```sh
wget https://github.com/tinygo-org/tinygo/releases/download/v0.42.0/tinygo0.42.0.linux-amd64.tar.gz
# сверить sha256 с digest выпуска: GitHub API, поле assets[].digest
mkdir -p ~/.local && tar -xzf tinygo0.42.0.linux-amd64.tar.gz -C ~/.local   # → ~/.local/tinygo
export PATH=$PATH:$HOME/.local/tinygo/bin
tinygo version          # проверить: поддерживает ли он установленную версию Go
tinygo build -target=targets/linux-glibc-amd64.json -o bin/small/hello ./examples/hello
```

Нужен `libc6-dev`: C-файлам рантайма TinyGo нужны заголовки glibc. Он входит в `build-essential`.

Варианты А и Б ниже — запасные: на случай, если target-файл перестанет работать с новой версией TinyGo.

**Вариант А — PR уже влит и вошёл в релиз.** Ставим .deb из релизов TinyGo:

```sh
wget https://github.com/tinygo-org/tinygo/releases/download/vX.Y.Z/tinygo_X.Y.Z_amd64.deb
sudo dpkg -i tinygo_X.Y.Z_amd64.deb
tinygo version          # проверить: какие версии Go поддерживаются
tinygo help build | grep -- -libc
```

**Вариант Б — собрать TinyGo из ветки PR.** Действуйте по официальному руководству «Build from source» на tinygo.org. Ниже порядок действий; точные команды и требуемую версию LLVM берите из руководства на момент сборки.

```sh
git clone https://github.com/tinygo-org/tinygo && cd tinygo
git fetch origin pull/5838/head:libc-glibc && git checkout libc-glibc
git submodule update --init --recursive
# Вариант с системным LLVM (быстрее, чем собирать LLVM целиком):
sudo apt install llvm-NN-dev clang-NN libclang-NN-dev lld-NN   # NN — версия из руководства
go install -tags=llvmNN                                        # или make, как сказано в руководстве
tinygo version
```

**Вариант В** — основной путь выше (спайк S3).

TinyGo использует установленный Go (его стандартную библиотеку). Версия Go должна входить в диапазон, который печатает `tinygo version`.

## 4. Makefile

| Цель | Что делает |
|---|---|
| `make dev` | gc-сборка всех примеров в `bin/` (без `-s -w`, для отладки) |
| `make release-gc` | gc с релизными флагами ([size.md](size.md), §4) в `bin/gc/` |
| `make small` | TinyGo с релизными флагами в `bin/small/` |
| `make small-debug` | TinyGo с `-panic=print`, с отладочной информацией |
| `make test` | `go test ./internal/core/...` + `xvfb-run -a go test -tags gui ./...` |
| `make race` | то же с `-race` (только gc) |
| `make selftest` | запускает `bin/*/<пример> -selftest` под `xvfb-run` для обоих тулчейнов |
| `make deps` | `tools/depcheck.sh` — белый список импортов |
| `make size` | `tools/sizecheck.sh` — сверка с `size-budget.txt` |
| `make check-protos` | `tools/check-protos.sh` — прототипы против настоящих заголовков |
| `make abi` | максимальная версия `GLIBC_` в релизных бинарниках не выше 2.36 (Debian 12) |
| `make windows` | кросс-сборка gc под Windows через mingw (фаза 5) |
| `make small-upx` | gc + UPX, вне бюджета ([size.md](size.md), §5) |
| `make ci` | `deps check-protos test selftest size abi` |

Переменные:

| Переменная | По умолчанию | Зачем |
|---|---|---|
| `TINYGO` | `tinygo` | путь к собранному из ветки TinyGo |
| `CGO_CFLAGS` | `-Os -ffunction-sections -fdata-sections` | флаги размера C-части; в `#cgo` их писать нельзя ([driver-gtk3.md](driver-gtk3.md), §9) |
| `CGO_LDFLAGS` | `-Wl,--gc-sections -Wl,--as-needed` | то же для линковщика |
| `TINYGO_SCHED` | `none` | `threads` для приложений с горутинами |
| `ARCH` | `amd64` | какой target-файл TinyGo брать: `targets/linux-glibc-$(ARCH).json` |
| `GTK_DEBUG_ENV` | `NO_AT_BRIDGE=1 G_DEBUG=fatal-criticals` | окружение для тестов |

Скелет `examples` в Makefile:

```make
EXAMPLES := hello form logger
# Флаги размера C-части — только через окружение: в #cgo gc их не пропускает (driver-gtk3.md, §9).
export CGO_CFLAGS  := -Os -ffunction-sections -fdata-sections
export CGO_LDFLAGS := -Wl,--gc-sections -Wl,--as-needed
GOFLAGS_REL := -trimpath -ldflags="-s -w -buildid="
ARCH ?= amd64
TINYFLAGS := -target=targets/linux-glibc-$(ARCH).json -opt=z -no-debug -panic=trap -gc=conservative -scheduler=$(TINYGO_SCHED)

bin/small/%: FORCE
	$(TINYGO) build $(TINYFLAGS) -o $@ ./examples/$*
	strip --strip-all $@   # -no-debug оставляет таблицу символов: −20 КБ на спайке S3
```

## 5. arm64 (Raspberry Pi OS 64-bit и т. п.)

Работает так же. Отличается каталог библиотек: `/usr/lib/aarch64-linux-gnu`. gc собирается прямо на устройстве или кросс-компилятором (`gcc-aarch64-linux-gnu`, `CC=aarch64-linux-gnu-gcc GOARCH=arm64 CGO_ENABLED=1`). Для кросс-линковки на машине сборки нужны файлы `.so.0` целевой архитектуры (multiarch: `dpkg --add-architecture arm64`, `apt install libgtk-3-0:arm64`).

## 6. Упаковка (фаза 4)

`.deb` для приложения на nui:
- `Depends: libgtk-3-0 | libgtk-3-0t64`.
- `/usr/share/applications/<appid>.desktop` с `StartupWMClass=<appid>`, где `<appid>` — значение `nui.SetAppID`.
- Иконка в `/usr/share/icons/hicolor/<размер>/apps/<appid>.png`, `Window.SetIconName("<appid>")`.

Если GTK3 в системе нет, бинарник не запустится и выдаст ошибку загрузчика (ADR-12). Поэтому зависимость в пакете обязательна.
