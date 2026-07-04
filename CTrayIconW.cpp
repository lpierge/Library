/*
	CTrayIconW.cpp
	Classe per la gestione dell'icona nel system tray (MFC).
	Versione aggiornata dell'originale (CTrayIcon.cpp) per poter usare caratteri Wide.
	(Unicode esplicito)
	Luca Piergentili, 07/06/26

	Vedi le note in CTrayIconW.h.
*/
#include "pragma.h"
#include "env.h"
#include "macro.h"
#include <string.h>
#include "strings.h"
#include "window.h"
#include <wchar.h>
#include <afxtempl.h>
#include <winuser.h>
#include <afxdisp.h>
#include "CTrayIconW.h"
#include "CTrayIconPosition.h"
#include "CPPTooltipW.h"
#include "CFilenameFactory.h"

#include "traceexpr.h"
//#define _TRACE_FLAG			_TRFLAG_TRACEOUTPUT // opzioni: _TRFLAG_NOTRACE, _TRFLAG_TRACEFILE, _TRFLAG_TRACECONSOLE, _TRFLAG_TRACEOUTPUT, _TRFLAG_TRACEBREAKPOINT
#define _TRACE_FLAG			_TRFLAG_NOTRACE // opzioni: _TRFLAG_NOTRACE, _TRFLAG_TRACEFILE, _TRFLAG_TRACECONSOLE, _TRFLAG_TRACEOUTPUT, _TRFLAG_TRACEBREAKPOINT
#define _TRACE_FLAG_INFO	_TRACE_FLAG
#define _TRACE_FLAG_WARN	_TRACE_FLAG
#define _TRACE_FLAG_ERR		_TRACE_FLAG

BEGIN_MESSAGE_MAP(CTrayIconW,CWnd)
	ON_WM_DESTROY()
END_MESSAGE_MAP()

/*
	CTrayIconW()
*/
CTrayIconW::CTrayIconW()
{
	m_bIsValid = FALSE;
	memset(&m_NotifyIconDataW, 0, sizeof(m_NotifyIconDataW));
	m_NotifyIconDataW.cbSize = sizeof(NOTIFYICONDATAW);
	m_hWndParent = (HWND)NULL;
	m_hIcon = (HICON)NULL;
	m_hBalloonIcon = (HICON)NULL;
	m_bSharedIcon = TRUE;
	m_nMenuId = (UINT)-1;
	m_nBlankIconId = (UINT)-1L;
	memset(m_szToolTipTextW,'\0',sizeof(m_szToolTipTextW));
	m_nTimeout = m_nTimerId = 0L;
	m_nBalloonType = (m_winVer.GetCommonControlsVer() >= PACKVERSION(5,0)) ? BALLOON_USE_NATIVE : BALLOON_USE_EXTENDED;
	m_nCloseIconID = 0;
	memset(m_szCssStylesW,'\0',sizeof(m_szCssStylesW));
	m_nWrap = 50;
	m_hWndClickNotify = NULL;
	m_nMsgClickNotify = 0;
}

/*
	~CTrayIconW()
*/
CTrayIconW::~CTrayIconW()
{
	// elimina il timer
	if(m_nTimerId!=0L)
	{
		::KillTimer(AfxGetMainWnd()->m_hWnd,m_nTimerId);
		m_nTimerId = 0L;
	}

	// chiude l'handle solo se non e' in share
	if(m_hBalloonIcon!=(HICON)NULL && !m_bSharedIcon)
		::DestroyIcon(m_hBalloonIcon);

	if(m_TrayTooltip.GetSafeHwnd())
		m_TrayTooltip.DestroyWindow();

	// rimuove l'icona dalla tray area
	Destroy();

	// per la derivazione da CWnd
	if(m_hWnd)
		DestroyWindow();
}

/*
	OnDestroy()
*/
void CTrayIconW::OnDestroy(void)
{
	CWnd::OnDestroy();
}

