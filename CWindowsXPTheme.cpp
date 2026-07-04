/*
	CWindowsXPTheme.cpp
	Classe di interfaccia per i temi di Windows XP.
	Il codice relativo all'interfaccia con l'API vera e propria e'
	stato ripreso e modificato dall'originale di David Yuheng Zhao.
	Luca Piergentili, 14/09/03
	lpiergentili@yahoo.com
*/
#include "pragma.h"
#include "env.h"
#include "macro.h"
#include "window.h"
#include "CSync.h"
#include "CWindowsXPTheme.h"

#include "traceexpr.h"
//#define _TRACE_FLAG			_TRFLAG_TRACEOUTPUT // opzioni: _TRFLAG_NOTRACE, _TRFLAG_TRACEFILE, _TRFLAG_TRACECONSOLE, _TRFLAG_TRACEOUTPUT, _TRFLAG_TRACEBREAKPOINT
#define _TRACE_FLAG			_TRFLAG_NOTRACE // opzioni: _TRFLAG_NOTRACE, _TRFLAG_TRACEFILE, _TRFLAG_TRACECONSOLE, _TRFLAG_TRACEOUTPUT, _TRFLAG_TRACEBREAKPOINT
#define _TRACE_FLAG_INFO	_TRACE_FLAG
#define _TRACE_FLAG_WARN	_TRACE_FLAG
#define _TRACE_FLAG_ERR		_TRACE_FLAG

// statiche per il modulo
static CSyncThreads m_csTheme(NULL,SYNC_INFINITE_TIMEOUT);

// statiche per la classe (comuni a tutte le istanze della classe)
// il contatore per le referenze viene usato per sapere quando caricare/scaricare la dll
int		CWindowsXPTheme::m_nRefCount = 0;
HMODULE	CWindowsXPTheme::m_hThemeDll = NULL;
int		CWindowsXPTheme::m_nIsThemingSupported = -1;

/*
	IsThemingSupported()
*/
BOOL CWindowsXPTheme::IsThemingSupported(void)
{
	// effettua il controllo solo alla prima chiamata, verificando l'avvenuto caricamento della DLL
	if(m_nIsThemingSupported==-1)
	{
		m_csTheme.Lock();
		m_nIsThemingSupported = (m_hThemeDll!=NULL) ? 1 : 0;
		m_csTheme.Unlock();
	}

	return(m_nIsThemingSupported==1);
}

/*
	IsWindowsClassicStyle()
*/
BOOL CWindowsXPTheme::IsWindowsClassicStyle(void)
{
	BOOL bIsWindowsClassicStyle = FALSE;

	// effettua il controllo solo alla prima chiamata, verificando l'avvenuto caricamento della dll
	if(m_nIsThemingSupported==-1)
	{
		m_csTheme.Lock();
		m_nIsThemingSupported = (m_hThemeDll!=NULL) ? 1 : 0;
		m_csTheme.Unlock();
	}

	if(m_nIsThemingSupported==1)
		bIsWindowsClassicStyle = !(IsThemeActive() && IsAppThemed());
	else
		bIsWindowsClassicStyle = FALSE;

	return(bIsWindowsClassicStyle);
}

/*
	CWindowsXPTheme()
*/
CWindowsXPTheme::CWindowsXPTheme()
{
	// carica la DLL solo alla prima chiamata (l'handle viene condiviso da tutte le istanze della classe)
	if(m_nRefCount++==0)
	{
		m_csTheme.Lock();
		m_hThemeDll = ::LoadLibrary("UxTheme.dll");
		m_csTheme.Unlock();
	}
}

/*
	~CWindowsXPTheme()
*/
CWindowsXPTheme::~CWindowsXPTheme()
{
	// scarica la DLL quando non esistono piu' istanze della classe
	if(--m_nRefCount==0)
	{
		m_csTheme.Lock();
		if(m_hThemeDll)
			::FreeLibrary(m_hThemeDll),m_hThemeDll = NULL;
		m_csTheme.Unlock();
	}
}

/*
	GetProc()
*/
void* CWindowsXPTheme::GetProc(LPCSTR szProc,void* pfnFail)
{
	void* pRet = pfnFail;
	if(m_hThemeDll)
		pRet = ::GetProcAddress(m_hThemeDll,szProc);
	return(pRet);
}
