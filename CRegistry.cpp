/*$
	CRegistry.cpp
	Classe base per l'accesso al registro (SDK/MFC).
	Luca Piergentili, 07/08/00
	lpiergentili@yahoo.com

	Vedi le note in CRegistry.h
*/
#include "pragma.h"
#include "env.h"
#include "macro.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "strings.h"
#include "window.h"
#include <shellapi.h>
#include "CRegKey.h"
#include "CRegistry.h"

// modernita' oblige (2026)
#include <shlwapi.h>
#pragma comment(lib, "shlwapi.lib")

#include "traceexpr.h"
//#define _TRACE_FLAG			_TRFLAG_TRACEOUTPUT // opzioni: _TRFLAG_NOTRACE, _TRFLAG_TRACEFILE, _TRFLAG_TRACECONSOLE, _TRFLAG_TRACEOUTPUT, _TRFLAG_TRACEBREAKPOINT
#define _TRACE_FLAG			_TRFLAG_NOTRACE // opzioni: _TRFLAG_NOTRACE, _TRFLAG_TRACEFILE, _TRFLAG_TRACECONSOLE, _TRFLAG_TRACEOUTPUT, _TRFLAG_TRACEBREAKPOINT
#define _TRACE_FLAG_INFO	_TRACE_FLAG
#define _TRACE_FLAG_WARN	_TRACE_FLAG
#define _TRACE_FLAG_ERR		_TRACE_FLAG

/*
	RegisterFileType()

	Registra il tipo file nel registro.

	Note:
	se dopo la registrazione l'icona non viene visualizzata correttamente bisogna aggiornare la cache:
	dal prompt admin eseguire:
	>ie4uinit.exe -ClearIconCache
	>taskkill /IM explorer.exe /F
	>del /A /F "%localappdata%\IconCache.db"
	>start explorer
	se e' stato usato "Apri con" -> "Usa sempre" Windows crea un UserChoice, per cancellarlo:
	reg delete HKCU\Software\Microsoft\Windows\CurrentVersion\Explorer\FileExts\<.ext>\UserChoice /f
	poi riassocia il file dal pannello "Scegli app predefinite" o via codice
*/
BOOL CRegistry::RegisterFileType(LPREGISTERFILETYPE lpRegFileType)
{
	ASSERTEXPR(lpRegFileType);

	char key[REGKEY_MAX_KEY_NAME+1] = {0};
	char value[REGKEY_MAX_KEY_VALUE+1] = {0};
	CRegistryKey regkey;
	LONG reg;
	BOOL flag = TRUE;
	memset(key,'\0',sizeof(key));

	// crea la chiave relativa all'estensione
	// es. HKEY_LOCAL_MACHINE\SOFTWARE\Classes\.gzw
	snprintf(key,sizeof(key),"SOFTWARE\\Classes\\%s",lpRegFileType->extension);
	if((reg = regkey.Open(HKEY_CURRENT_ACCESS_LEVEL,key))!=ERROR_SUCCESS)
		reg = regkey.Create(HKEY_CURRENT_ACCESS_LEVEL,key);

	if(reg==ERROR_SUCCESS)
	{
		// imposta il valore di default con il nome relativo
		// es. HKEY_LOCAL_MACHINE\SOFTWARE\Classes\.gzw -> (Default) = gzwfile
		regkey.SetValue(lpRegFileType->name,"");

		// imposta il tipo mime
		// es. HKEY_LOCAL_MACHINE\SOFTWARE\Classes\.gzw -> Content Type = application/x-gzw-compressed
		regkey.SetValue(lpRegFileType->contenttype,"Content Type");

		regkey.Close();
	}
	else
		flag = FALSE;

	// crea la chiave per il nome relativo all'estensione
	// es. HKEY_LOCAL_MACHINE\SOFTWARE\Classes\gzwfile
	snprintf(key,sizeof(key),"SOFTWARE\\Classes\\%s",lpRegFileType->name);
	if((reg = regkey.Open(HKEY_CURRENT_ACCESS_LEVEL,key))!=ERROR_SUCCESS)
		reg = regkey.Create(HKEY_CURRENT_ACCESS_LEVEL,key);
		
	if(reg==ERROR_SUCCESS)
	{
		// imposta il valore di default con la descrizione del tipo
		// es. HKEY_LOCAL_MACHINE\SOFTWARE\Classes\extfile -> (Default) = GZW compressed data
		regkey.SetValue(lpRegFileType->description,"");
		regkey.SetBinaryValue(0x00000000,"EditFlags");

		regkey.Close();

		// crea la chiave per l'icona di default
		// es. HKEY_LOCAL_MACHINE\SOFTWARE\Classes\extfile\DefaultIcon
		snprintf(key,sizeof(key),"SOFTWARE\\Classes\\%s\\DefaultIcon",lpRegFileType->name);
		if((reg = regkey.Open(HKEY_CURRENT_ACCESS_LEVEL,key))!=ERROR_SUCCESS)
			reg = regkey.Create(HKEY_CURRENT_ACCESS_LEVEL,key);
		if(reg==ERROR_SUCCESS)
		{
			// es. HKEY_LOCAL_MACHINE\SOFTWARE\Classes\extfile\DefaultIcon -> c:\bin\gzwshell.exe,0
			snprintf(value,sizeof(key),"%s,%d",lpRegFileType->shell,lpRegFileType->defaulticon);
			regkey.SetValue(value,"");

			regkey.Close();
		}
		else
			flag = FALSE;

		// crea la chiave per l'apertura del file tramite la shell
		// es. HKEY_LOCAL_MACHINE\SOFTWARE\Classes\extfile\shell\open\command
		snprintf(key,sizeof(key),"SOFTWARE\\Classes\\%s\\shell\\open",lpRegFileType->name);
		if((reg = regkey.Open(HKEY_CURRENT_ACCESS_LEVEL,key))!=ERROR_SUCCESS)
			reg = regkey.Create(HKEY_CURRENT_ACCESS_LEVEL,key);
		if(reg==ERROR_SUCCESS)
		{
			regkey.SetBinaryValue(0x00000001,"EditFlags");
			regkey.Close();
		}
		else
			flag = FALSE;
		
		snprintf(key,sizeof(key),"SOFTWARE\\Classes\\%s\\shell\\open\\command",lpRegFileType->name);
		if((reg = regkey.Open(HKEY_CURRENT_ACCESS_LEVEL,key))!=ERROR_SUCCESS)
			reg = regkey.Create(HKEY_CURRENT_ACCESS_LEVEL,key);
		if(reg==ERROR_SUCCESS)
		{
			// es. HKEY_LOCAL_MACHINE\SOFTWARE\Classes\extfile\shell\open\command -> c:\bin\gzwshell.exe
			snprintf(value,sizeof(key),"%s %s",lpRegFileType->shell,lpRegFileType->shellopenargs);
			regkey.SetValue(value,"");
			regkey.Close();
		}
		else
			flag = FALSE;
	}
	else
		flag = FALSE;

	return(flag);
}

