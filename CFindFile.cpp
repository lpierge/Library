/*$
	CFindFile.cpp
	Classe base per la ricerca files (SDK/MFC).
	Luca Piergentili, 14/02/00
	lpiergentili@yahoo.com
*/
#include "pragma.h"
#include "env.h"
#include "macro.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "strings.h"
#include "window.h"
#include "win32api.h"
#include "CNodeList.h"
#include "CDateTime.h"
#include "CFindFile.h"

#include "traceexpr.h"
//#define _TRACE_FLAG			_TRFLAG_TRACEOUTPUT // opzioni: _TRFLAG_NOTRACE, _TRFLAG_TRACEFILE, _TRFLAG_TRACECONSOLE, _TRFLAG_TRACEOUTPUT, _TRFLAG_TRACEBREAKPOINT
#define _TRACE_FLAG			_TRFLAG_NOTRACE // opzioni: _TRFLAG_NOTRACE, _TRFLAG_TRACEFILE, _TRFLAG_TRACECONSOLE, _TRFLAG_TRACEOUTPUT, _TRFLAG_TRACEBREAKPOINT
#define _TRACE_FLAG_INFO	_TRACE_FLAG
#define _TRACE_FLAG_WARN	_TRACE_FLAG
#define _TRACE_FLAG_ERR		_TRACE_FLAG

/*
	CFindFile()
*/
CFindFile::CFindFile()
{
	CFindFile::Reset(FALSE);
}

/*
	Reset()
*/
void CFindFile::Reset(BOOL bClose/* = TRUE*/)
{
	if(bClose)
		CFindFile::Close();

	m_enumDateFormat = UTC_TIME;
	m_nCount = 0;
	m_nInternalCount = -1;
	m_bFindFirstCall = TRUE;
	m_bFindExFirstCall = TRUE;
	memset(&m_stFindFile,'\0',sizeof(FINDFILE));
	m_stFindFile.handle = INVALID_HANDLE_VALUE;
	memset(&m_stWin32FindData,'\0',sizeof(WIN32_FIND_DATA));
	m_listFileNames.EraseAll();

	// puntatore al <this> della classe chiamante che implementa le callbacks (statica e reale), 
	// in modo  tale che la callback statica, chiamata qui, possa poi chiamare la callback reale
	m_thisPtr = NULL;
	m_lpfnCallBack = NULL;
}

/*
	ExistEx()
	
	Controlla se lo skeleton esiste.
	Passare il pathname completo + lo skeleton ("c:\*.exe" or "c:\file.exe").
*/
BOOL CFindFile::ExistEx(LPCSTR lpcszSkel)
{
	ASSERTEXPR(lpcszSkel);
	if(!lpcszSkel)
		return(FALSE);

	BOOL bExist = CFindFile::First(lpcszSkel,_A_NORMAL|_A_RDONLY|_A_HIDDEN|_A_SYSTEM);
	
	CFindFile::Close();

	return(bExist);
}

