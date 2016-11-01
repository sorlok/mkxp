cmake_minimum_required(VERSION 2.8.11)

set(CMAKE_OSX_DEPLOYMENT_TARGET 10.7)


Project(mkxp)

## Setup options ##

option(SHARED_FLUID "Dynamically link fluidsynth at build time" OFF)
option(WORKDIR_CURRENT "Keep current directory on startup" OFF)
option(FORCE32 "Force 32bit compile on 64bit OS" OFF)
set(BINDING "MRI" CACHE STRING "The Binding Type (MRI, MRUBY, NULL)")
set(EXTERNAL_LIB_PATH "" CACHE PATH "External precompiled lib prefix")


## Misc setup ##

include(cmake/PrepUtils.cmake)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

IF("${CMAKE_SYSTEM}" MATCHES "Linux")
	SET(LINUX ON)
ENDIF()

IF(WORKDIR_CURRENT)
	list(APPEND DEFINES
		WORKDIR_CURRENT
	)
ENDIF()

SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11  -O2")


IF(FORCE32)
	if(APPLE)
		SET(CMAKE_OSX_ARCHITECTURES "i386")
	else()
		SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -m32")
		SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m32")
	endif()
ENDIF()

#We need a Windows flag
IF(LINUX)
  SET(WINDOWS OFF)

  #Determine Linux architecture through some hackery.
  if("${CMAKE_SIZEOF_VOID_P}" EQUAL "8")
    MESSAGE("64-bit Linux Builds are ON")
    SET(LIN32_ONLY OFF)
  else()
    MESSAGE("32-bit Linux Builds are ON")
    SET(LIN32_ONLY ON)
  endif()
elseif(APPLE)
  SET(WINDOWS OFF)
else() #Windows?
  SET(WINDOWS ON)

  #Determine Windows architecture through some hackery.
  if("${CMAKE_SIZEOF_VOID_P}" EQUAL "8")
    MESSAGE("64-bit Windows Builds are ON")
    SET(WIN32_ONLY OFF)
  else()
    MESSAGE("32-bit Windows Builds are ON")
    SET(WIN32_ONLY ON)
  endif()
endif()

#We want as few dlls as possible on Windows, and a non-console Win32 app.
if (WINDOWS)
  set(CMAKE_EXE_LINKER_FLAGS "-static-libgcc -static-libstdc++  -mwindows")
elseif(LINUX)
  #Linux static hackery (works around a steam overlay bug).
  set(CMAKE_EXE_LINKER_FLAGS "-static-libgcc -static-libstdc++")
endif()

set(CMAKE_OSX_DEPLOYMENT_TARGET 10.7)

IF(LINUX)
	if(CMAKE_SIZEOF_VOID_P MATCHES "8" AND NOT(FORCE32) )
		set(CMAKE_EXECUTABLE_SUFFIX ".bin.x86_64")
		set(BIN_RPATH "\$ORIGIN/lib64")
		set(LIB_PATH "lib64")
	else()
		set(CMAKE_EXECUTABLE_SUFFIX ".bin.x86")
		set(BIN_RPATH "\$ORIGIN/lib")
		set(LIB_PATH "lib")
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_FILE_OFFSET_BITS=64")
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_FILE_OFFSET_BITS=64")
	endif()
elseif(APPLE)
	SET(BIN_RPATH "@executable_path/../Frameworks")
	set(LIB_PATH "lib")
endif()

set(CMAKE_SKIP_BUILD_RPATH TRUE)
set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
set(CMAKE_INSTALL_RPATH ${BIN_RPATH})
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH FALSE)

## Locate core libs ##

find_package(PkgConfig REQUIRED)

if (EXTERNAL_LIB_PATH)
	set(CMAKE_PREFIX_PATH ${EXTERNAL_LIB_PATH})
	
	if(EXISTS "${EXTERNAL_LIB_PATH}/${LIB_PATH}/pkgconfig/")
		SET(ENV{PKG_CONFIG_PATH} "${EXTERNAL_LIB_PATH}/${LIB_PATH}/pkgconfig/")
	endif()
	if(APPLE)
		set(PLATFORM_SHARED_LIBS 
			libSDL2.dylib libSDL2_image-2.0.0.dylib libSDL2_ttf-2.0.0.dylib libSDL_sound-1.0.1.dylib
			libfreetype.6.dylib libsigc-2.0.0.dylib
			CACHE STRING "List of shared libraries that need to be copied into the OS X bundle")
		foreach(lib ${PLATFORM_SHARED_LIBS})
			if(EXISTS ${EXTERNAL_LIB_PATH}/lib/${lib})
				list(APPEND PLATFORM_COPY_LIBS
					${EXTERNAL_LIB_PATH}/lib/${lib}
				)
			endif()
		endforeach()
	endif()
