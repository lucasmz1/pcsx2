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
mkdir AppDir
OUTDIR=./AppDir

echo "Copying desktop file..."
cp "$PCSX2DIR/.github/workflows/scripts/linux/pcsx2-qt.desktop" "$OUTDIR/net.pcsx2.PCSX2.desktop"
cp "$PCSX2DIR/bin/resources/icons/AppIconLarge.png" "$OUTDIR/PCSX2.png"
cd $OUTDIR
wget -O "sharun" "https://github.com/VHSgunzo/sharun/releases/download/v0.7.8/sharun-x86_64"
chmod +x ./sharun
xvfb-run -- ./sharun l -p -v -e -k "$BUILDDIR/bin/pcsx2-qt"
ln $SHARUN AppRun
./AppRun -g
cd ..
echo "Copying resources into AppDir..."
cp -a "$BUILDDIR/bin/resources" "$OUTDIR/bin"

# Fix up translations.
rm -fr "$OUTDIR/bin/translations" "$OUTDIR/translations"
cp -a "$BUILDDIR/bin/translations" "$OUTDIR/bin"
wget -q -O appimagetool "https://github.com/AppImage/appimagetool/releases/download/continuous/appimagetool-x86_64.AppImage"
ARCH=x86_64 VERSION="2.2.0" ./appimagetool -n "$OUTDIR" && mv ./*.AppImage "PCSX2-sharun.AppImage"

