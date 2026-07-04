/*
	CTrayIcon.cpp
	Classe per la gestione dell'icona nel system tray (MFC).
	Luca Piergentili, 22/08/01
	lpiergentili@yahoo.com

	Vedi le note in CTrayIcon.h.
*/
#include "pragma.h"
#include "env.h"
#include "macro.h"
#include <string.h>
#include "strings.h"
#include "window.h"
#include <afxtempl.h>
#include <winuser.h>
#include <afxdisp.h>
#include "CTrayIcon.h"
#include "CTrayIconPosition.h"
#include "CPPTooltip.h"
#include "CFilenameFactory.h"

#include "traceexpr.h"
//#define _TRACE_FLAG			_TRFLAG_TRACEOUTPUT // opzioni: _TRFLAG_NOTRACE, _TRFLAG_TRACEFILE, _TRFLAG_TRACECONSOLE, _TRFLAG_TRACEOUTPUT, _TRFLAG_TRACEBREAKPOINT
#define _TRACE_FLAG			_TRFLAG_NOTRACE // opzioni: _TRFLAG_NOTRACE, _TRFLAG_TRACEFILE, _TRFLAG_TRACECONSOLE, _TRFLAG_TRACEOUTPUT, _TRFLAG_TRACEBREAKPOINT
#define _TRACE_FLAG_INFO	_TRACE_FLAG
#define _TRACE_FLAG_WARN	_TRACE_FLAG
#define _TRACE_FLAG_ERR		_TRACE_FLAG

BEGIN_MESSAGE_MAP(CTrayIcon,CWnd)
	ON_WM_DESTROY()
END_MESSAGE_MAP()

/*
	CTrayIcon()
*/
CTrayIcon::CTrayIcon()
{
	m_bIsValid = FALSE;
	memset(&m_NotifyIconData,'\0',sizeof(m_NotifyIconData));
	m_hWndParent = (HWND)NULL;
	m_hIcon = (HICON)NULL;
	m_hBalloonIcon = (HICON)NULL;
	m_bSharedIcon = TRUE;
	m_nMenuId = (UINT)-1;
	m_nBlankIconId = (UINT)-1L;
	memset(m_szToolTipText,'\0',sizeof(m_szToolTipText));
	m_nTimeout = m_nTimerId = 0L;
	m_nBalloonType = (m_winVer.GetCommonControlsVer() >= PACKVERSION(5,0)) ? BALLOON_USE_NATIVE : BALLOON_USE_EXTENDED;
	m_nCloseIconID = 0;
	memset(m_szCssStyles,'\0',sizeof(m_szCssStyles));
}

/*
	~CTrayIcon()
*/
CTrayIcon::~CTrayIcon()
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
void CTrayIcon::OnDestroy(void)
{
	CWnd::OnDestroy();
}

