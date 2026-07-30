# Microsoft Developer Studio Project File - Name="glide3x" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

CFG=glide3x - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "glide3x.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "glide3x.mak" CFG="glide3x - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "glide3x - Win32 Release" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "glide3x - Win32 Debug" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "glide3x - Win32 Release No ASM" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "glide3x - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "glide3x___Win32_Release"
# PROP BASE Intermediate_Dir "glide3x___Win32_Release"
# PROP BASE Ignore_Export_Lib 1
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "."
# PROP Intermediate_Dir "Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MT /W3 /GX /O2 /I ".\\" /I "h5\incsrc" /I "swlibs\fxmisc" /I "swlibs\newpci\pcilib" /I "h5\glide3\src" /I "h5\minihwc" /I "swlibs\texus2\lib" /FI"winglide3.h" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "GLIDE3X_EXPORTS" /D __MSC__=1 /D "__WIN32__" /YX /FD /c
# ADD CPP /nologo /MT /W3 /GX /O2 /I ".\\" /I "h5\incsrc" /I "swlibs\fxmisc" /I "swlibs\newpci\pcilib" /I "h5\glide3\src" /I "h5\minihwc" /I "swlibs\texus2\lib" /FI"winglide3.h" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "GLIDE3X_EXPORTS" /D __MSC__=1 /D "__WIN32__" /YX /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0xc09 /i ".\\" /i "h5\incsrc" /i "swlibs\fxmisc" /i "swlibs\newpci\pcilib" /i "h5\glide3\src" /i "h5\minihwc" /d "NDEBUG" /d __MSC__=1 /d __WIN32__=1 /d "H3" /d GLIDE3=1
# ADD RSC /l 0xc09 /i ".\\" /i "h5\incsrc" /i "swlibs\fxmisc" /i "swlibs\newpci\pcilib" /i "h5\glide3\src" /i "h5\minihwc" /d "NDEBUG" /d GLIDE3=1 /d __MSC__=1 /d __WIN32__=1 /d "H3" /d "H4" /d "H5"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib ddraw.lib dxguid.lib /nologo /dll /pdb:none /machine:I386 /release /OPT:REF
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib ddraw.lib dxguid.lib /nologo /base:"0x18000000" /version:3.20 /dll /pdb:none /machine:I386 /release /OPT:REF
# Begin Custom Build
TargetPath=.\glide3x.dll
TargetName=glide3x
InputPath=.\glide3x.dll
SOURCE="$(InputPath)"

"c:\windows\system\$(TargetName).dll" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	copy $(TargetPath) c:\windows\system\$(TargetName).dll

# End Custom Build

!ELSEIF  "$(CFG)" == "glide3x - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "glide3x___Win32_Debug"
# PROP BASE Intermediate_Dir "glide3x___Win32_Debug"
# PROP BASE Ignore_Export_Lib 1
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /I ".\\" /I "h5\incsrc" /I "swlibs\fxmisc" /I "swlibs\newpci\pcilib" /I "h5\glide3\src" /I "h5\minihwc" /I "swlibs\texus2\lib" /FI"winglide3.h" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "GLIDE3X_EXPORTS" /D __MSC__=1 /D "__WIN32__" /FR /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /I ".\\" /I "h5\incsrc" /I "swlibs\fxmisc" /I "swlibs\newpci\pcilib" /I "h5\glide3\src" /I "h5\minihwc" /I "swlibs\texus2\lib" /FI"winglide3.h" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "GLIDE3X_EXPORTS" /D __MSC__=1 /D "__WIN32__" /FR /YX /FD /GZ /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0xc09 /i ".\\" /i "h5\incsrc" /i "swlibs\fxmisc" /i "swlibs\newpci\pcilib" /i "h5\glide3\src" /i "h5\minihwc" /d "_DEBUG" /d __MSC__=1 /d __WIN32__=1 /d "H3" /d GLIDE3=1 /d DEBUG=1
# ADD RSC /l 0xc09 /i ".\\" /i "h5\incsrc" /i "swlibs\fxmisc" /i "swlibs\newpci\pcilib" /i "h5\glide3\src" /i "h5\minihwc" /d "_DEBUG" /d DEBUG=1 /d __MSC__=1 /d __WIN32__=1
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib ddraw.lib dxguid.lib /nologo /dll /debug /machine:I386 /pdbtype:sept
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib ddraw.lib dxguid.lib /nologo /base:"0x18000000" /version:3.20 /dll /debug /machine:I386 /out:"./glide3x.dll" /pdbtype:sept
# SUBTRACT LINK32 /pdb:none