/*
	UnregisterFileType()

	Elimina la registrazione per il tipo dal registro (includere il punto nell'estensione).
*/
BOOL CRegistry::UnregisterFileType(LPCSTR lpcszExtension)
{
	ASSERTEXPR(lpcszExtension);
	if(!lpcszExtension)
		return(FALSE);

	char key[REGKEY_MAX_KEY_NAME+1] = {0};
	BYTE value[REGKEY_MAX_KEY_VALUE+1] = {0};
	DWORD valuesize;
	CRegistryKey regkey;
	BOOL flag = FALSE;

	// cerca la chiave relativa all'estensione
	// es. HKEY_LOCAL_MACHINE\SOFTWARE\Classes\.gzw
	snprintf(key,sizeof(key),"SOFTWARE\\Classes\\%s",lpcszExtension);
	if(regkey.Open(HKEY_CURRENT_ACCESS_LEVEL,key)==ERROR_SUCCESS)
	{
		// ricava il nome relativo
		// es. HKEY_LOCAL_MACHINE\SOFTWARE\Classes\.gzw -> (Default) = gzwfile
		memset(value,'\0',sizeof(value));
		valuesize = sizeof(value);
		if(regkey.QueryValue(value,(LPCSTR)NULL,&valuesize)==ERROR_SUCCESS)
		{
			regkey.Close();

			flag = TRUE;

			// elimina la chiave relativa all'estensione
			if(regkey.Open(HKEY_CURRENT_ACCESS_LEVEL,"SOFTWARE\\Classes")==ERROR_SUCCESS)
			{
				if(flag)
					flag = regkey.DeleteKey(lpcszExtension);
				regkey.Close();
			}

			// elimina la chiave relativa al nome
			if(regkey.Open(HKEY_CURRENT_ACCESS_LEVEL,"SOFTWARE\\Classes")==ERROR_SUCCESS)
			{
				if(flag)
					flag = regkey.DeleteKey((LPCSTR)value);
				regkey.Close();
			}
		}
	}
	
	return(flag);
}

/*
	GetRegisteredFileType()

	Ricava le informazioni relative al tipo (includere il punto nell'estensione).
*/
BOOL CRegistry::GetRegisteredFileType(LPCSTR lpcszExtension,LPREGISTERFILETYPE pFileType,BOOL bExtractIcon/* = FALSE*/)
{
	ASSERTEXPR(lpcszExtension && pFileType);
	if(!lpcszExtension || !pFileType)
		return(FALSE);

	char program[_MAX_PATH+1] = {0};
	char index[5] = {0};
	int nIconIndex = 0;
	char key[REGKEY_MAX_KEY_NAME+1] = {0};
	BYTE value[REGKEY_MAX_KEY_VALUE+1] = {0};
	DWORD valuesize;
	CRegistryKey regkey;

	memset(pFileType,'\0',sizeof(REGISTERFILETYPE));
	strcpyn(pFileType->extension,lpcszExtension,sizeof(pFileType->extension));

	snprintf(key,sizeof(key),"SOFTWARE\\Classes\\%s",lpcszExtension);

	if(regkey.Open(HKEY_CURRENT_ACCESS_LEVEL,key)==ERROR_SUCCESS)
	{
		memset(value,'\0',sizeof(value));
		valuesize = sizeof(value);

		if(regkey.QueryValue(value,"",&valuesize)==ERROR_SUCCESS)
		{
			strcpyn(pFileType->name,(const char*)value,sizeof(pFileType->name));
			
			BYTE content[REGKEY_MAX_KEY_VALUE+1];
			DWORD contentsize;
			memset(content,'\0',sizeof(content));
			contentsize = sizeof(content);
			if(regkey.QueryValue(content,"Content Type",&contentsize)==ERROR_SUCCESS)
				strcpyn(pFileType->contenttype,(const char *)content,sizeof(pFileType->contenttype));

			snprintf(key,sizeof(key),"SOFTWARE\\Classes\\%s",pFileType->name);
			if(regkey.Open(HKEY_CURRENT_ACCESS_LEVEL,key)==ERROR_SUCCESS)
			{
				memset(content,'\0',sizeof(content));
				contentsize = sizeof(content);
				if(regkey.QueryValue(content,"",&contentsize)==ERROR_SUCCESS)
					strcpyn(pFileType->description,(const char *)content,sizeof(pFileType->description));
			}
			
			snprintf(key,sizeof(key),"SOFTWARE\\Classes\\%s\\shell\\open\\command",pFileType->name);
			if(regkey.Open(HKEY_CURRENT_ACCESS_LEVEL,key)==ERROR_SUCCESS)
			{
				memset(content,'\0',sizeof(content));
				contentsize = sizeof(content);
				if(regkey.QueryValue(content,"",&contentsize)==ERROR_SUCCESS)
					strcpyn(pFileType->shell,(const char *)content,sizeof(pFileType->shell));
			}

			// cerca l'entrata "DefaultIcon"
			memset(program,'\0',sizeof(program));
			snprintf(key,sizeof(key),"SOFTWARE\\Classes\\%s\\DefaultIcon",value);
			if(regkey.Open(HKEY_CURRENT_ACCESS_LEVEL,key)==ERROR_SUCCESS)
			{
				memset(value,'\0',sizeof(value));
				valuesize = sizeof(value);
				
				if(regkey.QueryValue(value,"",&valuesize)==ERROR_SUCCESS)
				{
					strlrw((char *)value);
					int i;
					char* p = (char *)value;
					for(i = 0; i < sizeof(program)-1 && *p!=','; i++)
					{
						if(*p!=',')
							program[i] = *p;
						
						p++;
					}
					program[i] = '\0';
					while(*p==',' || isspace((unsigned char)*p))
						p++;
					for(i = 0; i < sizeof(index)-1 && *p; i++)
						index[i] = *p++;
					index[i] = '\0';
					nIconIndex = atoi(index);
				}
			}
			// entrata "DefaultIcon" non trovata, sicuramente il tipo specifica il nome della classe
			// relativo al CLSID (es. CorelPhotoPaint.Image.6), quindi, dato che non so come cazzo
			// tirare fuori l'icona associata via OLE, cerca il programma associato all'estensione
			// e recupera la prima icona (quella con indice 0)
			//
			// (mitico commento dell'anno 2000, riletto oggi nel 2025... :)
			else
			{
				GetProgramForRegisteredFileType(lpcszExtension,program,sizeof(program));
			}

			if(program[0]!='\0' && bExtractIcon && pFileType)
			{	
				pFileType->hicon = ExtractIcon(m_hInstance,program,nIconIndex);
			}
		}
		
		regkey.Close();
	}
	
	return(TRUE);
}