/*
	Create()
*/
BOOL CTrayIcon::Create(HWND hWnd,HICON hIcon,UINT nMsg,UINT nMenuId,UINT nBlankIconId/* = (UINT)-1L*/,LPCSTR lpcszTooltip/* = NULL*/)
{
	Destroy();

	if(!m_bIsValid)
	{
		// inizializza la struttura per l'icona nella tray area
		memset(&m_NotifyIconData,'\0',sizeof(m_NotifyIconData));
		m_NotifyIconData.cbSize           = sizeof(NOTIFYICONDATA);
		m_NotifyIconData.hWnd             = m_hWndParent = hWnd;
		m_NotifyIconData.uID              = m_nMenuId = nMenuId;
		m_NotifyIconData.uFlags           = NIF_MESSAGE|NIF_ICON|NIF_TIP;
		m_NotifyIconData.uCallbackMessage = nMsg;
		m_NotifyIconData.hIcon            = m_hIcon = hIcon;

		// se win >= 2k inizializza anche per il balloon nativo
		if(m_winVer.GetCommonControlsVer() >= PACKVERSION(5,0))
		{
			m_NotifyIconData.uFlags |= NIF_INFO;
			m_NotifyIconData.szInfoTitle[0] = '\0';
			m_NotifyIconData.szInfo[0] = '\0';
			m_NotifyIconData.uTimeout = BALLOON_DEFAULT_TIMEOUT * 1000L;
			m_NotifyIconData.dwInfoFlags = NIIF_NONE;
		}
		
		// imposta il testo del tooltip
		if(lpcszTooltip)
		{
			strcpyn(m_szToolTipText,lpcszTooltip,sizeof(m_szToolTipText));
			strcpyn(m_NotifyIconData.szTip,m_szToolTipText,sizeof(m_NotifyIconData.szTip));
		}
		else
		{
			memset(m_szToolTipText,'\0',sizeof(m_szToolTipText));
			memset(m_NotifyIconData.szTip,'\0',sizeof(m_NotifyIconData.szTip));
		}

		// crea l'icona nella tray area
		m_bIsValid = ::Shell_NotifyIcon(NIM_ADD,&m_NotifyIconData);

		// se viene specificato l'identificatore, imposta per il balloon esteso
		if((m_nBlankIconId = nBlankIconId)!=(UINT)-1L)
		{
			// inizializza per ricavare in seguito la posizione dell'icona
			m_TrayPosition.InitializeTracking(hWnd,nMenuId,nBlankIconId);

			// crea il balloon (esteso)
			m_TrayTooltip.Create(hWnd);
			m_TrayTooltip.SetSize(CPPToolTip::PPTTSZ_MARGIN_CX,4);
			m_TrayTooltip.SetSize(CPPToolTip::PPTTSZ_MARGIN_CY,4);
		}
	}

	return(m_bIsValid);
}

/*
	Destroy()
*/
BOOL CTrayIcon::Destroy(void)
{ 
	if(m_bIsValid)
	{
		if(m_hWndParent!=(HWND)NULL && m_nMenuId!=(UINT)-1)
		{
			// rimuove l'icona dalla tray area
			m_NotifyIconData.cbSize = sizeof(NOTIFYICONDATA);
			m_NotifyIconData.hWnd   = m_hWndParent;
			m_NotifyIconData.uID    = m_nMenuId;
			m_bIsValid = !::Shell_NotifyIcon(NIM_DELETE,&m_NotifyIconData);
			memset(&m_NotifyIconData,'\0',sizeof(m_NotifyIconData));
		}
	}

	return(!m_bIsValid);
}

/*
	SetIcon()
*/
BOOL CTrayIcon::SetIcon(HICON hIcon)
{
	BOOL bSet = FALSE;

	if(m_bIsValid)
	{
		if(m_hWndParent!=(HWND)NULL && m_nMenuId!=(UINT)-1)
		{
			// imposta l'icona per la tray area in base all'handle
			m_NotifyIconData.uFlags = NIF_ICON;
			m_NotifyIconData.hIcon = m_hIcon = hIcon;
			bSet = ::Shell_NotifyIcon(NIM_MODIFY,&m_NotifyIconData);
		}
	}

	return(bSet);
}

/*
	SetIcon()
*/
BOOL CTrayIcon::SetIcon(UINT nIconId)
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
				m_NotifyIconData.uFlags = NIF_ICON;
				m_NotifyIconData.hIcon = m_hIcon = hIcon;
				bSet = ::Shell_NotifyIcon(NIM_MODIFY,&m_NotifyIconData);
			}
		}
	}

	return(bSet);
}