/*
	Find()
	
	Ricerca lo skeleton specificato.
	Passare il pathname completo + lo skeleton ("c:\*.exe" or "c:\file.exe").
	Chiamare in un ciclo fino a che non restituisce NULL.
*/
LPCSTR CFindFile::Find(LPCSTR lpcszSkel/* = "\\*.*" */)
{    
	ASSERTEXPR(lpcszSkel);
	if(!lpcszSkel)
		return(FALSE);

	char* p;
	LPSTR lpFile = NULL;
	static char szPathName[_MAX_FILEPATH+1] = {0};
	static char szFileName[_MAX_FILEPATH+1] = {0};

	// ricava il pathname
	memset(szPathName,'\0',sizeof(szPathName));
	strcpyn(szFileName,lpcszSkel,sizeof(szFileName));
	strrev(szFileName);
	if((p = strchr(szFileName,'\\'))==(char*)NULL)
		p = strchr(szFileName,':');
	if(p)
	{
		strcpyn(szPathName,p,sizeof(szPathName));
		strrev(szPathName);
	}
	memset(szFileName,'\0',sizeof(szFileName));
    	
	// cerca la prima istanza dello skeleton
	if(m_bFindFirstCall)
	{
		m_bFindFirstCall = FALSE;
		m_nCount = 0;
	     
		// i files trovati corrispondono allo skeleton
		if(CFindFile::First(lpcszSkel,_A_NORMAL|_A_RDONLY|_A_HIDDEN|_A_SYSTEM))
		{
			if(m_stFindFile.attrib & _A_SUBDIR)
				goto next;

			strcpyn(szFileName,m_stFindFile.name,sizeof(szFileName));
			strcatn(szPathName,szFileName,sizeof(szPathName));
			lpFile = szPathName;
			m_nCount++;

			// se e' stato specificato un pathname completo e non uno skeleton ("c:\\file.ext"), termina la ricerca
			if(strchr(lpcszSkel,'?')==(char*)NULL && strchr(lpcszSkel,'*')==(char*)NULL)
			{
				CFindFile::Close();
				m_bFindFirstCall = TRUE;
			}
		}
		else
		{
			CFindFile::Close();
			m_bFindFirstCall = TRUE;
		}
    }
    else // cerca le istanze successive dello skeleton
    {
next:
		// trovato il file seguente
		if(CFindFile::Next())
		{
			// esclude "." e ".."
			if(m_stFindFile.attrib & _A_SUBDIR)
				goto next;

			strcpyn(szFileName,m_stFindFile.name,sizeof(szFileName));
			strcatn(szPathName,szFileName,sizeof(szPathName));
			lpFile = szPathName;
			m_nCount++;
		}
		else
		{
			CFindFile::Close();
			m_bFindFirstCall = TRUE;
		}
	}

	return(lpFile);
}

/*
	FindEx()
	
	Cerca lo skeleton specificato a partire dalla directory iniziale (terminare con '\').
	Chiamare in un ciclo fino a che non restituisce NULL.
*/
LPCSTR CFindFile::FindEx(	LPCSTR	lpcszStartDir	/* = "\\" */,
							LPCSTR	lpcszSkel		/* = "*.*" */,
							BOOL	bRecursive		/* = TRUE */,
							UINT	uAttribute		/* = _A_ALLFILES */,
							UINT*	puiAttribute	/* = NULL */,
							DWORD*	pdwSize			/* = NULL */,
							VOID*	pVoidPtr		/* = NULL */)
{
	ASSERTEXPR(lpcszStartDir && lpcszSkel);
	if(!lpcszStartDir || !lpcszSkel)
		return(NULL);

	LPSTR lpFile = NULL;

	// crea la lista
	if(m_bFindExFirstCall)
	{
		m_nCount = 0;
		m_listFileNames.RemoveAll();

		// chiama la callback per comunicare l'inizio della ricerca
		if(m_lpfnCallBack)
			m_lpfnCallBack((WPARAM)m_thisPtr,(LPARAM)NULL,(LPARAM)0,(LPARAM)pVoidPtr);

		// cerca lo skeleton (chiamera' la callback)
		CFindFile::Search(lpcszStartDir,lpcszSkel,bRecursive,uAttribute,pVoidPtr);

		// chiama la callback per comunicare il termine della ricerca
		if(m_lpfnCallBack)
			m_lpfnCallBack((WPARAM)m_thisPtr,(LPARAM)NULL,(LPARAM)2,(LPARAM)pVoidPtr);
		
		m_bFindExFirstCall = FALSE;

		m_nCount = m_listFileNames.Count();
	}

	// restituisce quanto presente nella lista
	if(!m_bFindExFirstCall)
	{
		if(m_listFileNames.Count() > 0)
		{
			if(m_nInternalCount==-1)
				m_nInternalCount = 0;

			FINDFILE* f;
			if((f = (FINDFILE*)m_listFileNames.GetAt(m_nInternalCount))!=(FINDFILE*)NULL)
			{
				lpFile = f->name;
				if(puiAttribute)
					*puiAttribute = f->attrib;
				if(pdwSize)
					*pdwSize = f->size;
				m_nInternalCount++;
			}
			else
			{
				lpFile = NULL;
				m_listFileNames.RemoveAll();
				m_bFindExFirstCall = TRUE;
				m_nInternalCount = -1;
			}
		}
		else
		{
			m_bFindExFirstCall = TRUE;
			m_nInternalCount = -1;
		}
	}

	return(lpFile);
}

