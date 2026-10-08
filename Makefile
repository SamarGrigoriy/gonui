# nui — сборка и проверки (docs/build.md, §4).
#
# Цели, которым нужны GTK под Xvfb и TinyGo, проще всего гонять в чистом Debian 12:
#   make docker-ci           — всё, что не требует libgtk-3-dev, в контейнере nui-bookworm
#   make ci                  — check на хосте (нужен libgtk-3-dev) + docker-ci
#   tools/docker-run.sh make test   — любая цель в контейнере

GO           ?= go
TINYGO       ?= tinygo
TINYGO_SCHED ?= none
ARCH         ?= amd64
EXAMPLES     := hello
GTK_DEBUG_ENV := NO_AT_BRIDGE=1 G_DEBUG=fatal-criticals
XVFB         ?= xvfb-run -a

# Флаги размера C-части — только через окружение: в #cgo gc их не пропускает (driver-gtk3.md, §9).
export CGO_CFLAGS  := -Os -ffunction-sections -fdata-sections
export CGO_LDFLAGS := -Wl,--gc-sections -Wl,--as-needed

GOFLAGS_REL := -trimpath -ldflags="-s -w -buildid="
# Выпущенный TinyGo + target-файл nui: системная glibc вместо musl (ADR-17).
TINYFLAGS   := -target=targets/linux-glibc-$(ARCH).json -opt=z -no-debug -panic=trap -gc=conservative -scheduler=$(TINYGO_SCHED)

.PHONY: all dev release-gc small test race selftest deps size abi check-protos vet check ci ci-container docker-ci shots clean FORCE

all: dev

dev: $(EXAMPLES:%=bin/%)                     ## gc, для отладки
bin/%: FORCE
	$(GO) build -o $@ ./examples/$*

release-gc: $(EXAMPLES:%=bin/gc/%)           ## gc с релизными флагами
bin/gc/%: FORCE
	$(GO) build $(GOFLAGS_REL) -o $@ ./examples/$*

small: $(EXAMPLES:%=bin/small/%)             ## TinyGo, релиз
# strip обязателен: -no-debug убирает DWARF, но оставляет таблицу символов (size.md, §4).
bin/small/%: FORCE
	$(TINYGO) build $(TINYFLAGS) -o $@ ./examples/$*
	strip --strip-all $@

test:                                        ## юнит-тесты ядра + интеграционные на GTK (обычная и отладочная сборка)
	$(GO) test -count=1 -cover ./internal/core/...
	$(GTK_DEBUG_ENV) $(XVFB) $(GO) test -count=1 -tags gui .
	$(GTK_DEBUG_ENV) $(XVFB) $(GO) test -count=1 -tags 'gui nuidebug' .

race:                                        ## то же под детектором гонок (только gc)
	$(GO) test -count=1 -race ./internal/core/...
	$(GTK_DEBUG_ENV) $(XVFB) $(GO) test -count=1 -race -tags gui .

selftest:                                    ## примеры обоими тулчейнами: окно, цикл, выход с кодом 0
	@mkdir -p bin/selftest
	@for e in $(EXAMPLES); do \
		$(GO) build -tags selftest -o bin/selftest/$$e-gc ./examples/$$e && \
		$(TINYGO) build $(TINYFLAGS) -tags selftest -o bin/selftest/$$e-tinygo ./examples/$$e && \
		for t in gc tinygo; do \
			$(GTK_DEBUG_ENV) timeout 20 $(XVFB) ./bin/selftest/$$e-$$t || { echo "selftest: $$e ($$t) — ошибка"; exit 1; }; \
			echo "selftest: $$e ($$t) — ok"; \
		done; \
	done

deps:                                        ## белый список импортов, без горутин в библиотеке
	./tools/depcheck.sh

size: release-gc small                       ## размеры против size-budget.txt
	./tools/sizecheck.sh

abi: release-gc small                        ## glibc не новее Debian 12
	./tools/abicheck.sh

check-protos:                                ## прототипы против настоящих заголовков (нужен libgtk-3-dev)
	./tools/check-protos.sh

vet:
	$(GO) vet ./...
	$(GO) vet -tags gui .
	$(GO) vet -tags 'gui nuidebug' .

check: vet deps check-protos                 ## статические проверки на хосте

ci-container: deps test race selftest size abi   ## всё, что не требует libgtk-3-dev (внутри контейнера)

docker-ci:                                   ## ci-container в чистом Debian 12
	./tools/docker-run.sh make ci-container

ci: check docker-ci

SHOTS := docs/reports/phase-1
shots: release-gc small                      ## фотоотчёт: окна под Xvfb с openbox (в контейнере)
	@mkdir -p $(SHOTS)
	LANG=ru_RU.UTF-8 $(GTK_DEBUG_ENV) $(XVFB) -s "-screen 0 640x360x24" sh -c '\
		openbox >/dev/null 2>&1 & sleep 1; \
		for t in gc small; do ./bin/$$t/hello & pid=$$!; sleep 1.5; rootshot | pnmtopng > $(SHOTS)/hello-$$t.png; kill $$pid; sleep 0.5; done; \
		NUI_SHOTS=$(SHOTS) $(GO) test -count=1 -tags gui -run TestBoxLayout .'

clean:
	rm -rf bin

FORCE:
