#!/usr/bin/env bash

# This is free and unencumbered software released into the public domain.
#
# Anyone is free to copy, modify, publish, use, compile, sell, or
# distribute this software, either in source code form or as a compiled
# binary, for any purpose, commercial or non-commercial, and by any
# means.
#
# In jurisdictions that recognize copyright laws, the author or authors
# of this software dedicate any and all copyright interest in the
# software to the public domain. We make this dedication for the benefit
# of the public at large and to the detriment of our heirs and
# successors. We intend this dedication to be an overt act of
# relinquishment in perpetuity of all present and future rights to this
# software under copyright law.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
# EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
# IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
# OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
# ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
# OTHER DEALINGS IN THE SOFTWARE.
#
# For more information, please refer to <http://unlicense.org/>

SCRIPTDIR=$(dirname "${BASH_SOURCE[0]}")


set -e
sudo apt install xvfb
SHARUN=./sharun
APPIMAGETOOL=./appimagetool-x86_64
mkdir AppDir
OUTDIR=./AppDir

if [ ! -f "$SHARUN" ]; then
	wget -O "$SHARUN" "https://github.com/VHSgunzo/sharun/releases/download/v0.7.8/sharun-x86_64"
	mv "$SHARUN" "$OUTDIR"
fi

# Using go-appimage
# Backported from https://github.com/stenzek/duckstation/pull/3251
if [ ! -f "$APPIMAGETOOL" ]; then
	APPIMAGETOOLURL=$(wget -q https://github.com/AppImage/appimagetool/releases/download/continuous/appimagetool-x86_64.AppImage)
	wget -O "$APPIMAGETOOL" "$APPIMAGETOOLURL"
	chmod +x "$APPIMAGETOOL"
fi

echo "Locating extra libraries..."
EXTRA_LIBS_ARGS=""
for lib in "${MANUAL_LIBS[@]}"; do
	srcpath=$(find "$DEPSDIR" -name "$lib")
	if [ ! -f "$srcpath" ]; then
		echo "Missinge extra library $lib. Exiting."
		exit 1
	fi

	echo "Found $lib at $srcpath."

	if [ "$EXTRA_LIBS_ARGS" == "" ]; then
		EXTRA_LIBS_ARGS="--library=$srcpath"
	else
		EXTRA_LIBS_ARGS="$EXTRA_LIBS_ARGS,$srcpath"
	fi
done

# Why the nastyness? linuxdeploy strips our main binary, and there's no option to turn it off.
# It also doesn't strip the Qt libs. We can't strip them after running linuxdeploy, because
# patchelf corrupts the libraries (but they still work), but patchelf+strip makes them crash
# on load. So, make a backup copy, strip the original (since that's where linuxdeploy finds
# the libs to copy), then swap them back after we're done.
# Isn't Linux packaging amazing?

rm -fr "$DEPSDIR.bak"
cp -a "$DEPSDIR" "$DEPSDIR.bak"
IFS="
"
for i in $(find "$DEPSDIR" -iname '*.so'); do
  echo "Stripping deps library ${i}"
  strip "$i"
done

echo "Copying desktop file..."
cp "$PCSX2DIR/.github/workflows/scripts/linux/pcsx2-qt.desktop" "$OUTDIR/net.pcsx2.PCSX2.desktop"
cp "$PCSX2DIR/bin/resources/icons/AppIconLarge.png" "$OUTDIR/PCSX2.png"
cd $OUTDIR
chmod +x ./sharun
xvfb-run -- ./sharun l -p -v -e -k "$BUILDDIR/bin/pcsx2-qt" $EXTRA_LIBS_ARGS
ln $SHARUN AppRun
./AppRun -g
cd ..
echo "Copying resources into AppDir..."
cp -a "$BUILDDIR/bin/resources" "$OUTDIR/bin"

# Fix up translations.
rm -fr "$OUTDIR/bin/translations" "$OUTDIR/translations"
cp -a "$BUILDDIR/bin/translations" "$OUTDIR/bin"

ARCH=x86_64 VERSION="2.2.0" "$APPIMAGETOOL" -n "$OUTDIR" && mv ./*.AppImage "$NAME.AppImage"

