/*$
	CSync.cpp
	Classi per la sincronizzazione tra processi/threads.
	Luca Piergentili, 18/11/02
	lpiergentili@yahoo.com

	Vedi le note in CSync.h
*/
#include "pragma.h"
#include "env.h"
#include "macro.h"
#include <string.h>
#include <ctype.h>
#include "strings.h"
#include "window.h"
#include "datetime.h"
#include "win32api.h"
#include "CSync.h"

#include "traceexpr.h"
//#define _TRACE_FLAG			_TRFLAG_TRACEOUTPUT // opzioni: _TRFLAG_NOTRACE, _TRFLAG_TRACEFILE, _TRFLAG_TRACECONSOLE, _TRFLAG_TRACEOUTPUT, _TRFLAG_TRACEBREAKPOINT
#define _TRACE_FLAG			_TRFLAG_NOTRACE // opzioni: _TRFLAG_NOTRACE, _TRFLAG_TRACEFILE, _TRFLAG_TRACECONSOLE, _TRFLAG_TRACEOUTPUT, _TRFLAG_TRACEBREAKPOINT
#define _TRACE_FLAG_INFO	_TRACE_FLAG
#define _TRACE_FLAG_WARN	_TRACE_FLAG
#define _TRACE_FLAG_ERR		_TRACE_FLAG

/*
	CSyncThreads()
*/
CSyncThreads::CSyncThreads(LPCSTR lpcszName/* = NULL */,int nTimeout/* = 0 */)
{
	m_nCounter = 0L;
	
	m_nTimeout = nTimeout;
	if(m_nTimeout <= 0)
		m_nTimeout = SYNC_DEFAULT_TIMEOUT;
	
	memset(m_szName,'\0',sizeof(m_szName));
	if(lpcszName)
		SetName(lpcszName);

	memset(&m_CriticalSection,'\0',sizeof(m_CriticalSection));
	::InitializeCriticalSection(&m_CriticalSection);

	char szWindowsPlatform[128] = {0};
	DWORD dwMajorVersion = 0L;
	DWORD dwMinorVersion = 0L;
	m_osVersion = GetWindowsVersion(szWindowsPlatform,sizeof(szWindowsPlatform),&dwMajorVersion,&dwMinorVersion);
}

/*
	~CSyncThreads()
*/
CSyncThreads::~CSyncThreads()
{
	// qui nella classe non esiste, e non puo'/deve esistere, un controllo su eventuali threads utilizzando 
	// la sezione critica al momento della chiusura
	// il processo principale (ossia chi ha creato l'oggetto CSyncThreads) deve sincronizzarsi con i threads 
	// usando meccanismi come WaitForMultipleObjects() e solo dopo essersi assicurato del termine di tutti i 
	// threads, puo' eliminare la sezione critica e rilasciare l'oggetto CSyncThreads
	::DeleteCriticalSection(&m_CriticalSection);
}

