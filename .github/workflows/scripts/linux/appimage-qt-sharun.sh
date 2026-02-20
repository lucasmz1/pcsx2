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
wget -O "sharun" "https://github.com/VHSgunzo/sharun/releases/download/v0.7.9/sharun-x86_64"

# Baixar arquivo .desktop (usar link raw!)
wget -O "net.pcsx2.PCSX2.desktop" "https://raw.githubusercontent.com/lucasmz1/pcsx2/master/.github/workflows/scripts/linux/pcsx2-qt.desktop"

chmod +x ./sharun

# Gerar AppRun com xvfb
find ${GITHUB_WORKSPACE}/ -type f -iname 'pcsx2-qt' -executable | xargs -i -t ./sharun l -p -v -e -s -k {}
./sharun l -p -v -e -s -k \
	/usr/lib/x86_64-linux-gnu/dri/* \
	/usr/lib/x86_64-linux-gnu/vdpau/* \
	/usr/lib/x86_64-linux-gnu/libshaderc.so* \
	/usr/lib/x86_64-linux-gnu/libQt6Svg.so* \
	/usr/lib/x86_64-linux-gnu/lib*CL*.so* \
	/usr/lib/x86_64-linux-gnu/libvulkan*.so* \
	/usr/lib/x86_64-linux-gnu/libVkLayer*.so* \
	/usr/lib/x86_64-linux-gnu/libvulkan_* \
    /usr/lib/x86_64-linux-gnu/lib*GL*.so* \
	/usr/lib/x86_64-linux-gnu/libXss.so* \
	/usr/lib/x86_64-linux-gnu/gio/modules/* \
	/usr/lib/x86_64-linux-gnu/gdk-pixbuf-*/*/*/* \
	/usr/lib/x86_64-linux-gnu/alsa-lib/* \
	/usr/lib/x86_64-linux-gnu/pulseaudio/* \
	/usr/lib/x86_64-linux-gnu/pipewire-0.3/* \
	/usr/lib/x86_64-linux-gnu/spa-0.2/*/* || true
./sharun l -p -v -e -s -k /usr/bin/vulkaninfo
#find /usr/lib/x86_64-linux-gnu/qt6/ -iname 'lib**' -print0 | xargs -0 -I{} ./sharun l -p -v -e -s -k {}
mkdir -p ./shared/lib/qt6/plugins/ && find . -iname 'plugins' | xargs -i -t -exec mv -f {} ./shared/lib/qt6/
find ./shared/lib/home/runner/deps/lib/ -iname 'lib*.so*' -print0 | xargs -0 -I{} cp -a {} ./shared/lib/
find ./shared/lib/local/ -iname 'lib*.so*' -print0 | xargs -0 -I{} cp -a {} ./shared/lib/
find . -iname 'home' | xargs -i -t -exec rm -rf {}
find . -iname 'local' | xargs -i -t -exec rm -rf {}
find "${GITHUB_WORKSPACE}" -iname 'qt.conf' -type f | xargs -i -t -exec cp {} "./bin/"
ln -rs ./shared/lib/libshaderc.so.1 ./shared/lib/libshaderc_shared.so.1
ln ./sharun AppRun
./sharun -g

cd ..

echo "Copying resources into AppDir..."
find ${GITHUB_WORKSPACE}/ -name 'resources' | xargs -i -t -exec cp -r {} "$OUTDIR/bin"

# Corrigir traduções
rm -rf "$OUTDIR/bin/translations" "$OUTDIR/translations"

find ${GITHUB_WORKSPACE}/ -name 'translations' | xargs -i -t -exec cp -r {} "$OUTDIR/bin"

# Baixar appimagetool
wget -q -O appimagetool "https://github.com/AppImage/appimagetool/releases/download/continuous/appimagetool-x86_64.AppImage"
chmod +x appimagetool

# Criar AppImage
ARCH=x86_64 ./appimagetool -n "$OUTDIR"
