/*$
	CRegKey.h
	Classe base per l'accesso al registro (SDK/MFC).
	Riadattata dal codice originale ATL (M$) (vedi http://codeguru.earthweb.com/system/CRegKey.shtml).
	La chiave standard per registrare i dati del programma e': HKEY_CURRENT_USER\Software\[Fabbricante]\[App]
	Luca Piergentili, 14/07/99
	lpiergentili@yahoo.com
*/
#include "pragma.h"
#include "env.h"
#include "macro.h"
#include <stdlib.h>
#include "window.h"
#include <winreg.h>
#include "CRegKey.h"

#include "traceexpr.h"
//#define _TRACE_FLAG			_TRFLAG_TRACEOUTPUT // opzioni: _TRFLAG_NOTRACE, _TRFLAG_TRACEFILE, _TRFLAG_TRACECONSOLE, _TRFLAG_TRACEOUTPUT, _TRFLAG_TRACEBREAKPOINT
#define _TRACE_FLAG			_TRFLAG_NOTRACE // opzioni: _TRFLAG_NOTRACE, _TRFLAG_TRACEFILE, _TRFLAG_TRACECONSOLE, _TRFLAG_TRACEOUTPUT, _TRFLAG_TRACEBREAKPOINT
#define _TRACE_FLAG_INFO	_TRACE_FLAG
#define _TRACE_FLAG_WARN	_TRACE_FLAG
#define _TRACE_FLAG_ERR		_TRACE_FLAG

/*
	Create()
*/
LONG CRegistryKey::Create(	HKEY					hKeyParent,
							LPCSTR					lpcszKeyName,
							LPSTR					lpszClass		/*=REG_NONE*/,
							DWORD					dwOptions		/*=REG_OPTION_NON_VOLATILE*/,
							REGSAM					samDesired		/*=KEY_CREATE_SUB_KEY|KEY_WOW64_64KEY*/,
							LPSECURITY_ATTRIBUTES	lpSecAttr		/*=NULL*/,
							LPDWORD					lpdwDisposition	/*=NULL*/
							)
{
	ASSERTEXPR(hKeyParent && lpcszKeyName);
	if(!hKeyParent || !lpcszKeyName)
		return(-1L);
	
	CRegistryKey::Close();

	DWORD dw;
	HKEY hKey = NULL;
	LONG lRes = ::RegCreateKeyEx(	hKeyParent,
									(LPCSTR)lpcszKeyName,
									0L,
									(LPSTR)lpszClass,
									dwOptions,
									samDesired,
									lpSecAttr,
									&hKey,
									&dw
									);

	if(lpdwDisposition!=NULL)
		*lpdwDisposition = dw;

	if(lRes==ERROR_SUCCESS)
		m_hKey = hKey;

	return(lRes);
}

/*
	Open()
*/
LONG CRegistryKey::Open(HKEY hKeyParent,LPCSTR lpcszKeyName,REGSAM samDesired/*=KEY_ALL_ACCESS|KEY_WOW64_64KEY*/)
{
	CRegistryKey::Close();

	HKEY hKey = NULL;
	LONG lRes = ::RegOpenKeyEx(hKeyParent,(LPCSTR)lpcszKeyName,0L,samDesired,&hKey);

	if(lRes==ERROR_SUCCESS)
		m_hKey = hKey;

	return(lRes);
}

/*
	Close()
*/
LONG CRegistryKey::Close(void)
{
	LONG lRes = ERROR_SUCCESS;
	
	if(m_hKey!=NULL)
	{
		lRes = ::RegCloseKey(m_hKey);
		m_hKey = NULL;
	}

	return(lRes);
}

/*
	QueryValue()
*/
LONG CRegistryKey::QueryValue(LPBYTE lpszValue,LPCSTR lpcszValueName,DWORD* pdwCount)
{
	ASSERTEXPR(lpszValue && lpcszValueName);
	if(!lpszValue || !lpcszValueName)
		return(-1L);
	
	// le query value x stringa devono ricevere la dimensione del buffer come
	// ultimo parametro, la funzione restituira' quanto effettivamente letto
	DWORD dwType = 0L;
	return(::RegQueryValueEx(m_hKey,(LPCSTR)lpcszValueName,0L,&dwType,lpszValue,pdwCount));
}