/*
	SetToolTip()
*/
BOOL CTrayIcon::SetToolTip(LPCSTR lpcszTooltip/* = NULL*/)
{
	BOOL bTooltip = FALSE;

	if(m_bIsValid)
	{
		if(m_hWndParent!=(HWND)NULL && m_nMenuId!=(UINT)-1)
		{
			// imposta il testo per il tooltip, quello classico, non il balloon
			m_NotifyIconData.uFlags = NIF_TIP;
			
			if(lpcszTooltip && strcmp(lpcszTooltip,"")!=0)
			{
				strcpyn(m_szToolTipText,lpcszTooltip,sizeof(m_szToolTipText));
				
				// se il testo sfora, sostituisce la fine con [...] (solo se non e' gia' stato accorciato...)
				if(strlen(lpcszTooltip) >= TRAYICON_MAX_TOOLTIP_TEXT && !strstr(lpcszTooltip,ABBREVIATE_DEFAULT_STRING))
				{
					CFilenameFactory fn;
					strcpyn(m_szToolTipText,fn.Abbreviate(lpcszTooltip,TRAYICON_MAX_TOOLTIP_TEXT-1),sizeof(m_szToolTipText));
				}

				strcpyn(m_NotifyIconData.szTip,m_szToolTipText,sizeof(m_NotifyIconData.szTip));
			}
			else
			{
				memset(m_szToolTipText,'\0',sizeof(m_szToolTipText));
				memset(m_NotifyIconData.szTip,'\0',sizeof(m_NotifyIconData.szTip));
			}
			
			bTooltip = ::Shell_NotifyIcon(NIM_MODIFY,&m_NotifyIconData);
		}
	}

	return(bTooltip);
}

