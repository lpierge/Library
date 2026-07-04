/*
	Note modifiche:
	trasformazione da ANSI a Unicode, non viene specificato il tag //LPI perche' viene rivisto il codice intero
	possibili alcune cose specifiche con //LPI
	Luca Piergentili, 06/2026
*/
#include "env.h"
#include "pragma.h"
#include "macro.h"
#include "window.h"
#include "CPPHtmlDrawerW.h"
#include "atlconv.h"    // for Unicode conversion - requires #include <afxdisp.h> // MFC OLE automation classes
#include <shellapi.h>
#pragma comment(lib, "comctl32.lib")

#include "traceexpr.h"
//#define _TRACE_FLAG			_TRFLAG_TRACEOUTPUT // opzioni: _TRFLAG_NOTRACE, _TRFLAG_TRACEFILE, _TRFLAG_TRACECONSOLE, _TRFLAG_TRACEOUTPUT, _TRFLAG_TRACEBREAKPOINT
#define _TRACE_FLAG			_TRFLAG_NOTRACE // opzioni: _TRFLAG_NOTRACE, _TRFLAG_TRACEFILE, _TRFLAG_TRACECONSOLE, _TRFLAG_TRACEOUTPUT, _TRFLAG_TRACEBREAKPOINT
#define _TRACE_FLAG_INFO	_TRACE_FLAG
#define _TRACE_FLAG_WARN	_TRACE_FLAG
#define _TRACE_FLAG_ERR		_TRACE_FLAG

#define PPHTMLDRAWER_NO_HOVERLINK	-2	//A hot area is not exist under the cursor
#define PPHTMLDRAWER_BREAK_CHARS	L" -.,!:;)}]?" //A set of the chars to break line in the text wrap mode

enum {
		MODE_DRAW = 0,
		MODE_FIRSTPASS,
		MODE_SECONDPASS
	};

/*
#define m_szOffsetShadow.cx		4 //
#define m_szOffsetShadow.cy		4 //
#define m_szDepthShadow.cx		7 //
#define m_szDepthShadow.cy		7 //
#define PPHTMLDRAWER_SHADOW_COLOR		RGB (64, 64, 64) //A gradient shadow's color
*/


/////////////////////////////////////////////////////////////////////////////
// CPPHtmlDrawer

CPPHtmlDrawerW::CPPHtmlDrawerW()
{
	m_nNumPass = MODE_FIRSTPASS;

	m_hInstDll = NULL;
	m_bFreeInstDll = FALSE;
	m_hDC = NULL;
	m_hImageList = NULL;
	
	m_csCallbackRepaint.hWnd = NULL;
	m_csCallbackRepaint.nMessage = 0;
	m_csCallbackRepaint.lParam = 0;
	m_csCallbackRepaint.wParam = 0;
	
	m_csCallbackLink.hWnd = NULL;
	m_csCallbackLink.nMessage = 0;
	m_csCallbackLink.lParam = 0;
	m_csCallbackLink.wParam = 0;

//	m_clrShadow = PPHTMLDRAWER_SHADOW_COLOR;

	m_hLinkCursor = NULL; // No cursor as yet
	m_nHoverIndexLink = PPHTMLDRAWER_NO_HOVERLINK;

	SetListOfTags();
	SetListSpecChars();
    SetTableOfColors();
	SetDefaultCursor();
	EnableEscapeSequences();
	SetMaxWidth(0);
//	EnableTextWrap(FALSE); //A text warpping was disabled by default
//	EnableTextWrap(TRUE); //A text warpping was disabled by default
	SetImageShadow(4, 4);
	SetTabSize(32);
	SetDefaultCssStyles();
	EnableOutput();
	SetDisabledColor(::GetSysColor(COLOR_BTNSHADOW));
}

CPPHtmlDrawerW::~CPPHtmlDrawerW()
{
	SetResourceDll(NULL);

	if (NULL != m_hLinkCursor)
	{
		::DestroyCursor(m_hLinkCursor);
		m_hLinkCursor = NULL;
	}
	
	if (NULL != m_hImageList)
		::DeleteObject(m_hImageList);
}

void CPPHtmlDrawerW::EnableOutput(BOOL bEnable /* = TRUE */)
{
	m_bIsEnable = bEnable;
} //End of EnableOutput

void CPPHtmlDrawerW::SetDisabledColor(COLORREF color)
{
	m_crDisabled = color;
}

HICON CPPHtmlDrawerW::GetIconFromResources(DWORD dwID, int nWidth /* = 0 */, int nHeight /* = 0 */) const
{
	if (0 == dwID) return NULL;

	// Find correct resource handle
#ifdef _MFC_VER
	HINSTANCE hInstResource = AfxFindResourceHandle(MAKEINTRESOURCE(dwID), RT_GROUP_ICON);
#else
	HINSTANCE hInstResource = ::GetModuleHandle(NULL);
#endif
	// Set icon when the mouse is IN the button
	HICON hIcon = (HICON)::LoadImageW(hInstResource, MAKEINTRESOURCEW(dwID), IMAGE_ICON, nWidth, nHeight, LR_DEFAULTCOLOR);
	
	return hIcon;
}

HICON CPPHtmlDrawerW::GetIconFromFile(LPCWSTR lpszPath, int nWidth /* = 0 */, int nHeight /* = 0 */) const
{
	HICON hIcon = (HICON)::LoadImageW(NULL, lpszPath, IMAGE_ICON, nWidth, nHeight, LR_LOADFROMFILE | LR_DEFAULTCOLOR);
	
	return hIcon;
}

HICON CPPHtmlDrawerW::GetIconFromDll(DWORD dwID, int nWidth /* = 0 */, int nHeight /* = 0 */, LPCWSTR lpszPathDll /* = NULL */) const
{
	if (0 == dwID) return NULL;

	HICON hIcon = NULL;

	HINSTANCE hInstDll = NULL;
	BOOL bNewDll = FALSE;

	if (NULL == lpszPathDll)
	{
		if (NULL != m_hInstDll)
			hInstDll = m_hInstDll;
	}
	else
	{
		//Load New Library
		hInstDll = ::LoadLibraryExW(lpszPathDll, NULL, 0);
		if (NULL != hInstDll)
			bNewDll = TRUE;	
	}

	if (NULL != hInstDll)
	{
		hIcon = (HICON)::LoadImageW(hInstDll, MAKEINTRESOURCEW(dwID), IMAGE_ICON, nWidth, nHeight, LR_DEFAULTCOLOR);

		if (bNewDll)
			::FreeLibrary(hInstDll);
	}

	return hIcon;
}

HBITMAP CPPHtmlDrawerW::GetBitmapFromResources(DWORD dwID) const
{
	if (0 == dwID) return NULL;

	// Find correct resource handle
#ifdef _MFC_VER
	HINSTANCE hInstResource = AfxFindResourceHandle(MAKEINTRESOURCE(dwID), RT_BITMAP);
#else
	HINSTANCE hInstResource = ::GetModuleHandle(NULL);
#endif
	// Load bitmap
	HBITMAP hBitmap = (HBITMAP)::LoadImageW(hInstResource, MAKEINTRESOURCEW(dwID), IMAGE_BITMAP, 0, 0, LR_DEFAULTCOLOR);
	
	return hBitmap;
}

HBITMAP CPPHtmlDrawerW::GetBitmapFromFile(LPCWSTR lpszPath) const
{
	HBITMAP hBitmap = (HBITMAP)::LoadImageW(NULL, lpszPath, IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION | LR_DEFAULTSIZE);

	return hBitmap;
}

HBITMAP CPPHtmlDrawerW::GetBitmapFromDll(DWORD dwID, LPCWSTR lpszPathDll /* = NULL */) const
{
	if (0 == dwID) return NULL;

	HBITMAP hBitmap = NULL;

	HINSTANCE hInstDll = NULL;
	BOOL bNewDll = FALSE;

	if (NULL == lpszPathDll)
	{
		if (NULL != m_hInstDll)
			hInstDll = m_hInstDll;
	}
	else
	{
		//Load New Library
		hInstDll = ::LoadLibraryExW(lpszPathDll, NULL, 0);
		if (NULL != hInstDll)
			bNewDll = TRUE;	
	}

	if (NULL != hInstDll)
	{
		hBitmap = (HBITMAP)::LoadImageW(hInstDll, MAKEINTRESOURCEW(dwID), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION | LR_DEFAULTSIZE);

		if (bNewDll)
			::FreeLibrary(hInstDll);
	}

	return hBitmap;
}

CPPString CPPHtmlDrawerW::GetStringFromResource(DWORD dwID) const
{
	if (0 == dwID) return L"";

	CPPString str;
	str.LoadString(dwID);

	return str;
}

CPPString CPPHtmlDrawerW::GetStringFromDll(DWORD dwID, LPCWSTR lpszPathDll /* = NULL */) const
{
	if (0 == dwID) return L"";

	CPPString str = L"";

	HINSTANCE hInstDll = NULL;
	BOOL bNewDll = FALSE;

	if (NULL == lpszPathDll)
	{
		if (NULL != m_hInstDll)
			hInstDll = m_hInstDll;
	}
	else
	{
		//Load New Library
		hInstDll = ::LoadLibraryExW(lpszPathDll, NULL, 0);
		if (NULL != hInstDll)
			bNewDll = TRUE;	
	}

	if (NULL != hInstDll)
	{
#ifdef _UNICODE
#define CHAR_FUDGE 1    // one TCHAR unused is good enough
#else
#define CHAR_FUDGE 2    // two BYTES unused for case of DBC last char
#endif
		// try fixed buffer first (to avoid wasting space in the heap)
		wchar_t szTemp[256];
		
		DWORD dwLen = ::LoadStringW(hInstDll, dwID, szTemp, (sizeof(szTemp) * sizeof(wchar_t)));
		// If resource not found (or ::LoadString failure)
		if (0 != dwLen) 
		{
			if ((sizeof(szTemp) * sizeof(wchar_t)) - dwLen > CHAR_FUDGE)
			{
				str = szTemp;
			} // if
			else
			{
				// try buffer size of 512, then larger size until entire string is retrieved
				int nSize = 256;
				do
				{
					nSize += 256;
					dwLen = ::LoadStringW(hInstDll, dwID, str.GetBuffer(nSize-1), nSize);
				} while (nSize - dwLen <= CHAR_FUDGE);
				str.ReleaseBuffer();
			}
#undef CHAR_FUDGE
		}

		if (bNewDll)
			::FreeLibrary(hInstDll);
	}
	return str;
}

///////////////////////////////////////////////////////////
// Get tooltip string for menu and toolbar items from the 
// resources of the application.
// 
// Parameters:
//		nID - Resource ID of the string
//		nNumParam - Which parameter will gets:
//					 0=long,
//					 1=short,
//					 2=disable
//
//
// Format prompt string:  long prompt \n short prompt \n disable prompt
////////////////////////////////////////////////////////////
CPPString CPPHtmlDrawerW::GetResCommandPrompt(UINT nID, UINT nNumParam /* = 0 */)
{
	CPPString str = GetStringFromResource(nID);
	if (!str.IsEmpty())
	{
		int nFirst = 0;
		int nLast = nFirst;
		UINT nCount = 0;
		while ((nCount <= nNumParam) && (nFirst < str.GetLength()))
		{
			nLast = str.Find(L'\n', nFirst);
			if (nLast < 0)
			{
				//Char wasn't found
				if (nCount == nNumParam)
					str = str.Mid(nFirst, str.GetLength() - nFirst);
				else
					str.Empty();
				
				return str;
			}
			else
			{
				//Char was found
				if (nCount == nNumParam)
				{
					str = str.Mid(nFirst, nLast - nFirst);
					return str;
				}
				else
				{
					nFirst = nLast + 1;
				} //if
			} //if
			nCount ++;
		} //while
	} //if

	return L"";
} //End of GetResCommandPrompt

/////////////////////////////////////////////////////////////////////////////
// 
void CPPHtmlDrawerW::SetListSpecChars()
{
	AddSpecChar(L"&amp;", L"&");			// ampersand
	AddSpecChar(L"&bull;", L"\x95\0");	// bullet  NOT IN MS SANS SERIF
	AddSpecChar(L"&copy;", L"\xA9\0");	// copyright
//	AddSpecChar(L"&euro;", L"\x80\0");	// euro sign IN NOT CYRILLIC FONTS
	AddSpecChar(L"&euro;", L"\x88\0");	// euro sign IN CYRILLIC FONTS
	AddSpecChar(L"&gt;", L">");			// greater than
	AddSpecChar(L"&iquest;", L"\xBF\0");	// inverted question mark
	AddSpecChar(L"&lt;", L"<<");			// less than
	AddSpecChar(L"&nbsp;", L" ");			// nonbreaking space
	AddSpecChar(L"&para;", L"\xB6\0");	// paragraph sign
	AddSpecChar(L"&pound;", L"\xA3\0");	// pound sign
	AddSpecChar(L"&quot;", L"\"");		// quotation mark
	AddSpecChar(L"&reg;", L"\xAE\0");		// registered trademark
	AddSpecChar(L"&trade;", L"\x99\0");	// trademark NOT IN MS SANS SERIF
} //End of SetListSpecChars

void CPPHtmlDrawerW::AddSpecChar(LPCWSTR lpszAlias, LPCWSTR lpszValue)
{
	iter_mapStyles iter = m_mapSpecChars.find(lpszAlias);
	
	if (iter != m_mapSpecChars.end())
		iter->second = lpszValue;		//Modifies
	else
		m_mapSpecChars.insert(std::make_pair(lpszAlias, lpszValue)); //Add new
} //End of AddSpecialChar

void CPPHtmlDrawerW::ReplaceSpecChars()
{
	CPPString sAlias, sValue;
	for (iter_mapStyles iter = m_mapSpecChars.begin(); iter != m_mapSpecChars.end(); ++iter)
	{
		sAlias = iter->first;
		sValue = iter->second;
		m_csHtmlText.Replace(sAlias, sValue);
	} //for

	m_csHtmlText.Remove(L'\r');
	if (!m_bEnableEscapeSequences)
	{
		//ENG: Remove escape sequences
		//RUS: Удаляем специальные символы
		m_csHtmlText.Remove(L'\n');
		m_csHtmlText.Remove(L'\t');
	}
	else
	{
		//ENG: Replace escape sequences to HTML tags
		//RUS: Заменяем специальные символы HTML тэгами
		m_csHtmlText.Replace(L"\n", L"<br>");
		m_csHtmlText.Replace(L"\t", L"<t>");
	} //if
} //End of ReplaceSpecChars

/////////////////////////////////////////////////////////////////////////////
// 
void CPPHtmlDrawerW::SetListOfTags()
{
	AddTagToList(L"b", TAG_BOLD, L"bold");
	AddTagToList(L"i", TAG_ITALIC, L"italic");
	AddTagToList(L"em", TAG_ITALIC, L"italic");
	AddTagToList(L"u", TAG_UNDERLINE, L"underline");
	AddTagToList(L"s", TAG_STRIKEOUT, L"strikeout");
	AddTagToList(L"strike", TAG_STRIKEOUT, L"strikeout");
	AddTagToList(L"font", TAG_FONT, L"font");
	AddTagToList(L"hr", TAG_HLINE, L"");
	AddTagToList(L"br", TAG_NEWLINE, L"");
	AddTagToList(L"\n", TAG_NEWLINE, L"");
	AddTagToList(L"t", TAG_TABULATION, L"");
	AddTagToList(L"\t", TAG_TABULATION, L"");
	AddTagToList(L"left", TAG_LEFT, L"left");
	AddTagToList(L"center", TAG_CENTER, L"center");
	AddTagToList(L"right", TAG_RIGHT, L"right");
	AddTagToList(L"justify", TAG_JUSTIFY, L"justify");
	AddTagToList(L"baseline", TAG_BASELINE, L"baseline");
	AddTagToList(L"top", TAG_TOP, L"top");
	AddTagToList(L"vcenter", TAG_VCENTER, L"vcenter");
	AddTagToList(L"middle", TAG_VCENTER, L"vcenter");
	AddTagToList(L"bottom", TAG_BOTTOM, L"vcenter");
	AddTagToList(L"bmp", TAG_BITMAP, L"");
	AddTagToList(L"icon", TAG_ICON, L"");
	AddTagToList(L"ilst", TAG_IMAGELIST, L"");
	AddTagToList(L"string", TAG_STRING, L"");
	AddTagToList(L"body", TAG_NEWSTYLE, L"body");
	AddTagToList(L"h1", TAG_NEWSTYLE, L"h1");
	AddTagToList(L"h2", TAG_NEWSTYLE, L"h2");
	AddTagToList(L"h3", TAG_NEWSTYLE, L"h3");
	AddTagToList(L"h4", TAG_NEWSTYLE, L"h4");
	AddTagToList(L"h5", TAG_NEWSTYLE, L"h5");
	AddTagToList(L"h6", TAG_NEWSTYLE, L"h6");
	AddTagToList(L"code", TAG_NEWSTYLE, L"code");
	AddTagToList(L"pre", TAG_NEWSTYLE, L"pre");
	AddTagToList(L"big", TAG_NEWSTYLE, L"big");
	AddTagToList(L"small", TAG_NEWSTYLE, L"small");
	AddTagToList(L"sub", TAG_NEWSTYLE, L"sub");
	AddTagToList(L"sup", TAG_NEWSTYLE, L"sup");
	AddTagToList(L"span", TAG_SPAN, L"span");
	AddTagToList(L"a", TAG_HYPERLINK, L"link");
} //End of SetListOfTags

////////////////////////////////////////////////////////////////////////
// Format for the new tags:
//		lpszName		- a tag's name in the HTML string
//		dwTagIndex		- ID of the tag
//		lpszFullName	- a custom name if tag must be closing. Empty if not.  
////////////////////////////////////////////////////////////////////////
void CPPHtmlDrawerW::AddTagToList(LPCWSTR lpszName, DWORD dwTagIndex, LPCWSTR lpszFullName)
{
	STRUCT_TAGPROP tp;
	tp.dwTagIndex = dwTagIndex;
	tp.strTagName = lpszFullName;

	iterMapTags iterMap = m_mapTags.find(lpszName);
	
	if (iterMap != m_mapTags.end())
		iterMap->second = tp; //Modifies
	else
		m_mapTags.insert(std::make_pair(lpszName, tp)); //Add new
} //End of AddTagToList

DWORD CPPHtmlDrawerW::GetTagFromList(CPPString sTagName, CPPString & strFullName, BOOL & bCloseTag)
{
	strFullName.Empty();

	bCloseTag = (sTagName.GetAt(0) == L'/') ? TRUE : FALSE;
	if (bCloseTag)
		sTagName = sTagName.Mid(1);

	iterMapTags iterMap = m_mapTags.find(sTagName);
	
	if (iterMap != m_mapTags.end())
	{
		STRUCT_TAGPROP tp = iterMap->second;
		strFullName = tp.strTagName;
		
		return tp.dwTagIndex;
	} //if

	return TAG_NONE;
} //End of GetTagFromList

