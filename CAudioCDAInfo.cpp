/*
	CAudioCDAInfo.cpp
	Classe per info sui files .cda.
	Luca Piergentili, 14/07/03
	lpiergentili@yahoo.com
*/
#include "pragma.h"
#include "env.h"
#include "macro.h"
#include <string.h>
#include "window.h"
#include "mmaudio.h"
#include "mciaudio.h"
#include "CAudioCDAInfo.h"

#include "traceexpr.h"
//#define _TRACE_FLAG			_TRFLAG_TRACEOUTPUT // opzioni: _TRFLAG_NOTRACE, _TRFLAG_TRACEFILE, _TRFLAG_TRACECONSOLE, _TRFLAG_TRACEOUTPUT, _TRFLAG_TRACEBREAKPOINT
#define _TRACE_FLAG			_TRFLAG_NOTRACE // opzioni: _TRFLAG_NOTRACE, _TRFLAG_TRACEFILE, _TRFLAG_TRACECONSOLE, _TRFLAG_TRACEOUTPUT, _TRFLAG_TRACEBREAKPOINT
#define _TRACE_FLAG_INFO	_TRACE_FLAG
#define _TRACE_FLAG_WARN	_TRACE_FLAG
#define _TRACE_FLAG_ERR		_TRACE_FLAG

/*
	CAudioCDAInfo()
*/
CAudioCDAInfo::CAudioCDAInfo(LPCSTR lpcszCdaFileName/*=NULL*/)
{
	memset(&m_cdaTrackInfo,'\0',sizeof(CDATRACKINFO));
	m_pMCIAudioCDA = new CMCIAudioCDA();
	if(m_pMCIAudioCDA)
		if(lpcszCdaFileName)
			Link(lpcszCdaFileName);
}

/*
	~CAudioCDAInfo()
*/
CAudioCDAInfo::~CAudioCDAInfo()
{
	if(m_pMCIAudioCDA)
	{
		Unlink();
		delete m_pMCIAudioCDA,m_pMCIAudioCDA = NULL;
	}
	memset(&m_cdaTrackInfo,'\0',sizeof(CDATRACKINFO));
}

/*
	Link()
*/
int CAudioCDAInfo::Link(LPCSTR lpcszCdaFileName)
{
	// inizializza
	memset(&m_cdaTrackInfo,'\0',sizeof(CDATRACKINFO));

	// nome traccia
	char* p = (char*)strrchr(lpcszCdaFileName,'\\');
	if(p)
		p++;
	if(!p)
		p = (char*)lpcszCdaFileName;
	strcpyn(m_cdaTrackInfo.szTrackName,p,sizeof(m_cdaTrackInfo.szTrackName));
	p = strchr(m_cdaTrackInfo.szTrackName,'.');
	if(p)
		*p = '\0';

	// ricava il numero di traccia relativa al file (brano) - lo so, e' un po' 'na pecionata ma e' efficace
	// (ripreso da mciaudio.cpp)
	m_cdaTrackInfo.nTrack = 0;
	memset(m_cdaTrackInfo.szTrackNumber,'\0',sizeof(m_cdaTrackInfo.szTrackNumber));
	int i = 0;
	p = (char*)lpcszCdaFileName;
	while(*p && i < sizeof(m_cdaTrackInfo.szTrackNumber)-1)
	{
		if(isdigit(*p))
			m_cdaTrackInfo.szTrackNumber[i++] = *p;
		p++;
	}
	m_cdaTrackInfo.nTrack = atoi(m_cdaTrackInfo.szTrackNumber);
	if(m_cdaTrackInfo.nTrack <= 0 || m_cdaTrackInfo.nTrack > 1965)
	{
		m_cdaTrackInfo.nTrack = 0;
		memset(m_cdaTrackInfo.szTrackNumber,'\0',sizeof(m_cdaTrackInfo.szTrackNumber));
		return(MCIERR_OUTOFRANGE);
	}
	
	// ricava le informazioni relative alla traccia
	MCIERROR mcierror = MMSYSERR_NOERROR;
	if((mcierror = m_pMCIAudioCDA->Open(lpcszCdaFileName))==MMSYSERR_NOERROR)
		mcierror = m_pMCIAudioCDA->GetTrackInfo(&m_cdaTrackInfo);

	return(mcierror);
}

/*
	Unlink()
*/
void CAudioCDAInfo::Unlink(void)
{
	if(m_pMCIAudioCDA)
	{
		m_pMCIAudioCDA->Close();
		memset(&m_cdaTrackInfo,'\0',sizeof(CDATRACKINFO));
	}
}

/*
	GetLength()
*/
QWORD CAudioCDAInfo::GetLength(long& lMinutes,long& lSeconds)
{
	lMinutes = lSeconds = 0L;
	
	if(m_pMCIAudioCDA)
	{
		lMinutes = m_cdaTrackInfo.lMinutes;
		lSeconds = m_cdaTrackInfo.lSeconds;
	}
	
	return(m_cdaTrackInfo.qwTrackSize);
}