/*
	SetIconForRegisteredFileType()

	Imposta l'icona di default per il tipo file.
*/
BOOL CRegistry::SetIconForRegisteredFileType(LPREGISTERFILETYPE lpRegFileType)
{
	ASSERTEXPR(lpRegFileType);
	if(!lpRegFileType)
		return(FALSE);

	char key[REGKEY_MAX_KEY_NAME+1] = {0};
	char value[REGKEY_MAX_KEY_VALUE+1] = {0};
	CRegistryKey regkey;
	LONG reg;
	BOOL flag = TRUE;

	// crea la chiave relativa all'estensione
	// es. HKEY_LOCAL_MACHINE\SOFTWARE\Classes\.gzw
	snprintf(key,sizeof(key),"SOFTWARE\\Classes\\%s",lpRegFileType->extension);
	if((reg = regkey.Open(HKEY_CURRENT_ACCESS_LEVEL,key))!=ERROR_SUCCESS)
		reg = regkey.Create(HKEY_CURRENT_ACCESS_LEVEL,key);

	if(reg==ERROR_SUCCESS)
	{
		// imposta il valore di default con il nome relativo
		// es. HKEY_LOCAL_MACHINE\SOFTWARE\Classes\.gzw -> (Default) = gzwfile
		regkey.SetValue(lpRegFileType->name,"");

		regkey.Close();
	}
	else
		flag = FALSE;

	// crea la chiave per il nome relativo all'estensione
	// es. HKEY_LOCAL_MACHINE\SOFTWARE\Classes\gzwfile
	snprintf(key,sizeof(key),"SOFTWARE\\Classes\\%s",lpRegFileType->name);
	if((reg = regkey.Open(HKEY_CURRENT_ACCESS_LEVEL,key))!=ERROR_SUCCESS)
		reg = regkey.Create(HKEY_CURRENT_ACCESS_LEVEL,key);
		
	if(reg==ERROR_SUCCESS)
	{
		// imposta il valore di default con la descrizione del tipo
		// es. HKEY_LOCAL_MACHINE\SOFTWARE\Classes\extfile -> (Default) = GZW compressed data
		regkey.SetValue(lpRegFileType->description,"");
		regkey.SetBinaryValue(0x00000000,"EditFlags");

		regkey.Close();

		// crea la chiave per l'icona di default
		// es. HKEY_LOCAL_MACHINE\SOFTWARE\Classes\extfile\DefaultIcon
		snprintf(key,sizeof(key),"SOFTWARE\\Classes\\%s\\DefaultIcon",lpRegFileType->name);
		if((reg = regkey.Open(HKEY_CURRENT_ACCESS_LEVEL,key))!=ERROR_SUCCESS)
			reg = regkey.Create(HKEY_CURRENT_ACCESS_LEVEL,key);
		if(reg==ERROR_SUCCESS)
		{
			// es. HKEY_LOCAL_MACHINE\SOFTWARE\Classes\extfile\DefaultIcon -> c:\bin\gzwshell.exe,0
			snprintf(value,sizeof(value),"%s,%d",lpRegFileType->shell,lpRegFileType->defaulticon);
			regkey.SetValue(value,"");

			regkey.Close();
		}
		else
			flag = FALSE;
	}
	else
		flag = FALSE;

	return(flag);
}

