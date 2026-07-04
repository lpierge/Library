/*
	CTrayIconPosition.cpp
	Basata sul codice originale di Irek Zielinski
	Luca Piergentili, 10/04/04 (modificato il codice originale)
*/
#include "pragma.h"
#include "env.h"
#include "macro.h"
#include "window.h"
#include "win32api.h"
#include "CWindowsVersion.h"
#include "CWindowsXPTheme.h"
#include "CTrayIconPosition.h"

#include "traceexpr.h"
//#define _TRACE_FLAG			_TRFLAG_TRACEOUTPUT // opzioni: _TRFLAG_NOTRACE, _TRFLAG_TRACEFILE, _TRFLAG_TRACECONSOLE, _TRFLAG_TRACEOUTPUT, _TRFLAG_TRACEBREAKPOINT
#define _TRACE_FLAG			_TRFLAG_NOTRACE // opzioni: _TRFLAG_NOTRACE, _TRFLAG_TRACEFILE, _TRFLAG_TRACECONSOLE, _TRFLAG_TRACEOUTPUT, _TRFLAG_TRACEBREAKPOINT
#define _TRACE_FLAG_INFO	_TRACE_FLAG
#define _TRACE_FLAG_WARN	_TRACE_FLAG
#define _TRACE_FLAG_ERR		_TRACE_FLAG

/*
	CTrayIconPosition()
*/
CTrayIconPosition::CTrayIconPosition()
{
	m_hWndParent = NULL;
	m_nTrayIconID = -1;
	m_nBlankIconID = (UINT)-1L;
	m_rcTrayRect.SetRect(0,0,0,0);
	m_eDefaultPrecision = High;
	m_tLastUpdate = CTime(2000,1,1,1,1,1);
	m_ptPosition.x = m_ptPosition.y = 0;
	m_nPrecisionArray[0] = 60; //seconds for low precision mode
	m_nPrecisionArray[1] = 30; //seconds for medium precision mode
	m_nPrecisionArray[2] = 10; //seconds for high precision mode
}

/*
	SetPrecisionTimeout()

	Performs check of current position of tray icon olny from time to time. You can decide if you want low , med or high precision.
	Default values of precisions are: Low=60 seconds, Med=30 seconds, High=10 secods

	For example, Low precision at 60 seconds means that this class does a full calculation of tray icon and this value expires in
	next 60 seconds. If application will request position of tray icon in time shorter than 60 seconds - there will be no recalculation
	and previous calculated value will be used.
*/
void CTrayIconPosition::SetPrecisionTimeout(int nLowSec,int nMedSec,int nHighSec)
{
	m_nPrecisionArray[0] = nLowSec;
	m_nPrecisionArray[1] = nMedSec;
	m_nPrecisionArray[2] = nHighSec;
}

/*
	InitializeTracking()
*/
void CTrayIconPosition::InitializeTracking(HWND hWndParent,int nIconID,int nBlankIconID)
{
	m_hWndParent = hWndParent;
	m_nTrayIconID = nIconID;
	m_nBlankIconID = nBlankIconID;
}

/*
	GetPosition()
*/
BOOL CTrayIconPosition::GetPosition(CPoint &point,enumPrecision prPrec)
{
	int iTotalSec = (int)(m_tLastUpdate - CTime::GetCurrentTime()).GetTotalSeconds();
	if(iTotalSec < 0)
		iTotalSec = -iTotalSec;

	enumPrecision prec = (prPrec==Default) ? m_eDefaultPrecision : prPrec;

	BOOL bUpdateRequired = FALSE;
	if(prec==Low && iTotalSec > m_nPrecisionArray[0] || prec==Medium && iTotalSec > m_nPrecisionArray[1] || prec==High && iTotalSec > m_nPrecisionArray[2])
		bUpdateRequired = TRUE;

	// calcola l'angolo superiore sinistro della tray area sempre e comunque perche' la FindPosition()
	// funziona a cazzo di cane se la taskbar e' in nero (ad es. con Windows Vista)
	if(bUpdateRequired)
	{		
		int nOfs = 0;
 		CWindowsXPTheme winTheme;
		int nTaskbarPlacement = GetTrayRect();

 		if(!winTheme.IsWindowsClassicStyle())
 			nOfs += 25;
		
		if(nTaskbarPlacement==ABE_BOTTOM)
		{
			point.x = m_rcTrayRect.left + nOfs;
			point.y = m_rcTrayRect.top;
		}
		else if(nTaskbarPlacement==ABE_LEFT)
		{
			point.x = m_rcTrayRect.right;
			point.y = m_rcTrayRect.top + nOfs;
		}
		else if(nTaskbarPlacement==ABE_TOP)
		{
			point.x = m_rcTrayRect.left + nOfs;
			point.y = m_rcTrayRect.bottom;
		}
		else if(nTaskbarPlacement==ABE_RIGHT)
		{
			point.x = m_rcTrayRect.left;
			point.y = m_rcTrayRect.top + nOfs;
		}
		
		return(FALSE);
	}
	else
	{
		point = m_ptPosition;
	}

	return(TRUE);
}

/*
	GetTrayRect()
*/
int CTrayIconPosition::GetTrayRect(void)
{    
	m_rcTrayRect.SetRect(0,0,0,0);
	TASKBARPOS tbi;
	::GetTaskBarPos(&tbi);

	if(tbi.nTaskbarPlacement!=-1)
	{
		m_rcTrayRect.CopyRect(&(tbi.rc));
		::EnumChildWindows(tbi.hWnd,CTrayIconPosition::FindTrayRectCallback,(LPARAM)&m_rcTrayRect);
	}    
	else  
	{        
		m_rcTrayRect.SetRect(tbi.nScreenWidth - 40,tbi.nScreenHeight - 20,tbi.nScreenWidth,tbi.nTaskbarHeight);
	}
	
	return(tbi.nTaskbarPlacement);
}

/*
	FindTrayRectCallback()
*/
BOOL CALLBACK CTrayIconPosition::FindTrayRectCallback(HWND hWnd,LPARAM lParam)
{    
	TCHAR tszClassName[256] = {0};
	::GetClassName(hWnd,tszClassName,sizeof(tszClassName)-1);
	if(_tcscmp(tszClassName,_T("TrayNotifyWnd"))==0)    
	{
		CRect *pRect = (CRect*)lParam;
		::GetWindowRect(hWnd,pRect);
		return(FALSE);
	}    

	return(TRUE);
}
