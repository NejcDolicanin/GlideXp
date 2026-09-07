# GlideXp
Glide XP project 1017

## Compile of Glide2 to Glide3 wrapper with mingw
cd GlideXp\GLIDE2X
<br/><br/>
mingw32-make -f Makefile.mingw              ; build -> ../glide2x.dll
<br/>
mingw32-make -f Makefile.mingw clean        ; remove mingw-obj/, the DLL, gendate.h
<br/>
mingw32-make -f Makefile.mingw exports      ; dump the export table (must be 123)
<br/>
mingw32-make -f Makefile.mingw SYMBOLS=1    ; keep DWARF for gdb
<br/>

##More info - the original site
https://wenchy.net/old/glidexp/