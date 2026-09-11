/*
	CAudioMP3.cpp
	Classe (derivata) per l'interfaccia audio verso i files di tipo .mp3
	Luca Piergentili, 07/06/03
	lpiergentili@yahoo.com
*/
#include "pragma.h"
#include "env.h"
#include "macro.h"
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "strings.h"
#include "window.h"
#include "CThread.h"
#include "CFindFile.h"
#include <mmsystem.h>
#include "mmaudio.h"
#include "CAudioObject.h"
#include "player.h"
#include "maddll.h"
#include "CMP3Info.h"
#include "CId3Lib.h"
#include "CAudioMP3.h"
#include "mp3check.h"

#include "traceexpr.h"
#define _TRACE_FLAG			_TRFLAG_TRACEOUTPUT // opzioni: _TRFLAG_NOTRACE, _TRFLAG_TRACEFILE, _TRFLAG_TRACECONSOLE, _TRFLAG_TRACEOUTPUT, _TRFLAG_TRACEBREAKPOINT
//#define _TRACE_FLAG			_TRFLAG_NOTRACE // opzioni: _TRFLAG_NOTRACE, _TRFLAG_TRACEFILE, _TRFLAG_TRACECONSOLE, _TRFLAG_TRACEOUTPUT, _TRFLAG_TRACEBREAKPOINT
#define _TRACE_FLAG_INFO	_TRACE_FLAG
#define _TRACE_FLAG_WARN	_TRACE_FLAG
#define _TRACE_FLAG_ERR		_TRACE_FLAG

void MadCallback(int id,unsigned long secs,void *pData)
{
	static unsigned long duration = 0;
	
	if(id==0)
	{
		char* fname = (char*)pData;
		TRACEEXPR((_TRACE_FLAG_INFO,__NOFILE__,__NOLINE__,
				"MadCallback(%d): secs=%ld file=%s\n",
				id,
				secs,
				fname
		));
		
		duration = secs;
	}
	else if(id==1)
	{
		struct player* pl = (struct player*)pData;
		int progress = (100 * secs) / duration;
		TRACEEXPR((_TRACE_FLAG_INFO,__NOFILE__,__NOLINE__,
				"MadCallback(%d): tot bytes=%ld tot time=%ld/%ld tot secs=%ld - %d%%\n",
				id,
				pl->stats.total_bytes,
				pl->stats.total_time.seconds,
				duration,
				secs,
				progress
		));
	}
}

/*
	CAudioMP3()
*/
CAudioMP3::CAudioMP3(HWND /*hWnd*/ /* = NULL*/)
{
	// inizializza soltanto
	m_pPlayThread = NULL;
	m_nPlayerMode = mmAudioPmClosed;
	m_hWnd = NULL;
	m_nMsg = (UINT)-1;
	m_nThreadPriority = (UINT)-1;
	memset(m_szAudioFileName,'\0',sizeof(m_szAudioFileName));
	memset(&m_playerThreadParams,'\0',sizeof(m_playerThreadParams));
	m_playerThreadParams.status = &m_nPlayerMode;
	m_playerThreadParams.hEvent = ::CreateEvent(NULL,TRUE,FALSE,NULL);
	::SetEvent(m_playerThreadParams.hEvent);
	m_pMadData = ::MadPlayerInit();
	m_playerThreadParams.mad_data = m_pMadData;
	m_playerThreadParams.pFnCallback = MadCallback;
}

/*
	~CAudioMP3()
*/
CAudioMP3::~CAudioMP3()
{
	// chiude l'evento per la sincronizzazione (per lo stato corrente)
	if(m_playerThreadParams.hEvent)
		::CloseHandle(m_playerThreadParams.hEvent);
	
	// chiude il player
	if(GetStatus()!=mmAudioPmClosed)
		Close();

	// chiude il riproduttore
	::MadPlayerDone(m_pMadData);

	// imposta lo status
	SetStatus(mmAudioPmClosed);
}