/*
	Create()
*/
BOOL CTrayIconW::Create(HWND hWnd,HICON hIcon,UINT nMsg,UINT nMenuId,UINT nBlankIconId/* = (UINT)-1L*/,LPCWSTR lpcwszTooltip/* = NULL*/)
{
	Destroy();

	if(!m_bIsValid)
	{
		// inizializza la struttura per l'icona nella tray area
		memset(&m_NotifyIconDataW,'\0',sizeof(m_NotifyIconDataW));
		m_NotifyIconDataW.cbSize           = sizeof(NOTIFYICONDATAW);
		m_NotifyIconDataW.hWnd             = m_hWndParent = hWnd;
		m_NotifyIconDataW.uID              = m_nMenuId = nMenuId;
		m_NotifyIconDataW.uFlags           = NIF_MESSAGE|NIF_ICON|NIF_TIP;
		m_NotifyIconDataW.uCallbackMessage = nMsg;
		m_NotifyIconDataW.hIcon            = m_hIcon = hIcon;

		// se win >= 2k inizializza anche per il balloon nativo
		if(m_winVer.GetCommonControlsVer() >= PACKVERSION(5,0))
		{
			m_NotifyIconDataW.uFlags |= NIF_INFO;
			m_NotifyIconDataW.szInfoTitle[0] = L'\0';
			m_NotifyIconDataW.szInfo[0] = L'\0';
			m_NotifyIconDataW.uTimeout = BALLOON_DEFAULT_TIMEOUT * 1000L;
			m_NotifyIconDataW.dwInfoFlags = NIIF_NONE;
		}
		
		// imposta il testo del tooltip
		if(lpcwszTooltip)
		{
			// wcsncpy usa il conteggio dei caratteri (sizeof / sizeof(wchar_t))
			wcsncpy(m_szToolTipTextW,lpcwszTooltip,sizeof(m_szToolTipTextW) / sizeof(wchar_t));
			m_szToolTipTextW[(sizeof(m_szToolTipTextW) / sizeof(wchar_t)) - 1] = L'\0';
			
			wcsncpy(m_NotifyIconDataW.szTip,m_szToolTipTextW,sizeof(m_NotifyIconDataW.szTip) / sizeof(wchar_t));
			m_NotifyIconDataW.szTip[(sizeof(m_NotifyIconDataW.szTip) / sizeof(wchar_t)) - 1] = L'\0';
		}
		else
		{
			memset(m_szToolTipTextW,'\0',sizeof(m_szToolTipTextW));
			memset(m_NotifyIconDataW.szTip,'\0',sizeof(m_NotifyIconDataW.szTip));
		}

		// crea l'icona nella tray area usando l'API Unicode esplicitamente
		m_bIsValid = ::Shell_NotifyIconW(NIM_ADD,&m_NotifyIconDataW);

		// se viene specificato l'identificatore, imposta per il balloon esteso
		if((m_nBlankIconId = nBlankIconId)!=(UINT)-1L)
		{
			// inizializza per ricavare in seguito la posizione dell'icona
			m_TrayPosition.InitializeTracking(hWnd,nMenuId,nBlankIconId);

			// crea il balloon (esteso)
			m_TrayTooltip.Create(hWnd);
			m_TrayTooltip.SetSize(CPPToolTipW::PPTTSZ_MARGIN_CX,4);
			m_TrayTooltip.SetSize(CPPToolTipW::PPTTSZ_MARGIN_CY,4);
		}
	}

	return(m_bIsValid);
}

/*
	Destroy()
*/
BOOL CTrayIconW::Destroy(void)
{ 
	if(m_bIsValid)
	{
		if(m_hWndParent!=(HWND)NULL && m_nMenuId!=(UINT)-1)
		{
			// rimuove l'icona dalla tray area
			m_NotifyIconDataW.cbSize = sizeof(NOTIFYICONDATAW);
			m_NotifyIconDataW.hWnd   = m_hWndParent;
			m_NotifyIconDataW.uID    = m_nMenuId;
			m_bIsValid = !::Shell_NotifyIconW(NIM_DELETE,&m_NotifyIconDataW);
			memset(&m_NotifyIconDataW,'\0',sizeof(m_NotifyIconDataW));
		}
	}

	return(!m_bIsValid);
}

/*
	SetIcon()
*/
BOOL CTrayIconW::SetIcon(HICON hIcon)
{
	BOOL bSet = FALSE;

	if(m_bIsValid)
	{
		if(m_hWndParent!=(HWND)NULL && m_nMenuId!=(UINT)-1)
		{
			// imposta l'icona per la tray area in base all'handle
			m_NotifyIconDataW.uFlags = NIF_ICON;
			m_NotifyIconDataW.hIcon = m_hIcon = hIcon;
			bSet = ::Shell_NotifyIconW(NIM_MODIFY,&m_NotifyIconDataW);
		}
	}

	return(bSet);
}