///////////////////////////////////////////////////////
// 
///////////////////////////////////////////////////////
void CPPHtmlDrawerW::SetTableOfColors()
{
	//Frequency used
	SetColorName(L"aqua", RGB(0x00, 0xFF, 0xFF)); 
	SetColorName(L"black", RGB(0x00, 0x00, 0x00)); 
	SetColorName(L"blue", RGB(0x00, 0x00, 0xFF)); 
	SetColorName(L"brown", RGB(0xA5, 0x2A, 0x2A)); 
	SetColorName(L"cyan", RGB(0x00, 0xFF, 0xFF));
	SetColorName(L"gold", RGB(0xFF, 0xD7, 0x00)); 
	SetColorName(L"gray", RGB(0x80, 0x80, 0x80)); 
	SetColorName(L"green", RGB(0x00, 0x80, 0x00)); 
	SetColorName(L"magenta", RGB(0xFF, 0x00, 0xFF)); 
	SetColorName(L"maroon", RGB(0x80, 0x00, 0x00)); 
	SetColorName(L"navy", RGB(0x00, 0x00, 0x80)); 
	SetColorName(L"olive", RGB(0x80, 0x80, 0x00)); 
	SetColorName(L"orange", RGB(0xFF, 0xA5, 0x00)); 
	SetColorName(L"pink", RGB(0xFF, 0xC0, 0xCB)); 
	SetColorName(L"purple", RGB(0x80, 0x00, 0x80)); 
	SetColorName(L"red", RGB(0xFF, 0x00, 0x00)); 
	SetColorName(L"silver", RGB(0xC0, 0xC0, 0xC0)); 
	SetColorName(L"snow", RGB(0xFF, 0xFA, 0xFA)); 
	SetColorName(L"violet", RGB(0xEE, 0x82, 0xEE)); 
	SetColorName(L"white", RGB(0xFF, 0xFF, 0xFF)); 
	SetColorName(L"yellow", RGB(0xFF, 0xFF, 0x00)); 

//LPI alli te quedas...
/*
	//Common Used
	SetColorName(_T("aliceblue"), RGB(0xF0, 0xF8, 0xFF)); 
	SetColorName(_T("antiquewhite"), RGB(0xFA, 0xEB, 0xD7)); 
	SetColorName(_T("aquamarine"), RGB(0x7F, 0xFF, 0xD4)); 
	SetColorName(_T("azure"), RGB(0xF0, 0xFF, 0xFF)); 
	SetColorName(_T("beige"), RGB(0xF5, 0xF5, 0xDC)); 
	SetColorName(_T("bisque"), RGB(0xFF, 0xE4, 0xC4));
	SetColorName(_T("blanchedalmond"), RGB(0xFF, 0xEB, 0xCD)); 
	SetColorName(_T("blueviolet"), RGB(0x8A, 0x2B, 0xE2)); 
	SetColorName(_T("burlywood"), RGB(0xDE, 0xB8, 0x87)); 
	SetColorName(_T("cadetblue"), RGB(0x5F, 0x9E, 0xA0)); 
	SetColorName(_T("chartreuse"), RGB(0x7F, 0xFF, 0x00)); 
	SetColorName(_T("chocolate"), RGB(0xD2, 0x69, 0x1E)); 
	SetColorName(_T("coral"), RGB(0xFF, 0x7F, 0x50)); 
	SetColorName(_T("cornflowerblue"), RGB(0x64, 0x95, 0xED)); 
	SetColorName(_T("cornsilk"), RGB(0xFF, 0xF8, 0xDC)); 
	SetColorName(_T("crimson"), RGB(0xDC, 0x14, 0x3C)); 
	SetColorName(_T("darkblue"), RGB(0x00, 0x00, 0x8B)); 
	SetColorName(_T("darkcyan"), RGB(0x00, 0x8B, 0x8B)); 
	SetColorName(_T("darkgoldenrod"), RGB(0xB8, 0x86, 0x0B)); 
	SetColorName(_T("darkgray"), RGB(0xA9, 0xA9, 0xA9)); 
	SetColorName(_T("darkgreen"), RGB(0x00, 0x64, 0x00)); 
	SetColorName(_T("darkkhaki"), RGB(0xBD, 0xB7, 0x6B)); 
	SetColorName(_T("darkmagenta"), RGB(0x8B, 0x00, 0x8B)); 
	SetColorName(_T("darkolivegreen"), RGB(0x55, 0x6B, 0x2F)); 
	SetColorName(_T("darkorange"), RGB(0xFF, 0x8C, 0x00)); 
	SetColorName(_T("darkorchid"), RGB(0x99, 0x32, 0xCC)); 
	SetColorName(_T("darkred"), RGB(0x8B, 0x00, 0x00)); 
	SetColorName(_T("darksalmon"), RGB(0xE9, 0x96, 0x7A)); 
	SetColorName(_T("darkseagreen"), RGB(0x8F, 0xBC, 0x8B)); 
	SetColorName(_T("darkslateblue"), RGB(0x48, 0x3D, 0x8B)); 
	SetColorName(_T("darkslategray"), RGB(0x2F, 0x4F, 0x4F)); 
	SetColorName(_T("darkturquoise"), RGB(0x00, 0xCE, 0xD1)); 
	SetColorName(_T("darkviolet"), RGB(0x94, 0x00, 0xD3)); 
	SetColorName(_T("deeppink"), RGB(0xFF, 0x14, 0x93)); 
	SetColorName(_T("deepskyblue"), RGB(0x00, 0xBF, 0xFF)); 
	SetColorName(_T("dimgray"), RGB(0x69, 0x69, 0x69)); 
	SetColorName(_T("dodgerblue"), RGB(0x1E, 0x90, 0xFF)); 
	SetColorName(_T("firebrick"), RGB(0xB2, 0x22, 0x22)); 
	SetColorName(_T("floralwhite"), RGB(0xFF, 0xFA, 0xF0)); 
	SetColorName(_T("forestgreen"), RGB(0x22, 0x8B, 0x22)); 
	SetColorName(_T("fuchsia"), RGB(0xFF, 0x00, 0xFF)); 
	SetColorName(_T("gainsboro"), RGB(0xDC, 0xDC, 0xDC)); 
	SetColorName(_T("ghostwhite"), RGB(0xF8, 0xF8, 0xFF)); 
	SetColorName(_T("goldenrod"), RGB(0xDA, 0xA5, 0x20)); 
	SetColorName(_T("greenyellow"), RGB(0xAD, 0xFF, 0x2F)); 
	SetColorName(_T("honeydew"), RGB(0xF0, 0xFF, 0xF0)); 
	SetColorName(_T("hotpink"), RGB(0xFF, 0x69, 0xB4)); 
	SetColorName(_T("indianred"), RGB(0xCD, 0x5C, 0x5C)); 
	SetColorName(_T("indigo"), RGB(0x4B, 0x00, 0x82)); 
	SetColorName(_T("ivory"), RGB(0xFF, 0xFF, 0xF0)); 
	SetColorName(_T("khaki"), RGB(0xF0, 0xE6, 0x8C)); 
	SetColorName(_T("lavender"), RGB(0xE6, 0xE6, 0xFA)); 
	SetColorName(_T("lavenderblush"), RGB(0xFF, 0xF0, 0xF5)); 
	SetColorName(_T("lawngreen"), RGB(0x7C, 0xFC, 0x00)); 
	SetColorName(_T("lemonchiffon"), RGB(0xFF, 0xFA, 0xCD)); 
	SetColorName(_T("lightblue"), RGB(0xAD, 0xD8, 0xE6)); 
	SetColorName(_T("lightcoral"), RGB(0xF0, 0x80, 0x80)); 
	SetColorName(_T("lightcyan"), RGB(0xE0, 0xFF, 0xFF));
	SetColorName(_T("lightgoldenrodyellow"), RGB(0xFA, 0xFA, 0xD2)); 
	SetColorName(_T("lightgreen"), RGB(0x90, 0xEE, 0x90)); 
	SetColorName(_T("lightgrey"), RGB(0xD3, 0xD3, 0xD3)); 
	SetColorName(_T("lightpink"), RGB(0xFF, 0xB6, 0xC1)); 
	SetColorName(_T("lightsalmon"), RGB(0xFF, 0xA0, 0x7A)); 
	SetColorName(_T("lightseagreen"), RGB(0x20, 0xB2, 0xAA)); 
	SetColorName(_T("lightskyblue"), RGB(0x87, 0xCE, 0xFA)); 
	SetColorName(_T("lightslategray"), RGB(0x77, 0x88, 0x99)); 
	SetColorName(_T("lightsteelblue"), RGB(0xB0, 0xC4, 0xDE));
	SetColorName(_T("lightyellow"), RGB(0xFF, 0xFF, 0xE0)); 
	SetColorName(_T("lime"), RGB(0x00, 0xFF, 0x00)); 
	SetColorName(_T("limegreen"), RGB(0x32, 0xCD, 0x32)); 
	SetColorName(_T("linen"), RGB(0xFA, 0xF0, 0xE6)); 
	SetColorName(_T("mediumaquamarine"), RGB(0x66, 0xCD, 0xAA)); 
	SetColorName(_T("mediumblue"), RGB(0x00, 0x00, 0xCD)); 
	SetColorName(_T("mediumorchid"), RGB(0xBA, 0x55, 0xD3)); 
	SetColorName(_T("mediumpurple"), RGB(0x93, 0x70, 0xDB)); 
	SetColorName(_T("mediumseagreen"), RGB(0x3C, 0xB3, 0x71)); 
	SetColorName(_T("mediumslateblue"), RGB(0x7B, 0x68, 0xEE)); 
	SetColorName(_T("mediumspringgreen"), RGB(0x00, 0xFA, 0x9A)); 
	SetColorName(_T("mediumturquoise"), RGB(0x48, 0xD1, 0xCC)); 
	SetColorName(_T("mediumvioletred"), RGB(0xC7, 0x15, 0x85)); 
	SetColorName(_T("midnightblue"), RGB(0x19, 0x19, 0x70)); 
	SetColorName(_T("mintcream"), RGB(0xF5, 0xFF, 0xFA)); 
	SetColorName(_T("mistyrose"), RGB(0xFF, 0xE4, 0xE1)); 
	SetColorName(_T("moccasin"), RGB(0xFF, 0xE4, 0xB5)); 
	SetColorName(_T("navajowhite"), RGB(0xFF, 0xDE, 0xAD)); 
	SetColorName(_T("oldlace"), RGB(0xFD, 0xF5, 0xE6)); 
	SetColorName(_T("olivedrab"), RGB(0x6B, 0x8E, 0x23)); 
	SetColorName(_T("orangered"), RGB(0xFF, 0x45, 0x00)); 
	SetColorName(_T("orchid"), RGB(0xDA, 0x70, 0xD6)); 
	SetColorName(_T("palegoldenrod"), RGB(0xEE, 0xE8, 0xAA)); 
	SetColorName(_T("palegreen"), RGB(0x98, 0xFB, 0x98)); 
	SetColorName(_T("paleturquoise"), RGB(0xAF, 0xEE, 0xEE)); 
	SetColorName(_T("palevioletred"), RGB(0xDB, 0x70, 0x93)); 
	SetColorName(_T("papayawhip"), RGB(0xFF, 0xEF, 0xD5));
	SetColorName(_T("peachpuff"), RGB(0xFF, 0xDA, 0xB9)); 
	SetColorName(_T("peru"), RGB(0xCD, 0x85, 0x3F)); 
	SetColorName(_T("plum"), RGB(0xDD, 0xA0, 0xDD)); 
	SetColorName(_T("powderblue"), RGB(0xB0, 0xE0, 0xE6)); 
	SetColorName(_T("rosybrown"), RGB(0xBC, 0x8F, 0x8F)); 
	SetColorName(_T("royalblue"), RGB(0x41, 0x69, 0xE1)); 
	SetColorName(_T("saddlebrown"), RGB(0x8B, 0x45, 0x13)); 
	SetColorName(_T("salmon"), RGB(0xFA, 0x80, 0x72)); 
	SetColorName(_T("sandybrown"), RGB(0xF4, 0xA4, 0x60)); 
	SetColorName(_T("seagreen"), RGB(0x2E, 0x8B, 0x57)); 
	SetColorName(_T("seashell"), RGB(0xFF, 0xF5, 0xEE)); 
	SetColorName(_T("sienna"), RGB(0xA0, 0x52, 0x2D)); 
	SetColorName(_T("skyblue"), RGB(0x87, 0xCE, 0xEB)); 
	SetColorName(_T("slateblue"), RGB(0x6A, 0x5A, 0xCD)); 
	SetColorName(_T("slategray"), RGB(0x70, 0x80, 0x90)); 
	SetColorName(_T("springgreen"), RGB(0x00, 0xFF, 0x7F)); 
	SetColorName(_T("steelblue"), RGB(0x46, 0x82, 0xB4)); 
	SetColorName(_T("tan"), RGB(0xD2, 0xB4, 0x8C)); 
	SetColorName(_T("teal"), RGB(0x00, 0x80, 0x80)); 
	SetColorName(_T("thistle"), RGB(0xD8, 0xBF, 0xD8)); 
	SetColorName(_T("tomato"), RGB(0xFF, 0x63, 0x47)); 
	SetColorName(_T("turquoise"), RGB(0x40, 0xE0, 0xD0)); 
	SetColorName(_T("wheat"), RGB(0xF5, 0xDE, 0xB3)); 
	SetColorName(_T("whitesmoke"), RGB(0xF5, 0xF5, 0xF5)); 
	SetColorName(_T("yellowgreen"), RGB(0x9A, 0xCD, 0x32));
*/
	//Systems colors
	SetColorName(L"activeborder", ::GetSysColor(COLOR_ACTIVEBORDER)); 
	SetColorName(L"activecaption", ::GetSysColor(COLOR_ACTIVECAPTION)); 
	SetColorName(L"appworkspace", ::GetSysColor(COLOR_APPWORKSPACE)); 
	SetColorName(L"background", ::GetSysColor(COLOR_BACKGROUND)); 
	SetColorName(L"buttonface", ::GetSysColor(COLOR_BTNFACE)); 
	SetColorName(L"buttonhighlight", ::GetSysColor(COLOR_BTNHILIGHT)); 
	SetColorName(L"buttonshadow", ::GetSysColor(COLOR_BTNSHADOW)); 
	SetColorName(L"buttontext", ::GetSysColor(COLOR_BTNTEXT)); 
	SetColorName(L"captiontext", ::GetSysColor(COLOR_CAPTIONTEXT)); 
	SetColorName(L"graytext", ::GetSysColor(COLOR_GRAYTEXT)); 
	SetColorName(L"highlight", ::GetSysColor(COLOR_HIGHLIGHT)); 
	SetColorName(L"highlighttext", ::GetSysColor(COLOR_HIGHLIGHTTEXT)); 
	SetColorName(L"inactiveborder", ::GetSysColor(COLOR_INACTIVEBORDER)); 
	SetColorName(L"inactivecaption", ::GetSysColor(COLOR_INACTIVECAPTION)); 
	SetColorName(L"inactivecaptiontext", ::GetSysColor(COLOR_INACTIVECAPTIONTEXT)); 
	SetColorName(L"infobackground", ::GetSysColor(COLOR_INFOBK)); 
	SetColorName(L"infotext", ::GetSysColor(COLOR_INFOTEXT)); 
	SetColorName(L"menu", ::GetSysColor(COLOR_MENU)); 
	SetColorName(L"menutext", ::GetSysColor(COLOR_MENUTEXT)); 
	SetColorName(L"scrollbar", ::GetSysColor(COLOR_SCROLLBAR)); 
	SetColorName(L"threeddarkshadow", ::GetSysColor(COLOR_3DDKSHADOW)); 
	SetColorName(L"threedface", ::GetSysColor(COLOR_3DFACE)); 
	SetColorName(L"threedhighlight", ::GetSysColor(COLOR_3DHIGHLIGHT)); 
	SetColorName(L"threedlightshadow", ::GetSysColor(COLOR_3DLIGHT)); 
	SetColorName(L"threedshadow", ::GetSysColor(COLOR_3DSHADOW)); 
	SetColorName(L"window", ::GetSysColor(COLOR_WINDOW)); 
	SetColorName(L"windowframe", ::GetSysColor(COLOR_WINDOWFRAME)); 
	SetColorName(L"windowtext", ::GetSysColor(COLOR_WINDOWTEXT)); 
} //End SetTableOfColors

void CPPHtmlDrawerW::SetColorName(LPCWSTR lpszColorName, COLORREF color)
{
	iterMapColors iterMap = m_mapColors.find(lpszColorName);
	
	if (iterMap != m_mapColors.end())
		iterMap->second = color; //Modifies
	else
		m_mapColors.insert(std::make_pair(lpszColorName, color)); //Add new
} //End SetColorName

COLORREF CPPHtmlDrawerW::GetColorByName(LPCWSTR lpszColorName, COLORREF crDefColor /* = RGB(0, 0, 0) */)
{
	if (m_bIsEnable)
	{
		iterMapColors iterMap = m_mapColors.find(lpszColorName);
		
		if (iterMap != m_mapColors.end())
			crDefColor = iterMap->second;
	}
	else
	{
		//For disabled output
		crDefColor = m_crDisabled;
	} //if
	return crDefColor;
} //End GetColorByName

/////////////////////////////////////////////////////////////////
// Gets the system tooltip's logfont
/////////////////////////////////////////////////////////////////
LPLOGFONTW CPPHtmlDrawerW::GetSystemToolTipFont() const
{
    static LOGFONTW lf;
	
    NONCLIENTMETRICSW ncm;
    ncm.cbSize = sizeof(NONCLIENTMETRICSW);
    if (!SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(NONCLIENTMETRICSW), &ncm, 0))
        return FALSE;
	
    memcpy(&lf, &(ncm.lfStatusFont), sizeof(LOGFONTW));
	
    return &lf; 
} //End GetSystemToolTipFont

////////////////////////////////////////////
// Check a pointer over the hyperlink
//   In: lpPoint - the coordinates of the mouse pointer 
//  Out: -1 - hyperlink not found
//       index of the hyperlink
////////////////////////////////////////////
int CPPHtmlDrawerW::PtInHyperlink(LPPOINT lpPoint)
{
	for (UINT i = 0; i < m_arrLinks.size(); ++i)
	{
		STRUCT_HYPERLINK & link = m_arrLinks [i];
		if ((link.rcArea.left <= lpPoint->x) && (link.rcArea.right >= lpPoint->x) &&
			(link.rcArea.top <= lpPoint->y) && (link.rcArea.bottom >= lpPoint->y))
			return i;
	} //for
	return -1;
} //End PtInHyperlink

void CPPHtmlDrawerW::JumpToHyperlink(int nLink)
{
	STRUCT_HYPERLINK & link = m_arrLinks [nLink];
//	TRACE(_T("Jump to Hyperlink number = %d\n"), nLink);
	if (!link.sHyperlink.IsEmpty())
	{
		switch (link.nTypeLink)
		{
		case LINK_HREF:
			GotoURL(link.sHyperlink);
			break;
		case LINK_MESSAGE:
			CallbackOnClickHyperlink(link.sHyperlink);
			break;
		} //switch
	} //if
} //End JumpToHyperlink

void CPPHtmlDrawerW::OnLButtonDown(LPPOINT lpClient)
{
//	TRACE (_T("CPPHtmlDrawerW::OnLButtonDown()\n"));
	
	int nLink = PtInHyperlink(lpClient);
	if (nLink >= 0)
	{
		//Hyperlink under the mouse pointer
		JumpToHyperlink(nLink);
	} //if
} //End OnLButtonDown

BOOL CPPHtmlDrawerW::OnSetCursor(LPPOINT lpClient)
{
	int nLink = PtInHyperlink(lpClient);
	if (nLink >= 0)
	{
		STRUCT_HYPERLINK link = m_arrLinks [nLink];
		if (m_nHoverIndexLink != link.nIndexLink)
		{
			m_nHoverIndexLink = link.nIndexLink;
			CallbackOnRepaint(m_nHoverIndexLink);
			//Redraw Window
		} //if
		
		if (!link.sHyperlink.IsEmpty() && (NULL != m_hLinkCursor))
		{
			::SetCursor(m_hLinkCursor);
			return TRUE;
		} //if
	}
	else if (m_nHoverIndexLink != PPHTMLDRAWER_NO_HOVERLINK)
	{
		m_nHoverIndexLink = PPHTMLDRAWER_NO_HOVERLINK;
		CallbackOnRepaint(m_nHoverIndexLink);
		//Redraw Window
	} //if
	
    return FALSE;
} //End OnSetCursor

BOOL CPPHtmlDrawerW::OnTimer()
{
	BOOL bRedraw = FALSE;
	if (m_arrAni.size() > 0)
	{
		for (UINT i = 0; i < m_arrAni.size(); ++i)
		{
			STRUCT_ANIMATION & sa = m_arrAni [i];
			if (sa.nMaxImages > 0)
			{
				sa.nTimerCount ++;
				if (sa.nTimerCount >= sa.nSpeed)
				{
					sa.nTimerCount = 0;
					sa.nIndex ++;
					if (sa.nIndex >= sa.nMaxImages)
						sa.nIndex = 0;
					bRedraw = TRUE;
				} //if
				m_arrAni [i] = sa;
			} //if
		} //for
	} //if

	return bRedraw;
} //End of OnTimer

void CPPHtmlDrawerW::CallbackOnRepaint(int nIndexLink)
{
//	TRACE(_T("CPPHtmlDrawerW::CallbackOnRepaint()\n")); 

	if ((NULL == m_csCallbackRepaint.hWnd) || !m_csCallbackRepaint.nMessage)
		return; 
 	
	::SendMessage(m_csCallbackRepaint.hWnd, m_csCallbackRepaint.nMessage, (LPARAM)nIndexLink, m_csCallbackRepaint.lParam);  
} //End CallbackOnRepaint

void CPPHtmlDrawerW::CallbackOnClickHyperlink(LPCWSTR sLink)
{
//	TRACE(_T("CPPHtmlDrawerW::CallbackOnClickHyperlink()\n")); 

	if ((NULL == m_csCallbackLink.hWnd) || !m_csCallbackLink.nMessage)
		return; 
	
	::SendMessage(m_csCallbackLink.hWnd, m_csCallbackLink.nMessage, (LPARAM)sLink, m_csCallbackLink.lParam);  	
} //if CallbackOnClickHyperlink

HINSTANCE CPPHtmlDrawerW::GotoURL(LPCWSTR url, int showcmd /* = SW_SHOW */)
{
	SetHyperlinkCursor(NULL);

    wchar_t key[MAX_PATH + MAX_PATH];

    // First try ShellExecute()
    HINSTANCE result = ShellExecuteW(NULL, L"open", url, NULL, NULL, showcmd);

    // If it failed, get the .htm regkey and lookup the program
    if ((UINT)result <= HINSTANCE_ERROR) 
	{

        if (GetRegKey(HKEY_CLASSES_ROOT, L".htm", key) == ERROR_SUCCESS) 
		{
            lstrcatW(key, L"\\shell\\open\\command");

            if (GetRegKey(HKEY_CLASSES_ROOT,key,key) == ERROR_SUCCESS) 
			{
                wchar_t *pos;
                pos = wcsstr(key, L"\"%1\"");
                if (pos == NULL) 
				{                     // No quotes found
                    pos = wcsstr(key, L"%1");      // Check for %1, without quotes 
                    if (pos == NULL)                   // No parameter at all...
                        pos = key+lstrlenW(key)-1;
                    else
                        *pos = L'\0';                   // Remove the parameter
                }
                else
                    *pos = L'\0';                       // Remove the parameter

                lstrcatW(pos, L" ");
                lstrcatW(pos, url);

                USES_CONVERSION;
                result = (HINSTANCE) WinExec(W2A(key),showcmd);
				//LPI
				/*
				oppure, piu' moderno:
				// USES_CONVERSION; -> non serve piu'
				result = ::ShellExecuteW(NULL, L"open", key, NULL, NULL, showcmd); // -> passa la stringa key direttamente in Unicode
				*/
            } //if
        } //if
    } //if
    return result;
} //End GotoURL

LONG CPPHtmlDrawerW::GetRegKey(HKEY key, LPCWSTR subkey, LPWSTR retdata)
{
    HKEY hkey;
    LONG retval = RegOpenKeyExW(key, subkey, 0, KEY_QUERY_VALUE, &hkey);

    if (retval == ERROR_SUCCESS) {
        long datasize = MAX_PATH;
        wchar_t data[MAX_PATH];
        RegQueryValueW(hkey, NULL, data, &datasize);
        lstrcpyW(retdata,data);
        RegCloseKey(hkey);
    } //if

    return retval;
} //End GetRegKey

/////////////////////////////////////////////////////////////////
void CPPHtmlDrawerW::DrawHtml (LPSIZE lpSize, LPCRECT lpRect)
{
	//ENG: Bounding rectangle of a current area for output
	//RUS: Ограничивающей прямоугольник для текущей области вывода
	RECT rcArea;
	rcArea.left = lpRect->left;
	rcArea.right = lpRect->right;
	rcArea.top = lpRect->top;
	rcArea.bottom = lpRect->bottom;

	SIZE szArea;
	szArea.cx = szArea.cy = 0;
	
	if (MODE_FIRSTPASS == m_nNumPass)
	{
		//ENG: In preparing mode clears an auxiliary tables
		//RUS: В режиме подготовки очищаем вспомогательные таблицы 
		m_arrLinks.clear();
//		m_arrTable.clear();
		m_arrHtmlLine.clear();
//		m_arrTableSizes.clear();
		m_arrAni.clear();
	} //if

	m_nCurLine = 0;
	m_nCurTable = -1;
	m_nNumCurTable = -1;
	m_nCurIndexLink = -1;
	m_nCurIndexAni = -1;
	
	//ENG: Clear stack of tags
	//RUS: Очищаем стэк тэгов
	m_arrStack.clear();
	
	int nIndex = 0;
	int nBegin;
	CPPString strText;
	
	//ENG: Applies a default styles
	//RUS: Применяем стили по-умолчанию
	SetDefaultStyles(m_defStyle);
	SelectNewHtmlStyle(L"body", m_defStyle);
	
	//ENG: Creates a default font
	//RUS: Создаем шрифт по умолчанию
	m_lfDefault.lfHeight = m_defStyle.nSizeFont;
	m_lfDefault.lfWidth = 0;
	m_lfDefault.lfOrientation = 0;
	m_lfDefault.lfEscapement = 0;
	m_lfDefault.lfWeight = m_defStyle.nWeightFont;
	m_lfDefault.lfItalic = (BYTE)m_defStyle.bItalicFont;
	m_lfDefault.lfStrikeOut = (BYTE)m_defStyle.bStrikeOutFont;
	m_lfDefault.lfUnderline = (BYTE)m_defStyle.bUnderlineFont;
	m_lfDefault.lfCharSet = DEFAULT_CHARSET;
	m_lfDefault.lfOutPrecision = OUT_DEFAULT_PRECIS;
	m_lfDefault.lfClipPrecision = CLIP_DEFAULT_PRECIS;
	m_lfDefault.lfQuality = DEFAULT_QUALITY;
	m_lfDefault.lfPitchAndFamily = FF_DONTCARE;
	wcscpy (m_lfDefault.lfFaceName, m_defStyle.sFaceFont);
	m_hFont = ::CreateFontIndirectW(&m_lfDefault);
	
	//ENG: Remember a current context setting
	//RUS: Запоминаем текущие настройки контекст устройства
	m_hOldFont = (HFONT)::SelectObject(m_hDC, m_hFont);
	m_nOldBkMode = ::SetBkMode(m_hDC, m_defStyle.nBkMode);
	m_crOldText = ::SetTextColor(m_hDC, m_defStyle.crText);
	m_crOldBk = ::SetBkColor(m_hDC, m_defStyle.crBkgnd);
	::GetTextMetricsW(m_hDC, &m_tm);
	
	while (nIndex < m_csHtmlText.GetLength())
	{
		//ENG: Searching a begin of table
		//RUS: Ищем начало таблицы
		nBegin = nIndex;
		BOOL bFoundTable = SearchTag(m_csHtmlText, nIndex, L"table");

		//ENG: Gets a text before a table
		//RUS: Получаем текст до таблицы
		strText = m_csHtmlText.Mid(nBegin, nIndex - nBegin);

		//ENG: If text before a table is exist
		//RUS: Если текст перед таблицей существует
		if (!strText.IsEmpty())
		{
			//ENG: Add a tag BODY around of a output text
			//RUS: Добавляем тэг BODY вокруг выводимого текста
//			strText = _T("<body>") + strText + _T("</body>");

			//ENG: Output a text before of a table
			//RUS: Выводим текст перед таблицей
			szArea = DrawHtmlString(strText, &rcArea);

			//ENG: Updates a output area size
			//RUS: Обновляем размер области вывода
			lpSize->cx = max(lpSize->cx, szArea.cx);
			lpSize->cy += szArea.cy;
			if (MODE_DRAW == m_nNumPass)
				rcArea.top += szArea.cy;
		} //if
		
		//ENG: If table was found
		//RUS: Если таблица была найдена
		if (bFoundTable)
		{
			//ENG: Searching an end of the table
			//RUS: Ищем окончание таблицы
			nBegin = nIndex;
			nIndex += 6;
			SearchEndOfTable(m_csHtmlText, nIndex);

			//ENG: Cuts a text of a table
			//RUS: Вырезаем текст таблицы
			strText = m_csHtmlText.Mid(nBegin, nIndex - nBegin);

			//ENG: Output a table
			//RUS: Вывод таблицы
			szArea = DrawHtmlTable(strText, &rcArea);
			
			//ENG: Updates a output area size
			//RUS: Обновляем размер области вывода
			lpSize->cx = max(lpSize->cx, szArea.cx);
			lpSize->cy += szArea.cy;
			if (MODE_DRAW == m_nNumPass)
				rcArea.top += szArea.cy;
		} //if
	} //while
	
	//ENG: Restore context setting
	//RUS: Восстанавливаем настроки контекста устройства
	::SetBkMode(m_hDC, m_nOldBkMode);
	::SetBkColor(m_hDC, m_crOldBk);
	::SetTextColor(m_hDC, m_crOldText);
	::SelectObject(m_hDC, m_hOldFont);
	
	//ENG: Clear stack of tags
	//RUS: Очищаем стэк тэгов
	m_arrStack.clear();
	
	//ENG: Delete a font
	//RUS: Удаляем шрифт
	::DeleteObject(m_hFont);
} //End of DrawHtml


