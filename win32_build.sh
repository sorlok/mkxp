#!/bin/bash

SOURCES=(
  src/main.cpp
  src/audio.cpp
  src/bitmap.cpp
  src/eventthread.cpp
  src/gl-debug.cpp
  src/filesystem.cpp
  src/font.cpp
  src/input.cpp
  src/plane.cpp
  src/scene.cpp
  src/sprite.cpp
  src/table.cpp
  src/tilequad.cpp
  src/viewport.cpp
  src/window.cpp
  src/texpool.cpp
  src/shader.cpp
  src/glstate.cpp
  src/tilemap.cpp
  src/autotiles.cpp
  src/graphics.cpp
  src/etc.cpp
  src/config.cpp
  src/settingsmenu.cpp
  src/keybindings.cpp
  src/tileatlas.cpp
  src/sharedstate.cpp
  src/gl-fun.cpp
  src/gl-meta.cpp
  src/vertex.cpp
  src/soundemitter.cpp
  src/sdlsoundsource.cpp
  src/alstream.cpp
  src/audiostream.cpp
  src/rgssad.cpp
  src/bundledfont.cpp
  src/vorbissource.cpp
  src/windowvx.cpp
  src/tilemapvx.cpp
  src/tileatlasvx.cpp
  src/autotilesvx.cpp
  src/midisource.cpp
  src/fluid-fun.cpp
  src/cmdline.cpp
  src/config_file.cpp
  src/convert.cpp
  src/options_description.cpp
  src/parsers.cpp
  src/positional_options.cpp
  src/split.cpp
  src/utf8_codecvt_facet.cpp
  src/value_semantic.cpp
  src/variables_map.cpp
  src/winmain.cpp
)
  
MRI_SOURCES=(
  binding-mri/binding-mri.cpp
  binding-mri/keys-binding.cpp
  binding-mri/binding-util.cpp
  binding-mri/table-binding.cpp
  binding-mri/etc-binding.cpp
  binding-mri/bitmap-binding.cpp
  binding-mri/font-binding.cpp
  binding-mri/graphics-binding.cpp
  binding-mri/input-binding.cpp
  binding-mri/sprite-binding.cpp
  binding-mri/viewport-binding.cpp
  binding-mri/plane-binding.cpp
  binding-mri/window-binding.cpp
  binding-mri/tilemap-binding.cpp
  binding-mri/audio-binding.cpp
  binding-mri/module_rpg.cpp
  binding-mri/filesystem-binding.cpp
  binding-mri/windowvx-binding.cpp
  binding-mri/tilemapvx-binding.cpp
)

EMBED=(
  shader/transSimple.frag
  shader/trans.frag
  shader/hue.frag
  shader/sprite.frag
  shader/plane.frag
  shader/bitmapBlit.frag
  shader/minimal.vert
  shader/gray.frag
  shader/simple.frag
  shader/simpleColor.frag
  shader/simpleAlpha.frag
  shader/simpleAlphaUni.frag
  shader/flashMap.frag
  shader/flatColor.frag
  shader/simple.vert
  shader/simpleColor.vert
  shader/sprite.vert
  shader/tilemap.vert
  shader/blur.frag
  shader/blurH.vert
  shader/blurV.vert
  shader/simpleMatrix.vert
  shader/tilemapvx.vert
  shader/common.h
  assets/verdana.ttf
  assets/icon.png
)

AR=(
  libx64-msvcrt-ruby220-static.a
  libSDL2main.a
  libSDL2_image.a
  libSDL_sound.a
  libSDL2_ttf.a
  libfreetype.a
  libpng16.a
  libjpeg.a
  libvorbisfile.a
  libvorbis.a
  libogg.a
  libz.a
  libphysfs.a
  libpixman-1.a
  libsigc-2.0.a
  libSDL2.a
)

MINGW_AR=(
  libiconv.a
)

AR_FILES=()
for ar in ${AR[*]}
do
	AR_FILES+=("$HOME/mingw-root/usr/local/lib/${ar}")
done

for ar in ${MINGW_AR[*]}
do
	AR_FILES+=("$HOME/mingw-root/usr/local/lib/${ar}")
done

BOOST_I=/usr/i686-w64-mingw32/sys-root/mingw/include

PKGS=(sigc++-2.0 pixman-1 zlib physfs vorbisfile
      sdl2 SDL2_image SDL2_ttf SDL_sound openal)
      
source ../deps-win32/src/build_env
      
CXX=x86_64-w64-mingw32-g++
STRIP=x86_64-w64-mingw32-strip

CFLAGS=$(pkg-config --cflags ${PKGS[*]})
CFLAGS+=" -std=c++11 -I${BOOST_I} -I$HOME/mingw-root/usr/local/include/ -I$HOME/mingw-root/usr/local/include/AL/ -I$HOME/mingw-root/usr/local/include/ruby-2.2.0/"
CFLAGS+=" -pipe -O2"

RUBY_CFLAGS=$(pkg-config --cflags ruby-2.2)


OPENAL_LDFLAGS=$(pkg-config --libs openal)
LDFLAGS="-lwinmm -lshell32 -lws2_32 -liphlpapi -limagehlp -lshlwapi -Wl,--no-undefined -lm -ldinput8 -ldxguid -ldxerr8 -luser32 -lgdi32 -lwinmm -limm32 -lole32 -loleaut32 -lshell32 -lversion -luuid"

OBJECTS=()

LINK_ONLY=false
DEBUG=false

# Parse args
for arg in $@
do
	if [ "${arg}" == "--link-only" ]; then
		LINK_ONLY=true
	fi
	if [ "${arg}" == "--debug" ]; then
		DEBUG=true
	fi
	
done

if [ $DEBUG == true ]; then
	CFLAGS+=" -g"
fi

for embed in ${EMBED[*]}
do
	file="../${embed}"
	base=$(basename $file)
	out="${base}.xxd"

	if [ $LINK_ONLY == false ]; then
		echo "Generating ${out}"
		xxd -i $file > "${out}"
	fi
done

#Grumble...
sed -i 's|___||' *.xxd

for src in ${SOURCES[*]}
do
	file="../${src}"
	base=$(basename --suffix=.cpp $src)
	out="${base}.o"
	OBJECTS+=" ${out}"
	
	if [ $LINK_ONLY == false ]; then
		echo "Compiling ${out}"
		$CXX -c -o $out $file -I. -I../src $CFLAGS
	fi
done

for src in ${MRI_SOURCES[*]}
do
	file="../${src}"
	base=$(basename --suffix=.cpp $src)
	out="${base}.o"
	OBJECTS+=" ${out}"
	
	if [ $LINK_ONLY == false ]; then
		echo "Compiling ${out}"
		$CXX -c -o $out $file -I. -I../src -I../binding-mri $CFLAGS $RUBY_CFLAGS
	fi
done

function link_exe {
	EXE_F="${1}.exe"
	echo "Linking ${EXE_F}"
	echo "CMD: $CXX -o $EXE_F ${OBJECTS[*]} -lmingw32 -static-libstdc++ -static-libgcc ${AR_FILES[*]} $LDFLAGS -L/home/sethhetu/mingw-root/usr/local/bin -l:OpenAL32.dll $2"
	$CXX -o $EXE_F ${OBJECTS[*]} -lmingw32 -static-libstdc++ -static-libgcc ${AR_FILES[*]} $LDFLAGS -L/home/sethhetu/mingw-root/usr/local/bin -l:OpenAL32.dll $2
	if [ $DEBUG == false ]; then
		$STRIP -s $EXE_F
	fi
}

link_exe mkxp -mwindows
link_exe mkxp-console
