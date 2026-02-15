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

#!/usr/bin/env bash

SCRIPTDIR=$(dirname "${BASH_SOURCE[0]}")

set -e
sudo apt install -y xvfb
mkdir -p AppDir
OUTDIR=./AppDir

cd "$OUTDIR"

# Copiar ícone
find "${GITHUB_WORKSPACE}/" -iname "AppIconLarge.png" | xargs -i -t cp {} .
mv AppIconLarge.png PCSX2.png

# Baixar sharun
wget -O "sharun" "https://github.com/VHSgunzo/sharun/releases/download/v0.7.8/sharun-x86_64"

# Baixar arquivo .desktop (usar link raw!)
wget -O "net.pcsx2.PCSX2.desktop" "https://raw.githubusercontent.com/lucasmz1/pcsx2/master/.github/workflows/scripts/linux/pcsx2-qt.desktop"

chmod +x ./sharun

# Gerar AppRun com xvfb
xvfb-run -- ./sharun l -p -v -e -k "${GITHUB_WORKSPACE}/install/bin/pcsx2-qt"
ln -sf ./sharun AppRun
./sharun -g

cd ..

echo "Copying resources into AppDir..."
find ${GITHUB_WORKSPACE}/ -iname 'resources' | xargs -i -t -exec cp -r {} "$OUTDIR/bin"

# Corrigir traduções
rm -rf "$OUTDIR/bin/translations" "$OUTDIR/translations"

find ${GITHUB_WORKSPACE}/ -iname 'translations' | xargs -i -t -exec cp -r {} "$OUTDIR/bin"

# Baixar appimagetool
wget -q -O appimagetool "https://github.com/AppImage/appimagetool/releases/download/continuous/appimagetool-x86_64.AppImage"
chmod +x appimagetool

# Criar AppImage
ARCH=x86_64 VERSION="2.2.0" ./appimagetool -n "$OUTDIR"

# Renomear AppImage final
mv ./*.AppImage "PCSX2-sharun.AppImage"