endif()

pkg_check_modules(SIGCXX        REQUIRED sigc++-2.0)
pkg_check_modules(PIXMAN        REQUIRED pixman-1)
pkg_check_modules(VORBISFILE    REQUIRED vorbisfile)

#These are messed up currently on Windows
if (WINDOWS) 
  pkg_check_modules(PHYSFS         physfs>=2.1)
  pkg_check_modules(SDL2           sdl2)
  pkg_check_modules(SDL2_TTF       SDL2_ttf)
  pkg_check_modules(SDL2_IMAGE     SDL2_image)
  pkg_check_modules(SDL_SOUND      SDL_sound)
else()
  pkg_check_modules(PHYSFS     REQUIRED    physfs>=2.1)
  pkg_check_modules(SDL2       REQUIRED    sdl2)
  pkg_check_modules(SDL2_TTF   REQUIRED    SDL2_ttf)
  pkg_check_modules(SDL2_IMAGE REQUIRED    SDL2_image)
  pkg_check_modules(SDL_SOUND  REQUIRED    SDL_sound)
endif()


#OS-X confuses OpenAL (Framework) and OpenALSOFT (library)
if(APPLE)
  pkg_check_modules(OPENALSOFT    REQUIRED openal)
  set(OPENAL_INCLUDE_DIR  ${OPENALSOFT_INCLUDE_DIRS})
  set(OPENAL_LIBRARY  ${OPENALSOFT_LIBRARIES})
else()
  find_package(OpenAL REQUIRED)
endif()

#Not worth the trouble.
if(APPLE)
  set(Boost_LIBRARIES "")
else()
  find_package(Boost 1.49 COMPONENTS program_options REQUIRED)
endif()

find_package(ZLIB REQUIRED)

# We hijack Ruby differently in each case. :)
if(APPLE)
  set(RUBY_STATIC_LIB  "/opt/local/lib/libruby-static.a")
else()
  set(RUBY_STATIC_LIB  "/usr/local/lib/libruby-static.a")
endif()

#Windows adds a whole bunch of garbage
if (WINDOWS)
  if (WIN32_ONLY)
    set(RUBY_STATIC_LIB
      "/home/sethhetu/.rake-compiler/builds/i686-w64-mingw32.static/ruby-2.2.2/libmsvcrt-ruby220-static.a"
      "/home/sethhetu/source/mxe/usr/i686-w64-mingw32.static/lib/libphysfs.a"
      "/home/sethhetu/source/mxe/usr/i686-w64-mingw32.static/lib/libSDL2_image.a"
      "/home/sethhetu/source/mxe/usr/i686-w64-mingw32.static/lib/libSDL2_ttf.a"
      "/home/sethhetu/source/mxe/usr/i686-w64-mingw32.static/lib/libSDL_sound.a"
      "/home/sethhetu/source/mxe/usr/i686-w64-mingw32.static/lib/libSDL2.a"
      "/home/sethhetu/source/mxe/usr/i686-w64-mingw32.static/lib/libFLAC.a"
      "/home/sethhetu/source/mxe/usr/i686-w64-mingw32.static/lib/libspeex.a"
      "/home/sethhetu/source/mxe/usr/i686-w64-mingw32.static/lib/libgmp.a"
      "/home/sethhetu/source/mxe/usr/i686-w64-mingw32.static/lib/libjpeg.a"
      "/home/sethhetu/source/mxe/usr/i686-w64-mingw32.static/lib/libiphlpapi.a")
  else()
    set(RUBY_STATIC_LIB
      "/home/sethhetu/.rake-compiler/builds/x86_64-w64-mingw32.static/ruby-2.2.2/libx64-msvcrt-ruby220-static.a"
      "/home/sethhetu/source/mxe/usr/x86_64-w64-mingw32.static/lib/libphysfs.a"
      "/home/sethhetu/source/mxe/usr/x86_64-w64-mingw32.static/lib/libSDL2_image.a"
      "/home/sethhetu/source/mxe/usr/x86_64-w64-mingw32.static/lib/libSDL2_ttf.a"
      "/home/sethhetu/source/mxe/usr/x86_64-w64-mingw32.static/lib/libSDL_sound.a"
      "/home/sethhetu/source/mxe/usr/x86_64-w64-mingw32.static/lib/libSDL2.a"
      "/home/sethhetu/source/mxe/usr/x86_64-w64-mingw32.static/lib/libFLAC.a"
      "/home/sethhetu/source/mxe/usr/x86_64-w64-mingw32.static/lib/libspeex.a"
      "/home/sethhetu/source/mxe/usr/x86_64-w64-mingw32.static/lib/libgmp.a"
      "/home/sethhetu/source/mxe/usr/x86_64-w64-mingw32.static/lib/libjpeg.a"
      "/home/sethhetu/source/mxe/usr/x86_64-w64-mingw32.static/lib/libiphlpapi.a")
  endif()
