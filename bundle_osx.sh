#!/bin/bash

#Prepare a bundle for Last Dream.
rm -rf LastDream.app

#Make directory structure
cp -r build/mkxp.app LastDream.app
rm -rf LastDream.app/Contents/Frameworks
mkdir LastDream.app/Contents/Frameworks
mkdir LastDream.app/Contents/Resources

#Copy over libs
cp /opt/local/lib/libsigc-2.0.0.dylib  LastDream.app/Contents/Frameworks/
cp /opt/local/lib/libSDL2-2.0.0.dylib  LastDream.app/Contents/Frameworks/
cp /opt/local/lib/libSDL2_image-2.0.0.dylib  LastDream.app/Contents/Frameworks/
cp /opt/local/lib/libSDL2_ttf-2.0.0.dylib  LastDream.app/Contents/Frameworks/
cp /opt/local/lib/libSDL_sound-1.0.1.dylib  LastDream.app/Contents/Frameworks/
cp /opt/local/lib/libphysfs.1.dylib  LastDream.app/Contents/Frameworks/
cp /opt/local/lib/libpixman-1.0.dylib  LastDream.app/Contents/Frameworks/
cp /opt/local/lib/libvorbisfile.3.dylib  LastDream.app/Contents/Frameworks/
cp /opt/local/lib/libz.1.dylib  LastDream.app/Contents/Frameworks/
cp /opt/local/lib/libopenal.1.dylib  LastDream.app/Contents/Frameworks/
cp /opt/local/lib/libiconv.2.dylib  LastDream.app/Contents/Frameworks/
cp steamworks_133b/redistributable_bin/osx32/libsteam_api.dylib  LastDream.app/Contents/Frameworks/

#Make an Info file!
cp Info.plist.in  LastDream.app/Contents/Info.plist

#Copy our Icon over!
cp ld_icon.ico LastDream.app/Contents/Resources/


#Relink libs
install_name_tool  -change  /opt/local/lib/libsigc-2.0.0.dylib  @executable_path/../Frameworks/libsigc-2.0.0.dylib  LastDream.app/Contents/MacOS/mkxp 
install_name_tool  -change  /opt/local/lib/libSDL2-2.0.0.dylib   @executable_path/../Frameworks/libSDL2-2.0.0.dylib    LastDream.app/Contents/MacOS/mkxp 
install_name_tool  -change  /opt/local/lib/libSDL2_image-2.0.0.dylib  @executable_path/../Frameworks/libSDL2_image-2.0.0.dylib    LastDream.app/Contents/MacOS/mkxp 
install_name_tool  -change   /opt/local/lib/libSDL2_ttf-2.0.0.dylib  @executable_path/../Frameworks/libSDL2_ttf-2.0.0.dylib    LastDream.app/Contents/MacOS/mkxp 
install_name_tool  -change   /opt/local/lib/libSDL_sound-1.0.1.dylib @executable_path/../Frameworks/libSDL_sound-1.0.1.dylib    LastDream.app/Contents/MacOS/mkxp 
install_name_tool  -change   libphysfs.1.dylib @executable_path/../Frameworks/libphysfs.1.dylib    LastDream.app/Contents/MacOS/mkxp 
install_name_tool  -change   /opt/local/lib/libpixman-1.0.dylib  @executable_path/../Frameworks/libpixman-1.0.dylib    LastDream.app/Contents/MacOS/mkxp 
install_name_tool  -change  /opt/local/lib/libvorbisfile.3.dylib @executable_path/../Frameworks/libvorbisfile.3.dylib   LastDream.app/Contents/MacOS/mkxp 
install_name_tool  -change  /opt/local/lib/libz.1.dylib @executable_path/../Frameworks/libz.1.dylib   LastDream.app/Contents/MacOS/mkxp 
install_name_tool  -change  /opt/local/lib/libopenal.1.dylib @executable_path/../Frameworks/libopenal.1.dylib   LastDream.app/Contents/MacOS/mkxp 
install_name_tool  -change /opt/local/lib/libiconv.2.dylib @executable_path/../Frameworks/libiconv.2.dylib   LastDream.app/Contents/MacOS/mkxp 

#Steam is weird
install_name_tool  -change @loader_path/libsteam_api.dylib  @executable_path/../Frameworks/libsteam_api.dylib   LastDream.app/Contents/MacOS/mkxp


#Copy resources
cp -r ../LastDream_mkxp/* LastDream.app/Contents/Resources/

echo "Potential linker problems:"

otool -L LastDream.app/Contents/MacOS/mkxp  | grep -e "/opt/local/lib" -e "/usr/local/lib"

echo "Done"