/*
	FindFile()
	
	Cerca lo skeleton specificato a partire dalla directory iniziale (terminare con '\').
	I files trovati vengono inseriti nella lista interna e devono essere recuperati con la Get...().
*/
UINT CFindFile::FindFile(	LPCSTR	lpcszStartDir	/* = "\\" */,
							LPCSTR	lpcszSkel		/* = "*.*" */,
							BOOL	bRecursive		/* = TRUE */,
							UINT	uAttribute		/* = _A_ALLFILES */,
							VOID*	pVoidPtr		/* = NULL */)
{
	ASSERTEXPR(lpcszStartDir && lpcszSkel);
	if(!lpcszStartDir || !lpcszSkel)
		return(0);

	m_listFileNames.RemoveAll();
		
	// chiama la callback per comunicare l'inizio della ricerca
	if(m_lpfnCallBack)
		m_lpfnCallBack((WPARAM)m_thisPtr,(LPARAM)NULL,(LPARAM)0,(LPARAM)pVoidPtr);

	// cerca lo skeleton (chiamera' la callback)
	CFindFile::Search(lpcszStartDir,lpcszSkel,bRecursive,uAttribute,pVoidPtr);

	// chiama la callback per comunicare il termine della ricerca
	if(m_lpfnCallBack)
		m_lpfnCallBack((WPARAM)m_thisPtr,(LPARAM)NULL,(LPARAM)2,(LPARAM)pVoidPtr);

	m_nCount = m_listFileNames.Count();

	return(m_nCount);
}

/*
	GetFileName()
	
	Restituisce il nome file relativo all'elemento della lista interna.
	Chiamare in un ciclo che vada da 0 a Count().
*/
LPCSTR CFindFile::GetFileName(int nIndex)
{
	ASSERTEXPR(nIndex >= 0);

	LPSTR lpFile = NULL;
	
	if(m_listFileNames.Count() > 0)
	{
		if(nIndex >= 0 && nIndex < m_listFileNames.Count())
		{
			FINDFILE* f;
			if((f = (FINDFILE*)m_listFileNames.GetAt(nIndex))!=(FINDFILE*)NULL)
				lpFile = f->name;
		}
	}

	return(lpFile);
}

/*
	GetFindFile()
	
	Restituisce l'elemento della lista interna.
	Chiamare in un ciclo che vada da 0 a Count().
*/
FINDFILE* CFindFile::GetFindFile(int nIndex)
{
	ASSERTEXPR(nIndex >= 0);

	FINDFILE* f = NULL;
	
	if(m_listFileNames.Count() > 0)
		if(nIndex >= 0 && nIndex < m_listFileNames.Count())
			f = (FINDFILE*)m_listFileNames.GetAt(nIndex);

	return(f);
}