/*
	SetIcon()
*/
BOOL CTrayIconW::SetIcon(UINT nIconId)
{
	BOOL bSet = FALSE;

	if(m_bIsValid)
	{
		if(m_hWndParent!=(HWND)NULL && m_nMenuId!=(UINT)-1)
		{
			// imposta l'icona per la tray area in base all'id risorsa
			HICON hIcon;
			if((hIcon = (HICON)::LoadImage(AfxGetInstanceHandle(),MAKEINTRESOURCE(nIconId),IMAGE_ICON,16,16,LR_DEFAULTCOLOR))!=(HICON)NULL)
			{
				m_NotifyIconDataW.uFlags = NIF_ICON;
				m_NotifyIconDataW.hIcon = m_hIcon = hIcon;
				bSet = ::Shell_NotifyIconW(NIM_MODIFY,&m_NotifyIconDataW);
			}
		}
	}

	return(bSet);
}

/*
	SetToolTip()
*/
BOOL CTrayIconW::SetToolTip(LPCWSTR lpcwszTooltip/* = NULL*/)
{
	BOOL bTooltip = FALSE;

	if(m_bIsValid)
	{
		if(m_hWndParent!=(HWND)NULL && m_nMenuId!=(UINT)-1)
		{
			// imposta il testo per il tooltip, quello classico, non il balloon
			m_NotifyIconDataW.uFlags = NIF_TIP;
			
			if(lpcwszTooltip && wcscmp(lpcwszTooltip,L"")!=0)
			{
				// wcsncpy usa il conteggio dei caratteri (sizeof / sizeof(wchar_t))
				wcsncpy(m_szToolTipTextW,lpcwszTooltip,sizeof(m_szToolTipTextW) / sizeof(wchar_t));
				m_szToolTipTextW[(sizeof(m_szToolTipTextW) / sizeof(wchar_t)) - 1] = L'\0';
				
				// se il testo sfora, sostituisce la fine con [...] (solo se non e' gia' stato accorciato...)
				if(wcslen(lpcwszTooltip) >= TRAYICON_MAX_TOOLTIP_TEXT && !wcsstr(lpcwszTooltip,ABBREVIATE_DEFAULT_STRINGW))
				{
					CFilenameFactory fn;
					wcsncpy(m_szToolTipTextW, fn.AbbreviateW(lpcwszTooltip,TRAYICON_MAX_TOOLTIP_TEXT-1),sizeof(m_szToolTipTextW) / sizeof(wchar_t));
					m_szToolTipTextW[(sizeof(m_szToolTipTextW) / sizeof(wchar_t)) - 1] = L'\0';
				}

				wcsncpy(m_NotifyIconDataW.szTip,m_szToolTipTextW,sizeof(m_NotifyIconDataW.szTip) / sizeof(wchar_t));
				m_NotifyIconDataW.szTip[(sizeof(m_NotifyIconDataW.szTip) / sizeof(wchar_t)) - 1] = L'\0';
			}
			else
			{
				memset(m_szToolTipTextW,'\0',sizeof(m_szToolTipTextW));
				memset(m_NotifyIconDataW.szTip,'\0',sizeof(m_NotifyIconDataW.szTip));
			}
			
			bTooltip = ::Shell_NotifyIconW(NIM_MODIFY,&m_NotifyIconDataW);
		}
	}

	return(bTooltip);
}

/*
	SetCssStyles()
*/
void CTrayIconW::SetCssStyles(LPCWSTR lpcwszCssStyles)
{
	wcsncpy(m_szCssStylesW,lpcwszCssStyles,sizeof(m_szCssStylesW) / sizeof(wchar_t));
	m_szCssStylesW[(sizeof(m_szCssStylesW) / sizeof(wchar_t)) - 1] = L'\0';
}