/*
	Lock()
*/
BOOL CSyncThreads::Lock(int nTimeout/* = 0 */)
{
	BOOL bLocked = FALSE;
	int nElapsed = 0;
	int nMsTimeoutIncrement = SYNC_MS_TIMEOUT_INCREMENT;

	// stabilisce il valore per il timeout sull'attesa per il lock
	if(nTimeout==0)
	{
		if(m_nTimeout <= 0)
			m_nTimeout = SYNC_DEFAULT_TIMEOUT;
		nTimeout = m_nTimeout;
	}
	if(nTimeout!=SYNC_INFINITE_TIMEOUT)
	{
		nMsTimeoutIncrement = nTimeout / 10;
		if(nMsTimeoutIncrement < SYNC_MS_TIMEOUT_INCREMENT)
			nMsTimeoutIncrement = SYNC_MS_TIMEOUT_INCREMENT;
	}

	// la chiamata a EnterCriticalSection() blocca l'esecuzione fino all'acquisizione della sezione critica,
	// ma qui deve usare un meccanismo che permetta l'uso di un timeout se la sezione critica e' gia' in uso
	// la zoccola di Windows9.x non ha la TryEnter...(), per cui deve rimappare sulla Enter...(), perdendo in
	// tal caso l'opzione per il timeout
	switch(m_osVersion)
	{
		case WINDOWS_NT:
		case WINDOWS_2000:
		case WINDOWS_XP:
		case WINDOWS_VISTA:
		case WINDOWS_SEVEN:
		{
			// usa la TryEnterCriticalSection() implementando tentativi successivi via timeout
			do {
				// prova a bloccare, se fallisce..., riprova
				if((bLocked = ::TryEnterCriticalSection(&m_CriticalSection))==FALSE)
				{
					if(nTimeout!=SYNC_INFINITE_TIMEOUT)
					{
						::Sleep(nMsTimeoutIncrement);
						nElapsed += nMsTimeoutIncrement;
						if(nElapsed >= nTimeout)
							break;
					}
				}

				// se invece riesce, incrementa il contatore d'uso
				if(bLocked)
				{
					InterlockedIncrement(&m_nCounter);
					TRACEEXPR((_TRACE_FLAG_INFO,__NOFILE__,__NOLINE__,(char*)"CSyncThreads::Lock(): [%s] counter %ld\n",m_szName[0]!='\0' ? m_szName : "NONAME",m_nCounter));
				}
			} while(!bLocked);
		}
		break;
		
		default:
		{
			// e' costretto a usare la EnterCriticalSection(), bloccante, perde quindi la 
			// possibilita' di aspettare via timeout
			::EnterCriticalSection(&m_CriticalSection);
			bLocked = TRUE;
			InterlockedIncrement(&m_nCounter);
			TRACEEXPR((_TRACE_FLAG_INFO,__NOFILE__,__NOLINE__,(char*)"CSyncThreads::Lock(): [%s] counter %ld\n",m_szName[0]!='\0' ? m_szName : "NONAME",m_nCounter));
		}
		break;
	}

	TRACEEXPR((_TRACE_FLAG_INFO,__NOFILE__,__NOLINE__,(char*)"CSyncThreads::Lock(): [%s] lock %s\n",m_szName[0]!='\0' ? m_szName : "NONAME",bLocked ? "succeed" : "FAILED"));

	// solo se in modo DEBUG: se il lock fallisce, chiede conferma e riprova in extremis, ricorsivamente
#ifdef _CSYNC_VERBOSE
	if(!bLocked)
	{
		char buffer[512] = {0};
		snprintf(buffer,sizeof(buffer),"[%s]: lock failed, try again ?",m_szName[0]!='\0' ? m_szName : "NONAME");
		if(::MessageBox(NULL,buffer,"CSyncThreads::Lock()",MB_YESNO|MB_ICONWARNING|MB_TASKMODAL|MB_SETFOREGROUND|MB_TOPMOST)==IDYES)
			return(this->Lock(nTimeout));
	}
#endif

	return(bLocked);
}

/*
	Unlock()
*/
BOOL CSyncThreads::Unlock(void)
{
	// decrementa il contatore d'uso
	InterlockedDecrement(&m_nCounter);
	TRACEEXPR((_TRACE_FLAG_INFO,__NOFILE__,__NOLINE__,(char*)"CSyncThreads::Unlock(): [%s] counter %ld\n",m_szName[0]!='\0' ? m_szName : "NONAME",m_nCounter));
	::LeaveCriticalSection(&m_CriticalSection);

	return(TRUE);
}

/*
	SetTimeout()
*/
void CSyncThreads::SetTimeout(int nTimeout)
{
	m_nTimeout = nTimeout;
	if(m_nTimeout <= 0)
		m_nTimeout = SYNC_DEFAULT_TIMEOUT;
}