/*
	QueryValue()
*/
LONG CRegistryKey::QueryValue(DWORD& dwValue,LPCSTR lpcszValueName)
{
	ASSERTEXPR(lpcszValueName);
	if(!lpcszValueName)
		return(-1L);
	
	DWORD dwType = 0L;
	DWORD dwCount = sizeof(DWORD);
	return(::RegQueryValueEx(m_hKey,(LPCSTR)lpcszValueName,0L,&dwType,(LPBYTE)&dwValue,&dwCount));
}

/*
	SetValue()
*/
HRESULT CRegistryKey::SetValue(LPCSTR lpcszValue,LPCSTR lpcszValueName/*=NULL*/)
{
	ASSERTEXPR(lpcszValue);
	if(!lpcszValue)
		return(-1L);

	return(::RegSetValueEx(m_hKey,(LPCSTR)lpcszValueName,NULL,REG_SZ,(CONST BYTE *)lpcszValue,(DWORD)(lstrlen(lpcszValue)+1)));
}

/*
	SetValue()
*/
LONG CRegistryKey::SetValue(DWORD dwValue,LPCSTR lpcszValueName)
{
	ASSERTEXPR(lpcszValueName);
	if(!lpcszValueName)
		return(-1L);

	return(::RegSetValueEx(m_hKey,(LPCSTR)lpcszValueName,NULL,REG_DWORD,(CONST BYTE *)&dwValue,sizeof(DWORD)));
}

/*
	SetBinaryValue()
*/
LONG CRegistryKey::SetBinaryValue(DWORD dwValue,LPCSTR lpcszValueName)
{
	ASSERTEXPR(lpcszValueName);
	if(!lpcszValueName)
		return(-1L);

	return(::RegSetValueEx(m_hKey,(LPCSTR)lpcszValueName,NULL,REG_BINARY,(CONST BYTE *)&dwValue,sizeof(DWORD)));
}

/*
	SetValue()
*/
LONG WINAPI CRegistryKey::SetValue(HKEY hKeyParent,LPCSTR lpcszKeyName,LPCSTR lpcszValue,LPCSTR lpcszValueName/* = NULL */)
{
	ASSERTEXPR(hKeyParent && lpcszKeyName && lpcszValue);
	if(!hKeyParent || !lpcszKeyName || !lpcszValue)
		return(-1L);

	CRegistryKey key;
	LONG lRes = key.Create(hKeyParent,lpcszKeyName);

	if(lRes==ERROR_SUCCESS)
		lRes = key.SetValue(lpcszValue,lpcszValueName);

	return(lRes);
}

/*
	SetKeyValue()
*/
LONG CRegistryKey::SetKeyValue(LPCSTR lpcszKeyName,LPCSTR lpcszValue,LPCSTR lpcszValueName/*=NULL*/)
{
	ASSERTEXPR(lpcszKeyName && lpcszValue);
	if(!lpcszKeyName || !lpcszValue)
		return(-1L);

	CRegistryKey key;
	LONG lRes = key.Create(m_hKey,lpcszKeyName);

	if(lRes==ERROR_SUCCESS)
		lRes = key.SetValue(lpcszValue,lpcszValueName);
	
	return(lRes);
}

/*
	DeleteValue()
*/
BOOL CRegistryKey::DeleteValue(LPCSTR lpcszValue)
{
	ASSERTEXPR(lpcszValue);
	if(!lpcszValue)
		return(FALSE);

	return(::RegDeleteValue(m_hKey,(LPCSTR)lpcszValue)==ERROR_SUCCESS);
}

/*
	DeleteKey()
*/
BOOL CRegistryKey::DeleteKey(LPCSTR lpcszKey)
{
	ASSERTEXPR(lpcszKey);
	if(!lpcszKey)
		return(FALSE);

	CRegistryKey key;
	
	if(key.Open(m_hKey,lpcszKey)!=ERROR_SUCCESS)
		return(FALSE);

	FILETIME time;
	char szBuffer[_MAX_PATH+1] = {0};
	DWORD dwSize = sizeof(szBuffer);
	
	// cerca le eventuali sotto-chiavi presenti eliminandole singolarmente perche' su NT
	// ::RegDeleteKey() non elimina la chiave se quest'ultima possiede sotto-chiavi
	while(::RegEnumKeyEx(key.m_hKey,0L,(LPSTR)szBuffer,&dwSize,NULL,NULL,NULL,&time)==ERROR_SUCCESS)
	{
		if(!key.DeleteKey(szBuffer))
			return(FALSE);
		
		dwSize = sizeof(szBuffer);
	}

	key.Close();

	return(DeleteSubKey(lpcszKey));
}