/*
	BalloonW()
*/
void CTrayIconW::Balloon(	LPCWSTR	lpcwszTitle			/* = NULL*/,					// titolo
							LPCWSTR	lpcwszText			/* = NULL*/,					// testo
							UINT	nIconType			/* = MB_ICONINFORMATION*/,		// icona (nativo: info, warning, errore, esteso: custom)
							UINT	nTimeout			/* = BALLOON_DEFAULT_TIMEOUT*/,	// timeout per rimozione
							UINT	nIconID				/* = (UINT)-1L*/,				// handle x icona custom (esclude il nome file), solo per esteso
							LPCWSTR	lpcwszIconFileName	/* = NULL*/,					// nome file x icona custom (esclude l'handle), solo per esteso
							CSize	iconSize			/* = CSize(16,16)*/,			// dimensione icona custom, solo per esteso
							UINT	nCloseIconID		/* = (UINT)-1L*/				// id risorsa per icona chiusura, solo per esteso
							)
{
	// controlla se deve visualizzare o rimuovere il balloon
	BOOL bRemoveBalloon = !lpcwszTitle || !lpcwszText;

	// il balloon (nativo) richiede win >= 2K
	if(m_nBalloonType==BALLOON_USE_NATIVE)
	{
		// imposta il titolo
		wchar_t szTitle[BALLOON_MAX_TOOLTIP_TITLE+1] = {0};
		if(lpcwszTitle)
		{
			wcsncpy(szTitle,lpcwszTitle,sizeof(szTitle) / sizeof(wchar_t));
			szTitle[(sizeof(szTitle) / sizeof(wchar_t)) - 1] = L'\0';
			
			// se il testo sfora, sostituisce la fine con [...] (solo se non e' gia' stato accorciato...)
			if(wcslen(lpcwszTitle) >= BALLOON_MAX_TOOLTIP_TITLE && !wcsstr(lpcwszTitle,ABBREVIATE_DEFAULT_STRINGW))
			{
				CFilenameFactory fn;
				wcsncpy(szTitle,fn.AbbreviateW(lpcwszTitle,BALLOON_MAX_TOOLTIP_TITLE-1),sizeof(szTitle) / sizeof(wchar_t));
				szTitle[(sizeof(szTitle) / sizeof(wchar_t)) - 1] = L'\0';
			}
		}
		
		// imposta il testo
		wchar_t szText[BALLOON_MAX_TOOLTIP_TEXT+1] = {0};
		if(lpcwszText)
		{
			wcsncpy(szText,lpcwszText,sizeof(szText) / sizeof(wchar_t));
			szText[(sizeof(szText) / sizeof(wchar_t)) - 1] = L'\0';
		}

		// imposta il tipo di icona da utilizzare
		DWORD dwIconType = NIIF_NONE;
		if(!bRemoveBalloon)
		{
			switch(nIconType) {
				case MB_ICONINFORMATION:
					dwIconType = NIIF_INFO;
					break;
				case MB_ICONWARNING:
					dwIconType = NIIF_WARNING;
					break;
				case MB_ICONERROR:
					dwIconType = NIIF_ERROR;
					break;
			}
		}

		// ricava il valore per il timeout per la rimozione del balloon
		if(!bRemoveBalloon)
		{
			nTimeout = (nTimeout >= 1 && nTimeout <= 30) ? nTimeout : BALLOON_DEFAULT_TIMEOUT;
			nTimeout *= 1000L;
		}
		else
			nTimeout = 0;
		m_nTimeout = nTimeout;

		// imposta il timer per la rimozione del balloon
		if(!bRemoveBalloon)
			if(nTimeout!=0)
			{
				if(m_nTimerId!=0L)
				{
					::KillTimer(AfxGetMainWnd()->m_hWnd,m_nTimerId);
					m_nTimerId = 0L;
				}
				
				m_nTimerId = ::SetTimer(AfxGetMainWnd()->m_hWnd,(UINT)this,m_nTimeout,(TIMERPROC)BalloonTimerProc);
			}

		// notifica al sistema i cambi relativi al balloon
		m_NotifyIconDataW.uFlags = NIF_INFO;
		if(!bRemoveBalloon)
		{
			wcsncpy(m_NotifyIconDataW.szInfoTitle,szTitle,(sizeof(m_NotifyIconDataW.szInfoTitle) / sizeof(wchar_t)) - 1);
			m_NotifyIconDataW.szInfoTitle[(sizeof(m_NotifyIconDataW.szInfoTitle) / sizeof(wchar_t)) - 1] = L'\0';

			wcsncpy(m_NotifyIconDataW.szInfo,szText,(sizeof(m_NotifyIconDataW.szInfo) / sizeof(wchar_t)) - 1);
			m_NotifyIconDataW.szInfo[(sizeof(m_NotifyIconDataW.szInfo) / sizeof(wchar_t)) - 1] = L'\0';
		}
		else
		{
			memset(m_NotifyIconDataW.szInfoTitle,'\0',sizeof(m_NotifyIconDataW.szInfoTitle));
			memset(m_NotifyIconDataW.szInfo,'\0',sizeof(m_NotifyIconDataW.szInfo));
		}
		m_NotifyIconDataW.dwInfoFlags = dwIconType;
		m_NotifyIconDataW.uTimeout = nTimeout;
		::Shell_NotifyIconW(NIM_MODIFY,&m_NotifyIconDataW);
		
		// in modo che le notifiche successive non considerino il balloon
		m_NotifyIconDataW.szInfoTitle[0] = L'\0';
		m_NotifyIconDataW.szInfo[0] = L'\0';
	}
	else if(m_nBalloonType==BALLOON_USE_EXTENDED)
	{
		// ricava le coordinate a cui visualizzare
		CPoint ptPosition;
		m_TrayPosition.GetPosition(ptPosition);

		// ricava il timeout per la rimozione del balloon
		if(!bRemoveBalloon)
		{
			nTimeout = (nTimeout >= 1 && nTimeout <= 30) ? nTimeout : BALLOON_DEFAULT_TIMEOUT;
			nTimeout *= 1000L;
		}
		else
			nTimeout = 0;
		m_nTimeout = nTimeout;

		if(!bRemoveBalloon)
		{
			// imposta il timer per la rimozione del balloon
			if(nTimeout!=0)
			{
				if(m_nTimerId!=0L)
				{
					::KillTimer(AfxGetMainWnd()->m_hWnd,m_nTimerId);
					m_nTimerId = 0L;
				}
				
				m_nTimerId = ::SetTimer(AfxGetMainWnd()->m_hWnd,(UINT)this,m_nTimeout,(TIMERPROC)BalloonTimerProc);
			}

			// imposta lo stile
			// riconverte ad ANSI, dato che la CPPToolTip li' non supporta W
#if 0
			if(m_szCssStylesW[0] != '\0')
			{
				LPSTR pCssStyle = WideCharToAnsi(m_szCssStylesW,CP_UTF8);
				if(pCssStyle)
				{
					m_TrayTooltip.SetCssStyles(pCssStyle);
					free(pCssStyle);
				}
			}
#else
			if(m_szCssStylesW[0] != '\0')
				m_TrayTooltip.SetCssStyles(m_szCssStylesW);
#endif

			// formatta il testo
			wchar_t szText[1024] = {0};
			if(nCloseIconID!=(UINT)-1L)
				m_nCloseIconID = nCloseIconID;
			
#if 0
			// non wrappa il testo, la SetMaxTipWidth() non funziona
			_snwprintf(szText,
					sizeof(szText) / sizeof(wchar_t),
					L"<table>"
					L"<tr>"
					L"<td align=left><h4>%s</h4></td>"
					L"<td align=right><a><icon idres=%d width=16 height=16 style=g hotstyle></a></td>"
					L"</tr>"
					L"</table><hr><br>"
					L"%s",
					lpcwszTitle,
					m_nCloseIconID,
					lpcwszText);
			szText[(sizeof(szText) / sizeof(wchar_t)) - 1] = L'\0';
#else
			// la SetMaxTipWidth() della versione di CPPToolTip usata qui solo dimensiona il balloon ma non
			// wrappa il testo, inserisce quindi dei <br> nel testo originale prima della formattazione

			// lunghezza del testo originale
			size_t nOrigLen = wcslen(lpcwszText);

			// alloca il buffer per modificare il testo
			// ogni inserimento di "<br>" aggiunge 4 caratteri, qui stima un margine di 256 caratteri
			wchar_t* wszTextWrapped = (wchar_t*)_alloca((nOrigLen + 256) * sizeof(wchar_t));

			size_t nLineCounter = 0;
			size_t nDestIndex = 0;
			for(size_t i=0; i < nOrigLen; i++) 
			{
				wszTextWrapped[nDestIndex++] = lpcwszText[i];
				nLineCounter++;

				// se raggiunge i <m_nWrap> caratteri ed incontra uno spazio, spezza la riga
				if(nLineCounter >= (size_t)m_nWrap && lpcwszText[i]==L' ') 
				{
					// sovrascrive lo spazio (o si accoda) inserendo "<br>"
					// torna indietro di 1 per sostituire lo spazio con il tag HTML
					nDestIndex--;
					wszTextWrapped[nDestIndex++] = L'<';
					wszTextWrapped[nDestIndex++] = L'b';
					wszTextWrapped[nDestIndex++] = L'r';
					wszTextWrapped[nDestIndex++] = L'>';
        
					nLineCounter = 0;
				}
			}
			wszTextWrapped[nDestIndex] = L'\0';

			// formatta il tutto con il buffer modificato sopra
			_snwprintf(szText,
				sizeof(szText) / sizeof(wchar_t),
				L"<table>"
				L"<tr>"
				L"<td align=left><h4>%s</h4></td>"
				L"<td align=right><a><icon idres=%d width=16 height=16 style=g hotstyle></a></td>"
				L"</tr>"
				L"</table>"
				L"<hr><br>"
				L"%s", // testo con i <br> iniettati
				lpcwszTitle,
				m_nCloseIconID,
				wszTextWrapped);
			szText[(sizeof(szText) / sizeof(wchar_t)) - 1] = L'\0';
#endif
			// visualizza il tooltip
			if(nIconID!=(UINT)-1L)
			{
				// carica l'icona dall'handle
				m_TrayTooltip.SetEffectBk(CPPDrawManagerW::EFFECT_SOLID,0);
				m_TrayTooltip.SetDelayTime(PPTOOLTIP_TIME_AUTOPOP,1000);
				m_TrayTooltip.SetBehaviour(PPTOOLTIP_CLOSE_LEAVEWND);
				if(m_hWndClickNotify!=NULL && m_nMsgClickNotify!=0)
					m_TrayTooltip.SetOnBalloonClick(m_hWndClickNotify,m_nMsgClickNotify);
				m_TrayTooltip.ShowHelpTooltip(&ptPosition,szText,nIconID,iconSize);
			}
			else
			{
				// chiude l'handle solo se non e' in share
				if(m_hBalloonIcon!=(HICON)NULL && !m_bSharedIcon)
					::DestroyIcon(m_hBalloonIcon);
				m_hBalloonIcon = (HICON)NULL;
				
				// carica l'icona dal file
				if(*lpcwszIconFileName)
				{
					m_hBalloonIcon = (HICON)::LoadImageW(NULL,lpcwszIconFileName,IMAGE_ICON,iconSize.cx,iconSize.cy,LR_DEFAULTCOLOR|LR_LOADFROMFILE);
					m_bSharedIcon = FALSE;
				}
				else // carica l'icona predefinita
				{
					switch(nIconType) {
						case MB_ICONERROR:
							m_hBalloonIcon = ::LoadIcon(NULL,IDI_ERROR);
							break;
						case MB_ICONWARNING:
							m_hBalloonIcon = ::LoadIcon(NULL,IDI_WARNING);
							break;
						case MB_ICONINFORMATION:
						default:
							m_hBalloonIcon = ::LoadIcon(NULL,IDI_INFORMATION);
							break;
					}
					
					m_bSharedIcon = TRUE;
				}

				if(m_hBalloonIcon)
				{
					m_TrayTooltip.SetEffectBk(CPPDrawManagerW::EFFECT_SOLID,0);
					m_TrayTooltip.SetDelayTime(PPTOOLTIP_TIME_AUTOPOP,1000);
					m_TrayTooltip.SetBehaviour(PPTOOLTIP_CLOSE_LEAVEWND);
					if(m_hWndClickNotify!=NULL && m_nMsgClickNotify!=0)
						m_TrayTooltip.SetOnBalloonClick(m_hWndClickNotify,m_nMsgClickNotify);
					m_TrayTooltip.ShowHelpTooltip(&ptPosition,szText,m_hBalloonIcon);
				}
			}
		}
	}
}

/*
	BalloonTimerProc()
*/
VOID CALLBACK CTrayIconW::BalloonTimerProc(HWND /*hWnd*/,UINT /*uMsg*/,UINT_PTR idEvent,DWORD /*dwTime*/)
{
	CTrayIconW* pTrayIcon = (CTrayIconW*)idEvent;	
	if(pTrayIcon)
	{
		// rimuove il timer
		if(pTrayIcon->m_nTimerId!=0L)
		{
			::KillTimer(AfxGetMainWnd()->m_hWnd,pTrayIcon->m_nTimerId);
			pTrayIcon->m_nTimerId = 0L;
		}

		// rimuove il tooltip
		if(pTrayIcon->m_nBalloonType==BALLOON_USE_NATIVE)
			pTrayIcon->Balloon();
		else if(pTrayIcon->m_nBalloonType==BALLOON_USE_EXTENDED)
			pTrayIcon->m_TrayTooltip.HideTooltip();
	}
}
