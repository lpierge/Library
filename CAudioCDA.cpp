/*
	CAudioCDA.cpp
	Classe (derivata) per l'interfaccia audio verso i files di tipo .mp3
	Come decoder viene utilizzata l'interfaccia MCI, acceduta tramite le classi presenti in mciaudio.
	Luca Piergentili, 10/06/03
	lpiergentili@yahoo.com
*/
#include "pragma.h"
#include "env.h"
#include "macro.h"
#include "window.h"
#include "mmaudio.h"
#include "mciaudio.h"
#include "CAudioCDA.h"

#include "traceexpr.h"
//#define _TRACE_FLAG			_TRFLAG_TRACEOUTPUT // opzioni: _TRFLAG_NOTRACE, _TRFLAG_TRACEFILE, _TRFLAG_TRACECONSOLE, _TRFLAG_TRACEOUTPUT, _TRFLAG_TRACEBREAKPOINT
#define _TRACE_FLAG			_TRFLAG_NOTRACE // opzioni: _TRFLAG_NOTRACE, _TRFLAG_TRACEFILE, _TRFLAG_TRACECONSOLE, _TRFLAG_TRACEOUTPUT, _TRFLAG_TRACEBREAKPOINT
#define _TRACE_FLAG_INFO	_TRACE_FLAG
#define _TRACE_FLAG_WARN	_TRACE_FLAG
#define _TRACE_FLAG_ERR		_TRACE_FLAG

/*
	CAudioCDA()
*/
CAudioCDA::CAudioCDA(HWND hWnd/*=NULL*/)
{
	m_hWnd = hWnd;
	m_pMCIAudioCda = new CMCIAudioCDA();
}

/*
	~CAudioCDA()
*/
CAudioCDA::~CAudioCDA()
{
	if(m_pMCIAudioCda)
	{
		if(m_pMCIAudioCda->GetStatus()==mmAudioPmPlaying)
			m_pMCIAudioCda->Stop();
		if(m_pMCIAudioCda->GetStatus()!=mmAudioPmClosed)
			m_pMCIAudioCda->Close();
		delete m_pMCIAudioCda,m_pMCIAudioCda = NULL;
	}
}

/*
	GetLength()
*/
QWORD CAudioCDA::GetLength(long& lMinutes,long& lSeconds)
{
	CDATRACKINFO cdaTrackInfo = {0};
	
	if(m_pMCIAudioCda)
		m_pMCIAudioCda->GetTrackInfo(&cdaTrackInfo);
	
	lMinutes = cdaTrackInfo.lMinutes;
	lSeconds = cdaTrackInfo.lSeconds;
	
	return(cdaTrackInfo.qwTrackSize);
}
