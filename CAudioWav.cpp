/*
	CAudioWav.cpp
	Classe (derivata) per l'interfaccia audio verso i files di tipo .wav
	Come decoder viene utilizzata l'interfaccia MCI, acceduta tramite le classi presenti in mciaudio.
	Luca Piergentili, 13/07/03
	lpiergentili@yahoo.com
*/
#include "pragma.h"
#include "env.h"
#include "macro.h"
#include "window.h"
#include "mmaudio.h"
#include "mciaudio.h"
#include "CAudioWav.h"

#include "traceexpr.h"
//#define _TRACE_FLAG			_TRFLAG_TRACEOUTPUT // opzioni: _TRFLAG_NOTRACE, _TRFLAG_TRACEFILE, _TRFLAG_TRACECONSOLE, _TRFLAG_TRACEOUTPUT, _TRFLAG_TRACEBREAKPOINT
#define _TRACE_FLAG			_TRFLAG_NOTRACE // opzioni: _TRFLAG_NOTRACE, _TRFLAG_TRACEFILE, _TRFLAG_TRACECONSOLE, _TRFLAG_TRACEOUTPUT, _TRFLAG_TRACEBREAKPOINT
#define _TRACE_FLAG_INFO	_TRACE_FLAG
#define _TRACE_FLAG_WARN	_TRACE_FLAG
#define _TRACE_FLAG_ERR		_TRACE_FLAG

/*
	CAudioWav()
*/
CAudioWav::CAudioWav(HWND hWnd/*=NULL*/)
{
	m_hWnd = hWnd;
	memset(&m_mcifi,'\0',sizeof(MCIFILEINFO));
	m_mcifi.mmaPlayerStatus = mmAudioPmClosed;
	m_pMCIAudioWave = new CMCIAudioWave(&m_mcifi);
}

/*
	~CAudioWav()
*/
CAudioWav::~CAudioWav()
{
	if(m_pMCIAudioWave)
	{
		if(m_mcifi.mmaPlayerStatus==mmAudioPmPlaying)
			m_pMCIAudioWave->Stop();
		if(m_mcifi.mmaPlayerStatus!=mmAudioPmClosed)
			m_pMCIAudioWave->Close();
		delete m_pMCIAudioWave,m_pMCIAudioWave = NULL;
	}
}

/*
	GetLength()
*/
QWORD CAudioWav::GetLength(long& lMinutes,long& lSeconds)
{
	lMinutes = m_pMCIAudioWave ? m_mcifi.lMinutes : 0L;
	lSeconds = m_pMCIAudioWave ? m_mcifi.lSeconds : 0L;
	
	return(m_pMCIAudioWave ? (long)m_mcifi.qwFileSize : 0L);
}
