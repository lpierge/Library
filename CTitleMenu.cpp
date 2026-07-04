/*
	CTitleMenu.cpp
	Based on the code of Per Fikse(1999/06/16) on codeguru.earthweb.com
	Author: Arthur Westerman
	Bug reports by : Brian Pearson 
	
	Luca Piergentili, 09/02/04 (modifiche minori)
*/
#include "pragma.h"
#include "env.h"
#include "macro.h"
#include "window.h"
#include "CTitleMenu.h"

#include "traceexpr.h"
//#define _TRACE_FLAG			_TRFLAG_TRACEOUTPUT // opzioni: _TRFLAG_NOTRACE, _TRFLAG_TRACEFILE, _TRFLAG_TRACECONSOLE, _TRFLAG_TRACEOUTPUT, _TRFLAG_TRACEBREAKPOINT
#define _TRACE_FLAG			_TRFLAG_NOTRACE // opzioni: _TRFLAG_NOTRACE, _TRFLAG_TRACEFILE, _TRFLAG_TRACECONSOLE, _TRFLAG_TRACEOUTPUT, _TRFLAG_TRACEBREAKPOINT
#define _TRACE_FLAG_INFO	_TRACE_FLAG
#define _TRACE_FLAG_WARN	_TRACE_FLAG
#define _TRACE_FLAG_ERR		_TRACE_FLAG

/*
	CTitleMenu()
*/
CTitleMenu::CTitleMenu()
{
	HFONT hFont = CreatePopupMenuTitleFont();
	if(hFont)
		m_Font.Attach(hFont);

	m_lLeft  = ::GetSysColor(COLOR_ACTIVECAPTION);
	m_lRight = ::GetSysColor(COLOR_GRADIENTACTIVECAPTION);
	//m_lRight = ::GetSysColor(27);
	m_lText  = ::GetSysColor(COLOR_CAPTIONTEXT);
	m_bDrawEdge = FALSE;
	m_nEdgeFlags = BDR_SUNKENINNER;

	memset(m_szTitle,'\0',sizeof(m_szTitle));
	
	m_bCanDoGradientFill = FALSE;
	if((m_hMSImg32 = ::LoadLibrary("msimg32.dll"))!=(HMODULE)NULL)
		if((m_lpfnGradientFill = (LPFNDLLFUNC1)::GetProcAddress(m_hMSImg32,"GradientFill"))!=(LPFNDLLFUNC1)NULL)
			m_bCanDoGradientFill = TRUE;		

}

/*
	~CTitleMenu()
*/
CTitleMenu::~CTitleMenu()
{
	m_Font.DeleteObject();
	
	if(m_hMSImg32)
		::FreeLibrary(m_hMSImg32);
}

/*
	CreatePopupMenuTitleFont()
*/
HFONT CTitleMenu::CreatePopupMenuTitleFont(void)
{
	// start by getting the stock menu font
	HFONT hFont = (HFONT)::GetStockObject(ANSI_VAR_FONT);
	if(hFont) 
	{ 
		LOGFONT lf;
		if(::GetObject(hFont,sizeof(LOGFONT),&lf)) //get the complete LOGFONT describing this font
		{
			lf.lfWeight = FW_BOLD; // set the weight to bold

			return(::CreateFontIndirect(&lf)); // recreate this font with just the weight changed
		}
	}

	return(NULL);
}

/*
	AddTitle()
*/
void CTitleMenu::AddTitle(LPCSTR lpcszTitle,UINT nID/* = (UINT)-1L*/)
{
	// insert an empty owner-draw item at top to serve as the title
	int i = 0;
	char* p = (char*)lpcszTitle;
	while(i < sizeof(m_szTitle)-1 && *p)
	{
		m_szTitle[i] = *p;
		if(*p=='&')
			m_szTitle[++i] = '&';
		i++,p++;
	}

	// elemento (titolo) selezionabile a seconda dell'id
	InsertMenu(0,MF_BYPOSITION|MF_OWNERDRAW|MF_STRING,nID==(UINT)-1L ? 0 : nID);
}