SIZE CPPHtmlDrawerW::DrawHtmlTable (CPPString & sTable, LPCRECT lpRect)
{
	//ENG: Jump to the next table
	//RUS: Начинаем новую таблицу
	m_nCurTable++;

	int i;
	UINT pos;
	SIZE size = {0, 0};
	SIZE szTable;
	RECT rcTable = {0, 0, 0, 0};
	RECT rcRow;

	if (MODE_FIRSTPASS == m_nNumPass) 
	{
		//ENG: Get size of the table
		//RUS: Получаем размеры таблицы
		szTable = GetTableDimensions(sTable);
		
		STRUCT_TABLE st;
		STRUCT_CELL sc;
		sc.nRowSpan = 0;
		sc.nColSpan = 0;
//		sc.bHeightPercent = FALSE;
//		sc.bWidthPercent = FALSE;
//		sc.nHeight = 0;
//		sc.nWidth = 0;
		sc.szText.cx = sc.szText.cy = sc.szCell.cx = sc.szCell.cy = 0;
		sc.bFixedWidth = FALSE;
	
		//ENG: Creates a template of an empty table
		//RUS: Создаем шаблон пустой таблицы
		vecRow rows;
		for (i = 0; i < szTable.cx; i++)
		{
			rows.push_back(sc);
			st.width.push_back(0);
			st.fixed_width.push_back(FALSE);
		} //for
		for (i = 0; i < szTable.cy; i++)
		{
			st.cells.push_back(rows);
			st.height.push_back(0);
		} //for
		
		//ENG: Add a new table
		//RUS: Добавляем новую таблицу
		m_arrTables.push_back(st);
	} //if

	//ENG: Gets an info about a current table
	//RUS: Взять информацию о текущей таблице 
	int nIndexTable = m_nCurTable;
	STRUCT_TABLE cur_table = m_arrTables [nIndexTable];
	
	szTable.cx = cur_table.width.size();
	szTable.cy = cur_table.height.size();

	//ENG: Applies styles of <table> tag
	//RUS: Применяем стили таблицы (тэг <table>)
	m_defStyle.strTag = L"table";
	StoreRestoreStyle(FALSE);
	SelectNewHtmlStyle(m_defStyle.strTag, m_defStyle);
	
	//ENG: Passes a tag body and get a properties of the tag
	//RUS: Пропускаем тэг начала ячейки и получаем строку свойств тэга
	int nIndex = 0;
	CPPString sTag;
	SearchNextTag(sTable, sTag, nIndex);
	CPPString sProperties = SplitTag(sTag);

	//ENG: Analyses a properties of the tag
	//RUS: Анализируем свойства тэга
	AnalyseCellParam(sProperties, m_defStyle, TRUE);
	UpdateContext();

	if (MODE_FIRSTPASS != m_nNumPass)
	{
		//ENG: Gets a real size of the table
		//RUS: Получаем реальные размеры таблицы
		rcTable.left = lpRect->left;
		rcTable.top = rcTable.bottom = lpRect->top;

		int nWidthTable = m_defStyle.nPadding + cur_table.width.size() - 1;
		for (pos = 0; pos < cur_table.width.size(); ++pos)
			nWidthTable += cur_table.width [pos] + m_defStyle.nPadding;
		rcTable.bottom += m_defStyle.nPadding + cur_table.height.size() - 1;
		for (pos = 0; pos < cur_table.height.size(); ++pos)
			rcTable.bottom += cur_table.height [pos] + m_defStyle.nPadding;

		if (CPPDrawManagerW::PEN_DOUBLE == m_defStyle.nBorderStyle)
		{
			nWidthTable += 6;
			rcTable.bottom += 6;
		}
		else
		{
			nWidthTable += m_defStyle.nBorderWidth * 2;
			rcTable.bottom += m_defStyle.nBorderWidth * 2;
		} //if

		//ENG: Horizontal align of the table
		//RUS: Выравнивание таблицы по горизонтали
		int nRealWidth = lpRect->right - lpRect->left;

		if (nWidthTable < nRealWidth)
		{
			//RUS: Попытаемся растянуть таблицу на всю доступную область
			int nDelta = nRealWidth - nWidthTable;
			int nNotFixedColumns = 0;
			for (pos = 0; pos < cur_table.fixed_width.size(); ++pos)
			{
				if (!cur_table.fixed_width [pos])
					nNotFixedColumns++;
			} //for
			for (pos = 0; (pos < cur_table.fixed_width.size()) && (nNotFixedColumns > 0); ++pos)
			{
				if (!cur_table.fixed_width [pos])
				{
					int nStep = nDelta / nNotFixedColumns;
					cur_table.width [pos] += nStep;
					nDelta -= nStep;
					nNotFixedColumns--;
					nWidthTable += nStep;
				} //if
			} //for
		} //if

		if (nWidthTable < nRealWidth)
		{
			switch (m_defStyle.nHorzAlign)
			{
			case ALIGN_RIGHT:
				rcTable.left = lpRect->right - nWidthTable;
				break;
			case ALIGN_CENTER:
				rcTable.left += (nRealWidth - nWidthTable) / 2;
				break;
			} //switch
		} //if
		rcTable.right = rcTable.left + nWidthTable;

		//Calculate the real column's width and row's height
//		if (CPPDrawManager::PEN_DOUBLE == m_defStyle.nBorderStyle)
//			rcTable.bottom += m_defStyle.nBorderWidth * 6;
//		else
//			rcTable.bottom += m_defStyle.nBorderWidth * 2;
	} //if

	//Draw table border
	if (MODE_DRAW == m_nNumPass)
	{
		if (m_defStyle.nFillBkgnd >= 0)
		{
			m_drawmanager.FillEffect(m_hDC, m_defStyle.nFillBkgnd, &rcTable, 
				m_defStyle.crBkgnd, m_defStyle.crMidBkgnd, m_defStyle.crEndBkgnd,
				5);
		}
		else if (!m_defStyle.strNameResBk.IsEmpty())
		{
			DrawBackgroundImage(m_hDC, rcTable.left, rcTable.top, rcTable.right - rcTable.left, rcTable.bottom - rcTable.top, m_defStyle.strNameResBk);
		} //if
		if (m_defStyle.nBorderWidth > 0)
		{
			if (m_bIsEnable)
			{
				m_drawmanager.DrawRectangle(m_hDC, &rcTable, m_defStyle.crBorderLight, m_defStyle.crBorderDark,
					m_defStyle.nBorderStyle, m_defStyle.nBorderWidth);
			}
			else
			{
				m_drawmanager.DrawRectangle(m_hDC, &rcTable, m_crDisabled, m_crDisabled,
					m_defStyle.nBorderStyle, m_defStyle.nBorderWidth);
			} //if
		} //if
	} //if

	rcRow = rcTable;

	if (MODE_FIRSTPASS != m_nNumPass)
	{
		if (CPPDrawManagerW::PEN_DOUBLE == m_defStyle.nBorderStyle)
		{
			rcRow.left += 3;
			rcRow.top  += 3;
			rcRow.right -= 3;
			rcRow.bottom -= 3;
		}
		else
		{
			rcRow.left += m_defStyle.nBorderWidth;
			rcRow.top  += m_defStyle.nBorderWidth;
			rcRow.right -= m_defStyle.nBorderWidth;
			rcRow.bottom -= m_defStyle.nBorderWidth;
		}
	} //if

	if (szTable.cx && szTable.cy)
	{
		int nNewRow = 0;
		int nEndRow;
		CPPString sTagName, sTagParam, sRow;
		for (i = 0; i < szTable.cy; ++i)
		{
			//ENG: Searching a begin of the row
			//RUS: Поиск начала строки
			if (SearchTag(sTable, nNewRow, L"tr"))
			{
				//ENG: The begin of the row was found. Searching end of the row
				//RUS: Начало строки найдено. Ищем окончание строки
				nEndRow = nNewRow;
				SearchEndOfRow(sTable, nEndRow);
				//ENG: The end of the row was found
				//RUS: Окончание строки найдено
				sRow = sTable.Mid(nNewRow, nEndRow - nNewRow);
				
				//ENG: Draw a row of the table
				//RUS: Выводим строку таблицы
				DrawHtmlTableRow(sRow, &rcRow, cur_table, i);
				
				//ENG: Jump to char after the end of the row
				//RUS: Перемещаемся на символ, следующий за окончанием строки
				nNewRow = nEndRow + 5;
			} //if
		} //for
	} //if

	if (MODE_DRAW != m_nNumPass)
	{
		//ENG: Analysing cell's width
		//RUS: Анализ ширины ячейки
		for (i = 1; i <= szTable.cx; i++)
		{
			for (int y = 0; y < szTable.cy; y++)
			{
				vecRow & row = cur_table.cells [y];
				for (int x = 0; x < szTable.cx; x++)
				{
					STRUCT_CELL & sc = row [x];
					if (sc.nColSpan == i)
					{
						if (i == 1)
						{
							cur_table.width [x] = max (cur_table.width [x], sc.szCell.cx);
							if (sc.bFixedWidth)
								cur_table.fixed_width [x] = TRUE;
						}
						else
						{
							int span_width = 0;
							for (int z = 0; z < i; z++)
							{
								span_width += cur_table.width [x + z];
								if (sc.bFixedWidth)
									cur_table.fixed_width [x + z] = TRUE;
							} //for
							
							if (span_width < sc.szText.cx)
							{
								int step = (sc.szCell.cx - span_width) / i;
								cur_table.width [x + i - 1] += (sc.szCell.cx - span_width) % i;
								int z;
								for (z = 0; z < i; z++)
									cur_table.width [x + z] += step;
							} //if
						} //if
					} //if
				} //for
			} //for
		} //for

		//ENG: Analysing cell's height
		//RUS: Анализ высоты ячейки
		for (i = 1; i <= szTable.cy; i++)
		{
			for (int y = 0; y < szTable.cy; y++)
			{
				vecRow & row = cur_table.cells [y];
				for (int x = 0; x < szTable.cx; x++)
				{
					STRUCT_CELL & sc = row [x];
					if (sc.nRowSpan == i)
					{
						if (i == 1)
							cur_table.height [y] = max (cur_table.height [y], sc.szCell.cy);
						else
						{
							int span_height = 0;
							for (int z = 0; z < i; z++)
								span_height += cur_table.height [y + z];
							
							if (span_height < sc.szCell.cy)
							{
								int step = (sc.szCell.cy - span_height) / i;
								cur_table.height [y] += (sc.szCell.cy - span_height) % i;
								int z;
								for (z = 0; z < i; z++)
									cur_table.height [y + z] += step;
							} //if
						} //if
					} //if
				} //for
			} //for
		} //for

		size.cx += m_defStyle.nPadding + szTable.cx - 1;
		size.cy += m_defStyle.nPadding + szTable.cy - 1;
		for (i = 0; i < szTable.cx; i++)
			size.cx += cur_table.width [i] + m_defStyle.nPadding;
		for (i = 0; i < szTable.cy; i++)
			size.cy += cur_table.height [i] + m_defStyle.nPadding;
		
		if (CPPDrawManagerW::PEN_DOUBLE == m_defStyle.nBorderStyle)
		{
			size.cx += m_defStyle.nBorderWidth * 6;
			size.cy += m_defStyle.nBorderWidth * 6;
		}
		else
		{
			size.cx += m_defStyle.nBorderWidth * 2;
			size.cy += m_defStyle.nBorderWidth * 2;
		} //if

//		size.cx = GetTableWidth(strTable, 0, size.cx, TRUE);
	}
	else
	{
		size.cx = rcTable.right - rcTable.left;
		size.cy = rcTable.bottom - rcTable.top;
	} //if

	//ENG: Stores a current table
	//RUS: Сохраняем текущую таблицу
	m_arrTables [nIndexTable] = cur_table;

	//ENG: Restore styles before <table> tag
	//RUS: Восстанавливаем стили до тэга <table>
	m_defStyle.strTag = L"table";
	if (StoreRestoreStyle(TRUE))
		UpdateContext();

	return size;
} //End DrawHtmlTable

///////////////////////////////////////////////////////////////////////////////
// CPPHtmlDrawer::DrawHtmlTableRow
//	Draw a row of the table
//-----------------------------------------------------------------------------
// Parameters:
//		sRow	- a text of the cell with the tags. For example: "<tr>...</tr>"
//		lpRect	- a bounding rectangle for the row
//		st		- the info about current table
//		nRow	- the current row of the table
///////////////////////////////////////////////////////////////////////////////
//
///////////////////////////////////////////////////////////////////////////////
void CPPHtmlDrawerW::DrawHtmlTableRow(CPPString & sRow, LPCRECT lpRect, STRUCT_TABLE & st, int nRow)
{
	//ENG: Applies styles of <tr> tag
	//RUS: Применяем стили строки (тэг <tr>)
	m_defStyle.strTag = L"tr";
	StoreRestoreStyle(FALSE);
	SelectNewHtmlStyle(m_defStyle.strTag, m_defStyle);
	
	int nCol = 0;
	int i;
	vecRow & row = st.cells [nRow];
	
	//ENG: Passes a tag body and get a properties of the tag
	//RUS: Пропускаем тэг начала ячейки и получаем строку свойств тэга
	int nIndex = 0;
	CPPString sTag;
	SearchNextTag(sRow, sTag, nIndex);
	CPPString sProperties = SplitTag(sTag);

	//ENG: Analyses a properties of the tag
	//RUS: Анализируем свойства тэга
	AnalyseCellParam(sProperties, m_defStyle, FALSE);
	UpdateContext();
	
	while (nIndex < sRow.GetLength())
	{
		int nEndRow = nIndex;
		int nNewCell = nIndex;
		//ENG: Search an end of the cell or a begin of the nested table
		//RUS: Ищем конец ячейки или начало вложенной таблицы
		SearchTag(sRow, nEndRow, L"/tr");
		SearchTag(sRow, nNewCell, L"td");
		if (nNewCell < nEndRow)
		{
			//ENG: Search an existing cell
			//RUS: Поиск существующей ячейки
			STRUCT_CELL * sc2 = &row [nCol];
			while ((sc2->nColSpan < 0) && (nCol < (int)row.size())) 
			{
				nCol++;
				sc2 = &row [nCol];
			} //while
			STRUCT_CELL & sc = row [nCol];
			//ENG: Searching the end of the cell
			//RUS: Ищем окончание ячейки
			nIndex = nNewCell;
			SearchEndOfCell(sRow, nIndex);
			CPPString sCell = sRow.Mid(nNewCell, nIndex - nNewCell);

			RECT rcCell = {0, 0, 0, 0};
			if (MODE_FIRSTPASS != m_nNumPass)
			{
				//ENG: Gets a real rectangle to draw a cell
				//RUS: Получаем реальный прямоугольник для вывода ячейки
				rcCell = *lpRect;
				rcCell.left += m_defStyle.nPadding;
				for (i = 0; i < nCol; i++)
					rcCell.left += st.width [i] + m_defStyle.nPadding + 1;
				rcCell.right = rcCell.left;
				for (i = 0; i < sc.nColSpan; i++)
					rcCell.right += st.width [nCol + i];
				rcCell.right += (sc.nColSpan - 1) * (m_defStyle.nPadding + 1);
				
				rcCell.top += m_defStyle.nPadding;
				for (i = 0; i < nRow; i++)
					rcCell.top += st.height [i] + m_defStyle.nPadding + 1;
				rcCell.bottom = rcCell.top;
				for (i = 0; i < sc.nRowSpan; i++)
					rcCell.bottom += st.height [nRow + i];
				rcCell.bottom += (sc.nRowSpan - 1) * (m_defStyle.nPadding + 1);

				//ENG: cellspacing - margins from table's edge to the cell's edge
				//RUS: cellspacing - отступ от контура таблицы до ячейки 
//				rcCell.left += m_defStyle.nPadding;
//				rcCell.top += m_defStyle.nPadding;
//				rcCell.right -= m_defStyle.nPadding;
//				rcCell.bottom -= m_defStyle.nPadding;
			} //if

			DrawHtmlTableCell(sCell, &rcCell, sc);

			if (MODE_DRAW != m_nNumPass)
			{
				//ENG: Add a cellspacing
				//RUS: Добавляем отступ ячейки от
//				sc.szCell.cx += m_defStyle.nPadding + m_defStyle.nPadding;
//				sc.szCell.cy += m_defStyle.nPadding + m_defStyle.nPadding;
				
				//ENG: Stores a span cells
				//RUS: Запоминаем объединенные ячейки
				int nColSpan = sc.nColSpan + nCol;
				int nRowSpan = sc.nRowSpan + nRow;
				for (i = nCol + 1; i < nColSpan; i++)
				{
					STRUCT_CELL & scTemp = row [i];
					scTemp.nColSpan = -1;
					scTemp.nRowSpan = -1;
				} //for
				for (i = nRow + 1; i < nRowSpan; i++)
				{
					vecRow & rowTemp = st.cells [i];
					STRUCT_CELL & scTemp = rowTemp [nCol];
					scTemp.nColSpan = -1;
					scTemp.nRowSpan = -1;
				} //for
			} //if
			nCol += sc.nColSpan;
		}
		else
		{
			nIndex = sRow.GetLength();
		} //if
	} //while

	//ENG: Restore styles before <tr> tag
	//RUS: Восстанавливаем стили до тэга <tr>
	m_defStyle.strTag = L"tr";
	if (StoreRestoreStyle(TRUE))
		UpdateContext();

} //End of DrawHtmlTableRow

///////////////////////////////////////////////////////////////////////////////
// CPPHtmlDrawer::DrawHtmlTableCell
//	Draw a table's cell
//-----------------------------------------------------------------------------
// Parameters:
//		sCell	- a text of the cell with the tags. For example: "<td>...</td>"
//		lpRect	- a bounding rectangle for cell
//		sc		- the info about current cell
///////////////////////////////////////////////////////////////////////////////
//
///////////////////////////////////////////////////////////////////////////////
void CPPHtmlDrawerW::DrawHtmlTableCell(CPPString & sCell, LPCRECT lpRect, STRUCT_CELL & sc)
{
	if (MODE_DRAW != m_nNumPass)
	{
		sc.szText.cx = 0;
		sc.szText.cy = 0;
	} //if
	
	RECT rcCell = *lpRect;
 //LPI	RECT rcText;
		RECT rcText = {0};

	//ENG: Applies styles of <td> tag
	//RUS: Применяем стили ячейки (тэг <td>)
	m_defStyle.strTag = L"td";
	StoreRestoreStyle(FALSE);
	SelectNewHtmlStyle(m_defStyle.strTag, m_defStyle);

	//ENG: Passes a tag body and get a properties of the tag
	//RUS: Пропускаем тэг начала ячейки и получаем строку свойств тэга
	int nIndex = 0;
	CPPString sTag;
	SearchNextTag(sCell, sTag, nIndex);
	CPPString sProperties = SplitTag(sTag);

	//ENG: Analyses a properties of the tag
	//RUS: Анализируем свойства тэга
	m_defStyle.nCellWidth = m_defStyle.nCellHeight = 0;
	m_defStyle.bCellWidthPercent = m_defStyle.bCellHeightPercent = FALSE;
	SIZE szSpan = AnalyseCellParam(sProperties, m_defStyle, FALSE);

	if (MODE_FIRSTPASS == m_nNumPass)
	{
		//ENG: Stores a cell span info
		//RUS: Сохраняем информацию об объединении ячеек
		sc.nColSpan = szSpan.cx;
		sc.nRowSpan = szSpan.cy;
		//ENG: Stores an info about the recommended cell sizes
		//RUS: Сохраняем информацию об рекомендованных размерах ячейки
//		sc.nWidth = m_defStyle.nCellWidth;
//		sc.bWidthPercent = m_defStyle.bCellWidthPercent;
//		sc.nHeight = m_defStyle.nCellHeight;
//		sc.bHeightPercent = m_defStyle.bCellHeightPercent;
		//ENG: 
		//RUS: Если указаны минимальные рaзмеры ячейки, то установить их как начальные
		sc.szText.cx = m_defStyle.nCellWidth;
//		sc.szText.cy = m_defStyle.nCellHeight;
		sc.szText.cy = 0;

		if (m_defStyle.nCellWidth > 0)
			sc.bFixedWidth = TRUE;

		rcText = rcCell;
		rcText.right = rcText.left + sc.szText.cx;
		rcText.bottom = rcText.top + sc.szText.cy;
	}
	else if (MODE_DRAW == m_nNumPass)
	{
		//ENG: cellspacing - margins from table's edge to the cell's edge
		//RUS: cellspacing - отступ от контура таблицы до ячейки 
		rcText = rcCell;

		if (m_defStyle.nFillBkgnd >= 0)
		{
			//ENG: Filling cell background
			//RUS: Заполнение фона ячейки
			m_drawmanager.FillEffect(m_hDC, m_defStyle.nFillBkgnd, &rcText, 
				m_defStyle.crBkgnd, m_defStyle.crMidBkgnd, m_defStyle.crEndBkgnd, 5);
		} //if
		
		//Draws the border
		if (m_bIsEnable) 
			m_drawmanager.DrawRectangle(m_hDC, &rcText, m_defStyle.crBorderDark, m_defStyle.crBorderLight, m_defStyle.nBorderStyle);
		else 
			m_drawmanager.DrawRectangle(m_hDC, &rcText, m_crDisabled, m_crDisabled, m_defStyle.nBorderStyle);
		
		//ENG: cellpadding - margin from cell's edge to the inside cell text
		//RUS: cellpadding - отступ от контура ячейки, до текста внутри ее
		rcText.left += m_defStyle.nMargin + m_defStyle.nBorderWidth;
		rcText.top += m_defStyle.nMargin + m_defStyle.nBorderWidth;
		rcText.right -= m_defStyle.nMargin + m_defStyle.nBorderWidth;
		rcText.bottom -= m_defStyle.nMargin + m_defStyle.nBorderWidth;
		
		//Vertical align
		switch (m_defStyle.nVertAlign)
		{
		case ALIGN_BOTTOM:
			rcText.top = rcText.bottom - sc.szText.cy;
			break;
		case ALIGN_VCENTER:
			rcText.top += (rcText.bottom - rcText.top - sc.szText.cy) / 2;
			break;
		} //switch
	} //if

	//ENG: Draws a cell
	//RUS: Вывод ячейки
	while(nIndex < sCell.GetLength())
	{
		int nEndCell = nIndex;
		int nNewTable = nIndex;
		//ENG: Search an end of the cell or a begin of the nested table
		//RUS: Ищем конец ячейки или начало вложенной таблицы
		SearchTag(sCell, nEndCell, L"/td");
		SearchTag(sCell, nNewTable, L"table");
		//ENG: Gets a nearly index of the tag
		//RUS: Получаем индекс ближайшего тэга
		int nNearlyTag = min(nEndCell, nNewTable);
		SIZE szTemp = {0, 0};
		if (nNearlyTag > nIndex)
		{
			//ENG: If between the last index and the current index there is a text
			//RUS: Если между последним индексом и текущим индексом существует текст
			CPPString sText = sCell.Mid(nIndex, nNearlyTag - nIndex);
			szTemp = DrawHtmlString(sText, &rcText);
			nIndex = nNearlyTag;
		} //if
		else if (nNewTable < nEndCell)
		{
			//ENG: A nested table was found
			//RUS: Найдена вложенная таблица
			nIndex = nNewTable;
			SearchEndOfTable(sCell, nIndex);
			CPPString sTable = sCell.Mid(nNewTable, nIndex - nNewTable);
			szTemp = DrawHtmlTable(sTable, &rcText); 
		}
		else
		{
			//ENG: Alas, it is the end of the cell
			//RUS: Конец ячейки
			nIndex = sCell.GetLength();
		} //if
		
		if (MODE_DRAW != m_nNumPass)
		{
			//ENG: On first and second passes we are calculate the dimensions of the cell
			//RUS: На первом и втором проходах вычисляем размеры ячейки.
			sc.szText.cx = max(szTemp.cx, sc.szText.cx);
			sc.szText.cy += szTemp.cy;
		} //if
		rcText.top += szTemp.cy;
	} //while

	if (MODE_DRAW != m_nNumPass)
	{
		//ENG: On first and second passes we are calculate the dimensions of the cell
		//RUS: На первом и втором проходах вычисляем размеры ячейки.
		sc.szCell.cx = max(m_defStyle.nCellWidth, sc.szText.cx);
		sc.szCell.cy = max(m_defStyle.nCellHeight, sc.szText.cy);

		//ENG: Add the margins of the text from the cell's edges
		//RUS: Добавляем отступы текста от границ ячейки
		sc.szCell.cx += 2 * (m_defStyle.nMargin + m_defStyle.nBorderWidth);
		sc.szCell.cy += 2 * (m_defStyle.nMargin + m_defStyle.nBorderWidth);
	} //if
		
	//ENG: Restore styles before <td> tag
	//RUS: Воостанавливаем стили, которые были до тэга <td>
	m_defStyle.strTag = L"td";
	if (StoreRestoreStyle(TRUE))
		UpdateContext();
}