!ELSEIF  "$(CFG)" == "glide3x - Win32 Release No ASM"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "glide3x___Win32_Release_No_ASM"
# PROP BASE Intermediate_Dir "glide3x___Win32_Release_No_ASM"
# PROP BASE Ignore_Export_Lib 1
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "."
# PROP Intermediate_Dir "glide3x___Win32_Release_No_ASM"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MT /W3 /GX /O2 /I ".\\" /I "h5\incsrc" /I "swlibs\fxmisc" /I "swlibs\newpci\pcilib" /I "h5\glide3\src" /I "h5\minihwc" /I "swlibs\texus2\lib" /FI"winglide3.h" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "GLIDE3X_EXPORTS" /D __MSC__=1 /D "__WIN32__" /YX /FD /c
# ADD CPP /nologo /MT /W3 /GX /O2 /I ".\\" /I "h5\incsrc" /I "swlibs\fxmisc" /I "swlibs\newpci\pcilib" /I "h5\glide3\src" /I "h5\minihwc" /I "swlibs\texus2\lib" /FI"winglide3.h" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "GLIDE3X_EXPORTS" /D __MSC__=1 /D "__WIN32__" /D GLIDE_USE_C_TRISETUP=1 /YX /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0xc09 /i ".\\" /i "h5\incsrc" /i "swlibs\fxmisc" /i "swlibs\newpci\pcilib" /i "h5\glide3\src" /i "h5\minihwc" /d "NDEBUG" /d GLIDE3=1 /d __MSC__=1 /d __WIN32__=1 /d "H3" /d "H4" /d "H5"
# ADD RSC /l 0xc09 /i ".\\" /i "h5\incsrc" /i "swlibs\fxmisc" /i "swlibs\newpci\pcilib" /i "h5\glide3\src" /i "h5\minihwc" /d GLIDE3=1 /d __MSC__=1 /d __WIN32__=1 /d "H3" /d "H4" /d "H5" /d "NDEBUG" /d WINXP_FASTER_ALT_TAB_FIX=1 /d GLIDE_USE_C_TRISETUP=1
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib ddraw.lib dxguid.lib /nologo /base:"0x18000000" /version:3.20 /dll /pdb:none /machine:I386 /release /OPT:REF
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib ddraw.lib dxguid.lib /nologo /base:"0x18000000" /version:3.20 /dll /pdb:none /machine:I386 /release /OPT:REF

!ENDIF 

# Begin Target

# Name "glide3x - Win32 Release"
# Name "glide3x - Win32 Debug"
# Name "glide3x - Win32 Release No ASM"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Group "ASM Files"

# PROP Default_Filter "asm"
# Begin Source File

SOURCE=.\h5\glide3\src\xdraw2.asm

!IF  "$(CFG)" == "glide3x - Win32 Release"

# Begin Custom Build - Performing Custom Build Step on $(InputPath)
IntDir=.\Release
WkspDir=.
InputPath=.\h5\glide3\src\xdraw2.asm
InputName=xdraw2

BuildCmds= \
	ml.exe -I$(WkspDir) -nologo -c -coff -Cx -Fo$(IntDir)\$(InputName).obj $(InputPath) \
	ml.exe -I$(WkspDir) -DGL_AMD3D -nologo -c -coff -Cx -Fo$(IntDir)\$(InputName)_amd.obj $(InputPath) \
	

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"$(IntDir)\$(InputName)_amd.obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)
# End Custom Build

!ELSEIF  "$(CFG)" == "glide3x - Win32 Debug"

