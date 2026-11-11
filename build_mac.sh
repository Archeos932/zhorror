#!/bin/sh
# Compile BOURDONNEMENT pour macOS (binaire universel arm64 + x86_64) et assemble Bourdonnement.app + Bourdonnement-macOS.zip.
# Necessite les Command Line Tools d'Xcode (xcode-select --install). A lancer sur un Mac : ./build_mac.sh
set -e
cd "$(dirname "$0")"

APP=Bourdonnement.app
rm -rf "$APP"
mkdir -p "$APP/Contents/MacOS" "$APP/Contents/Resources"

clang -O2 -std=gnu11 -fobjc-arc -Wall -Wno-deprecated-declarations \
    -mmacosx-version-min=10.15 -arch arm64 -arch x86_64 \
    main.c src/plateforme/mac/mac_platform.m \
    -framework Cocoa -framework OpenGL -framework AudioToolbox -framework CoreText -framework Carbon -framework GameController \
    -o "$APP/Contents/MacOS/bourdonnement"

cat > "$APP/Contents/Info.plist" <<'PLIST'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleName</key><string>BOURDONNEMENT</string>
    <key>CFBundleDisplayName</key><string>BOURDONNEMENT</string>
    <key>CFBundleIdentifier</key><string>com.bourdonnement.game</string>
    <key>CFBundleExecutable</key><string>bourdonnement</string>
    <key>CFBundlePackageType</key><string>APPL</string>
    <key>CFBundleShortVersionString</key><string>0.0</string>
    <key>CFBundleVersion</key><string>1</string>
    <key>LSMinimumSystemVersion</key><string>10.15</string>
    <key>NSHighResolutionCapable</key><false/>
    <key>LSApplicationCategoryType</key><string>public.app-category.games</string>
</dict>
</plist>
PLIST

# signature ad hoc (obligatoire sur Apple Silicon ; ne remplace pas une vraie signature pour la distribution)
codesign --force --deep -s - "$APP"

ditto -c -k --keepParent "$APP" Bourdonnement-macOS.zip
echo "OK : $APP et Bourdonnement-macOS.zip"