SIZE CPPHtmlDrawerW::DrawHtmlString (CPPString & sHtml, LPCRECT lpRect)
{
	SIZE szTextArea = {0, 0};

	COLORREF clrShadow = m_bIsEnable ? m_crShadow : GetColorByName(L"");

	//ENG: For any string we are add a <body> tag as wrapper
	//RUS: Для любой строки добавляем тэг <body>
	sHtml = L"<body>" + sHtml;
	sHtml += L"</body>";

	//ENG: Bounding rectangle for a full text
	//RUS: Ограничивающий прямоугольник для вывода всего текста
	m_rcOutput.top = lpRect->top;
	m_rcOutput.left = lpRect->left;
	m_rcOutput.bottom = lpRect->bottom;
	m_rcOutput.right = lpRect->right;

	//ENG: The width of the bounding rectangle
	//RUS: Ширина ограничивающего прямоугольника
	int nTextWrapWidth = m_rcOutput.right - m_rcOutput.left;

	//ENG: A current position for output
	//RUS: Текущая позиция для вывода
	POINT ptOutput;
	ptOutput.x = lpRect->left;
	ptOutput.y = lpRect->top;

//	szTextArea.cx = szTextArea.cy = 0;
//	m_szOutput.cx = m_szOutput.cy = 0;

//	m_szOutput = CSize(0, 0);

	//ENG: If a text is empty
	//RUS: Если текста для вывода нет
//	if (str.IsEmpty())
//	{
//		szTextArea.cx = szTextArea.cy = 0;
//		return;
//	} //if

	int nFirstLine = m_nCurLine;

//	POINT pt;
//	pt.x = lpRect->left;
//	pt.y = lpRect->top;

	int y;
	SIZE sz;

	CPPString sText = L"";
	CPPString sTag = L""; //String of the tag
	CPPString sProperties = L""; //String of the tag's property
	CPPString sParameter = L"";
	CPPString sValue = L"";

	BOOL bCloseTag = FALSE; //TRUE if tag have symbol '\'

	//ENG: Initializing a new line
	//RUS: Инициализация новой строки
	ptOutput.x = InitNewLine(ptOutput.x);
	int nBeginLineX = ptOutput.x;
	int nSpacesInLine = m_hline.nSpaceChars;
	int nRealWidth = m_hline.nWidthLine;

	int nIndex = 0;
//LPI	int nBegin = 0;
	int i = 0;
	while (i < sHtml.GetLength())
	{
		//ENG: Searching a first tag
		//RUS: Поиск первого тэга
		sText = SearchNextTag(sHtml, sTag, i);
		sProperties = SplitTag(sTag);

		//ENG: Before a tag was exist a text
		//RUS: Перед тэгом есть текст для вывода
		if (!sText.IsEmpty())
		{
			//ENG: Transform text
			//RUS: Преобразуем текст
			switch (m_defStyle.nTextTransform)
			{
			case TEXT_TRANSFORM_UPPERCASE:
				//ENG: All chars make upper
				//RUS: Все символы переводим в верхний регистр
				sText.MakeUpper();
				break;
			case TEXT_TRANSFORM_LOWERCASE:
				//ENG: All chars make lower
				//RUS: Все символы переводим в нижний регистр
				sText.MakeLower();
				break;
			case TEXT_TRANSFORM_CAPITALIZE:
				//ENG: Each first char of a word to upper
				//RUS: Кадый первый символ слова в верхний регистр, остальные в нижний
				sText.MakeLower();
				for (nIndex = 0; nIndex < sText.GetLength(); nIndex++)
				{
					if ((sText.GetAt(nIndex) >= L'a') && (sText.GetAt(nIndex) <= L'z'))
					{
						if ((0 == nIndex) || (L' ' == sText.GetAt(nIndex - 1)))
							sText.SetAt(nIndex, sText.GetAt(nIndex) - L'a' + L'A');
					} //if
				} //if
				break;
			} //switch

			//RUS: Зацикливаем до тех пор, пока не будет выведен весь текст
			while (!sText.IsEmpty())
			{
				//ENG: Reset an additional interval for space chars
				//RUS: Сброс дополнительного интервала между словами
				::SetTextJustification(m_hDC, 0, 0);

				//ENG: Gets a size a output text
				//RUS: Получаем размер выводимого текста
				::GetTextExtentPoint32W(m_hDC, sText, sText.GetLength(), &sz);

				//ENG: Gets a real top coordinate to output with vertical alignment
				//RUS: Получаем реальную коодинату верха вывода с учетом вертикального выравнивания
				y = VerticalAlignText(ptOutput.y, sz.cy);

				CPPString sTemp = sText;
				int nMaxSize = nTextWrapWidth - ptOutput.x + m_rcOutput.left;

				if (m_nMaxWidth && ((nMaxSize - sz.cx) < 0) && nTextWrapWidth)
				{
					//ENG: Text wrap was enabled and text out for a bounding rectangle
					int nRealSize = nMaxSize;
					sTemp = GetWordWrap(sText, nTextWrapWidth, nRealSize);
					sz.cx = nRealSize;
				}
				else
				{
					sText.Empty();
				} //if

				if (MODE_DRAW == m_nNumPass)
				{
					if (sz.cx)
					{
						if ((0 == (nRealWidth - sz.cx)) && (L' ' == sTemp.GetAt(sTemp.GetLength() - 1)))
						{
							//ENG: Removes the right space chars for the last output in line
							//RUS: Если это последний вывод в строке, то убираем пробелы справа
							sTemp.TrimRight();
							nSpacesInLine = GetCountOfChars(sTemp);
							SIZE szTemp;
							::GetTextExtentPoint32W(m_hDC, sTemp, sTemp.GetLength(), &szTemp);
							nRealWidth -= (sz.cx - szTemp.cx);
						} //if

						if ((ALIGN_JUSTIFY == m_hline.nHorzAlign) && m_hline.bWrappedLine)
							::SetTextJustification(m_hDC, nMaxSize - nRealWidth, nSpacesInLine);
						nRealWidth -= sz.cx;
						
						//ENG: Gets a size a output text
						//RUS: Получаем размер выводимого текста
						::GetTextExtentPoint32W(m_hDC, sTemp, sTemp.GetLength(), &sz);
						
						//ENG: Stores a current area as a hyperlink area if it available
						//RUS: Сохранякм текущую область как область гиперлинка если он установлен
						StoreHyperlinkArea(ptOutput.x, y, ptOutput.x + sz.cx, y + sz.cy);
						
						//ENG: Real output a text
						//RUS: Вывод текста
						::TextOutW(m_hDC, ptOutput.x, y, sTemp, sTemp.GetLength());
						nSpacesInLine -= GetCountOfChars(sTemp);
						
						//ENG: If sets an overline style then draw a line over the text
						//RUS: Если установлен стиль overline, то рисуем линию над текстом
						if (m_defStyle.bOverlineFont)
						{
							HPEN hpenOverline = ::CreatePen(PS_SOLID, (m_defStyle.nWeightFont >= FW_BOLD) ? 2 : 1, m_defStyle.crText);
							HPEN hOldPen = (HPEN)::SelectObject(m_hDC, hpenOverline);
							::MoveToEx(m_hDC, ptOutput.x, y, NULL);
							::LineTo(m_hDC, ptOutput.x + sz.cx, y);
							::SelectObject(m_hDC, hOldPen);
						} //if
					} //if
				}
				else
				{
					//ENG: Stores a last horizontal alignment
					//RUS: Сохраняем последнее горизонтальное выравнивание
					m_hline.nHorzAlign = m_defStyle.nHorzAlign;

					//ENG:
					//RUS:
					m_hline.nSpaceChars += GetCountOfChars(sTemp);
				} //if

				//ENG: Moves to a right of the outputed text
				//RUS: Перемещаемся справа от выведенного текста
				ptOutput.x += sz.cx;
				if (!sText.IsEmpty())
				{
					//ENG: Not all text was printed (cause text wrap) 
					//RUS: Не вся строка еще вывелась (в случае переноса текста)
					m_hline.bWrappedLine = TRUE;
					Tag_NewLine(&ptOutput, 1, &szTextArea);
					nBeginLineX = ptOutput.x;
					nSpacesInLine = m_hline.nSpaceChars;
					nRealWidth = m_hline.nWidthLine;
				}
			} //while
		} //if

		//ENG: If tag was found then analyzing ...
		//RUS: Если тэг найден, анализируем ...
		if (!sTag.IsEmpty())
		{
			//ENG: Reset temporary parameters
			//RUS: Сброс временных параметров
			m_defStyle.strTag.Empty();
			bCloseTag = FALSE;
			
			//ENG: Get Tag's name
			//RUS: Получаем имя тэга
			nIndex = 0;
			
			//ENG: Searching a tag's value
			//RUS: Поиск значения тэга
			DWORD dwTag = GetTagFromList(sTag, m_defStyle.strTag, bCloseTag);
			
			//ENG: If a tag was found in a list of the tags
			//RUS: Если тэг найден в списке
			if (TAG_NONE != dwTag)
			{
				//ENG: If it is a style tag 
				//RUS: Если текущий тэг для работы со стилями
				if (!m_defStyle.strTag.IsEmpty())
				{
					//ENG: Checks on permissibility of tag
					//RUS: Проверяем на допустимость тэга
					if (StoreRestoreStyle(bCloseTag))
					{
						//ENG: If it isn't a close tag
						//RUS: Если это не окончание тэга
						if (!bCloseTag)
						{
							//ENG: Processing a tag
							//RUS: Обработка тэга
							switch (dwTag)
							{
							case TAG_BOLD:
								m_defStyle.nWeightFont <<= 1;
								if (m_defStyle.nWeightFont > FW_BLACK)
									m_defStyle.nWeightFont = FW_BLACK;
								break;
							case TAG_ITALIC:
								m_defStyle.bItalicFont = m_defStyle.bItalicFont ? FALSE : TRUE;
								break;
							case TAG_UNDERLINE:
								m_defStyle.bUnderlineFont = m_defStyle.bUnderlineFont ? FALSE : TRUE;
								break;
							case TAG_STRIKEOUT:
								m_defStyle.bStrikeOutFont = m_defStyle.bStrikeOutFont ? FALSE : TRUE;
								break;
							case TAG_FONT:
								//Search parameters
								while (nIndex < sProperties.GetLength())
								{
									//ENG: Searching a parameters of a tag
									//RUS: Поиск параметров тэга
									sValue = GetNextProperty(sProperties, nIndex, sParameter);
									//ENG: If a parameter was found
									//RUS: Если параметр найден
									if (!sParameter.IsEmpty())
									{
										//ENG: Processing a parameters of a tag
										//RUS: Обработка параметров тэга
										if (sParameter == L"face")
											m_defStyle.sFaceFont = GetStyleString(sValue, m_defStyle.sFaceFont);
										else if (sParameter == L"size")
											m_defStyle.nSizeFont = GetLengthUnit(sValue, m_defStyle.nSizeFont, TRUE);
										else if (sParameter == L"color")
										{
											if (m_bIsEnable)
												m_defStyle.crText = GetStyleColor(sValue, m_defStyle.crText);
											else
												m_defStyle.crText = GetColorByName(L"");
										}
										else if (sParameter == L"style")
											GetStyleFontShortForm(sValue);
										else if (sParameter == L"weight")
											m_defStyle.nWeightFont = GetStyleFontWeight(sValue, m_defStyle.nWeightFont);
										else if (sParameter == L"bkgnd")
										{
											if (((sValue == L"transparent") && sValue.IsEmpty()) || !m_bIsEnable)
											{
												m_defStyle.nBkMode = TRANSPARENT;
											}
											else
											{
												m_defStyle.nBkMode = OPAQUE;
												m_defStyle.crBkgnd = GetStyleColor(sValue, m_defStyle.crBkgnd);
											} //if
										} //if
									} //if
								} //while
								break;
							case TAG_LEFT:
								m_defStyle.nHorzAlign = ALIGN_LEFT;
								break;
							case TAG_CENTER:
								m_defStyle.nHorzAlign = ALIGN_CENTER;
								break;
							case TAG_RIGHT:
								m_defStyle.nHorzAlign = ALIGN_RIGHT;
								break;
							case TAG_JUSTIFY:
								m_defStyle.nHorzAlign = ALIGN_JUSTIFY;
								break;
							case TAG_BASELINE:
								m_defStyle.nVertAlign = ALIGN_BASELINE;
								break;
							case TAG_TOP:
								m_defStyle.nVertAlign = ALIGN_TOP;
								break;
							case TAG_VCENTER:
								m_defStyle.nVertAlign = ALIGN_VCENTER;
								break;
							case TAG_BOTTOM:
								m_defStyle.nVertAlign = ALIGN_BOTTOM;
								break;
							case TAG_NEWSTYLE:
								SelectNewHtmlStyle(sTag, m_defStyle);
								break;
							case TAG_SPAN:
								while (nIndex < sProperties.GetLength())
								{
									//ENG: Searching a parameters of a tag
									//RUS: Поиск параметров тэга
									sValue = GetNextProperty(sProperties, nIndex, sParameter);
									//ENG: If a parameter was found
									//RUS: Если параметр найден
									if (sParameter == L"class")
										SelectNewHtmlStyle(L"." + GetStyleString(sValue, L""), m_defStyle);
								} //while
								break;
							case TAG_HYPERLINK:
								//ENG: A default values
								//RUS: Значения по умолчанию
								m_defStyle.nTypeLink = LINK_MESSAGE;
								m_defStyle.sHyperlink.Empty();
								while (nIndex < sProperties.GetLength())
								{
									//ENG: Searching a parameters of a tag
									//RUS: Поиск параметров тэга
									sValue = GetNextProperty(sProperties, nIndex, sParameter);
									//ENG: If a parameter was found
									//RUS: Если параметр найден
									if (!sParameter.IsEmpty())
									{
										//ENG: Processing a parameters of a tag
										//RUS: Обработка параметров тэга
										if (sParameter == L"href")
										{
											m_defStyle.nTypeLink = LINK_HREF;
											m_defStyle.sHyperlink = GetStyleString(sValue, L"");
										} //if
										if (sParameter == L"msg")
										{
											m_defStyle.nTypeLink = LINK_MESSAGE;
											m_defStyle.sHyperlink = GetStyleString(sValue, L"");
										} //if
									} //if
								} //while
								//ENG: Gets a index of a current link
								//RUS: Получаем индекс текущей гиперссылки
								m_nCurIndexLink ++;
								//ENG: If a mouse over this link
								//RUS: Если мыш над этим тэгом
								if (m_nCurIndexLink == m_nHoverIndexLink)
									SelectNewHtmlStyle(L"a:hover", m_defStyle);
								else
									SelectNewHtmlStyle(L"a:link", m_defStyle);
								break;
								} //switch
							} //if
							//ENG: Update a device context
							//RUS: Обновление контекста устройства
							UpdateContext();
						} //if
					}
					else 
					{
						BOOL bPercent;
						BOOL bShadow;
						BOOL bAutoDelete;
						int nWidth, nNum;
						
						STRUCT_IMAGE si;
						STRUCT_CHANGESTYLE csTemp; //Temporary structure
						STRUCT_ANIMATION sa;
						
						SIZE szReal;
						HBITMAP hBitmap = NULL;;
						HICON hIcon = NULL;
						
						DWORD nMaxCol, nMaxRow;
						UINT nIdRes, nIdDll;
						//CPPString str;
						
						//ENG: Processing a tag
						//RUS: Обработка тэга
						switch (dwTag)
						{
						case TAG_HLINE:
							//ENG: Draws the horizontal line
							//RUS: Рисование горизонтальной линии
							csTemp = m_defStyle;
							csTemp.nBorderWidth = 1;
							//ENG: Applies a new styles for <hr> tag
							SelectNewHtmlStyle(L"hr", csTemp);
							nWidth = 100;
							bPercent = TRUE;
							
							while (nIndex < sProperties.GetLength())
							{
								//ENG: Searching a parameters of a tag
								//RUS: Поиск параметров тэга
								sValue = GetNextProperty(sProperties, nIndex, sParameter);
								//ENG: If a parameter was found
								//RUS: Если параметр найден
								if (!sParameter.IsEmpty())
								{
									//ENG: Processing a parameters of a tag
									//RUS: Обработка параметров тэга
									if (sParameter == L"width")
									{
										bPercent = IsPercentableValue(sValue);
										nWidth = GetLengthUnit(sValue, 100);
									}
									else if (sParameter == L"size")
										csTemp.nBorderWidth = GetLengthUnit(sValue, csTemp.nBorderWidth);
									else if (sParameter == L"color")
									{
										if (m_bIsEnable)
											csTemp.crText = GetStyleColor(sValue, csTemp.crText);
										else
											csTemp.crText = GetColorByName(L"");
									}
								} //if
							} //while
							
							if (bPercent)
							{
								if (MODE_FIRSTPASS == m_nNumPass)
								{
									m_hline.nAddPercentWidth += nWidth;
									nWidth = 1;
								}
								else nWidth = ::MulDiv(lpRect->right - lpRect->left, nWidth, 100);
							} //if
							
							if (MODE_FIRSTPASS == m_nNumPass)
							{
								m_hline.nHeightLine = max(m_hline.nHeightLine, csTemp.nBorderWidth + 8);
								m_hline.nHorzAlign = m_defStyle.nHorzAlign; //Store a last horizontal alignment
							}
							else if (MODE_DRAW == m_nNumPass)
							{
								m_drawmanager.DrawLine(m_hDC, ptOutput.x, ptOutput.y + m_hline.nHeightLine / 2, 
									ptOutput.x + nWidth, ptOutput.y + m_hline.nHeightLine / 2, 
									csTemp.crText, CPPDrawManagerW::PEN_SOLID, csTemp.nBorderWidth);
							} //if
							ptOutput.x += nWidth;
							break;
						case TAG_NEWLINE:
							//ENG: New line
							//RUS: Новая строка
							nNum = 1;
							if (!sProperties.IsEmpty())
							{
								sProperties = sProperties.Mid(1);
								nNum = GetLengthUnit(sProperties, nNum);
							} //if
							m_hline.bWrappedLine = FALSE;
							Tag_NewLine(&ptOutput, nNum, &szTextArea);
							nBeginLineX = ptOutput.x;
							nSpacesInLine = m_hline.nSpaceChars;
							nRealWidth = m_hline.nWidthLine;
							break;
						case TAG_TABULATION:
							//ENG: Tabulation
							//RUS: Табуляция
							nNum = 1;
							if (!sProperties.IsEmpty())
							{
								sProperties = sProperties.Mid(1);
								nNum = GetLengthUnit(sProperties, nNum);
							} //if
							Tag_Tabulation(&ptOutput, nNum);
							break;
						case TAG_BITMAP:
							//-----------------------------
							//Draws the bitmap 
							//-----------------------------
							//ENG: Default Parameters
							//RUS: Параметры по умолчанию
							si.nIdRes = 0;
							si.nIdDll = 0;
							si.nHandle = 0;
							si.nWidth = 100;
							si.bPercentWidth = TRUE;
							si.nHeight = 100;
							si.bPercentHeight = TRUE;
							si.crMask = RGB(255, 0, 255);
							si.bUseMask = FALSE;
							si.nStyles = 0;
							si.nHotStyles = 0;
							si.strSrcFile.Empty();
							si.strPathDll.Empty();
							
							//ENG: Searching image parameters
							//RUS: Поиск параметров изображения
							AnalyseImageParam(sProperties, si);
							
							//ENG: If a image's source was specified
							//RUS: Если указан источник изображения
							if (si.nIdRes || si.nIdDll || si.nHandle || !si.strSrcFile.IsEmpty())
							{
								//ENG: Sets a autodelete flag of the image object
								//RUS: Установлен флаг автоматического удаления объекта изображения
								bAutoDelete = TRUE;
								
								//ENG: Gets a handle of the image
								//RUS: Получить дескриптор изображения
								if (si.nIdRes)
									hBitmap = GetBitmapFromResources(si.nIdRes);
								else if (!si.strSrcFile.IsEmpty())
									hBitmap = GetBitmapFromFile(si.strSrcFile);
								else if (si.nIdDll)
									hBitmap = GetBitmapFromDll(si.nIdDll, si.strPathDll);
								else if (si.nHandle)
								{
									hBitmap = (HBITMAP)si.nHandle;
									//ENG: If an image handle specified, disables autodelete
									//RUS: Если указан дескриптор изображения, то запрещаем удаление
									bAutoDelete = FALSE;
								} //if
								
								//ENG: If a handle of an image was retrieved
								//RUS: Если дескриптор изображения получен
								if (NULL != hBitmap)
								{
									//ENG: Image with shadow or not?
									//RUS: Изображение с тенью или нет
									bShadow = IsImageWithShadow(si);
									
									//ENG: Retrieves an original size of an image
									//RUS: Получаем оригинальный размер изображения
									m_drawmanager.GetSizeOfBitmap(hBitmap, &sz);
									
									//ENG: Retrieves an output size
									//RUS: Получаем размеры для рисования
									if (si.bPercentWidth) si.nWidth = ::MulDiv(sz.cx, si.nWidth, 100);
									if (si.bPercentHeight) si.nHeight = ::MulDiv(sz.cy, si.nHeight, 100);
									
									//ENG: If a shadow was enabled then set a real size
									//RUS: Если тень доступна, то устанавливаем реальный размер
									if (si.nWidth && si.nHeight && bShadow)
									{
										sz.cx = si.nWidth + m_szOffsetShadow.cx;
										sz.cy = si.nHeight + m_szOffsetShadow.cy;
									} //if
									
									int nMaxSize = nTextWrapWidth - ptOutput.x + m_rcOutput.left;
									if (m_nMaxWidth && ((nMaxSize - sz.cx) < 0) && nTextWrapWidth) 
									{
										//ENG: Not all text was printed (cause text wrap) 
										//RUS: Не вся строка еще вывелась (в случае переноса текста)
										m_hline.bWrappedLine = TRUE;
										Tag_NewLine(&ptOutput, 1, &szTextArea);
										nBeginLineX = ptOutput.x;
										nSpacesInLine = m_hline.nSpaceChars;
										nRealWidth = m_hline.nWidthLine;
									} //if
									nRealWidth -= sz.cx;

									//ENG: Store a last horizontal alignment
									//RUS: Запоминаем последнее горизонтальное выравнивание
									if (MODE_FIRSTPASS == m_nNumPass) 
										m_hline.nHorzAlign = m_defStyle.nHorzAlign;
									
									//ENG: Retrieves a vertical coordinates of drawing area
									//RUS: Получаем вертикальную координату области рисования
									y = VerticalAlignImage(ptOutput.y, si.nHeight);
									
									//ENG: If an image is exist and not prepare mode
									//RUS: Если изображение доступно и не установлен режим подготовки
									if (si.nWidth && si.nHeight && (MODE_DRAW == m_nNumPass))
									{
										//ENG: Add an output area to hyperlink list if needed
										//RUS: Если необходимо добавляем область вывода в список гиперссылок
										StoreHyperlinkArea(ptOutput.x, y, ptOutput.x + sz.cx, y + sz.cy);
										
										//ENG: If a mouse over an image then applies a hot styles
										//RUS: Если мышь над изображением, то применяем соотвествующие стили
										if (m_defStyle.nTypeLink != LINK_NONE)
										{
											if (m_nCurIndexLink == m_nHoverIndexLink)
												si.nStyles = si.nHotStyles;
										} //if
										
										if (!m_bIsEnable)
											si.nStyles = (si.nStyles & 0xFF00) | IMAGE_EFFECT_MONOCHROME;
										
										//ENG: Drawing an image
										//RUS: Рисование изображения
										m_drawmanager.DrawBitmap(m_hDC, ptOutput.x, y, si.nWidth, si.nHeight, hBitmap, 
											si.bUseMask, si.crMask, si.nStyles, 
											bShadow, 
											m_szOffsetShadow.cx, m_szOffsetShadow.cy, 
											m_szDepthShadow.cx, m_szDepthShadow.cy, 
											clrShadow);
									} //if
									
									//ENG: Moves to a right of the outputed image
									//RUS: Перемещаемся справа от выведенного изображения
									ptOutput.x += sz.cx; //si.nWidth;
									
									//ENG: If needed delete a handle of an image
									//RUS: Если необходимо удаляем дескриптор изображения
									if (bAutoDelete)
										::DeleteObject(hBitmap);
								} //if
							} //if
							break;
						case TAG_ICON:
							//-----------------------------
							//Draws the icon
							//-----------------------------
							//ENG: Default Parameters
							//RUS: Параметры по умолчанию
							si.nIdRes = 0;
							si.nIdDll = 0;
							si.nHandle = 0;
							si.nWidth = 100;
							si.bPercentWidth = TRUE;
							si.nHeight = 100;
							si.bPercentHeight = TRUE;
							si.nStyles = 0;
							si.nHotStyles = 0;
							si.strSrcFile.Empty();
							si.strPathDll.Empty();
							
							//ENG: Searching image parameters
							//RUS: Поиск параметров изображения
							AnalyseImageParam(sProperties, si);
							
							//ENG: If a image's source was specified
							//RUS: Если указан источник изображения
							if (si.nIdRes || si.nIdDll || si.nHandle || !si.strSrcFile.IsEmpty())
							{
								//ENG: Sets a autodelete flag of the image object
								//RUS: Установлен флаг автоматического удаления объекта изображения
								bAutoDelete = TRUE;
								
								//RUS: Получаем требуемый размер иконки
								sz.cx = si.nWidth;
								sz.cy = si.nHeight;
								if (si.bPercentWidth) sz.cx = ::MulDiv(::GetSystemMetrics(SM_CXICON), si.nWidth, 100);
								if (si.bPercentHeight) sz.cy = ::MulDiv(::GetSystemMetrics(SM_CYICON), si.nHeight, 100);
								
								//ENG: Gets a handle of the image
								//RUS: Получить дескриптор изображения
								if (si.nIdRes)
									hIcon = GetIconFromResources(si.nIdRes, sz.cx, sz.cy);
								else if (!si.strSrcFile.IsEmpty())
									hIcon = GetIconFromFile(si.strSrcFile, sz.cx, sz.cy);
								else if (si.nIdDll)
									hIcon = GetIconFromDll(si.nIdDll, sz.cx, sz.cy, si.strPathDll);
								else if (si.nHandle)
								{
									hIcon = (HICON)si.nHandle;
									
									//ENG: If an image handle specified, disables autodelete
									//RUS: Если указан дескриптор изображения, то запрещаем удаление
									bAutoDelete = FALSE;
								} //if
								
								//ENG: If a handle of an image was retrieved
								//RUS: Если дескриптор изображения получен
								if (NULL != hIcon)
								{
									//ENG: Image with shadow or not?
									//RUS: Изображение с тенью или нет
									BOOL bShadow = IsImageWithShadow(si);
									
									//ENG: Retrieves an original size of an image
									//RUS: Получаем оригинальный размер изображения
									m_drawmanager.GetSizeOfIcon(hIcon, &sz);
									si.nWidth = sz.cx;
									si.nHeight = sz.cy;
									
									//ENG: Retrieves an output size
									//RUS: Получаем размеры для рисования
									//									if (si.bPercentWidth) si.nWidth = ::MulDiv(sz.cx, si.nWidth, 100);
									//									if (si.bPercentHeight) si.nHeight = ::MulDiv(sz.cy, si.nHeight, 100);
									
									//ENG: If a shadow was enabled then set a real size
									//RUS: Если тень доступна, то устанавливаем реальный размер
									if (si.nWidth && si.nHeight && bShadow)
									{
										sz.cx = si.nWidth + m_szOffsetShadow.cx;
										sz.cy = si.nHeight + m_szOffsetShadow.cy;
									} //if

									int nMaxSize = nTextWrapWidth - ptOutput.x + m_rcOutput.left;
									if (m_nMaxWidth && ((nMaxSize - sz.cx) < 0) && nTextWrapWidth) 
									{
										//ENG: Not all text was printed (cause text wrap) 
										//RUS: Не вся строка еще вывелась (в случае переноса текста)
										m_hline.bWrappedLine = TRUE;
										Tag_NewLine(&ptOutput, 1, &szTextArea);
										nBeginLineX = ptOutput.x;
										nSpacesInLine = m_hline.nSpaceChars;
										nRealWidth = m_hline.nWidthLine;
									} //if
									nRealWidth -= sz.cx;
									
									//ENG: Store a last horizontal alignment
									//RUS: Запоминаем последнее горизонтальное выравнивание
									if (MODE_FIRSTPASS == m_nNumPass) 
										m_hline.nHorzAlign = m_defStyle.nHorzAlign;
									
									//ENG: Retrieves a vertical coordinates of drawing area
									//RUS: Получаем вертикальную координату области рисования
									y = VerticalAlignImage(ptOutput.y, si.nHeight);
									
									//ENG: If an image is exist and not prepare mode
									//RUS: Если изображение доступно и не установлен режим подготовки
									if (si.nWidth && si.nHeight && (MODE_DRAW == m_nNumPass))
									{
										//ENG: Add an output area to hyperlink list if needed
										//RUS: Если необходимо добавляем область вывода в список гиперссылок
										StoreHyperlinkArea(ptOutput.x, y, ptOutput.x + sz.cx, y + sz.cy);
										
										//ENG: If a mouse over an image then applies a hot styles
										//RUS: Если мышь над изображением, то применяем соотвествующие стили
										if (m_defStyle.nTypeLink != LINK_NONE)
										{
											if (m_nCurIndexLink == m_nHoverIndexLink)
												si.nStyles = si.nHotStyles;
										} //if
										
										if (!m_bIsEnable)
											si.nStyles = (si.nStyles & 0xFF00) | IMAGE_EFFECT_MONOCHROME;
										
										//ENG: Drawing an image
										//RUS: Рисование изображения
										m_drawmanager.DrawIcon(m_hDC, ptOutput.x, y, si.nWidth, si.nHeight, hIcon, si.nStyles, 
											bShadow, 
											m_szOffsetShadow.cx, m_szOffsetShadow.cy, 
											m_szDepthShadow.cx, m_szDepthShadow.cy, 
											clrShadow);
									} //if
									//ENG: Moves to a right of the outputed image
									//RUS: Перемещаемся справа от выведенного изображения
									ptOutput.x += sz.cx; //si.nWidth;
									
									//ENG: If needed delete a handle of an image
									//RUS: Если необходимо удаляем дескриптор изображения
									if (bAutoDelete) 
										::DestroyIcon(hIcon);
								} //if
							} //if
							break;
						case TAG_IMAGELIST:
							//-----------------------------
							//Draws the icon from image list
							//-----------------------------
							//ENG: Default Parameters
							//RUS: Параметры по умолчанию
							si.nIndexImageList = 0;
							si.nIdRes = 0;
							si.nIdDll = 0;
							si.nHandle = 0;
							si.nWidth = 100;
							si.bPercentWidth = TRUE;
							si.nHeight = 100;
							si.bPercentHeight = TRUE;
							si.nSpeed = 0;
							si.bUseMask = FALSE;
							si.crMask = RGB(255, 0, 255);
							si.cx = 0;//GetSystemMetrics(SM_CXICON);
							si.cy = 0;//GetSystemMetrics(SM_CYICON);
							si.nStyles = 0;
							si.nHotStyles = 0;
							si.strSrcFile.Empty();
							si.strPathDll.Empty();
							
							//ENG: Searching image parameters
							//RUS: Поиск параметров изображения
							AnalyseImageParam(sProperties, si);
							
							//ENG: Image with shadow or not?
							//RUS: Изображение с тенью или нет
							bShadow = IsImageWithShadow(si);
							
							if (si.nIdRes || si.nIdDll || si.nHandle || !si.strSrcFile.IsEmpty())
							{
								//ENG: Sets a autodelete flag of the image object
								//RUS: Установлен флаг автоматического удаления объекта изображения
								bAutoDelete = TRUE;
								
								//ENG: Gets a handle of the image
								//RUS: Получить дескриптор изображения
								if (si.nIdRes)
									hBitmap = GetBitmapFromResources(si.nIdRes);
								else if (!si.strSrcFile.IsEmpty())
									hBitmap = GetBitmapFromFile(si.strSrcFile);
								else if (si.nIdDll)
									hBitmap = GetBitmapFromDll(si.nIdDll, si.strPathDll);
								else if (si.nHandle)
								{
									hBitmap = (HBITMAP)si.nHandle;
									//ENG: If an image handle specified, disables autodelete
									//RUS: Если указан дескриптор изображения, то запрещаем удаление
									bAutoDelete = FALSE;
								} //if
								
								//ENG: If a handle of an image was retrieved
								//RUS: Если дескриптор изображения получен
								if (NULL != hBitmap)
								{
									//ENG: Retrieves an original size of an image
									//RUS: Получаем оригинальный размер изображения
									m_drawmanager.GetSizeOfBitmap(hBitmap, &sz);

									//ENG: Creates a no specified sizes
									//RUS: Создаем незаданные размеры
									if (!si.cx && !si.cy)
										si.cx = si.cy = min(sz.cx, sz.cy);
									else if (!si.cx)
										si.cx = si.cy;
									else if (!si.cy)
										si.cy = si.cx;
									
									//ENG: Retrieves an output size
									//RUS: Получаем размеры для рисования
									if (si.bPercentWidth) si.nWidth = ::MulDiv(si.cx, si.nWidth, 100);
									if (si.bPercentHeight) si.nHeight = ::MulDiv(si.cy, si.nHeight, 100);
									
									//ENG: If a shadow was enabled then set a real size
									//RUS: Если тень доступна, то устанавливаем реальный размер
									szReal.cx = si.nWidth;
									szReal.cy = si.nHeight;
									if (si.nWidth && si.nHeight && bShadow)
									{
										szReal.cx += m_szOffsetShadow.cx;
										szReal.cy += m_szOffsetShadow.cy;
									} //if
									
									//ENG: Gets a max columns and rows of the images on the bitmap
									//RUS: Получаем максимальное число колонок и строк изображений на битмапке
									nMaxCol = sz.cx / si.cx;
									nMaxRow = sz.cy / si.cy;
									
									if (si.nSpeed)
									{
										if (MODE_FIRSTPASS == m_nNumPass)
										{
											sa.nIndex = si.nIndexImageList;
											sa.nMaxImages = nMaxCol * nMaxRow;
											sa.nSpeed = si.nSpeed;
											sa.nTimerCount = 0;
											m_arrAni.push_back(sa);
										}
										else if (MODE_DRAW == m_nNumPass)
										{
											m_nCurIndexAni ++;
											sa = m_arrAni [m_nCurIndexAni];
											si.nIndexImageList = sa.nIndex;
										} //if
									} //if
									
									//ENG: If a specified index of image is a legitimate value
									//RUS: Если указанный индекс изображения допустим
									if ((si.nIndexImageList < (int)(nMaxCol * nMaxRow)) && nMaxCol && nMaxRow)
									{
										int nMaxSize = nTextWrapWidth - ptOutput.x + m_rcOutput.left;
										if (m_nMaxWidth && ((nMaxSize - szReal.cx) < 0) && nTextWrapWidth) 
										{
											//ENG: Not all text was printed (cause text wrap) 
											//RUS: Не вся строка еще вывелась (в случае переноса текста)
											m_hline.bWrappedLine = TRUE;
											Tag_NewLine(&ptOutput, 1, &szTextArea);
											nBeginLineX = ptOutput.x;
											nSpacesInLine = m_hline.nSpaceChars;
											nRealWidth = m_hline.nWidthLine;
										} //if
										nRealWidth -= szReal.cx;
										
										//ENG: Store a last horizontal alignment
										//RUS: Запоминаем последнее горизонтальное выравнивание
										if (MODE_FIRSTPASS == m_nNumPass) 
											m_hline.nHorzAlign = m_defStyle.nHorzAlign;
										
										//ENG: Retrieves a vertical coordinates of drawing area
										//RUS: Получаем вертикальную координату области рисования
										y = VerticalAlignImage(ptOutput.y, szReal.cy);
										
										//ENG: If an image is exist and not prepare mode
										//RUS: Если изображение доступно и не установлен режим подготовки
										if (si.nWidth && si.nHeight && (MODE_DRAW == m_nNumPass))
										{
											//ENG: Add an output area to hyperlink list if needed
											//RUS: Если необходимо добавляем область вывода в список гиперссылок
											StoreHyperlinkArea(ptOutput.x, y, ptOutput.x + szReal.cx, y + szReal.cy);
											
											//ENG: If a mouse over an image then applies a hot styles
											//RUS: Если мышь над изображением, то применяем соотвествующие стили
											if (m_defStyle.nTypeLink != LINK_NONE)
											{
												if (m_nCurIndexLink == m_nHoverIndexLink)
													si.nStyles = si.nHotStyles;
											} //if
											
											if (!m_bIsEnable)
												si.nStyles = (si.nStyles & 0xFF00) | IMAGE_EFFECT_MONOCHROME;
											
											//ENG: Drawing an image
											//RUS: Рисование изображения
											m_drawmanager.DrawImageList(m_hDC, ptOutput.x, y, si.nWidth, si.nHeight, hBitmap,
												si.nIndexImageList, si.cx, si.cy,
												si.bUseMask, si.crMask, si.nStyles, 
												bShadow, 
												m_szOffsetShadow.cx, m_szOffsetShadow.cy, 
												m_szDepthShadow.cx, m_szDepthShadow.cy, 
												clrShadow);
										} //if
										
										//ENG: Moves to a right of the outputed image
										//RUS: Перемещаемся справа от выведенного изображения
										ptOutput.x += szReal.cx;
									} //if
									
									//ENG: If needed delete a handle of an image
									//RUS: Если необходимо удаляем дескриптор изображения
									if (bAutoDelete)
										::DeleteObject(hBitmap);
								} //if
							}
							else if (NULL != m_hImageList)
							{
								// Ensure that the common control DLL is loaded. 
								InitCommonControls(); 

								if ((int)si.nIndexImageList < ImageList_GetImageCount(m_hImageList))
								{
									hIcon = ImageList_ExtractIcon(NULL, m_hImageList, si.nIndexImageList);
									if (NULL != hIcon)
									{
										sz.cx = si.nWidth;
										sz.cy = si.nHeight;
										if (si.bPercentWidth) sz.cx = ::MulDiv(m_szImageList.cx, si.nWidth, 100);
										if (si.bPercentHeight) sz.cy = ::MulDiv(m_szImageList.cy, si.nHeight, 100);
										
										szReal.cx = sz.cx;
										szReal.cy = sz.cy;
										if (sz.cx && sz.cy && bShadow)
										{
											szReal.cx += m_szOffsetShadow.cx;
											szReal.cy += m_szOffsetShadow.cy;
										} //if
										
										int nMaxSize = nTextWrapWidth - ptOutput.x + m_rcOutput.left;
										if (m_nMaxWidth && ((nMaxSize - szReal.cx) < 0) && nTextWrapWidth) 
										{
											//ENG: Not all text was printed (cause text wrap) 
											//RUS: Не вся строка еще вывелась (в случае переноса текста)
											m_hline.bWrappedLine = TRUE;
											Tag_NewLine(&ptOutput, 1, &szTextArea);
											nBeginLineX = ptOutput.x;
											nSpacesInLine = m_hline.nSpaceChars;
											nRealWidth = m_hline.nWidthLine;
										} //if
										nRealWidth -= sz.cx;

										if (MODE_FIRSTPASS == m_nNumPass) 
											m_hline.nHorzAlign = m_defStyle.nHorzAlign; //Store a last horizontal alignment
										y = VerticalAlignImage(ptOutput.y, szReal.cy);
										if (sz.cx && sz.cy && (MODE_DRAW == m_nNumPass))
										{
											StoreHyperlinkArea(ptOutput.x, y, ptOutput.x + szReal.cx, y + szReal.cy);
											
											if (m_defStyle.nTypeLink != LINK_NONE)
											{
												if (m_nCurIndexLink == m_nHoverIndexLink)
													si.nStyles = si.nHotStyles;
											} //if
											
											if (!m_bIsEnable)
												si.nStyles = (si.nStyles & 0xFF00) | IMAGE_EFFECT_MONOCHROME;
											
											m_drawmanager.DrawIcon(m_hDC, ptOutput.x, y, 
												sz.cx, sz.cy, hIcon, si.nStyles, 
												bShadow, 
												m_szOffsetShadow.cx, m_szOffsetShadow.cy, 
												m_szDepthShadow.cx, m_szDepthShadow.cy, 
												clrShadow);
											::DestroyIcon(hIcon);
										} //if
										ptOutput.x += szReal.cx;
									} //if
								} //if
							} //if
							break;
						case TAG_STRING:
							//-----------------------------
							//Draws the string
							//-----------------------------
							nIdRes = 0;
							nIdDll = 0;
							sText.Empty();
							while (nIndex < sProperties.GetLength())
							{
								//ENG: Searching a parameters of a tag
								//RUS: Поиск параметров тэга
								sValue = GetNextProperty(sProperties, nIndex, sParameter);
								//ENG: If a parameter was found
								//RUS: Если параметр найден
								if (!sParameter.IsEmpty())
								{
									if (sParameter == L"idres")
										nIdRes = GetLengthUnit(sValue, nIdRes);
									else if (sParameter == L"iddll")
										nIdRes = GetLengthUnit(sValue, nIdDll);
									else if (sParameter == L"srcdll")
										sText = GetStyleString(sValue, sText);
								} //if
							} //while
							if (nIdRes || nIdDll)
							{
								if (nIdRes)
									sText = GetStringFromResource(nIdRes);
								else if (nIdDll)
									sText = GetStringFromDll(nIdDll, sText);
								
								if (!sText.IsEmpty())
								{
									::GetTextExtentPoint32W(m_hDC, sText, sText.GetLength(), &sz);
									if (MODE_FIRSTPASS == m_nNumPass) m_hline.nHorzAlign = m_defStyle.nHorzAlign; //Store a last horizontal alignment
									y = VerticalAlignText(ptOutput.y, sz.cy);
									if (MODE_DRAW == m_nNumPass)
									{
										StoreHyperlinkArea(ptOutput.x, y, ptOutput.x + sz.cx, y + sz.cy);
										::TextOutW(m_hDC, ptOutput.x, y, sText, sText.GetLength());
									} //if
									ptOutput.x += sz.cx;
								} //if
							} //if
							break;
						} //switch
					} //if
				} //if
		} //if
	} //for
	if (nBeginLineX != ptOutput.x)
	{
		m_hline.bWrappedLine = FALSE;
		Tag_NewLine(&ptOutput, 1, &szTextArea);
	}

	//ENG: Reset an additional interval for space chars
	//RUS: Сброс дополнительного интервала между словами
	::SetTextJustification(m_hDC, 0, 0);

	szTextArea.cy = ptOutput.y - lpRect->top;

	//Adds the percent's length to the line's length
	for (i = nFirstLine; i < m_nCurLine; i++)
	{
		m_hline = m_arrHtmlLine [i];
		if (0 != m_hline.nAddPercentWidth)
		{
			m_hline.nWidthLine += ::MulDiv(m_hline.nAddPercentWidth, szTextArea.cx, 100);
			szTextArea.cx = max(szTextArea.cx, m_hline.nWidthLine);
		} //if
	} //for
//
//	if (NULL != lpSize)
//	{
//		szTextArea.cx = m_szOutput.cx;
//		szTextArea.cy = m_szOutput.cy;
//	} //if
	return szTextArea;
} //End DrawHtmlString

