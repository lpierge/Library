/*
	CId3Lib.cpp
	Classe d'interfaccia con la libreria id3lib (http://www.id3lib.org/) per info/tags sui files .mp3.
	Luca Piergentili, 21/06/03
	lpiergentili@yahoo.com

	Vedi le note CId3Lib.h.
*/
#include "pragma.h"
#include "env.h"
#include "macro.h"
#include "strings.h"
#include <string.h>
#include "window.h"
#include "win32api.h"
#include "mmaudio.h"
#include "CId3v1Tag.h"
#include "CId3Lib.h"

#include "traceexpr.h"
//#define _TRACE_FLAG			_TRFLAG_TRACEOUTPUT // opzioni: _TRFLAG_NOTRACE, _TRFLAG_TRACEFILE, _TRFLAG_TRACECONSOLE, _TRFLAG_TRACEOUTPUT, _TRFLAG_TRACEBREAKPOINT
#define _TRACE_FLAG			_TRFLAG_NOTRACE // opzioni: _TRFLAG_NOTRACE, _TRFLAG_TRACEFILE, _TRFLAG_TRACECONSOLE, _TRFLAG_TRACEOUTPUT, _TRFLAG_TRACEBREAKPOINT
#define _TRACE_FLAG_INFO	_TRACE_FLAG
#define _TRACE_FLAG_WARN	_TRACE_FLAG
#define _TRACE_FLAG_ERR		_TRACE_FLAG

/*
	CId3Lib()
*/
CId3Lib::CId3Lib(LPCSTR lpcszFileName /*= NULL*/)
{
	memset(m_szYear,'\0',sizeof(m_szYear));

	if(lpcszFileName)
		Link(lpcszFileName);
}

/*
	Link()
*/
BOOL CId3Lib::Link(LPCSTR lpcszFileName)
{
	BOOL bRet = FALSE;
	
	if(::FileExists(lpcszFileName) && striright(lpcszFileName,MP3_EXTENSION)==0)
		bRet = CId3v1Tag::Link(lpcszFileName);

	return(bRet);
}