/*
	GetIconForRegisteredFileType()

	Ricava l'handle relativo all'icona per il tipo file (da chiudere poi con DestroyIcon()).
*/
HICON CRegistry::GetIconForRegisteredFileType(LPCSTR lpcszExtension,LPREGISTERFILETYPE pFileType/* = NULL*/,UINT nID/* = 0*/)
{
	ASSERTEXPR(lpcszExtension);
	if(!lpcszExtension)
		return(NULL);

	HICON hIcon = (HICON)NULL;
	REGISTERFILETYPE registerfiletype = {0};
	
	if(nID==0)
	{
		hIcon = GetRegisteredFileType(lpcszExtension,pFileType ? pFileType : &registerfiletype,TRUE) ? (pFileType ? pFileType->hicon : registerfiletype.hicon) : NULL;
	}
	else
	{
		GetRegisteredFileType(lpcszExtension,pFileType ? pFileType : &registerfiletype,FALSE);
#if defined(_AFX) || defined(_AFX_DLL)
		hIcon = ::LoadIcon(AfxGetInstanceHandle(),MAKEINTRESOURCE(nID));
#else
		hIcon = NULL;
#endif
		if(pFileType)
			pFileType->hicon = hIcon;
	}
	
	return(hIcon);
}

/*
	GetSafeIconForRegisteredFileType()

	Ricava l'handle relativo all'icona per il tipo file registrato, restituendo l'icona di sistema
	se non trova quella richiesta (includere il punto nell'estensione).
*/
HICON CRegistry::GetSafeIconForRegisteredFileType(LPCSTR lpcszExtension,LPREGISTERFILETYPE pFileType/* = NULL*/,UINT nID/* = 0*/)
{
	ASSERTEXPR(lpcszExtension);
	if(!lpcszExtension)
		return(NULL);

	HICON hIcon;

	if((hIcon = GetIconForRegisteredFileType(lpcszExtension,pFileType,nID))==(HICON)NULL)
		hIcon = ::LoadIcon(NULL,IDI_WINLOGO);

	return(hIcon);
}

/*
	GetSystemIcon()

	Ricava l'handle relativo all'icona di sistema.
*/
HICON CRegistry::GetSystemIcon(IDI_PREDEFINED_ICON id)
{
	HICON hIcon;

	switch(id)
	{
		case IDI_APPLICATION_ICON:
			hIcon = ::LoadIcon(NULL,IDI_APPLICATION);
			break;
		case IDI_ASTERISK_ICON:
			hIcon = ::LoadIcon(NULL,IDI_ASTERISK);
			break;
		case IDI_EXCLAMATION_ICON:
			hIcon = ::LoadIcon(NULL,IDI_EXCLAMATION);
			break;
		case IDI_HAND_ICON:
			hIcon = ::LoadIcon(NULL,IDI_HAND);
			break;
		case IDI_QUESTION_ICON:
			hIcon = ::LoadIcon(NULL,IDI_QUESTION);
			break;
		case IDI_WINLOGO_ICON:
		default:
			hIcon = ::LoadIcon(NULL,IDI_WINLOGO);
			break;
	}

	return(hIcon);
}

#if 0
SHFILEINFO GetTypeIcon(const char* szType)
{
	SHFILEINFO shIcon;
	memset(&shIcon, '\0', sizeof(shIcon));
	SHGetFileInfo(szType, FILE_ATTRIBUTE_NORMAL, &shIcon, sizeof(shIcon), SHGFI_USEFILEATTRIBUTES | SHGFI_DISPLAYNAME | SHGFI_TYPENAME | SHGFI_ICON);
	return shIcon;
}
#endif

/*
	GetProgramForRegisteredFileType()

	Ricava l'applicazione associata al tipo file registrato.
	Il nome del programma viene restituito eliminando gli eventuali parametri/opzioni presenti nel registro.

	LPCSTR lpcszFileName	nome del file (estensione inclusa o solo estensione) per il tipo
	LPSTR lpszProgram		buffer dove copiare il nome dell'eseguibile associato
	int nSize				dimensione del buffer
*/
BOOL CRegistry::GetProgramForRegisteredFileType(LPCSTR lpcszFileName,LPSTR lpszProgram,int nSize)
{
	ASSERTEXPR(lpcszFileName && lpszProgram && nSize > 0);
	if(!lpcszFileName || !lpszProgram || nSize <= 0)
		return(FALSE);

	char ext[_MAX_EXT+1] = {0};
	char key[REGKEY_MAX_KEY_NAME+1] = {0};
	BYTE value[REGKEY_MAX_KEY_VALUE+1] = {0};
	DWORD valuesize;
	CRegistryKey regkey;
		
	// ricava l'estensione del file
	int len = (int)strlen(lpcszFileName)-1;
	int i=0;
	for(; i < sizeof(ext)+2 && lpcszFileName[len-i]!='.'; i++) // sizeof-2 per '.' e '\0'
		ext[i] = lpcszFileName[len-i];
	ext[i++] = '.';
	ext[i] = '\0';
	strrev(ext);

	// programma associato al tipo file
	memset(lpszProgram,'\0',nSize);

	// ricava l'associazione dal registro
	snprintf(key,sizeof(key),"SOFTWARE\\Classes\\%s",ext);

	if(regkey.Open(HKEY_CURRENT_ACCESS_LEVEL,key)==ERROR_SUCCESS)
	{
		memset(value,'\0',sizeof(value));
		valuesize = sizeof(value);

		if(regkey.QueryValue(value,"",&valuesize)==ERROR_SUCCESS)
		{
			snprintf(key,sizeof(key),"SOFTWARE\\Classes\\%s\\shell\\open\\command",value);
			if(regkey.Open(HKEY_CURRENT_ACCESS_LEVEL,key)==ERROR_SUCCESS)
			{
				memset(value,'\0',sizeof(value));
				valuesize = sizeof(value);
				
				if(regkey.QueryValue(value,"",&valuesize)==ERROR_SUCCESS)
				{
					strlrw((char*)value);

					char* p = (char*)value;
					while(*p)
					{
						if(*p=='"')
							*p = ' ';
						p++;
					}

					p = (char*)value;
					while(isspace((unsigned char)*p))
						p++;
					for(i = 0; i < nSize+1; i++)
					{
						if(isspace((unsigned char)*p))
							if(stristr(lpszProgram,".exe"))
								break;

						lpszProgram[i] = *p++;
					}
					
					lpszProgram[i] = '\0';
				}
			}
		}
		
		regkey.Close();
	}
	
	return(lpszProgram[0]!='\0');
}