void CPPHtmlDrawerW::StoreHyperlinkArea(int left, int top, int right, int bottom)
{
	if (m_defStyle.nTypeLink != LINK_NONE)
	{
		STRUCT_HYPERLINK link;
		link.rcArea.left = left;
		link.rcArea.top = top;
		link.rcArea.right = right;
		link.rcArea.bottom = bottom;
		link.sHyperlink = m_defStyle.sHyperlink;
		link.nTypeLink = m_defStyle.nTypeLink;
		link.nIndexLink = m_nCurIndexLink;
		m_arrLinks.push_back(link);
	} //if
} //StoreHyperlinkArea

void CPPHtmlDrawerW::SelectNewHtmlStyle(LPCWSTR lpszNameStyle, STRUCT_CHANGESTYLE & cs)
{
	//Unpack a new styles
	UnpackTextStyle(GetTextStyle(lpszNameStyle), cs);
}

BOOL CPPHtmlDrawerW::StoreRestoreStyle(BOOL bRestore)
{
	BOOL bOk = FALSE;
	if (bRestore)
	{
		//Restore styles
		if (m_arrStack.size() > 0)
		{
			STRUCT_CHANGESTYLE cs = m_arrStack.back();
			if (cs.strTag == m_defStyle.strTag)
			{
				m_defStyle = cs;
				m_arrStack.pop_back();
				bOk = TRUE;
			} //if
		} //if
		m_defStyle.strTag.Empty();
	}
	else 
	{
		m_arrStack.push_back(m_defStyle);
		bOk = TRUE;
	} //if

	return bOk;
} //End StoreRestoreStyle

void CPPHtmlDrawerW::UpdateContext()
{
	::SelectObject(m_hDC, m_hOldFont);
	::DeleteObject(m_hFont);
	m_lfDefault.lfHeight = m_defStyle.nSizeFont;
	m_lfDefault.lfWeight = m_defStyle.nWeightFont;
	m_lfDefault.lfItalic = (BYTE)m_defStyle.bItalicFont;
	m_lfDefault.lfStrikeOut = (BYTE)m_defStyle.bStrikeOutFont;
	m_lfDefault.lfUnderline = (BYTE)m_defStyle.bUnderlineFont;
	wcscpy(m_lfDefault.lfFaceName, m_defStyle.sFaceFont);
	m_hFont = ::CreateFontIndirectW(&m_lfDefault);
	m_hOldFont = (HFONT)::SelectObject(m_hDC, m_hFont);
	::GetTextMetricsW(m_hDC, &m_tm);
	
	::SetBkMode(m_hDC, m_defStyle.nBkMode);
	::SetTextColor(m_hDC, m_defStyle.crText);
	::SetBkColor(m_hDC, m_defStyle.crBkgnd);
} //End UpdateContext

int CPPHtmlDrawerW::VerticalAlignText(int y, int nHeight)
{
	//Vertical align
	if (MODE_FIRSTPASS == m_nNumPass)
	{
		//If calculate then don't output text
		m_hline.nDescentLine = max(m_hline.nDescentLine, nHeight - m_tm.tmAscent);
		m_hline.nHeightLine = max(m_hline.nHeightLine, m_tm.tmAscent);
	}
	else if (MODE_DRAW == m_nNumPass)
	{
		switch (m_defStyle.nVertAlign)
		{
		case ALIGN_VCENTER:
			y += (m_hline.nHeightLine - m_tm.tmHeight) / 2;
			break;
		case ALIGN_BASELINE:
			y += m_hline.nHeightLine - m_hline.nDescentLine - m_tm.tmAscent;
			break;
		case ALIGN_BOTTOM:
			y += m_hline.nHeightLine - m_tm.tmAscent;
			break;
		} //switch
	} //if
	return y;
} //End VerticalAlignText

int CPPHtmlDrawerW::VerticalAlignImage(int y, int nHeight)
{
	//Vertical align
	if (MODE_FIRSTPASS == m_nNumPass)
	{
		//If calculate then don't output text
		m_hline.nHeightLine = max(m_hline.nHeightLine, nHeight);
	}
	else if (MODE_DRAW == m_nNumPass)
	{
		switch (m_defStyle.nVertAlign)
		{
		case ALIGN_VCENTER:
			y += (m_hline.nHeightLine - nHeight) / 2;
			break;
		case ALIGN_BASELINE:
			y += m_hline.nHeightLine - m_hline.nDescentLine - nHeight;
			break;
		case ALIGN_BOTTOM:
			y += m_hline.nHeightLine - nHeight;
			break;
		} //switch
	} //if
	return y;
} //End VerticalAlignImage