/*
	SetName()
*/
BOOL CSyncThreads::SetName(LPCSTR lpcszName)
{
	ASSERTEXPR(lpcszName);

	BOOL bSet = FALSE;

	// imposta solo se non impostato gia'
	if(lpcszName && m_szName[0]=='\0')
	{
		for(int i = 0; *lpcszName && i < sizeof(m_szName)-1;)
		{
			if(isalnum(*lpcszName) || *lpcszName=='_')
				m_szName[i++] = *lpcszName;
			lpcszName++;
		}
		bSet = TRUE;
	}

	return(bSet);
}

/*
	CSyncProcesses()
*/
CSyncProcesses::CSyncProcesses(LPCSTR lpcszName/* = NULL */,int nTimeout/* = 0 */)
{
	m_hHandle = NULL;
	m_nLockCount = 0L;
	
	m_nTimeout = nTimeout;
	if(m_nTimeout <= 0)
		m_nTimeout = SYNC_DEFAULT_TIMEOUT;

	memset(m_szName,'\0',sizeof(m_szName));
	if(lpcszName)
		SetName(lpcszName);

	memset(&m_csLockCount,'\0',sizeof(m_csLockCount));
	::InitializeCriticalSection(&m_csLockCount);
}

/*
	~CSyncProcesses()
*/
CSyncProcesses::~CSyncProcesses()
{
	// vedi le note nel dtor di CSyncThreads riguardo il rilascio degli oggetti
	::DeleteCriticalSection(&m_csLockCount);

	if(m_hHandle)
	{
		::ReleaseMutex(m_hHandle);
		::CloseHandle(m_hHandle);
	}
}

/*
	Lock()
*/
BOOL CSyncProcesses::Lock(int nTimeout/* = 0 */)
{
	int nMsTimeoutIncrement = SYNC_MS_TIMEOUT_INCREMENT;
	int nElapsed = 0;
	BOOL bLocked = FALSE;	
	DWORD dwRet = 0L;

	// nome mutex	
	if(m_szName[0]=='\0')
	{
		TRACEEXPR((_TRACE_FLAG_ERR,__NOFILE__,__NOLINE__,(char*)"CSyncProcesses::Lock(): invalid mutex name\n"));

#ifdef _CSYNC_VERBOSE
		::MessageBox(NULL,"Invalid mutex name.","CSyncProcesses::Lock()",MB_OK|MB_ICONERROR|MB_TASKMODAL|MB_SETFOREGROUND|MB_TOPMOST);
#endif

		return(bLocked);
	}

	// handle
	if(!m_hHandle)
	{
		TRACEEXPR((_TRACE_FLAG_ERR,__NOFILE__,__NOLINE__,(char*)"CSyncProcesses::Lock(): [%s] invalid mutex handle\n",m_szName));

#ifdef _CSYNC_VERBOSE
		::MessageBox(NULL,"Invalid mutex handle.","CSyncProcesses::Lock()",MB_OK|MB_ICONERROR|MB_TASKMODAL|MB_SETFOREGROUND|MB_TOPMOST);
#endif

		return(bLocked);
	}

	// valore timeout
	if(nTimeout==0)
	{
		if(m_nTimeout <= 0)
			m_nTimeout = SYNC_DEFAULT_TIMEOUT;
		nTimeout = m_nTimeout;
	}
	if(nTimeout!=SYNC_INFINITE_TIMEOUT)
	{
		nMsTimeoutIncrement = nTimeout / 10;
		if(nMsTimeoutIncrement < SYNC_MS_TIMEOUT_INCREMENT)
			nMsTimeoutIncrement = SYNC_MS_TIMEOUT_INCREMENT;
	}

	// acquisisce il blocco sul mutex tramite la WaitForSingleObject():
	// "The single-object wait functions return when the state of the specified object is signaled"
	// ossia lo segnala ("acquisisce" e "blocca") con la WaitForSingleObject(): tale funzione ritorna
	// quando l'oggetto e' stato segnalato o il timeout e' scaduto, la prima fra le due
	do {
		// se il mutex gia' stato segnalato da un altro thread, si mette in attesa (Sleep) fino a che 
		// si libera e poi prova a bloccarlo (WaitForSingleObject)
		if(m_nLockCount > 0L)
		{
			dwRet = WAIT_FAILED;
			::Sleep(nMsTimeoutIncrement);
		}
		else
			dwRet = ::WaitForSingleObject(m_hHandle,nMsTimeoutIncrement);

		if(dwRet==WAIT_FAILED || dwRet==WAIT_ABANDONED || dwRet==WAIT_TIMEOUT)	// segnalamento fallito, bloccato/timeout/etc.
			bLocked = FALSE;
		else if(dwRet==WAIT_OBJECT_0) // segnalamento riuscito, ossia e' riuscito a bloccare il mutex
			bLocked = TRUE;

		if(bLocked)
		{
			::EnterCriticalSection(&m_csLockCount);
			m_nLockCount++;
			TRACEEXPR((_TRACE_FLAG_INFO,__NOFILE__,__NOLINE__,(char*)"CSyncProcesses::Lock(): [%s] lock succeed (%ld)\n",m_szName,m_nLockCount));
			::LeaveCriticalSection(&m_csLockCount);
		}
		else
		{
			// lock fallito, decrementa a intervalli, fino a raggiungere il max stabilito per il timeout, e riprova
			TRACEEXPR((_TRACE_FLAG_INFO,__NOFILE__,__NOLINE__,(char*)"CSyncProcesses::Lock(): [%s] lock failed (%ld)\n",m_szName,m_nLockCount));
			if(nTimeout!=SYNC_INFINITE_TIMEOUT)
			{
				nElapsed += nMsTimeoutIncrement;
				if(nElapsed >= nTimeout)
					break;
			}
			else
			{
				::Sleep(nMsTimeoutIncrement);
			}
		}
	} while(!bLocked);

	// solo se in modo DEBUG: se il lock fallisce, chiede conferma e riprova in extremis, ricorsivamente
#ifdef _CSYNC_VERBOSE
	if(!bLocked)
	{
		char buffer[512];
		snprintf(buffer,sizeof(buffer),"%s: lock failed, try again ?",m_szName);
		if(::MessageBox(NULL,buffer,"CSyncProcesses::Lock()",MB_YESNO|MB_ICONWARNING|MB_TASKMODAL|MB_SETFOREGROUND|MB_TOPMOST)==IDYES)
			return(this->Lock(nTimeout));
	}
#endif

	return(bLocked);
}

