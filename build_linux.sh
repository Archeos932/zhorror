#!/bin/sh
# Compile BOURDONNEMENT pour Linux (X11/XWayland + OpenGL + ALSA) et assemble Bourdonnement-Linux/ + .tar.gz.
# Paquets (Debian / Ubuntu) : sudo apt install gcc libx11-dev libgl-dev libasound2-dev libfreetype-dev
set -e
cd "$(dirname "$0")"

CC=${CC:-gcc}
FT_CFLAGS=$(pkg-config --cflags freetype2 2>/dev/null || echo "-I/usr/include/freetype2")
FT_LIBS=$(pkg-config --libs freetype2 2>/dev/null || echo "-lfreetype")

OUT=Bourdonnement-Linux
rm -rf "$OUT"
mkdir -p "$OUT"

# -Wno-format-truncation : gcc signale chaque snprintf qui pourrait couper (voulu : c'est son role)
$CC -O2 -std=gnu11 -Wall -Wno-format-truncation ${BOURDON_BUILD:+-DBOURDON_BUILD=$BOURDON_BUILD} $FT_CFLAGS \
    main.c src/plateforme/linux/linux_platform.c \
    -lX11 -lGL -lasound $FT_LIBS -ldl -lpthread -lm \
    -o "$OUT/bourdonnement"

tar -czf Bourdonnement-Linux.tar.gz "$OUT"
echo "OK : $OUT/bourdonnement et Bourdonnement-Linux.tar.gz"