/*
	Search()
	
	Cerca lo skeleton specificato a partire dalla directory iniziale (terminare con '\').
	I files trovati vengono inseriti nella lista interna.

	Al momento solo esistono due chiamate alla callback, quella effettuta qui sotto per informare sul file trovato, e la 
	finale, chiamata NON qui ma in FindEx() e FindFile(), al ritorno della chiamata alla Search(). La chiamata iniziale,
	per avvisare che inizia la ricerca, non esiste e chi usa la classe deve occuparsi, nel suo proprio codice, di tenere
	traccia di quando la callback viene chiamata per la prima volta, almeno fino a quando non venga modificato il codice
	in questa classe.
 	La chiamata finale alla callback per comunicare il termine della ricerca, va fatta fuori da qui perche' la Search() 
	chiamandosi ricorsivamente per ricorrere le sub directory, genererebbe piu di una chiamata finale, confondendo il 
	chiamante.
*/
HANDLE CFindFile::Search(LPCSTR lpcszStartDir,LPCSTR lpcszSkel,BOOL bRecursive,UINT uAttribute,VOID* pVoidPtr)
{
	ASSERTEXPR(lpcszStartDir && lpcszSkel);
	if(!lpcszStartDir || !lpcszSkel)
		return(INVALID_HANDLE_VALUE);

	BOOL bAddToList = FALSE;
	FINDFILE f = {0};
	char szPathName[_MAX_FILEPATH+1] = {0};

	if(lpcszStartDir[strlen(lpcszStartDir)-1]!='\\')
		return(INVALID_HANDLE_VALUE);

	if((strlen(lpcszStartDir) + strlen(lpcszSkel)) < sizeof(szPathName))
		snprintf(szPathName,sizeof(szPathName),"%s%s",lpcszStartDir,lpcszSkel);
	else
		return(INVALID_HANDLE_VALUE);

	// cerca la prima istanza dello skeleton
	if(CFindFile::First(szPathName,uAttribute,&f))
	{
		do
		{
			// considera solo i nomi file
			if(f.name[0]!='.')
			{
				// imposta il pathname completo
				if((strlen(lpcszStartDir) + strlen(f.name)) < sizeof(szPathName))
				{
					snprintf(szPathName,sizeof(szPathName),"%s%s",lpcszStartDir,f.name);
					strcpyn(f.name,szPathName,sizeof(f.name));
				}
				else
					return(INVALID_HANDLE_VALUE);

				// chiama la callback, se impostata, e aggiunge o meno il file alla lista interna a 
				// seconda del valore che questa restituisce (1 aggiunge alla lista, 0 lo scarta)
				bAddToList = TRUE;
				if(m_lpfnCallBack)
					bAddToList = m_lpfnCallBack((WPARAM)m_thisPtr,(LPARAM)&f,(LPARAM)1,(LPARAM)pVoidPtr);

				// inserisce il nome file nella lista
				if(bAddToList)
				{
					FINDFILE* ff = (FINDFILE*)m_listFileNames.Add();
					if(ff)
					{
						ff->attrib = f.attrib;
						memcpy(&ff->datetime,&f.datetime,sizeof(SYSTEMTIME));
						ff->size = f.size;
						strcpyn(ff->name,szPathName,sizeof(ff->name));
					}
				}

				// per non grippare il sistema
				::Yield();
			}
		}
		while(CFindFile::Next(&f));

		// chiude ed invalida l'handle per evitare memory/resource leak
        CFindFile::Close(f.handle); 
        f.handle = INVALID_HANDLE_VALUE;

	}

	if(bRecursive)
	{
		// pathname completo
		if((strlen(lpcszStartDir) + 3) < sizeof(szPathName))
			snprintf(szPathName,sizeof(szPathName),"%s*.*",lpcszStartDir);
		else
			return(INVALID_HANDLE_VALUE);
		
		// cerca la prima istanza dello skeleton
		if(CFindFile::First(szPathName,_A_SUBDIR,&f))
		{
			do
			{
				// cerca nelle subdir, pero' verifica prima che non si tratti dei puntatori . e ..
				if((f.attrib & _A_SUBDIR) && f.name[0]!='.')
				{
					// filtro di sicurezza: evita i Reparse Points (link simbolici/Mount points)
					// previene loop infiniti e scansioni di drive esterni mappati come cartelle
					// es. il Junction Point (un tipo di Reparse Point) e' un link, nella dir
					// figlia, alla dir padre, se non si evita produce una ricorsione infinita
					// usato anche per gli alias, es. C:\Documents and Settings che punta alla
					// dir C:\Users
					if(f.attrib & FILE_ATTRIBUTE_REPARSE_POINT)
						continue;

					// filtro per nome: esclude le cartelle di sistema ed il cestino
					if(stricmp(f.name,"$Recycle.Bin")==0 || stricmp(f.name,"System Volume Information")==0 || stricmp(f.name,"RECYCLER"/* sistemi FAT32/XP */)==0)
						continue;

					// ha passato i filtri, costruisce il pathname completo per la ricerca (ricorsiva)
					if((strlen(lpcszStartDir) + strlen(f.name) + 1) < sizeof(szPathName))
						snprintf(szPathName,sizeof(szPathName),"%s%s\\",lpcszStartDir,f.name);
					else
						return(INVALID_HANDLE_VALUE);
					
					// ricerca ricorsivamente
					CFindFile::Search(szPathName,lpcszSkel,bRecursive,uAttribute,pVoidPtr);
				}
			}
			while(CFindFile::Next(&f));

			// chiude ed invalida l'handle per evitare memory/resource leak
            CFindFile::Close(f.handle);
            f.handle = INVALID_HANDLE_VALUE;

		}
	}

	//return(f.handle);
	return(VALID_HANDLE_VALUE);
}