void CPPHtmlDrawerW::Tag_NewLine(LPPOINT lpPoint, int nNum, LPSIZE lpSize)
{
	//New line
	if (nNum <= 0)
		nNum = 1;

	if (MODE_FIRSTPASS == m_nNumPass)
	{
		if (!m_hline.nHeightLine)
			m_hline.nHeightLine = m_tm.tmHeight;
		lpSize->cx = max(lpSize->cx, lpPoint->x - m_rcOutput.left);
		m_hline.nWidthLine = lpPoint->x - m_rcOutput.left; //Adds the real length of the lines
		m_hline.nHeightLine += m_hline.nDescentLine; //Adds the real height of the lines
		m_arrHtmlLine [m_nCurLine] = m_hline;
	} //if
	
	m_nCurLine ++;

	lpPoint->y += m_hline.nHeightLine * nNum;
	lpPoint->x = InitNewLine(m_rcOutput.left);	
} //End Tag_NewLine

int CPPHtmlDrawerW::InitNewLine(int x)
{
	if (MODE_FIRSTPASS == m_nNumPass)
	{
		//ENG: Creates a new line with default parameters
		//RUS: Создание новой линии с параметрами по-умолчанию
		m_hline.nAddPercentWidth = 0;
		m_hline.nDescentLine = 0;
		m_hline.nHeightLine = 0;
		m_hline.nWidthLine = 0;
		m_hline.nHorzAlign = m_defStyle.nHorzAlign;
		m_hline.nSpaceChars = 0;
		m_arrHtmlLine.push_back(m_hline);
	}
	else if (MODE_DRAW == m_nNumPass)
	{
		//ENG: Gets the data of the first line and converts the percent value to the real width
		//RUS: Получаем данные первой строки и преобразуем процентную ширину в реальную
		m_hline = m_arrHtmlLine [m_nCurLine];
		int nRealWidth = m_rcOutput.right - m_rcOutput.left;
		
		if (m_hline.nAddPercentWidth)
			m_hline.nWidthLine += ::MulDiv(nRealWidth, m_hline.nAddPercentWidth, 100);

		if ((ALIGN_JUSTIFY == m_hline.nHorzAlign) && m_hline.bWrappedLine)
			::SetTextJustification(m_hDC, nRealWidth - m_hline.nWidthLine, m_hline.nSpaceChars);
		else
			::SetTextJustification(m_hDC, 0, 0);
		
		//ENG: Horizontal coordinate of the begin output
		//RUS: Координата начала вывода с учетом выравнивания
		switch (m_hline.nHorzAlign)
		{
		case ALIGN_CENTER:
			x = m_rcOutput.left + (nRealWidth - m_hline.nWidthLine) / 2;
			break;
		case ALIGN_RIGHT:
			x = m_rcOutput.left + nRealWidth - m_hline.nWidthLine;
			break;
		} //switch
	} //if
	return x;
} //End of InitNewLine

void CPPHtmlDrawerW::Tag_Tabulation(LPPOINT lpPoint, int nNum)
{
	//Tabulation
	if (!nNum)
		nNum = 1;
	int nWidth = (lpPoint->x - m_rcOutput.left) % m_nTabSize;
	if (nWidth)
	{
		//aligns with tab
		lpPoint->x += m_nTabSize - nWidth;
		nNum --;
	} //if
	lpPoint->x += (nNum * m_nTabSize);
} //End Tag_Tabulation

/////////////////////////////////////////////////////////////////////////////////////////

void CPPHtmlDrawerW::Draw(HDC hDC, LPCWSTR lpszHtml, LPPOINT lpPoint)
{
	//ENG: Preparing an output text
	//RUS: Подготовка текста к выводу
	SIZE size;
	PrepareOutput(hDC, lpszHtml, &size);

	//ENG: If output was disabled
	//RUS: Если вывод запрещен
	if (!size.cx || !size.cy)
		return;
	
	//ENG: Calculates an output area
	//RUS: Подсчет области вывода
	RECT rect;
	rect.left = lpPoint->x;
	rect.top = lpPoint->y;
	rect.right = rect.left + size.cx;
	rect.bottom = rect.top + size.cy;
	
	//ENG: Output a prepared text
	//RUS: Вывод подготовленного текста
	DrawPreparedOutput(hDC, &rect);
} //End Draw

void CPPHtmlDrawerW::PrepareOutput(HDC hDC, LPCWSTR lpszHtml, LPSIZE lpSize)
{
	//ENG: Copy initial parameters
	//RUS: Копирование начальных параметров
	m_hDC = hDC;

	//ENG: Reset text justification
	::SetTextJustification(m_hDC, 0, 0);

	RECT rect;
	rect.left = rect.right = rect.top = rect.bottom = 0;
//	if (m_bIsTextWrapEnabled)
		rect.right = m_nMaxWidth;
	m_csHtmlText = lpszHtml;
	ReplaceSpecChars();
	lpSize->cx = lpSize->cy = 0;
	
	//ENG: If prepared text wasn't empty then return
	//RUS: Если подготовленный текст не пустой, то выход
	if (!m_csHtmlText.IsEmpty())
	{
		//ENG: Sets a prepare mode
		//RUS: Устанавливаем режим подготовки
		m_nNumPass = MODE_FIRSTPASS;

		m_arrTables.clear();

		//ENG: Prepares to real output
		//RUS: Подготовка к реальному выводу
		DrawHtml(lpSize, &rect);

		if (!lpSize->cx && !lpSize->cy)
			m_csHtmlText.Empty();

		//Cuts a tooltip if his real width more than m_nMaxWidth
		if (m_nMaxWidth/*m_bIsTextWrapEnabled*/ && (lpSize->cx > m_nMaxWidth))
			lpSize->cx = m_nMaxWidth;
		
		lpSize->cx ++;
		lpSize->cy ++;
	} //if
} //End PrepareOutput

////////////////////////////////////////////////////////////////////
// CPPHtmlDrawer::DrawPreparedOutput()
//		Draw a string prepared by PrepareOutput method.
//------------------------------------------------------------------
// Parameters:
//		hDC				- Device Context to drawing 
//		lpRect			- Pointer to RECT structure contains a bounding rectangle of
//						  drawing area.
////////////////////////////////////////////////////////////////////
void CPPHtmlDrawerW::DrawPreparedOutput(HDC hDC, LPCRECT lpRect)
{
	//ENG: If prepared text was empty then return
	//RUS: Если подготовленный текст пустой, то выход
	if (m_csHtmlText.IsEmpty())
		return;

	//ENG: Copy initial parameters
	//RUS: Копирование начальных параметров
	m_hDC = hDC;
	SIZE size = {0, 0};

	//ENG: Sets a output mode
	//RUS: Устанавливаем режим вывода
	m_nNumPass = MODE_DRAW;

	RECT rect = *lpRect;
//	if (((rect.right - rect.left) > m_nMaxWidth) && m_bIsTextWrapEnabled)
//		rect.right = rect.left + m_nMaxWidth;

	//ENG: Real output the prepared string
	//RUS: Вывод подготовленной строки
	DrawHtml(&size, &rect);
} //End of DrawPreparedOutput

// The following appeared in Paul DiLascia's Jan 1998 MSJ articles.
// It loads a "hand" cursor from the winhlp32.exe module
void CPPHtmlDrawerW::SetDefaultCursor()
{
	if (m_hLinkCursor == NULL)                // No cursor handle - load our own
    {
#ifdef IDC_HAND
		//This code was added from Zorglab's comments to hyperlink control from Chris Maunder
		m_hLinkCursor = ::LoadCursor(NULL, IDC_HAND); // Load Windows' hand cursor
		if (m_hLinkCursor != NULL)                    // if not available, load it from winhlp32.exe
			return;
#endif //IDC_HAND
		// Get the windows directory
        CPPString strWndDir;
        GetWindowsDirectoryW(strWndDir.GetBuffer(MAX_PATH), MAX_PATH);
        strWndDir.ReleaseBuffer();

        strWndDir += L"\\winhlp32.exe";
        // This retrieves cursor #106 from winhlp32.exe, which is a hand pointer
        HMODULE hModule = LoadLibraryW(strWndDir);
        if (hModule) 
		{
            HCURSOR hHandCursor = ::LoadCursorW(hModule, MAKEINTRESOURCEW(106));
            if (hHandCursor)
                m_hLinkCursor = CopyCursor(hHandCursor);
        } //if
        FreeLibrary(hModule);
    } //if
} //End SetDefaultCursor

void CPPHtmlDrawerW::SetHyperlinkCursor(HCURSOR hCursor /* = NULL */)
{
	if ((m_hLinkCursor == hCursor) && (NULL != m_hLinkCursor))
		return;

	if (NULL != m_hLinkCursor)
	{
		::DestroyCursor(m_hLinkCursor);
		m_hLinkCursor = NULL;
	} //if
	

    if (NULL == hCursor)
		SetDefaultCursor();
	else
		m_hLinkCursor = hCursor;
} //End SetHyperlinkCursor

HCURSOR CPPHtmlDrawerW::GetHyperlinkCursor() const
{
    return m_hLinkCursor;
} //End GetHyperlinkCursor

/////////////////////////////////////////////////////////////////////
// CPPHtmlDrawer::SetCallbackHyperlink
// This function sets or removes the notification messages from the control before display.
//
// Parameters:
//	hWnd [in] -    If non-NULL the control will be send the notification 
//				   to specified window
//				   Else the notification will not send
///////////////////////////////////////////////////////////////////////
void CPPHtmlDrawerW::SetCallbackHyperlink(HWND hWnd, UINT nMessage, LPARAM lParam /* = 0 */)
{
//	TRACE(_T("CPPHtmlDrawerW::SetCallbackHyperlink()\n"));

	m_csCallbackLink.hWnd = hWnd;
	if (NULL == hWnd)
	{
		m_csCallbackLink.nMessage = 0;
		m_csCallbackLink.lParam = 0;
	}
	else
	{
		m_csCallbackLink.nMessage = nMessage;
		m_csCallbackLink.lParam = lParam;
	} //if
} //End SetCallbackHyperlink

void CPPHtmlDrawerW::SetCallbackRepaint(HWND hWnd, UINT nMessage, LPARAM lParam /* = 0 */)
{
//	TRACE(_T("CPPHtmlDrawerW::SetCallbackRepaint()\n"));

	m_csCallbackRepaint.hWnd = hWnd;
	if (NULL == hWnd)
	{
		m_csCallbackRepaint.nMessage = 0;
		m_csCallbackRepaint.lParam = 0;
	}
	else
	{
		m_csCallbackRepaint.nMessage = nMessage;
		m_csCallbackRepaint.lParam = lParam;
	} //if
} //End SetCallbackRepaint

/////////////////////////////////////////////////////////////////////////////
//  CPPToolTip::SetImageList (public member function)
//    sets the image list to tooltip
//
//  Parameters :
//		nIdBitmap	[in] - Resource IDs of the bitmap to be associated with the image list
//		cx			[in] - Dimensions of each image, in pixels.
//		cy			[in] - Dimensions of each image, in pixels.
//		nCount		[in] - Number of images that the image list initially contains.
//		crMask		[in] - Color used to generate a mask. Each pixel of this color in the 
//						   specified bitmap is changed to black, and the corresponding 
//						   bit in the mask is set to one.
//  Returns :
//		None
//
/////////////////////////////////////////////////////////////////////////////
void CPPHtmlDrawerW::SetImageList(UINT nIdBitmap, int cx, int cy, int nCount, COLORREF crMask /* = RGB(255, 0, 255) */)
{
	// Load bitmap
	HBITMAP hBitmap = GetBitmapFromResources(nIdBitmap);
	SetImageList(hBitmap, cx, cy, nCount, crMask);
} //End SetImageList

/////////////////////////////////////////////////////////////////////////////
//  CPPToolTip::SetImageList (public member function)
//    sets the image list to tooltip
//
//  Parameters :
//		hBitmap		[in] - Handle of the bitmap to be associated with the image list
//		cx			[in] - Dimensions of each image, in pixels.
//		cy			[in] - Dimensions of each image, in pixels.
//		nCount		[in] - Number of images that the image list initially contains.
//		crMask		[in] - Color used to generate a mask. Each pixel of this color in the 
//						   specified bitmap is changed to black, and the corresponding 
//						   bit in the mask is set to one.
//  Returns :
//		None
//
/////////////////////////////////////////////////////////////////////////////
void CPPHtmlDrawerW::SetImageList(HBITMAP hBitmap, int cx, int cy, int nCount, COLORREF crMask /* = RGB(255, 0, 255) */)
{
	//ENG: Removes previously image list
	//RUS: Удаляем предыдущий список изображений
	if (NULL != m_hImageList)
		::DeleteObject(m_hImageList);

	//ENG: If don't need to create a new image list
	//RUS: Если не нужно создавать новый список изображений
	if (NULL == hBitmap)
		return;

	// Ensure that the common control DLL is loaded. 
	InitCommonControls(); 
	
	m_hImageList = ImageList_Create(cx, cy, ILC_COLOR32 | ILC_MASK, nCount, 1);
	ImageList_AddMasked(m_hImageList, hBitmap, crMask);
	m_szImageList.cx = cx;
	m_szImageList.cy = cy;
} //End SetImageList

/////////////////////////////////////////////////////////////////////////////
//  CPPToolTip::GetImageList (public member function)
//    gets the image list from tooltip
//
//  Parameters :
//		sz		   [out] - Dimensions of each image, in pixels.
//  Returns :
//		A pointer to a CImageList object
//
/////////////////////////////////////////////////////////////////////////////
//CImageList * CPPHtmlDrawer::GetImageList(CSize & sz)
//{
//	sz = m_szImageList;
//	return &m_ImageList;
//} //End GetImageList

void CPPHtmlDrawerW::EnableEscapeSequences(BOOL bEnable /* = TRUE */)
{
	m_bEnableEscapeSequences = bEnable;
}

void CPPHtmlDrawerW::LoadResourceDll(LPCWSTR lpszPathDll, DWORD dwFlags /* = 0 */)
{
	HINSTANCE hInst = NULL;
	if (NULL != lpszPathDll)
		hInst = ::LoadLibraryExW(lpszPathDll, NULL, dwFlags);
	
	SetResourceDll(hInst);

	if (NULL != hInst)
		m_bFreeInstDll = TRUE;
} //End LoadResourceDll

void CPPHtmlDrawerW::SetResourceDll(HINSTANCE hInstDll /* = NULL */)
{
	if (NULL != m_hInstDll)
	{
		if (!m_bFreeInstDll)
			return;
		::FreeLibrary(m_hInstDll);
		m_hInstDll = NULL;
	} //if

	m_bFreeInstDll = FALSE;

	if (NULL != hInstDll)
		m_hInstDll = hInstDll;
} //End SetResourceDll


CPPDrawManagerW * CPPHtmlDrawerW::GetDrawManager()
{
	return &m_drawmanager;
} //End GetDrawManager

BOOL CPPHtmlDrawerW::IsImageWithShadow(_STRUCT_IMAGE & si)
{
	DWORD dwStyles = si.nStyles | si.nHotStyles;
	if ((dwStyles & IMAGE_EFFECT_MONO_SHADOW) || 
		(dwStyles & IMAGE_EFFECT_GRADIENT_SHADOW))
		return TRUE;
	
	return FALSE;
}

///////////////////////////////////////////////////////////////////////////
// Map of the styles
void CPPHtmlDrawerW::SetDefaultCssStyles()
{
	CPPString str = L"";
	str += L"body {font-size: 10pt; color:black; font-family:Verdana}\r\n";
	str += L"p {font-size: 10pt; color:black; font-family:Verdana; font-weight:bold}\r\n";
	str += L"h1 {font-size: 14pt; color:black; font-family:Verdana; font-weight:bold}\r\n";
	str += L"h2 {font-size: 13pt; color:#ff9900; font-family:Verdana; font-weight:bold}\r\n";
	str += L"h3 {font-size: 12pt; color:#ff9900; font-family:Arial; font-weight:bold}\r\n";
	str += L"h4 {font-size: 10pt; color:black; font-family:Verdana; font-weight:bold}\r\n";
	str += L"h5 {font-size: 9pt; color:#ff9900; font-family:Verdana; font-weight:bold}\r\n";
	str += L"h6 {font-size: 65%; color:#626262; font-family:Verdana; font-weight:normal}\r\n";
	str += L"pre {font-size: 9pt; font-family:\"Courier\"; background-color:#fbedbb}\r\n";
	str += L"code {color:#990000; font-family:Arial}\r\n";
	str += L"a:link {text-decoration:none; color:blue}\r\n";
	str += L"a:hover {text-decoration:underline; color:blue}\r\n";
	str += L"sub {font-size:65%; vertical-align:bottom}\r\n";
	str += L"sup {font-size:65%; vertical-align:top}\r\n";
	str += L"big {font-size:125%}\r\n";
	str += L"small {font-size:75%}\r\n";
	str += L".cpp-comment {color:green; font-style:italic}\r\n";
//	str += _T("td {text-align:center; color:#ff0000; vertical-align:middle}\r\n");
//	str += _T("table {padding:2; border-width:1; color:red}\r\n");

	SetCssStyles(str);
} //End SetDefaultCssStyle

void CPPHtmlDrawerW::SetCssStyles(DWORD dwIdCssString, LPCWSTR lpszPathDll /* = NULL */)
{
	CPPString str;
	if (NULL == lpszPathDll)
		str = GetStringFromResource(dwIdCssString);
	else
		str = GetStringFromDll(dwIdCssString, lpszPathDll);
	SetCssStyles(str);
} //End SetCssStyles

void CPPHtmlDrawerW::SetCssStyles(LPCWSTR lpszCssString /* = NULL */)
{
	m_mapStyles.clear(); //removes previously styles

	if (NULL == lpszCssString)
	{
		SetDefaultCssStyles();
	}
	else
	{
		CPPString str = (CPPString)lpszCssString;
		m_strCssStyles = str;
		
		CPPString strName;
		CPPString strProperty;
		
		int nBegin;
		wchar_t chSymbol;
		int nIndex = 0;
		
		while (nIndex < str.GetLength())
		{
			//Passes a space in begin string
			if (GetIndexNextAlphaNum(str, nIndex))
			{
				nBegin = nIndex;
				//Searching end of the style name
				chSymbol = GetIndexNextChars(str, nIndex, L" {");
				if ((nIndex > nBegin) && (0 != chSymbol))
				{
					strName = str.Mid(nBegin, nIndex - nBegin);
					if (!strName.IsEmpty())
					{
						if (chSymbol != L' ')
							nIndex --;
						chSymbol = GetIndexNextChars(str, nIndex, L"{");
						if (0 != chSymbol)
						{
							nBegin = nIndex + 1;
							chSymbol = GetIndexNextChars(str, nIndex, L"}");
							if ((nIndex > nBegin) && (0 != chSymbol))
							{
								strProperty = str.Mid(nBegin, nIndex - nBegin);
								SetTextStyle(strName, strProperty);
							} //if
						} //if
					} //if
				} //if
			} //if
		} //while
	} //if
} //End SetCssStyles

LPCWSTR CPPHtmlDrawerW::GetCssStyles()
{
	return (LPCWSTR)m_strCssStyles;
} //End GetCssStyles

LPCWSTR CPPHtmlDrawerW::GetTextStyle(LPCWSTR lpszStyleName)
{
	CPPString name = (CPPString)lpszStyleName;
	name.MakeLower();
	iter_mapStyles iterMap = m_mapStyles.find(name);
	
	if (iterMap != m_mapStyles.end())
		return (LPCWSTR)iterMap->second;

	//Not found
	return NULL;
} //End GetTextStyle

void CPPHtmlDrawerW::SetTextStyle(LPCWSTR lpszStyleName, LPCWSTR lpszStyleValue)
{
	CPPString name = (CPPString)lpszStyleName;
	name.MakeLower();
	iter_mapStyles iterMap = m_mapStyles.find(name);
	
	if (iterMap != m_mapStyles.end())
	{
		//Modifies 
		iterMap->second = (CPPString)lpszStyleValue;
	}
	else
	{
		//Add new
		m_mapStyles.insert(std::make_pair(name, (CPPString)lpszStyleValue));
	} //if
} //End SetTextStyle

void CPPHtmlDrawerW::RemoveTextStyle(LPCWSTR lpszStyleName)
{
	CPPString name = (CPPString)lpszStyleName;
	name.MakeLower();
	iter_mapStyles iterMap = m_mapStyles.find(name);
	
	if (iterMap == m_mapStyles.end())
		return; //item was not found
	
	m_mapStyles.erase(iterMap);
} //End RemoveTextStyle

void CPPHtmlDrawerW::AddToTextStyle(LPCWSTR /*lpszStyleName*/, LPCWSTR /*lpszAddStyle*/)
{
} //End AddToTextStyle

void CPPHtmlDrawerW::UnpackTextStyle(CPPString strStyle, _STRUCT_CHANGESTYLE & cs)
{
	//Gets a string
	strStyle.MakeLower();
	if (strStyle.IsEmpty())
		return;

	CPPString strName;
	CPPString strParameter;

	int nBegin;
	wchar_t chSymbol;
	int nIndex = 0;
	CPPString str;

	while (nIndex < strStyle.GetLength())
	{
		//Passes a space in begin string
		if (GetIndexNextAlphaNum(strStyle, nIndex))
		{
			nBegin = nIndex;
			//Searching end of the style name
			chSymbol = GetIndexNextChars(strStyle, nIndex, L" :");
			if (0 != chSymbol)
			{
				//Gets a property's name
				strName = strStyle.Mid(nBegin, nIndex - nBegin);

				//Gets a property's value
				strParameter = GetParameterString(strStyle, nIndex, L':');

				//Analyzing name
				if (strName == L"font-size")
				{
					cs.nSizeFont = GetLengthUnit(strParameter, cs.nSizeFont, TRUE);
				}
				else if (strName == L"font-family")
				{
					if (!strParameter.IsEmpty())
						cs.sFaceFont = strParameter;
				}
				else if (strName == L"font-style")
				{
					cs.bItalicFont = GetStyleFontStyle(strParameter, cs.bItalicFont);
				}
				else if (strName == L"font-weight")
				{
					cs.nWeightFont = GetStyleFontWeight(strParameter, cs.nWeightFont);
				}
				else if (strName == L"text-align")
				{
					cs.nHorzAlign = GetStyleHorzAlign(strParameter, cs.nHorzAlign);
				}
				else if (strName == L"text-transform")
				{
					cs.nTextTransform = GetStyleTextTransform(strParameter, cs.nTextTransform);
				}
				else if (strName == L"color")
				{
					if (m_bIsEnable)
						cs.crText = GetStyleColor(strParameter, cs.crText);
					else
						cs.crText = GetColorByName(L"");
				}
				else if (strName == L"background-color")
				{
					if (((strParameter == L"transparent") && strParameter.IsEmpty()) || !m_bIsEnable)
					{
						cs.nBkMode = TRANSPARENT;
					}
					else
					{
						cs.nBkMode = OPAQUE;
						cs.crBkgnd = GetStyleColor(strParameter, cs.crBkgnd);
					} //if
				}
				else if (strName == L"text-decoration")
				{
					StyleTextDecoration(strParameter, cs);
				}
				else if (strName == L"vertical-align")
				{
					cs.nVertAlign = GetStyleVertAlign(strParameter, cs.nVertAlign);
				}
				else if (strName == L"border-color")
				{
					if (m_bIsEnable)
						cs.crBorderLight = GetStyleColor(strParameter, cs.crBorderLight);
					else
						cs.crBorderLight = GetColorByName(L"");
					cs.crBorderDark = cs.crBorderLight;
				}
				else if ((strName == L"border-width") || (strName == L"size"))
				{
					cs.nBorderWidth = StyleBorderWidth(strParameter, cs.nBorderWidth);
					if (!cs.nBorderWidth)
						cs.nBorderStyle = CPPDrawManagerW::PEN_NULL;
					else if (CPPDrawManagerW::PEN_NULL == cs.nBorderStyle)
						cs.nBorderStyle = CPPDrawManagerW::PEN_SOLID;
				}
				else if (strName == L"border-style")
				{
					cs.nBorderStyle = StyleBorder(strParameter, cs.nBorderStyle);
					if ((CPPDrawManagerW::PEN_NULL != cs.nBorderStyle) && !cs.nBorderWidth)
						cs.nBorderWidth = 1;
				}
				else if (strName == L"margin")
				{
					cs.nMargin = GetLengthUnit(strParameter, cs.nMargin);
				}
				else if (strName == L"padding")
				{
					cs.nPadding = GetLengthUnit(strParameter, cs.nPadding);
				} //if
			} //if
		} //if
	} //while
} //End UnpackTextStyle

