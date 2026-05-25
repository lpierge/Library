/*$
	CWindowsVersion.cpp
	Classe per ricavare la versione di Windows.
	Luca Piergentili, 20/11/02
	lpiergentili@yahoo.com

	Esempio:

	//
	//	GetWindowsVersion()
	//
	char szWindowsPlatform[128] = {0};
	DWORD dwMajorVersion=0L;
	DWORD dwMinorVersion=0L;

	OSVERSIONTYPE ostype = GetWindowsVersion(szWindowsPlatform,sizeof(szWindowsPlatform)-1,&dwMajorVersion,&dwMinorVersion);
	printf(	"\nGetWindowsVersion()\n"\
			"OSVERSIONTYPE: %d\n"\
			"Windows platform: %s\n"\
			"OS ver major: %ld\n"\
			"OS ver minor: %ld\n"\
			"[Press Enter]\n",
			ostype,szWindowsPlatform,dwMajorVersion,dwMinorVersion);
	getchar();

	//
	//	GetWindowsVersionEx()
	//
	OSVERSIONINFOEX osvi = {0};
	GetWindowsVersionEx(&osvi);
	printf("\nGetWindowsVersionEx()\n");
	printf("dwOSVersionInfoSize %ld\n",osvi.dwOSVersionInfoSize);
	printf("dwMajorVersion      %ld\n",osvi.dwMajorVersion);
	printf("dwMinorVersion      %ld\n",osvi.dwMinorVersion);
	printf("dwBuildNumber       %ld\n",osvi.dwBuildNumber);
	printf("dwPlatformId        %ld\n",osvi.dwPlatformId);
	printf("szCSDVersion         %s\n",osvi.szCSDVersion);
	printf("wServicePackMajor    %d\n",osvi.wServicePackMajor);
	printf("wServicePackMinor    %d\n",osvi.wServicePackMinor);
	printf("wSuiteMask           %d\n",osvi.wSuiteMask);
	printf("wProductType         %d\n",osvi.wProductType);
	printf("wReserved            %d\n",osvi.wReserved);
	printf("[Press Enter]\n");
	getchar();

	//
	//	GetWindowsVersionData()
	//
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
	unixTimeStampToDate(regValuesCurrentVersion[InstallDate_INDEX].value.dword,buf,sizeof(buf)-1);

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

	//
	//	CWindowsVersion
	//
	CWindowsVersion winver;
	DWORD dwMajor=0L;
	DWORD dwMinor=0L;
	char szInfo[128] = {0};
	winver.GetPlatformInfo(szInfo,sizeof(szInfo)-1);
	printf(	"\nCWindowsVersion:\n"\
			"GetVersionNumber()=%d: major ver %ld, minor ver %ld\n"\
			"GetVersionType()=%d\n"\
			"GetVersionString()=%s\n"\
			"GetPlatformString()=%s\n"\
			"GetPlatformInfo()=%s\n"\
			"IsRunningOnCartoons()=%s\n"\
			"IsRunningOnNT()=%s\n"\
			"IsRunningOnXP()=%s\n"\
			"GetCommonControlsVer()=%ld\n"\
			"[Press Enter]\n",
			winver.GetVersionNumber(dwMajor,dwMinor),dwMajor,dwMinor,
			winver.GetVersionType(),
			winver.GetVersionString(),
			winver.GetPlatformString(),
			szInfo,
			winver.IsRunningOnCartoons() ? "yes" : "no",
			winver.IsRunningOnNT() ? "yes" : "no",
			winver.IsRunningOnXP() ? "yes" : "no",
			winver.GetCommonControlsVer()
			);
	getchar();
*/
#include "pragma.h"
#include "env.h"
#include "macro.h"
#include "strings.h"
#include "window.h"
#include "win32api.h"
#include "CRegKey.h"
//#include "CWindowsXPTheme.h"
#include "CWindowsVersion.h"

#include "traceexpr.h"
//#define _TRACE_FLAG			_TRFLAG_TRACEOUTPUT // opzioni: _TRFLAG_NOTRACE, _TRFLAG_TRACEFILE, _TRFLAG_TRACECONSOLE, _TRFLAG_TRACEOUTPUT, _TRFLAG_TRACEBREAKPOINT
#define _TRACE_FLAG			_TRFLAG_NOTRACE // opzioni: _TRFLAG_NOTRACE, _TRFLAG_TRACEFILE, _TRFLAG_TRACECONSOLE, _TRFLAG_TRACEOUTPUT, _TRFLAG_TRACEBREAKPOINT
#define _TRACE_FLAG_INFO	_TRACE_FLAG
#define _TRACE_FLAG_WARN	_TRACE_FLAG
#define _TRACE_FLAG_ERR		_TRACE_FLAG

