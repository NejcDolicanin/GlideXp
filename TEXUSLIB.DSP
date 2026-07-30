# Microsoft Developer Studio Project File - Name="texuslib" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=texuslib - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "texuslib.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "texuslib.mak" CFG="texuslib - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "texuslib - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "texuslib - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE "texuslib - Win32 Release No ASM" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "texuslib - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release\texuslib"
# PROP Intermediate_Dir "Release\texuslib"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD CPP /nologo /W3 /GX /O2 /I ".\\" /I "h5\incsrc" /I "swlibs\fxmisc" /I "swlibs\newpci\pcilib" /I "h5\glide3\src" /I "h5\minihwc" /I "swlibs\texus2\lib" /FI"winglide3.h" /D "NDEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /D __MSC__=1 /D "__WIN32__" /YX /FD /c
# ADD BASE RSC /l 0xc09 /d "NDEBUG"
# ADD RSC /l 0xc09 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "texuslib - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug\texuslib"
# PROP Intermediate_Dir "Debug\texuslib"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /W3 /Gm /GX /ZI /Od /I ".\\" /I "h5\incsrc" /I "swlibs\fxmisc" /I "swlibs\newpci\pcilib" /I "h5\glide3\src" /I "h5\minihwc" /I "swlibs\texus2\lib" /FI"winglide3.h" /D "_DEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /D __MSC__=1 /D "__WIN32__" /YX /FD /GZ /c
# ADD BASE RSC /l 0xc09 /d "_DEBUG"
# ADD RSC /l 0xc09 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "texuslib - Win32 Release No ASM"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "texuslib___Win32_Release_No_ASM"
# PROP BASE Intermediate_Dir "texuslib___Win32_Release_No_ASM"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "texuslib___Win32_Release_No_ASM"
# PROP Intermediate_Dir "texuslib___Win32_Release_No_ASM"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /I ".\\" /I "h5\incsrc" /I "swlibs\fxmisc" /I "swlibs\newpci\pcilib" /I "h5\glide3\src" /I "h5\minihwc" /I "swlibs\texus2\lib" /FI"winglide3.h" /D "NDEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /D __MSC__=1 /D "__WIN32__" /YX /FD /c
# ADD CPP /nologo /W3 /GX /O2 /I ".\\" /I "h5\incsrc" /I "swlibs\fxmisc" /I "swlibs\newpci\pcilib" /I "h5\glide3\src" /I "h5\minihwc" /I "swlibs\texus2\lib" /FI"winglide3.h" /D "_LIB" /D __MSC__=1 /D "__WIN32__" /D "NDEBUG" /D "WIN32" /D "_MBCS" /D GLIDE_USE_C_TRISETUP=1 /YX /FD /c
# ADD BASE RSC /l 0xc09 /d "NDEBUG"
# ADD RSC /l 0xc09 /d "NDEBUG" /d WINXP_FASTER_ALT_TAB_FIX=1
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ENDIF 

# Begin Target

# Name "texuslib - Win32 Release"
# Name "texuslib - Win32 Debug"
# Name "texuslib - Win32 Release No ASM"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\SWLIBS\TEXUS2\LIB\3DF.C
# End Source File
# Begin Source File

SOURCE=.\SWLIBS\TEXUS2\LIB\BITCODER.C
# End Source File
# Begin Source File

SOURCE=.\SWLIBS\TEXUS2\LIB\CLAMP.C
# End Source File
# Begin Source File

SOURCE=.\SWLIBS\TEXUS2\LIB\CODEC.C
# End Source File
# Begin Source File

SOURCE=.\SWLIBS\TEXUS2\LIB\DEQUANT.C
# End Source File
# Begin Source File

SOURCE=.\SWLIBS\TEXUS2\LIB\DIFFUSE.C
# End Source File
# Begin Source File

SOURCE=.\SWLIBS\TEXUS2\LIB\EIGEN.C
# End Source File
# Begin Source File

SOURCE=.\SWLIBS\TEXUS2\LIB\MIPMAP.C
# End Source File
# Begin Source File

SOURCE=.\SWLIBS\TEXUS2\LIB\NCC.C
# End Source File
# Begin Source File

SOURCE=.\SWLIBS\TEXUS2\LIB\NCCNNET.C
# End Source File
# Begin Source File

SOURCE=.\SWLIBS\TEXUS2\LIB\PAL256.C
# End Source File
# Begin Source File

SOURCE=.\SWLIBS\TEXUS2\LIB\PAL6666.C
# End Source File
# Begin Source File

SOURCE=.\SWLIBS\TEXUS2\LIB\PPM.C
# End Source File
# Begin Source File

SOURCE=.\SWLIBS\TEXUS2\LIB\QUANTIZE.C
# End Source File
# Begin Source File

SOURCE=.\SWLIBS\TEXUS2\LIB\READ.C
# End Source File
# Begin Source File

SOURCE=.\SWLIBS\TEXUS2\LIB\RESAMPLE.C
# End Source File
# Begin Source File

SOURCE=.\SWLIBS\TEXUS2\LIB\RGT.C
# End Source File
# Begin Source File

SOURCE=.\SWLIBS\TEXUS2\LIB\TEXUSLIB.C
# End Source File
# Begin Source File

SOURCE=.\SWLIBS\TEXUS2\LIB\TGA.C
# End Source File
# Begin Source File

SOURCE=.\SWLIBS\TEXUS2\LIB\TXS.C
# End Source File
# Begin Source File

SOURCE=.\SWLIBS\TEXUS2\LIB\UTIL.C
# End Source File
# Begin Source File

SOURCE=.\SWLIBS\TEXUS2\LIB\WRITE.C
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=.\SWLIBS\TEXUS2\LIB\SST2COMP.H
# End Source File
# Begin Source File

SOURCE=.\SWLIBS\TEXUS2\LIB\SST2FXT1.H
# End Source File
# Begin Source File

SOURCE=.\SWLIBS\TEXUS2\LIB\TEXUS.H
# End Source File
# Begin Source File

SOURCE=.\SWLIBS\TEXUS2\LIB\TEXUSINT.H
# End Source File
# Begin Source File

SOURCE=.\SWLIBS\TEXUS2\LIB\VIEW.C
# End Source File
# End Group
# End Target
# End Project