/*
	Open()
*/
MMAERROR CAudioMP3::Open(LPCSTR lpcszAudioFileName)
{
	// controlla che il file esista
	if(!::FileExists(lpcszAudioFileName))
		return(MMAERR_PLAYER_INVALID_FILE);

	if(m_Mp3Info.Load(lpcszAudioFileName))
		MadCallback(0,m_Mp3Info.GetLength(),(void*)lpcszAudioFileName);
	
	m_id3Lib.Link(lpcszAudioFileName);
	
	// se non e' gia' chiuso provvede
	if(GetStatus()!=mmAudioPmClosed)
		Close();

	// imposta l'evento per la sincronizzazione (per lo stato corrente)
	::SetEvent(m_playerThreadParams.hEvent);

	// imposta lo status
	strcpyn(m_szAudioFileName,lpcszAudioFileName,sizeof(m_szAudioFileName));
	SetStatus(mmAudioPmOpened);
	SetStatus(mmAudioPmReady);

	return(MMAERR_NOERROR);
}

/*
	Play()
*/
MMAERROR CAudioMP3::Play(void)
{
	// se non e' gia' stoppato, provvede
	if(GetStatus()!=mmAudioPmStopped)
		Stop();

	// imposta il nome file da riprodurre
	strcpyn(m_playerThreadParams.szAudioFileName,m_szAudioFileName,sizeof(m_playerThreadParams.szAudioFileName));

	// controlla che sia un MP3 valido
	if(!is_valid_mp3(m_szAudioFileName))
		return(MMAERR_PLAYER_INVALID_FILE);

	// lancia il thread per il riproduttore
	// "BUG: Modal Dialogs in MFC Regular DLL Cause ASSERT in AfxWndProc"
	//m_pPlayThread = ::BeginThread(MadPlayerThread,(LPVOID)&m_playerThreadParams,THREAD_PRIORITY_TIME_CRITICAL,0L,CREATE_SUSPENDED);
	m_pPlayThread = ::AfxBeginThread(MadPlayerThread,(LPVOID)&m_playerThreadParams,THREAD_PRIORITY_TIME_CRITICAL,0L,CREATE_SUSPENDED);

	// imposta l'evento per la sincronizzazione (per lo stato corrente)
	if(!m_pPlayThread) 
	{
		// lancio del thread fallito, imposta l'evento
		TRACEEXPR((_TRACE_FLAG_ERR,__NOFILE__,__NOLINE__,"CAudioMP3::Play(): unable to start player thread\n"));
		::SetEvent(m_playerThreadParams.hEvent);	
		
		// imposta lo status
		SetStatus(mmAudioPmClosed);

		return(MMAERR_PLAYER_INTERNAL);
	}
	else
	{
		// imposta evento e thread
		::ResetEvent(m_playerThreadParams.hEvent);

		m_pPlayThread->m_bAutoDelete = TRUE;
//		m_pPlayThread->SetAutoDelete(TRUE);
//		m_pPlayThread->SetPriority(m_nThreadPriority);
		
		// avvia il thread
		m_pPlayThread->ResumeThread();
//		m_pPlayThread->Resume();
		
		// imposta lo status corrente
		SetStatus(mmAudioPmPlaying);
		
		return(MMAERR_NOERROR);
	}
}

/*
	Pause()
*/
MMAERROR CAudioMP3::Pause(void)
{
	// mette in pausa solo se in riproduzione
	if(GetStatus()==mmAudioPmPlaying)
	{
		// aspetta che il thread del riproduttore senta la pausa, ossia l'impostazione dell'evento per la sincronizzazione (per lo stato corrente)
		m_playerThreadParams.chrKeyPressed = 'p';
		::WaitForSingleObject(m_playerThreadParams.hEvent,INFINITE);

		// il thread ha sentito l'evento, resetta l'evento per la sincronizzazione (per lo stato corrente)
		::ResetEvent(m_playerThreadParams.hEvent);
		
		// imposta lo status corrente
		SetStatus(mmAudioPmPaused);
		
		m_playerThreadParams.chrKeyPressed = 0;
	}

	return(MMAERR_NOERROR);
}