/*
	DeleteSubKey()
*/
BOOL CRegistryKey::DeleteSubKey(LPCSTR lpcszSubKey)
{
	ASSERTEXPR(lpcszSubKey);
	if(!lpcszSubKey)
		return(FALSE);

	return(::RegDeleteKey(m_hKey,(LPCSTR)lpcszSubKey)==ERROR_SUCCESS);
}

/*
	CreateRegistryKey()

	Crea la coppia nome/valore dentro la chiave specificata, es.:
	key   = "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run"
	name  = "FRC"
	value = "C:\\MWCR\\FRC.exe"
*/
BOOL CreateRegistryKey(LPCSTR lpcszKey,LPCSTR lpcszName,LPCSTR lpcszValue)
{
	ASSERTEXPR(lpcszKey && lpcszName && lpcszValue);
	if(!lpcszKey || !lpcszName || !lpcszValue)
		return(FALSE);

	CRegistryKey Registry;
	BOOL bCreated = FALSE;

	if(Registry.Open(HKEY_CURRENT_ACCESS_LEVEL,lpcszKey)==ERROR_SUCCESS)
	{
		BYTE szKey[_MAX_PATH+1] = {0};
		DWORD dwKeySize = sizeof(szKey);

		if(Registry.QueryValue(szKey,lpcszName,&dwKeySize)!=ERROR_SUCCESS)
			bCreated = Registry.SetValue(lpcszValue,lpcszName)==ERROR_SUCCESS;
		
		Registry.Close();
	}

	return(bCreated);
}

/*
	GetRegistryKey()

	Cerca la coppia nome/valore dentro la chiave specificata, es.:
	key   = "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run"
	name  = "FRC"
	value = "C:\\MWCR\\FRC.exe"
*/
BOOL GetRegistryKey(LPCSTR lpcszKey,LPCSTR lpcszName,LPSTR lpszValue,int nValueSize)
{
	ASSERTEXPR(lpcszKey && lpcszName && lpszValue && nValueSize > 0);
	if(!lpcszKey || !lpcszName || !lpszValue || nValueSize <= 0)
		return(FALSE);

	CRegistryKey Registry;
	BOOL bGet = FALSE;

	memset(lpszValue,'\0',nValueSize);

	if(Registry.Open(HKEY_CURRENT_ACCESS_LEVEL,lpcszKey)==ERROR_SUCCESS)
	{
		BYTE szKey[_MAX_PATH+1] = {0};
		DWORD dwKeySize = sizeof(szKey);

		if(Registry.QueryValue(szKey,lpcszName,&dwKeySize)==ERROR_SUCCESS)
		{
			snprintf(lpszValue,sizeof(nValueSize),"%s",szKey);
			bGet = TRUE;
		}
		
		Registry.Close();
	}

	return(bGet);
}

/*
	DeleteRegistryKey()

	Elimina la coppia nome/valore dentro la chiave specificata, es.:
	key   = "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run"
	name  = "FRC"
	value = "C:\\MWCR\\FRC.exe"
*/
BOOL DeleteRegistryKey(LPCSTR lpcszKey,LPCSTR lpcszName)
{
	ASSERTEXPR(lpcszKey && lpcszName);
	if(!lpcszKey || !lpcszName)
		return(FALSE);

	CRegistryKey Registry;
	BOOL bDeleted = FALSE;

	if(Registry.Open(HKEY_CURRENT_ACCESS_LEVEL,lpcszKey)==ERROR_SUCCESS)
	{
		BYTE szKey[_MAX_PATH+1] = {0};
		DWORD dwKeySize = sizeof(szKey);

		if(Registry.QueryValue(szKey,lpcszName,&dwKeySize)==ERROR_SUCCESS)
			bDeleted = Registry.DeleteValue(lpcszName)==ERROR_SUCCESS;

		Registry.Close();
	}

	return(bDeleted);
}