BOOL CPPHtmlDrawerW::GetStyleFontStyle(CPPString & str, BOOL bDefault)
{
	if ((str == L"normal") || str.IsEmpty())
	{
		bDefault = FALSE;
	}
	else if ((str == L"italic") || (str == L"oblique")) 
	{
		bDefault = TRUE;
	} //if

	return bDefault;
} //End GetStyleFontStyle

int CPPHtmlDrawerW::GetStyleFontWeight(CPPString & str, int nDefault)
{
	if ((str == L"normal") || str.IsEmpty())
	{
		nDefault = FW_NORMAL;
	}
	else if (str == L"bold")
	{
		nDefault = FW_BOLD;
	}
	else if (str == L"bolder")
	{
		nDefault = 900;
	}
	else if (str == L"lighter")
	{
		nDefault = 100;
	}
	else
	{
		nDefault = _wtoi(str);
	} //if

	return nDefault;
} //End GetStyleFontWeight

int CPPHtmlDrawerW::GetStyleHorzAlign(CPPString & str, int nDefault)
{
	if ((str == L"left") || str.IsEmpty())
	{
		nDefault = ALIGN_LEFT;
	}
	else if (str == L"center")
	{
		nDefault = ALIGN_CENTER;
	}
	else if (str == L"right")
	{
		nDefault = ALIGN_RIGHT;
	}

	return nDefault;
} //End GetStyleHorzAlign

int CPPHtmlDrawerW::GetStyleVertAlign(CPPString & str, int nDefault)
{
	if ((str == L"baseline") || str.IsEmpty())
	{
		nDefault = ALIGN_BASELINE;
	}
	else if ((str == L"middle") || (str == L"vcenter"))
	{
		nDefault = ALIGN_VCENTER;
	}
	else if (str == L"top")
	{
		nDefault = ALIGN_TOP;
	}
	else if (str == L"bottom")
	{
		nDefault = ALIGN_BOTTOM;
	}
	
	return nDefault;
} //End GetStyleVertAlign

int CPPHtmlDrawerW::GetStyleTextTransform(CPPString & str, int nDefault)
{
	if ((str == L"none") || str.IsEmpty())
	{
		nDefault = TEXT_TRANSFORM_NONE;
	}
	else if (str == L"uppercase")
	{
		nDefault = TEXT_TRANSFORM_UPPERCASE;
	}
	else if (str == L"lowercase")
	{
		nDefault = TEXT_TRANSFORM_LOWERCASE;
	}
	else if (str == L"capitalize")
	{
		nDefault = TEXT_TRANSFORM_CAPITALIZE;
	}
	
	return nDefault;
}

COLORREF CPPHtmlDrawerW::GetStyleColor(CPPString & str, COLORREF crDefault)
{
//	if (!m_bIsEnable)
//		return GetColorByName("");
	
	if (!str.IsEmpty())
	{
		if (str.GetAt(0) == L'#')
		{
			if (str.GetLength() == 7)
			{
				CPPString strHex = L"0x";
				strHex += str.Mid(5, 2);
				strHex += str.Mid(3, 2);
				strHex += str.Mid(1, 2);
				crDefault = (COLORREF)wcstoul(strHex, 0, 0);
			} //if
		}
		else if ((str.GetAt(0) >= '0') && (str.GetAt(0) <= '9'))
			crDefault = (COLORREF)wcstoul(str, 0, 0);
		else
			crDefault = GetColorByName(str, crDefault);
	} //if 

	return crDefault;
} //End GetStyleColor

int CPPHtmlDrawerW::GetLengthUnit(CPPString & str, int nDefault, BOOL bFont /* = FALSE */)
{
	if (str.IsEmpty())
		return nDefault;
	
	if (IsPercentableValue(str))
	{
		//Percent value
		int percent = _wtoi(str.Left(str.GetLength() - 1));
		return ::MulDiv(nDefault, percent, 100);
	} //if

	int nSign = 0;
	if (str.GetAt(0) == L'+') nSign = 1;
	else if (str.GetAt(0) == L'-') nSign = -1;
	
	if (0 != nSign) str = str.Right(str.GetLength() - 1);
	
	//ENG: This code fragment fixed by Reinhard Steiner(2004/10/20).
	int nValue = _wtoi(str);
	CPPString strUnit;
	if(str.GetLength() >= 2)
		strUnit = str.Right(2);

	if (strUnit == L"px")		nDefault = nValue;
	else if (strUnit == L"ex")
	{
		SIZE szText;
		CPPString strText = L"x";
		::GetTextExtentPoint32W(m_hDC, strText, strText.GetLength(), &szText);
		nDefault = nValue * szText.cy;
	}
	else if (strUnit == L"em")	nDefault = nValue * m_tm.tmHeight;
	else
	{
		//Gets pixel in inch
		nValue *= ::GetDeviceCaps(m_hDC, LOGPIXELSY);
		if (strUnit == L"in")		nDefault = nValue;
		else if (strUnit == L"cm")	nDefault = (int)((double)nValue / 2.54);
		else if (strUnit == L"mm")	nDefault = (int)((double)nValue / 25.4);
		else if (strUnit == L"pt")	nDefault = nValue / 72;
		else if (strUnit == L"pc")	nDefault = nValue / 6;
		else
		{
			nValue = wcstoul(str, 0, 0);//_ttoi(str);
			if ((nValue > 0) && (nValue < 8) && bFont)
			{
				int nSize [] = {8, 10, 12, 14, 18, 24, 36};
				nDefault = nSize [nValue - 1];
			}
			else
			{
				nDefault = nValue;
			} //if
		} //if
	} //if
	
	return nDefault;
} //End GetLengthUnit

void CPPHtmlDrawerW::StyleTextDecoration(CPPString & str, _STRUCT_CHANGESTYLE & cs)
{
	if (str.IsEmpty())
		str = L"none";
	
	int nBegin = 0;
	int nEnd = 0;
	CPPString strTemp;
	while (nBegin < str.GetLength())
	{
		if (GetIndexNextAlphaNum(str, nBegin))
		{
			nEnd = nBegin;
			GetIndexNextChars(str, nEnd, L" ,");
			strTemp = str.Mid(nBegin, nEnd - nBegin);
			nBegin = nEnd;
			if (strTemp == L"none")
			{
				cs.bUnderlineFont = FALSE;
				cs.bStrikeOutFont = FALSE;
				cs.bOverlineFont = FALSE;
			}
			else if (strTemp == L"underline")
			{
				cs.bUnderlineFont = TRUE;
			}
			else if (strTemp == L"line-through")
			{
				cs.bStrikeOutFont = TRUE;
			}
			else if (strTemp == L"overline")
			{
				cs.bOverlineFont = TRUE;
			}  //if
		} //if
	} //while
} //End StyleTextDecoration

int CPPHtmlDrawerW::StyleBorderWidth(CPPString & str, int nDefault)
{
	if (str ==L"thin")		nDefault = ::MulDiv(75, nDefault, 100);
	else if (str ==L"thick")	nDefault = ::MulDiv(125, nDefault, 100);
	else if (str !=L"medium")nDefault = GetLengthUnit(str, nDefault);

	return nDefault;
} //End StyleBorderWidth

int CPPHtmlDrawerW::StyleBorder(CPPString & str, int nDefault)
{
	if ((str == L"none") || str.IsEmpty()) nDefault = CPPDrawManagerW::PEN_NULL;
	else if (str == L"solid") nDefault = CPPDrawManagerW::PEN_SOLID;
	else if (str == L"dotted") nDefault = CPPDrawManagerW::PEN_DOT;
	else if (str == L"dashed") nDefault = CPPDrawManagerW::PEN_DASH;
	else if (str == L"double") nDefault = CPPDrawManagerW::PEN_DOUBLE;

	return nDefault;
} //End StyleBorder

void CPPHtmlDrawerW::SetDefaultStyles(_STRUCT_CHANGESTYLE & /*cs*/)
{
	m_defStyle.strTag.Empty();		//The name of the last opened tag
	
	//Font
	m_defStyle.nSizeFont = 16;		//The height of the logic font
	m_defStyle.nWeightFont = FW_NORMAL;	//The weight of the logic font
	m_defStyle.bItalicFont = FALSE;	//Is italic logic font?
	m_defStyle.bUnderlineFont = FALSE;//Is underline logic font?
	m_defStyle.bStrikeOutFont = FALSE;//Is strikeout logic font?
	m_defStyle.bOverlineFont = FALSE; //Is overline logic font?
	m_defStyle.sFaceFont = L"Verdana";  //The face name of the logic font
	
	//Color		
	m_defStyle.crText = RGB (0, 0, 0);	//The foreground color 
	m_defStyle.crBkgnd = RGB (255, 255, 255);	//The background color (also begin for the gradient)
	m_defStyle.crBorderLight = RGB (0, 0, 0);	//The border color
	m_defStyle.crBorderDark = RGB (0, 0, 0);	//The border color
	m_defStyle.crMidBkgnd = RGB (255, 255, 255);//The middle background color
	m_defStyle.crEndBkgnd = RGB (255, 255, 255);//The end background color
	
	//Fill
	m_defStyle.nBkMode = TRANSPARENT;		//The background mode for the text (TRANSPARENT, OPAQUE)
	m_defStyle.nFillBkgnd = -1;	//The fill effect of the background
	m_defStyle.strNameResBk.Empty();
	
	//Align
	m_defStyle.nHorzAlign = ALIGN_LEFT;	//The horizontal align
	m_defStyle.nVertAlign = ALIGN_BASELINE;	//The vertical align
	
	//Border
	m_defStyle.nBorderStyle = CPPDrawManagerW::PEN_NULL;	//The border style
	m_defStyle.nBorderWidth = 0;	//The width of the border
	
	//Text
	m_defStyle.nTextTransform = TEXT_TRANSFORM_NONE;//Transformation of the text (NONE, UPPERCASE, LOWERCASE, CAPITALIZE)
	
	//Margins
	m_defStyle.nMargin = 2;
	
	//Padding
	m_defStyle.nPadding = 0;
	
	//Hyperlink
	m_defStyle.nTypeLink = LINK_NONE;		//The type of the link (NONE, HREF, MESSAGE)
	m_defStyle.sHyperlink.Empty(); //The additional parameter for the link
} //SetDefaultStyles

/////////////////////////////////////////////////////////////////
// Search body of the next tag
//---------------------------------------------------------------
// Parameters:
//     In: str    - a string with html text
//         nIndex - an index of the first char to the searching in the string
//    Out: nIndex - an index of the char in the string after found tag's text
//         strTag - a string contained the tag's text if was found
// Return: A string before found tag's text 
/////////////////////////////////////////////////////////////////
CPPString CPPHtmlDrawerW::SearchNextTag(CPPString & str, CPPString & strTag, int & nIndex)
{
	int nBegin;
	CPPString sText = L"";
	strTag.Empty();

	while (nIndex < str.GetLength())
	{
		nBegin = nIndex;
		//Searching a chars of the begin tag
		nIndex = str.Find(L"<", nIndex);
		if (nIndex < 0)
			nIndex = str.GetLength(); //A tag wasn't found
		sText += str.Mid(nBegin, nIndex - nBegin);
		if (nIndex < str.GetLength())
		{
			//May be it is a begin of the tag?
			if ((nIndex < (str.GetLength() - 1)) && (L'<' != str.GetAt(nIndex + 1)))
			{
				//Yes of cause!!!
				strTag = GetTagBody(str, nIndex);
				return sText;
			}
			//No, it is a char '<'
			sText += L"<";
			nIndex += 2;
			break;
		} //if
	} //while
	return sText;
} //End SearchNextTag

/////////////////////////////////////////////////////////////////
// CPPHtmlDrawer::GetTagBody
//	Gets a name of tag with a parameters
//---------------------------------------------------------------
// Parameters:
//	[in]
//		str		-	a string with html text
//		nIndex	-   an index of the begin of the tag. 
//	[out]
//		nIndex  -	an index of char after the tag
//---------------------------------------------------------------
// Return values:
//	A tag's name .
/////////////////////////////////////////////////////////////////
CPPString CPPHtmlDrawerW::GetTagBody(CPPString & str, int & nIndex)
{
	CPPString sTagName = L"";
	//ENG: Search the tag's end 
	//RUS: Ищем окончание тэга
	int nEndOfTag = str.Find(L'>', nIndex);
	//ENG: The tag's end was found. Passes a tag's begin char ('<')
	//RUS: Конец тэга найденю Пропускаем символ начала тэга
	nIndex++;
	if (nEndOfTag > nIndex)
	{
		//ENG: Gets a full body of tag
		//RUS: Получаем полную строку тэга
		sTagName = str.Mid(nIndex, nEndOfTag - nIndex);
		//ENG: Jump to next char after the tag
		//RUS: Перемещаемся на следующий за тэгом символ
		nIndex = nEndOfTag + 1;
	} //if
	return sTagName;
} //End of GetTagBody

/////////////////////////////////////////////////////////////////
// Split a tag to his name and properties
//---------------------------------------------------------------
// Parameters:
//     In: sTag    - a string with tag's text
//    Out: sTag	   - a tag's name
// Return: A property's string 
/////////////////////////////////////////////////////////////////
CPPString CPPHtmlDrawerW::SplitTag(CPPString & sTag)
{
	CPPString sParam(L"");
	int nIndex = 0;
	wchar_t tch = GetIndexNextChars(sTag, nIndex, L" =");
	if (tch != L'\0')
	{
		//ENG: The separator was found. Splits a tag's body to his name and his parameteres 
		//RUS: Разделитель найден. Разделяем тело тэга на имя и параметры
		sParam = sTag.Mid(nIndex);
		sTag = sTag.Left(nIndex);
		sParam.TrimLeft(L' ');
	} //if
	return sParam;
} //End of SplitTag

CPPString CPPHtmlDrawerW::GetNextProperty(CPPString & str, int & nIndex, CPPString & sProp)
{
	CPPString sValue(L"");
	sProp.Empty();
	
	//Passes the spaces before a property
	if (GetIndexNextAlphaNum(str, nIndex))
	{
		//The begin of the property was found
		int nBegin = nIndex;
		//Searching end of the property
		GetIndexNextChars(str, nIndex, L" =");
		//Gets a property's string
		sProp = str.Mid(nBegin, nIndex - nBegin);
		wchar_t chFound = GetIndexNextNoChars(str, nIndex, L" ");
		if (L'=' == chFound)
		{
			chFound = GetIndexNextNoChars(str, nIndex, L" =");
			if ((L'\'' == chFound) || (L'\"' == chFound))
			{
				nIndex++;
			}
			else
			{
				chFound = L' ';
			} //if
			sValue += chFound;
			nBegin = nIndex;
			GetIndexNextChars(str, nIndex, sValue);
			sValue = str.Mid(nBegin, nIndex - nBegin);
			nIndex ++;
		} //if
	} //if
	return sValue;
} //End of GetNextProperty

/////////////////////////////////////////////////////////////////
// Searching the next property of the tag
//---------------------------------------------------------------
// Parameters:
//     In: str    - a string with html text
//         nIndex - an index of the first char to the searching in the string
//    Out: nIndex - an index of the char in the string after found tag's text
// Return: A property's string 
/////////////////////////////////////////////////////////////////
CPPString CPPHtmlDrawerW::SearchPropertyOfTag(CPPString & str, int & nIndex)
{
	CPPString sText = L"";
	
	//Passes the spaces before a property
	if (GetIndexNextAlphaNum(str, nIndex))
	{
		//The begin of the property was found
		int nBegin = nIndex;
		//Searching end of the property

//LPI		TCHAR chFound = GetIndexNextChars(str, nIndex, _T(" ="));
			GetIndexNextChars(str, nIndex, L" =");
			
		//Gets a property's string
		sText = str.Mid(nBegin, nIndex - nBegin);
	} //if
	return sText;
} //End SearchPropertyOfTag

/////////////////////////////////////////////////////////////////
// Search a tag
//---------------------------------------------------------------
// Parameters:
//     In: str    - a string with html text
//         nIndex - an index of the first char to the searching in the string
//    Out: nIndex - an index of the first char of the tag
// Return: TRUE if specified tag was found 
//---------------------------------------------------------------
// Example: (strTag = "table") or (strTag = "/table")
/////////////////////////////////////////////////////////////////
BOOL CPPHtmlDrawerW::SearchTag(CPPString & str, int & nIndex, CPPString strTag)
{
	strTag = L"<" + strTag;
	while (nIndex < str.GetLength())
	{
		nIndex = str.Find(strTag, nIndex);
		if (nIndex < 0)
			nIndex = str.GetLength();
		else
		{
			if (nIndex > 0)
			{
				if (str.GetAt(nIndex - 1) != L'<')
					return TRUE;
				nIndex += 2;
			}
			else return TRUE;
		} //if
	}
	return FALSE;
} //End SearchTag

/////////////////////////////////////////////////////////////////
// Search a first alpha_num chars or first arithmetic char
//---------------------------------------------------------------
// Parameters:
//     In: str    - a string with html text
//         nIndex - an index of the first char to the searching in the string
//    Out: nIndex - an index of the first found char
// Return: TRUE if specified char was found 
/////////////////////////////////////////////////////////////////
BOOL CPPHtmlDrawerW::GetIndexNextAlphaNum(CPPString & str, int & nIndex, BOOL bArithmetic /* = FALSE */)
{
	wchar_t ch;
	for (; nIndex < str.GetLength(); nIndex++)
	{
		ch = str.GetAt(nIndex);
		if ((ch >= L'0') && (ch <= L'9'))
			return TRUE;
		if ((ch >= L'A') && (ch <= L'Z'))
			return TRUE;
		if ((ch >= L'a') && (ch <= L'z'))
			return TRUE;
		if (ch == L'.')
			return TRUE;
		if (bArithmetic)
		{
			if ((L'+' == ch) || (L'-' == ch) || 
				(L'*' == ch) || (L'/' == ch))
				return TRUE;
		} //if
	} //for
	return FALSE;
} //End GetIndexNextAlphaNum

/////////////////////////////////////////////////////////////////
// Search a first char of the chars set
//---------------------------------------------------------------
// Parameters:
//     In: str      - a string with html text
//         nIndex   - an index of the first char to the searching in the string
//		   strChars - the set of the chars
//    Out: nIndex   - an index of the first found char
// Return: A found char or zero if chars was not found  
/////////////////////////////////////////////////////////////////
wchar_t CPPHtmlDrawerW::GetIndexNextChars(CPPString & str, int & nIndex, CPPString strChars)
{
	int i;
	for (; nIndex < str.GetLength(); nIndex++)
	{
		for (i = 0; i < strChars.GetLength(); i++)
		{
			if (str.GetAt(nIndex) == strChars.GetAt(i))
				return str.GetAt(nIndex);
		} //for
	} //for
	return 0;
} //End GetIndexNextChars

/////////////////////////////////////////////////////////////////
// Search a first char isn't specified in chars set
//---------------------------------------------------------------
// Parameters:
//     In: str      - a string with html text
//         nIndex   - an index of the first char to the searching in the string
//		   strChars - the set of the chars
//    Out: nIndex   - an index of the first char isn't from chars set
// Return: A found char or zero if all chars was specified in the chars set  
/////////////////////////////////////////////////////////////////
wchar_t CPPHtmlDrawerW::GetIndexNextNoChars(CPPString & str, int & nIndex, CPPString strChars)
{
	int i;
	BOOL bFound;
	for (; nIndex < str.GetLength(); nIndex++)
	{
		bFound = FALSE;
		for (i = 0; (i < strChars.GetLength()) && !bFound; i++)
		{
			if (str.GetAt(nIndex) == strChars.GetAt(i))
				bFound = TRUE;
		} //for
		if (!bFound)
			return str.GetAt(nIndex);
	} //for
	return 0;
} //End GetIndexNextNoChars

/////////////////////////////////////////////////////////////////
// Is exist a property's parameter?
//---------------------------------------------------------------
// Parameters:
//     In: str         - a string with html text
//         nIndex      - an index of the first char to the searching in the string
//		   chSeparator - the char is a begin of the parameter
//    Out: nIndex   - an index of the begin parameter (if it exist) or the begin of the next property
// Return: TRUE if parameter was found  
/////////////////////////////////////////////////////////////////
BOOL CPPHtmlDrawerW::GetBeginParameter(CPPString & str, int & nIndex, wchar_t chSeparator /* = _T(':') */)
{
	wchar_t ch;
	for (; nIndex < str.GetLength(); nIndex++) 
	{
		//Gets a current char
		ch = str.GetAt(nIndex);
		if (L' ' != ch)
		{
			//if it is not space char
			if (chSeparator == ch)
			{
				//if begin of the property's parameter was found
				nIndex ++; //jump to the next char after a begin parameter
				return TRUE;
			}
			else
			{
				return FALSE;
			}//if
		} //if
	} //for
	return FALSE;
} //End GetBeginParameter

/////////////////////////////////////////////////////////////////
// Gets a parameter for the currrent property
//---------------------------------------------------------------
// Parameters:
//     In: str         - a string with html text
//         nIndex      - an index of the first char to the searching in the string
//		   chSeparator - the char is a begin of the parameter
//    Out: nIndex   - an index of the first char after the parameter
// Return: String of the property's parameter (empty if it is not exist)  
/////////////////////////////////////////////////////////////////
CPPString CPPHtmlDrawerW::GetParameterString(CPPString & str, int & nIndex, wchar_t chBeginParam /* = _T(':') */, CPPString strSeparators /* = _T(";") */)
{
	if (GetBeginParameter(str, nIndex, chBeginParam))
	{
		//Parameter for the current property was found
		wchar_t ch = GetIndexNextNoChars(str, nIndex, strSeparators + L" ");
		if (0 != ch)
		{
			int nBegin = nIndex;
			if (L'"' == str.GetAt(nIndex))
			{
				nIndex++;
				wchar_t ch = GetIndexNextChars(str, nIndex, L"\"");
				if (L'"' == ch)
				{
					nIndex ++;
					return str.Mid(nBegin + 1, nIndex - nBegin - 2);
				} //if
			}
			else
			{
				GetIndexNextChars(str, nIndex, strSeparators);
				return str.Mid(nBegin, nIndex - nBegin);
			} //if
		} //if
	} //if
	return L"";
} //End GetParameterString

/////////////////////////////////////////////////////////////////
// Gets a name of the tag
//---------------------------------------------------------------
// Parameters:
//     In: str         - a tag's string
//         nIndex      - an index of the first char to the searching in the string
//    Out: nIndex   - an index of the first char after the parameter
// Return: Name of the tag (empty if it is not exist)  
/////////////////////////////////////////////////////////////////
CPPString CPPHtmlDrawerW::GetNameOfTag(CPPString & str, int & nIndex)
{
	CPPString strName = L"";
	GetIndexNextNoChars(str, nIndex, L" ");
	int nBegin = nIndex;
	GetIndexNextChars(str, nIndex, L" =");
	if (nIndex > nBegin)
		strName = str.Mid(nBegin, nIndex - nBegin);
	
	return strName;
} //End GetNameOfTag

/////////////////////////////////////////////////////
// Gets dimensions of the table
//---------------------------------------------------
//  In: sTable - the string contains a HTML table
// Return: cx - number of the columns
//         cy - number of the row
/////////////////////////////////////////////////////
SIZE CPPHtmlDrawerW::GetTableDimensions(CPPString & sTable)
{
	//ENG: A table dimensions by default
	//RUS: Размеры таблицы по умолчанию
	SIZE szTable = {0, 0};
	int nIndex = 0;
	int nCol = 0;
	while (nIndex < sTable.GetLength())
	{
		//ENG: Search a begin of the row
		//RUS: Ищем начало строки
		if (SearchTag(sTable, nIndex, L"tr"))
		{
			//ENG: Increment count of the rows
			//RUS: Увеличиваем количество строк
			szTable.cy++;

			//ENG: Count of the columns in current row
			//RUS: Количество колонок в текущей строке
			nCol = 0;
			int nEndRow;
			int nNewCell;
			do 
			{
				nEndRow = nNewCell = nIndex;
				//ENG: Search an end of the row or a begin of the cell
				//RUS: Ищем конец строки или начало ячейки
				SearchTag(sTable, nEndRow, L"/tr");
				SearchTag(sTable, nNewCell, L"td");
				if (nNewCell < nEndRow)
				{
					nIndex = nNewCell;

					//ENG: Passes a tag body and get a properties of the tag
					//RUS: Пропускаем тэг начала ячейки и получаем строку свойств тэга
					CPPString sTag;
					SearchNextTag(sTable, sTag, nNewCell);
					CPPString sProperties = SplitTag(sTag);

					//ENG: Analyses a properties of the tag
					//RUS: Анализируем свойства тэга
					STRUCT_CHANGESTYLE style;
					SIZE szSpan = AnalyseCellParam(sProperties, style, TRUE);

					//ENG: Increment count of the cells
					//RUS: Увеличиваем количество ячеек в строке
					nCol += szSpan.cx;

					//ENG: Jump to end of the cell
					//RUS: Переходим на конец ячейки
					SearchEndOfCell(sTable, nIndex);
				} //if
			} while (nNewCell < nEndRow);
			nIndex = nEndRow;
			if (nCol > szTable.cx)
				szTable.cx = nCol;
		} //if
	} //while
	return szTable;
} //End GetTableDimensions