/*
	ExecuteFileType()

	Esegue l'applicazione associata al tipo file registrato.
	Ricava l'eseguibile relativo al tipo file e lo esegue passandogli come unico parametro
	il nome del file, ignorando gli eventuali parametri/opzioni presenti nel registro.

	LPCSTR lpcszFileName	nome del file (estensione inclusa) per il tipo
*/
BOOL CRegistry::ExecuteFileType(LPCSTR lpcszFileName)
{
	ASSERTEXPR(lpcszFileName);
	if(!lpcszFileName)
		return(FALSE);

	BOOL flag = FALSE;
	char program[_MAX_PATH+1] = {0};

	if(GetProgramForRegisteredFileType(lpcszFileName,program,sizeof(program)))
	{
		char cmd[(_MAX_PATH*2)+1] = {0};
		snprintf(cmd,sizeof(cmd),"%s %s",program,lpcszFileName);
		STARTUPINFO si = {0};
		si.cb = sizeof(STARTUPINFO);
		PROCESS_INFORMATION pi = {0};

		if(::CreateProcess(NULL,cmd,NULL,NULL,FALSE,0L,NULL,NULL,&si,&pi))
		{
			flag = TRUE;
			::CloseHandle(pi.hProcess);
		}
	}

	return(flag);
}

/*
	GetCommandForRegisteredFileType()

	Ricava l'applicazione associata al tipo file/comando registrati.
	Il nome del programma viene restituito includendo gli eventuali parametri/opzioni presenti nel registro.

	LPCSTR lpcszCommand		comando registrato ("open", "print", etc.)
	LPCSTR lpcszFileName	nome del file (estensione inclusa o solo estensione) per il tipo
	LPSTR lpszProgram		buffer dove copiare il nome dell'eseguibile associato
	int nSize				dimensione del buffer
*/
BOOL CRegistry::GetCommandForRegisteredFileType(LPCSTR lpcszCommand,LPCSTR lpcszFileName,LPSTR lpszProgram,int nSize)
{
	ASSERTEXPR(lpcszCommand && lpcszFileName && lpszProgram && nSize > 0);
	if(!lpcszCommand || !lpcszFileName || !lpszProgram || nSize <= 0)
		return(FALSE);

	char ext[_MAX_EXT+1] = {0};
	char key[REGKEY_MAX_KEY_NAME+1] = {0};
	BYTE value[REGKEY_MAX_KEY_VALUE+1] = {0};
	DWORD valuesize;
	CRegistryKey regkey;
		
	// ricava l'estensione del file
	int len = (int)strlen(lpcszFileName)-1;
	int i = 0;
	for(; i < sizeof(ext)+2 && lpcszFileName[len-i]!='.'; i++) // sizeof-2 per '.' e '\0'
		ext[i] = lpcszFileName[len-i];
	ext[i++] = '.';
	ext[i] = '\0';
	strrev(ext);

	// programma associato al tipo file
	memset(lpszProgram,'\0',nSize);

	// ricava l'associazione dal registro
	snprintf(key,sizeof(key),"SOFTWARE\\Classes\\%s",ext);

	if(regkey.Open(HKEY_CURRENT_ACCESS_LEVEL,key)==ERROR_SUCCESS)
	{
		memset(value,'\0',sizeof(value));
		valuesize = sizeof(value);

		if(regkey.QueryValue(value,"",&valuesize)==ERROR_SUCCESS)
		{
			snprintf(key,sizeof(key),"SOFTWARE\\Classes\\%s\\shell\\%s\\command",value,lpcszCommand);
			if(regkey.Open(HKEY_CURRENT_ACCESS_LEVEL,key)==ERROR_SUCCESS)
			{
				memset(value,'\0',sizeof(value));
				valuesize = sizeof(value);
				
				if(regkey.QueryValue(value,"",&valuesize)==ERROR_SUCCESS)
					strcpyn(lpszProgram,(const char *)value,nSize);
			}
		}
		
		regkey.Close();
	}
	
	return(lpszProgram[0]!='\0');
}

