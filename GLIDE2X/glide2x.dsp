# Microsoft Developer Studio Project File - Name="glide2x" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

CFG=glide2x - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "glide2x.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "glide2x.mak" CFG="glide2x - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "glide2x - Win32 Release" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "glide2x - Win32 Debug" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "glide2x - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir ".."
# PROP Intermediate_Dir "Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "GLIDE2X_EXPORTS" /YX /FD /c
# ADD CPP /nologo /MT /W3 /vd0 /O2 /I ".." /I "..\h5\incsrc" /I "..\swlibs\fxmisc" /I "..\h5\glide3\src" /I "..\swlibs\newpci\pcilib" /I "..\h5\minihwc" /I "..\swlibs\texus2\lib" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "GLIDE2X_EXPORTS" /D "G2_TRANS" /Yu"g2pch.h" /FD /c
# SUBTRACT CPP /Fr
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0xc09 /d "NDEBUG"
# ADD RSC /l 0xc09 /i ".." /i "..\h5\incsrc" /i "..\swlibs\fxmisc" /i "..\swlibs\newpci\pcilib" /i "..\h5\glide3\src" /i "..\h5\minihwc" /d "NDEBUG" /d "H3" /d "H4" /d "H5" /d __MSC__=1 /d __WIN32__=1 /d "G2_TRANS"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /base:"0x1A000000" /version:1.0 /dll /pdb:none /machine:I386 /OPT:REF

!ELSEIF  "$(CFG)" == "glide2x - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "GLIDE2X_EXPORTS" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /Gm /vd0 /ZI /Od /I ".." /I "..\h5\incsrc" /I "..\swlibs\fxmisc" /I "..\h5\glide3\src" /I "..\swlibs\newpci\pcilib" /I "..\h5\minihwc" /I "..\swlibs\texus2\lib" /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "GLIDE2X_EXPORTS" /D "G2_TRANS" /Yu"g2pch.h" /FD /GZ /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0xc09 /d "_DEBUG"
# ADD RSC /l 0xc09 /i ".." /i "..\h5\incsrc" /i "..\swlibs\fxmisc" /i "..\swlibs\newpci\pcilib" /i "..\h5\glide3\src" /i "..\h5\minihwc" /d "_DEBUG" /d DEBUG=1 /d __MSC__=1 /d __WIN32__=1 /d "G2_TRANS"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /debug /machine:I386 /pdbtype:sept
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /base:"0x1A000000" /version:0.1 /dll /debug /machine:I386 /out:"../glide2x.dll" /pdbtype:sept

!ENDIF 

# Begin Target

# Name "glide2x - Win32 Release"
# Name "glide2x - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\g2buffer.cpp
# End Source File
# Begin Source File

SOURCE=.\g2clip.cpp
# End Source File
# Begin Source File

SOURCE=.\g2color.cpp
# End Source File
# Begin Source File

SOURCE=.\g2debug.cpp
# End Source File
# Begin Source File

SOURCE=.\g2draw.cpp
# End Source File
# Begin Source File

SOURCE=.\g2fog.cpp
# End Source File
# Begin Source File

SOURCE=.\g2glide.cpp
# End Source File
# Begin Source File

SOURCE=.\g2gutex.cpp
# End Source File
# Begin Source File

SOURCE=.\g2impfuncs.cpp
# End Source File
# Begin Source File

SOURCE=.\g2main.cpp
# End Source File
# Begin Source File

SOURCE=.\g2misc.cpp
# End Source File
# Begin Source File

SOURCE=.\g2pch.cpp
# ADD CPP /Yc"g2pch.h"
# End Source File
# Begin Source File

SOURCE=.\g2sst.cpp
# End Source File
# Begin Source File

SOURCE=.\g2state.cpp
# End Source File
# Begin Source File

SOURCE=.\g2tex.cpp
# End Source File
# Begin Source File

SOURCE=.\g2utils.cpp
# End Source File
# Begin Source File

SOURCE=..\glidexp.rc
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=.\g2buffer.h
# End Source File
# Begin Source File

SOURCE=.\g2clip.h
# End Source File
# Begin Source File

SOURCE=.\g2color.h
# End Source File
# Begin Source File

SOURCE=.\g2debug.h
# End Source File
# Begin Source File

SOURCE=.\g2draw.h
# End Source File
# Begin Source File

SOURCE=.\g2exports.inl
# End Source File
# Begin Source File

SOURCE=.\g2fog.h
# End Source File
# Begin Source File

SOURCE=.\g2glide.h
# End Source File
# Begin Source File

SOURCE=.\g2impfuncs.h
# End Source File
# Begin Source File

SOURCE=.\g2misc.h
# End Source File
# Begin Source File

SOURCE=.\g2pch.h
# End Source File
# Begin Source File

SOURCE=.\g2sst.h
# End Source File
# Begin Source File

SOURCE=.\g2state.h
# End Source File
# Begin Source File

SOURCE=.\g2tex.h
# End Source File
# Begin Source File

SOURCE=.\g2utils.h
# End Source File
# Begin Source File

SOURCE=..\gxpver.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# End Group
# Begin Source File

SOURCE=.\all_exports.txt
# End Source File
# End Target
# End Project