/*
	First()

	Cerca la prima istanza del nome file.
	Il terzo parametro e' opzionale perche per la ricerca ricorsiva deve essere usata la struttura del chiamante, non quella della classe.
*/
BOOL CFindFile::First(LPCSTR lpcszFileName,UINT /*uAttribute*/,FINDFILE* f/* = NULL */)
{
	ASSERTEXPR(lpcszFileName);
	if(!lpcszFileName)
		return(FALSE);

	HANDLE hHandle = INVALID_HANDLE_VALUE;

	if(!f)
	{
		memset(&m_stFindFile,'\0',sizeof(FINDFILE));
		m_stFindFile.handle = INVALID_HANDLE_VALUE;
	}
	else
	{
		memset(f,'\0',sizeof(FINDFILE));
		f->handle = INVALID_HANDLE_VALUE;
	}

	if((hHandle = ::FindFirstFile(lpcszFileName,&m_stWin32FindData))!=INVALID_HANDLE_VALUE)
	{
		FILETIME localfiletime;
		
		if(!f)
		{
			// struttura della classe
			strcpyn(m_stFindFile.name,m_stWin32FindData.cFileName,sizeof(m_stFindFile.name));
			m_stFindFile.handle  = hHandle;
			m_stFindFile.size    = m_stWin32FindData.nFileSizeLow;
			if(m_enumDateFormat==LOCAL_TIME)
			{
				::FileTimeToLocalFileTime(&m_stWin32FindData.ftLastWriteTime,&localfiletime);
				::FileTimeToSystemTime(&localfiletime,&m_stFindFile.datetime);
			}
			else if(m_enumDateFormat==UTC_TIME)
			{
				::FileTimeToSystemTime(&m_stWin32FindData.ftLastWriteTime,&m_stFindFile.datetime);
			}
			m_stFindFile.attrib  = (UINT)m_stWin32FindData.dwFileAttributes;
		}
		else
		{
			// struttura del chiamante
			strcpyn(f->name,m_stWin32FindData.cFileName,sizeof(f->name));
			f->handle  = hHandle;
			f->size    = m_stWin32FindData.nFileSizeLow;
			if(m_enumDateFormat==LOCAL_TIME)
			{
				::FileTimeToLocalFileTime(&m_stWin32FindData.ftLastWriteTime,&localfiletime);
				::FileTimeToSystemTime(&localfiletime,&f->datetime);
			}
			else if(m_enumDateFormat==UTC_TIME)
			{
				::FileTimeToSystemTime(&m_stWin32FindData.ftLastWriteTime,&f->datetime);
			}
			f->attrib  = (UINT)m_stWin32FindData.dwFileAttributes;
		}
	}

	return(hHandle!=INVALID_HANDLE_VALUE);
}

/*
	Next()
	
	Cerca l'istanza successiva del nome file.
*/
BOOL CFindFile::Next(FINDFILE* f/* = NULL */)
{
	HANDLE hHandle = (f!=(FINDFILE*)NULL ? f->handle : m_stFindFile.handle);

	if(hHandle==INVALID_HANDLE_VALUE)
		return(FALSE);

	if(!::FindNextFile(hHandle,&m_stWin32FindData))
		return(FALSE);

	FILETIME localfiletime = {0};

	if(!f)
	{
		// struttura della classe
		strcpyn(m_stFindFile.name,m_stWin32FindData.cFileName,sizeof(m_stFindFile.name));
		m_stFindFile.size    = m_stWin32FindData.nFileSizeLow;
		if(m_enumDateFormat==LOCAL_TIME)
		{
			::FileTimeToLocalFileTime(&m_stWin32FindData.ftLastWriteTime,&localfiletime);
			::FileTimeToSystemTime(&localfiletime,&m_stFindFile.datetime);
		}
		else if(m_enumDateFormat==UTC_TIME)
		{
			::FileTimeToSystemTime(&m_stWin32FindData.ftLastWriteTime,&m_stFindFile.datetime);
		}
		m_stFindFile.attrib  = (UINT)m_stWin32FindData.dwFileAttributes;
	}
	else
	{
		// struttura del chiamante
		strcpyn(f->name,m_stWin32FindData.cFileName,sizeof(f->name));
		f->size    = m_stWin32FindData.nFileSizeLow;
		if(m_enumDateFormat==LOCAL_TIME)
		{
			::FileTimeToLocalFileTime(&m_stWin32FindData.ftLastWriteTime,&localfiletime);
			::FileTimeToSystemTime(&localfiletime,&f->datetime);
		}
		else if(m_enumDateFormat==UTC_TIME)
		{
			::FileTimeToSystemTime(&m_stWin32FindData.ftLastWriteTime,&f->datetime);
		}
		f->attrib  = (UINT)m_stWin32FindData.dwFileAttributes;
	}

	return(TRUE);
}