/*
	ShellFileType()

	Esegue l'applicazione associata al tipo file/comando registrati.
	Ricava l'eseguibile relativo al tipo file e lo esegue passandogli come parametro (cerca il '%1')
	il nome del file, includendo gli eventuali parametri/opzioni presenti nel registro.

	LPCSTR lpcszCommand		comando registrato ("open", "print", etc.)
	LPCSTR lpcszFileName	nome del file (estensione inclusa) per il tipo
*/
BOOL CRegistry::ShellFileType(LPCSTR lpcszCommand,LPCSTR lpcszFileName)
{
	ASSERTEXPR(lpcszCommand && lpcszFileName);
	if(!lpcszCommand || !lpcszFileName)
		return(FALSE);

	BOOL flag = FALSE;
	char program[_MAX_PATH+1] = {0};
	char filename[_MAX_PATH+1] = {0};

	strcpyn(filename,lpcszFileName,sizeof(filename));
	if(memcmp(filename,"http://",7)==0 && !stristr(filename,".htm"))
		strcpy(filename,".htm");

	if(GetCommandForRegisteredFileType(lpcszCommand,filename,program,sizeof(program)))
	{
		char cmd[(_MAX_PATH*2)+1] = {0};
		char* ext = strrchr(filename,'.');
		if(!ext)
			ext = (char*)"";

		if(stricmp(ext,".exe")!=0 && stricmp(ext,".com")!=0)
		{
			char* p = strstr(program,"%1");

			// per media player
			if(!p)
				p = strstr(program,"%L");

			if(p)
			{
				int i = (int)(p-program);
				strcpyn(cmd,program,sizeof(cmd));
				cmd[i] = '\0';
				strcatn(cmd,lpcszFileName,sizeof(cmd));
				
				p = strstr(program,"%1");
				// per media player
				if(!p)
					p = strstr(program,"%L");

				if(*(p+2))
					//strcat(cmd+i+strlen(lpcszFileName),p+2);
					strcatn(cmd+i+strlen(lpcszFileName),p+2,sizeof(cmd));
			}
			else
				snprintf(cmd,sizeof(cmd),"%s \"%s\"",program,lpcszFileName);

			while((p = strchr(cmd,'%'))!=NULL)
			{
				char buffer[(_MAX_PATH*2)+1] = {0};
				char var[_MAX_PATH+1] = {0};
				char value[_MAX_PATH+1] = {0};
				int i = (int)(p-cmd+1);
				int n = 0;
				for(; n < sizeof(var)-2 && cmd[i]; n++,i++)
				{
					if(cmd[i]=='%')
						break;
					var[n] = cmd[i];
				}
				var[n] = '\0';

				if((p = getenv(var))!=NULL)
				{
					strcpyn(value,p,sizeof(value));
				
				strcpyn(buffer,cmd,sizeof(buffer));
				p = strchr(buffer,'%');
				strcpyn(p,value,(int)(sizeof(buffer)-(p-buffer)));
				p = strchr(cmd,'%');
				//strcat(buffer,p+strlen(var)+2);
				strcatn(buffer,p+strlen(var)+2,sizeof(buffer));

				strcpyn(cmd,buffer,sizeof(cmd));
				}
				else
					break;
			}
		}
		else
			strcpyn(cmd,filename,sizeof(cmd));

		STARTUPINFO si = {0};
		si.cb = sizeof(STARTUPINFO);
		PROCESS_INFORMATION pi = {0};

		if(::CreateProcess(NULL,cmd,NULL,NULL,FALSE,0L,NULL,NULL,&si,&pi))
		{
			flag = TRUE;
			::CloseHandle(pi.hProcess);
		}
	}

	return(flag);
}

/*
	GetContentTypeExtension()

	Ricava l'estensione relativa al tipo mime.
*/
LPSTR CRegistry::GetContentTypeExtension(LPCSTR lpcszContentType,LPSTR lpszExt,UINT nExtSize)
{
	ASSERTEXPR(lpcszContentType && lpszExt && nExtSize > 0);
	if(!lpcszContentType || !lpszExt || nExtSize <= 0)
		return(FALSE);

	char* p = NULL;
	char key[REGKEY_MAX_KEY_NAME+1] = {0};
	BYTE value[REGKEY_MAX_KEY_VALUE+1] = {0};
	DWORD valuesize;
	CRegistryKey regkey;
		
	memset(lpszExt,'\0',nExtSize);

	snprintf(key,sizeof(key),"SOFTWARE\\Classes\\MIME\\Database\\Content Type\\%s",lpcszContentType);

	if(regkey.Open(HKEY_CURRENT_ACCESS_LEVEL,key)==ERROR_SUCCESS)
	{
		memset(value,'\0',sizeof(value));
		valuesize = sizeof(value);

		if(regkey.QueryValue(value,"Extension",&valuesize)==ERROR_SUCCESS)
		{
			strcpyn(lpszExt,(const char *)value,nExtSize);
			p = lpszExt;
		}

		regkey.Close();
	}
	
	return(p);
}

/*
	AddMenuEntryForRegisteredFileType()

	Aggiunge l'entrata al menu contestuale della shell per il tipo file registrato.

	LPCSTR lpcszExtension	estensione del tipo (punto incluso)
	LPCSTR lpcszMenuText	testo per il menu contestuale della shell
	LPCSTR lpcszCommand		comando da associare all'entrata del menu (specificare gli
						eventuali parametri, ad es. '%1', ed opzioni
*/
BOOL CRegistry::AddMenuEntryForRegisteredFileType(LPCSTR lpcszExtension,LPCSTR lpcszMenuText,LPCSTR lpcszCommand)
{
	ASSERTEXPR(lpcszExtension && lpcszMenuText && lpcszCommand);
	if(!lpcszExtension || !lpcszMenuText || !lpcszCommand)
		return(FALSE);

	char key[REGKEY_MAX_KEY_NAME+1] = {0};
	BYTE value[REGKEY_MAX_KEY_VALUE+1] = {0};
	CRegistryKey regkey;
	LONG reg;
	BOOL flag = TRUE;

	// cerca la chiave relativa all'estensione
	// es. HKEY_LOCAL_MACHINE\SOFTWARE\Classes\.gzw
	snprintf(key,sizeof(key),"SOFTWARE\\Classes\\%s",lpcszExtension);
	if(regkey.Open(HKEY_CURRENT_ACCESS_LEVEL,key)==ERROR_SUCCESS)
	{
		// ricava il nome relativo
		// es. HKEY_LOCAL_MACHINE\SOFTWARE\Classes\.gzw -> (Default) = gzwfile
		memset(value,'\0',sizeof(value));
		DWORD valuesize = sizeof(value);
		if(regkey.QueryValue(value,"",&valuesize)==ERROR_SUCCESS)
		{
			regkey.Close();

			// apre la chiave per il nome relativo all'estensione
			// es. HKEY_LOCAL_MACHINE\SOFTWARE\Classes\gzwfile
			snprintf(key,sizeof(key),"SOFTWARE\\Classes\\%s",value);
			if((reg = regkey.Open(HKEY_CURRENT_ACCESS_LEVEL,key))==ERROR_SUCCESS)
			{
				// crea la chiave per l'apertura del file tramite la shell
				// es. HKEY_LOCAL_MACHINE\SOFTWARE\Classes\extfile\shell\open\command
				snprintf(key,sizeof(key),"SOFTWARE\\Classes\\%s\\shell\\%s\\command",value,lpcszMenuText);
				if((reg = regkey.Open(HKEY_CURRENT_ACCESS_LEVEL,key))!=ERROR_SUCCESS)
					reg = regkey.Create(HKEY_CURRENT_ACCESS_LEVEL,key);
				if(reg==ERROR_SUCCESS)
				{
					// es. HKEY_LOCAL_MACHINE\SOFTWARE\Classes\extfile\shell\open\command -> c:\bin\gzwshell.exe
					snprintf((char*)value,sizeof(key),"%s",lpcszCommand);
					regkey.SetValue((LPCSTR)value,"");
					regkey.Close();
				}
				else
					flag = FALSE;
			}
			else
				flag = FALSE;
		}
	}

	return(flag);
}