// statiche per la classe (comuni a tutte le istanze della classe)
// il contatore per le referenze viene usato solo per sapere se inizializzare i dati
int				CWindowsVersion::m_nRefCount = 0;
DWORD			CWindowsVersion::m_dwMajorVersion = 0L;
DWORD			CWindowsVersion::m_dwMinorVersion = 0L;
OSVERSIONTYPE	CWindowsVersion::m_OsVersionType = UNKNOWN_WINDOWS_VERSION;
char			CWindowsVersion::m_szOsType[] = {0};
char			CWindowsVersion::m_szWindowsPlatform[] = {0};
BOOL			CWindowsVersion::m_bIsRunningOnCartoons = FALSE;
BOOL			CWindowsVersion::m_bIsRunningOnNT = FALSE;
DWORD			CWindowsVersion::m_dwCommonControlsDllVersion = 0L;

/*
	CWindowsVersion()
*/
CWindowsVersion::CWindowsVersion()
{
	// si suppone che il sistema operativo non cambi ogni 5 minuti...
	// il contatore per le referenze viene usato solo per sapere quando inizializzare i dati
	// (una sola inizializzazione per tutte le istanze della classe)
	if(m_nRefCount++==0)
	{
		// versione OS
		// occhio alla ::GetWindowsVersion() sta' in win32api.cpp
		switch((m_OsVersionType = ::GetWindowsVersion(m_szWindowsPlatform,sizeof(m_szWindowsPlatform),&m_dwMajorVersion,&m_dwMinorVersion)))
		{
			case WINDOWS_31:
				strcpyn(m_szOsType,"3.1",sizeof(m_szOsType));
				break;
			case WINDOWS_95:
				strcpyn(m_szOsType,"95",sizeof(m_szOsType));
				m_bIsRunningOnCartoons = TRUE;
				break;
			case WINDOWS_98:
				strcpyn(m_szOsType,"98",sizeof(m_szOsType));
				m_bIsRunningOnCartoons = TRUE;
				break;
			case WINDOWS_MILLENNIUM:
				strcpyn(m_szOsType,"ME",sizeof(m_szOsType));
				m_bIsRunningOnCartoons = TRUE;
				break;
			case WINDOWS_NT:
				_snprintf(m_szOsType,sizeof(m_szOsType)-1,"NT %ld.%ld",m_dwMajorVersion,m_dwMinorVersion);
				m_bIsRunningOnNT = TRUE;
				break;
			case WINDOWS_2000:
				_snprintf(m_szOsType,sizeof(m_szOsType)-1,"NT %ld.%ld [2000]",m_dwMajorVersion,m_dwMinorVersion);
				m_bIsRunningOnNT = TRUE;
				break;
			case WINDOWS_XP:
				_snprintf(m_szOsType,sizeof(m_szOsType)-1,"NT %ld.%ld [XP]",m_dwMajorVersion,m_dwMinorVersion);
				m_bIsRunningOnCartoons = TRUE;
				m_bIsRunningOnNT = TRUE;
				break;
			case WINDOWS_VISTA:
				_snprintf(m_szOsType,sizeof(m_szOsType)-1,"NT %ld.%ld [Vista]",m_dwMajorVersion,m_dwMinorVersion);
				m_bIsRunningOnCartoons = TRUE;
				m_bIsRunningOnNT = TRUE;
				break;
			case WINDOWS_SEVEN:
				_snprintf(m_szOsType,sizeof(m_szOsType)-1,"NT %ld.%ld [Seven]",m_dwMajorVersion,m_dwMinorVersion);
				m_bIsRunningOnCartoons = TRUE;
				m_bIsRunningOnNT = TRUE;
				break;
			default:
				strcpy(m_szOsType,"UNKNOWN_WINDOWS_VERSION");
				break;
		}

		// versione dll per i controlli
		m_dwCommonControlsDllVersion = ::GetDllVersion("comctl32.dll");
	}

	// per far caricare la DLL alla classe relativa solo quando serve
// 	m_pXPTheme = NULL;
}

/*
	~CWindowsVersion()
*/
CWindowsVersion::~CWindowsVersion()
{
// 	if(m_pXPTheme)
// 		delete m_pXPTheme,m_pXPTheme = NULL;
}

