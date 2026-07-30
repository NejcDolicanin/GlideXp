# Microsoft Developer Studio Project File - Name="FxASM" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Console Application" 0x0103

CFG=FxASM - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "FxASM.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "FxASM.mak" CFG="FxASM - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "FxASM - Win32 Debug" (based on "Win32 (x86) Console Application")
!MESSAGE "FxASM - Win32 Release" (based on "Win32 (x86) Console Application")
!MESSAGE "FxASM - Win32 Release No ASM" (based on "Win32 (x86) Console Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "FxASM - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "FxASM___Win32_Debug"
# PROP BASE Intermediate_Dir "FxASM___Win32_Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug\FxASM"
# PROP Intermediate_Dir "Debug\FxASM"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /I ".\\" /I "h5\incsrc" /I "swlibs\fxmisc" /I "swlibs\newpci\pcilib" /I "h5\glide3\src" /I "h5\minihwc" /FI"winglide3.h" /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /D __MSC__=1 /D "__WIN32__" /YX /FD /GZ /c
# ADD CPP /nologo /W3 /Gm /GX /ZI /Od /I ".\\" /I "h5\incsrc" /I "swlibs\fxmisc" /I "swlibs\newpci\pcilib" /I "h5\glide3\src" /I "h5\minihwc" /FI"winglide3.h" /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /D __MSC__=1 /D "__WIN32__" /YX /FD /GZ /c
# ADD BASE RSC /l 0xc09 /d "_DEBUG"
# ADD RSC /l 0xc09 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept
# Begin Custom Build
WkspDir=.
TargetPath=.\Debug\FxASM\FxASM.exe
InputPath=.\Debug\FxASM\FxASM.exe
SOURCE="$(InputPath)"

BuildCmds= \
	$(TargetPath) -inline > $(WkspDir)\fxinline.h \
	$(TargetPath) -hex > $(WkspDir)\fxgasm.h \
	

"$(WkspDir)\fxinline.h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"$(WkspDir)\fxgasm.h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)
# End Custom Build

!ELSEIF  "$(CFG)" == "FxASM - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "FxASM___Win32_Release"
# PROP BASE Intermediate_Dir "FxASM___Win32_Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release\FxASM"
# PROP Intermediate_Dir "Release\FxASM"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /I ".\\" /I "h5\incsrc" /I "swlibs\fxmisc" /I "swlibs\newpci\pcilib" /I "h5\glide3\src" /I "h5\minihwc" /FI"winglide3.h" /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /D __MSC__=1 /D "__WIN32__" /YX /FD /c
# ADD CPP /nologo /W3 /GX /O2 /I ".\\" /I "h5\incsrc" /I "swlibs\fxmisc" /I "swlibs\newpci\pcilib" /I "h5\glide3\src" /I "h5\minihwc" /FI"winglide3.h" /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /D __MSC__=1 /D "__WIN32__" /YX /FD /c
# ADD BASE RSC /l 0xc09 /d "NDEBUG"
# ADD RSC /l 0xc09 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386
# Begin Custom Build
WkspDir=.
TargetPath=.\Release\FxASM\FxASM.exe
InputPath=.\Release\FxASM\FxASM.exe
SOURCE="$(InputPath)"

BuildCmds= \
	$(TargetPath) -inline > $(WkspDir)\fxinline.h \
	$(TargetPath) -hex > $(WkspDir)\fxgasm.h \
	

"$(WkspDir)\fxinline.h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"$(WkspDir)\fxgasm.h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)
# End Custom Build

!ELSEIF  "$(CFG)" == "FxASM - Win32 Release No ASM"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "FxASM___Win32_Release_No_ASM"
# PROP BASE Intermediate_Dir "FxASM___Win32_Release_No_ASM"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "FxASM___Win32_Release_No_ASM"
# PROP Intermediate_Dir "FxASM___Win32_Release_No_ASM"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /I ".\\" /I "h5\incsrc" /I "swlibs\fxmisc" /I "swlibs\newpci\pcilib" /I "h5\glide3\src" /I "h5\minihwc" /FI"winglide3.h" /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /D __MSC__=1 /D "__WIN32__" /YX /FD /c
# ADD CPP /nologo /W3 /GX /O2 /I ".\\" /I "h5\incsrc" /I "swlibs\fxmisc" /I "swlibs\newpci\pcilib" /I "h5\glide3\src" /I "h5\minihwc" /FI"winglide3.h" /D "_CONSOLE" /D __MSC__=1 /D "__WIN32__" /D "NDEBUG" /D "WIN32" /D "_MBCS" /D GLIDE_USE_C_TRISETUP=1 /YX /FD /c
# ADD BASE RSC /l 0xc09 /d "NDEBUG"
# ADD RSC /l 0xc09 /d "NDEBUG" /d WINXP_FASTER_ALT_TAB_FIX=1
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386
# Begin Custom Build
WkspDir=.
TargetPath=.\FxASM___Win32_Release_No_ASM\FxASM.exe
InputPath=.\FxASM___Win32_Release_No_ASM\FxASM.exe
SOURCE="$(InputPath)"

BuildCmds= \
	$(TargetPath) -inline > $(WkspDir)\fxinline.h \
	$(TargetPath) -hex > $(WkspDir)\fxgasm.h \
	

"$(WkspDir)\fxinline.h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"$(WkspDir)\fxgasm.h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)
# End Custom Build

!ENDIF 

# Begin Target

# Name "FxASM - Win32 Debug"
# Name "FxASM - Win32 Release"
# Name "FxASM - Win32 Release No ASM"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\H5\Glide3\Src\Fxgasm.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# End Group
# End Target
# End Project
