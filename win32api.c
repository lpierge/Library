/*$
	win32api.c
	Implementazione di quanto omesso dall' API (SDK/MFC).
	Luca Piergentili, 13/09/98
	lpiergentili@yahoo.com

	Attenzione: se si incorpora questo file (.c) in un progetto C++/MFC, la inclusione del file "window.h" puo' generare, in cascata, la 
	inclusione di "windows.h" e "afx.h", con quest'ultimo generando l'errore: #error MFC requires C++ compilation (use a .cpp suffix)
	dato che il file ha estensione .c, oltre ad errori di vario tipo.
	Per risolvere o si rinomina il file in .cpp o si forza la compilazione in modalita' C++ con l'opzione relativa (/TP).
	(Proprieta' del progetto -> C++ -> Avanzato -> Compilare come... -> C++)
*/
#include "pragma.h"
#include "env.h"
#include "macro.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include "strings.h"
#include <time.h>
#include <limits.h>
#include "datetime.h"
#include "window.h"
#include "win32api.h"
#include <shellapi.h>
#include <shlwapi.h>

#include "traceexpr.h"
//#define _TRACE_FLAG			_TRFLAG_TRACEOUTPUT // opzioni: _TRFLAG_NOTRACE, _TRFLAG_TRACEFILE, _TRFLAG_TRACECONSOLE, _TRFLAG_TRACEOUTPUT, _TRFLAG_TRACEBREAKPOINT
#define _TRACE_FLAG			_TRFLAG_NOTRACE // opzioni: _TRFLAG_NOTRACE, _TRFLAG_TRACEFILE, _TRFLAG_TRACECONSOLE, _TRFLAG_TRACEOUTPUT, _TRFLAG_TRACEBREAKPOINT
#define _TRACE_FLAG_INFO	_TRACE_FLAG
#define _TRACE_FLAG_WARN	_TRACE_FLAG
#define _TRACE_FLAG_ERR		_TRACE_FLAG

/* per uso interno */
static UINT InternalMessageBox(HWND hWnd,LPCSTR lpcszText,LPCSTR lpcszTitle,UINT nStyle);

/*
	GetWindowsVersion()

	Ricava la versione di Windows (stringa descrittiva e numero versione major/minor).
	(vedi anche il codice in CWindowsVersion.cpp)

	Esempio:

	char szWindowsPlatform[128] = {0};
	DWORD dwMajorVersion=0L;
	DWORD dwMinorVersion=0L;

	OSVERSIONTYPE ostype = GetWindowsVersion(szWindowsPlatform,sizeof(szWindowsPlatform),&dwMajorVersion,&dwMinorVersion);
	printf(	"\nGetWindowsVersion()\n"\
			"OSVERSIONTYPE: %d\n"\
			"Windows platform: %s\n"\
			"OS ver major: %ld\n"\
			"OS ver minor: %ld\n"\
			"[Press Enter]\n",
			ostype,szWindowsPlatform,dwMajorVersion,dwMinorVersion);
	getchar();
*/
OSVERSIONTYPE GetWindowsVersion(LPSTR lpszWindowsPlatform,UINT nSize,LPDWORD pdwMajorVersion,LPDWORD pdwMinorVersion)
{
	ASSERTEXPR(lpszWindowsPlatform);
	ASSERTEXPR(nSize > 0);

	OSVERSIONINFOEX os = {0};
	os.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
	BOOL bOsVersionInfo = FALSE;
	OSVERSIONTYPE osversiontype = UNKNOWN_WINDOWS_VERSION;
	strcpyn(lpszWindowsPlatform,"UNKNOWN_WINDOWS_VERSION",nSize);

	if(!lpszWindowsPlatform || !(nSize > 0))
		return(osversiontype);

	/* try calling GetVersionEx using the OSVERSIONINFOEX structure, which is supported on Windows 2000
	if that fails, try using the OSVERSIONINFO structure */
	if((bOsVersionInfo = GetVersionEx((OSVERSIONINFO*)&os))==FALSE)
	{
		/* if OSVERSIONINFOEX doesn't work, try OSVERSIONINFO */
		memset(&os,'\0',sizeof(OSVERSIONINFO));
		os.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
		bOsVersionInfo = GetVersionEx((OSVERSIONINFO*)&os);
	}

	if(bOsVersionInfo)
	{
		/* service pack */
		char szServiceRelease[_MAX_PATH+1] = {0};
		strcpyn(szServiceRelease,os.szCSDVersion,sizeof(szServiceRelease));
		
		switch(os.dwPlatformId)
		{
			/* Win3.1 */
			case VER_PLATFORM_WIN32s:
				osversiontype = WINDOWS_31;
				strcpyn(lpszWindowsPlatform,"Microsoft® Windows 3.1 (TM)",nSize);
				break;
			
			/* Win95/98 */
			case VER_PLATFORM_WIN32_WINDOWS:
				if(os.dwMinorVersion==0)
				{
					osversiontype = WINDOWS_95;
					snprintf(lpszWindowsPlatform,nSize,"Microsoft® Windows 95 (TM) %s",szServiceRelease);
				}
				else if(os.dwMinorVersion==10)
				{
					osversiontype = WINDOWS_98;
					snprintf(lpszWindowsPlatform,nSize,"Microsoft® Windows 98 (TM) %s",szServiceRelease);
				}
				if(os.dwMinorVersion==90)
				{
					osversiontype = WINDOWS_MILLENNIUM;
					snprintf(lpszWindowsPlatform,nSize,"Microsoft® Windows Millenium (TM) %s",szServiceRelease);
				}
				break;
				
			/* WinNT */
			case VER_PLATFORM_WIN32_NT:
			{
				DWORD dwVersion = 1L;
				typedef DWORD (WINAPI* PRtlGetNtProductType) (PDWORD pVersion);
				PRtlGetNtProductType pfnRtlGetNtProductType = (PRtlGetNtProductType)GetProcAddress(GetModuleHandle("ntdll.dll"),"RtlGetNtProductType");
				if(pfnRtlGetNtProductType)
					pfnRtlGetNtProductType(&dwVersion);

				if(os.dwMajorVersion==4)
				{
					osversiontype = WINDOWS_NT;
					snprintf(lpszWindowsPlatform,nSize,"Microsoft® Windows NT 4.%ld %s (TM) %s",os.dwMinorVersion,dwVersion!=1L ? "Server" : "Workstation",szServiceRelease);
				}
				else if(os.dwMajorVersion==5)
				{
					osversiontype = os.dwMinorVersion==0 ? WINDOWS_2000 : WINDOWS_XP;
					snprintf(lpszWindowsPlatform,nSize,"Microsoft® Windows %s %s Edition (TM) %s",os.dwMinorVersion==0 ? "2000" : "XP",dwVersion!=1L ? "Professional" : "Home",szServiceRelease);
				}
				else if(os.dwMajorVersion==6)
				{
					osversiontype = WINDOWS_VISTA;
					snprintf(lpszWindowsPlatform,nSize,"Microsoft® Windows Vista %s Edition (TM) %s",dwVersion!=1L ? "Professional" : "Home",szServiceRelease);
				}
				else if(os.dwMajorVersion==7)
				{
					osversiontype = WINDOWS_SEVEN;
					snprintf(lpszWindowsPlatform,nSize,"Microsoft® Windows Seven %s Edition (TM) %s",dwVersion!=1L ? "Professional" : "Home",szServiceRelease);
				}
				
				break;
			}
		}
	
		*pdwMajorVersion = os.dwMajorVersion;
		*pdwMinorVersion = os.dwMinorVersion;
	}
	
	return(osversiontype);
}

/*
	GetWindowsVersionEx()

	Ricava la versione di Windows (riempie la struttura).
	(vedi anche il codice in CWindowsVersion.cpp)

	Tenere presente che esistono due strutture differenti: OSVERSIONINFO 
	(vedi funzione sopra) e OSVERSIONINFOEX (questa funzione).

	Esempio:

	OSVERSIONINFOEX osvi = {0};
	GetWindowsVersionEx(&osvi);
	printf("\nGetWindowsVersionEx()\n");
	printf("dwOSVersionInfoSize %ld\n",osvi.dwOSVersionInfoSize);
	printf("dwMajorVersion      %ld\n",osvi.dwMajorVersion);
	printf("dwMinorVersion      %ld\n",osvi.dwMinorVersion);
	printf("dwBuildNumber       %ld\n",osvi.dwBuildNumber);
	printf("dwPlatformId        %ld\n",osvi.dwPlatformId);
	printf("szCSDVersion        %s\n",osvi.szCSDVersion);
	printf("wServicePackMajor   %d\n",osvi.wServicePackMajor);
	printf("wServicePackMinor   %d\n",osvi.wServicePackMinor);
	printf("wSuiteMask          %d\n",osvi.wSuiteMask);
	printf("wProductType        %d\n",osvi.wProductType);
	printf("wReserved           %d\n",osvi.wReserved);
	printf("[Press Enter]\n");
	getchar();

	Restituisce TRUE se riesce, FALSE altrimenti.
*/
BOOL GetWindowsVersionEx(OSVERSIONINFOEX* osvi)
{
	ASSERTEXPR(osvi);
    memset(osvi,'\0',sizeof(OSVERSIONINFOEX));
    osvi->dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
	return(GetVersionEx((OSVERSIONINFO*)osvi));
}

/*
	GetWindowsVersionData()

	Ricava la versione di Windows (riempie dinamicamente l'array del chiamante).
	(vedi anche il codice in CWindowsVersion.cpp)

	OJO al redireccionamiento del registro WOW64 (Windows-on-Windows 64-bit)
	Aplicaciones de 32 bits en sistemas de 64 bits: si el programa C está compilado como una aplicación de 32 bits y se
	ejecuta en un sistema operativo Windows de 64 bits, Windows redirige automáticamente el acceso a ciertas claves del 
	registro.
	Wow6432Node: para las aplicaciones de 32 bits, las claves bajo:
	HKEY_LOCAL_MACHINE\SOFTWARE
	se redirigen a:
	HKEY_LOCAL_MACHINE\SOFTWARE\Wow6432Node
	Es decir, cuando el programa de 32 bits intenta leer:
	HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows NT\CurrentVersion
	en realidad está leyendo de:
	HKEY_LOCAL_MACHINE\SOFTWARE\Wow6432Node\Microsoft\Windows NT\CurrentVersion.
	Para acceder a la vista de 64 bits del Registro desde un programa de 32 bits, hay que usar el flag KEY_WOW64_64KEY al 
	abrir la clave del Registro con RegOpenKeyEx().

	_M_IX86: Se define cuando se está compilando para la arquitectura x86 (32 bits).
	_M_X64:	Se define cuando se está compilando para la arquitectura x64 (64 bits).
	_WIN64:	Se define cuando se está compilando para un entorno de 64 bits (es decir, Windows de 64 bits, ya sea x64 o ARM64, 
			aunque para C++ nativo en PC es prácticamente siempre x64).

	Esempio: creare l'array con un elemento per ogni chiave di interesse, la GetWindowsVersionData()
			 si incarica di ricavare i valori dinamicamente, le #define di cui sotto sono gli indici 
			 per gli elementi

	REGVALUEINFO regValuesCurrentVersion[] = {
		{"BuildLab", REG_SZ,0},
		{"BuildLabEx", REG_SZ,0},
		{"CurrentBuild", REG_SZ,0},
		{"CurrentBuildNumber", REG_SZ,0},
		{"CurrentMajorVersionNumber", REG_DWORD,0},
		{"CurrentMinorVersionNumber", REG_DWORD,0},
		{"CurrentType", REG_SZ,0},
		{"CurrentVersion", REG_SZ,0},
		{"InstallationType", REG_SZ,0},
		{"InstallDate", REG_DWORD,0}, // 0x6024f748, 1613035336
		{"PathName", REG_SZ,0},
		{"ProductId", REG_SZ,0},
		{"ProductName", REG_SZ,0},
		{"RegisteredOrganization", REG_SZ,0},
		{"RegisteredOwner", REG_SZ,0},
		{"ReleaseId", REG_SZ,0},
		{"SystemRoot", REG_SZ,0},
		{"WinREVersion", REG_SZ,0}
	};

	#define BuildLab_INDEX						0
	#define BuildLabEx_INDEX					1
	#define CurrentBuild_INDEX					2
	#define CurrentBuildNumber_INDEX			3
	#define CurrentMajorVersionNumber_INDEX		4
	#define CurrentMinorVersionNumber_INDEX		5
	#define CurrentType_INDEX					6
	#define CurrentVersion_INDEX				7
	#define InstallationType_INDEX				8
	#define InstallDate_INDEX					9
	#define PathName_INDEX						10
	#define ProductId_INDEX						11
	#define ProductName_INDEX					12
	#define RegisteredOrganization_INDEX		13
	#define RegisteredOwner_INDEX				14
	#define ReleaseId_INDEX						15
	#define SystemRoot_INDEX					16
	#define WinREVersion_INDEX					17

	int size = sizeof(regValuesCurrentVersion) / sizeof(regValuesCurrentVersion[0]);
	GetWindowsVersionData(&(regValuesCurrentVersion[0]),size);

	char buf[128]={0};
	unix_timestamp_to_date(regValuesCurrentVersion[InstallDate_INDEX].value.dword,buf,sizeof(buf)-1);

	printf(	"\nGetCurrentVersionData()\n"\
			"running on: %s\n"\
			"installation date: %s\n"\
			"major ver number: %ld\n"\
			"minor ver number: %ld\n"\
			"[Press Enter]\n",
			regValuesCurrentVersion[ProductName_INDEX].value.string,
			buf,
			regValuesCurrentVersion[CurrentMajorVersionNumber_INDEX].value.dword,
			regValuesCurrentVersion[CurrentMinorVersionNumber_INDEX].value.dword
			);
	getchar();

	Restituisce TRUE se riesce, FALSE altrimenti.
*/
BOOL GetWindowsVersionData(REGVALUEINFO *pRegValues,UINT nSize)
{
	ASSERTEXPR(pRegValues);
	ASSERTEXPR(nSize > 0);

	char szBuffer[1024] = {0};
	int nBuffersize = sizeof(szBuffer);

	HKEY hKey = NULL;
	LONG lResult = 0L;
   
    /* Open the registry key HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows NT\CurrentVersion
    KEY_READ specifies that we only need read access to the key. */
    lResult = RegOpenKeyEx(	HKEY_LOCAL_MACHINE,
							HKEY_MICROSOFT_WINDOWS_CURRENTVERSION,
							0,										/* reserved, must be zero */
#if defined( _M_IX86)
							KEY_READ|KEY_WOW64_64KEY,				/* desired access rights */
#elif defined(_M_X64)
							KEY_READ,								/* desired access rights */
#else
	#error "unsupported platform"
#endif
							&hKey									/* receives the opened key handle */
							);

	if(lResult!=ERROR_SUCCESS)
        return(FALSE);

	DWORD dwActualType = 0L;	/* actual data type of the retrieved value */
	DWORD dwDataSize = 0L;		/* size of the data buffer */
	BYTE byteBuffer[512] = {0};	/* buffer to hold the retrieved data (adjust size if needed for longer strings) */
	int nBufferpointer = 0;

    /* iterate through the list of desired registry values */
    for(int i = 0; i < (int)nSize; ++i)
	{
		LPCTSTR valueName  = pRegValues[i].name;
        DWORD expectedType = pRegValues[i].type;

        /* initialize for each query */
        memset(byteBuffer,'\0',sizeof(byteBuffer));
        dwDataSize = sizeof(byteBuffer);

        /* query the value from the opened key */
        lResult = RegQueryValueEx(	hKey,			/* handle to the opened key */
									valueName,		/* Nnme of the value to query */
									NULL,			/* reserved, must be NULL */
									&dwActualType,	/* variable that receives the value's type */
									byteBuffer,		/* buffer that receives the value's data */
									&dwDataSize		/* variable that specifies the size of the buffer */
									);

        if(lResult==ERROR_SUCCESS)
		{
            /* check if the actual type matches the expected type (important for correct interpretation) */
            if(dwActualType==expectedType)
			{
				/* format the value name, left-aligned */
                nBufferpointer += wtfsnprintf(szBuffer+nBufferpointer,nBuffersize+strlen(szBuffer),"%-25s: ",valueName);

                if(expectedType==REG_SZ) 
				{
                    nBufferpointer += wtfsnprintf(szBuffer+nBufferpointer,nBuffersize+strlen(szBuffer),"%s\n",(LPCTSTR)byteBuffer);
					strcpyn(pRegValues[i].value.string,(char*)byteBuffer,REG_VALUE_MAXSTR);
                }
				else if(expectedType==REG_DWORD)
				{
                    /* for DWORD (REG_DWORD) type, cast and format as unsigned long */
                    nBufferpointer += wtfsnprintf(szBuffer+nBufferpointer,nBuffersize+strlen(szBuffer),"%lu\n",*(DWORD*)byteBuffer);
					
					/*
					esta es la forma correcta de interpretar los bytes binarios de un REG_DWORD leídos directamente del 
					registro de Windows como un valor numérico entero de 32 bits:

					DWORD dw = *(DWORD*)buffer;
						
					se denomina type punning, pero en lugar de eso se recomienda:

					unsigned char buffer[4];	// estos 4 bytes contienen un valor DWORD binario
					
					// Rellenar buffer con datos binarios, ej:
					buffer[0] = 0xDE;
					buffer[1] = 0xAD;
					buffer[2] = 0xBE;
					buffer[3] = 0xEF;
					DWORD dw_value;
					memcpy(&dw_value, buffer, sizeof(DWORD)); // Forma segura
					*/
					pRegValues[i].value.ulong = *(DWORD*)byteBuffer;
                }
                /* add more else if blocks here for other data types if needed (e.g., REG_BINARY, REG_MULTI_SZ) */
            } 
			else 
			{
                ; /* ERROR: value found but its type does not match expected type */
            }
        }
		else if(lResult==ERROR_FILE_NOT_FOUND)
		{
            ; /* ERROR: value not found */
        }
		else
		{
            ; /* ERROR: while querying value */
        }
    }

    RegCloseKey(hKey);

    return(TRUE);
}

/* (ri)definisce i tipi minimi necessari per non includere ntddk.h/wdm.h */
typedef LONG NTSTATUS;
#define STATUS_SUCCESS ((NTSTATUS)0x00000000L)

/*typedef struct _OSVERSIONINFOW {
    ULONG dwOSVersionInfoSize;
    ULONG dwMajorVersion;
    ULONG dwMinorVersion;
    ULONG dwBuildNumber;
    ULONG dwPlatformId;
    WCHAR  szCSDVersion[128];
} RTL_OSVERSIONINFOW, *PRTL_OSVERSIONINFOW;
*/

/* typedef per la funzione */
typedef NTSTATUS (NTAPI *pRtlGetVersion)(PRTL_OSVERSIONINFOW);