/*
	Close()
	
	Chiude la ricerca.
*/
BOOL CFindFile::Close(HANDLE h/* = INVALID_HANDLE_VALUE*/)
{
	BOOL bClosed = FALSE;

	HANDLE hHandle = (h!=INVALID_HANDLE_VALUE ? h : m_stFindFile.handle);

	if(hHandle!=INVALID_HANDLE_VALUE)
	{
		bClosed = ::FindClose(hHandle);
		m_stFindFile.handle = INVALID_HANDLE_VALUE;
	}
	
	return(bClosed);
}

/*
	SplitPathName()

	Divide il pathname nei suoi componenti.
*/
BOOL CFindFile::SplitPathName(LPCSTR lpcszPathName,LPSTR lpszDirectory,UINT cbDirectory,LPSTR lpszFileName,UINT cbFileName,BOOL bUseCurrenDirectory/* = TRUE*/)
{
	ASSERTEXPR(lpcszPathName && lpszDirectory && cbDirectory > 0 && lpszFileName && cbFileName > 0);
	if(!lpcszPathName || !lpszDirectory || cbDirectory <= 0 || !lpszFileName || cbFileName <= 0)
		return(FALSE);

	char* p;
	
	memset(lpszDirectory,'\0',cbDirectory);
	memset(lpszFileName,'\0',cbFileName);

	// controlla se il pathname contiene una directory
	if(!(p = (char*)strchr(lpcszPathName,'\\')))
		p = (char*)strchr(lpcszPathName,':');

	if(p)
	{
		char szBuffer[_MAX_FILEPATH+1] = {0};
		
		strcpyn(szBuffer,lpcszPathName,sizeof(szBuffer));
		strrev(szBuffer);
		
		if((p = (char*)strchr(szBuffer,'\\'))==(char*)NULL)
			p = (char*)strchr(szBuffer,':');

		strcpyn(lpszDirectory,p,cbDirectory);
		strrev(lpszDirectory);
		*p = '\0';
		
		strcpyn(lpszFileName,szBuffer,cbFileName);
		strrev(lpszFileName);
	}
	else // nessuna directory, ricava la corrente
	{
		if(bUseCurrenDirectory)
			::GetCurrentDirectory(cbDirectory,lpszDirectory);
		strcpyn(lpszFileName,lpcszPathName,cbFileName);
	}

	if(*lpszDirectory)
		if((lpszDirectory[strlen(lpszDirectory)-1]!='\\') && ((int)(strlen(lpszDirectory)+1) < cbDirectory))
			strcatn(lpszDirectory,"\\",cbDirectory);

	return(TRUE);
}

