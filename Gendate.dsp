# Microsoft Developer Studio Project File - Name="Gendate" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Console Application" 0x0103

CFG=Gendate - Win32 Release
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "Gendate.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "Gendate.mak" CFG="Gendate - Win32 Release"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "Gendate - Win32 Release" (based on "Win32 (x86) Console Application")
!MESSAGE "Gendate - Win32 Debug" (based on "Win32 (x86) Console Application")
!MESSAGE "Gendate - Win32 Release No ASM" (based on "Win32 (x86) Console Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "Gendate - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Gendate___Win32_Release"
# PROP BASE Intermediate_Dir "Gendate___Win32_Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release\Gendate"
# PROP Intermediate_Dir "Release\Gendate"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /D "WIN32" /D GLIDE2=1 /YX /FD /c
# ADD CPP /nologo /W3 /GX /O2 /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /D "WIN32" /D GLIDE2=1 /YX /FD /c
# ADD BASE RSC /l 0xc09 /d "NDEBUG"
# ADD RSC /l 0xc09 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386
# Begin Custom Build
ProjDir=.
TargetPath=.\Release\Gendate\Gendate.exe
InputPath=.\Release\Gendate\Gendate.exe
SOURCE="$(InputPath)"

"$(ProjDir)\gendate.h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	$(TargetPath) > $(ProjDir)\gendate.h

# End Custom Build

!ELSEIF  "$(CFG)" == "Gendate - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Gendate___Win32_Debug"
# PROP BASE Intermediate_Dir "Gendate___Win32_Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Debug\Gendate"
# PROP Intermediate_Dir "Debug\Gendate"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /D "WIN32" /D GLIDE2=1 /YX /FD /c
# ADD CPP /nologo /W3 /GX /O2 /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /D "WIN32" /D GLIDE2=1 /YX /FD /c
# ADD BASE RSC /l 0xc09 /d "NDEBUG"
# ADD RSC /l 0xc09 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386
# Begin Custom Build
ProjDir=.
TargetPath=.\Debug\Gendate\Gendate.exe
InputPath=.\Debug\Gendate\Gendate.exe
SOURCE="$(InputPath)"

"$(ProjDir)\gendate.h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	$(TargetPath) > $(ProjDir)\gendate.h

# End Custom Build

!ELSEIF  "$(CFG)" == "Gendate - Win32 Release No ASM"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Gendate___Win32_Release_No_ASM"
# PROP BASE Intermediate_Dir "Gendate___Win32_Release_No_ASM"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Gendate___Win32_Release_No_ASM"
# PROP Intermediate_Dir "Gendate___Win32_Release_No_ASM"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /D "WIN32" /D GLIDE2=1 /YX /FD /c
# ADD CPP /nologo /W3 /GX /O2 /D "_CONSOLE" /D GLIDE2=1 /D "NDEBUG" /D "WIN32" /D "_MBCS" /D GLIDE_USE_C_TRISETUP=1 /YX /FD /c
# ADD BASE RSC /l 0xc09 /d "NDEBUG"
# ADD RSC /l 0xc09 /d "NDEBUG" /d WINXP_FASTER_ALT_TAB_FIX=1
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386
# Begin Custom Build
ProjDir=.
TargetPath=.\Gendate___Win32_Release_No_ASM\Gendate.exe
InputPath=.\Gendate___Win32_Release_No_ASM\Gendate.exe
SOURCE="$(InputPath)"

"$(ProjDir)\gendate.h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	$(TargetPath) > $(ProjDir)\gendate.h

# End Custom Build

!ENDIF 

# Begin Target

# Name "Gendate - Win32 Release"
# Name "Gendate - Win32 Debug"
# Name "Gendate - Win32 Release No ASM"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\h5\incsrc\gendate.c
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