/*
	GetPlatformInfo()
*/
void CWindowsVersion::GetPlatformInfo(LPSTR pBuffer,int cbBuffer)
{
	CRegistryKey regkey;
	char key[REGKEY_MAX_KEY_NAME+1] = {0};
	BYTE value[REGKEY_MAX_KEY_VALUE+1] = {0};
	char buffer[1024] = {0};
	int nbuffer = sizeof(buffer)-1;
	int n = 0;

	n += _snprintf(buffer+n,nbuffer-1-n,"%s",m_szWindowsPlatform);
	
	// NT
	if(m_OsVersionType==WINDOWS_NT || m_OsVersionType==WINDOWS_2000 || m_OsVersionType==WINDOWS_XP || m_OsVersionType==WINDOWS_VISTA || m_OsVersionType==WINDOWS_SEVEN)
	{
		strcpyn(key,"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion",sizeof(key));
		if(regkey.Open(HKEY_CURRENT_ACCESS_LEVEL,key)==ERROR_SUCCESS)
		{
			DWORD dwSize;

			dwSize = sizeof(value);
			memset(value,0,sizeof(value));
			if(regkey.QueryValue(value,"CurrentBuildNumber",&dwSize)==ERROR_SUCCESS)
				n += _snprintf(buffer+n,nbuffer-1-n,"\nbuild: %s",value);

			dwSize = sizeof(value);
			memset(value,0,sizeof(value));
			if(regkey.QueryValue(value,"CurrentType",&dwSize)==ERROR_SUCCESS)
				n += _snprintf(buffer+n,nbuffer-1-n,"\ntype: %s",value);

			dwSize = sizeof(value);
			memset(value,0,sizeof(value));
			if(regkey.QueryValue(value,"ProductId",&dwSize)==ERROR_SUCCESS)
				n += _snprintf(buffer+n,nbuffer-1-n,"\nproduct id: %s",value);

			dwSize = sizeof(value);
			memset(value,0,sizeof(value));
			if(regkey.QueryValue(value,"RegisteredOrganization",&dwSize)==ERROR_SUCCESS)
				if(value[0]!='\0')
					n += _snprintf(buffer+n,nbuffer-1-n,"\nregistered to: %s",value);

			regkey.Close();
		}

		strcpyn(key,"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",sizeof(key));
		if(regkey.Open(HKEY_CURRENT_ACCESS_LEVEL,key)==ERROR_SUCCESS)
		{
			DWORD dwSize;

			dwSize = sizeof(value);
			memset(value,0,sizeof(value));
			if(regkey.QueryValue(value,"VendorIdentifier",&dwSize)==ERROR_SUCCESS)
				n += _snprintf(buffer+n,nbuffer-1-n,"\nMain CPU:\n%s",value);

			dwSize = sizeof(value);
			memset(value,0,sizeof(value));
			if(regkey.QueryValue(value,"Identifier",&dwSize)==ERROR_SUCCESS)
				n += _snprintf(buffer+n,nbuffer-1-n,"\n%s",value);

			DWORD dwValue = 0L;
			if(regkey.QueryValue(dwValue,"~MHz")==ERROR_SUCCESS)
				n += _snprintf(buffer+n,nbuffer-1-n,"\n%ld Mhz",dwValue);

			regkey.Close();
		}
	}
	else // Cartoons
	{
		strcpyn(key,"SOFTWARE\\Microsoft\\Windows\\CurrentVersion",sizeof(key));
		if(regkey.Open(HKEY_CURRENT_ACCESS_LEVEL,key)==ERROR_SUCCESS)
		{
			DWORD dwSize;

			dwSize = sizeof(value);
			memset(value,'\0',sizeof(value));
			if(regkey.QueryValue(value,"VersionNumber",&dwSize)==ERROR_SUCCESS)
				n += _snprintf(buffer+n,nbuffer-1-n,"\nversion number: %s",value);

			dwSize = sizeof(value);
			memset(value,'\0',sizeof(value));
			if(regkey.QueryValue(value,"ProductId",&dwSize)==ERROR_SUCCESS)
				n += _snprintf(buffer+n,nbuffer-1-n,"\nproduct id: %s",value);

			dwSize = sizeof(value);
			memset(value,'\0',sizeof(value));
			if(regkey.QueryValue(value,"RegisteredOrganization",&dwSize)==ERROR_SUCCESS)
				if(value[0]!='\0')
					n += _snprintf(buffer+n,nbuffer-1-n,"\nregistered to: %s",value);

			regkey.Close();
		}
	}

	strcpyn(pBuffer,buffer,cbBuffer);
}

/*
	GetWindowsXPTheme()
*/
/*
const CWindowsXPTheme* CWindowsVersion::GetWindowsXPTheme(void)
{
	// i temi vengono supportati solo a partire da XP
	if(m_OsVersionType >= WINDOWS_XP)
		if(!m_pXPTheme)
			m_pXPTheme = new CWindowsXPTheme();

	return(m_pXPTheme);
}
*/