elseif(LINUX)
  #More static hackery
  if (LIN32_ONLY)
    set(RUBY_STATIC_LIB2
      "/usr/lib/i386-linux-gnu/libfreetype.a"
      "/usr/lib/i386-linux-gnu/libsigc-2.0.a"
      "/usr/lib/i386-linux-gnu/libpixman-1.a"
      "/usr/lib/i386-linux-gnu/libboost_program_options.a"
      "/usr/lib/i386-linux-gnu/libvorbisfile.a"
      "/usr/lib/i386-linux-gnu/libvorbis.a"
      "/usr/lib/i386-linux-gnu/libogg.a"
      "/usr/lib/i386-linux-gnu/libcrypt.a"
      "/usr/lib/i386-linux-gnu/libjpeg.a"
      "/usr/lib/i386-linux-gnu/libpng.a"
      "/usr/lib/i386-linux-gnu/libz.a"
    )
  else()
    set(RUBY_STATIC_LIB2
      "/usr/lib/x86_64-linux-gnu/libfreetype.a"
      "/usr/lib/x86_64-linux-gnu/libsigc-2.0.a"
      "/usr/lib/x86_64-linux-gnu/libpixman-1.a"
      "/usr/lib/x86_64-linux-gnu/libboost_program_options.a"
      "/usr/lib/x86_64-linux-gnu/libvorbisfile.a"
      "/usr/lib/x86_64-linux-gnu/libvorbis.a"
      "/usr/lib/x86_64-linux-gnu/libogg.a"
      "/usr/lib/x86_64-linux-gnu/libcrypt.a"
      "/usr/lib/x86_64-linux-gnu/libjpeg.a"
      "/usr/lib/x86_64-linux-gnu/libpng.a"
      "/usr/lib/x86_64-linux-gnu/libz.a"
    )
  endif()
  set(RUBY_STATIC_LIB
    "/usr/local/lib/libruby-static.a"
    "/usr/local/lib/libphysfs.a"
    "/usr/local/lib/libSDL2_image.a"
    "/usr/local/lib/libSDL2_ttf.a"
    "/usr/local/lib/libSDL_sound.a"
    "/usr/local/lib/libSDL2.a"
    ${RUBY_STATIC_LIB2}
    "pthread"
    "dl"
    "m"
  )

  set(SIGCXX_LIBRARIES "")
  set(SDL2_LIBRARIES "")
  set(SDL2_IMAGE_LIBRARIES "")
  set(SDL2_TTF_LIBRARIES "")
  set(SDL_SOUND_LIBRARIES "")
  set(PHYSFS_LIBRARIES "")
  set(PIXMAN_LIBRARIES "")
  set(Boost_LIBRARIES "")
  set(VORBISFILE_LIBRARIES "")
  set(VORBIS_LIBRARIES "")
  set(ZLIB_LIBRARY "")
  set(MRI_LIBRARIES "")
endif()


## Setup main source ##

