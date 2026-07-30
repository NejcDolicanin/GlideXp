#if !defined(GDBG_INFO_ON) || (GDBG_INFO_ON == 0)
#if defined(GDBG_INFO_ON)
#undef GDBG_INFO_ON
#endif /* defined(GDBG_INFO_ON) */
#define GDBG_INFO_ON
#endif /* !defined(GDBG_INFO_ON) || (GDBG_INFO_ON == 0) */

#include <stddef.h>
#include <stdlib.h>
#include <math.h>

#include <3dfx.h>

#ifdef HWC_EXT_INIT
#include "hwcext.h"
#else
#include <fxpci.h>
#endif

#if macintosh
#include <h3cinitdd_mac.h>
#endif
#include <h3cinit.h>
#include <minihwc.h>
#include "hwcio.h"
#include "setmode.h"

#ifdef __WIN32__
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <ddraw.h>
#include "qmodes.h"
#define IS_32
#define Not_VxD
#include <minivdd.h>
#include <vmm.h>
#include <configmg.h>

#endif

char * getRegPath() 
{
	char *retVal = NULL;
	OSVERSIONINFO ovi;
	
	GDBG_INFO(80, "getRegPath\n");
	
	ovi.dwOSVersionInfoSize = sizeof ( ovi );
	GetVersionEx ( &ovi );
	if (ovi.dwPlatformId == VER_PLATFORM_WIN32_NT) {
		HKEY hKey;
		DWORD type ;
		static char strval[255];
		DWORD szData = sizeof(strval) ;
		
		GDBG_INFO(80, "OS == WNT\n");
		
		/* Go fishing for the registry path on Win2K */
		if (RegOpenKey(HKEY_LOCAL_MACHINE, "HARDWARE\\DEVICEMAP\\VIDEO", &hKey) == ERROR_SUCCESS)
		{
			if (RegQueryValueEx(hKey, "\\Device\\Video0", NULL, &type, strval, &szData) ==
				ERROR_SUCCESS)
			{
				if (type != REG_SZ)
				{
				/* It is hardcoded on NT via Display Control code. see:
					* $/devel/swtools/bansheecp2 */
					retVal = "SYSTEM\\CurrentControlSet\\Services\\3Dfx\\Device0\\glide";
				}
				else
				{
					strcat(strval, "\\glide") ;
					retVal = (char*)((int)strval + strlen("\\REGISTRY\\Machine\\")) ;
				}
			}
			else
				retVal = "SYSTEM\\CurrentControlSet\\Services\\3Dfx\\Device0\\glide";
			
			RegCloseKey(hKey);
		}
	}
	else {
		QDEVNODE QDevNode;
		QIN Qin;
		int status;
		
		GDBG_INFO(80, "OS == W9X\n");
		//printf("OS == W9X\n");
		
		Qin.dwSubFunc = QUERYDEVNODE;
		{
			HDC curDC = GetDC(NULL);
			
			status = ExtEscape ( curDC, QUERYESCMODE, 
				sizeof(Qin), (LPCSTR)&Qin, 
				sizeof(QDevNode), (LPSTR)&QDevNode );
			ReleaseDC(NULL, curDC);
		}
		
		if ( status > 0 ) {
			static char regPath[255];
			
			//printf("QDevNode.dwDevNode == %x\n", QDevNode.dwDevNode);
			
			CM_Get_DevNode_Key( QDevNode.dwDevNode, NULL, 
				&regPath, sizeof(regPath), 
				CM_REGISTRY_SOFTWARE );
			//printf("regpath == %s\n", regPath);
			
			if (regPath[0]) {
				strcat(regPath, "\\glide");
				retVal = regPath;
			}
		}
		if (retVal == NULL)
		{
			retVal = "SYSTEM\\CurrentControlSet\\Services\\Class\\Display\\0000\\Glide";
		}
		
	}
	GDBG_INFO(80, "getRegPath:  retVal = %s\n", retVal);
	
	return retVal;
} /* getRegPath */

char * getModesRegPath() 
{
	char *retVal = NULL;
	OSVERSIONINFO ovi;
	
	ovi.dwOSVersionInfoSize = sizeof ( ovi );
	GetVersionEx ( &ovi );
	if (ovi.dwPlatformId == VER_PLATFORM_WIN32_NT) {
		HKEY hKey;
		DWORD type ;
		static char strval[255];
		DWORD szData = sizeof(strval) ;
		
		GDBG_INFO(80, "OS == WNT\n");
		
		/* Go fishing for the registry path on Win2K */
		if (RegOpenKey(HKEY_LOCAL_MACHINE, "HARDWARE\\DEVICEMAP\\VIDEO", &hKey) == ERROR_SUCCESS)
		{
			if (RegQueryValueEx(hKey, "\\Device\\Video0", NULL, &type, strval, &szData) ==
				ERROR_SUCCESS)
			{
				if (type != REG_SZ)
				{
				/* It is hardcoded on NT via Display Control code. see:
					* $/devel/swtools/bansheecp2 */
					retVal = "SYSTEM\\CurrentControlSet\\Services\\3Dfx\\Device0\\modes\\";
				}
				else
				{
					strcat(strval, "\\modes\\") ;
					retVal = (char*)((int)strval + strlen("\\REGISTRY\\Machine\\")) ;
				}
			}
			else
				retVal = "SYSTEM\\CurrentControlSet\\Services\\3Dfx\\Device0\\modes\\";
			
			RegCloseKey(hKey);
		}
	}
	else {
		QDEVNODE QDevNode;
		QIN Qin;
		int status;
		
		GDBG_INFO(80, "OS == W9X\n");
		
		Qin.dwSubFunc = QUERYDEVNODE;
		{
			HDC curDC = GetDC(NULL);
			
			status = ExtEscape ( curDC, QUERYESCMODE, 
				sizeof(Qin), (LPCSTR)&Qin, 
				sizeof(QDevNode), (LPSTR)&QDevNode );
			ReleaseDC(NULL, curDC);
		}
		
		if ( status > 0 ) {
			static char regPath[255];
			
			CM_Get_DevNode_Key( QDevNode.dwDevNode, NULL, 
				&regPath, sizeof(regPath), 
				CM_REGISTRY_SOFTWARE );
			
			if (regPath[0]) {
				strcat(regPath, "\\modes\\");
				retVal = regPath;
			}
		}
		if (retVal == NULL)
		{
			retVal = "SYSTEM\\CurrentControlSet\\Services\\Class\\Display\\0000\\modes\\";
		}
	}
	
	return retVal;
} /* getModesRegPath */