/*
	Unlock()
*/
BOOL CSyncProcesses::Unlock(void)
{
	::EnterCriticalSection(&m_csLockCount);
	if(m_nLockCount > 0L)
		m_nLockCount--;
	TRACEEXPR((_TRACE_FLAG_ERR,__NOFILE__,__NOLINE__,(char*)"CSyncProcesses::Unlock(): [%s] unlocked (%ld)\n",m_szName,m_nLockCount));
	::LeaveCriticalSection(&m_csLockCount);

	::ReleaseMutex(m_hHandle);

	return(TRUE);
}

/*
	SetTimeout()
*/
void CSyncProcesses::SetTimeout(int nTimeout)
{
	m_nTimeout = nTimeout;
	if(m_nTimeout <= 0)
		m_nTimeout = SYNC_DEFAULT_TIMEOUT;
}

/*
	SetName()
*/
BOOL CSyncProcesses::SetName(LPCSTR lpcszName)
{
	ASSERTEXPR(lpcszName);

	BOOL bSet = FALSE;

	if(lpcszName && m_szName[0]=='\0')
	{
		for(int i=0; *lpcszName && i < sizeof(m_szName)-1;)
		{
			if(isalnum(*lpcszName) || *lpcszName=='_')
				m_szName[i++] = *lpcszName;
			lpcszName++;
		}

		if(!m_hHandle)
			m_hHandle = ::CreateMutex(NULL,FALSE,m_szName);

		bSet = TRUE;
	}

	return(bSet);
}
