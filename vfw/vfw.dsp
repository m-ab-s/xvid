# Microsoft Developer Studio Project File - Name="vfw" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

CFG=vfw - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "vfw.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "vfw.mak" CFG="vfw - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "vfw - Win32 Release" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "vfw - Win32 Debug" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "vfw - Win32 Release _SMP" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "vfw - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /YX /FD /c
# ADD CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /YX /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0xc09 /d "NDEBUG"
# ADD RSC /l 0xc09 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /machine:I386
# ADD LINK32 user32.lib comdlg32.lib comctl32.lib advapi32.lib gdi32.lib shell32.lib winmm.lib ..\xvidcore\build\win32\bin\core.lib /nologo /dll /machine:I386 /out:"bin\xvid.dll"

!ELSEIF  "$(CFG)" == "vfw - Win32 Debug"

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
# ADD BASE CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "BFRAMES" /YX /FD /GZ /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0xc09 /d "_DEBUG"
# ADD RSC /l 0xc09 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /debug /machine:I386 /pdbtype:sept
# ADD LINK32 user32.lib comdlg32.lib comctl32.lib advapi32.lib gdi32.lib shell32.lib winmm.lib ..\xvidcore\build\win32\bin\core.lib /nologo /dll /incremental:no /debug /machine:I386 /out:"bin\xvid.dll" /pdbtype:sept
# SUBTRACT LINK32 /pdb:none

!ELSEIF  "$(CFG)" == "vfw - Win32 Release _SMP"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "vfw___Win32_Release__SMP"
# PROP BASE Intermediate_Dir "vfw___Win32_Release__SMP"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release_SMP"
# PROP Intermediate_Dir "Release_SMP"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /YX /FD /c
# ADD CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_SMP" /YX /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0xc09 /d "NDEBUG"
# ADD RSC /l 0xc09 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 user32.lib comdlg32.lib comctl32.lib advapi32.lib gdi32.lib shell32.lib winmm.lib ..\xvidcore\build\win32\bin\core.lib /nologo /dll /machine:I386 /out:"bin\xvid.dll"
# ADD LINK32 user32.lib comdlg32.lib comctl32.lib advapi32.lib gdi32.lib shell32.lib winmm.lib ..\xvidcore\build\win32\bin\core.lib pthreadVC.lib /nologo /dll /machine:I386 /out:"bin\xvid.dll"

!ENDIF 

# Begin Target

# Name "vfw - Win32 Release"
# Name "vfw - Win32 Debug"
# Name "vfw - Win32 Release _SMP"
# Begin Group "doc"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\authors.txt
# End Source File
# Begin Source File

SOURCE=.\gpl.txt
# End Source File
# Begin Source File

SOURCE=.\help.txt
# End Source File
# Begin Source File

SOURCE=.\todo.txt
# End Source File
# End Group
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\src\2pass.c
# End Source File
# Begin Source File

SOURCE=.\src\codec.c
# End Source File
# Begin Source File

SOURCE=.\src\config.c
# End Source File
# Begin Source File

SOURCE=.\src\driverproc.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=.\src\2pass.h
# End Source File
# Begin Source File

SOURCE=.\src\codec.h
# End Source File
# Begin Source File

SOURCE=.\src\config.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# Begin Source File

SOURCE=.\src\config.rc
# End Source File
# Begin Source File

SOURCE=.\src\XviD_logo.bmp
# End Source File
# End Group
# Begin Group "Linker Defs"

# PROP Default_Filter "def"
# Begin Source File

SOURCE=.\src\driverproc.def
# End Source File
# End Group
# Begin Source File

SOURCE=..\xvidcore\build\win32\bin\core.lib
# End Source File
# End Target
# End Project
