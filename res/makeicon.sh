#!/bin/sh
ICNS_BASE=../dist/macos/bundle/InputServer.app/Contents/Resources
if ! which magick >/dev/null 2>&1; then
    echo "Need ImageMagick for this"
    exit 10
fi
cd "$(dirname "$0")" || exit $?
if [ ! -r inputserver.png ]; then
    echo "Use inkscape (or another vector graphics editor) to create inputserver.png from inputserver.svg first"
    exit 10
fi
rm -rf work || exit $?
mkdir -p work || exit $?
for s in 16 24 32 48 64 128 256 512 1024; do
    magick convert inputserver.png -resize "${s}x${s}" -depth 8 "work/${s}.png" || exit $?
done
# windows icon
magick convert work/{16,24,32,48,64,128}.png inputserver.png inputserver.ico || exit $?
# macos icon
png2icns "$ICNS_BASE/InputServer.icns" work/{16,32,256,512,1024}.png || exit $?
rm -rf work
echo Done