set(MAIN_HEADERS
	src/quadarray.h
	src/audio.h
	src/binding.h
	src/bitmap.h
	src/disposable.h
	src/etc.h
	src/etc-internal.h
	src/eventthread.h
	src/flashable.h
	src/font.h
	src/input.h
	src/plane.h
	src/scene.h
	src/sprite.h
	src/table.h
	src/texpool.h
	src/tilequad.h
	src/transform.h
	src/viewport.h
	src/window.h
	src/serializable.h
	src/shader.h
	src/glstate.h
	src/quad.h
	src/tilemap.h
	src/tilemap-common.h
	src/graphics.h
	src/gl-debug.h
	src/global-ibo.h
	src/exception.h
	src/filesystem.h
	src/serial-util.h
	src/intrulist.h
	src/binding.h
	src/gl-util.h
	src/util.h
	src/config.h
	src/settingsmenu.h
	src/keybindings.h
	src/tileatlas.h
	src/sharedstate.h
	src/al-util.h
	src/boost-hash.h
	src/debugwriter.h
	src/gl-fun.h
	src/gl-meta.h
	src/vertex.h
	src/soundemitter.h
	src/aldatasource.h
	src/alstream.h
	src/audiostream.h
	src/rgssad.h
	src/windowvx.h
	src/tilemapvx.h
	src/tileatlasvx.h
	src/sharedmidistate.h
	src/fluid-fun.h
	src/sdl-util.h
)

set(MAIN_SOURCE
	src/main.cpp
	src/audio.cpp
	src/bitmap.cpp
	src/eventthread.cpp
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
	src/gl-debug.cpp
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
)

#Windows Icon
if (WINDOWS)
  set(WINDOWS_RES_FILE "windows_icon.rc")
else()
  set(WINDOWS_RES_FILE "")
endif()



#On OS-X, we just compile boost::program_options manually.
if(APPLE)
  set(MAIN_SOURCE 
    ${MAIN_SOURCE}
    boost/cmdline.cpp
    boost/config_file.cpp
    boost/convert.cpp
    boost/options_description.cpp
    boost/parsers.cpp
    boost/positional_options.cpp
    boost/split.cpp
    boost/utf8_codecvt_facet.cpp
    boost/value_semantic.cpp
    boost/variables_map.cpp
    boost/winmain.cpp
  )
endif()


source_group("MKXP Source" FILES ${MAIN_SOURCE} ${MAIN_HEADERS})

## Setup embedded source ##

set(EMBEDDED_INPUT
	shader/common.h
	shader/transSimple.frag
	shader/trans.frag
	shader/hue.frag
	shader/sprite.frag
	shader/plane.frag
	shader/gray.frag
	shader/bitmapBlit.frag
	shader/flatColor.frag
	shader/simple.frag
	shader/simpleColor.frag
	shader/simpleAlpha.frag
	shader/simpleAlphaUni.frag
	shader/flashMap.frag
	shader/minimal.vert
	shader/simple.vert
	shader/simpleColor.vert
	shader/sprite.vert
	shader/tilemap.vert
	shader/tilemapvx.vert
	shader/blur.frag
	shader/blurH.vert
	shader/blurV.vert
	shader/simpleMatrix.vert
	assets/liberation.ttf
	assets/deja.ttf
	assets/droid.ttf
	assets/verdana.ttf
	assets/icon.png
)

if (RGSS2)
	list(APPEND DEFINES
		RGSS2
	)
endif()

if (SHARED_FLUID)
	pkg_check_modules(FLUID REQUIRED fluidsynth)
	list(APPEND DEFINES
		SHARED_FLUID
	)
endif()

## Process Embeddeds ##

find_program(XXD_EXE xxd
	DOC "Location of the xxd executable"
)

macro(ProcessWithXXD outvar inputfile outdir)
	get_filename_component(basefile ${inputfile} NAME)
	set(outputfile ${outdir}/${basefile}.xxd)
	set_source_files_properties(${outputfile} PROPERTIES HEADER_ONLY TRUE)
	add_custom_command(
		OUTPUT ${outputfile}
		COMMAND ${XXD_EXE} -i ${inputfile} ${outputfile}
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
		DEPENDS ${inputfile}
		COMMENT "Generating XXD for ${inputfile}"
	)
	list(APPEND ${outvar}
		${outputfile}
	)