# Begin Custom Build - Performing Custom Build Step on $(InputPath)
IntDir=.\Debug
WkspDir=.
InputPath=.\h5\glide3\src\xdraw2.asm
InputName=xdraw2

BuildCmds= \
	ml.exe -I$(WkspDir) -nologo -c -coff -Cx -Fo$(IntDir)\$(InputName).obj $(InputPath) \
	ml.exe -I$(WkspDir) -DGL_AMD3D -nologo -c -coff -Cx -Fo$(IntDir)\$(InputName)_amd.obj $(InputPath) \
	

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"$(IntDir)\$(InputName)_amd.obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)
# End Custom Build

!ELSEIF  "$(CFG)" == "glide3x - Win32 Release No ASM"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\h5\glide3\src\xdraw3.asm

!IF  "$(CFG)" == "glide3x - Win32 Release"

# Begin Custom Build - Performing Custom Build Step on $(InputPath)
IntDir=.\Release
WkspDir=.
InputPath=.\h5\glide3\src\xdraw3.asm
InputName=xdraw3

BuildCmds= \
	ml.exe -I$(WkspDir) -nologo -c -coff -Cx -Fo$(IntDir)\$(InputName).obj $(InputPath) \
	ml.exe -I$(WkspDir) -DGL_AMD3D -nologo -c -coff -Cx -Fo$(IntDir)\$(InputName)_amd.obj $(InputPath) \
	

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"$(IntDir)\$(InputName)_amd.obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)
# End Custom Build

!ELSEIF  "$(CFG)" == "glide3x - Win32 Debug"

# Begin Custom Build - Performing Custom Build Step on $(InputPath)
IntDir=.\Debug
WkspDir=.
InputPath=.\h5\glide3\src\xdraw3.asm
InputName=xdraw3

BuildCmds= \
	ml.exe -I$(WkspDir) -nologo -c -coff -Cx -Fo$(IntDir)\$(InputName).obj $(InputPath) \
	ml.exe -I$(WkspDir) -DGL_AMD3D -nologo -c -coff -Cx -Fo$(IntDir)\$(InputName)_amd.obj $(InputPath) \
	

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"$(IntDir)\$(InputName)_amd.obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)
# End Custom Build

!ELSEIF  "$(CFG)" == "glide3x - Win32 Release No ASM"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\h5\glide3\src\xtexdl.asm

!IF  "$(CFG)" == "glide3x - Win32 Release"

# Begin Custom Build - Performing Custom Build Step on $(InputPath)
IntDir=.\Release
WkspDir=.
InputPath=.\h5\glide3\src\xtexdl.asm
InputName=xtexdl

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml.exe -I$(WkspDir) -nologo -c -coff -Cx -Fo$(IntDir)\$(InputName).obj $(InputPath)

# End Custom Build

!ELSEIF  "$(CFG)" == "glide3x - Win32 Debug"

# Begin Custom Build - Performing Custom Build Step on $(InputPath)
IntDir=.\Debug
WkspDir=.
InputPath=.\h5\glide3\src\xtexdl.asm
InputName=xtexdl

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml.exe -I$(WkspDir) -nologo -c -coff -Cx -Fo$(IntDir)\$(InputName).obj $(InputPath)

# End Custom Build

!ELSEIF  "$(CFG)" == "glide3x - Win32 Release No ASM"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# End Group
# Begin Source File

SOURCE=.\h5\glide3\src\cpuid.c
# End Source File
# Begin Source File

SOURCE=.\H5\Glide3\Src\Diget.c
# End Source File
# Begin Source File

SOURCE=.\H5\Glide3\Src\Diglide.c
# End Source File
# Begin Source File

SOURCE=.\h5\glide3\src\digutex.c
# End Source File
# Begin Source File

SOURCE=.\H5\Glide3\Src\Disst.c
# End Source File
# Begin Source File

SOURCE=.\H5\Glide3\Src\Distate.c
# End Source File
# Begin Source File

SOURCE=.\H5\Glide3\Src\Distrip.c
# End Source File
# Begin Source File