/*
	RemoveMenuEntryForRegisteredFileType()

	Elimina l'entrata dal menu contestuale della shell per il tipo file registrato.

	LPCSTR lpcszExtension	estensione del tipo (punto incluso)
	LPCSTR lpcszMenuText	testo per il menu contestuale della shell
*/
BOOL CRegistry::RemoveMenuEntryForRegisteredFileType(LPCSTR lpcszExtension,LPCSTR lpcszMenuText)
{
	ASSERTEXPR(lpcszExtension && lpcszMenuText);
	if(!lpcszExtension || !lpcszMenuText)
		return(FALSE);

	char key[REGKEY_MAX_KEY_NAME+1] = {0};
	BYTE value[REGKEY_MAX_KEY_VALUE+1] = {0};
	CRegistryKey regkey;
	LONG reg;
	BOOL flag = TRUE;

	// cerca la chiave relativa all'estensione
	// es. HKEY_LOCAL_MACHINE\SOFTWARE\Classes\.gzw
	snprintf(key,sizeof(key),"SOFTWARE\\Classes\\%s",lpcszExtension);
	if(regkey.Open(HKEY_CURRENT_ACCESS_LEVEL,key)==ERROR_SUCCESS)
	{
		// ricava il nome relativo
		// es. HKEY_LOCAL_MACHINE\SOFTWARE\Classes\.gzw -> (Default) = gzwfile
		memset(value,'\0',sizeof(value));
		DWORD valuesize = sizeof(value);
		if(regkey.QueryValue(value,"",&valuesize)==ERROR_SUCCESS)
		{
			regkey.Close();

			// apre la chiave per il nome relativo all'estensione
			// es. HKEY_LOCAL_MACHINE\SOFTWARE\Classes\gzwfile
			snprintf(key,sizeof(key),"SOFTWARE\\Classes\\%s",value);
			if((reg = regkey.Open(HKEY_CURRENT_ACCESS_LEVEL,key))==ERROR_SUCCESS)
			{
				// crea la chiave per l'apertura del file tramite la shell
				// es. HKEY_LOCAL_MACHINE\SOFTWARE\Classes\extfile\shell\open\command
				snprintf(key,sizeof(key),"SOFTWARE\\Classes\\%s\\shell",value);
				if((reg = regkey.Open(HKEY_CURRENT_ACCESS_LEVEL,key))==ERROR_SUCCESS)
				{
					regkey.DeleteKey(lpcszMenuText);
					regkey.Close();
				}
				else
					flag = FALSE;
			}
			else
				flag = FALSE;
		}
	}

	return(flag);
}

// codice nuovo (2026):

/*
	OpenFile()

	Apre il file lanciando l'applicazione associata come default di sistema all'estensione di tale file.

	Chiede alla Shell di eseguire l'applicazione registrata per gestire il tipo file specificato dalla
	estensione.	
*/
BOOL CRegistry::OpenFile(LPCSTR lpcszFileName,LPCSTR lpcszParameters)
{
	// ShellExecute gestisce:
	// - HKCU (associazioni utente moderne)
	// - HKLM (associazioni di sistema/legacy)
	// - Verbi (open, edit, runas)
	HINSTANCE hInst = ShellExecute(	NULL,           // parent window
									"open",         // tipo operazione
									lpcszFileName,  // file o programma
									lpcszParameters,// parametri
									NULL,           // directory di lavoro (se NULL usa quella del file)
									SW_SHOWNORMAL   // stato della finestra
									);

	// se il ritorno e' <= 32 si e' verificato un errore
	return(((INT_PTR)hInst > 32));
}