/*
	GetWindowsUAPlatform()

	Ricava la versione di Windows in formato UA.
	(vedi anche il codice in CWindowsVersion.cpp)

	Esempio:
	
	char szUA[32] = {0};
	double nUA = 0.0;
	GetWindowsUAPlatform(szUA,sizeof(szUA),&nUA);
	printf(	"\GetWindowsUAPlatform(): %s (version %.0f)\n[Press Enter]\n",szUA,nUA);
	getchar();
*/
BOOL GetWindowsUAPlatform(LPSTR lpszVersion,UINT nVersionSize,double* pnVersion)
{
	ASSERTEXPR(lpszVersion);
	ASSERTEXPR(nVersionSize > 0);
	ASSERTEXPR(pnVersion);

    memset(lpszVersion,'\0',nVersionSize);
	*pnVersion = 0.0f;

    HMODULE hNtdll = GetModuleHandleA("ntdll.dll");
    if(!hNtdll)
	{
        strcpyn(lpszVersion,"Unknown OS",nVersionSize);
        return(FALSE);
    }

    pRtlGetVersion ptrRtlGetVersion = (pRtlGetVersion)GetProcAddress(hNtdll,"RtlGetVersion");
    if(!ptrRtlGetVersion)
	{
        strcpyn(lpszVersion,"Unknown OS",nVersionSize);
        return(FALSE);
    }

    RTL_OSVERSIONINFOW rovi = {0};
    rovi.dwOSVersionInfoSize = sizeof(rovi);
    if(ptrRtlGetVersion(&rovi)!=STATUS_SUCCESS)
	{
        strcpyn(lpszVersion,"Unknown OS",nVersionSize);
        return(FALSE);
    }

    /* NT version string (sempre NT 10.0 per Win10/11) */
	const char* pNTVersion = "Windows";  /* fallback */
    if(rovi.dwMajorVersion >= 10)
	{
		*pnVersion = 10.0f;
        pNTVersion = "Windows NT 10.0";
    }
	else if(rovi.dwMajorVersion==6)
	{
        if(rovi.dwMinorVersion==3)
		{
			*pnVersion = 6.3f;
            pNTVersion = "Windows NT 6.3";   /* 8.1 */
        }
		else if(rovi.dwMinorVersion==2)
		{
			*pnVersion = 6.2f;
            pNTVersion = "Windows NT 6.2";   /* 8 */
        }
		else if(rovi.dwMinorVersion==1)
		{
			*pnVersion = 6.1f;
            pNTVersion = "Windows NT 6.1";   /* 7 */
        }
		else if(rovi.dwMinorVersion==0)
		{
			*pnVersion = 6.0f;
            pNTVersion = "Windows NT 6.0";   /* Vista */
        }
    }
	else if(rovi.dwMajorVersion==5)
	{
		*pnVersion = 5.1f;
        pNTVersion = "Windows NT 5.1";       /* XP */
    }

    /* architettura */
    SYSTEM_INFO si = {0};
    GetNativeSystemInfo(&si);

    BOOL bIsWow64 = FALSE;
    IsWow64Process(GetCurrentProcess(),&bIsWow64);

    const char* pArch = "Win32";
    if(si.wProcessorArchitecture==PROCESSOR_ARCHITECTURE_AMD64)
	{
        pArch = bIsWow64 ? "WOW64" : "Win64; x64";
    }
	else if(si.wProcessorArchitecture==PROCESSOR_ARCHITECTURE_ARM64)
	{
        pArch = "Win64; ARM64";
    }
	else if(si.wProcessorArchitecture==PROCESSOR_ARCHITECTURE_INTEL)
	{
        pArch = bIsWow64 ? "WOW64" : "x86";
    }

	snprintf(lpszVersion,nVersionSize,"%s; %s",pNTVersion,pArch);

	return(TRUE);
}

/*
	GetFriendlyWindowsName()

	Ricava la versione di Windows in formato "amichevole".
	(vedi anche il codice in CWindowsVersion.cpp)

	Esempio:

	char szOSFriendlyVersion[128] = {0};
	GetFriendlyWindowsName(szOSFriendlyVersion,sizeof(szOSFriendlyVersion));
	printf(	"\GetFriendlyWindowsName(): %s\n[Press Enter]\n",szOSFriendlyVersion);
	getchar();
*/
BOOL GetFriendlyWindowsName(LPSTR lpszVersion,UINT nVersionSize)
{
	ASSERTEXPR(lpszVersion);
	ASSERTEXPR(nVersionSize > 0);

    memset(lpszVersion,'\0',nVersionSize);

    /* carica ptrRtlGetVersion dinamicamente */
    HMODULE hNtdll = GetModuleHandleA("ntdll.dll");
    if(!hNtdll)
	{
        strcpyn(lpszVersion,"Unknown OS",nVersionSize);
        return(FALSE);
    }

    pRtlGetVersion ptrRtlGetVersion = (pRtlGetVersion)GetProcAddress(hNtdll,"RtlGetVersion");
    if(!ptrRtlGetVersion)
	{
        strcpyn(lpszVersion,"Unknown OS",nVersionSize);
        return(FALSE);
    }

    RTL_OSVERSIONINFOW osvi = {0};
    osvi.dwOSVersionInfoSize = sizeof(osvi);
    if(ptrRtlGetVersion(&osvi) != STATUS_SUCCESS)
	{
        strcpyn(lpszVersion,"Unknown OS",nVersionSize);
        return(FALSE);
    }

    DWORD dwBuild = osvi.dwBuildNumber;

    /* nome base OS */
    const char *pOSName = "Windows (legacy)"; /* fallback */
    if(dwBuild >= 22000)
	{
        pOSName = "Windows 11";
    }
	else if(dwBuild >= 10240 && dwBuild < 22000)
	{
        pOSName = "Windows 10";
    }
	else if(dwBuild >= 9600)
	{
        pOSName = "Windows 8.1";
    }

    RTL_OSVERSIONINFOW rovi = {0};
    rovi.dwOSVersionInfoSize = sizeof(rovi);
    if(ptrRtlGetVersion(&rovi)!=STATUS_SUCCESS)
	{
        strcpyn(lpszVersion,"Unknown OS",nVersionSize);
        return(FALSE);
    }

    /* NT version string (sempre NT 10.0 per Win10/11) */
	const char* pNTVersion = "Windows"; /* fallback */
	double pnVersion = 0.0f;
    if(rovi.dwMajorVersion >= 10)
	{
		pnVersion = 10.0f;
        pNTVersion = "Windows NT 10.0";
    }
	else if(rovi.dwMajorVersion==6)
	{
        if(rovi.dwMinorVersion==3)
		{
			pnVersion = 6.3f;
            pNTVersion = "Windows NT 6.3";   /* 8.1 */
        }
		else if(rovi.dwMinorVersion==2)
		{
			pnVersion = 6.2f;
            pNTVersion = "Windows NT 6.2";   /* 8 */
        }
		else if(rovi.dwMinorVersion==1)
		{
			pnVersion = 6.1f;
            pNTVersion = "Windows NT 6.1";   /* 7 */
        }
		else if(rovi.dwMinorVersion==0)
		{
			pnVersion = 6.0f;
            pNTVersion = "Windows NT 6.0";   /* Vista */
        }
    }
	else if(rovi.dwMajorVersion==5)
	{
		pnVersion = 5.1f;
        pNTVersion = "Windows NT 5.1";       /* XP */
    }

    /* edizione */
    DWORD dwProductType = 0;
    if(!GetProductInfo(rovi.dwMajorVersion,rovi.dwMinorVersion,0,0,&dwProductType))
        dwProductType = 0;

	/* giusto per informazione: sono piu' di 170 tipi!!! */
	const char *pEdition = "Unknown Edition";
	switch(dwProductType) {
		case PRODUCT_CORE:
		case PRODUCT_HOME_BASIC:
		case PRODUCT_HOME_PREMIUM:			pEdition = "Home";					break;
		case PRODUCT_CORE_SINGLELANGUAGE:	pEdition = "Home Single Language";	break;
		case PRODUCT_PROFESSIONAL:			pEdition = "Pro";					break;
		case PRODUCT_ENTERPRISE:			pEdition = "Enterprise";			break;
		case PRODUCT_EDUCATION:				pEdition = "Education";				break;
		/* case PRODUCT_PRO_FOR_WORKSTATIONS:	pEdition = "Pro for Workstations"; break; */
		case PRODUCT_IOTENTERPRISE:			pEdition = "IoT Enterprise";		break;
		case PRODUCT_PRO_CHINA:				pEdition = "Pro China";				break;
		case PRODUCT_ENTERPRISEG:			pEdition = "Enterprise G";			break;
	}

    /* versione friendly dal registro */
    char szFriendlyVersion[64] = "";
    HKEY hKey = NULL;
    if(RegOpenKeyEx(HKEY_LOCAL_MACHINE,"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion",0,KEY_READ,&hKey)==ERROR_SUCCESS)
    {
        char buf[64] = {0};
        DWORD size = sizeof(buf);

        /* prova prima DisplayVersion (Win10 20H2+ / Win11) */
        if(RegQueryValueEx(hKey,"DisplayVersion",NULL,NULL,(LPBYTE)buf,&size)==ERROR_SUCCESS && buf[0])
        {
            strcpyn(szFriendlyVersion,buf,sizeof(szFriendlyVersion));
        }
        else
        {
            /* fallback su ReleaseId (vecchie Win10) */
            size = sizeof(buf);
            if(RegQueryValueEx(hKey,"ReleaseId",NULL,NULL,(LPBYTE)buf,&size)==ERROR_SUCCESS && buf[0])
                strcpyn(szFriendlyVersion,buf,sizeof(szFriendlyVersion));
        }

        RegCloseKey(hKey);
    }

	int len = wtfsnprintf(	lpszVersion,
							nVersionSize,
							"%s %s%s%s (dwBuild %u)",
							pOSName,
							pEdition,
							(szFriendlyVersion[0] != '\0') ? " " : "",
							szFriendlyVersion,
							dwBuild);

    if(len < 0 || len >= (int)nVersionSize)
	{
        strcpyn(lpszVersion,"Unknown Windows (buffer too small)",nVersionSize);
        return(FALSE);
    }

	return(TRUE);
}

/*
	GetThisModuleFileName()

	Recupera il nome del file eseguibile corrente, considerando come eseguibile solo quanto termina con ".exe".
	Non usa GetModuleFileName() perche' quest'ultima con W95/98, se il numero di versione interno del file e' 
	inferiore a 4, restituisce il nome corto.

	Restituisce il puntatore al nome dell'eseguibile o NULL.
*/
LPSTR GetThisModuleFileName(LPSTR lpszFileName,UINT nSize)
{
	ASSERTEXPR(lpszFileName);
	ASSERTEXPR(nSize > 0);

	int i = 0;
	char* p;
	char szModuleName[_MAX_PATH+1] = {0};

	memset(lpszFileName,'\0',nSize);

	/* ricava il nome del modulo dalla linea di comando */	
	strcpyn(szModuleName,GetCommandLine(),sizeof(szModuleName));

	if((p = stristr(szModuleName,".exe"))!=NULL)
		i = (int)(p - szModuleName);
	if(i > 0)
	{
		strcpyn(szModuleName,GetCommandLine(),sizeof(szModuleName));
		memcpy(szModuleName+i,".exe",4);
	}
	else
		return(NULL);

	p = szModuleName;
	while(*p)
	{
		if(*p=='"')
			*p = ' ';
		p++;
	}

	p = szModuleName;
	while(isspace((unsigned char)*p))
		p++;
	
	for(i=0; i < (int)nSize+1; i++)
	{
		if(isspace((unsigned char)*p))
			if(stristr(lpszFileName,".exe"))
				break;

		lpszFileName[i] = *p++;
	}
	
	lpszFileName[i] = '\0';

	return(lpszFileName);
}

/*
	GetDllVersion()

	The following code fragment illustrates how you can use GetDllVersion to test if Comctl32.dll is
	version 4.71 or later.

	if(GetDllVersion(TEXT("comctl32.dll")) >= PACKVERSION(4,71))
		//Proceed
	else
		//MicrosoftReallySucksBalls...

	Restituisce una DWORD con il numero di versione.
*/
DWORD GetDllVersion(LPCSTR lpszDllName)
{
	ASSERTEXPR(lpszDllName);

	HINSTANCE hDll = NULL;
	DWORD dwVersion = 0L;

	if((hDll = LoadLibrary(lpszDllName))!=(HINSTANCE)NULL)
	{
		DLLGETVERSIONPROC pDllGetVersion = (DLLGETVERSIONPROC)GetProcAddress(hDll,"DllGetVersion");

		/*
		HRESULT CALLBACK DllGetVersion(DLLVERSIONINFO *pdvi);
		
		Version 5.0. DLLs that are shipped with Windows 2000 or later systems may return a
		DLLVERSIONINFO2 structure. To maintain backward compatibility, the first member of
		a DLLVERSIONINFO2 structure is a DLLVERSIONINFO structure.

		Because some DLLs may not implement this function, you must test for it explicitly.
		Depending on the particular DLL, the lack of a DllGetVersion function may be a useful
		indicator of the version.
		*/
		if(pDllGetVersion)
		{
			DLLVERSIONINFO dvi;
			HRESULT hr;

			ZeroMemory(&dvi,sizeof(DLLVERSIONINFO));
			dvi.cbSize = sizeof(DLLVERSIONINFO);

			hr = (*pDllGetVersion)(&dvi);
			if(SUCCEEDED(hr))
				dwVersion = PACKVERSION(dvi.dwMajorVersion,dvi.dwMinorVersion);
		}

		FreeLibrary(hDll);
	}

	return(dwVersion);
}

/*
	GetUniqueMutexName()

	Restituisce una stringa (allocata dinamicamente) con un nome unico per il mutex,
	da liberare poi con free, o NULL per errore.
*/
LPSTR GetUniqueMutexName(void)
{
	static int n = 0;
	char* pMutexName = NULL;
	if((pMutexName = (char*)calloc(_MAX_FILEPATH+1,sizeof(char)))!=NULL)
		snprintf(pMutexName,_MAX_FILEPATH+1,"mutex-%d-%lld",n++,unix_timestamp());
	return(pMutexName);
}

/*
	WritePrivateProfileInt()

	Scrive un intero nel file .ini (nell'API e' presente solo la WritePrivateProfileString()).

	Restituisce TRUE se riesce, FALSE altrimenti.
*/
BOOL WritePrivateProfileInt(LPCSTR lpcszSectioneName,LPCSTR lpcszKeyName,int nValue,LPCSTR lpcszIniFile)
{
	ASSERTEXPR(lpcszSectioneName);
	ASSERTEXPR(lpcszKeyName);
	ASSERTEXPR(nValue > 0);
	ASSERTEXPR(lpcszIniFile);

	char szBuffer[16] = {0};
	snprintf(szBuffer,sizeof(szBuffer),"%d",nValue);
	return(WritePrivateProfileString(lpcszSectioneName,lpcszKeyName,szBuffer,lpcszIniFile));
}

/*
	GetLastErrorString()

	Ricava e visualizza il messaggio relativo all'ultimo codice d'errore (parametro -1L) o ricava (senza 
	visualizzare) il messaggio relativo al codice d'errore specificato dal parametro.

	Nel secondo caso, alloca il buffer per la descrizione che deve essere rilasciato dal chiamante tramite
	la funzione LocalFree().
*/
LPVOID GetLastErrorString(DWORD dwError)
{
	LPVOID pBuffer = NULL;

	if(dwError==(DWORD)-1L)
	{
		FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS,
					NULL,
					GetLastError(),
					MAKELANGID(LANG_NEUTRAL,SUBLANG_SYS_DEFAULT/*SUBLANG_DEFAULT*/),
					(LPTSTR)&pBuffer,
					0,
					NULL);

		MessageBox(NULL,(LPCSTR)pBuffer,"Error",MB_OK|MB_ICONWARNING|MB_TASKMODAL|MB_SETFOREGROUND|MB_TOPMOST);

		LocalFree(pBuffer);
		pBuffer = NULL;
	}
	else
	{
		FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS,
					NULL,
					dwError,
					MAKELANGID(LANG_NEUTRAL,SUBLANG_SYS_DEFAULT/*SUBLANG_DEFAULT*/),
					(LPTSTR)&pBuffer,
					0,
					NULL);

		char* p = (char*)pBuffer;
		int i = (int)strlen(p)-1;
		if(p[i]=='\r' || p[i]=='\n')
			do {
				p[i] = '\0';
				i = (int)strlen(p)-1;
			} while(i > 0 && p[i]=='\r' || p[i]=='\n');
	}

	return(pBuffer);
}

/*
	GetLastErrorDescription()

	Ricava e visualizza il messaggio relativo all'ultimo codice d'errore,
	formattando numero e descrizione nel buffer ricevuto come parametro.

	Restituisce il codice d'errore numerico che ha ricavato.
 */
DWORD GetLastErrorDescription(LPSTR lpBuffer,UINT nSize)
{
	ASSERTEXPR(lpBuffer);
	ASSERTEXPR(nSize > 0);

    DWORD dwError = GetLastError();
    DWORD dwResult = 0L;
    LPSTR lpMsgBuf = NULL;
    
    /* ottiene la descrizione dell'errore */
	dwResult = FormatMessage(	FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
								NULL,
								dwError,
								MAKELANGID(LANG_NEUTRAL,SUBLANG_DEFAULT),
								(char*)&lpMsgBuf,
								0,
								NULL
								);

	if(dwResult==0L)
	{
		/* FormatMessage ha fallito, restituisce una stringa di errore generica */
		snprintf(lpBuffer,nSize,"Unknown error (%lu), FormatMessage failed",dwError);
		return(dwError);
	}
    
	/* elimina eventuali caratteri di cr/lf */
	size_t len = strlen(lpMsgBuf);
	while(len > 0 && (lpMsgBuf[len-1]=='\r' || lpMsgBuf[len-1]=='\n'))
		lpMsgBuf[--len] = '\0';

	/* formatta la stringa finale */
	snprintf(lpBuffer,nSize,"%s (%lu)",lpMsgBuf,dwError);

	/* rilascia la memoria allocata da FormatMessage */
	LocalFree(lpMsgBuf);

	return(dwError);
}

/*
	MessageBoxResource()

	Ricava dalle risorse il messaggio specificato dall'id e lo visualizza.

	Restituisce il codice del bottone premuto.
*/
UINT MessageBoxResource(HWND hWnd,UINT nStyle,LPCSTR lpcszTitle,UINT nID)
{
	ASSERTEXPR(lpcszTitle);
	ASSERTEXPR(nID > 0);

	char szBuffer[2048] = {"<unable to load the string from resources>"};
	LoadString(NULL,nID,szBuffer,sizeof(szBuffer)-1);
	return(InternalMessageBox(hWnd,szBuffer,lpcszTitle,nStyle));
}