/*
	MeasureItem()
*/
void CTitleMenu::MeasureItem(LPMEASUREITEMSTRUCT mi)
{
	// get the screen dc to use for retrieving size information
	CDC dc;
	dc.Attach(::GetDC(NULL));
	
	// select the title font
	HFONT hfontOld = (HFONT)SelectObject(dc.m_hDC,(HFONT)m_Font);
	
	// compute the size of the title
	CSize size = dc.GetTextExtent(m_szTitle,strlen(m_szTitle));

	// deselect the title font
	::SelectObject(dc.m_hDC,hfontOld);

	// add in the left margin for the menu item - vedi sotto quando disegna il menu
	//size.cx += ::GetSystemMetrics(SM_CXMENUCHECK)+8;
	size.cy += 6;

	//Return the width and height
	//+ include space for border
	const int nBorderSize = 2;
	mi->itemWidth = size.cx + nBorderSize;
	mi->itemHeight = size.cy + nBorderSize;
	
	// cleanup
	::ReleaseDC(NULL, dc.Detach());
}

/*
	DrawItem()
*/
void CTitleMenu::DrawItem(LPDRAWITEMSTRUCT di)
{
	COLORREF crOldBk = ::SetBkColor(di->hDC,m_lLeft);
	
	if(m_bCanDoGradientFill&&(m_lLeft!=m_lRight))
	{
 		TRIVERTEX rcVertex[2];
		di->rcItem.right--; // exclude this point, like FillRect does 
		di->rcItem.bottom--;
		rcVertex[0].x		= di->rcItem.left;
		rcVertex[0].y		= di->rcItem.top;
		rcVertex[0].Red	= GetRValue(m_lLeft)<<8;	// color values from 0x0000 to 0xff00 !!!!
		rcVertex[0].Green	= GetGValue(m_lLeft)<<8;
		rcVertex[0].Blue	= GetBValue(m_lLeft)<<8;
		rcVertex[0].Alpha	= 0x0000;
		rcVertex[1].x		= di->rcItem.right; 
		rcVertex[1].y		= di->rcItem.bottom;
		rcVertex[1].Red	= GetRValue(m_lRight)<<8;
		rcVertex[1].Green	= GetGValue(m_lRight)<<8;
		rcVertex[1].Blue	= GetBValue(m_lRight)<<8;
		rcVertex[1].Alpha	= 0;
		GRADIENT_RECT rect;
		rect.UpperLeft		= 0;
		rect.LowerRight	= 1;
		
		// fill the area 
		::GradientFill(di->hDC,rcVertex,2,&rect,1,GRADIENT_FILL_RECT_H);
	}
	else
	{
		::ExtTextOut(di->hDC,0,0,ETO_OPAQUE,&di->rcItem,NULL,0,NULL);
	}

	if(m_bDrawEdge)
		::DrawEdge(di->hDC,&di->rcItem,m_nEdgeFlags,BF_RECT);
 
	int modeOld = ::SetBkMode(di->hDC,TRANSPARENT);
	COLORREF crOld = ::SetTextColor(di->hDC,m_lText);
	
	// select font into the dc
	HFONT hfontOld = (HFONT)::SelectObject(di->hDC,(HFONT)m_Font);

	// add the menu margin offset - eliminare se sotto si usa DT_CENTER
	di->rcItem.left += ::GetSystemMetrics(SM_CXMENUCHECK)+2;

	// draw the text
	::DrawText(di->hDC,m_szTitle,-1,&di->rcItem,DT_SINGLELINE|DT_VCENTER|DT_LEFT/*DT_CENTER*/);

	//Restore font and colors...
	::SelectObject(di->hDC, hfontOld);
	::SetBkMode(di->hDC, modeOld);
	::SetBkColor(di->hDC, crOldBk);
	::SetTextColor(di->hDC, crOld);
}

/*
	GradientFill()
*/
BOOL CTitleMenu::GradientFill(HDC hdc,PTRIVERTEX pVertex,DWORD dwNumVertex,PVOID pMesh,DWORD dwNumMesh,DWORD dwMode)
{
	return(m_bCanDoGradientFill ? m_lpfnGradientFill(hdc,pVertex,dwNumVertex,pMesh,dwNumMesh,dwMode) : FALSE);
}