SOURCE=.\H5\Glide3\Src\Ditex.c
# End Source File
# Begin Source File

SOURCE=.\H5\Glide3\Src\Fifo.c
# End Source File
# Begin Source File

SOURCE=.\H5\Glide3\Src\G3df.c
# End Source File
# Begin Source File

SOURCE=.\H5\Glide3\Src\Gaa.c
# End Source File
# Begin Source File

SOURCE=.\H5\Glide3\Src\Gbanner.c
# End Source File
# Begin Source File

SOURCE=.\H5\Glide3\Src\Gdraw.c
# End Source File
# Begin Source File

SOURCE=.\H5\Glide3\Src\Gerror.c
# End Source File
# Begin Source File

SOURCE=.\H5\Glide3\Src\Gglide.c
# End Source File
# Begin Source File

SOURCE=.\H5\Glide3\Src\Glfb.c
# End Source File
# Begin Source File

SOURCE=.\glidexp.rc
# End Source File
# Begin Source File

SOURCE=.\H5\Glide3\Src\Gpci.c
# End Source File
# Begin Source File

SOURCE=.\H5\Glide3\Src\Gsfc.c
# End Source File
# Begin Source File

SOURCE=.\H5\Glide3\Src\Gsplash.c
# End Source File
# Begin Source File

SOURCE=.\H5\Glide3\Src\Gsst.c
# End Source File
# Begin Source File

SOURCE=.\H5\Glide3\Src\Gstrip.c
# End Source File
# Begin Source File

SOURCE=.\H5\Glide3\Src\Gtex.c
# End Source File
# Begin Source File

SOURCE=.\H5\Glide3\Src\Gtexdl.c
# End Source File
# Begin Source File

SOURCE=.\H5\Glide3\Src\Gthread.c
# End Source File
# Begin Source File

SOURCE=.\H5\Glide3\Src\Gu.c
# End Source File
# Begin Source File

SOURCE=.\h5\glide3\src\guclip.c
# End Source File
# Begin Source File

SOURCE=.\h5\glide3\src\gutex.c
# End Source File
# Begin Source File

SOURCE=.\H5\Glide3\Src\Gxdraw.c

!IF  "$(CFG)" == "glide3x - Win32 Release"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "glide3x - Win32 Debug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "glide3x - Win32 Release No ASM"

# PROP BASE Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\H5\GLIDE3\SRC\Winsurf.cpp
# End Source File
# Begin Source File

SOURCE=.\H5\Glide3\Src\xtexdl_def.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=.\h5\glide3\src\cpuid.h
# End Source File
# Begin Source File

SOURCE=.\H5\Glide3\Src\Fxcmd.h
# End Source File
# Begin Source File

SOURCE=.\H5\Glide3\Src\Fxglide.h
# End Source File
# Begin Source File

SOURCE=.\H5\Glide3\Src\Fxsplash.h
# End Source File
# Begin Source File

SOURCE=.\H5\Glide3\Src\G3ext.h
# End Source File
# Begin Source File

SOURCE=.\H5\Glide3\Src\Glide.h
# End Source File
# Begin Source File

SOURCE=.\H5\Glide3\Src\Glidesys.h
# End Source File
# Begin Source File

SOURCE=.\H5\Glide3\Src\Glideutl.h
# End Source File
# Begin Source File

SOURCE=.\H5\Glide3\Src\Gsfc.h
# End Source File
# Begin Source File

SOURCE=.\H5\Glide3\Src\Gsfctabl.h
# End Source File
# Begin Source File

SOURCE=.\H5\Glide3\Src\Gsstdef.h
# End Source File
# Begin Source File

SOURCE=.\gxpver.h
# End Source File
# Begin Source File

SOURCE=.\H5\Glide3\Src\macglide3.h
# End Source File
# Begin Source File

SOURCE=.\H5\Glide3\Src\Qmodes.h
# End Source File
# Begin Source File

SOURCE=.\H5\Glide3\Src\Tv.h
# End Source File
# Begin Source File

SOURCE=.\h5\glide3\src\winglide3.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# End Group
# Begin Group "Minihwc"

