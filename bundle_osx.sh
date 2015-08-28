#!/bin/bash

#Variable!
APPNAME="LastDream.app"
ORIGRES="../LastDream_mkxp"

APPBINARY="$APPNAME/Contents/MacOS/mkxp"
echo "Bundling $APPNAME"


#Prepare a bundle for your App.
rm -rf $APPNAME

#Make directory structure
cp -r build/mkxp.app $APPNAME
rm -rf $APPNAME/Contents/Frameworks
mkdir $APPNAME/Contents/Frameworks
mkdir $APPNAME/Contents/Resources

#Starting from the library itself, build up a list of required libraries.
#We identify those as libraries in /opt (MacPorts), those that are locally built
# (/usr/local), and libphysfs (which is weird for some reason?)
libraries=()
lib_deps=()
temp_lib[0]="$APPBINARY"
#declare -A libraries #bin/lib -> {local_deps}
#libraries=( ["$APPNAME/Contents/MacOS/mkxp"]=[] )

#Helper: check if an element is in an array
#http://stackoverflow.com/questions/3685970/check-if-an-array-contains-a-value
containsElement () {
  local e
  for e in "${@:2}"; do [[ "$e" == "$1" ]] && return 0; done
  return 1
}

#Keep looping and adding libraries until we're done.
while [ "${#temp_lib[@]}" -gt "0" ] ; do
  #echo "Top of loop: ${#temp_lib[@]}"
  new_temp_lib=()
  for lib in "${temp_lib[@]}" ; do
    #Add it.
    #echo "Scanning Library: $lib"
    libraries+=($lib)

    #Get its dependencies.
    count=0
    while read ln; do
      #Always skip the first line for libraries (self)
      #We can't use an exact path match for reasons.
      count=$((count+1))
      if [ $count -eq "1" ]; then
        if [[ $lib != $APPBINARY ]]; then
         #echo "SKIPPING first line: $ln"
         first=false
         continue
        fi
      fi

      #Hack for libphysfs
      oldLn=$ln
      if [[ $ln == "libphysfs"* ]]; then
       ln="/opt/local/lib/$ln"
      fi

      if containsElement "$ln" "${new_temp_lib[@]}" || containsElement "$ln" "${temp_lib[@]}" ; then
        #echo "Skipping lib: $ln (already found)"
        :
      else
        #echo "New lib: $ln"
        new_temp_lib+=($ln)
        lib_deps+=($oldLn) #"physfs" directly.
      fi
      #echo "line: $ln"
    done < <(otool -L $lib | grep -o  -e "^\t/opt/[a-zA-Z0-9_./-]*"  -e "^\t/usr/local/[a-zA-Z0-9_./-]*"  -e "^\tlibphysfs.[0-9].dylib")
  done

  #Swap over the new list
  temp_lib=()
  for lib in "${new_temp_lib[@]}" ; do
    temp_lib+=($lib)
  done
done


#Copy over the libraries
echo "Copying over libraries..."
copied_libs=()  #Resultant path
copied_libs+=($APPBINARY)
for lib in "${lib_deps[@]}"; do
  #Grumble grumble, physfs...
  if [[ $lib == "libphysfs"* ]]; then
    lib="/opt/local/lib/$lib"
  fi

  #Extract the filename
  filename="$APPNAME/Contents/Frameworks/${lib##*/}"
  copied_libs+=($filename)
  ##echo "Library mapping: $lib : $filename"
  cp $lib $filename
done


#Now, scan and update the libraries.
echo "Re-writing dependencies."
for lib in "${copied_libs[@]}"; do
  #echo "Replacing deps fro $lib"
  for dep in "${lib_deps[@]}"; do
    filename="@executable_path/../Frameworks/${dep##*/}"
    #echo "  Replacing: $dep ; with: $filename"
    install_name_tool  -change  $dep  $filename  $lib
  done
done



<<"COMMENT"
>>>>>>> 739beb9... Beefed up the bundler script to really scan every library (recursively) and modify all listed dependencies.

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


