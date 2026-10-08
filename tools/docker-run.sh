#!/bin/sh
# Выполнить команду в чистом Debian 12 (образ nui-bookworm: runtime GTK, Xvfb, openbox,
# rootshot; без libgtk-3-dev) с Go и TinyGo хоста. Команда идёт от текущего пользователя,
# кэши сборки лежат в $NUI_DOCKER_CACHE (по умолчанию ~/.cache/nui/docker).
#   tools/docker-run.sh make test
#   TINYGOROOT=… — распакованный выпуск TinyGo (по умолчанию ~/.cache/nui/tinygo-0.42.0/x/tinygo)
set -eu
root=$(cd "$(dirname "$0")/.." && pwd)
GOROOT_HOST=$(go env GOROOT)
# В Debian-пакете Go каталоги src, lib и др. — симлинки в /usr/share/go-*: монтируем и его.
GOSHARE=$(dirname "$(readlink -f "$GOROOT_HOST/src")")
TINYGOROOT=${TINYGOROOT:-$HOME/.cache/nui/tinygo-0.42.0/x/tinygo}
CACHE=${NUI_DOCKER_CACHE:-$HOME/.cache/nui/docker}
mkdir -p "$CACHE/home" "$CACHE/go" "$CACHE/xdg"
docker build -q -t nui-bookworm -f "$root/tools/bookworm.Dockerfile" "$root/tools" >/dev/null

set -- --rm --user "$(id -u):$(id -g)" \
	-v "$root":/src -w /src \
	-v "$GOROOT_HOST":"$GOROOT_HOST":ro -v "$GOSHARE":"$GOSHARE":ro \
	-v "$CACHE":/cache \
	-e HOME=/cache/home -e GOCACHE=/cache/go -e XDG_CACHE_HOME=/cache/xdg \
	-e GOTOOLCHAIN=local -e GOFLAGS=-buildvcs=false \
	-e PATH="/opt/tinygo/bin:$GOROOT_HOST/bin:/usr/local/bin:/usr/bin:/bin" \
	nui-bookworm "$@"
# TinyGo монтируем, только если он есть: иначе docker создал бы пустой каталог от root.
if [ -d "$TINYGOROOT" ]; then
	set -- -v "$TINYGOROOT":/opt/tinygo:ro "$@"
fi
exec docker run "$@"