/*
	Resume()
*/
MMAERROR CAudioMP3::Resume(void)
{
	// riavvia solo se in pausa
	if(GetStatus()==mmAudioPmPaused)
	{
		// aspetta che il thread del riproduttore senta il riavvio, ossia l'impostazione dell'evento per la sincronizzazione (per lo stato corrente)
		m_playerThreadParams.chrKeyPressed = 'P';
		::WaitForSingleObject(m_playerThreadParams.hEvent,INFINITE);

		// il thread ha sentito l'evento, resetta l'evento per la sincronizzazione (per lo stato corrente)
		::ResetEvent(m_playerThreadParams.hEvent);

		// imposta lo status corrente
		SetStatus(mmAudioPmPlaying);

		m_playerThreadParams.chrKeyPressed = 0;
	}
	// se e' stato stoppato riproduce
	else if(GetStatus()==mmAudioPmStopped)
	{
		Play();
	}

	return(MMAERR_NOERROR);
}

/*
	Stop()
*/
MMAERROR CAudioMP3::Stop(void)
{
	// stoppa solo se non lo e' gia' stato
	if(GetStatus()!=mmAudioPmStopped)
	{
		if(GetStatus()==mmAudioPmPlaying || GetStatus()==mmAudioPmPaused)
		{
			// aspetta che il thread del riproduttore senta lo stop, ossia l'impostazione dell'evento per la sincronizzazione (per lo stato corrente)
			m_playerThreadParams.chrKeyPressed = 'q';
			::WaitForSingleObject(m_playerThreadParams.hEvent,INFINITE);
			
			// il thread ha sentito l'evento, resetta l'evento per la sincronizzazione (per lo stato corrente)
			::ResetEvent(m_playerThreadParams.hEvent);
			m_playerThreadParams.chrKeyPressed = 0;
		}

		// imposta lo status corrente
		SetStatus(mmAudioPmStopped);
	}

	return(MMAERR_NOERROR);
}

/*
	Close()
*/
MMAERROR CAudioMP3::Close(void)
{
	if(GetStatus()!=mmAudioPmClosed)
	{
		if(GetStatus()==mmAudioPmPlaying || GetStatus()==mmAudioPmPaused)
		{
			// aspetta che il thread del riproduttore senta la chiusura, ossia l'impostazione dell'evento per la sincronizzazione (per lo stato corrente)
			m_playerThreadParams.chrKeyPressed = 'q';
			::WaitForSingleObject(m_playerThreadParams.hEvent,INFINITE);
			
			// il thread ha sentito l'evento, resetta l'evento per la sincronizzazione (per lo stato corrente)
			::ResetEvent(m_playerThreadParams.hEvent);
			m_playerThreadParams.chrKeyPressed = 0;
		}

		// imposta lo status corrente
		SetStatus(mmAudioPmClosed);
	}

	return(MMAERR_NOERROR);
}

/*
	Seek()

	Avanza o retrocede la riproduzione di <n> secondi (es. +10, -10).
*/
MMAERROR CAudioMP3::Seek(long lSeconds)
{
	// riposiziona solo se in riproduzione
	if(GetStatus()==mmAudioPmPlaying)
	{
		// passa la durata totale ed i secondi di salto
		m_playerThreadParams.dwTotalSeconds = (DWORD)m_Mp3Info.GetLength();
		m_playerThreadParams.nSeekSeconds = lSeconds;

		// aspetta che il thread del riproduttore senta la seek, ossia l'impostazione dell'evento per la sincronizzazione (per lo stato corrente)
		m_playerThreadParams.chrKeyPressed = 'S';
		::WaitForSingleObject(m_playerThreadParams.hEvent,INFINITE);

		// il thread ha sentito l'evento, resetta l'evento per la sincronizzazione (per lo stato corrente)
		::ResetEvent(m_playerThreadParams.hEvent);
		
		// imposta lo status corrente
		SetStatus(mmAudioPmPlaying);
		
		m_playerThreadParams.chrKeyPressed = 0;
		m_playerThreadParams.nSeekSeconds = 0;
	}

	return(MMAERR_NOERROR);
}

/*
	GetLength()
*/
QWORD CAudioMP3::GetLength(long& lMinutes,long& lSeconds)
{
	long lLength = m_Mp3Info.GetLength();
	lMinutes = lSeconds = 0L;

	if(lLength > 0L)
	{
		lMinutes = lLength / 60L;
		lSeconds = lLength % 60L;
	}

	return(m_Mp3Info.GetFileSize());
}