/////////////////////////////////////////////////////
// CPPHtmlDrawer::SearchEndOfTable
//	Searching the end of the table
//---------------------------------------------------
//  Parameter:    
//		str - the string contains a HTML table
//		nIndex - index of the first char after the <table> tag
//	Return values:
//		nIndex - index of the begin char of a </table> tag
/////////////////////////////////////////////////////
void CPPHtmlDrawerW::SearchEndOfTable(CPPString & str, int & nIndex)
{
	int nBeginTable = nIndex + 7;
	int nEndTable = nIndex + 7;
	int nTable = 1;
	do
	{
		SearchTag(str, nBeginTable, L"table");
		SearchTag(str, nEndTable, L"/table");
		if (nBeginTable < nEndTable)
		{
			nTable++;
			nBeginTable += 7;
		}
		else if (nEndTable < nBeginTable)
		{
			nTable --;
			nEndTable += 8;
		} //if
	}
	while ((nBeginTable != nEndTable) && nTable); //while
	nIndex = nEndTable - 8;
} //End SearchEndOfTable

/////////////////////////////////////////////////////
// CPPHtmlDrawer::SearchEndOfRow
//	Searching the end of the row
//---------------------------------------------------
//  Parameter:    
//		str - the string contains a HTML table
//		nIndex - index of the first char after the <tr> tag
//	Return values:
//		nIndex - index of the begin char of a </tr> tag
/////////////////////////////////////////////////////
void CPPHtmlDrawerW::SearchEndOfRow(CPPString & str, int & nIndex)
{
	nIndex += 4;
	int nBeginRow, nEndRow, nStartTable;
	int nRow = 1;

	do
	{
		nBeginRow = nEndRow = nStartTable = nIndex;

		SearchTag(str, nBeginRow, L"tr");
		SearchTag(str, nEndRow, L"/tr");
		SearchTag(str, nStartTable, L"table");
		
		if ((nStartTable < nBeginRow) && (nStartTable < nEndRow))
		{
			SearchEndOfTable(str, nStartTable);
			nIndex = nStartTable + 6;
		}
		else if (nBeginRow < nEndRow)
		{
			nRow++;
			nIndex = nBeginRow + 4;
		}
		else if (nEndRow < nBeginRow)
		{
			nRow --;
			nIndex = nEndRow + 5;
		} //if
	}
	while ((nIndex < str.GetLength()) && nRow); //while
	nIndex -= 5;
} //End SearchEndOfRow

/////////////////////////////////////////////////////
// CPPHtmlDrawer::SearchEndOfCell
//	Searching the end of the cell
//---------------------------------------------------
//  Parameter:    
//		str - the string contains a HTML table
//		nIndex - index of the first char after the <td> tag
//	Return values:
//		nIndex - index of the begin char of a </td> tag
/////////////////////////////////////////////////////
void CPPHtmlDrawerW::SearchEndOfCell(CPPString & str, int & nIndex)
{
	nIndex += 4;
	int nEndCell, nStartTable;
	do
	{
		nEndCell = nStartTable = nIndex;

		SearchTag(str, nEndCell, L"/td");
		SearchTag(str, nStartTable, L"table");
		
		if (nStartTable < nEndCell)
		{
			SearchEndOfTable(str, nStartTable);
			nEndCell = nIndex = nStartTable + 6;
		}
		else
		{
			nIndex = nEndCell + 5;
		} //if
	}
	while (nStartTable < nEndCell); //while
	nIndex -= 5;
} //End SearchEndOfCell

///////////////////////////////////////////////////////////////////////
// Analysing the cell parameters
//---------------------------------------------------------------------
// Parameters:
//   In: strTag - str string contains parameters of the <table>, <td> or <tr> tags
//           cs - the structures contains the current styles
//		 bTable - 
//  Out:     cs - the structures contains the new styles
///////////////////////////////////////////////////////////////////////
SIZE CPPHtmlDrawerW::AnalyseCellParam(CPPString & sProperties, _STRUCT_CHANGESTYLE & cs, BOOL bTable)
{
	SIZE szSpan = {1, 1};
	if (sProperties.IsEmpty())
		return szSpan;
	
	int i = 0;
	CPPString sParameter;
	CPPString sValue;
	
	while (i < sProperties.GetLength())
	{
		//ENG: Searching a parameters of a tag
		//RUS: Поиск параметров тэга
		sValue = GetNextProperty(sProperties, i, sParameter);

		//ENG: Processes the specific parameters for <table> tag.
		//RUS: Обрабатываем специфические для тэга <table> параметры
		if(bTable)
		{
			if (sParameter == L"cellpadding")
			{
				cs.nMargin = GetLengthUnit(sValue, cs.nMargin);
			}
			else if (sParameter == L"cellspacing")
			{
				cs.nPadding = GetLengthUnit(sValue, cs.nPadding);
			} 
			else if (sParameter == L"background")
			{
				cs.strNameResBk = sValue;
			} //if
		} //if

		if (sParameter == L"rowspan")
		{
			szSpan.cy = GetLengthUnit(sValue, szSpan.cy);
		}
		else if (sParameter == L"colspan")
		{
			szSpan.cx = GetLengthUnit(sValue, szSpan.cx);
		}
		else if (sParameter == L"border")
		{
			cs.nBorderWidth = GetLengthUnit(sValue, cs.nBorderWidth);
			if (!cs.nBorderWidth)
				cs.nBorderStyle = CPPDrawManagerW::PEN_NULL;
			else if (CPPDrawManagerW::PEN_NULL == cs.nBorderStyle)
				cs.nBorderStyle = CPPDrawManagerW::PEN_SOLID;
		}
		else if (sParameter == L"borderstyle")
		{
			cs.nBorderStyle = StyleBorder(sValue, cs.nBorderStyle);
			if ((CPPDrawManagerW::PEN_NULL != cs.nBorderStyle) && !cs.nBorderWidth)
					cs.nBorderWidth = 1;
		}
		else if (sParameter == L"bordercolor")
		{
			if (m_bIsEnable)
				cs.crBorderLight = GetStyleColor(sValue, cs.crBorderLight);
			else
				cs.crBorderLight = GetColorByName(L"");
			cs.crBorderDark = cs.crBorderLight;
		}
		else if (sParameter == L"bordercolorlight")
		{
			if (m_bIsEnable)
				cs.crBorderLight = GetStyleColor(sValue, cs.crBorderLight);
			else
				cs.crBorderLight = GetColorByName(L"");
		}
		else if (sParameter == L"bordercolordark")
		{
			if (m_bIsEnable)
				cs.crBorderDark = GetStyleColor(sValue, cs.crBorderDark);
			else
				cs.crBorderDark = GetColorByName(L"");
		}
		else if (sParameter == L"bgcolor")
		{
			if (m_bIsEnable)
			{
				cs.crBkgnd = GetStyleColor(sValue, cs.crBkgnd);
				if (cs.nFillBkgnd < 0)
					cs.nFillBkgnd = CPPDrawManagerW::EFFECT_SOLID;
			} //if
		}
		else if (sParameter == L"bgmidcolor")
		{
			if (m_bIsEnable)
				cs.crMidBkgnd = GetStyleColor(sValue, cs.crMidBkgnd);
		}
		else if (sParameter == L"bgendcolor")
		{
			if (m_bIsEnable)
				cs.crEndBkgnd = GetStyleColor(sValue, cs.crEndBkgnd);
		}
		else if (sParameter == L"bgeffect")
		{
			if (m_bIsEnable)
				cs.nFillBkgnd = GetStyleBkgndEffect(sValue, cs.nFillBkgnd);
		}
		else if (sParameter == L"align")
		{
			cs.nHorzAlign = GetStyleHorzAlign(sValue, cs.nHorzAlign);
		}
		else if (sParameter == L"valign")
		{
			cs.nVertAlign = GetStyleVertAlign(sValue, cs.nVertAlign);
		}
		else if (sParameter == L"width")
		{
			cs.nCellWidth = GetLengthUnit(sValue, cs.nCellWidth);
		}
		else if (sParameter == L"height")
		{
			cs.nCellHeight = GetLengthUnit(sValue, cs.nCellHeight);
		} //if
	} //for

	//ENG:
	//RUS: 
	if ((CPPDrawManagerW::PEN_NULL == cs.nBorderStyle) || !cs.nBorderWidth)
	{
		cs.nBorderStyle = CPPDrawManagerW::PEN_NULL;
		cs.nBorderWidth = 0;
	}
	else if (CPPDrawManagerW::PEN_SOLID != cs.nBorderStyle)
	{
		cs.nBorderWidth = 1;
	}	//if

	//ENG: 
	//RUS: Для ячеек ширина всегда равна 1
	if (!bTable && cs.nBorderWidth)
		cs.nBorderWidth = 1;

	return szSpan;
} //End AnalyseCellParam

///////////////////////////////////////////////////////////////////////
// Analysing the image parameters
//---------------------------------------------------------------------
// Parameters:
//   In: sProperties - the sing contains
//           si - the structures contains the image parameters
//  Out:     si - the structures contains the image parameters
///////////////////////////////////////////////////////////////////////
void CPPHtmlDrawerW::AnalyseImageParam(CPPString & sProperties, _STRUCT_IMAGE & si)
{
	if (sProperties.IsEmpty())
		return;
	
	int i = 0;
	CPPString sParameter;
	CPPString sValue;
	
	while (i < sProperties.GetLength())
	{
		//ENG: Searching a parameters of a tag
		//RUS: Поиск параметров тэга
		sValue = GetNextProperty(sProperties, i, sParameter);

//		sParameter = SearchPropertyOfTag(sProperties, i);
//		sValue = GetParameterString(sProperties, i, _T('='), _T(" "));
			
		if (sParameter == L"index")
		{
			si.nIndexImageList = GetLengthUnit(sValue, si.nIndexImageList);
		}
		else if (sParameter == L"idres")
		{
			si.nIdRes = GetLengthUnit(sValue, si.nIdRes);
		}
		else if (sParameter == L"iddll")
		{
			si.nIdDll = GetLengthUnit(sValue, si.nIdDll);
		}
		else if (sParameter == L"handle")
		{
			si.nHandle = GetLengthUnit(sValue, si.nHandle);
		}
		else if (sParameter == L"file")
		{
			si.strSrcFile = GetStyleString(sValue, si.strSrcFile);
		}
		else if (sParameter == L"srcdll")
		{
			si.strPathDll = GetStyleString(sValue, si.strPathDll);
		}
		else if (sParameter == L"mask")
		{
			si.crMask = GetStyleColor(sValue, si.crMask);
			si.bUseMask = TRUE;
		}
		else if (sParameter == L"style")
		{
			si.nStyles = GetStyleImageShortForm(sValue);
			si.nHotStyles = si.nStyles;
		}
		else if (sParameter == L"hotstyle")
		{
			si.nHotStyles = GetStyleImageShortForm(sValue);
		}
		else if (sParameter == L"cx")
		{
			si.cx = GetLengthUnit(sValue, si.cx);
		}
		else if (sParameter == L"cy")
		{
			si.cy = GetLengthUnit(sValue, si.cy);
		}
		else if (sParameter == L"width")
		{
			si.bPercentWidth = IsPercentableValue(sValue);
			si.nWidth = GetLengthUnit(sValue, si.nWidth);
		}
		else if (sParameter == L"height")
		{
			si.bPercentHeight = IsPercentableValue(sValue);
			si.nHeight = GetLengthUnit(sValue, si.nHeight);
		}
		else if (sParameter == L"speed")
		{
			si.nSpeed = GetLengthUnit(sValue, si.nSpeed);
		} //if
	} //for
} //End AnalyseImageParam

CPPString CPPHtmlDrawerW::GetStyleString(CPPString str, CPPString strDefault)
{
	if (!str.IsEmpty())
		strDefault = str;
	return str;
}

///////////////////////////////////////////////////////////////////////
// Analysing the short form of the font style
//---------------------------------------------------------------------
// Parameters:
//   In: str - string contains parameters of the font in the short form
// Short form styles
//       [+] - positive style
//       [-] - inverse style
//       [b] - bold
//       [i] - italic
//       [u] - underlined
//       [s] - strikeout
//       [o] - overline
///////////////////////////////////////////////////////////////////////
void CPPHtmlDrawerW::GetStyleFontShortForm(CPPString & str)
{
	if (!str.IsEmpty())
	{
		BOOL bSetValue = TRUE;
		for (int i = 0; i < str.GetLength(); i++)
		{
			switch (str.GetAt(i))
			{
			case L'-':
				bSetValue = FALSE;
				break;
			case L'+':
				bSetValue = TRUE;
				break;
			case L'b':
				m_defStyle.nWeightFont = (bSetValue) ? FW_BOLD : FW_NORMAL;
				bSetValue = TRUE;
				break;
			case L'i':
				m_defStyle.bItalicFont = bSetValue;
				bSetValue = TRUE;
				break;
			case L'u':
				m_defStyle.bUnderlineFont = bSetValue;
				bSetValue = TRUE;
				break;
			case L's':
				m_defStyle.bStrikeOutFont = bSetValue;
				bSetValue = TRUE;
				break;
			case L'o':
				m_defStyle.bOverlineFont = bSetValue;
				bSetValue = TRUE;
				break;
			} //switch
		} //for
	} //if
} //End GetStyleFontShortForm

//Get font style value
UINT CPPHtmlDrawerW::GetStyleImageShortForm(CPPString & str)
{
	UINT uStyle = 0; //Original image
	
	if (!str.IsEmpty())
	{
		for (int i = 0; i < str.GetLength(); i++)
		{
			switch (str.GetAt(i))
			{
			case L'd':
				uStyle |= IMAGE_EFFECT_DARKEN;
				break;
			case L'g':
				uStyle |= IMAGE_EFFECT_GRAYEN;
				break;
			case L's':
				if (m_szOffsetShadow.cx || m_szOffsetShadow.cy)
				{
					if (m_bGradientShadow)
						uStyle |= IMAGE_EFFECT_GRADIENT_SHADOW;
					else uStyle |= IMAGE_EFFECT_MONO_SHADOW;
				} //if
				break;
			case L'l':
				uStyle |= IMAGE_EFFECT_LIGHTEN;
				break;
			} //switch
		} //for
	} //if
	
	return uStyle;
} //End GetStyleImageShortForm

BOOL CPPHtmlDrawerW::IsPercentableValue(CPPString & str)
{
	if (!str.IsEmpty())
	{
		if (str.GetAt(str.GetLength() - 1) == L'%')
			return TRUE;
	}
	return FALSE;
}

int CPPHtmlDrawerW::GetStyleBkgndEffect(CPPString & str, int nDefault)
{
	if (!str.IsEmpty())
	{
		if (str == L"transparent")
			nDefault = -1;
		else if (str == L"solid")
			nDefault = CPPDrawManagerW::EFFECT_SOLID;
		else if (str == L"hgradient")
			nDefault = CPPDrawManagerW::EFFECT_HGRADIENT;
		else if (str == L"vgradient")
			nDefault = CPPDrawManagerW::EFFECT_VGRADIENT;
		else if (str == L"hcgradient")
			nDefault = CPPDrawManagerW::EFFECT_HCGRADIENT;
		else if (str == L"vcgradient")
			nDefault = CPPDrawManagerW::EFFECT_VCGRADIENT;
		else if (str == L"3hgradient")
			nDefault = CPPDrawManagerW::EFFECT_3HGRADIENT;
		else if (str == L"3vgradient")
			nDefault = CPPDrawManagerW::EFFECT_3VGRADIENT;
#ifdef USE_SHADE
		else if (str == L"noise")
			nDefault = CPPDrawManagerW::EFFECT_NOISE;
		else if (str == L"diagshade")
			nDefault = CPPDrawManagerW::EFFECT_DIAGSHADE;
		else if (str == L"hshade")
			nDefault = CPPDrawManagerW::EFFECT_HSHADE;
		else if (str == L"vshade")
			nDefault = CPPDrawManagerW::EFFECT_VSHADE;
		else if (str == L"hbump")
			nDefault = CPPDrawManagerW::EFFECT_HBUMP;
		else if (str == L"vbump")
			nDefault = CPPDrawManagerW::EFFECT_VBUMP;
		else if (str == L"softbump")
			nDefault = CPPDrawManagerW::EFFECT_SOFTBUMP;
		else if (str == L"hardbump")
			nDefault = CPPDrawManagerW::EFFECT_HARDBUMP;
		else if (str == L"metal")
			nDefault = CPPDrawManagerW::EFFECT_METAL;
#endif
		else nDefault = GetLengthUnit(str, nDefault);
	} //if

	return nDefault;
} //End GetStyleBkgndEffect

int CPPHtmlDrawerW::GetTableWidth(CPPString & str, int nClientWidth, int nMinWidth, BOOL bSet /* = FALSE */)
{
	if (!str.IsEmpty())
	{
		int i = 0;
		CPPString strProperty;
		CPPString strParameter;
		
		while (i < str.GetLength())
		{
			strProperty = SearchPropertyOfTag(str, i);
			strParameter = GetParameterString(str, i, L'=', L" ");
			strProperty.MakeLower();
			
			if (strProperty == L"width")
			{
				if (IsPercentableValue(strParameter))
				{
					int nWidth = GetLengthUnit(strParameter, 100);
					if (bSet)
					{
						if (nWidth <= 100)
							nClientWidth = ::MulDiv(nMinWidth, 100, nWidth);
						else
							nClientWidth = ::MulDiv(nMinWidth, nWidth, 100);
					}
					else
					{
						if (nWidth < 100)
							nClientWidth = ::MulDiv(nClientWidth, nWidth, 100);
					} //if
				}
				else
				{
					nClientWidth = GetLengthUnit(strParameter, nMinWidth);
				} //if
				break;
			} //if
		} //while
	} //if

	if (nClientWidth < nMinWidth)
		nClientWidth = nMinWidth;

	return nClientWidth;
} //End GetTableWidth

void CPPHtmlDrawerW::DrawBackgroundImage(HDC hDC, int nDestX, int nDestY, int nWidth, int nHeight, CPPString strNameImage)
{
	if (!m_bIsEnable)
		return;
	if (strNameImage.IsEmpty())
		return;
	if (strNameImage.GetLength() < 6)
		return;

	HBITMAP hBitmap = NULL;

	int nIndex = 0;
	if (GetIndexNextAlphaNum(strNameImage, nIndex))
	{
		int nBegin = nIndex;
		//Searching end of the style name
		wchar_t chSymbol = GetIndexNextChars(strNameImage, nIndex, L" :");
		if (0 != chSymbol)
		{
			//Gets a property's name
			CPPString strName = strNameImage.Mid(nBegin, nIndex - nBegin);
			//Gets a property's value
			CPPString strParameter = GetParameterString(strNameImage, nIndex, L':');
			
			if (strName == L"idres")
			{
				UINT nID = (UINT)GetLengthUnit(strParameter, 0);
				hBitmap = GetBitmapFromResources(nID);
			}
			else if (strName == L"iddll")
			{
				UINT nID = (UINT)GetLengthUnit(strParameter, 0);
				hBitmap = GetBitmapFromDll(nID);
			}
			else if (strName == L"file")
			{
				hBitmap = GetBitmapFromFile(strParameter);
			} //if
		} //if
	} //if

	if (NULL == hBitmap)
		return;

	SIZE sz;
	m_drawmanager.GetSizeOfBitmap(hBitmap, &sz);
	HDC hSrcDC = ::CreateCompatibleDC(hDC);
	HBITMAP hOldBitmap = (HBITMAP)::SelectObject(hSrcDC, hBitmap);
	m_drawmanager.MultipleCopy(hDC, nDestX, nDestY, nWidth, nHeight, hSrcDC, 0, 0, sz.cx, sz.cy);
	::SelectObject(hSrcDC, hOldBitmap);
	::DeleteDC(hSrcDC);

	::DeleteObject(hBitmap);
	hBitmap = NULL;
} //End of DrawBackgroundImage

////////////////////////////////////////////////////////////////////
// CPPHtmlDrawer::SetTooltipShadow()
//		Sets a image's shadow.
//------------------------------------------------------------------
// Parameters:
//		nOffsetX, 
//		nOffsetY		- The offsets of the tooltip's shadow from the tooltip's window.
//		nDarkenPercent	- So far as colors under the shadow will be darken (0 - 100)
//      bGradient		- TRUE to use a gradient shadow.
//		nDepthX,
//		nDepthY			- The gradient depths of the tooltip's shadow.
////////////////////////////////////////////////////////////////////
void CPPHtmlDrawerW::SetImageShadow(int nOffsetX, int nOffsetY, BYTE nDarkenPercent /* = 50 */, 
								  BOOL bGradient /* = TRUE */, int nDepthX /* = 7 */, int nDepthY /* = 7 */)
{
	m_szOffsetShadow.cx = nOffsetX;
	m_szOffsetShadow.cy = nOffsetY;
	m_szDepthShadow.cx = nDepthX;
	m_szDepthShadow.cy = nDepthY;
	m_nDarkenShadow = min(100, nDarkenPercent);
	m_bGradientShadow = bGradient;
	BYTE nColor = (BYTE)::MulDiv(255, 100 - m_nDarkenShadow, 100);
	m_crShadow = RGB(nColor, nColor, nColor);
} //End of SetTooltipShadow

CPPString CPPHtmlDrawerW::GetWordWrap(CPPString & str, int nMaxSize, int & nRealSize)
{
	int nCurIndex = 0;
	int nLastIndex = 0;
	SIZE sz = {0, 0};
	wchar_t tch = L' ';
	CPPString sResult = L"";
	while ((sz.cx <= nRealSize) && (0 != tch))
	{
		nLastIndex = nCurIndex;
		nCurIndex ++;
		tch = GetIndexNextChars(str, nCurIndex, PPHTMLDRAWER_BREAK_CHARS);
		::GetTextExtentPoint32W(m_hDC, str, nCurIndex, &sz);
	} //while

	if (0 == nLastIndex)
	{
		if (nMaxSize == nRealSize)
		{
			//RUS: Разрывов в строке не обнаружено, поэтому будем разбивать строку 
			//     по символам, а не по словам
			sz.cx = 0;
			int i;
			for (i = 1; i < str.GetLength(); i++)
			{
				::GetTextExtentPoint32W(m_hDC, str, i + 1, &sz);
				if (sz.cx > nRealSize)
				{
					sResult = str.Left(i);
					str = str.Mid(i);
					::GetTextExtentPoint32W(m_hDC, sResult, i, &sz);
					nRealSize = sz.cx;
					return sResult;
				} //if
			} //for
			::GetTextExtentPoint32W(m_hDC, str, i, &sz);
			//RUS: Невозможно разбить строку, выводим целиком
			sResult = str;
			str.Empty();
		}
		else
		{
			//RUS: В отставшееся место текущей строки не влазит ни одного слова
			sz.cx = 0;
		} //if
	}
	else 
	{
		sResult = str.Left(nLastIndex + 1);
		str = str.Mid(nLastIndex + 1);
		sResult.TrimRight();
		::GetTextExtentPoint32W(m_hDC, sResult, sResult.GetLength(), &sz);
//		str.TrimRight();
		str.TrimLeft();
	} //if
	nRealSize = sz.cx;
	return sResult;
} //End of GetWordWrap

int CPPHtmlDrawerW::GetCountOfChars(CPPString str, wchar_t tchar /*= _T(' ')*/)
{
	int nCount = 0;
	//ENG:
	//RUS:
	for (int i = 0; i < str.GetLength(); i++)
	{
		if (tchar == str.GetAt(i))
			nCount++;
	} //if
	return nCount;
}