/*
	MessageBoxResourceEx()

	Ricava dalle risorse il messaggio specificato dall'id (permettendo un numero 
	di parametri variabile) e visualizza.

	Restituisce il codice del bottone premuto.
*/
UINT MessageBoxResourceEx(HWND hWnd,UINT nStyle,LPCSTR lpcszTitle,UINT nID,...)
{
	ASSERTEXPR(lpcszTitle);
	ASSERTEXPR(nID > 0);

	LPSTR pArgs = NULL;
	char szBuffer[2048] = {"<unable to load the string from resources>"};
	char szFormat[2048] = {0};

	if(LoadString(NULL,nID,szFormat,sizeof(szFormat)-1) > 0)
	{
		pArgs = (LPSTR)&nID + sizeof(nID);
		memset(szBuffer,'\0',sizeof(szBuffer));
		vsnprintf(szBuffer,sizeof(szBuffer),szFormat,pArgs);
	}

	return(InternalMessageBox(hWnd,szBuffer,lpcszTitle,nStyle));
}

/*
	InternalMessageBox()

	Versione interna/custom per visualizzare i messaggi.

	Restituisce il codice del bottone premuto.
*/
UINT InternalMessageBox(HWND hWnd,LPCSTR lpcszText,LPCSTR lpcszTitle,UINT nStyle)
{
	ASSERTEXPR(lpcszText);
	ASSERTEXPR(lpcszTitle);

	return((UINT)MessageBox(hWnd,lpcszText,lpcszTitle,nStyle|MB_TASKMODAL|MB_SETFOREGROUND|MB_TOPMOST));
}

/*
	FormatResourceString()

	Carica la risorsa.

	Restituisce l'id della risorsa, o 0 per errore.
*/
UINT FormatResourceString(LPSTR lpszString,UINT nSize,UINT nID)
{
	ASSERTEXPR(lpszString);
	ASSERTEXPR(nSize > 0);
	ASSERTEXPR(nID > 0);

	memset(lpszString,'\0',nSize);
	return(LoadString(NULL,nID,lpszString,nSize-1));
}

/*
	FormatResourceStringEx()

	Carica la risorsa.

	Restituisce l'id della risorsa, o -1 per errore.
*/
UINT FormatResourceStringEx(LPSTR lpszString,UINT nSize,UINT nID,...)
{
	ASSERTEXPR(lpszString);
	ASSERTEXPR(nSize > 0);
	ASSERTEXPR(nID > 0);

	int nRet = -1;
	LPSTR pArgs;
	char szFormat[2048] = {0};

	memset(lpszString,'\0',nSize);

	if(LoadString(NULL,nID,szFormat,sizeof(szFormat)-1) > 0)
	{
		pArgs = (LPSTR)&nID + sizeof(nID);
		nRet = vsnprintf(lpszString,nSize,szFormat,pArgs);
	}

	return((UINT)nRet);
}

/*
	ExtractResource()

	Estrae la risorsa nel file specificato.

	Restituisce TRUE se riesce, FALSE altrimenti.

	Note:
	Se nel file delle risorse (.rc) la risorsa viene identificata nell'albero come testo, ad es. "WAVE", allora si 
	puo' chiamare la funzione cosi':
	ExtractResource(IDR_WAVE_ABOUT,"WAVE",szWave);

	Negli altri casi, per evitare complicazioni durante l'estrazione, definire la risorsa come RCDATA e poi usare
	la seguente procedura (ad es. per un icona):
	- nel file resource.h definire l'icona come IDR_ICO_FILE con l'ultimo numero disponibile (es. 105), aggiornando
	  poi di conseguenza il valore di _APS_NEXT_RESOURCE_VALUE con il seguente numero (106 in questo caso)
	- nel file .rc aggiungere alla fine una linea come: IDR_ICO_FILE RCDATA "nomefileicona.ico"
	- estrarre la risorsa con la seguente chiamata: ExtractResource(IDR_ICO_FILE,RT_RCDATA,lpcsOutputFilename);

	Questo perche', sempre nel caso delle icone, l'icona su disco ha una struttura precisa, mentre quando viene
	inserita nel file .rc viene "smontata" in un indice (RT_GROUP_ICON) dei formati (es: 16x16, 32x32, 48x48)
	contenuti nell'icona e nelle immagini vere e proprie (RT_ICON), memorizzate separatemente. Il che significa
	che poi per recuperare (estrarre) l'icona bisognerebbe implementare un ciclo che scorra usando le strutture
	GRPICONDIR e ICONDIRENTRY, etc., che darsi di capoccia su uno spigolo fa meno male.

	Conclusione, nel file .rc mettere (o duplicare, se gia' esiste come risorsa ufficiale) la risorsa che si dovra'
	poi estrarre con lo specificatore RCDATA ed estrarla poi usando il'id RT_RCDATA.
*/
BOOL ExtractResource(UINT nID,LPCSTR lpcszResName,LPCSTR lpcszOutputFile)
{
	ASSERTEXPR(nID > 0);
	ASSERTEXPR(lpcszResName);
	ASSERTEXPR(lpcszOutputFile);

	BOOL bExtracted = FALSE;

	HRSRC hRes = FindResource(NULL,MAKEINTRESOURCE(nID),lpcszResName);
	if(hRes)
	{
		HGLOBAL hGlobal = LoadResource(NULL,hRes);
		if(hGlobal)
		{
			LPVOID lpVoid = LockResource(hGlobal);
			if(lpVoid)
			{
				DWORD dwError = 0L;
				EnsurePathnameExists(lpcszOutputFile,&dwError);

				HANDLE handle;
				if((handle = CreateFile(lpcszOutputFile,GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL))!=INVALID_HANDLE_VALUE)
				{
					DWORD dwToWrite = SizeofResource(NULL,hRes);
					DWORD dwWritten = 0L;
					WriteFile(handle,lpVoid,(UINT)dwToWrite,&dwWritten,NULL);
					CloseHandle(handle);
					bExtracted = (dwToWrite==dwWritten);
				}
			}
		}
	}

	return(bExtracted);
}

/*
	ExtractResourceIntoBuffer()

	Estrae la risorsa nel buffer di memoria.

	Restituisce TRUE se riesce, FALSE altrimenti.
*/
BOOL ExtractResourceIntoBuffer(UINT nID,LPCSTR lpcszResName,LPSTR lpBuffer,UINT nSize)
{
	ASSERTEXPR(nID > 0);
	ASSERTEXPR(lpcszResName);
	ASSERTEXPR(lpBuffer);
	ASSERTEXPR(nSize > 0);

	BOOL bExtracted = FALSE;

	HRSRC hRes = FindResource(NULL,MAKEINTRESOURCE(nID),lpcszResName);
	if(hRes)
	{
		HGLOBAL hGlobal = LoadResource(NULL,hRes);
		if(hGlobal)
		{
			LPVOID lpVoid = LockResource(hGlobal);
			if(lpVoid)
			{
				memset(lpBuffer,'\0',nSize);
				UINT nResSize = SizeofResource(NULL,hRes);
				if(nResSize <= nSize)
				{
					memcpy(lpBuffer,lpVoid,nResSize);
					bExtracted = TRUE;
				}
			}
		}
	}

	return(bExtracted);
}

/*
	CreateShortcut()

	Crea lo shortcut al file specificato.

	Restituisce TRUE se riesce, FALSE altrimenti.
*/
BOOL CreateShortcut(LPCSTR lpcszTarget,LPCSTR lpcszArguments,LPCSTR lpcszLinkFileName,LPCSTR lpcszLinkLocation,LPCSTR lpcszWorkingDir,UINT nIconIndex)
{
	ASSERTEXPR(lpcszTarget);
	ASSERTEXPR(lpcszArguments);
	ASSERTEXPR(lpcszLinkFileName);
	ASSERTEXPR(lpcszLinkLocation);
	ASSERTEXPR(lpcszWorkingDir);
	ASSERTEXPR(nIconIndex > 0);

	BOOL bCreated = FALSE;
	HRESULT hres;
	ITEMIDLIST *id; 
	char szLocation[_MAX_PATH+1] = {0};
	char szLink[_MAX_PATH+1] = {0};
	
	/* se non viene specificato nessun percorso, crea il link sul desktop */
	if(!lpcszLinkLocation)
	{
		SHGetSpecialFolderLocation(NULL,CSIDL_DESKTOPDIRECTORY,&id); 
		SHGetPathFromIDList(id,&szLocation[0]); 
	}
	else
		strcpyn(szLocation,lpcszLinkLocation,sizeof(szLocation));
	
	/* compone il pathname completo per il link */
	snprintf(szLink,sizeof(szLink),"%s\\%s.lnk",szLocation,lpcszLinkFileName);

	hres = CoInitialize(NULL);
	if(SUCCEEDED(hres))
	{
		IShellLink* psl;
		hres = CoCreateInstance(	
#ifdef __cplusplus
									CLSID_ShellLink,	/* C++: CLSID_ShellLink */
#else
									&CLSID_ShellLink,	/* C: &CLSID_ShellLink */
#endif
									NULL,
									CLSCTX_INPROC_SERVER,
#ifdef __cplusplus
									IID_IShellLink,	/* C++: IID_IShellLink */
#else
									&IID_IShellLink,	/* C: &IID_IShellLink */
#endif
									(LPVOID*)&psl
									);

		if(SUCCEEDED(hres))
		{
			IPersistFile* ppf;

			/* in C++ chiama direttamente i membri di <psl>, mentre in C deve usare la v-table (lpVtbl) per chiamare i metodi */

#ifdef __cplusplus
			psl->SetPath(lpcszTarget);
#else
			psl->lpVtbl->SetPath(psl, lpcszTarget);
#endif

			if(lpcszArguments)
#ifdef __cplusplus
				psl->SetArguments(lpcszArguments);
#else
				psl->lpVtbl->SetArguments(psl, lpcszArguments);
#endif

			if(lpcszWorkingDir)
#ifdef __cplusplus
				psl->SetWorkingDirectory(lpcszWorkingDir);
#else
				psl->lpVtbl->SetWorkingDirectory(psl, lpcszWorkingDir);
#endif

#ifdef __cplusplus
			hres = psl->QueryInterface(IID_IPersistFile,(LPVOID*)&ppf);
#else
			hres = psl->lpVtbl->QueryInterface(psl, &IID_IPersistFile, (LPVOID*)&ppf);
#endif
			if(SUCCEEDED(hres))
			{
				WORD wsz[_MAX_PATH+1] = {0};
				MultiByteToWideChar(CP_ACP,MB_PRECOMPOSED,szLink,-1,(LPWSTR)wsz,sizeof(wsz));
#ifdef __cplusplus
				hres = ppf->Save((LPCOLESTR)wsz,TRUE);
				ppf->Release();
#else
				ppf->lpVtbl->Save(ppf, (LPCOLESTR)wsz, TRUE);
				ppf->lpVtbl->Release(ppf);
#endif
				bCreated = TRUE;
			}

			if(nIconIndex!=(UINT)-1)
#ifdef __cplusplus
				psl->SetIconLocation(lpcszTarget,nIconIndex);
#else
				psl->lpVtbl->SetIconLocation(psl, lpcszTarget, nIconIndex);
#endif
			
#ifdef __cplusplus
			psl->Release();
#else
			psl->lpVtbl->Release(psl);
#endif
		}
	
		CoUninitialize();
	}

	return(bCreated);
}

/*
	Delay()

	Cicla il numero di millisecondi specificato, cedendo ciclicamente il controllo alla coda dei 
	messaggi per non inchiodare il programma.
*/
void Delay(UINT nMillisec)
{
	ASSERTEXPR(nMillisec > 0);
	if(nMillisec==0)
		return;

	DWORD dwStart = GetTickCount();

	do {
		/* mantiene la UI/Console reattiva (e gestisce WM_CLOSE se chiamata) */
		PeekAndPump();
        
		/* rilascia la CPU per un istante (fondamentale!) */
		Sleep(1);
        
	} while((GetTickCount() - dwStart) < (DWORD)nMillisec);
}