/*
	Balloon()
*/
void CTrayIcon::Balloon(LPCSTR	lpcszTitle/* = NULL*/,					// titolo
						LPCSTR	lpcszText/* = NULL*/,					// testo
						UINT	nIconType/* = MB_ICONINFORMATION*/,		// icona (nativo: info, warning, errore - esteso: custom)
						UINT	nTimeout/* = BALLOON_DEFAULT_TIMEOUT*/,	// timeout per rimozione
						UINT	nIconID/* = (UINT)-1L*/,				// handle x icona custom (esclude il nome file), solo per esteso
						LPCSTR	lpcszIconFileName/* = NULL*/,			// nome file x icona custom (esclude l'handle), solo per esteso
						CSize	iconSize/* = CSize(16,16)*/,			// dimensione icona custom, solo per esteso
						UINT	nCloseIconID/* = (UINT)-1L*/			// id risorsa per icona chiusura, solo per esteso
						)
{
	// controlla se deve visualizzare o rimuovere il balloon
	BOOL bRemoveBalloon = !lpcszTitle || !lpcszText;

	// il balloon (nativo) richiede win >= 2K
	if(m_nBalloonType==BALLOON_USE_NATIVE)
	{
		// imposta il titolo
		char szTitle[BALLOON_MAX_TOOLTIP_TITLE+1] = {0};
		if(lpcszTitle)
		{
			strcpyn(szTitle,lpcszTitle,sizeof(szTitle));
			
			// se il testo sfora, sostituisce la fine con [...] (solo se non e' gia' stato accorciato...)
			if(strlen(lpcszTitle) >= BALLOON_MAX_TOOLTIP_TITLE && !strstr(lpcszTitle,ABBREVIATE_DEFAULT_STRING))
			{
				CFilenameFactory fn;
				strcpyn(szTitle,fn.Abbreviate(lpcszTitle,BALLOON_MAX_TOOLTIP_TITLE-1),sizeof(szTitle));
			}
		}
		
		// imposta il testo
		char szText[BALLOON_MAX_TOOLTIP_TEXT+1] = {0};
		if(lpcszText)
			strcpyn(szText,lpcszText,sizeof(szText));

		// imposta il tipo di icona da utilizzare
		DWORD dwIconType = NIIF_NONE;
		if(!bRemoveBalloon)
		{
			switch(nIconType)
			{
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

		// ricava il timeout per la rimozione del balloon
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
		m_NotifyIconData.uFlags = NIF_INFO;
		if(!bRemoveBalloon)
		{
			strcpyn(m_NotifyIconData.szInfoTitle,szTitle,sizeof(m_NotifyIconData.szInfoTitle)-1);
			strcpyn(m_NotifyIconData.szInfo,szText,sizeof(m_NotifyIconData.szInfo)-1);
		}
		else
		{
			memset(m_NotifyIconData.szInfoTitle,'\0',sizeof(m_NotifyIconData.szInfoTitle)-1);
			memset(m_NotifyIconData.szInfo,'\0',sizeof(m_NotifyIconData.szInfo)-1);
		}
		m_NotifyIconData.dwInfoFlags = dwIconType;
		m_NotifyIconData.uTimeout = nTimeout;
		::Shell_NotifyIcon(NIM_MODIFY,&m_NotifyIconData);
		
		// in modo che le notifiche successive non considerino il balloon
		m_NotifyIconData.szInfoTitle[0] = '\0';
		m_NotifyIconData.szInfo[0] = '\0';
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
			if(m_szCssStyles[0]!='\0')
				m_TrayTooltip.SetCssStyles(m_szCssStyles);
			
			// formatta il testo
			char szText[1024];
			if(nCloseIconID!=(UINT)-1L)
				m_nCloseIconID = nCloseIconID;
			snprintf(szText,
					sizeof(szText),
					"<table>"
					"<tr>"
					"<td align=left><h4>%s</h4></td>"
					"<td align=right><a><icon idres=%d width=16 height=16 style=g hotstyle></a></td>"
					"</tr>"
					"</table><hr><br>"
					"%s",
					lpcszTitle,
					m_nCloseIconID,
					lpcszText);

			// visualizza il tooltip
			if(nIconID!=(UINT)-1L)
			{
				// carica l'icona dall'handle
				m_TrayTooltip.SetEffectBk(CPPDrawManager::EFFECT_SOLID,0);
				m_TrayTooltip.SetDelayTime(PPTOOLTIP_TIME_AUTOPOP,1000);
				m_TrayTooltip.SetBehaviour(PPTOOLTIP_CLOSE_LEAVEWND);
//				m_TrayTooltip.SetColorBk(RGB(255,255,255));
				m_TrayTooltip.ShowHelpTooltip(&ptPosition,szText,nIconID,iconSize);
			}
			else
			{
				// chiude l'handle solo se non e' in share
				if(m_hBalloonIcon!=(HICON)NULL && !m_bSharedIcon)
					::DestroyIcon(m_hBalloonIcon);
				m_hBalloonIcon = (HICON)NULL;
				
				// carica l'icona dal file
				if(*lpcszIconFileName)
				{
					m_hBalloonIcon = (HICON)::LoadImage(NULL,lpcszIconFileName,IMAGE_ICON,iconSize.cx,iconSize.cy,LR_DEFAULTCOLOR|LR_LOADFROMFILE);
					m_bSharedIcon = FALSE;
				}
				else // carica l'icona predefinita
				{
					switch(nIconType)
					{
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
					m_TrayTooltip.SetEffectBk(CPPDrawManager::EFFECT_SOLID,0);
					m_TrayTooltip.SetDelayTime(PPTOOLTIP_TIME_AUTOPOP,1000);
					m_TrayTooltip.SetBehaviour(PPTOOLTIP_CLOSE_LEAVEWND);
//					m_TrayTooltip.SetColorBk(RGB(255,255,255));
					m_TrayTooltip.ShowHelpTooltip(&ptPosition,szText,m_hBalloonIcon);
				}
			}
		}
	}
}

/*
	BalloonTimerProc()
*/
VOID CALLBACK CTrayIcon::BalloonTimerProc(HWND /*hWnd*/,UINT /*uMsg*/,UINT_PTR idEvent,DWORD /*dwTime*/)
{
	CTrayIcon* pTrayIcon = (CTrayIcon*)idEvent;	
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
