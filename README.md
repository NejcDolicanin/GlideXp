# GlideXp
Glide XP project 1016

## Compile of Glide2 to Glide3 wrapper with mingw
cd GlideXp\GLIDE2X

mingw32-make -f Makefile.mingw              ; build -> ../glide2x.dll
mingw32-make -f Makefile.mingw clean        ; remove mingw-obj/, the DLL, gendate.h
mingw32-make -f Makefile.mingw exports      ; dump the export table (must be 123)
mingw32-make -f Makefile.mingw SYMBOLS=1    ; keep DWARF for gdb