/*
	RunFile()

	Apre il file lanciando l'applicazione specificata, passandole il nome del file di input.

	NON chiede alla Shell come fa in Openfile(), ma ricava in proprio la linea di comando da 
	eseguire a fronte dell'eseguibile, per poterlo aprire passandogli il file di input.
*/
BOOL CRegistry::RunFile(LPCSTR lpcszCustomExePath,LPCSTR lpcszInputFile)
{
	// estrae il nome dell'eseguibile dal path
	char szExeName[_MAX_FNAME+_MAX_EXT+1] = {0};
	char szDrive[_MAX_DRIVE+1] = {0};
	char szDir[_MAX_DIR+1] = {0};
	char szFname[_MAX_FNAME+1] = {0};
	char szExt[_MAX_EXT+1] = {0};
	_splitpath(lpcszCustomExePath,szDrive,szDir,szFname,szExt);
	snprintf(szExeName,sizeof(szExeName),"%s%s",szFname,szExt);

	BOOL bFound = FALSE;
	char szTemplate[MAX_PATH*2] = {0};

	// scansione HKEY_CLASSES_ROOT
	HKEY hRoot;
	if(RegOpenKeyEx(HKEY_CLASSES_ROOT,NULL,0,KEY_ENUMERATE_SUB_KEYS,&hRoot)==ERROR_SUCCESS)
	{
		char szSubKeyName[256] = {0};
		DWORD dwIndex = 0;
        
		while(RegEnumKey(hRoot,dwIndex++,szSubKeyName,sizeof(szSubKeyName))==ERROR_SUCCESS)
		{
			// cerca solo nelle chiavi che hanno shell\open\command
			char szCommandPath[512] = {0};
			snprintf(szCommandPath,sizeof(szCommandPath),"%s\\shell\\open\\command",szSubKeyName);
            
			HKEY hCmdKey;
			if(RegOpenKeyEx(HKEY_CLASSES_ROOT,szCommandPath,0,KEY_READ,&hCmdKey)==ERROR_SUCCESS)
			{
				char szValue[MAX_PATH*2] = {0};
				DWORD dwType = 0L;
				DWORD dwValSize = sizeof(szValue);
                
				if(RegQueryValueEx(hCmdKey,NULL,NULL,&dwType,(LPBYTE)szValue,&dwValSize)==ERROR_SUCCESS)
				{
					// l'eseguibile viene menzionato in questa riga di comando?
					if(strstr(strlwr(szValue),strlwr(szExeName))!=NULL)
					{
						strcpyn(szTemplate,szValue,sizeof(szTemplate));
						bFound = TRUE;
					}
				}
				::RegCloseKey(hCmdKey);
			}
			if(bFound)
				break;
		}
		::RegCloseKey(hRoot);
	}

	// se ha trovato il template, sostituisce %1 con il file eseguibile
	char szFinalCmd[(MAX_PATH * 2) + MAX_PATH] = {0};
	if(bFound)
	{
		// alcuni template hanno "%1", altri %1
		int ofs = 4;
		char* pToken = strstr(szTemplate,"\"%1\"");
		if(!pToken)
		{
			ofs = 2;
			pToken = strstr(szTemplate,"%1");
		}
		if(pToken)
		{
			*pToken = '\0'; // Tagliamo la stringa al placeholder
			snprintf(szFinalCmd,sizeof(szFinalCmd),"%s\"%s\"%s",szTemplate,lpcszInputFile,pToken + ofs);
		}
		else
		{
			// se non trova %1, lo aggiunge in coda
			snprintf(szFinalCmd,sizeof(szFinalCmd),"\"%s\" \"%s\"",lpcszCustomExePath,lpcszInputFile);
		}
	}
	else
	{
		// se non trovia nulla nel registro, usa il formato standard
		snprintf(szFinalCmd,sizeof(szFinalCmd),"\"%s\" \"%s\"",lpcszCustomExePath,lpcszInputFile);
	}

	// il ramo #if 0 e' la forma corretta, ma... (vedi sotto)
#if 0
	STARTUPINFO si;
	PROCESS_INFORMATION pi;

	ZeroMemory(&si,sizeof(si));
	si.cb = sizeof(si);
	si.dwFlags = STARTF_USESHOWWINDOW;
	si.wShowWindow = SW_HIDE;

	ZeroMemory(&pi,sizeof(pi));

	if(CreateProcess(NULL,szFinalCmd,NULL,NULL,FALSE,CREATE_NO_WINDOW,NULL,NULL,&si,&pi))
	{
		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);
	}
#else
	// prepara per il lancio del programma
	STARTUPINFO si = {0};
	PROCESS_INFORMATION pi = {0};

	si.cb = sizeof(si);
	// sembra un controsenso ma e' per ovviare alle minchiate di M$, per obbligare la CreateProcess()
	// a guardare in si.wShowWindow, altrimenti ignorerebbe l'SW_HIDE seguente
	si.dwFlags = STARTF_USESHOWWINDOW;
	si.wShowWindow = SW_HIDE;

	// https://gitlab.gnome.org/GNOME/gimp/-/work_items/16376
	// qui vengono al pettine i nodi delle cappellate fatte da programmi tipo GIMP, come quando tira 
	// fuori a console messaggi come questi:
	//	"set device 'System Aggregated Pointer' to mode: disabled"
	//	"(Teclee un carácter cualquiera para cerrar esta ventana)"
	// GIMP scrive direttamente su stderr ma, non trovando un terminale proprio, si appoggia a quello 
	// del padre (ossia il programma che lo esegue)
	// il comando > NUL 2>&1 agisce come un silenziatore, intercettando i messaggi prima che arrivino 
	// alla console del programma
	// GIMP e' solo un esempio, molti dei porting Unix/Linux a Windows operano nello stesso modo AC/DC
	char szSilentCmd[MAX_PATH * 3] = {0};
	snprintf(szSilentCmd,sizeof(szSilentCmd),"cmd.exe /c \" %s > NUL 2>&1 \"",szFinalCmd);

	if(CreateProcess(	NULL,
						szSilentCmd,
						NULL,
						NULL,
						FALSE,
						CREATE_NO_WINDOW,
						NULL,
						NULL,
						&si,
						&pi))
	{
		::CloseHandle(pi.hProcess);
		::CloseHandle(pi.hThread);
		return(TRUE);
	}
#endif

    return(FALSE);
}