#TEMP
install_name_tool  -change  /opt/local/lib/libSDL2-2.0.0.dylib    @executable_path/../Frameworks/libSDL2-2.0.0.dylib    Trump3016.app/Contents/Frameworks/libSDL2_image-2.0.0.dylib
install_name_tool  -change /opt/local/lib/libiconv.2.dylib @executable_path/../Frameworks/libiconv.2.dylib   Trump3016.app/Contents/Frameworks/libSDL2_image-2.0.0.dylib 
install_name_tool  -change  /opt/local/lib/libz.1.dylib @executable_path/../Frameworks/libz.1.dylib   Trump3016.app/Contents/Frameworks/libSDL2_ttf-2.0.0.dylib 
install_name_tool  -change  /opt/local/lib/libSDL2-2.0.0.dylib    @executable_path/../Frameworks/libSDL2-2.0.0.dylib    Trump3016.app/Contents/Frameworks/libSDL2_ttf-2.0.0.dylib
install_name_tool  -change /opt/local/lib/libiconv.2.dylib @executable_path/../Frameworks/libiconv.2.dylib   Trump3016.app/Contents/Frameworks/libSDL2_ttf-2.0.0.dylib 
install_name_tool  -change  /opt/local/lib/libSDL2-2.0.0.dylib    @executable_path/../Frameworks/libSDL2-2.0.0.dylib    Trump3016.app/Contents/Frameworks/libSDL_sound-1.0.1.dylib
install_name_tool  -change /opt/local/lib/libiconv.2.dylib @executable_path/../Frameworks/libiconv.2.dylib   Trump3016.app/Contents/Frameworks/libSDL_sound-1.0.1.dylib 
install_name_tool  -change  /opt/local/lib/libvorbisfile.3.dylib @executable_path/../Frameworks/libvorbisfile.3.dylib   Trump3016.app/Contents/Frameworks/libSDL_sound-1.0.1.dylib 
install_name_tool  -change  /opt/local/lib/libvorbis.0.dylib   @executable_path/../Frameworks/libvorbis.0.dylib   Trump3016.app/Contents/Frameworks/libSDL_sound-1.0.1.dylib 
install_name_tool  -change  /opt/local/lib/libFLAC.8.dylib   @executable_path/../Frameworks/libFLAC.8.dylib   Trump3016.app/Contents/Frameworks/libSDL_sound-1.0.1.dylib 
install_name_tool  -change  /opt/local/lib/libogg.0.dylib   @executable_path/../Frameworks/libogg.0.dylib   Trump3016.app/Contents/Frameworks/libSDL_sound-1.0.1.dylib 
install_name_tool  -change  /opt/local/lib/libspeex.1.dylib   @executable_path/../Frameworks/libspeex.1.dylib   Trump3016.app/Contents/Frameworks/libSDL_sound-1.0.1.dylib 
install_name_tool  -change  /opt/local/lib/libogg.0.dylib   @executable_path/../Frameworks/libogg.0.dylib   Trump3016.app/Contents/Frameworks/libFLAC.8.dylib 
install_name_tool  -change  /opt/local/lib/libogg.0.dylib   @executable_path/../Frameworks/libogg.0.dylib   Trump3016.app/Contents/Frameworks/libvorbis.0.dylib
install_name_tool  -change  /opt/local/lib/libvorbis.0.dylib   @executable_path/../Frameworks/libvorbis.0.dylib   Trump3016.app/Contents/Frameworks/libvorbisfile.3.dylib 
install_name_tool  -change  /opt/local/lib/libogg.0.dylib   @executable_path/../Frameworks/libogg.0.dylib   Trump3016.app/Contents/Frameworks/libvorbisfile.3.dylib

install_name_tool  -change  /opt/local/lib/libbz2.1.0.dylib @executable_path/../Frameworks/libbz2.1.0.dylib   Trump3016.app/Contents/Frameworks/libSDL2_ttf-2.0.0.dylib 
install_name_tool  -change  /opt/X11/lib/libfreetype.6.dylib @executable_path/../Frameworks/libfreetype.6.dylib   Trump3016.app/Contents/Frameworks/libSDL2_ttf-2.0.0.dylib 

COMMENT


#Special case
echo "Fixing Steam lib..."
cp steamworks_133b/redistributable_bin/osx32/libsteam_api.dylib  $APPNAME/Contents/Frameworks/
install_name_tool  -change @loader_path/libsteam_api.dylib  @executable_path/../Frameworks/libsteam_api.dylib   $APPBINARY


#Copy resources
echo "Copying resources..."
cp -r $ORIGRES/* $APPNAME/Contents/Resources/

#We need a build date...
SPECIAL_VERSION="2.1.$(date +%Y-%m-%d | sed "s|-||g")"
echo "Build version is: $SPECIAL_VERSION"

#Make an Info file!
#cp Info.plist.in  $APPNAME/Contents/Info.plist
#sed -i  "s|SPECIAL_VERSION_EXPAND|$SPECIAL_VERSION|"  $APPNAME/Contents/Info.plist
cat "Info.plist.in" | sed "s|SPECIAL_VERSION_EXPAND|$SPECIAL_VERSION|" >$APPNAME/Contents/Info.plist

#Now, do a sanity check.
echo "Potential linker problems:"
for lib in "${copied_libs[@]}"; do
  if [[ $lib == *"MacOS/mkxp"* ]]; then
    otool -L $lib | tail -n +2  | grep -e "/opt/local/lib" -e "/usr/local/lib"
  else
    otool -L $lib | tail -n +3  | grep -e "/opt/local/lib" -e "/usr/local/lib"
  fi
done

echo "Done"