/*
	CreatePathName()

	Crea il pathname.

	Se solo viene passato il nome del patname, si limita a crearlo cosi' come e'. Se invece viene passato
	anche la dimensione del buffer, allora controlla la validita' dei caratteri presenti nel nome e alla
	fine aggiorna il pathname ricevuto in input con la versione eventualmente corretta.
	Durante l'elaborazione aggiunge un \ alla fine di lpszPathName, tenerlo presente e nel caso eliminarlo
	con RemoveBackslash().
*/
BOOL CFindFile::CreatePathName(LPSTR lpszPathName,UINT cbPathName/* = (UINT)-1*/)
{    
	int i,n;
	char* p;
	FINDFILE* f;
	CFindFileList listDirectoryNames;
	char szDrive[_MAX_DRIVE+1+1] = {0};
	char szDirectory[1024] = {0};
	DWORD dwAttribute = 0L;
	BOOL bCreated = FALSE;

	// lavora sulla copia locale
	// se viene specificata la dimensione del buffer, effettua il controllo sulla validita' dei caratteri
	if(cbPathName!=(UINT)-1)
	{
		i = n = 0;
		if(isalpha(lpszPathName[0]) && lpszPathName[1]==':' && lpszPathName[2]=='\\')
		{
			szDirectory[i++] = lpszPathName[n++];
			szDirectory[i++] = lpszPathName[n++];
			szDirectory[i++] = lpszPathName[n++];
		}
		for(; lpszPathName[n] && i <= sizeof(szDirectory)-1; n++)
			if(!strchr("/:*?\"<>|",lpszPathName[n]))
				szDirectory[i++] = lpszPathName[n];
	}
	else
		strcpyn(szDirectory,lpszPathName,sizeof(szDirectory));
	
	strltrim(szDirectory);
	strrtrim(szDirectory);
	strstrim(szDirectory);
	EnsureBackslash(szDirectory,sizeof(szDirectory));

	// non crea il pathname se gia' esiste, se si tratta di un file restituisce errore
	if((dwAttribute = ::GetFileAttributes(szDirectory))!=(DWORD)-1L)
	{
		bCreated = (dwAttribute & FILE_ATTRIBUTE_DIRECTORY) ? TRUE : FALSE;
		goto done;
	}
		
	memset(szDrive,'\0',sizeof(szDrive));
	if((p = (char*)strchr(szDirectory,':'))!=(char*)NULL)
	{
		szDrive[0] = *(p-1);
		szDrive[1] = *(p);
		
		if(*(p+1)=='\\')
		{
			szDrive[2] = '\\';
			szDrive[3] = '\0';
		}
		else
			szDrive[2] = '\0';
	}
	else
	{
		// imposta il drive solo per i pathname assoluti
		if(szDirectory[0]=='\\')
		{
			char szBuffer[_MAX_FILEPATH+1];
			if(::GetCurrentDirectory(sizeof(szBuffer)-1,szBuffer)!=0)
			{
				szDrive[0] = szBuffer[0];
				szDrive[1] = ':';
				szDrive[2] = '\\';
				szDrive[3] = '\0';
			}
			else
			{
				bCreated = FALSE;
				goto done;
			}
		}
	}
	
	// rimuove l'identificatore del drive
	strrev(szDirectory);
	if((p = strchr(szDirectory,':'))!=(char*)NULL)
		*p = '\0';
	strrev(szDirectory);

	// crea la lista con le directory contenute nel pathname
	listDirectoryNames.RemoveAll();
	
	// il pathname deve contenere almeno una directory
	if(strchr(szDirectory,'\\')!=(char*)NULL)
	{
		char* token = strtok(szDirectory,"\\");
		
		for(int i = 0; token!=(char*)NULL; i++)
		{
			f = (FINDFILE*)listDirectoryNames.Add();
			if(f)
			{
				strcpyn(f->name,token,sizeof(f->name));
				token = strtok((char*)NULL,"\\");
			}
		}

		szDirectory[0] = '\0';
	}

	// identificativo del drive
	if(*szDrive)
		strcpyn(szDirectory,szDrive,sizeof(szDirectory));
	
	// crea il pathname
	for(i = 0; i < listDirectoryNames.Count(); i++)
	{
		if((f = (FINDFILE*)listDirectoryNames.GetAt(i))!=(FINDFILE*)NULL)
		{
			strcatn(szDirectory,f->name,sizeof(szDirectory));

			if(::GetFileAttributes(szDirectory)==(DWORD)-1L)
				::CreateDirectory(szDirectory,NULL);
			
			strcatn(szDirectory,"\\",sizeof(szDirectory));
		}
	}

	listDirectoryNames.RemoveAll();

done:

	if(cbPathName!=(UINT)-1)
		strcpyn(lpszPathName,szDirectory,cbPathName);

	return(bCreated ? bCreated : (::GetFileAttributes(szDirectory)!=(DWORD)-1L));
}
