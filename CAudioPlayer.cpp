/*
	CAudioPlayer.cpp
	Classe (semi-factory) per il player audio.
	Gestisce la riproduzione dei files audio supportati tramite gli oggetti derivati dalla classe CAudioObject.
	Luca Piergentili, 12/07/03
	lpiergentili@yahoo.com
*/
#include "pragma.h"
#include "env.h"
#include "macro.h"
#include "window.h"
#include <mmsystem.h>
#include "mmaudio.h"
#include "CAudioObject.h"
#include "CAudioVolume.h"
#include "CAudioWav.h"
#include "CAudioCDA.h"
#include "CAudioMP3.h"
#include "CAudioPlayer.h"

#include "traceexpr.h"
//#define _TRACE_FLAG			_TRFLAG_TRACEOUTPUT // opzioni: _TRFLAG_NOTRACE, _TRFLAG_TRACEFILE, _TRFLAG_TRACECONSOLE, _TRFLAG_TRACEOUTPUT, _TRFLAG_TRACEBREAKPOINT
#define _TRACE_FLAG			_TRFLAG_NOTRACE // opzioni: _TRFLAG_NOTRACE, _TRFLAG_TRACEFILE, _TRFLAG_TRACECONSOLE, _TRFLAG_TRACEOUTPUT, _TRFLAG_TRACEBREAKPOINT
#define _TRACE_FLAG_INFO	_TRACE_FLAG
#define _TRACE_FLAG_WARN	_TRACE_FLAG
#define _TRACE_FLAG_ERR		_TRACE_FLAG

static const char* pAudioFormatsArray[] = {
	WAV_EXTENSION,
	CDA_EXTENSION,
	MP3_EXTENSION,
	".mid",
	".wma",
	".asx",
	".ram",
	".rm",
	".ogg",
	".aac",
	".ac3",
	".dts",
	NULL
};

/*
	CAudioPlayer()
*/
CAudioPlayer::CAudioPlayer(HWND hWnd/* = NULL*/)
{
	m_hWnd = hWnd;
	m_pAudioObject = NULL;
	m_pAudioWav = NULL;
	m_pAudioCDA = NULL;
	m_pAudioMPeg = NULL;
}

/*
	~CAudioPlayer()
*/
CAudioPlayer::~CAudioPlayer()
{
	if(m_pAudioWav)
		delete m_pAudioWav,m_pAudioWav = NULL;
	if(m_pAudioCDA)
		delete m_pAudioCDA,m_pAudioCDA = NULL;
	if(m_pAudioMPeg)
		delete m_pAudioMPeg,m_pAudioMPeg = NULL;
	m_pAudioObject = NULL;
}

/*
	IsAudioFile()
*/
BOOL CAudioPlayer::IsAudioFile(LPCSTR lpcszAudioFileName)
{
	// formato audio, non necessariamente supportato dal player
	for(register int i=0; pAudioFormatsArray[i]!=NULL; i++)
		if(striright(lpcszAudioFileName,pAudioFormatsArray[i])==0)
			return(TRUE);
	
	return(FALSE);
}

/*
	IsSupportedFormat()
*/
BOOL CAudioPlayer::IsSupportedFormat(LPCSTR lpcszAudioFileName)
{
	// formato audio supportato dal player
	return(striright(lpcszAudioFileName,MP3_EXTENSION)==0 || striright(lpcszAudioFileName,WAV_EXTENSION)==0 || striright(lpcszAudioFileName,CDA_EXTENSION)==0);
}

/*
	Open()
*/
BOOL CAudioPlayer::Open(LPCSTR lpcszAudioFileName)
{
	// crea, se necessario, l'istanza dell'oggetto relativo al formato
	if(striright(lpcszAudioFileName,WAV_EXTENSION)==0)
		m_pAudioObject = m_pAudioWav ? m_pAudioWav : (m_pAudioWav = new CAudioWav(m_hWnd));
	else if(striright(lpcszAudioFileName,CDA_EXTENSION)==0)
		m_pAudioObject = m_pAudioCDA ? m_pAudioCDA : (m_pAudioCDA = new CAudioCDA(m_hWnd));
	else if(striright(lpcszAudioFileName,MP3_EXTENSION)==0)
		m_pAudioObject = m_pAudioMPeg ? m_pAudioMPeg : (m_pAudioMPeg = new CAudioMP3(m_hWnd));
	else
		m_pAudioObject = NULL;

	// apre il file audio con l'oggetto relativo al formato
	return(m_pAudioObject ? m_pAudioObject->Open(lpcszAudioFileName)==MMSYSERR_NOERROR : FALSE);
}