# PROP Default_Filter ""
# Begin Group "Headers"

# PROP Default_Filter "*.h"
# Begin Source File

SOURCE=.\H5\Minihwc\Fxhwc.h
# End Source File
# Begin Source File

SOURCE=.\H5\Minihwc\Hwcext.h
# End Source File
# Begin Source File

SOURCE=.\H5\Minihwc\Hwcio.h
# End Source File
# Begin Source File

SOURCE=.\H5\Minihwc\Initvga.h
# End Source File
# Begin Source File

SOURCE=.\H5\Minihwc\Lindri.h
# End Source File
# Begin Source File

SOURCE=.\H5\Minihwc\Minihwc.h
# End Source File
# Begin Source File

SOURCE=.\H5\Minihwc\Qmodes.h
# End Source File
# Begin Source File

SOURCE=.\H5\Minihwc\Setmode.h
# End Source File
# Begin Source File

SOURCE=.\H5\Minihwc\Tv.h
# End Source File
# End Group
# Begin Source File

SOURCE=.\H5\Minihwc\Dxdrvr.c
# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=.\H5\Minihwc\Gdebug.c
# End Source File
# Begin Source File

SOURCE=.\H5\Minihwc\Hwcio.c
# End Source File
# Begin Source File

SOURCE=.\H5\Minihwc\Minihwc.c
# End Source File
# Begin Source File

SOURCE=.\H5\Minihwc\nt6ksli_mode.c
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=.\H5\Minihwc\Win_mode.c
# End Source File
# Begin Source File

SOURCE=.\H5\Minihwc\win_regpath.c

!IF  "$(CFG)" == "glide3x - Win32 Release"

# ADD CPP /Od /Ob0

!ELSEIF  "$(CFG)" == "glide3x - Win32 Debug"

!ELSEIF  "$(CFG)" == "glide3x - Win32 Release No ASM"

# ADD CPP /Od /Ob0

!ENDIF 

# End Source File
# End Group
# Begin Group "Swlibs"

# PROP Default_Filter ""
# Begin Group "FxMisc"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\SWLIBS\FXMISC\3DFX.H
# End Source File
# Begin Source File

SOURCE=.\SWLIBS\FXMISC\FXDLL.H
# End Source File
# Begin Source File

SOURCE=.\SWLIBS\FXMISC\FXVER.H
# End Source File
# End Group
# Begin Group "PCILib"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\SWLIBS\NEWPCI\PCILIB\FXPCI.H
# End Source File
# Begin Source File

SOURCE=.\SWLIBS\NEWPCI\PCILIB\PCILIB.H
# End Source File
# End Group
# End Group
# Begin Group "Incsrc"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\h5\incsrc\cmddefs.h
# End Source File
# Begin Source File

SOURCE=.\h5\incsrc\fxhal.h
# End Source File
# Begin Source File

SOURCE=.\h5\incsrc\fxvid.h
# End Source File
# Begin Source File

SOURCE=.\h5\incsrc\gdebug.h
# End Source File
# Begin Source File

SOURCE=.\h5\incsrc\h3.h
# End Source File
# Begin Source File

SOURCE=.\h5\incsrc\h3cinit.h
# End Source File
# Begin Source File

SOURCE=.\h5\incsrc\h3defs.h
# End Source File
# Begin Source File

SOURCE=.\h5\incsrc\h3gdefs.h
# End Source File
# Begin Source File

SOURCE=.\h5\incsrc\h3hwc.h
# End Source File
# Begin Source File

SOURCE=.\h5\incsrc\h3info.h
# End Source File
# Begin Source File

SOURCE=.\h5\incsrc\h3regs.h
# End Source File
# Begin Source File

SOURCE=.\h5\incsrc\sst1vid.h
# End Source File
# Begin Source File

SOURCE=.\h5\incsrc\vector.h
# End Source File
# Begin Source File

SOURCE=.\h5\incsrc\vxd.h
# End Source File
# End Group
# Begin Source File

SOURCE=.\ChangeLog.txt
# End Source File
# End Target
# End Project