endmacro()

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/xxdhack)
include_directories(
	${CMAKE_CURRENT_BINARY_DIR}/xxdhack
)

foreach(item ${EMBEDDED_INPUT})
	ProcessWithXXD(EMBEDDED_SOURCE ${item} ${CMAKE_CURRENT_BINARY_DIR})
endforeach()

source_group("Embedded Source" FILES ${EMBEDDED_INPUT} ${EMBEDDED_SOURCE})

## Setup binding source ##

if (BINDING STREQUAL "MRI")
        if(APPLE)
          set(MRIVERSION "2.2" CACHE STRING "Version of MRI to link with")
        else()
          set(MRIVERSION "2.3" CACHE STRING "Version of MRI to link with")
        endif()
        if (WINDOWS)
	  pkg_check_modules(MRI  ruby-${MRIVERSION})
        else()
	  pkg_check_modules(MRI REQUIRED  ruby-${MRIVERSION})
        endif()

	list(APPEND DEFINES
		BINDING_MRI
	)
	set(BINDING_HEADERS
		binding-mri/binding-util.h
		binding-mri/binding-types.h
		binding-mri/serializable-binding.h
		binding-mri/disposable-binding.h
		binding-mri/sceneelement-binding.h
		binding-mri/viewportelement-binding.h
		binding-mri/flashable-binding.h
	)
	set(BINDING_SOURCE
		binding-mri/binding-mri.cpp
		binding-mri/binding-util.cpp
		binding-mri/table-binding.cpp
		binding-mri/etc-binding.cpp
		binding-mri/bitmap-binding.cpp
		binding-mri/font-binding.cpp
		binding-mri/graphics-binding.cpp
		binding-mri/keys-binding.cpp
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
elseif(BINDING STREQUAL "MRUBY")
	message(FATAL_ERROR "Mruby support in CMake needs to be finished")
	list(APPEND DEFINES
		BINDING_MRUBY
	)
	set(BINDING_HEADERS
		binding-mruby/binding-util.h
		binding-mruby/disposable-binding.h
		binding-mruby/flashable-binding.h
		binding-mruby/binding-types.h
		binding-mruby/sceneelement-binding.h
		binding-mruby/viewportelement-binding.h
		binding-mruby/serializable-binding.h
		binding-mruby/mrb-ext/file.h
		binding-mruby/mrb-ext/rwmem.h
		binding-mruby/mrb-ext/marshal.h
	)
	set(BINDING_SOURCE
		binding-mruby/binding-mruby.cpp
		binding-mruby/binding-util.cpp
		binding-mruby/window-binding.cpp
		binding-mruby/bitmap-binding.cpp
		binding-mruby/sprite-binding.cpp
		binding-mruby/font-binding.cpp
		binding-mruby/viewport-binding.cpp
		binding-mruby/plane-binding.cpp
		binding-mruby/audio-binding.cpp
		binding-mruby/tilemap-binding.cpp
		binding-mruby/etc-binding.cpp
		binding-mruby/graphics-binding.cpp
		binding-mruby/input-binding.cpp
		binding-mruby/table-binding.cpp
		binding-mruby/module_rpg.c
		binding-mruby/mrb-ext/file.cpp
		binding-mruby/mrb-ext/marshal.cpp
		binding-mruby/mrb-ext/rwmem.cpp
		binding-mruby/mrb-ext/kernel.cpp
		binding-mruby/mrb-ext/time.cpp
	)
elseif(BINDING STREQUAL "NULL")
	set(BINDING_SOURCE
		binding-null/binding-null.cpp
	)
else()
	message(FATAL_ERROR "Must choose a valid binding type.  MRI, MRUBY, or NULL")
endif()

source_group("Binding Source" FILES ${BINDING_SOURCE} ${BINDING_HEADERS})


# Link Steamlib
IF(LINUX)
        set(STEAM_LIBRARY_DIR "")
        if (LIN32_ONLY)
  	  set(STEAM_LIBRARY "${CMAKE_SOURCE_DIR}/steamworks_133b/redistributable_bin/linux32/libsteam_api.so")
        else()
          set(STEAM_LIBRARY "${CMAKE_SOURCE_DIR}/steamworks_133b/redistributable_bin/linux64/libsteam_api.so")

        endif()
elseif(APPLE)
        set(STEAM_LIBRARY_DIR "steamworks_133b/redistributable_bin/osx32")
	set(STEAM_LIBRARY "steam_api")
else() #Windows
  if (WIN32_ONLY)
    set(STEAM_LIBRARY_DIR "steamworks_133b/redistributable_bin")
    set(STEAM_LIBRARY "steam_api")
  else()
    set(STEAM_LIBRARY_DIR "steamworks_133b/redistributable_bin/win64")
    set(STEAM_LIBRARY "steam_api64")
  endif()
endif()


## Setup main executable ##

if(APPLE)
	find_library(CARBON_LIBRARY Carbon)
	find_library(IOKIT_LIBRARY IOKit)
	mark_as_advanced(CARBON_LIBRARY IOKIT_LIBRARY)
	list(APPEND PLATFORM_LIBRARIES
		${CARBON_LIBRARY}
		${IOKIT_LIBRARY}
		"-liconv"
	)
endif()

link_directories(
	${SIGCXX_LIBRARY_DIRS}
	${PIXMAN_LIBRARY_DIRS}
	${PHYSFS_LIBRARY_DIRS}
	${SDL2_LIBRARY_DIRS} # Blindly assume other SDL bits are in same directory
	${Boost_LIBRARY_DIR}
	${MRI_LIBDIR}
        ${STEAM_LIBRARY_DIR}
)

add_executable(${PROJECT_NAME} MACOSX_BUNDLE
	${MAIN_HEADERS}
	${MAIN_SOURCE}
	${BINDING_HEADERS}
	${BINDING_SOURCE}
	${EMBEDDED_SOURCE}
        ${WINDOWS_RES_FILE}
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
	${DEFINES}
)
target_include_directories(${PROJECT_NAME} PRIVATE
	src
	${SIGCXX_INCLUDE_DIRS}
	${PIXMAN_INCLUDE_DIRS}
	${PHYSFS_INCLUDE_DIRS}
	${SDL2_INCLUDE_DIRS} # Blindly assume other SDL bits are in same directory
	${Boost_INCLUDE_DIR}
	${MRI_INCLUDE_DIRS}
	${VORBISFILE_INCLUDE_DIRS}
	${FLUID_INCLUDE_DIRS}
	${OPENAL_INCLUDE_DIR}
	"steamworks_133b/"
)
if (WINDOWS)
  if (WIN32_ONLY)
    target_include_directories(${PROJECT_NAME} PRIVATE
        "/home/sethhetu/.rake-compiler/ruby/i686-w64-mingw32.static/ruby-2.2.2/include/ruby-2.2.0/"
        "/home/sethhetu/.rake-compiler/ruby/i686-w64-mingw32.static/ruby-2.2.2/include/ruby-2.2.0/i386-mingw32.static")
  else()
    target_include_directories(${PROJECT_NAME} PRIVATE
        "/home/sethhetu/.rake-compiler/ruby/x86_64-w64-mingw32.static/ruby-2.2.2/include/ruby-2.2.0/"
        "/home/sethhetu/.rake-compiler/ruby/x86_64-w64-mingw32.static/ruby-2.2.2/include/ruby-2.2.0/x64-mingw32.static")
  endif()
endif()

target_link_libraries(${PROJECT_NAME}
	${SIGCXX_LIBRARIES}
	${SDL2_LIBRARIES}
	${SDL2_IMAGE_LIBRARIES}
	${SDL2_TTF_LIBRARIES}
	${SDL_SOUND_LIBRARIES}
	${PHYSFS_LIBRARIES}
	${PIXMAN_LIBRARIES}
	${Boost_LIBRARIES}
	${MRI_LIBRARIES}
	${VORBISFILE_LIBRARIES}
	${FLUID_LIBRARIES}
	${OPENAL_LIBRARY}
	${ZLIB_LIBRARY}
	${STEAM_LIBRARY}

	${PLATFORM_LIBRARIES}
        ${RUBY_STATIC_LIB}
)


PostBuildMacBundle(${PROJECT_NAME} "" "${PLATFORM_COPY_LIBS}")
