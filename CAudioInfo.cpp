/*
	CAudioInfo.cpp
	Classe (semi-factory) per le info sull'audio.
	Ricava le info sui files audio supportati tramite gli oggetti derivati dalla classe CAudioInfoObject.
	Luca Piergentili, 14/07/03
	lpiergentili@yahoo.com
*/
#include "pragma.h"
#include "env.h"
#include "macro.h"
#include "window.h"
#include <mmsystem.h>
#include "mmaudio.h"
#include "CAudioInfoObject.h"
#include "CAudioWavInfo.h"
#include "CAudioCDAInfo.h"
#include "CAudioMP3Info.h"
#include "CAudioInfo.h"

#include "traceexpr.h"
//#define _TRACE_FLAG			_TRFLAG_TRACEOUTPUT // opzioni: _TRFLAG_NOTRACE, _TRFLAG_TRACEFILE, _TRFLAG_TRACECONSOLE, _TRFLAG_TRACEOUTPUT, _TRFLAG_TRACEBREAKPOINT
#define _TRACE_FLAG			_TRFLAG_NOTRACE // opzioni: _TRFLAG_NOTRACE, _TRFLAG_TRACEFILE, _TRFLAG_TRACECONSOLE, _TRFLAG_TRACEOUTPUT, _TRFLAG_TRACEBREAKPOINT
#define _TRACE_FLAG_INFO	_TRACE_FLAG
#define _TRACE_FLAG_WARN	_TRACE_FLAG
#define _TRACE_FLAG_ERR		_TRACE_FLAG

/*
	CAudioInfo()
*/
CAudioInfo::CAudioInfo(LPCSTR lpcszAudioFileName/* = NULL*/)
{
	m_pAudioInfoObject = NULL;
	m_pAudioInfoWav = NULL;
	m_pAudioInfoCDA = NULL;
	m_pAudioInfoMPeg = NULL;
	
	if(lpcszAudioFileName)
		Link(lpcszAudioFileName);
}

/*
	~CAudioInfo()
*/
CAudioInfo::~CAudioInfo()
{
	if(m_pAudioInfoWav)
		delete m_pAudioInfoWav,m_pAudioInfoWav = NULL;
	if(m_pAudioInfoCDA)
		delete m_pAudioInfoCDA,m_pAudioInfoCDA = NULL;
	if(m_pAudioInfoMPeg)
		delete m_pAudioInfoMPeg,m_pAudioInfoMPeg = NULL;
	m_pAudioInfoObject = NULL;
}

/*
	Link()
*/
int CAudioInfo::Link(LPCSTR lpcszAudioFileName)
{
	if(striright(lpcszAudioFileName,WAV_EXTENSION)==0)
		m_pAudioInfoObject = m_pAudioInfoWav ? m_pAudioInfoWav : (m_pAudioInfoWav = new CAudioWavInfo());
	else if(striright(lpcszAudioFileName,CDA_EXTENSION)==0)
		m_pAudioInfoObject = m_pAudioInfoCDA ? m_pAudioInfoCDA : (m_pAudioInfoCDA = new CAudioCDAInfo());
	else if(striright(lpcszAudioFileName,MP3_EXTENSION)==0)
		m_pAudioInfoObject = m_pAudioInfoMPeg ? m_pAudioInfoMPeg : (m_pAudioInfoMPeg = new CAudioMp3Info());
	else
		m_pAudioInfoObject = NULL;

	return(m_pAudioInfoObject ? m_pAudioInfoObject->Link(lpcszAudioFileName) : 0);
}