/*
	PeekAndPump()

	Passa temporaneamente il controllo alla coda dei messaggi per non inchidare il programma.
	Occhio alla differenza tra SDK e MFC, in un caso rimuove e nell'altro NON rimuove i messaggi.

	Restituisce TRUE se riesce, FALSE altrimenti.
*/
BOOL PeekAndPump(void)
{
	MSG msg;

#if (defined(_AFX) || defined(_AFXDLL)) && !defined(_CONSOLE)
    /* --- codice MFC Standard (SOLO per GUI) --- */
	while(PeekMessage(&msg,NULL,0,0,PM_NOREMOVE))
	{
		if(!AfxGetApp()->PumpMessage())
		{
			PostQuitMessage(0);
			return(FALSE);
		}
	}
	LONG lIdle = 0L;
	while(AfxGetApp()->OnIdle(lIdle++))
		;
#else
  #if 1
	/* elabora TUTTI i messaggi presenti nella coda prima di tornare
	il while invece dell' if evita che la coda dei messaggi si accumuli se il sistema e' sotto carico
	PM_REMOVE: e' fondamentale dopo aver inviato WM_CLOSE, se si usasse PM_NOREMOVE, il messaggio di 
	chiusura rimarrebbe li' ed il while diventerebbe un loop infinito */
	while(PeekMessage(&msg,NULL,0,0,PM_REMOVE))
	{
		if(msg.message==WM_QUIT)
			return(FALSE);
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
    
	/* se la coda e' vuota, cede il passo
	in ambiente x86 su Windows 10, questa e' la scelta migliore rispetto a Sleep(0) dato che:
	Sleep(0) cede il passo solo a thread con la stessa priorita', mentre  SwitchToThread() cede 
	il passo a qualsiasi thread pronto sull'attuale processore, il che e' perfetto per dare a 
	conhost.exe (il processo della console) il tempo di chiudersi mentre il programma aspetta un 
	istante */
	SwitchToThread();
  #else
    /* --- codice Win32 SDK puro o console MFC ---
       elaborazione messaggi Win32 di base (importante anche per
	   MFC console, perche' MFC usa messaggi per cleanup e thread) */
	if(PeekMessage(&msg,NULL,0,0,PM_REMOVE))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
    else /* cede tempo CPU se non ci sono messaggi */
	{
        SwitchToThread(); /* oppure Sleep(0); */
    }
  #endif
#endif

	return(TRUE);
}

/*
	GetDiskInfo()
*/
BOOL GetDiskInfo(LPCSTR lpcszRootPath,DISKINFO* pDiskInfo)
{
	ASSERTEXPR(lpcszRootPath);
	ASSERTEXPR(pDiskInfo);

	BOOL bRet = FALSE;
	char szRoot[_MAX_PATH+1] = {0};

	if(!lpcszRootPath || !pDiskInfo)
		return(bRet);

	strcpyn(szRoot,lpcszRootPath,sizeof(szRoot));
	if(szRoot[strlen(szRoot)-1]!='\\')
		strcatn(szRoot,"\\",sizeof(szRoot));

	memset(pDiskInfo,'\0',sizeof(DISKINFO));
	strcpyn(pDiskInfo->rootPath,szRoot,sizeof(pDiskInfo->rootPath));

	/* tipo di drive */
	pDiskInfo->driveType = GetDriveTypeA(szRoot);
	pDiskInfo->isRemoteDrive = (pDiskInfo->driveType==DRIVE_REMOTE);

	/* informazioni sul volume */
	bRet = GetVolumeInformationA(	szRoot,
									pDiskInfo->volumeName,
									_MAX_PATH,
									&pDiskInfo->serialNumber,
									&pDiskInfo->maxComponentLength,
									&pDiskInfo->fileSystemFlags,
									pDiskInfo->fileSystem,
									_MAX_PATH
									);
	if(!bRet)
		return(bRet);

	/* spazio su disco (64 bit) */
	bRet = GetDiskFreeSpaceExA(	szRoot,
								(PULARGE_INTEGER)&pDiskInfo->freeBytesForUser,
								(PULARGE_INTEGER)&pDiskInfo->totalBytes,
								(PULARGE_INTEGER)&pDiskInfo->freeBytes);
	if(!bRet)
		return(bRet);

	pDiskInfo->usedBytes = pDiskInfo->totalBytes - pDiskInfo->freeBytes;

	/* geometria del filesystem */
	bRet = GetDiskFreeSpaceA(	szRoot,
								&pDiskInfo->sectorsPerCluster,
								&pDiskInfo->bytesPerSector,
								&pDiskInfo->numberOfFreeClusters,
								&pDiskInfo->totalNumberOfClusters);
	if(!bRet)
		return(bRet);

	/* interpretazione FILE_SYSTEM_FLAGS */
	pDiskInfo->supportsCompression	= (pDiskInfo->fileSystemFlags & FILE_FILE_COMPRESSION)		!= 0;
	pDiskInfo->supportsEncryption	= (pDiskInfo->fileSystemFlags & FILE_SUPPORTS_ENCRYPTION)	!= 0;
	pDiskInfo->supportsSparseFiles	= (pDiskInfo->fileSystemFlags & FILE_SUPPORTS_SPARSE_FILES)	!= 0;
	pDiskInfo->supportsHardLinks	= (pDiskInfo->fileSystemFlags & FILE_SUPPORTS_HARD_LINKS)	!= 0;
	pDiskInfo->supportsACLs			= (pDiskInfo->fileSystemFlags & FILE_PERSISTENT_ACLS)		!= 0;
	pDiskInfo->isReadOnly			= (pDiskInfo->fileSystemFlags & FILE_READ_ONLY_VOLUME)		!= 0;
	pDiskInfo->isCompressedVolume	= (pDiskInfo->fileSystemFlags & FILE_VOLUME_IS_COMPRESSED)	!= 0;

	return(bRet);
}

/*
	GetDiskType()
*/
LPCSTR GetDiskType(DWORD dwType)
{
	switch(dwType)
	{
		case DRIVE_FIXED:		return "fixed";
		case DRIVE_REMOVABLE:	return "removable";
		case DRIVE_CDROM:		return "CD-ROM";
		case DRIVE_REMOTE:		return "Network";
		case DRIVE_RAMDISK:		return "RAM disk";
		default:				return "unknown drive type";
	}
}

/*
	GetDriveFromPath()

	Estrae il drive dal pathname.

	Restituisce 0 per errore, 1 per drive presente, 2 per UNC presente
*/
UINT GetDriveFromPath(LPCSTR lpcszPathName,LPSTR lpszDrive,UINT nSize)
{
	ASSERTEXPR(lpcszPathName);
	ASSERTEXPR(lpszDrive);
	ASSERTEXPR(nSize > 0);

	char szPathName[_MAX_PATH+1] = {0};
	char* pFilePart = NULL;

	DWORD dwLength = GetFullPathName(lpcszPathName,sizeof(szPathName),szPathName,&pFilePart);
	if(dwLength==0L || dwLength >= sizeof(szPathName))
		return(0);

	/* "C:\..." */
	if(szPathName[1]==':' && nSize >= 3)
	{
		/* ricava la lettera del drive e la converte in maiuscolo, non usare lpszDrive[0] -= 32; perche' se e' gia' maiuscolo sballa */
		lpszDrive[0] = szPathName[0];
		if(lpszDrive[0] >= 'a' && lpszDrive[0] <= 'z')
			lpszDrive[0] -= ('a' - 'A');

		lpszDrive[1] = ':';
		lpszDrive[2] = '\0';
		return(1);
	}

	/* UNC "\\server\share\..." */
	if(szPathName[0]=='\\' && szPathName[1]=='\\')
	{
		strcpyn(lpszDrive,"\\\\",nSize);
		return(2);
	}

	return(0);
}

/*
	GetFileTypeProbability()

	Euristica per indovinare il tipo di file (testo/binario) calcolando la probabilita'.

	Restituisce la probabilita' per i due tipi, o {-1,-1} in caso di errore (es. file non trovato).
*/
FILETYPEPROB GetFileTypeProbability(LPCSTR lpcszFileName)
{
	ASSERTEXPR(lpcszFileName);

	#define SAMPLE_CHUNK_SIZE 4096

	FILETYPEPROB probability = {-1,-1};

	ASSERTEXPR(lpcszFileName);
	if(!lpcszFileName)
		return(probability);
   
    FILE* fp = NULL;
    unsigned char buffer[SAMPLE_CHUNK_SIZE+1] = {0};
    size_t bytes_read = 0;
    int null_bytes = 0;
    int non_ascii_printable_bytes = 0;
    int text_like_bytes = 0;

    /* apre il file in modalita' binaria */
    if((fp = fopen(lpcszFileName,"rb"))==NULL)
        return(probability);

    /* legge un campione del file */
    bytes_read = fread(buffer,1,SAMPLE_CHUNK_SIZE,fp);
    fclose(fp);

    if(bytes_read==0)
	{
        /* file vuoto o errore di lettura, nessuna informazioen da ricavare, possibilita' al 50/50  */
        probability.text = 50;
        probability.binary = 50;
        return(probability);
    }

    /* analizza i byte del campione */
    for(size_t i=0; i < bytes_read; i++)
	{
		unsigned char byte = buffer[i];

        if(byte==0x00)
            null_bytes++;

        /* caratteri ASCII stampabili (0x20 a 0x7E) */
        /* più i caratteri di controllo "testuali" comuni: Tab (0x09), Line Feed (0x0A), Carriage Return (0x0D) */
        if((byte >= 0x20 && byte <= 0x7E) || byte == 0x09 || byte == 0x0A || byte == 0x0D)
		{
			text_like_bytes++;
        }
		else
		{
            /* se non e' un byte nullo e non e' un carattere "testuale" comune,
            e' un byte non-ASCII stampabile o un carattere di controllo insolito */
            if(byte!=0x00)
				non_ascii_printable_bytes++; /* gia' conteggiato nei null_bytes se 0x00 */
        }
    }

    /* calcola la probabilita' */
    /* la probabilita' di essere testo diminuisce con l'aumentare di null bytes e non ascii printable bytes */
    double text_score = 100.0;

    /* penalita' alta per i null bytes, ogni null byte riduce drasticamente il punteggio testo */
    text_score -= (double)null_bytes * 100.0 / 100.0; /* esempio: 100 null bytes -> 0% testo, 1 null byte -> 99% testo */

    /* penalita' per i byte non ASCII stampabili (meno forti dei null bytes) */
    /* se ci sono molti caratteri strani, è meno probabile che sia testo */
    text_score -= (double)non_ascii_printable_bytes * 100.0 / bytes_read * 1.5; /* peso maggiore per la loro frequenza */

    /* controlla che il punteggio non vada sotto zero o sopra 100 */
    if(text_score < 0) {
        text_score = 0;
    } else if(text_score > 100) {
        text_score = 100;
    }

    probability.text = (int)text_score;
    probability.binary = 100 - probability.text;

    return(probability);
}

/*
	CopyFileTo()

	Copia il file sorgente su quello di destinazione con opzione a move e dialogo con messaggio.

	Restituisce TRUE se riesce, FALSE altrimenti.
*/
BOOL CopyFileTo(HWND hWnd,LPCSTR lpcszSourceFile,LPCSTR lpcszDestFile,BOOL bMoveInsteadCopy,BOOL bShowDialog)
{
	ASSERTEXPR(lpcszSourceFile);
	ASSERTEXPR(lpcszDestFile);

	/* per garantire il doppio NULL dopo il nome del file per SHFileOperation() */
	char szFrom[_MAX_FILEPATH+2] = {0};
	char szTo[_MAX_FILEPATH+2] = {0};
	strcpyn(szFrom,lpcszSourceFile,sizeof(szFrom));
	strcpyn(szTo,lpcszDestFile,sizeof(szTo));

	/* imposta i parametri per SHFileOperation() */
	SHFILEOPSTRUCT sh;
	memset(&sh,'\0',sizeof(sh));

	sh.hwnd = hWnd;
	sh.wFunc = bMoveInsteadCopy ? FO_MOVE : FO_COPY;

	/* per evitare il problema della mancanza del doppio NULL finale */
	sh.pFrom = szFrom;
	sh.pTo = szTo;
	/* sh.pFrom = lpcszSourceFile; */
	/* sh.pTo = lpcszDestFile; */

	/* se invece di sovrascrivere bestialmente (quando in modo non-show) si vuole generare un nuovo nome file con progressivo, usare il flag FOF_RENAMEONCOLLISION  */
	/* sh.fFlags = bShowDialog ? (FOF_NORECURSION|FOF_SIMPLEPROGRESS) : (FOF_NORECURSION|FOF_NOCONFIRMATION|FOF_NOCONFIRMMKDIR|FOF_NOERRORUI|FOF_RENAMEONCOLLISION|FOF_SILENT); */
	sh.fFlags = bShowDialog ? (FOF_NORECURSION|FOF_SIMPLEPROGRESS) : (FOF_NORECURSION|FOF_NOCONFIRMATION|FOF_NOCONFIRMMKDIR|FOF_NOERRORUI|FOF_SILENT);

	/* effettua la copia */
	int nRet = SHFileOperation(&sh);

	/* in teoria, il successo della copia con SHFileOperation() e' garantito dal test (nRet==0 && sh.fAnyOperationsAborted==FALSE), ma non mi fido... */
	/* usare la modalita' share con CreateFile perche' altrimenti si incricca con il rilascio dell'handle da parte di SHFileOperation() con errore 32 */
	BOOL bFileExists = FALSE;
	HANDLE hHandle;
	if((hHandle = CreateFile(lpcszDestFile,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE/*0*/,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL))!=INVALID_HANDLE_VALUE)
	{
		CloseHandle(hHandle);
		bFileExists = TRUE;
	}

	return(sh.fAnyOperationsAborted ? FALSE : (bFileExists ? nRet==0 : FALSE));
}

/*
	RenameFileTo()

	Rinomina il file usando la MoveFile(), che a sua volta rinomina un file se la destinazione 
	si trova sullo stesso volume, o lo sposta se si trova su un volume diverso.

	Restituisce TRUE se riesce, FALSE altrimenti.
*/
BOOL RenameFileTo(LPCSTR lpcszOldFileName,LPCSTR lpcszNewFileName)
{
	ASSERTEXPR(lpcszOldFileName);
	ASSERTEXPR(lpcszNewFileName);

	/* se i nomi sono identici, ritorna */
	if(strcmp(lpcszOldFileName,lpcszNewFileName)==0)
		return(TRUE);

    /* MoveFileEx() rinomina o sposta un file esistente
       restituisce un valore diverso da zero in caso di successo, zero in caso di errore */
    return(MoveFileEx(lpcszOldFileName,lpcszNewFileName,MOVEFILE_REPLACE_EXISTING));
}

/*
	DeleteFileToRecycleBin()

	Elimina il file mettendolo nel cestino con opzione a dialogo con messaggio.

	Restituisce TRUE se riesce, FALSE altrimenti.
*/
BOOL DeleteFileToRecycleBin(HWND hWnd,LPCSTR lpcszFileName,BOOL bShowDialog,BOOL bAllowUndo)
{
	ASSERTEXPR(lpcszFileName);

	SHFILEOPSTRUCT sh = {0};
	sh.hwnd = hWnd;
	sh.wFunc = FO_DELETE;

	/* il buffer per il nomefile deve essere terminato da doppio NULL, quindi di assicura */
	char szFilename[_MAX_FILEPATH+128+1] = {0};
	strcpyn(szFilename,lpcszFileName,sizeof(szFilename));
	sh.pFrom = szFilename;

	FILEOP_FLAGS fProgress	= bAllowUndo ? ((FILEOP_FLAGS)FOF_ALLOWUNDO|FOF_SIMPLEPROGRESS) : ((FILEOP_FLAGS)FOF_SIMPLEPROGRESS);
	FILEOP_FLAGS fConfirm	= bAllowUndo ? ((FILEOP_FLAGS)FOF_ALLOWUNDO|FOF_NOCONFIRMATION|FOF_SILENT) : ((FILEOP_FLAGS)FOF_NOCONFIRMATION|FOF_SILENT);
	sh.fFlags = bShowDialog ? fProgress : fConfirm;
	/* sh.fFlags = bShowDialog ? (FOF_ALLOWUNDO|FOF_SIMPLEPROGRESS) : (FOF_ALLOWUNDO|FOF_NOCONFIRMATION|FOF_SILENT); */

	int nRet = SHFileOperation(&sh);

	BOOL bFileExists = FALSE;
	HANDLE hHandle;
	if((hHandle = CreateFile(lpcszFileName,GENERIC_READ,0/*FILE_SHARE_READ|FILE_SHARE_WRITE*/,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL))!=INVALID_HANDLE_VALUE)
	{
		CloseHandle(hHandle);
		bFileExists = TRUE;
	}

	return(sh.fAnyOperationsAborted ? FALSE : (bFileExists ? FALSE : (nRet==0)));
}

/*
	FileExists()

	Controlla se il file esiste (aprendolo).

	Restituisce TRUE se il file esiste, FALSE altrimenti.
*/
BOOL FileExists(LPCSTR lpcszFileName)
{
	ASSERTEXPR(lpcszFileName);

	BOOL bFileExists = FALSE;
	HANDLE hHandle = INVALID_HANDLE_VALUE;
	
	if((hHandle = CreateFile(lpcszFileName,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL))!=INVALID_HANDLE_VALUE)
	{
		CloseHandle(hHandle);
		bFileExists = TRUE;
	}
	
	return(bFileExists);
}

/*
    DoesFileExist()

	Controlla se il file esiste (ricavando gli attributi).

	Restituisce TRUE se il file esiste e riesce a ricavare gli attributi, FALSE altrimenti.
*/
BOOL DoesFileExist(LPCSTR lpcszFileName,LPDWORD pdwLastError)
{
	ASSERTEXPR(lpcszFileName);
	ASSERTEXPR(pdwLastError);

	DWORD dwAttributes = GetFileAttributes(lpcszFileName);
    *pdwLastError = 0L;

    if(dwAttributes==INVALID_FILE_ATTRIBUTES)
	{
		*pdwLastError = GetLastError();
        return(FALSE);
    }

	return((dwAttributes & FILE_ATTRIBUTE_ARCHIVE)!=0L);
}

/*
    DoesDirectoryExist()

	Controlla se la directory esiste (ricavando gli attributi).

	Occhio al codice:

	DWORD attr = GetFileAttributesW(L"C:\\TMP\\AAA");
	if (attr == INVALID_FILE_ATTRIBUTES) {
		DWORD err = GetLastError();
		switch (err) {
		case ERROR_FILE_NOT_FOUND:
			wprintf(L"C:\\TMP\\AAA non esiste (file finale mancante).\n");
			break;
		case ERROR_PATH_NOT_FOUND:
			wprintf(L"C:\\TMP\\AAA non esiste (directory intermedia mancante).\n");
			break;
		case ERROR_INVALID_DRIVE:
			wprintf(L"Unità Z: non esiste fisicamente.\n");
			break;
		case ERROR_INVALID_NAME:
		case ERROR_BAD_PATHNAME:
			wprintf(L"Percorso malformato o riservato al sistema.\n");
			break;
		default:
			wprintf(L"Altro errore: %lu\n", err);
		}
	}

	ERROR_FILE_NOT_FOUND(#2)	->	solo l’ultimo pezzo manca
	ERROR_PATH_NOT_FOUND(#3)	->	una cartella intermedia manca
	ERROR_INVALID_DRIVE(#15)	->	l’intera unità (es. Z:) non esiste
	ERROR_INVALID_NAME(#123)
	ERROR_BAD_PATHNAME(#161)	->	sintassi illegale o nome riservato

	In questo modo si puo' distinguere tra 'path valido ma inesistente' e 'path invalido'.
*/
BOOL DoesDirectoryExist(LPCSTR lpcszDirectory,LPDWORD pdwLastError/* passare NULL se non serve */)
{
	ASSERTEXPR(lpcszDirectory);

    if(pdwLastError)
		*pdwLastError = 0L;

	/* ricava gli attributi del pathname */
	DWORD dwAttributes = GetFileAttributes(lpcszDirectory);

	/* errore */
	if(dwAttributes==INVALID_FILE_ATTRIBUTES)
	{
		if(pdwLastError)
		{
			DWORD dwError = GetLastError();

			/* semplicemente non esiste il pathname (ad es. esiste C:\TMP ma non esiste C:\TMP\DATA) */
			/* FALSE + 0 */
			if(dwError==ERROR_FILE_NOT_FOUND || dwError==ERROR_PATH_NOT_FOUND)
				*pdwLastError = 0L;
			/* il pathname non e' valido nel sistema (non esiste fisicamente come risorsa, ad es. Z:) */
			/* FALSE + ERROR_INVALID_DRIVE|ERROR_INVALID_NAME|ERROR_BAD_PATHNAME */
			else if(dwError==ERROR_INVALID_DRIVE || dwError==ERROR_INVALID_NAME || dwError==ERROR_BAD_PATHNAME)
				*pdwLastError = dwError;
			/* FALSE + ERROR_INVALID_DATA */
			else
				*pdwLastError = ERROR_INVALID_DATA;
        }
		return(FALSE);
    }

	/* non e' INVALID_FILE_ATTRIBUTES, ma e' realmente una directory esistente ? */
	return((dwAttributes & FILE_ATTRIBUTE_DIRECTORY)!=0L);
}

/*
	FormatFileSize()

	Formatta la dimensione del file in forma abbreviata, stile MS-DOS.

	Restituisce il puntatore al buffer con i dati formattati.
*/
LPSTR FormatFileSize(QWORD qwFilesize,LPSTR lpszBuffer,UINT nSize)
{
	ASSERTEXPR(lpszBuffer);
	ASSERTEXPR(nSize > 0);

    double dblFormattedSize = 0.0;

	memset(lpszBuffer,'\0',nSize);

	/* formatta stile MS-DOS */
    if(qwFilesize < 1024LL)
    {
        /* meno di 1KB -> bytes */
        snprintf(lpszBuffer,nSize,"%lld bytes",qwFilesize);
    }
    else if(qwFilesize < 1024LL * 1024)
    {
        /* KB */
        dblFormattedSize = (double)qwFilesize / 1024.0;
        snprintf(lpszBuffer,nSize,"%.2f KB",dblFormattedSize);
    }
    else if(qwFilesize < 1024LL * 1024 * 1024)
    {
        /* MB */
        dblFormattedSize = (double)qwFilesize / (1024.0 * 1024.0);
        snprintf(lpszBuffer,nSize,"%.2f MB",dblFormattedSize);
    }
    else if(qwFilesize < 1024LL * 1024 * 1024 * 1024)
    {
        /* GB */
        dblFormattedSize = (double)qwFilesize / (1024.0 * 1024.0 * 1024.0);
        snprintf(lpszBuffer,nSize,"%.2f GB",dblFormattedSize);
    }
	else
    {
        /* TB */
        dblFormattedSize = (double)qwFilesize / (1024.0 * 1024.0 * 1024.0 * 1024.0);
        snprintf(lpszBuffer,nSize,"%.2f TB",dblFormattedSize);
    }
	
	return(lpszBuffer[0]=='\0' ? (char*)"" : lpszBuffer);
}

/*
	GetFileSizeBytes()

	Ricava la dimensione del file come QWORD.
	Specifica per file esagerati (oltre i 4,2 GB).

	Restituisce TRUE (impostando il parametro QWORD) se riesce, o FALSE per errore.
*/
BOOL GetFileSizeBytes(LPCSTR lpcszFileName,LPQWORD pqwFileSize)
{
	ASSERTEXPR(lpcszFileName);
	ASSERTEXPR(pqwFileSize);

    HANDLE hFile = CreateFile(	lpcszFileName,
								GENERIC_READ,
								FILE_SHARE_READ,
								NULL,
								OPEN_EXISTING,
								FILE_ATTRIBUTE_NORMAL,
								NULL);

    if(hFile!=INVALID_HANDLE_VALUE)
	{
		/* usa GetFileSizeEx per compatibilita' con file grandi */
		LARGE_INTEGER liFileSize;
		if(!GetFileSizeEx(hFile,&liFileSize))
		{
			CloseHandle(hFile);
			return(FALSE);
		}

		/* converte in QWORD (unsigned long long) */
		*pqwFileSize = liFileSize.QuadPart;

		CloseHandle(hFile);
	}

	return(TRUE);
}

/*
	LPFNGETFILESIZEEX

	Puntatore a funzione per l'indirizzo di GetFileSizeEx in kernel32.DLL.
*/
typedef BOOL (WINAPI* LPFNGETFILESIZEEX)(HANDLE hFile,PLARGE_INTEGER lpLargeInt);

/*
	GetFileSizeExtbyHandle()

	Calcola la dimensione del file usando l'handle passato come parametro.
	Specifica per file esagerati (oltre i 4,2 GB).

	Notare che il codice non usa la tecnica piu' efficente in forma intenzionata,
	lo scopo ultimo e' quello di illustrare una tecnica differente.

	Restituisce la dimensione del file o -1LL per errore.
*/
QWORD GetFileSizeExtbyHandle(HANDLE hFile,PLARGE_INTEGER lpLargeInt)
{
	ASSERTEXPR(hFile!=INVALID_HANDLE_VALUE);
	if(hFile==INVALID_HANDLE_VALUE)
		return((QWORD)-1LL);

	QWORD qwFileSize = (QWORD)-1LL;
	HMODULE hModule = LoadLibrary("kernel32.DLL");

	if(NULL!=hModule)
	{
		LPFNGETFILESIZEEX lpFnGetFileSizeEx = (LPFNGETFILESIZEEX)GetProcAddress(hModule,"GetFileSizeEx");
		
		if(NULL!=lpFnGetFileSizeEx)
		{
			LARGE_INTEGER li = {0};
			if(lpFnGetFileSizeEx(hFile,&li))
			{
				qwFileSize = li.QuadPart;
				if(lpLargeInt)
					memcpy(lpLargeInt,&li,sizeof(li));
			}
		}

		FreeLibrary(hModule);
	}

	return(qwFileSize);
}

/*
	GetFileSizeExtbyName()

	Calcola la dimensione del nome file passato come parametro.
	Specifica per file esagerati (oltre i 4,2 GB).

	Notare che il codice non usa la tecnica piu' efficente in forma intenzionata,
	lo scopo ultimo e' quello di illustrare una tecnica differente.

	Restituisce la dimensione del file o -1LL per errore.
*/
QWORD GetFileSizeExtbyName(LPCSTR lpcszFileName)
{
	ASSERTEXPR(lpcszFileName);

	QWORD qwFileSize = (QWORD)-1LL;
	WIN32_FIND_DATA stFindData = {0};
	HANDLE hHandle = INVALID_HANDLE_VALUE;

	if((hHandle = FindFirstFile(lpcszFileName,&stFindData))!=INVALID_HANDLE_VALUE)
	{
		FindClose(hHandle);
		
		if((stFindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)==0)
		{
			union {
				struct {DWORD low,high;} lh;
				QWORD size;
			} file;

			file.lh.low  = stFindData.nFileSizeLow;
			file.lh.high = stFindData.nFileSizeHigh;

			qwFileSize = file.size;
		}
	}

	return(qwFileSize);
}

/*
	GetFileAttr()

	Ricava gli attributi del nome file passato come parametro.
*/
BOOL GetFileAttr(FILE_ATTR* fileAttr)
{
	ASSERTEXPR(fileAttr);

	BOOL bResult = FALSE;
	HANDLE hHandle = INVALID_HANDLE_VALUE;

	fileAttr->filesize = (QWORD)-1LL;

	if((hHandle = FindFirstFile(fileAttr->filename,&fileAttr->finddata))!=INVALID_HANDLE_VALUE)
	{
		FindClose(hHandle);
		
		if((fileAttr->finddata.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)==0)
		{
			union {
				struct {DWORD low,high;} lh;
				QWORD size;
			} file;

			file.lh.low  = fileAttr->finddata.nFileSizeLow;
			file.lh.high = fileAttr->finddata.nFileSizeHigh;
			fileAttr->filesize = file.size;
	
			bResult = TRUE;
		}
	}

	return(bResult);
}

/*
	CopyFileMapped()

	Copia il file di input su quello di output, sovrascrivendo se gia' esiste.

	Non copia i files a dimensione 0, restituendo EOF (ERROR_HANDLE_EOF) in tal caso.
	Usa le mappature in memoria per copiare nel modo piu' rapido ed efficente possibile.
	(copia TurboMix! X)
	
	Restituisce ERROR_SUCCESS o il valore di GetLastError() per errore.
*/
DWORD CopyFileMapped(LPCSTR lpcszSrcFileName,LPCSTR lpcszDstFileName)
{
	ASSERTEXPR(lpcszSrcFileName);
	ASSERTEXPR(lpcszDstFileName);

	/*
	se ci si trova su x86, invece che su x64, mappare l'intero file in una botta sola, quando il file supera 
	1,5 ~ 2GB scasina leggermente, quindi imposta un limite oltre il quale la copia, viene eseguita per blocchi
	*/
	#define COPY_SINGLE_MAP_LIMIT   (256ULL * 1024 * 1024)  /* 256 MB */
	#define COPY_CHUNK_SIZE         (64ULL  * 1024 * 1024)  /* 64 MB */

    HANDLE hSrcFile = INVALID_HANDLE_VALUE;
    HANDLE hDstFile = INVALID_HANDLE_VALUE;
    HANDLE hSrcMap  = NULL;
    HANDLE hDstMap  = NULL;
	DWORD  dwResult = ERROR_SUCCESS;
    LARGE_INTEGER fileSize = {0};

	/* apre il file di input */
    hSrcFile = CreateFile(lpcszSrcFileName,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_FLAG_SEQUENTIAL_SCAN,NULL);
    if(hSrcFile==INVALID_HANDLE_VALUE)
    {
		dwResult = GetLastError();
        goto done;
    }

	/* ricava la dimensione del file per decidere se copiare in una botta sola o a blocchi */
    if(!GetFileSizeEx(hSrcFile,&fileSize))
    {
		dwResult = GetLastError();
        goto done;
    }
	if(fileSize.QuadPart==0LL) /* file vuoto! */
    {
		dwResult = ERROR_HANDLE_EOF;
        goto done;
    }

	/* apre (sovrascrivendo) o crea il file di output */
    hDstFile = CreateFile(lpcszDstFileName,GENERIC_READ | GENERIC_WRITE,0,NULL,CREATE_ALWAYS,0,NULL);
    if(hDstFile==INVALID_HANDLE_VALUE)
    {
		dwResult = GetLastError();
        goto done;
    }
    if(!SetFilePointerEx(hDstFile,fileSize,NULL,FILE_BEGIN) || !SetEndOfFile(hDstFile))
    {
		dwResult = GetLastError();
        goto done;
    }

	/* crea le mappature per i files */
    hSrcMap = CreateFileMapping(hSrcFile,NULL,PAGE_READONLY,0,0,NULL);
    if(!hSrcMap)
    {
		dwResult = GetLastError();
        goto done;
    }
    hDstMap = CreateFileMapping(hDstFile,NULL,PAGE_READWRITE,0,0,NULL);
    if(!hDstMap)
    {
		dwResult = GetLastError();
        goto done;
    }

	BOOL bSrcFlag;
	BOOL bDstFlag;

    /* effettua la copia */
    if((uint64_t)fileSize.QuadPart <= COPY_SINGLE_MAP_LIMIT)
    {
		/* 'trasforma' i files in indirizzi di memoria */
        void *pSrc = MapViewOfFile(hSrcMap,FILE_MAP_READ,0,0,0);
        void *pDst = MapViewOfFile(hDstMap,FILE_MAP_WRITE,0,0,0);
        if(!pSrc || !pDst)
        {
            dwResult = GetLastError();
            goto done;
        }
		
		/* i due file sono gia' mappati in memoria, quindi usa una semplice mempcy() */
        memcpy(pDst,pSrc,(size_t)fileSize.QuadPart);

		/* rimuove la mappatura */
        bSrcFlag = UnmapViewOfFile(pSrc);
        bDstFlag = UnmapViewOfFile(pDst);
        if(!bSrcFlag || !bDstFlag)
        {
            dwResult = GetLastError();
            goto done;
        }
    }
    else
    {
        uint64_t offset = 0LL;
        uint64_t remaining = (uint64_t)fileSize.QuadPart;

		/* dato che la copia avviene per 'blocchi', anche le mappature devono avvenire nello stesso modo, ossia per chunks */
        while(remaining)
        {
            SIZE_T chunk = (remaining > COPY_CHUNK_SIZE) ? (SIZE_T)COPY_CHUNK_SIZE : (SIZE_T)remaining;

            DWORD offLow  = (DWORD)(offset & 0xFFFFFFFF);
            DWORD offHigh = (DWORD)(offset >> 32);

			/* 'trasforma' i files in indirizzi di memoria */
            void *pSrc = MapViewOfFile(hSrcMap,FILE_MAP_READ,offHigh,offLow,chunk);
            void *pDst = MapViewOfFile(hDstMap, FILE_MAP_WRITE,offHigh,offLow,chunk);
            if(!pSrc || !pDst)
            {
                dwResult = GetLastError();
                goto done;
            }

			/* i due file sono gia' mappati in memoria, quindi usa una semplice mempcy() */
            memcpy(pDst, pSrc, chunk);

			/* rimuove la mappatura */
            bSrcFlag = UnmapViewOfFile(pSrc);
            bDstFlag = UnmapViewOfFile(pDst);
			if(!bSrcFlag || !bDstFlag)
			{
				dwResult = GetLastError();
				goto done;
			}

            offset    += chunk;
            remaining -= chunk;
        }

    }
	
	/* le due seguenti vanno chiamate in sequenza per garantire il completamento del ciclo: */

	FlushViewOfFile(NULL,0);	/* assicura che tutte le view mappate siano sincronizzate con la cache */
	FlushFileBuffers(hDstFile);	/* assicura che la cache venga scaricata sullo storage (il file si considera 'persistente') */

done:

	if(hSrcMap)
	{
		if(!CloseHandle(hSrcMap))
			if(dwResult==ERROR_SUCCESS)
				dwResult = GetLastError();
	}
	if(hDstMap)
	{
		if(!CloseHandle(hDstMap))
			if(dwResult==ERROR_SUCCESS)
				dwResult = GetLastError();
	}
	if(hSrcFile!=INVALID_HANDLE_VALUE)
	{
		if(!CloseHandle(hSrcFile))
			if(dwResult==ERROR_SUCCESS)
				dwResult = GetLastError();
	}
	if(hDstFile!=INVALID_HANDLE_VALUE)
	{
		if(!CloseHandle(hDstFile))
			if(dwResult==ERROR_SUCCESS)
				dwResult = GetLastError();
	}

    return(dwResult);
}

/*
	OpenMappedFile()

	Apre il file mappandolo in memoria.

	La funzione e' specifica per i files di testo, dato che aggiunge un carattere nullo alla fine della mappatura
	del file per poter trattare il buffer in cui viene letto come se fosse una stringa C null-terminated, cosa che
	permette l'uso delle funzioni str...(), etc., per cui NON si puo' usare tale funzione con file binari.

	La funzione legge la dimensione originale del file su disco e la salva in pMappedFile->qwFileSize (es.100 byte).
	Chiama quindi CreateFileMapping specificando una dimensione di mappatura di pMappedFile->qwFileSize + 1 (ossia 
	di 101 byte). Chiama quindi MapViewOfFile e scrive '\0' all'indice pMappedFile->qwFileSize + 1 (il byte #101).
	Pero', anche se il byte '\0' viene scritto sulla mappatura, il kernel del file system modifica comunque il file 
	su disco, per cui alla chiusura del file bisogna ripristinare il fine file originario.

	Prima di chiamare la funzione, azzerare la struttura che deve ricevere in input, copiando il nome del file nel 
	campo relativo.

	Restituisce TRUE se riesce, o FALSE per errore.	
*/
BOOL OpenMappedFile(MAPPEDFILE* pMappedFile)
{
	ASSERTEXPR(pMappedFile);

	BOOL bSuccess = FALSE;
	pMappedFile->dwError = ERROR_SUCCESS;
	pMappedFile->hFile = INVALID_HANDLE_VALUE;
	pMappedFile->hFileMap = NULL;
	pMappedFile->lpFileView = NULL;

	/*
	apre il file ricavando data e ora
	il flag FILE_SHARE_READ (invece di 0 per esclusivo) perche' possono verificarsi problemi di timing con un ipotetico
	codice che riempia il file nel momento esatto prima di mapparlo.
	*/
	if((pMappedFile->hFile = CreateFile(pMappedFile->szFileName,GENERIC_WRITE|GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL))!=INVALID_HANDLE_VALUE)
	{
		if(!GetFileTime(pMappedFile->hFile,&(pMappedFile->ftCreationTime),&(pMappedFile->ftLastAccessTime),&(pMappedFile->ftLastWriteTime)))
		{
			pMappedFile->dwError = GetLastError();
			goto done;
		}
	}
	else
	{
		pMappedFile->dwError = GetLastError();
		goto done;
	}

	/* dimensione originale del file */
	if(GetFileSizeExtbyHandle(pMappedFile->hFile,&(pMappedFile->largeInt)))
		pMappedFile->qwFileSize = pMappedFile->largeInt.QuadPart;
	else
	{
		pMappedFile->dwError = GetLastError();
		goto done;
	}

	/* errore, file a dimensione 0 */
	if(pMappedFile->qwFileSize <= 0L)
	{
		pMappedFile->dwError = ERROR_HANDLE_EOF;
		goto done;
	}

	/* calcola la dimensione mappata (con il +1) per controllare che l'aggiunta di un byte alla fine non causi un overflow (anche se improbabile) */
    LARGE_INTEGER mapSize;
    mapSize.QuadPart = pMappedFile->largeInt.QuadPart + sizeof(char); 
	if(mapSize.QuadPart <= pMappedFile->largeInt.QuadPart)
	{
		pMappedFile->dwError = ERROR_ARITHMETIC_OVERFLOW;
		goto done;
	}

	/* crea la mappa per il file usando la dimensione calcolata sopra (mapSize), ossia la dimensione del file + 1*/
	if((pMappedFile->hFileMap = CreateFileMapping(pMappedFile->hFile,NULL,PAGE_READWRITE,mapSize.HighPart,mapSize.LowPart,NULL))==NULL)
	{
		pMappedFile->dwError = GetLastError();
		goto done;
	}

	/* mappa il file */
	if((pMappedFile->lpFileView = MapViewOfFile(pMappedFile->hFileMap,FILE_MAP_WRITE,0,0,0))==NULL)
	{
		pMappedFile->dwError = GetLastError();
		goto done;
	}

	/* inserisce uno zero alla fine della mappa per poter scorrere il contenuto come se fosse una stringa */
	pMappedFile->pData = (LPSTR)(pMappedFile->lpFileView);
	pMappedFile->pData[pMappedFile->qwFileSize] = '\0';
	bSuccess = TRUE;

done:

	if(!bSuccess)
	{
		if(pMappedFile->lpFileView!=NULL)
			UnmapViewOfFile(pMappedFile->lpFileView);

		if(pMappedFile->hFileMap!=NULL)
			CloseHandle(pMappedFile->hFileMap);

		if(pMappedFile->hFile!=INVALID_HANDLE_VALUE)
			CloseHandle(pMappedFile->hFile);

		TRACEEXPR((_TRACE_FLAG_INFO,__FILE__,__LINE__,"OpenMappedFile(%s) failed -> GetLastError(%ld)\n",pMappedFile->szFileName,pMappedFile->dwError));
	}

	return(bSuccess);
}

/*
	CloseMappedFile()

	Chiude il file mappato in memoria, aperto previamente con OpenMappedFile().
	Il null aggiunto all'apertura del file viene aggiunto alla mappa, ma il kernel del file 
	system modifica di conseguenza il file fisico su disco, per cui qui deve ripristinare la
	dimensione, data e ora originali.

	Restituisce TRUE se riesce, o FALSE per errore.	
*/
BOOL CloseMappedFile(MAPPEDFILE* pMappedFile)
{
	ASSERTEXPR(pMappedFile);

	pMappedFile->dwError = ERROR_SUCCESS;

	/* chiude la mappa */
	if(!UnmapViewOfFile(pMappedFile->lpFileView))
		if(pMappedFile->dwError==ERROR_SUCCESS)
			pMappedFile->dwError = GetLastError();

	if(!CloseHandle(pMappedFile->hFileMap))
		if(pMappedFile->dwError==ERROR_SUCCESS)
			pMappedFile->dwError = GetLastError();

	/* posiziona il puntatore nel file per poter ripristinare il fine file originale */    
	LONG lHigh = pMappedFile->largeInt.HighPart;
	DWORD dwError = SetFilePointer(pMappedFile->hFile,pMappedFile->largeInt.LowPart,&lHigh,FILE_BEGIN);
	if((dwError==INVALID_SET_FILE_POINTER || dwError==ERROR_INVALID_PARAMETER) && GetLastError()!=NO_ERROR)
		if(pMappedFile->dwError==ERROR_SUCCESS)
			pMappedFile->dwError = GetLastError();

	if(!SetEndOfFile(pMappedFile->hFile))
		if(pMappedFile->dwError==ERROR_SUCCESS)
			pMappedFile->dwError = GetLastError();
    
	/* ripristina date/ora originali */
	if(!SetFileTime(pMappedFile->hFile,&(pMappedFile->ftCreationTime),&(pMappedFile->ftLastAccessTime),&(pMappedFile->ftLastWriteTime)))
		if(pMappedFile->dwError==ERROR_SUCCESS)
			pMappedFile->dwError = GetLastError();

	/* chiude il file */
	if(!CloseHandle(pMappedFile->hFile))
		if(pMappedFile->dwError==ERROR_SUCCESS)
			pMappedFile->dwError = GetLastError();

	if(pMappedFile->dwError!=ERROR_SUCCESS)
		TRACEEXPR((_TRACE_FLAG_INFO,__FILE__,__LINE__,"CloseMappedFile(%s) failed -> GetLastError(%ld)\n",pMappedFile->szFileName,pMappedFile->dwError));

	return(pMappedFile->dwError==ERROR_SUCCESS);
}

/*
	ChangeFileExtension()
	
	Cambia o aggiunge l'estensione a un nome file/path.
*/
BOOL ChangeFileExtension(LPSTR lpszFilename,UINT nSize,LPCSTR lpcszNewExt)
{
	ASSERTEXPR(lpszFilename);
	ASSERTEXPR(nSize > 0);
	ASSERTEXPR(lpcszNewExt);

	/* cerca l'ultima occorrenza del punto */
	char* pszLastDot = strrchr(lpszFilename,'.');

	/* cerca l'ultimo separatore di directory per evitare di confondersi
	   con punti in cartelle superiori (es. "C:\Mia.Cartella\file") */
	char* pszLastSlash = strrchr(lpszFilename,'\\');
	if(strrchr(lpszFilename,'/'))
	{
		char* pszAltSlash = strrchr(lpszFilename,'/');
		if(!pszLastSlash || pszAltSlash > pszLastSlash)
			pszLastSlash = pszAltSlash;
	}

	/* se il punto viene prima dello slash, non e' l'estensione del file ma parte del path */
	if(pszLastDot < pszLastSlash)
		pszLastDot = NULL;

	/* tronca la stringa al punto (se esiste) per rimuovere la vecchia estensione */
	if(pszLastDot)
		*pszLastDot = '\0';

	/* calcola la lunghezza necessaria */
	size_t nCurrentLen = strlen(lpszFilename);
	size_t nExtLen = strlen(lpcszNewExt);
	BOOL bNeedsDot = lpcszNewExt[0]!='.';
    
	/* verifica spazio: path attuale + '.' (se serve) + estensione + null */
	if(nCurrentLen + (bNeedsDot ? 1 : 0) + nExtLen + 1 > nSize)
		return(FALSE); /* buffer troppo piccolo */

	/* assembla la nuova stringa */
	if(bNeedsDot)
		strcat(lpszFilename,".");

	strcat(lpszFilename,lpcszNewExt);

	return(TRUE);
}

/*
	CheckFileExtension()

	Verifica se il nome file ha un estensione uguale a quella specificata per il confronto.
*/
BOOL CheckFileExtension(LPCSTR lpcszFilename,LPCSTR lpszExt)
{
	ASSERTEXPR(lpcszFilename);
	ASSERTEXPR(lpszExt);

    /* ricava il testo dell'estensione del nome file (salta il punto se presente) */
    LPCSTR lpcszDot = strrchr(lpcszFilename,'.');
    if(!lpcszDot)
        return(FALSE); /* nessuna estensione presente */

    LPCSTR lpcszFileExt = lpcszDot + 1;

    /* ricava il testo dell'estensione da testare (salta il punto se presente) */
    if(*lpszExt=='.')
        lpszExt++;

    return(lstrcmpi(lpcszFileExt,lpszExt)==0);
}

/*
	GetFileExtension()

	Restituisce il puntatore all'estensione del file, o NULL se non la trova.
	Se viene passata una URL, si occupa di escludere la query (?).
*/
LPCSTR GetFileExtension(LPCSTR lpcszFileName)
{
	ASSERTEXPR(lpcszFileName);

	const char* pExt = NULL;

	if(strchr(lpcszFileName,'?'))
	{
		char szUrl[L_MAX_URL_LENGTH+1] = {0};
		strcpyn(szUrl,lpcszFileName,sizeof(szUrl));
		char* p = strchr(szUrl,'?');
		if(p) *p = '\0';
		const char* pDot = strchr(szUrl,'.');
		if(pDot)
			pExt = pDot;
	}
	else
	{
		const char* p = strchr(lpcszFileName,'.');
		if(p)
			pExt = p;
	}

	return(pExt);
}

/*
	StripPathFromFile()

	Ricava il nome del file contenuto nel pathname.
	Il puntatore fa riferimento all'offset a cui inizia il nome file nella
	stringa, NON elimina fisicamente il pathname dalla stringa.

	Restituisce il puntatore al nome file dentro del pathname.
*/
LPCSTR StripPathFromFile(LPCSTR lpcszPathName)
{
	ASSERTEXPR(lpcszPathName);

	/* scorre fino all'ultimo '\' */
	char* pFile = (char*)strrchr(lpcszPathName,'\\');
	pFile = (pFile && *(pFile+1)) ? pFile+1 : (LPSTR)lpcszPathName;
	
	/* scorre  fino all'ultimo '/', se presente */
	char* p = (char*)strrchr(lpcszPathName,'/');
	if(p && *(p+1))
		pFile = p+1;
	
	return(pFile);
}

/*
	StripFileFromPath()

	Elimina il nome file dal pathname (backslash incluso opzionale).
	ASSUME che il pathname contenga (DEVE contenere) un nome file.

	Restituisce il puntatore al pathname.
*/
LPSTR StripFileFromPath(LPCSTR lpcszPathName,LPSTR lpszPath,UINT nSize,BOOL bRemoveBackslash)
{
	ASSERTEXPR(lpcszPathName);
	ASSERTEXPR(lpszPath);
	ASSERTEXPR(nSize > 0);

	strcpyn(lpszPath,lpcszPathName,nSize);

	char* p = (char*)strrchr(lpszPath,'\\');
	if(p)
		*((bRemoveBackslash ? p : p+1)) = '\0';

	return(lpszPath);
}

/*
	SplitFileName()

	Suddivide il pathname in path, nome file ed estensione.
	ASSUME che il pathname contenga (DEVE contenere) un nome file.
	Per i buffer in cui copia le suddivisioni, assume che le dimensioni siano come minimo 
	_MAX_FILEPATH, _MAX_FNAME, _MAX_EXT per path, nome ed estensione rispettivamente.

	Restituisce il puntatore al pathname.
*/
LPCSTR SplitFileName(LPCSTR lpcszPathName,LPSTR lpszPath,LPSTR pszName,LPSTR pszExt)
{
	ASSERTEXPR(lpcszPathName);
	ASSERTEXPR(lpszPath);
	ASSERTEXPR(pszName);
	ASSERTEXPR(pszExt);

	char* p = NULL;
	char szPath[_MAX_FILEPATH+1] = {0};
	strcpyn(lpszPath,StripFileFromPath(lpcszPathName,szPath,_MAX_FILEPATH,TRUE),_MAX_FILEPATH);

	strcpyn(pszName,StripPathFromFile(lpcszPathName),_MAX_FNAME);
	p = (char*)strrstr(pszName,".");
	if(p)
		*p='\0';

	p = (char*)strrstr(lpcszPathName,".");
	if(!p)
		p = (char*)"";
	strcpyn(pszExt,p,_MAX_EXT);

	return(lpcszPathName);
}

/*
	EnsureBackslash()

	Si assicura che il pathname termini con un backslash (\) aggiungendolo se necessario.
	Assicurarsi che il buffer sia di dimensione adecuata o il backslash non viene aggiunto.

	Restituisce il puntatore al pathname.
*/
LPSTR EnsureBackslash(LPSTR lpszPathName,UINT nSize)
{
	ASSERTEXPR(lpszPathName);
	ASSERTEXPR(nSize > 0);

	if(lpszPathName[strlen(lpszPathName)-1]!='\\')
		strcatn(lpszPathName,"\\",nSize);

	return(lpszPathName);
}

/*
	RemoveBackslash()

	Elimina il backslash (\) finale dal pathname.

	Restituisce il puntatore al pathname.
*/
LPSTR RemoveBackslash(LPSTR lpszPathName)
{
	ASSERTEXPR(lpszPathName);

	size_t i = strlen(lpszPathName);

	do {
		if(lpszPathName[i-1]=='\\')
		{
			lpszPathName[i-1] = '\0';
			i = strlen(lpszPathName);
		}
	} while(i-1 >= 0 && lpszPathName[i-1]=='\\');

	return(lpszPathName);
}

/*
	EnsurePathnameExists()
	
	Si assicura che il pathname esista, creandolo se necessario.

	Puo' ricevere un pathname puro, come:
		C:\Directory\SubDirectory
	un pathname terminato con \, come:
		C:\Directory\SubDirectory\
	o un pathname contenete un nome file, come:
		C:\Directory\SubDirectory\FileName.ext

	Per distinguere se il pathname contiene o meno un nome file, usa il '.' dell'eventuale
	estensione come indicatore: se lo trova elimina il nome (del file), risalendo fino al 
	primo \ che trova.

	Restituisce TRUE se riesce, o FALSE in caso di errore.
*/
BOOL EnsurePathnameExists(LPCSTR lpcszPathName,LPDWORD pdwError)
{
	ASSERTEXPR(lpcszPathName);
	ASSERTEXPR(pdwError);

	*pdwError = 0L;

	char szPathName[_MAX_PATH+1] = {0};

	/* pathname troppo lungo */
	if(strlen(lpcszPathName) > sizeof(szPathName)-1)
	{
		*pdwError = ERROR_BAD_PATHNAME;
		return(FALSE);
	}

	/* usa una copia locale */
	strcpyn(szPathName,lpcszPathName,sizeof(szPathName));

	/* verifica se il pathname contiene un nome file e nel caso lo elimina */
	char* p = strrchr(szPathName,'.');
	if(p)
	{
		p = strrchr(szPathName,'\\');
		if(p)
			*p = '\0';
	}

	/* verifica se il pathname termina con un \ e nel caso lo elimina */
	int n = (int)strlen(szPathName);
	if(szPathName[n-1]=='\\')
		szPathName[n-1] = '\0';

	return(CreatePathname(szPathName,pdwError));
}

/*
	CreatePathname()
	
	Crea il pathname completo, directory per directory, ricorsivamente.

	Restituisce TRUE se riesce, o FALSE in caso di errore.
*/
BOOL CreatePathname(LPCSTR lpcszPathName,LPDWORD pdwError)
{
	ASSERTEXPR(lpcszPathName);
	ASSERTEXPR(pdwError);

	*pdwError = 0L;

	if(CreateDirectory(lpcszPathName,NULL))
		return(TRUE);
    if((*pdwError = GetLastError())==ERROR_ALREADY_EXISTS)
		return(TRUE);

    char szParentDirectory[_MAX_PATH+1] = {0};
    strcpyn(szParentDirectory,lpcszPathName,sizeof(szParentDirectory));
    char* lastSlash = strrchr(szParentDirectory,'\\');
    if(!lastSlash)
		return(FALSE);
    *lastSlash = '\0';

    if(!CreatePathname(szParentDirectory,pdwError))
		return(FALSE);

	BOOL bRet = CreateDirectory(lpcszPathName,NULL) || ((*pdwError = GetLastError())==ERROR_ALREADY_EXISTS);
    
	return(bRet);
}

/*
	EnsureValidFileName()
	
	Elimina i caratteri non consentiti.
	Considera il primo parametro di input come un nome file, NON usare su pathnames,
	passare solo ed esclusivamente il nome del file.
*/
LPCSTR EnsureValidFileName(LPCSTR lpcszFileName,LPSTR lpszNewName,UINT cbNewName)
{
	ASSERTEXPR(lpcszFileName);
	ASSERTEXPR(lpszNewName);
	ASSERTEXPR(cbNewName > 0);

	int i = 0,n = 0,cb = 0;
	
	memset(lpszNewName,'\0',cbNewName);
	if(isalpha(lpcszFileName[0]) && lpcszFileName[1]==':' && lpcszFileName[2]=='\\')
	{
		lpszNewName[i++] = lpcszFileName[n++];
		lpszNewName[i++] = lpcszFileName[n++];
		lpszNewName[i++] = lpcszFileName[n++];
		cb = i;
	}
	for(; lpcszFileName[n] && i < (int)(cbNewName-1-cb); n++)
		if(!strchr("\\/:*?\"'<>|",lpcszFileName[n]))
			lpszNewName[i++] = lpcszFileName[n];
	
	strltrim(lpszNewName);
	strrtrim(lpszNewName);
	strstrim(lpszNewName);
	
	return(lpszNewName);
}

/*
	YetAnotherFileName()

	Verifica se gia' esiste un file con lo stesso nome, nel caso restituisce il nome con un progressivo numerico 
	incrementato ("name.ext" -> "name (n).ext") in modo che il chiamante eviti sovrascrivere la versione precedente.
	Incremento limitato a INT_MAX.
	Funzionalmente come la GetNext() in CFilenameFactory.cpp.

	Restituisce il puntatore al nuovo nome file, o NULL se fallisce.
*/
LPSTR YetAnotherFileName(LPCSTR lpcszFileName,LPSTR lpszNewName,UINT nNewNameSize)
{
	ASSERTEXPR(lpcszFileName);
	ASSERTEXPR(lpszNewName);
	ASSERTEXPR(nNewNameSize > 0);

	DWORD dwError = 0L;

	if(DoesFileExist(lpcszFileName,&dwError))
	{
		char szPath[_MAX_FILEPATH+1] = {0};
		char szName[_MAX_FNAME+1] = {0};
		char szExt[_MAX_EXT+1] = {0};

		memset(lpszNewName,'\0',nNewNameSize);

		/* assume _MAX_FILEPATH, _MAX_FNAME, _MAX_EXT per path, nome e ext */
		SplitFileName(lpcszFileName,szPath,szName,szExt);

		/* tutto ha un limite... */
		for(int n=0; n < INT_MAX; n++)
		{
			memset(lpszNewName,'\0',nNewNameSize);
			snprintf(lpszNewName,nNewNameSize,"%s\\%s (%d)%s",szPath,szName,n+1,szExt);
			ASSERTEXPR(strcmp(lpcszFileName,lpszNewName)!=0);

			if(!DoesFileExist(lpszNewName,&dwError))
			{
				TRACEEXPR((_TRACE_FLAG_INFO,__FILE__,__LINE__,"YetAnotherFileName(): old=%s, new=%s\n",lpcszFileName,lpszNewName));
				return(lpszNewName);
			}
		}
	}

	return(NULL);
}

/*
	DateTimeToString()

	Converte data e ora in formato MS-DOS a stringa in formato "DD MMM YYYY HH:MM:SS".

	Restituisce il puntatore alla stringa convertita.
*/
LPSTR DateTimeToString(UINT uDate,UINT uTime,LPSTR lpszBuffer,UINT nSize)
{
	ASSERTEXPR(uDate > 0);
	ASSERTEXPR(uTime > 0);
	ASSERTEXPR(lpszBuffer);
	ASSERTEXPR(nSize > 0);

	const char* const months[] = {"???","jan","feb","mar","apr","may","jun","jul","aug","sep","oct","nov","dec"};
	FILETIME ftFileTime = {0};
	SYSTEMTIME ftSystemTime = {0};
	memset(lpszBuffer,'\0',nSize);

	/* API Win32 */
	if(DosDateTimeToFileTime((WORD)uDate,(WORD)uTime,&ftFileTime))
		if(FileTimeToSystemTime(&ftFileTime,&ftSystemTime))
			snprintf(lpszBuffer,nSize,"%02d %s %04d %02d:%02d:%02d",ftSystemTime.wDay,months[ftSystemTime.wMonth],ftSystemTime.wYear,ftSystemTime.wHour,ftSystemTime.wMinute,ftSystemTime.wSecond);

	return(lpszBuffer);
}

/*
	StringToDateTime()
	
	Converte una stringa data/ora in formato "DD MMM YYYY HH:MM:SS" a formato MS-DOS. 

	Restituisce TRUE se riesce, o FALSE in caso di errore.
 */
BOOL StringToDateTime(LPCSTR lpcszDateTime,LPWORD pwDate,LPWORD pwTime)
{
	ASSERTEXPR(lpcszDateTime);
	ASSERTEXPR(pwDate);
	ASSERTEXPR(pwTime);

	BOOL bRet = FALSE;
	const char* const months[] = {"???","jan","feb","mar","apr","may","jun","jul","aug","sep","oct","nov","dec"};
    
	SYSTEMTIME ftSystemTime = {0};
    char month_str[4] = {0};
    int day,year,hour,minute,second;

    int scanned_items = sscanf(lpcszDateTime,"%d %3s %d %d:%d:%d",&day,month_str,&year,&hour,&minute,&second);

    /* se non sono 6 elementi, errore di parsing della stringa */
    if(scanned_items!=6)
        return(bRet);

    /* cerca il numero del mese corrispondente alla stringa */
    int month_num = 0;
    for(int i=1; i <= 12; ++i) {
        if(stricmp(month_str,months[i])==0)
		{
            month_num = i;
            break;
        }
    }

    if(month_num==0)
        return(bRet);

    /* riempie la struttura SYSTEMTIME */
    ftSystemTime.wYear = (WORD)year;
    ftSystemTime.wMonth = (WORD)month_num;
    ftSystemTime.wDay = (WORD)day;
    ftSystemTime.wHour = (WORD)hour;
    ftSystemTime.wMinute = (WORD)minute;
    ftSystemTime.wSecond = (WORD)second;
    ftSystemTime.wMilliseconds = 0; /* MS-DOS non ha millisecondi */

    /* convalida */
    if(day < 1 || day > 31 || hour < 0 || hour > 23 || minute < 0 || minute > 59 || second < 0 || second > 59)
        return(bRet);
 
    FILETIME ftFileTime = {0};

    /* converte SYSTEMTIME in FILETIME */
    if(SystemTimeToFileTime(&ftSystemTime,&ftFileTime))
	{
		/* converte FILETIME in data e ora MS-DOS */
		if(FileTimeToDosDateTime(&ftFileTime,pwDate,pwTime))
			bRet = TRUE;
	}

    return(bRet);
}

/*
	SetFileDateTime()

	Imposta data/ora del file in formato MS-DOS.

	Restituisce TRUE se riesce, o FALSE in caso di errore.

	Esempio:

    WORD newDateWord = dateWord;
    WORD newTimeWord = timeWord;

    // imposta un timestamp specifico:
    SYSTEMTIME st;
    GetSystemTime(&st);							// ora attuale UTC
    SystemTimeToFileTime(&st,&ftLocalTime);		// converte a FILETIME locale
    FileTimeToDosDateTime(&ftLocalTime,&newDateWord,&newTimeWord);

    if(SetFileDateTime(testFileName,newDateWord,newTimeWord)) {
        printf("Timestamp impostato con successo per '%s'.\n", testFileName);
        // verifica leggendo di nuovo i timestamp per confermare
        WORD verifyDate = 0, verifyTime = 0;
        if(GetFileeTime(testFileName, &verifyDate, &verifyTime)) {
            printf("  Verifica - Data Word: 0x%04X, Ora Word: 0x%04X\n", verifyDate, verifyTime);
        }
    } else {
        printf("Errore nell'impostazione dei timestamp per '%s'.\n", testFileName);
    }
*/
BOOL SetFileDateTime(LPCSTR lpcszFileName,WORD uDate,WORD uTime)
{
	ASSERTEXPR(lpcszFileName);

	BOOL bSet = FALSE;
	HANDLE hHandle = NULL;

	if((hHandle = CreateFile(lpcszFileName,GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL))!=INVALID_HANDLE_VALUE)
	{
		/* converte data e ora da WORD a FILETIME (ora locale) */
		FILETIME ftLocalTime = {0};
		if(DosDateTimeToFileTime(uDate,uTime,&ftLocalTime))
		{
			/* converte da ora locale a UTC */
			FILETIME ftLastWrite = {0};
			if(LocalFileTimeToFileTime(&ftLocalTime,&ftLastWrite))
				bSet = SetFileTime(hHandle,NULL,NULL,&ftLastWrite);
		}

		CloseHandle(hHandle);
	}

	return(bSet);
}

/*
	GetFileDateTime()

	Ricava data/ora (di ultima modifica, NON di creazione) del file in formato MS-DOS.
	(unificare con GetFileLastTime() ???)

	Restituisce TRUE se riesce, o FALSE in caso di errore.
*/
BOOL GetFileDateTime(LPCSTR lpcszFileName,LPWORD lpuDate,LPWORD lpuTime,LPQWORD pqwSize/* passare NULL se non serve */)
{
	ASSERTEXPR(lpcszFileName);
	ASSERTEXPR(lpuDate);
	ASSERTEXPR(lpuTime);

	BOOL bGet = FALSE;
	HANDLE hHandle = NULL;
    LARGE_INTEGER liSize = {0};

	*lpuDate = *lpuTime = 0L;
    if(pqwSize!=NULL)
		*pqwSize = 0LL;

	/* ricava data e ora e converte da WORD a FILETIME (ora locale) */
	if((hHandle = CreateFile(lpcszFileName,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL))!=INVALID_HANDLE_VALUE)
	{
		/* GetFileTime() accetta tre parametri per la data: lpCreationTime (data di creazione), lpLastAccessTime (data di ultimo accesso) e 
		lpLastWriteTime (data di ultima modifica/scrittura), qui passiamo solo il terzo per ottenere la data di ultima modifica, NON la
		data di creazione (in locale)
		la data di modifica e' relativa a quando il file fu originariamente creato sa zero o modificato, quella di creazione e' sempre
		quella locale, ossia quella in cui il file originario e' stato copiato in locale */
		FILETIME ftLastWrite = {0};
		if(GetFileTime(hHandle,NULL,NULL,&ftLastWrite))
		{
			/* converte da UTC a ora locale */
			FILETIME ftLocalTime = {0};
			if(FileTimeToLocalFileTime(&ftLastWrite,&ftLocalTime))
				bGet = (BOOL)FileTimeToDosDateTime(&ftLocalTime,lpuDate,lpuTime);
		}

        /* dimensione */
		if(pqwSize!=NULL)
        {
			/* se fallisce, pqwSize rimane 0 */
            if(GetFileSizeEx(hHandle,&liSize))
            {
                /* liSize.QuadPart e' il membro a 64-bit della LARGE_INTEGER */
                *pqwSize = liSize.QuadPart; 
            }
        }

		CloseHandle(hHandle);
	}

	return(bGet);
}

/*
	GetFileLastTime()

	Ottiene la data/ora di ultimo aggiornamento di un file.
	(unificare con GetFileDateTime() ???)

	Restituisce TRUE se riesce, o FALSE in caso di errore.
*/
BOOL GetFileLastTime(LPCSTR lpcszFileName,FILETIME* lpLastWriteTime)
{
	ASSERTEXPR(lpcszFileName);
	ASSERTEXPR(lpLastWriteTime);

    /* apre il file solo per leggere gli attributi */
	HANDLE hFile = CreateFile(	lpcszFileName,
								FILE_READ_ATTRIBUTES,
								FILE_SHARE_READ | FILE_SHARE_WRITE,
								NULL,
								OPEN_EXISTING,
								FILE_ATTRIBUTE_NORMAL,
								NULL
								);

	if(hFile==INVALID_HANDLE_VALUE)
		return(FALSE);

	BOOL bResult = GetFileTime(hFile,NULL,NULL,lpLastWriteTime);
	CloseHandle(hFile);

	return(bResult);
}

/*
	FileDateTimeToGregorian()

	Converte data/ora da formato MS-DOS a formato Gregoriano.
*/
void FileDateTimeToGregorian(GREGORIANDATETIME* pGregorianDateTime)
{
	ASSERTEXPR(pGregorianDateTime);

#if 1 /* METODO #1 */

	pGregorianDateTime->nYear = ((pGregorianDateTime->wDate >> 9) & 0x7F) + 1980;
	pGregorianDateTime->nMonth = (pGregorianDateTime->wDate >> 5) & 0x0F;
	pGregorianDateTime->nDay = pGregorianDateTime->wDate & 0x1F;

	pGregorianDateTime->nHour = (pGregorianDateTime->wTime >> 11) & 0x1F;
	pGregorianDateTime->nMinute = (pGregorianDateTime->wTime >> 5) & 0x3F;
	pGregorianDateTime->nSecond = (pGregorianDateTime->wTime & 0x1F) * 2;

	/* mktime per calcolare il giorno della settimana */
	struct tm datetime = {0};
	datetime.tm_year = pGregorianDateTime->nYear - 1900; /* anno - 1900 */
	datetime.tm_mon = pGregorianDateTime->nMonth - 1;	 /* mese base 0 */
	datetime.tm_mday = pGregorianDateTime->nDay;
	mktime(&datetime);

	/* il giorno della settimana e' a base 0 (0-6) */
	pGregorianDateTime->nDayOfWeek = datetime.tm_wday;
	const char* pDays[] = {"Sun","Mon","Tue","Wed","Thu","Fry","Sat"};
	strcpy(pGregorianDateTime->szDayOfWeek,pDays[pGregorianDateTime->nDayOfWeek]);

#else /* METODO #2 */

	FILETIME file_time;
	SYSTEMTIME sys_time;

	/* converte da DOS a FILETIME */
	if(DosDateTimeToFileTime(wDate,wTime,&file_time))
	{
		/* converte da FILETIME a SYSTEMTIME */
		if(FileTimeToSystemTime(&file_time,&sys_time))
		{
			const char* days[] = {"Sun","Mon","Tue","Wed","Thu","Fry","Sat"};
			/* giorno della settimana -> days[sys_time.wDayOfWeek] */
		}
	}

#endif
}

/*
	GregorianDateTimeToFile()

	Converte data/ora da formato Gregoriano a formato MS-DOS.

	Restituisce TRUE se riesce, o FALSE in caso di errore.
*/
BOOL GregorianDateTimeToFile(GREGORIANDATETIME* pGregorianDateTime)
{
	ASSERTEXPR(pGregorianDateTime);

	BOOL bRet = FALSE;
    SYSTEMTIME st = {0};
    FILETIME ft = {0};

    st.wYear   = (WORD)pGregorianDateTime->nYear;
    st.wMonth  = (WORD)pGregorianDateTime->nMonth;
    st.wDay    = (WORD)pGregorianDateTime->nDay;
    st.wHour   = (WORD)pGregorianDateTime->nHour;
    st.wMinute = (WORD)pGregorianDateTime->nMinute;
    st.wSecond = (WORD)pGregorianDateTime->nSecond;

    /* converte da SYSTEMTIME a FILETIME (formato UTC) */
    if(SystemTimeToFileTime(&st,&ft))
	{
		/* converte da FILETIME a formato MS-DOS */
		if(FileTimeToDosDateTime(&ft,&pGregorianDateTime->wDate,&pGregorianDateTime->wTime))
			bRet = TRUE;
	}

    return(bRet);
}

/*
	CompareFileDateTime()

	Confronta data e ora in formato MS-DOS di due file.
	
	Restituisce:
	 0 se il file A e' piu' recente di B
	 1 se il file A ed il file B hanno la stessa data/ora
	-1 se il file B e' piu' recente di A
 */
int CompareFileDateTime(WORD wFileAdate,WORD wFileAtime,WORD wFileBdate,WORD wFileBtime)
{
	ASSERTEXPR(wFileAdate > 0);
	ASSERTEXPR(wFileAtime > 0);
	ASSERTEXPR(wFileBdate > 0);
	ASSERTEXPR(wFileBtime > 0);

    /* confronta prima le date */
    if(wFileAdate > wFileBdate)
	{
        return 0; /* file A piu' recente (data A successiva a data B) */
    }
	else if(wFileAdate < wFileBdate)
	{
        return -1; /* file B piu' recente (data B successiva a data A) */
    }
	else
	{
        /* le date sono uguali, confronta le ore */
        if(wFileAtime > wFileBtime)
		{
            return 0; /* file A piu' recente (ora A successiva a ora B) */
        }
		else if(wFileAtime < wFileBtime)
		{
            return -1; /* file B piu' recente (ora B successiva a ora A) */
        }
		else
		{
            return 1; /* file A e B hanno la stessa data/ora */
        }
    }
}

/*
	CompareFilebyDate()
*/
BOOL CompareFilebyDate(LPCSTR lpcszFileSrc,FILETIME* targetFileTime)
{
	ASSERTEXPR(lpcszFileSrc);
	ASSERTEXPR(targetFileTime);

    HANDLE hSrcFile = CreateFile(lpcszFileSrc,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);
    if(hSrcFile==INVALID_HANDLE_VALUE)
		return(FALSE);

	FILETIME srcFileTime;
	GetFileTime(hSrcFile,NULL,NULL,&srcFileTime);
	CloseHandle(hSrcFile);

    return(CompareFileTime(&srcFileTime,targetFileTime) > 0);
}

/*
	CompareFileTimebyName()
	
	Confronta data/ora dei files.

	Restituisce TRUE se il file sorgente e' piu' recente o se il file di destinazione non esiste, 
	FALSE altrimenti.
*/
BOOL CompareFileTimebyName(LPCSTR lpcszFileSrc,LPCSTR lpcszFileDst)
{
	ASSERTEXPR(lpcszFileSrc);
	ASSERTEXPR(lpcszFileDst);

    HANDLE hSrcFile = CreateFile(lpcszFileSrc,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);
    if(hSrcFile==INVALID_HANDLE_VALUE)
		return(FALSE);

    HANDLE hDstFile = CreateFile(lpcszFileDst,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);
    if(hDstFile==INVALID_HANDLE_VALUE)
	{
        CloseHandle(hSrcFile);
        return(TRUE);
    }

    FILETIME srcFileTime;
	FILETIME dstFileTime;
    GetFileTime(hSrcFile,NULL,NULL,&srcFileTime);
    GetFileTime(hDstFile,NULL,NULL,&dstFileTime);

    CloseHandle(hSrcFile);
    CloseHandle(hDstFile);

    return(CompareFileTime(&srcFileTime,&dstFileTime) > 0);
}

/*
	GetFileDateSysDateDiff()
	
	Per calcolare la differenza in minuti in modo preciso ed evitare i problemi legati ai calcoli manuali (mesi 
	di 30/31 giorni, anni bisestili, ecc.), in Win32 bisogna convertire tutto nel formato FILETIME.
	FILETIME e' una struttura a 64 bit che conta gli intervalli di 100 nanosepolcondi dal 1° gennaio 1601 ed e' 
	perfetto per le sottrazioni perche' e' un numero lineare.
	Si ottengono quindi i due valori (del file, passato come parametro, e di sistema, ricavato in loco), si
	sottraggono e si divide per la costante dei minuti.

	Attenzione ai fusi orari (UTC vs Local): GetFileTime() e GetSystemTime() restituiscono data/ora UTC, quindi
	non usare GetLocalTime() per i confronti perche' potrebbero generarsi sfasamenti rispetto alla data del file.
	In altre parole, eseguire sempre tutti i calcoli interni in UTC e convertirli in "ora locale" solo se/quando 
	si devono visualizzare per l'utente.

	Esempio per cache file locale/server:

		int minutiLimite = 120;
		long diff = GetDiffInMinutes(ftDelMioFile);

		if(diff < 0)
			// file nel futuro, l'ora del server o del pc e' sballata

		if(diff > minutiLimite) {
			// il file e' vecchio, intervallo cache scaduto, riscarica
		} else if (diff == minutiLimite) {
			// caso esatto
		} else if (diff < minutiLimite) {
			// il file e' recente, rientra nell'intervallo della cache
		}
*/
long GetFileDateSysDateDiff(FILETIME* pftFileTime)
{
	ASSERTEXPR(pftFileTime);

	/*
	quanti intervalli da 100ns ci sono in un minuto?
	1 secondo = 10.000.000 (10^7) intervalli
	1 minuto  = 60 * 10.000.000 = 600.000.000
	*/
	#define TICKS_PER_MINUTE 600000000LL /* nota: LL per signed long long */

	/* NON usare QWORD peche' e' unsigned, LONGLONG invece e' signed */
    FILETIME ftNow = {0};
    SYSTEMTIME stNow = {0};
    ULARGE_INTEGER uiFile = {0};
	ULARGE_INTEGER uiNow = {0};
    LONGLONG diffTicks = 0LL;

    /* ottiene l'ora di sistema corrente (UTC per coerenza con i file) */
    GetSystemTime(&stNow);
    SystemTimeToFileTime(&stNow,&ftNow);

    /* converte le strutture FILETIME in interi a 64 bit */
    uiFile.LowPart  = pftFileTime->dwLowDateTime;
    uiFile.HighPart = pftFileTime->dwHighDateTime;

    uiNow.LowPart   = ftNow.dwLowDateTime;
    uiNow.HighPart  = ftNow.dwHighDateTime;

    /* calcolo della differenza con segno (+/-) */
    /* (ora attuale - ora del file) */
    diffTicks = (LONGLONG)uiNow.QuadPart - (LONGLONG)uiFile.QuadPart;

    /*
	divisione e cast a long
    se diffTicks e' positivo: il file e' nel PASSATO (normale)
    se diffTicks e' negativo: il file e' nel FUTURO (anomalia server)
	*/
    return((long)(diffTicks / TICKS_PER_MINUTE));
}

/*
	GetTaskBarPos()

	Ricava la posizione della taskbar.

	Restituisce TRUE se riesce, o FALSE in caso di errore.
*/
BOOL GetTaskBarPos(TASKBARPOS* tbi)
{
	ASSERTEXPR(tbi);

	memset(tbi,'\0',sizeof(TASKBARPOS));
	tbi->nTaskbarPlacement = -1;
	tbi->nScreenWidth = GetSystemMetrics(SM_CXSCREEN);
	tbi->nScreenHeight = GetSystemMetrics(SM_CYSCREEN);
	
	tbi->hWnd = FindWindow("Shell_TrayWnd",NULL);
	if(tbi->hWnd)
	{
		GetWindowRect(tbi->hWnd,&(tbi->rc));

		tbi->nTaskbarWidth = tbi->rc.right - tbi->rc.left;
		tbi->nTaskbarHeight = tbi->rc.bottom - tbi->rc.top;

		/* Daniel Lohmann: Calculate taskbar position from its window rect. However, on XP it may be that the
		taskbar is slightly larger or smaller than the screen size. Therefore we allow some tolerance here. */
		if(NEARLYEQUAL(tbi->rc.left,0,TASKBAR_X_TOLERANCE) && NEARLYEQUAL(tbi->rc.right,tbi->nScreenWidth,TASKBAR_X_TOLERANCE))
			tbi->nTaskbarPlacement = NEARLYEQUAL(tbi->rc.top,0,TASKBAR_Y_TOLERANCE) ? ABE_TOP : ABE_BOTTOM;
		else 
			tbi->nTaskbarPlacement = NEARLYEQUAL(tbi->rc.left,0,TASKBAR_X_TOLERANCE ) ? ABE_LEFT : ABE_RIGHT;

		return(TRUE);
	}

	return(FALSE);
}

/*
	SetForegroundWindowEx()

	Schiaffa la finestra in primo piano.
*/
void SetForegroundWindowEx(HWND hWnd,BOOL bInvalidate)
{
	ASSERTEXPR(hWnd);

	/* occhio che AttachThreadInput() scricca il debugger... (VC6.0) */
#ifndef _DEBUG
	AttachThreadInput(GetWindowThreadProcessId(GetForegroundWindow(),NULL),GetCurrentThreadId(),TRUE);
#endif

	/* forza un refresh della finestra */
	ShowWindow(hWnd,SW_RESTORE);
	SetForegroundWindow(hWnd);
	SetFocus(hWnd);
	if(bInvalidate)
		InvalidateRect(hWnd,NULL,TRUE);

#ifndef _DEBUG
	AttachThreadInput(GetWindowThreadProcessId(GetForegroundWindow(),NULL),GetCurrentThreadId(),FALSE);
#endif
}

/*
	AnsiToWideChar()
	
	Converte una stringa ANSI (char*) in Wide Character (wchar_t*).

	Restituisce un puntatore alla stringa wide allocata che deve essere liberata con free() o NULL per errore.
*/
wchar_t* AnsiToWideChar(LPCSTR pszAnsi,UINT codePage/* CP_ACP, CP_UTF8 */)
{
	ASSERTEXPR(pszAnsi);
	if(!pszAnsi)
		return(NULL);

	int len = MultiByteToWideChar(codePage,0,pszAnsi,-1,NULL,0);
	if(len <= 0)
		return(NULL);

	wchar_t* pwszWide = (wchar_t*)calloc(len,sizeof(wchar_t));
	if(!pwszWide)
		return NULL;

	if(MultiByteToWideChar(codePage,0,pszAnsi,-1,pwszWide,len)==0)
	{
		free(pwszWide);
		return(NULL);
	}

	return(pwszWide);
}

/*
	WideCharToAnsi()
	
	Converte una stringa Wide Character (wchar_t*) in ANSI (char*).
	
	Restituisce un puntatore alla stringa ANSI allocata che deve essere liberata con free() o NULL per errore.
*/
LPSTR WideCharToAnsi(const wchar_t* pwszWide,UINT codePage/* CP_ACP, CP_UTF8 */)
{
	ASSERTEXPR(pwszWide);
	if(!pwszWide)
		return(NULL);

	// il 2 e 8 parametro di WideCharToMultiByte() controllano il tipo di conversione e se e' avvenuta:
	// WC_NO_BEST_FIT_CHARS e bUsedDefaultChar x evitare alterazioni silenzione del testo originale
	// 0 e NULL x cambiare automaticamente al carattere piu' simile

    int len = WideCharToMultiByte(codePage,0,pwszWide,-1,NULL,0,NULL,NULL);
    if(len <= 0)
		return(NULL);
    
	char* pszAnsi = (char*)calloc(len,sizeof(char));
    if(!pszAnsi)
		return(NULL);
    
	if(WideCharToMultiByte(codePage,0,pwszWide,-1,pszAnsi,len,NULL,NULL)==0)
	{
        free(pszAnsi);
        return(NULL);
    }
    
	return(pszAnsi);
}

/*
	wcscount()

	Conta le occorrenze di un carattere nella stringa.
*/
int wcscount(LPCWSTR szString,LPCWSTR szChar)
{
	int count = 0;
	LPCWSTR p = szString;
	while((p = wcsstr(p,szChar))!=NULL)
	{
		count++;
		p += wcslen(szChar);
	}
	return(count);
}

/*
	wcsistr()

	Come wcsstr(), pero' ignorando la differenza tra maiuscole e minuscole.
*/
wchar_t *wcsistr(const wchar_t *haystack,const wchar_t *needle)
{
	/* se needle e' vuoto, restituisce haystack (come fa wcsstr) */
	if(*needle==L'\0')
		return((wchar_t *)haystack);

	/* scansiona ogni posizione di haystack */
	for(const wchar_t *h = haystack; *h != L'\0'; h++)
	{
		const wchar_t *h2 = h;
		const wchar_t *n  = needle;

		/* confronta carattere per carattere in modo case-insensitive */
		while(*n!=L'\0' && towlower((wint_t)*h2)==towlower((wint_t)*n))
		{
			h2++;
			n++;
		}

		/* se ha consumato tutto needle, ha trovato la corrispondenza */
		if(*n==L'\0')
			return((wchar_t *)h);
	}

	return(NULL);
}

/*
	GetLineFromStdin()

	Legge una riga da stdin, rimuove il carattere di newline ('\n') e svuota il buffer di input se necessario.
	Per ovviare allo scasinamento della fgets().

	Restituisce la lunghezza della stringa letta o 0 per errore.
*/
UINT GetLineFromStdin(LPSTR lpszBuffer,UINT nSize)
{
	ASSERTEXPR(lpszBuffer);
	ASSERTEXPR(nSize > 0);

	/* legge, restituisce subito 0 se errore di lettura (EOF o interruzione) */
	if(fgets(lpszBuffer,nSize,stdin)==NULL)
		return(0);

	/* rimuove il carattere '\n' finale, se presente */
	size_t len = strlen(lpszBuffer);
	if(len > 0 && lpszBuffer[len-1]=='\n')
	{
		lpszBuffer[len-1] = '\0';
		return((UINT)strlen(lpszBuffer));
	}

	/* se arriva qui, la riga era piu' lunga di 'size' ed il '\n' e' ancora nel buffer, lo pulisce */
	int c;
	while((c = getchar())!='\n' && c != EOF)
		;
    
	/* controlla se l'input ha avuto successo (non EOF) */
	return((c == EOF) ? 0 : (UINT)strlen(lpszBuffer));
}

/*
	ResetConsoleBuffer()

	Reimposta il buffer per la console con i valori di default, assicurando la 
	(ri)apparizione della barre di scrolling.
*/
void ResetConsoleBuffer(void)
{
	HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
	if(hConsole==INVALID_HANDLE_VALUE)
		return;

	CONSOLE_SCREEN_BUFFER_INFO csbi = {0};
	if(GetConsoleScreenBufferInfo(hConsole,&csbi))
	{
		// imposta una dimensione buffer amplia
		// quando le colonne del buffer (120) superano la larghezza della finestra
		// attuale, la barra di scorrimento orizzontale appare immediatamente
		COORD newSize = {120,9000};

		// se la finestra corrente e' piu' grande del nuovo buffer, stringe prima
		// la finestra per evitare il fallimento di SetConsoleScreenBufferSize()
		if(csbi.srWindow.Right - csbi.srWindow.Left + 1 > newSize.X)
		{
			SMALL_RECT tmpRect = {0,0,newSize.X-1,csbi.srWindow.Bottom-csbi.srWindow.Top};
			SetConsoleWindowInfo(hConsole,TRUE,&tmpRect);
		}

		SetConsoleScreenBufferSize(hConsole,newSize);
	}
}

/*
	InitConsoleGeometry()

	Reinizializza il buffer per la console con i valori specificati, assicurando la 
	(ri)apparizione della barre di scrolling.
*/
void InitConsoleGeometry(short nWidth,short nHeight)
{
	HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
	if(hConsole==INVALID_HANDLE_VALUE)
		return;

	CONSOLE_SCREEN_BUFFER_INFO csbi = {0};
	if(GetConsoleScreenBufferInfo(hConsole,&csbi))
	{
		COORD newSize = {0};
		newSize.X = nWidth;
		newSize.Y = nHeight;

		// se la finestra corrente fosse piu' grande del nuovo buffer che si vuole impostare,
		// SetConsoleScreenBufferSize() fallirebbe, quindi rimpicciolisce prima la finestra temporaneamente
		short nCurrentConsoleWidth = csbi.srWindow.Right - csbi.srWindow.Left + 1;
		short nCurrentConsoleHeight = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;

		if(nCurrentConsoleWidth > newSize.X || nCurrentConsoleHeight > newSize.Y)
		{
			SMALL_RECT rectTemporaneo;
			rectTemporaneo.Left		= 0;
			rectTemporaneo.Top		= 0;
			rectTemporaneo.Right	= (nCurrentConsoleWidth > newSize.X) ? newSize.X - 1 : nCurrentConsoleWidth - 1;
			rectTemporaneo.Bottom	= (nCurrentConsoleHeight > newSize.Y) ? newSize.Y - 1 : nCurrentConsoleHeight - 1;
            
			SetConsoleWindowInfo(hConsole,TRUE,&rectTemporaneo);
	}

	// imposta il buffer desiderato: se X e' maggiore della larghezza della
	// finestra, la barra di scorrimento orizzontale riappare all'istante
	SetConsoleScreenBufferSize(hConsole, newSize);
	}
}

/*
    ConsolePromptYesOrNo()

	Visualizza un prompt e aspetta la risposta dell'utente (y/n), senza visualizzare nessun messaggio.
	Termina alla pressione del tasto (y/n), senza necessitare Invio.

	Restituisce YES/NO a seconda del tasto premuto.
 */
ANSWER ConsolePromptYesOrNo(void)
{
	HWND hConsole = GetConsoleWindow();
	HANDLE hConsoleOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    HANDLE hConsoleInput = GetStdHandle(STD_INPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO csbi = {0};
    INPUT_RECORD inputBuffer[128] = {0};
    DWORD dwNumEventsRead = 0L;
    WORD wKey = 0;
    BOOL bContinue = TRUE;
    BOOL bYes = FALSE;

	/* si assicura la visibilita' */
	ShowWindow(hConsole,SW_RESTORE);

    /* svuota il buffer della tastiera, necessario per ReadConsoleInput() */
    FlushConsoleInputBuffer(hConsoleInput);

    /* posiziona il cursore */
    GetConsoleScreenBufferInfo(hConsoleOutput,&csbi);
    SetConsoleCursorPosition(hConsoleOutput,csbi.dwCursorPosition);

	/* aspetta che si verifichino eventi:
    PeekConsoleInput() legge gli eventi senza eliminarli dal buffer di input
    GetNumberOfConsoleInputEvents() controlla se si sono verificati eventi
    ReadConsoleInput() rimane in attesa che si verifichi un evento */
    while(bContinue)
    {
		if(ReadConsoleInput(	hConsoleInput,
								inputBuffer,
								128, /* NON usare sizeof(), NON e' char!!! */
								&dwNumEventsRead))
		{
			for(int i=0; i < (int)dwNumEventsRead; i++)
			{
				/* considera solo gli eventi di tasto premuto */
                if(inputBuffer[i].EventType == KEY_EVENT && inputBuffer[i].Event.KeyEvent.bKeyDown)
				{
					/* codice virtuale del tasto */
					wKey = inputBuffer[i].Event.KeyEvent.wVirtualKeyCode;

                    /* converte a maiuscolo */
                    char pressedChar = (char)toupper(inputBuffer[i].Event.KeyEvent.uChar.AsciiChar);
                    
					/* controlla il tasto premuto */
					if(pressedChar == 'Y')
					{
                        printf("y\n");
                        bYes = TRUE;
                        bContinue = FALSE;
                        break;
                    }
					else if(pressedChar == 'N')
					{
                        printf("n\n");
                        bYes = FALSE;
                        bContinue = FALSE;
                        break;
                    }
					else
					{
                        /* esclude i tasti Ctrl, X, Alt per evitare il bip
						se il cazzone dell'utente li mantiene premuti */
                        if(wKey!=VK_CONTROL && wKey!='X' && wKey!=VK_MENU)
							Beep(1000,300);
                    }
                }
            }
        }
		/* continua in loop sempre che non accadano eventi, che gli eventi non fossero tasti premuti
		o se non esce per 'break' (es. il tasto premuto non era 'y' o 'n' e suono il bip) */
    }

    return(bYes ? YES : NO);
}

/*
    ConsolePromptEnter()

	Idem come sopra, pero' senza prompt ed attesa per tasto Enter.
 */
void ConsolePromptEnter(void)
{
	HWND hConsole = GetConsoleWindow();
	HANDLE hConsoleOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    HANDLE hConsoleInput = GetStdHandle(STD_INPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO csbi = {0};
    INPUT_RECORD inputBuffer[128] = {0};
    DWORD dwNumEventsRead = 0L;
    WORD wKey = 0;
    BOOL bContinue = TRUE;
    BOOL bYes = FALSE;

	/* si assicura la visibilita' */
	ShowWindow(hConsole,SW_RESTORE);

    /* svuota il buffer della tastiera, necessario per ReadConsoleInput() */
    FlushConsoleInputBuffer(hConsoleInput);

    /* posiziona il cursore */
    GetConsoleScreenBufferInfo(hConsoleOutput,&csbi);
    SetConsoleCursorPosition(hConsoleOutput,csbi.dwCursorPosition);

	/* aspetta che si verifichino eventi:
    PeekConsoleInput() legge gli eventi senza eliminarli dal buffer di input
    GetNumberOfConsoleInputEvents() controlla se si sono verificati eventi
    ReadConsoleInput() rimane in attesa che si verifichi un evento */
    printf("-- press Enter --");
	while(bContinue)
    {
		if(ReadConsoleInput(	hConsoleInput,
								inputBuffer,
								128, /* NON usare sizeof(), NON e' char!!! */
								&dwNumEventsRead))
		{
			for(int i=0; i < (int)dwNumEventsRead; i++)
			{
				/* considera solo gli eventi di tasto premuto */
                if(inputBuffer[i].EventType == KEY_EVENT && inputBuffer[i].Event.KeyEvent.bKeyDown)
				{
					/* codice virtuale del tasto */
					wKey = inputBuffer[i].Event.KeyEvent.wVirtualKeyCode;

                    /* esce per Enter */
                    if(wKey==VK_RETURN)
					{
                        bContinue = FALSE;
                        break;
                    }
                }
            }
        }
    }
    printf("\n");
}

/*
    ClearConsoleScreen()
	
	Ripulisce lo schermo della consolle, come il CLS dell'MS-DOS.
*/
void ClearConsoleScreen(void)
{
    HANDLE hConsole = NULL;
    COORD coordScreen = {0,0}; /* angolo superiore/sinistra */
    DWORD cCharsWritten = 0L;
    CONSOLE_SCREEN_BUFFER_INFO csbi = {0};
    DWORD dwConSize = 0L;

    /* aggancia la finestra della console ed il buffer relativo per l'output */
	hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    if(!GetConsoleScreenBufferInfo(hConsole,&csbi))
        return;

    /* calcola dimensione tot del buffer della finestra */
    dwConSize = csbi.dwSize.X * csbi.dwSize.Y;

    /* riempie la finestra con spazi */
    if(!FillConsoleOutputCharacterA(hConsole,(char)' ',dwConSize,coordScreen,&cCharsWritten))
        return;

    /* riempie la finestra con attributo testo per default */
    if(!FillConsoleOutputAttribute(hConsole,csbi.wAttributes,dwConSize,coordScreen,&cCharsWritten))
        return;

    /* aggiorna la posizione del cursore */
    SetConsoleCursorPosition(hConsole,coordScreen);
}

/*
    MinimizeConsoleWindow()

	Iconizza la finestra della consolle.
*/
void MinimizeConsoleWindow(void)
{
    /* aggancia la finestra della console e minimizza */
    HWND hConsoleWnd = GetConsoleWindow();

    if(hConsoleWnd!=NULL)
        SendMessage(hConsoleWnd,WM_SYSCOMMAND,SC_MINIMIZE,0);
}

/*
    RestoreConsoleWindow()

	Ripristina la finestra della consolle.
*/
void RestoreConsoleWindow(void)
{
    /* aggancia la finestra della console e ripristina */
    HWND hConsoleWnd = GetConsoleWindow();

    if(hConsoleWnd!=NULL)
        SendMessage(hConsoleWnd,WM_SYSCOMMAND,SC_RESTORE,0);
}

/*
    CloseConsoleWindow()

	Chiude la finestra della consolle.
*/
void CloseConsoleWindow(void)
{
    DWORD consoleProcessId = 0L;
    HWND  hConsoleWnd = NULL;

    /* ricava l'id del processo della console, valido se la console e' il processo padre
    o se esiste un solo processo di consola, altrimenti usare GetConsoleProcessList() */
    GetWindowThreadProcessId(GetConsoleWindow(),&consoleProcessId);

    /* ricave l'handle della console */
    hConsoleWnd = GetConsoleWindow();

    if(hConsoleWnd!=NULL)
    {
        /* chiude la finestra della console enviando un WM_CLOSE, la soluzione piu' pulita
        se SendMessage() non fosse sufficente (coda dei msg piena, ritardi, etc.) si
        potrebbero tagliare le gambe al processo direttamente tramite la chiamata a:

        TerminateProcess(OpenProcess(PROCESS_TERMINATE, FALSE,consoleProcessId),0); */
        SendMessage(hConsoleWnd,WM_CLOSE,0,0);
		PeekAndPump();
    }
}

/*
	GetConsoleWidth()

	Calcola la larghezza della finestra della consolle.

	Restituisce la larghezza o 0 per errore.
*/
int GetConsoleWidth(void)
{
	HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
	CONSOLE_SCREEN_BUFFER_INFO csbi = {0};
	int nConsoleWidth = 0;

	if(hConsole!=INVALID_HANDLE_VALUE)
	{
		if(GetConsoleScreenBufferInfo(hConsole,&csbi))
		{
			/* csbi.dwSize.X contiene el ancho del buffer de la pantalla en columnas de caracteres */
			/* csbi.dwSize.Y contiene el alto del buffer de la pantalla en filas de caracteres */
			int consoleWidth = csbi.dwSize.X;
			int consoleHeight = csbi.dwSize.Y;

			/* csbi.srWindow.Right - csbi.srWindow.Left + 1 -> ancho de la ventana visible */
			/* csbi.srWindow.Bottom - csbi.srWindow.Top + 1 -> alto de la ventana visible */
			int visibleWidth = csbi.srWindow.Right - csbi.srWindow.Left + 1;
			int visibleHeight = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;

			nConsoleWidth = (csbi.srWindow.Right - csbi.srWindow.Left + 1) - 13;
		}
	}
	
	return(nConsoleWidth);
}

/*
	SetCustomConsoleIcon()

	Salva l'icona di sistema della finestra della console prima di sostituirla con quella del programma.
*/
void SetCustomConsoleIcon(HICON* hOldIconSmall,HICON* hOldIconBig,UINT nIconID)
{
	ASSERTEXPR(hOldIconSmall);
	ASSERTEXPR(hOldIconBig);
	ASSERTEXPR(nIconID > 0);

	HWND hwndConsole = GetConsoleWindow();
	if(hwndConsole)
	{
		HICON hNewIcon = LoadIcon(GetModuleHandle(NULL),MAKEINTRESOURCE(nIconID));
		if(hNewIcon)
		{
			/* SendMessage() restituisce l'icona precedente, quindi la salva */
			*hOldIconSmall = (HICON)SendMessage(hwndConsole,WM_SETICON,ICON_SMALL,(LPARAM)hNewIcon);	/* icona titolo */
			*hOldIconBig   = (HICON)SendMessage(hwndConsole,WM_SETICON,ICON_BIG,(LPARAM)hNewIcon);		/* icona per Alt+Tab */
		}
	}
}

/*
	RestoreConsoleIcon()

	Ripristina l'icona di sistema, eliminando quella del programma dalla finestra della console.
*/
void RestoreConsoleIcon(HICON hOldIconSmall,HICON hOldIconBig)
{
	HWND hwndConsole = GetConsoleWindow();
	if(hwndConsole)
	{
		/* ripristina le icone originali */
		if(hOldIconSmall) 
			SendMessage(hwndConsole,WM_SETICON,ICON_SMALL,(LPARAM)hOldIconSmall);
		if(hOldIconBig)   
			SendMessage(hwndConsole,WM_SETICON,ICON_BIG,(LPARAM)hOldIconBig);
	}
}

/*
	GenerateUniqueRandomSalt()
	
	Genera una stringa per il salt combinando lo unix timestampt + l'id del processo corrente + una
	serie casuale di lettere.

	Note (citazioni a proposito del concetto di "salt" lette qui' e la'):
	"Concatenate the real time system clock with a counter. Using the system clock guarantees that 
	values will be unique for different runs of program, and using the counter guarantees that values 
	will be unique within the same run of program.
	This is appropriate for server-type scenarios where the same process handles lots of passwords. 
	If your program might run many times per second, or have many instances running in parallel, 
	add the process ID too. If your process is distributed, include the server IP address or MAC.
	Note that lot of implementations use really short salts, even though keeping the salt short is 
	completely unnecessary. Don't do that, because it makes it hard to make sure they're unique."
	"The salt does not need to be random, it needs to be unique. This prevents two people with the 
	same password having their password hash to the same thing."
*/
LPSTR GenerateUniqueRandomSalt(LPSTR lpszBuffer,UINT nSize)
{
	ASSERTEXPR(lpszBuffer);
	ASSERTEXPR(nSize > 0);

	char szInternalBuffer[256] = {0};
	char szWordBuffer[128] = {0};
	memset(lpszBuffer,'\0',nSize);

	snprintf(	szInternalBuffer,
				sizeof(szInternalBuffer),
				"%lld%ld%s",
				unix_timestamp(),								/* len = 10 */
				GetProcessId(GetCurrentProcess()),				/* len = 5 */
				strwords(szWordBuffer,sizeof(szWordBuffer)-1)	/* len = 128 */
				);												/* tot = 143 */

	for(int i=0; i < (int)nSize; i++)
		lpszBuffer[i] = szInternalBuffer[i];

	return(lpszBuffer);
}

/*
	funzioni per impacchettare DWORD/QWORD, i tipi corrispondenti in C sarebbero:

	typedef unsigned long long QWORD;
	typedef unsigned long DWORD;		// su molte architetture, unsigned long e' 32 bit

	#include <stdint.h>					// alternativa piu' portabile C99/C11:
	typedef uint64_t QWORD;
	typedef uint32_t DWORD;
*/
DWORD PackTwoWORD(WORD low_word,WORD high_word)
{
    return(((DWORD)high_word << 16) | low_word);
}
WORD GetLowWORD(DWORD packed_value)
{
    return((WORD)(packed_value & 0xFFFF));
}
WORD GetHighWORD(DWORD packed_value)
{
	return((WORD)(packed_value >> 16));
}
QWORD PackTwoDWORD(DWORD low_dword,DWORD high_dword)
{
    return(((QWORD)high_dword << 32) | low_dword);
}
DWORD GetLowDWORD(QWORD packed_value)
{
    return((DWORD)(packed_value & 0xFFFFFFFF));
}
DWORD GetHighDWORD(QWORD packed_value)
{
    return((DWORD)(packed_value >> 32));
}
/*
	impacchettano/spacchettano BYTE in QWORD alla posizione specificata
	QWORD = 64 bits, BYTE = 8 bits
*/
/*uint64_t PackBYTEintoQWORD(uint64_t q,uint8_t val,int pos)*/
QWORD PackBYTEintoQWORD(QWORD q,BYTE val,int pos)
{
	/* inserisce il byte 'val' nella posizione 0-7 di 'q' e restituisce la qword aggiornata */
    int shift = pos * 8;
    /*uint64_t mask = (uint64_t)0xFF << shift;*/
    QWORD mask = (QWORD)0xFF << shift;
    /*return((q & ~mask) | ((uint64_t)val << shift));*/
    return((q & ~mask) | ((QWORD)val << shift));
}
/*uint8_t UnpackBYTEfromQWORD(uint64_t q,int pos)*/
BYTE UnpackBYTEfromQWORD(QWORD q,int pos)
{
	/* estrae il byte nella posizione 0-7 da 'q' */
    /*return((uint8_t)(q >> (pos * 8)));*/
    return((BYTE)(q >> (pos * 8)));
}
