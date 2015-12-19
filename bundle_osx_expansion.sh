#!/bin/bash

#Variable!
APPNAME="LastDreamExpansion.app"
ORIGRES="../LastDreamExpansion"
BNDLFLDR="./bundle"  #Where we put the final app

APPBINARY="$APPNAME/Contents/MacOS/mkxp"
echo "Bundling $APPNAME"


#Prepare a bundle for your App.
rm -rf $APPNAME
rm -rf $BNDLFLDR

#Make directory structure
mkdir $BNDLFLDR
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

       #Hack for openal
       if [[ $ln == "libopenal"* ]]; then
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
     done < <(otool -L $lib | grep -o  -e "^\t/opt/[a-zA-Z0-9_./-]*"  -e "^\t/usr/local/[a-zA-Z0-9_./-]*"  -e "^\tlibphysfs.[0-9].dylib" -e "^\tlibopenal.[0-9].dylib")

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
   if [[ $lib == "libopenal"* ]]; then
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


#Special case
echo "Fixing Steam lib..."
cp steamworks_133b/redistributable_bin/osx32/libsteam_api.dylib  $APPNAME/Contents/Frameworks/
install_name_tool  -change @loader_path/libsteam_api.dylib  @executable_path/../Frameworks/libsteam_api.dylib   $APPBINARY


#Copy resources
echo "Copying resources..."
cp -r $ORIGRES/Audio $APPNAME/Contents/Resources/
cp -r $ORIGRES/Data $APPNAME/Contents/Resources/
cp -r $ORIGRES/Fonts $APPNAME/Contents/Resources/
cp -r $ORIGRES/Graphics $APPNAME/Contents/Resources/
cp -r $ORIGRES/swapxt $APPNAME/Contents/Resources/
cp -r $ORIGRES/*README* $APPNAME/Contents/Resources/
cp -r $ORIGRES/ld_icon.icns $APPNAME/Contents/Resources/
cp -r $ORIGRES/mkxp.conf $APPNAME/Contents/Resources/


#Remove Mercurial stuff
echo "Removing Mercurial directories..."
rm -rf $APPNAME/Contents/Resources/Audio/.hg
rm -rf $APPNAME/Contents/Resources/Graphics/.hg




#LATER
#cp -r $ORIGRES/steam_appid.txt $APPNAME/Contents/Resources/


#We need a build date...
SPECIAL_VERSION="2.1.$(date +%Y-%m-%d | sed "s|-||g")"
echo "Build version is: $SPECIAL_VERSION"

#Make an Info file! Copy the default Game settings!
cat "Info.plist.in" | sed "s|SPECIAL_VERSION_EXPAND|$SPECIAL_VERSION|" | sed "s|LastDream1*|LastDreamExpansion|" >$APPNAME/Contents/Info.plist
cp -r Game.ini.default $APPNAME/Contents/Resources/Game.ini

#Do Valve's weird dance.
echo "Steamifying..."
appid=$(head -n 1 $ORIGRES/steam_appid.txt)
/Applications/ContentPrep.app/Contents/MacOS/contentprep.py --console --source=$APPNAME  --dest=$BNDLFLDR  --noscramble  --appid=$appid

#Now, do a sanity check.
echo "Checking for potential linker problems..."
for lib in "${copied_libs[@]}"; do
  echo "Checking $lib:"

  if [[ $lib == *"MacOS/mkxp"* ]]; then
    otool -L $lib | tail -n +2  | grep -e "/opt/local/lib" -e "/usr/local/lib"
  else
    otool -L $lib | tail -n +3  | grep -e "/opt/local/lib" -e "/usr/local/lib"
  fi

  fi

  #Sanity check 2
  otool -l $lib  | grep -A2 MIN_MACOSX | tail -n1 | grep 10.[^7]
done
echo "Potential problems (if any) listed above."

echo "Done; your Steam-bundled app is in \"bundle\"."


