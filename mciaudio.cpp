/*
	mciaudio.cpp
	Interfaccia con l'api MCI per i vari formati audio.
	Le classi definite sotto sono quelle che vengono utilizzate dai players relativi (CAudioWav e CAudioCDA).
	Luca Piergentili, 14/07/03
	lpiergentili@yahoo.com

	Parte del codice e' stata derivata (modificata e riadattata) da esempi MS:
	
	ACMAPP:
	This is a sample application that demonstrates how to use the Audio Compression
	Manager API's in Windows. This application is also useful as an ACM driver test.
	WARNING: There are known bugs with MCI Wave EDITING operations on non-PCM formats.
	THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
	EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED WARRANTIES
	OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
	Copyright (C) 1992 - 1997 Microsoft Corporation.  All Rights Reserved.

	CDPLAYER: Accesses Multimedia Devices Using MCI
	Note: The source files for this sample application may be incomplete. These files were
	acquired directly from the Microsoft(R) Systems Journal. We have decided to include the
	source files in the Microsoft Development Library in untested, unmodified form
	[...]
	CDPLAYER originally appeared in the November 24, 1992, issue of PC Magazine. Version 2.0
	[...]
	CDPLAYER is a companion application for "Simplify Access to Complex Multimedia Devices with
	the Media Control Interface" by Jeff Prosise (Microsoft Systems Journal, Vol. 9, No. 4).
*/
#include "pragma.h"
#include "env.h"
#include "macro.h"
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include "strings.h"
#include "window.h"
#include "muldiv32.h"
#include <mmsystem.h>
#include <mmreg.h>
#include <msacm.h>
#include "mmaudio.h"
#include "waveio.h"
#include "mciaudio.h"

#include "traceexpr.h"
//#define _TRACE_FLAG			_TRFLAG_TRACEOUTPUT // opzioni: _TRFLAG_NOTRACE, _TRFLAG_TRACEFILE, _TRFLAG_TRACECONSOLE, _TRFLAG_TRACEOUTPUT, _TRFLAG_TRACEBREAKPOINT
#define _TRACE_FLAG			_TRFLAG_NOTRACE // opzioni: _TRFLAG_NOTRACE, _TRFLAG_TRACEFILE, _TRFLAG_TRACECONSOLE, _TRFLAG_TRACEOUTPUT, _TRFLAG_TRACEBREAKPOINT
#define _TRACE_FLAG_INFO	_TRACE_FLAG
#define _TRACE_FLAG_WARN	_TRACE_FLAG
#define _TRACE_FLAG_ERR		_TRACE_FLAG

// statiche per la classe (comuni a tutte le istanze della classe)
// il contatore per le referenze viene usato per sapere quando aprire/chiudere il driver MCI (CD Audio)
int CMCIAudioCDA::m_nRefCount = 0;
int CMCIAudioCDA::m_bInitialized = FALSE;
CCDATrackInfoList* CMCIAudioCDA::m_plistCDATrackInfo = NULL;

/*
	CMCIAudioWave()
*/
CMCIAudioWave::CMCIAudioWave(PMCIFILEINFO pMcifi)
{
	// inizializza soltanto
	m_pMcifi = NULL;
	m_nTimerId = 0L;
	m_nMCIDeviceId = (MCIDEVICEID)-1;
	_snprintf(m_szMCIDeviceAlias,sizeof(m_szMCIDeviceAlias),"MyMCIDeviceAlias%ld",::GetTickCount());
	memset(m_szMCIDeviceStatus,'\0',sizeof(m_szMCIDeviceStatus));	
	m_pMcifi = pMcifi;
	m_bInitialized = (m_pMcifi!=(PMCIFILEINFO)NULL);
	if(m_bInitialized)
	{
		memset(m_pMcifi,'\0',sizeof(MCIFILEINFO));
		m_pMcifi->mmaPlayerStatus = mmAudioPmClosed;
	}
}

/*
	~CMCIAudioWave()
*/
CMCIAudioWave::~CMCIAudioWave()
{
	m_bInitialized = FALSE;

	// rilascia la struttura allocata
	if(m_pMcifi)
	{
		if(m_pMcifi->pwfx)
		{
			GlobalFreePtr(m_pMcifi->pwfx);
			m_pMcifi->pwfx = NULL;
		}
		memset(m_pMcifi,'\0',sizeof(MCIFILEINFO));
		m_pMcifi->mmaPlayerStatus = mmAudioPmClosed;
		m_pMcifi = NULL;
	}

	// elimina il timer
	if(m_nTimerId!=0)
	{
		::KillTimer(AfxGetMainWnd()->m_hWnd,m_nTimerId);
		m_nTimerId = 0;
	}
}

/*
	Load()
*/
MCIERROR CMCIAudioWave::Load(LPCSTR lpcszWavFileName)
{
	// controlla l'avvenuta inizializzazione
	if(!m_bInitialized)
	{
		TRACEEXPR((_TRACE_FLAG_ERR,__NOFILE__,__NOLINE__,"CMCIAudioWave::Load(): MCIERR_DEVICE_NOT_READY\n"));
		return(MCIERR_DEVICE_NOT_READY);
	}

	// inizializza i membri di pertinenza
	strcpyn(m_pMcifi->szFilePath,lpcszWavFileName,_MAX_PATH+1);
	char* p = strrchr(m_pMcifi->szFilePath,'\\');
	if(p)
		p++;
	if(!p)
		p = m_pMcifi->szFilePath;
	strcpyn(m_pMcifi->szFileTitle,p,_MAX_PATH+1);
	p = stristr(m_pMcifi->szFileTitle,WAV_EXTENSION);
	if(p)
		*p = '\0';
	m_pMcifi->qwFileSize		= 0L;
	m_pMcifi->uDosChangeDate	= 0;
	m_pMcifi->uDosChangeTime	= 0;
	m_pMcifi->dwFileAttributes	= 0L;
	strcpy(m_pMcifi->szFormatTag,"unknown");
	m_pMcifi->dwDataBytes		= 0L;
	m_pMcifi->dwDataSamples		= 0L;
	m_pMcifi->dlTotalLength		= 0.0f;
	m_pMcifi->lMinutes			= 0L;
	m_pMcifi->lSeconds			= 0L;
	if(m_pMcifi->pwfx)
	{
		GlobalFreePtr(m_pMcifi->pwfx);
		m_pMcifi->pwfx = NULL;
		m_pMcifi->cbwfx = 0;
	}

	// apre il file per ricavare le informazioni necessarie (dimensione, attributi, data/ora, etc.)
	HANDLE hWaveFile = ::CreateFile(m_pMcifi->szFilePath,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,0);
	if(hWaveFile!=INVALID_HANDLE_VALUE)
	{
		BY_HANDLE_FILE_INFORMATION bhfi = {0};
		if(::GetFileInformationByHandle(hWaveFile,&bhfi))
		{
			LARGE_INTEGER li = {0};
			m_pMcifi->dwFileAttributes = bhfi.dwFileAttributes;
			li.LowPart = bhfi.nFileSizeLow;
			li.HighPart = bhfi.nFileSizeHigh;
			m_pMcifi->qwFileSize = li.QuadPart;
			WORD wDosChangeDate;
			WORD wDosChangeTime;
			::FileTimeToDosDateTime(&bhfi.ftLastWriteTime,&wDosChangeDate,&wDosChangeTime);
			m_pMcifi->uDosChangeDate = (UINT)wDosChangeDate;
			m_pMcifi->uDosChangeTime = (UINT)wDosChangeTime;
		}

		::CloseHandle(hWaveFile);
	}
	else
	{
		TRACEEXPR((_TRACE_FLAG_ERR,__NOFILE__,__NOLINE__,"CMCIAudioWave::Load(): error opening %s\n",m_pMcifi->szFilePath));
		return(MCIERR_FILE_NOT_FOUND);
	}

	// apre il file (come wave) per ricavare le informazioni necessarie
	WAVEIOCB wio = {0};
	WIOERR werr = ::wioFileOpen(&wio,m_pMcifi->szFilePath,0L);
	if(werr==WIOERR_NOERROR)
	{
		UINT cbwfx = SIZEOF_WAVEFORMATEX(wio.pwfx);
		m_pMcifi->pwfx = (LPWAVEFORMATEX)GlobalAllocPtr(GHND,cbwfx);
		if(m_pMcifi->pwfx)
		{
			memcpy(m_pMcifi->pwfx,wio.pwfx,cbwfx);
			m_pMcifi->cbwfx = cbwfx;
			m_pMcifi->dwDataBytes = wio.dwDataBytes;
			m_pMcifi->dwDataSamples = wio.dwDataSamples;
			char szBuf[32];
			_snprintf(szBuf,sizeof(szBuf)-1,"%lu.%.03lu",m_pMcifi->dwDataBytes / m_pMcifi->pwfx->nAvgBytesPerSec,((m_pMcifi->dwDataBytes % m_pMcifi->pwfx->nAvgBytesPerSec) * 1000) / m_pMcifi->pwfx->nAvgBytesPerSec);
			m_pMcifi->dlTotalLength = atof(szBuf);
			m_pMcifi->lMinutes = (long)m_pMcifi->dlTotalLength / 60L;
			m_pMcifi->lSeconds = (long)m_pMcifi->dlTotalLength % 60L;
			::wioFileClose(&wio);
		}
	}
	else
	{
		TRACEEXPR((_TRACE_FLAG_ERR,__NOFILE__,__NOLINE__,"CMCIAudioWave::Load(): error loading %s\n",m_pMcifi->szFilePath));
		return(MCIERR_INVALID_FILE);
	}

	// ricava il formato (wave)
	ACMFORMATTAGDETAILS aftd = {0};
	aftd.cbStruct = sizeof(ACMFORMATTAGDETAILS);
	aftd.dwFormatTag = m_pMcifi->pwfx->wFormatTag;
	MMRESULT mmr = ::acmFormatTagDetails(NULL,&aftd,ACM_FORMATTAGDETAILSF_FORMATTAG);
	if(mmr==MMSYSERR_NOERROR)
	{
		strcpyn(m_pMcifi->szFormatTag,aftd.szFormatTag,MCIAUDIO_MAX_WAV_FORMAT_TAG+1);
	}
	else
	{
		switch(mmr)
		{
			case ACMERR_NOTPOSSIBLE:
				strcpyn(m_pMcifi->szFormatTag,"ACMERR_NOTPOSSIBLE",MCIAUDIO_MAX_WAV_FORMAT_TAG+1);
				break;
			case MMSYSERR_INVALFLAG:
				strcpyn(m_pMcifi->szFormatTag,"MMSYSERR_INVALFLAG",MCIAUDIO_MAX_WAV_FORMAT_TAG+1);
				break;
			case MMSYSERR_INVALHANDLE:
				strcpyn(m_pMcifi->szFormatTag,"MMSYSERR_INVALHANDLE",MCIAUDIO_MAX_WAV_FORMAT_TAG+1);
				break;
			case MMSYSERR_INVALPARAM:
				strcpyn(m_pMcifi->szFormatTag,"MMSYSERR_INVALPARAM",MCIAUDIO_MAX_WAV_FORMAT_TAG+1);
				break;
			default:
				strcpyn(m_pMcifi->szFormatTag,"UNKNOWN_ACM_ERROR",MCIAUDIO_MAX_WAV_FORMAT_TAG+1);
				break;
		}
		mmr = MMSYSERR_NOERROR;
		TRACEEXPR((_TRACE_FLAG_ERR,__NOFILE__,__NOLINE__,"CMCIAudioWave::Load(): error getting wave format %s\n",m_pMcifi->szFilePath));
	}
	
	return(mmr);
}

/*
	Open()
*/
MCIERROR CMCIAudioWave::Open(LPCSTR lpcszWavFileName/*= NULL*/,UINT uWaveInId/*= WAVE_MAPPER*/,UINT uWaveOutId/*= WAVE_MAPPER*/)
{
	// controlla l'avvenuta inizializzazione
	MCIERROR mcierr = MCIERR_DEVICE_NOT_READY;
	if(!m_bInitialized)
	{
		TRACEEXPR((_TRACE_FLAG_ERR,__NOFILE__,__NOLINE__,"CMCIAudioWave::Open(): MCIERR_DEVICE_NOT_READY\n"));
		return(MCIERR_DEVICE_NOT_READY);
	}

	// se viene specificato un nome file lo carica, in caso contrario bisogna chiamare la Load() autonomamente e anteriormente
	if(lpcszWavFileName)
		if((mcierr = Load(lpcszWavFileName))!=MMSYSERR_NOERROR)
			return(mcierr);

	// apre il driver MCI (stringa)
	char szMCICommand[MCIAUDIO_MAX_COMMAND_CHARS+1];
	_snprintf(szMCICommand,sizeof(szMCICommand)-1,"open \"%s\" type waveaudio alias %s",m_pMcifi->szFilePath,m_szMCIDeviceAlias);
	TRACEEXPR((_TRACE_FLAG_INFO,__NOFILE__,__NOLINE__,"CMCIAudioWave::Open() -> [%s]\n",szMCICommand));
	mcierr = ::mciSendString(szMCICommand,szMCICommand,sizeof(szMCICommand),NULL);
	TRACEEXPR((_TRACE_FLAG_INFO,__NOFILE__,__NOLINE__,"CMCIAudioWave::Open() <- [%ld,%s]\n",mcierr,szMCICommand));
	if(mcierr!=MMSYSERR_NOERROR)
		return(mcierr);

	// ricava l'id del device MCI in base all'alias ricavato sopra
	m_nMCIDeviceId = ::mciGetDeviceID(m_szMCIDeviceAlias);

	// imposta per il driver MCI
	if((mcierr = Send("set time format samples",NULL,0))!=MMSYSERR_NOERROR)
	{
		TRACEEXPR((_TRACE_FLAG_ERR,__NOFILE__,__NOLINE__,"CMCIAudioWave::Open(): error %ld setting time format sample\n",mcierr));
		return(mcierr);
	}
	if(uWaveInId!=WAVE_MAPPER)
	{
		_snprintf(szMCICommand,sizeof(szMCICommand)-1,"set input %u",uWaveInId);
		if((mcierr = Send(szMCICommand,NULL,0))!=MMSYSERR_NOERROR)
		{
			TRACEEXPR((_TRACE_FLAG_ERR,__NOFILE__,__NOLINE__,"CMCIAudioWave::Open(): error %ld setting input\n",mcierr));
			return(mcierr);
		}
	}
	if(uWaveOutId!=WAVE_MAPPER)
	{
		_snprintf(szMCICommand,sizeof(szMCICommand)-1,"set output %u",uWaveOutId);
		if((mcierr = Send(szMCICommand,NULL,0))!=MMSYSERR_NOERROR)
		{
			TRACEEXPR((_TRACE_FLAG_ERR,__NOFILE__,__NOLINE__,"CMCIAudioWave::Open(): error %ld setting output\n",mcierr));
			return(mcierr);
		}
	}

	// crea il timer per interrogare il driver MCI sullo stato corrente
	if(m_nTimerId!=0)
	{
		::KillTimer(AfxGetMainWnd()->m_hWnd,m_nTimerId);
		m_nTimerId = 0;
	}
	m_nTimerId = ::SetTimer(AfxGetMainWnd()->m_hWnd,(UINT)this,MCIAUDIO_TIMER_RESOLUTION,(TIMERPROC)WaveTimerProc);

	m_pMcifi->mmaPlayerStatus = mmAudioPmReady;

	return(MMSYSERR_NOERROR);
}

/*
	Play()
*/
MCIERROR CMCIAudioWave::Play(void)
{
	// controlla l'avvenuta inizializzazione
	MCIERROR mcierr = MCIERR_DEVICE_NOT_READY;
	if(!m_bInitialized)
	{
		TRACEEXPR((_TRACE_FLAG_ERR,__NOFILE__,__NOLINE__,"CMCIAudioWave::Play(): MCIERR_DEVICE_NOT_READY\n"));
		return(MCIERR_DEVICE_NOT_READY);
	}

	// ricava lunghezza e posizione corrente
	char szPosition[32];
	char szLength[32];
	if((mcierr = Send("status position",szPosition,sizeof(szPosition)))!=MMSYSERR_NOERROR)
	{
		TRACEEXPR((_TRACE_FLAG_ERR,__NOFILE__,__NOLINE__,"CMCIAudioWave::Play(): error %ld status position\n",mcierr));
		return(mcierr);
	}
	if((mcierr = Send("status length",szLength,sizeof(szLength)))!=MMSYSERR_NOERROR)
	{
		TRACEEXPR((_TRACE_FLAG_ERR,__NOFILE__,__NOLINE__,"CMCIAudioWave::Play(): error %ld status length\n",mcierr));
		return(mcierr);
	}

	// posiziona nel file
	if(strcmp(szPosition,szLength)==0)
		Send("seek to start",NULL,0);

	// riproduce
	if((mcierr = Send("play",NULL,0))==MMSYSERR_NOERROR)
	{
		m_pMcifi->mmaPlayerStatus = mmAudioPmPlaying;
		TRACEEXPR((_TRACE_FLAG_INFO,__NOFILE__,__NOLINE__,"CMCIAudioWave::Play(): playing\n"));
	}
	else
	{
		TRACEEXPR((_TRACE_FLAG_ERR,__NOFILE__,__NOLINE__,"CMCIAudioWave::Play(): failed\n"));
	}
	
	return(mcierr);
}

/*
	Pause()
*/
MCIERROR CMCIAudioWave::Pause(void)
{
	// controlla l'avvenuta inizializzazione
	MCIERROR mcierr = MCIERR_DEVICE_NOT_READY;
	if(!m_bInitialized)
	{
		TRACEEXPR((_TRACE_FLAG_ERR,__NOFILE__,__NOLINE__,"CMCIAudioWave::Pause(): MCIERR_DEVICE_NOT_READY\n"));
		return(MCIERR_DEVICE_NOT_READY);
	}

	// mette in pausa
	if((mcierr = Send("pause",NULL,0))==MMSYSERR_NOERROR)
	{
		m_pMcifi->mmaPlayerStatus = mmAudioPmPaused;
		TRACEEXPR((_TRACE_FLAG_INFO,__NOFILE__,__NOLINE__,"CMCIAudioWave::Pause(): paused\n"));
	}
	else
	{
		TRACEEXPR((_TRACE_FLAG_ERR,__NOFILE__,__NOLINE__,"CMCIAudioWave::Pause(): failed\n"));
	}
	
	return(mcierr);
}

/*
	Resume()
*/
MCIERROR CMCIAudioWave::Resume(void)
{
	// controlla l'avvenuta inizializzazione
	MCIERROR mcierr = MCIERR_DEVICE_NOT_READY;
	if(!m_bInitialized)
	{
		TRACEEXPR((_TRACE_FLAG_ERR,__NOFILE__,__NOLINE__,"CMCIAudioWave::Resume(): MCIERR_DEVICE_NOT_READY\n"));
		return(MCIERR_DEVICE_NOT_READY);
	}

	// riprende
	if((mcierr = Send("resume",NULL,0))==MMSYSERR_NOERROR)
	{
		m_pMcifi->mmaPlayerStatus = mmAudioPmPlaying;
		TRACEEXPR((_TRACE_FLAG_INFO,__NOFILE__,__NOLINE__,"CMCIAudioWave::Resume(): resumed\n"));
	}
	else
	{
		TRACEEXPR((_TRACE_FLAG_ERR,__NOFILE__,__NOLINE__,"CMCIAudioWave::Resume(): failed\n"));
	}
	
	return(mcierr);
}

/*
	Stop()
*/
MCIERROR CMCIAudioWave::Stop(void)
{
	// controlla l'avvenuta inizializzazione
	MCIERROR mcierr = MCIERR_DEVICE_NOT_READY;
	if(!m_bInitialized)
	{
		TRACEEXPR((_TRACE_FLAG_ERR,__NOFILE__,__NOLINE__,"CMCIAudioWave::Stop(): MCIERR_DEVICE_NOT_READY\n"));
		return(MCIERR_DEVICE_NOT_READY);
	}

	// ferma la riproduzione
	if((mcierr = Send("stop",NULL,0))==MMSYSERR_NOERROR)
	{
		m_pMcifi->mmaPlayerStatus = mmAudioPmStopped;
		TRACEEXPR((_TRACE_FLAG_INFO,__NOFILE__,__NOLINE__,"CMCIAudioWave::Stop(): stopped\n"));
	}
	else
	{
		TRACEEXPR((_TRACE_FLAG_ERR,__NOFILE__,__NOLINE__,"CMCIAudioWave::Stop(): failed\n"));
	}
	
	return(mcierr);
}

/*
	Close()
*/
MCIERROR CMCIAudioWave::Close(void)
{
	// controlla l'avvenuta inizializzazione
	MCIERROR mcierr = MCIERR_DEVICE_NOT_READY;
	if(!m_bInitialized)
	{
		TRACEEXPR((_TRACE_FLAG_ERR,__NOFILE__,__NOLINE__,"CMCIAudioWave::Close(): MCIERR_DEVICE_NOT_READY\n"));
		return(MCIERR_DEVICE_NOT_READY);
	}

	// chiude il driver MCI
	if((mcierr = Send("close",NULL,0))==MMSYSERR_NOERROR)
	{
		m_pMcifi->mmaPlayerStatus = mmAudioPmClosed;
		TRACEEXPR((_TRACE_FLAG_INFO,__NOFILE__,__NOLINE__,"CMCIAudioWave::Close(): closed\n"));
	}
	else
	{
		TRACEEXPR((_TRACE_FLAG_ERR,__NOFILE__,__NOLINE__,"CMCIAudioWave::Close(): failed\n"));
	}
	
	// elimina il timer
	if(m_nTimerId!=0)
	{
		::KillTimer(AfxGetMainWnd()->m_hWnd,m_nTimerId);
		m_nTimerId = 0;
	}

	return(mcierr);
}

/*
	SetPos()
*/
MCIERROR CMCIAudioWave::SetPos(UINT uCode,int nPos,int /*nMinPos*/,int nMaxPos)
{
	// controlla l'avvenuta inizializzazione
	MCIERROR mcierr = MCIERR_DEVICE_NOT_READY;
	if(!m_bInitialized)
	{
		TRACEEXPR((_TRACE_FLAG_ERR,__NOFILE__,__NOLINE__,"CMCIAudioWave::SetPos(): MCIERR_DEVICE_NOT_READY\n"));
		return(MCIERR_DEVICE_NOT_READY);
	}

	char szPosition[32];
	char szLength[32];
	LONG lLength;
	LONG lPosition;
	LONG lPageInc;

	if(m_pMcifi->mmaPlayerStatus==mmAudioPmClosed)
	{
		TRACEEXPR((_TRACE_FLAG_ERR,__NOFILE__,__NOLINE__,"CMCIAudioWave::SetPos(): error invalid status\n"));
		return(MMSYSERR_INVALPARAM);
	}

	mcierr = Send("status length",szLength,sizeof(szLength));
	if(mcierr!=MMSYSERR_NOERROR)
	{
		TRACEEXPR((_TRACE_FLAG_ERR,__NOFILE__,__NOLINE__,"CMCIAudioWave::SetPos(): error %ld status length\n",mcierr));
		return(mcierr);
	}

	lLength = strtol(szLength,NULL,10);
	if(lLength==0L)
	{
		TRACEEXPR((_TRACE_FLAG_ERR,__NOFILE__,__NOLINE__,"CMCIAudioWave::SetPos(): error status length\n"));
		return(MCIERR_DEVICE_LENGTH);
	}

	mcierr = Send("status position",szPosition,sizeof(szPosition));
	if(mcierr!=MMSYSERR_NOERROR)
	{
		TRACEEXPR((_TRACE_FLAG_ERR,__NOFILE__,__NOLINE__,"CMCIAudioWave::SetPos(): error %ld status position\n",mcierr));
		return(mcierr);
	}

	lPosition = strtol(szPosition,NULL,10);

	lPageInc = (lLength / 10);
	if(lPageInc==0L)
		lPageInc = 1;

	// posiziona
	switch(uCode)
	{
		case SB_PAGEDOWN:
			lPosition = min(lLength,lPosition + lPageInc);
			break;

		case SB_LINEDOWN:
			lPosition = min(lLength,lPosition + 1);
			break;

		case SB_PAGEUP:
			lPosition -= lPageInc;

		case SB_LINEUP:
			lPosition = (lPosition < 1) ? 0 : (lPosition - 1);
			break;

		case SB_TOP:
			lPosition = 0;
			break;

		case SB_BOTTOM:
			lPosition = lLength;
			break;

		case SB_THUMBPOSITION:
		case SB_THUMBTRACK:
			lPosition = (DWORD)MulDivRN((DWORD)nPos,(DWORD)lLength,(DWORD)nMaxPos);
			break;

		default:
			TRACEEXPR((_TRACE_FLAG_ERR,__NOFILE__,__NOLINE__,"CMCIAudioWave::SetPos(): invalid set pos param\n",mcierr));
			return(MMSYSERR_INVALPARAM);
	}

	_snprintf(szPosition,sizeof(szPosition)-1,"seek to %lu",lPosition);
	if((mcierr = Send(szPosition,NULL,0))!=MMSYSERR_NOERROR)
	{
		TRACEEXPR((_TRACE_FLAG_ERR,__NOFILE__,__NOLINE__,"CMCIAudioWave::SetPos(): error %ld seeking\n",mcierr));
	}

	return(mcierr);
}

/*
	Start()
*/
MCIERROR CMCIAudioWave::Start(void)
{
	// controlla l'avvenuta inizializzazione
	MCIERROR mcierr = MCIERR_DEVICE_NOT_READY;
	if(!m_bInitialized)
	{
		TRACEEXPR((_TRACE_FLAG_ERR,__NOFILE__,__NOLINE__,"CMCIAudioWave::Start(): MCIERR_DEVICE_NOT_READY\n"));
		return(MCIERR_DEVICE_NOT_READY);
	}

	// posiziona
	if((mcierr = Send("seek to start",NULL,0))!=MMSYSERR_NOERROR)
	{
		TRACEEXPR((_TRACE_FLAG_ERR,__NOFILE__,__NOLINE__,"CMCIAudioWave::Start(): error %ld\n",mcierr));
	}

	return(mcierr);
}

/*
	End()
*/
MCIERROR CMCIAudioWave::End(void)
{
	// controlla l'avvenuta inizializzazione
	MCIERROR mcierr = MCIERR_DEVICE_NOT_READY;
	if(!m_bInitialized)
	{
		TRACEEXPR((_TRACE_FLAG_ERR,__NOFILE__,__NOLINE__,"CMCIAudioWave::End(): MCIERR_DEVICE_NOT_READY\n"));
		return(MCIERR_DEVICE_NOT_READY);
	}

	// posiziona
	if((mcierr = Send("seek to end",NULL,0))!=MMSYSERR_NOERROR)
	{
		TRACEEXPR((_TRACE_FLAG_ERR,__NOFILE__,__NOLINE__,"CMCIAudioWave::End(): error %ld\n",mcierr));
	}

	return(mcierr);
}

/*
	Send()
*/
MCIERROR CMCIAudioWave::Send(LPCSTR lpcszCommand,LPSTR lpszReturn,UINT cbReturn)
{
	// controlla l'avvenuta inizializzazione
	MCIERROR mcierr = MCIERR_DEVICE_NOT_READY;
	if(!m_bInitialized)
	{
		TRACEEXPR((_TRACE_FLAG_ERR,__NOFILE__,__NOLINE__,"CMCIAudioWave::Send(): MCIERR_DEVICE_NOT_READY\n"));
		return(MCIERR_DEVICE_NOT_READY);
	}

	char* pch;
	char* psz;
	char szMCICommand[(MCIAUDIO_MAX_COMMAND_CHARS*2)+1];

	pch = (LPSTR)lpcszCommand;
	while(('\t'==*pch) || (' '==*pch))
		pch++;
	
	if(strlen(pch)==0)
		return(MMSYSERR_NOERROR);

	pch = szMCICommand;
	psz = (char*)lpcszCommand;
	while(('\0'!=*psz) && (' '!=*psz))
		*pch++ = *psz++;
	*pch++ = ' ';

	strcpy(pch,m_szMCIDeviceAlias);
	strcat(pch,psz);

	// invia il comando
	TRACEEXPR((_TRACE_FLAG_INFO,__NOFILE__,__NOLINE__,"CMCIAudioWave::Send() -> [%s]\n",szMCICommand));
	mcierr = ::mciSendString(szMCICommand,lpszReturn,cbReturn,NULL);
	TRACEEXPR((_TRACE_FLAG_INFO,__NOFILE__,__NOLINE__,"CMCIAudioWave::Send() <- [%ld,%s]\n",mcierr,lpszReturn));
	if(mcierr!=MMSYSERR_NOERROR)
	{
		char szErr[MCIAUDIO_MAX_COMMAND_CHARS+1];
		::mciGetErrorString(mcierr,szErr,sizeof(szErr));
		TRACEEXPR((_TRACE_FLAG_ERR,__NOFILE__,__NOLINE__,"CMCIAudioWave::Send() - error [%ld,%s]\n",mcierr,szErr));
	}

	return(mcierr);
}

/*
	GetStatus()
*/
LPCSTR CMCIAudioWave::GetStatus(void)
{
	static char szStatus[MCIAUDIO_MAX_COMMAND_CHARS+1];
	strcpyn(szStatus,m_szMCIDeviceStatus,sizeof(szStatus));
	TRACEEXPR((_TRACE_FLAG_INFO,__NOFILE__,__NOLINE__,"CMCIAudioWave::GetStatus() [%s]\n",szStatus));
	return(szStatus);
}

/*
	WaveTimerProc()
*/
VOID CALLBACK CMCIAudioWave::WaveTimerProc(HWND /*hwnd*/,UINT /*uMsg*/,UINT_PTR idEvent,DWORD /*dwTime*/)
{
	// callback (su timer) utilizzata per ricavare lo status corrente del driver MCI
	CMCIAudioWave* pAudioMCI = (CMCIAudioWave*)idEvent;
	
	if(pAudioMCI)
	{
		// ricava lo status corrente
		MCI_STATUS_PARMS mci_status_parms;
		mci_status_parms.dwCallback = NULL;
		mci_status_parms.dwReturn = 0;
		mci_status_parms.dwItem = MCI_STATUS_MODE;
		mci_status_parms.dwTrack = 0;

		if(::mciSendCommand((pAudioMCI->m_nMCIDeviceId==(MCIDEVICEID)-1 ? 0 : pAudioMCI->m_nMCIDeviceId),MCI_STATUS,MCI_WAIT|MCI_STATUS_ITEM,(DWORD)(LPMCI_STATUS_PARMS)&mci_status_parms)==MMSYSERR_NOERROR)
		{
			if(mci_status_parms.dwReturn==MCI_MODE_NOT_READY)
			{
				pAudioMCI->m_pMcifi->mmaPlayerStatus = mmAudioPmClosed;
				TRACEEXPR((_TRACE_FLAG_INFO,__NOFILE__,__NOLINE__,"CMCIAudioWave::WaveTimerProc() MCI_MODE_NOT_READY\n"));
			}
			else if(mci_status_parms.dwReturn==MCI_MODE_PAUSE)
			{
				pAudioMCI->m_pMcifi->mmaPlayerStatus = mmAudioPmPaused;
				TRACEEXPR((_TRACE_FLAG_INFO,__NOFILE__,__NOLINE__,"CMCIAudioWave::WaveTimerProc() MCI_MODE_PAUSE\n"));
			}
			else if(mci_status_parms.dwReturn==MCI_MODE_PLAY)
			{
				pAudioMCI->m_pMcifi->mmaPlayerStatus = mmAudioPmPlaying;
				TRACEEXPR((_TRACE_FLAG_INFO,__NOFILE__,__NOLINE__,"CMCIAudioWave::WaveTimerProc() MCI_MODE_PLAY\n"));
			}
			else if(mci_status_parms.dwReturn==MCI_MODE_STOP)
			{
				if(pAudioMCI->m_pMcifi->mmaPlayerStatus==mmAudioPmPlaying || pAudioMCI->m_pMcifi->mmaPlayerStatus==mmAudioPmDone)
				{
					pAudioMCI->m_pMcifi->mmaPlayerStatus = mmAudioPmDone;
					TRACEEXPR((_TRACE_FLAG_INFO,__NOFILE__,__NOLINE__,"CMCIAudioWave::WaveTimerProc() mmAudioPmDone\n"));
				}
				else
				{
					pAudioMCI->m_pMcifi->mmaPlayerStatus = mmAudioPmStopped;
					TRACEEXPR((_TRACE_FLAG_INFO,__NOFILE__,__NOLINE__,"CMCIAudioWave::WaveTimerProc() mmAudioPmStopped\n"));
				}
			}
		}
		else
		{
			pAudioMCI->m_pMcifi->mmaPlayerStatus = mmAudioPmUndefined;
		}
	}
}

BEGIN_MESSAGE_MAP(CMCIAudioCDA,CWnd)
	ON_WM_DESTROY()
	ON_MESSAGE(MM_MCINOTIFY,OnMciMessage)
END_MESSAGE_MAP()

/*
	CMCIAudioCDA()
*/
CMCIAudioCDA::CMCIAudioCDA()
{
	m_mmaPlayerStatus = mmAudioPmClosed;
	m_nTrackNumber = 0;
	memset(m_szTrackNumber,'\0',sizeof(m_szTrackNumber));
	m_nTimerId = 0L;
	m_hWndTimer = (HWND)NULL;
	m_nMCIDeviceId = (MCIDEVICEID)-1;

	MCI_OPEN_PARMS stMCIOpenParms = {0};
	MCI_SET_PARMS stMCISetParms = {0};

	TRACEEXPR((_TRACE_FLAG_INFO,__NOFILE__,__NOLINE__,"CMCIAudioCDA::CMCIAudioCDA(): ref. counter: %d\n",m_nRefCount));

	// l'apertura del driver MCI viene effettuata solo alla prima chiamata (l'istanza del driver viene condivisa da tutte le istanze della classe)
	if(m_nRefCount++==0)
	{
		MCIERROR mcierr = MMSYSERR_NOERROR;
		
		// crea la finestra per la ricezione dei messaggi di notifica inviati da MCI
		CString strClassName = AfxRegisterWndClass(CS_BYTEALIGNCLIENT|CS_BYTEALIGNWINDOW,0,0,0);
		CString strWndName;
		strWndName.Format("MCIAudioCDAWndName%ld",::GetTickCount());
		CreateEx(0,strClassName,strWndName,0,1,1,1,1,NULL,NULL,NULL);

		// apre il driver MCI in modalita' condivisa)
		stMCIOpenParms.lpstrDeviceType = (LPCSTR)MCI_DEVTYPE_CD_AUDIO;
		if((mcierr = mciSendCommand(NULL,MCI_OPEN,MCI_WAIT|MCI_OPEN_TYPE|MCI_OPEN_TYPE_ID|MCI_OPEN_SHAREABLE,(DWORD)(LPVOID)&stMCIOpenParms))==MMSYSERR_NOERROR)
		{
			m_nMCIDeviceId = stMCIOpenParms.wDeviceID;
			TRACEEXPR((_TRACE_FLAG_INFO,__NOFILE__,__NOLINE__,"CMCIAudioCDA::CMCIAudioCDA(): device id: %ld opened (%d)\n",m_nMCIDeviceId,mcierr));

			stMCISetParms.dwTimeFormat = MCI_FORMAT_TMSF;
			if((mcierr = mciSendCommand(m_nMCIDeviceId,MCI_SET,MCI_WAIT|MCI_SET_TIME_FORMAT,(DWORD)(LPVOID)&stMCISetParms))==MMSYSERR_NOERROR)
				m_bInitialized = TRUE;

			TRACEEXPR((_TRACE_FLAG_INFO,__NOFILE__,__NOLINE__,"CMCIAudioCDA::CMCIAudioCDA(): initialization: %s (%d)\n",m_bInitialized ? "succeed" : "failed",mcierr));
		}
		
		// crea la lista per le info sulle tracce
		m_plistCDATrackInfo = new CCDATrackInfoList();
	}
}

/*
	~CMCIAudioCDA()
*/
CMCIAudioCDA::~CMCIAudioCDA()
{
	TRACEEXPR((_TRACE_FLAG_INFO,__NOFILE__,__NOLINE__,"CMCIAudioCDA::~CMCIAudioCDA(): ref. counter: %d\n",m_nRefCount));

	m_mmaPlayerStatus = mmAudioPmClosed;

	// chiude il driver MCI quando non esistono piu' istanze della classe
	if(--m_nRefCount==0)
	{
		MCIERROR mcierr = MMSYSERR_NOERROR;
		if((mcierr = mciSendCommand(m_nMCIDeviceId,MCI_CLOSE,MCI_WAIT,NULL))==MMSYSERR_NOERROR)
			m_bInitialized = FALSE;

		TRACEEXPR((_TRACE_FLAG_INFO,__NOFILE__,__NOLINE__,"CMCIAudioCDA::~CMCIAudioCDA(): device id: %ld %s (%d)\n",m_nMCIDeviceId,m_bInitialized ? "close failed" : "closed",mcierr));

		if(m_plistCDATrackInfo)
			delete m_plistCDATrackInfo,m_plistCDATrackInfo = NULL;

		// per la derivazione da CWnd
		DestroyWindow();
	}
}

/*
	OnDestroy()
*/
void CMCIAudioCDA::OnDestroy(void)
{
	CWnd::OnDestroy();
}

/*
	OnMciMessage()
*/
LRESULT CMCIAudioCDA::OnMciMessage(WPARAM wParam,LPARAM lParam)
{
	// riceve i messaggi inviati dal driver MCI (imposta lo status corrente)
	if(wParam==MCI_NOTIFY_ABORTED)
	{
		TRACEEXPR((_TRACE_FLAG_ERR,__NOFILE__,__NOLINE__,"CMCIAudioCDA::OnMciMessage(): MCI_NOTIFY_ABORTED\n"));
	}
	else if(wParam==MCI_NOTIFY_FAILURE)
	{
		TRACEEXPR((_TRACE_FLAG_ERR,__NOFILE__,__NOLINE__,"CMCIAudioCDA::OnMciMessage(): MCI_NOTIFY_FAILURE\n"));
	}
	else if(wParam==MCI_NOTIFY_SUCCESSFUL)
	{
		TRACEEXPR((_TRACE_FLAG_INFO,__NOFILE__,__NOLINE__,"CMCIAudioCDA::OnMciMessage(): MCI_NOTIFY_SUCCESSFUL\n"));
		if(m_mmaPlayerStatus==mmAudioPmPlaying || m_mmaPlayerStatus==mmAudioPmDone)
			m_mmaPlayerStatus = mmAudioPmDone;
		else
			m_mmaPlayerStatus = mmAudioPmStopped;
	}
	else if(wParam==MCI_NOTIFY_SUPERSEDED)
	{
		TRACEEXPR((_TRACE_FLAG_INFO,__NOFILE__,__NOLINE__,"CMCIAudioCDA::OnMciMessage(): MCI_NOTIFY_SUPERSEDED\n"));
	}
	else
	{
		TRACEEXPR((_TRACE_FLAG_INFO,__NOFILE__,__NOLINE__,"CMCIAudioCDA::OnMciMessage(): wparam=%ld, lparam=%ld (?)\n",wParam,lParam));
	}

	return(0L);
}

/*
	Open()
*/
MCIERROR CMCIAudioCDA::Open(LPCSTR lpcszCdaFileName)
{
	TRACEEXPR((_TRACE_FLAG_INFO,__NOFILE__,__NOLINE__,"CMCIAudioCDA::Open(): opening %s\n",lpcszCdaFileName));

	// l'apertura del driver MCI viene effettuata nel ctor, vedi le note relative
	if(!m_bInitialized)
	{
		TRACEEXPR((_TRACE_FLAG_ERR,__NOFILE__,__NOLINE__,"CMCIAudioCDA::Open(): MCIERR_DEVICE_NOT_READY\n"));
		return(MCIERR_DEVICE_NOT_READY);
	}

	// ricava il numero di traccia relativa al file (brano) - e' un po' una pecionata ma efficace
	m_nTrackNumber = 0;
	memset(m_szTrackNumber,'\0',sizeof(m_szTrackNumber));
	int i = 0;
	char* p = (char*)lpcszCdaFileName;
	while(*p && i < sizeof(m_szTrackNumber)-1)
	{
		if(isdigit(*p))
			m_szTrackNumber[i++] = *p;
		p++;
	}
	m_nTrackNumber = atoi(m_szTrackNumber);
	if(m_nTrackNumber <= 0 || m_nTrackNumber > 1965)
	{
		TRACEEXPR((_TRACE_FLAG_ERR,__NOFILE__,__NOLINE__,"CMCIAudioCDA::Open(): invalid track #%d (out of range)\n",m_nTrackNumber));
		return(MCIERR_OUTOFRANGE);
	}

	TRACEEXPR((_TRACE_FLAG_INFO,__NOFILE__,__NOLINE__,"CMCIAudioCDA::Open(): opening track #%d\n",m_nTrackNumber));

	m_mmaPlayerStatus = mmAudioPmReady;

	return(MMSYSERR_NOERROR);
}

/*
	Play()
*/
MCIERROR CMCIAudioCDA::Play(void)
{
	MCIERROR mcierr = MCIERR_DEVICE_NOT_READY;

	// l'apertura del driver MCI viene effettuata nel ctor, vedi le note relative
	if(!m_bInitialized)
	{
		TRACEEXPR((_TRACE_FLAG_ERR,__NOFILE__,__NOLINE__,"CMCIAudioCDA::Play(): MCIERR_DEVICE_NOT_READY\n"));
		return(MCIERR_DEVICE_NOT_READY);
	}

	// per ogni riproduzione, controlla se deve caricare la lista interna con le info relative alle tracce
	// dato che condivide l'istanza del driver MCI tra le istanze della classe, se venissero richieste le info
	// sulla traccia durante una riproduzione, il driver MCI risponderebbe con un errore, per cui memorizza le
	// info all'inizio e poi le restituisce recuperandole dalla lista e non dal driver
	if(m_plistCDATrackInfo->Count() <= 0)
	{
		MCI_STATUS_PARMS stMCIStatusParms = {0};
		stMCIStatusParms.dwItem = MCI_STATUS_NUMBER_OF_TRACKS;
		if((mcierr = mciSendCommand(m_nMCIDeviceId,MCI_STATUS,MCI_WAIT|MCI_STATUS_ITEM,(DWORD)(LPVOID)&stMCIStatusParms))==MMSYSERR_NOERROR)
		{
			CDATRACKINFO* cda;
			int nNumTracks = stMCIStatusParms.dwReturn;
			for(int i=0; i < nNumTracks; i++)
			{
				cda = (CDATRACKINFO*)m_plistCDATrackInfo->Add();
				if(cda)
				{
					cda->nTrack = i+1;
					memset(&stMCIStatusParms,'\0',sizeof(stMCIStatusParms));
					stMCIStatusParms.dwItem = MCI_STATUS_LENGTH;
					stMCIStatusParms.dwTrack = (DWORD)cda->nTrack;
					mcierr = mciSendCommand(m_nMCIDeviceId,MCI_STATUS,MCI_WAIT|MCI_TRACK|MCI_STATUS_ITEM,(DWORD)(LPVOID)&stMCIStatusParms);
					if(mcierr==MMSYSERR_NOERROR)
					{
						cda->lMinutes   = (long)MCI_MSF_MINUTE(stMCIStatusParms.dwReturn);
						cda->lSeconds   = (long)MCI_MSF_SECOND(stMCIStatusParms.dwReturn);
						cda->lFrames    = (long)MCI_MSF_FRAME(stMCIStatusParms.dwReturn);
						TRACEEXPR((_TRACE_FLAG_INFO,__NOFILE__,__NOLINE__,"CMCIAudioCDA::Play(): getting info on track #%d, min=%ld, sec=%ld, frames=%ld\n",cda->nTrack,cda->lMinutes,cda->lSeconds,cda->lFrames));
					}
				}
			}
		}
	}

	long lMinutes = 0;
	long lSeconds = 0;
	long lFrames = 0;
	ITERATOR iter;
	CDATRACKINFO* cda;		
	if((iter = m_plistCDATrackInfo->First())!=(ITERATOR)NULL)
	{
		do
		{
			cda = (CDATRACKINFO*)iter->data;
			
			if(cda)
			{
				if(cda->nTrack==m_nTrackNumber)
				{
					lMinutes = cda->lMinutes;
					lSeconds = cda->lSeconds;
					lFrames = cda->lFrames;
					break;
				}
			}

			iter = m_plistCDATrackInfo->Next(iter);
			
		} while(iter!=(ITERATOR)NULL);
	}
	if(lMinutes!=0 && lSeconds!=0 && lFrames!=0)
	{
		// riproduce la traccia per la sua lunghezza
		MCI_PLAY_PARMS stMCIPlayParms = {0};
		stMCIPlayParms.dwCallback = (DWORD)CWnd::m_hWnd;
		stMCIPlayParms.dwFrom = MCI_MAKE_TMSF(m_nTrackNumber,0,0,0);
		stMCIPlayParms.dwTo = MCI_MAKE_TMSF(m_nTrackNumber,lMinutes,lSeconds,lFrames);
		if((mcierr=mciSendCommand(m_nMCIDeviceId,MCI_PLAY,MCI_NOTIFY|MCI_FROM|MCI_TO,(DWORD)(LPVOID)&stMCIPlayParms))==MMSYSERR_NOERROR)
		{
			m_mmaPlayerStatus = mmAudioPmPlaying;
			TRACEEXPR((_TRACE_FLAG_INFO,__NOFILE__,__NOLINE__,"CMCIAudioCDA::Play(): playing track #%d\n",m_nTrackNumber));

			if(m_nTimerId!=0)
			{
				::KillTimer(AfxGetMainWnd()->m_hWnd,m_nTimerId);
				TRACEEXPR((_TRACE_FLAG_INFO,__NOFILE__,__NOLINE__,"CMCIAudioCDA::Open(): mci timer %ld deleted\n",m_nTimerId));
				m_nTimerId = 0;
			}
			m_hWndTimer = AfxGetMainWnd()->m_hWnd;
			m_nTimerId = ::SetTimer(m_hWndTimer,(UINT)this,((lMinutes*60)+lSeconds)*1000,(TIMERPROC)CdaTimerProc);
			TRACEEXPR((_TRACE_FLAG_INFO,__NOFILE__,__NOLINE__,"CMCIAudioCDA::Open(): mci timer %ld created\n",m_nTimerId));
		}
	}

#ifdef _DEBUG
	if(mcierr!=MMSYSERR_NOERROR)
	{
		char szError[512] = {0};
		mciGetErrorString(mcierr,szError,sizeof(szError)-1);
		TRACEEXPR((_TRACE_FLAG_ERR,__NOFILE__,__NOLINE__,"CMCIAudioCDA::Play(): failed (dev id=%ld, error=%ld,%s)\n",m_nMCIDeviceId,mcierr,szError));
	}
#endif

	return(mcierr);
}

/*
	Pause()
*/
MCIERROR CMCIAudioCDA::Pause(void)
{
	// l'apertura del driver MCI viene effettuata nel ctor, vedi le note relative
	MCIERROR mcierr = MCIERR_DEVICE_NOT_READY;
	if(!m_bInitialized)
	{
		TRACEEXPR((_TRACE_FLAG_ERR,__NOFILE__,__NOLINE__,"CMCIAudioCDA::Pause(): MCIERR_DEVICE_NOT_READY\n"));
		return(MCIERR_DEVICE_NOT_READY);
	}

	// mette in pausa
	MCI_STATUS_PARMS stMCIStatusParms = {0};
	stMCIStatusParms.dwItem = MCI_STATUS_MODE;
	mciSendCommand(m_nMCIDeviceId,MCI_STATUS,MCI_WAIT|MCI_STATUS_ITEM,(DWORD)(LPVOID)&stMCIStatusParms);
	if(stMCIStatusParms.dwReturn!=MCI_MODE_PAUSE)
		mcierr = mciSendCommand(m_nMCIDeviceId,MCI_PAUSE,MCI_WAIT,NULL);
	if(mcierr==MMSYSERR_NOERROR)
	{
		m_mmaPlayerStatus = mmAudioPmPaused;
		TRACEEXPR((_TRACE_FLAG_INFO,__NOFILE__,__NOLINE__,"CMCIAudioCDA::Pause(): paused\n"));
	}
	else
	{
		TRACEEXPR((_TRACE_FLAG_ERR,__NOFILE__,__NOLINE__,"CMCIAudioCDA::Pause(): failed\n"));
	}
	
	return(mcierr);
}

/*
	Resume()
*/
MCIERROR CMCIAudioCDA::Resume(void)
{
	// l'apertura del driver MCI viene effettuata nel ctor, vedi le note relative
	MCIERROR mcierr = MCIERR_DEVICE_NOT_READY;
	if(!m_bInitialized)
	{
		TRACEEXPR((_TRACE_FLAG_ERR,__NOFILE__,__NOLINE__,"CMCIAudioCDA::Resume(): MCIERR_DEVICE_NOT_READY\n"));
		return(MCIERR_DEVICE_NOT_READY);
	}

	// riprende
	MCI_STATUS_PARMS stMCIStatusParms = {0};
	stMCIStatusParms.dwItem = MCI_STATUS_MODE;
	mciSendCommand(m_nMCIDeviceId,MCI_STATUS,MCI_WAIT|MCI_STATUS_ITEM,(DWORD)(LPVOID)&stMCIStatusParms);
	if(stMCIStatusParms.dwReturn!=MCI_MODE_PLAY)
	{
		MCI_PLAY_PARMS stMCIPlayParms = {0};
		mcierr = mciSendCommand(m_nMCIDeviceId,MCI_PLAY,MCI_NOTIFY,(DWORD)(LPVOID)&stMCIPlayParms);
	}
	if(mcierr==MMSYSERR_NOERROR)
	{
		m_mmaPlayerStatus = mmAudioPmPlaying;
		TRACEEXPR((_TRACE_FLAG_INFO,__NOFILE__,__NOLINE__,"CMCIAudioCDA::Resume(): resumed\n"));
	}
	else
	{
		TRACEEXPR((_TRACE_FLAG_ERR,__NOFILE__,__NOLINE__,"CMCIAudioCDA::Resume(): failed\n"));
	}
	
	return(mcierr);
}

/*
	Stop()
*/
MCIERROR CMCIAudioCDA::Stop(void)
{
	// l'apertura del driver MCI viene effettuata nel ctor, vedi le note relative
	MCIERROR mcierr = MCIERR_DEVICE_NOT_READY;
	if(!m_bInitialized)
	{
		TRACEEXPR((_TRACE_FLAG_ERR,__NOFILE__,__NOLINE__,"CMCIAudioCDA::Stop(): MCIERR_DEVICE_NOT_READY\n"));
		return(MCIERR_DEVICE_NOT_READY);
	}

	// ferma la riproduzione
	if((mcierr = mciSendCommand(m_nMCIDeviceId,MCI_STOP,MCI_WAIT,NULL))==MMSYSERR_NOERROR)
	{
		m_mmaPlayerStatus = mmAudioPmStopped;
		TRACEEXPR((_TRACE_FLAG_INFO,__NOFILE__,__NOLINE__,"CMCIAudioCDA::Stop(): stopped\n"));
	}
	else
	{
		TRACEEXPR((_TRACE_FLAG_ERR,__NOFILE__,__NOLINE__,"CMCIAudioCDA::Stop(): failed\n"));
	}
	
	return(mcierr);
}

/*
	Close()
*/
MCIERROR CMCIAudioCDA::Close(void)
{
	// la chiusura del driver MCI viene effettuata nel dtor, vedi le note relative
	MCIERROR mcierr = MMSYSERR_NOERROR;

	if(!m_bInitialized)
	{
		TRACEEXPR((_TRACE_FLAG_ERR,__NOFILE__,__NOLINE__,"CMCIAudioCDA::Close(): MCIERR_DEVICE_NOT_READY\n"));
		mcierr = MCIERR_DEVICE_NOT_READY;
	}

	return(mcierr);
}

/*
	GetTrackInfo()
*/
MCIERROR CMCIAudioCDA::GetTrackInfo(CDATRACKINFO* pCdaTrackInfo)
{
	// l'apertura del driver MCI viene effettuata nel ctor, vedi le note relative
	MCIERROR mcierr = MCIERR_DEVICE_NOT_READY;
	if(!m_bInitialized)
	{
		TRACEEXPR((_TRACE_FLAG_ERR,__NOFILE__,__NOLINE__,"CMCIAudioCDA::GetTRackInfo(): MCIERR_DEVICE_NOT_READY\n"));
		return(mcierr);
	}

	// ricava la lunghezza della traccia
	pCdaTrackInfo->nTrack = (pCdaTrackInfo->nTrack <= 0) ? m_nTrackNumber : pCdaTrackInfo->nTrack;
	MCI_STATUS_PARMS stMCIStatusParms = {0};
	stMCIStatusParms.dwItem = MCI_STATUS_LENGTH;
	stMCIStatusParms.dwTrack = pCdaTrackInfo->nTrack;
	mcierr = mciSendCommand(m_nMCIDeviceId,MCI_STATUS,MCI_WAIT|MCI_TRACK|MCI_STATUS_ITEM,(DWORD)(LPVOID)&stMCIStatusParms);
	
	// se il driver MCI sta' riproducendo va in errore o imposta a 0 il valore della struttura, in tal caso ricava le info dalla lista interna
	if(mcierr==MMSYSERR_NOERROR && stMCIStatusParms.dwReturn!=0L)
	{
		pCdaTrackInfo->lMinutes = (long)MCI_MSF_MINUTE(stMCIStatusParms.dwReturn);
		pCdaTrackInfo->lSeconds = (long)MCI_MSF_SECOND(stMCIStatusParms.dwReturn);
		pCdaTrackInfo->lFrames = (long)MCI_MSF_FRAME(stMCIStatusParms.dwReturn);
		TRACEEXPR((_TRACE_FLAG_ERR,__NOFILE__,__NOLINE__,"CMCIAudioCDA::GetTrackInfo(): through driver: track #%d, min=%ld, sec=%ld\n",pCdaTrackInfo->nTrack,pCdaTrackInfo->lMinutes,pCdaTrackInfo->lSeconds));
	}
	else
	{
		pCdaTrackInfo->lMinutes = pCdaTrackInfo->lSeconds = 0L;

		ITERATOR iter;
		CDATRACKINFO* cda;		
		if((iter = m_plistCDATrackInfo->First())!=(ITERATOR)NULL)
		{
			do
			{
				cda = (CDATRACKINFO*)iter->data;
				
				if(cda)
				{
					if(cda->nTrack==(pCdaTrackInfo->nTrack <= 0 ? m_nTrackNumber : pCdaTrackInfo->nTrack))
					{
						pCdaTrackInfo->lMinutes = cda->lMinutes;
						pCdaTrackInfo->lSeconds = cda->lSeconds;
						pCdaTrackInfo->lFrames = cda->lFrames;
						TRACEEXPR((_TRACE_FLAG_ERR,__NOFILE__,__NOLINE__,"CMCIAudioCDA::GetTrackInfo(): cached: track #%d, min=%ld, sec=%ld\n",pCdaTrackInfo->lMinutes,pCdaTrackInfo->lSeconds));
						break;
					}
				}

				iter = m_plistCDATrackInfo->Next(iter);
				
			} while(iter!=(ITERATOR)NULL);
		}
	}

	/*
	The current standard for CD audio requires a sampling rate of 44.1 kHz and a sample size of 16 bits (2 bytes per 
	sample). As a result, you need to store 2 x 44,100 = 88,200 bytes of data every second to record in mono. Recording 
	in stereo would require twice that much storage. That extrapolates to about 10 MB of data for every minute of stereo 
	sound: (88,200 * 2) * 60 -> 10,584,000 bytes x 1 min.
	*/
	DWORD dwTotSec = (pCdaTrackInfo->lMinutes * 60) + pCdaTrackInfo->lSeconds;
	pCdaTrackInfo->qwTrackSize = 176400L * dwTotSec;
	
	return(mcierr);
}

/*
	CdaTimerProc()
*/
VOID CALLBACK CMCIAudioCDA::CdaTimerProc(HWND /*hwnd*/,UINT /*uMsg*/,UINT_PTR idEvent,DWORD /*dwTime*/)
{
	// callback (su timer) utilizzata per ricavare lo status corrente del driver MCI
	CMCIAudioCDA* pAudioMCI = (CMCIAudioCDA*)idEvent;
	
	if(pAudioMCI)
	{
		if(pAudioMCI->m_nTimerId!=0 && pAudioMCI->m_hWndTimer!=(HWND)NULL)
		{
			::KillTimer(pAudioMCI->m_hWndTimer,pAudioMCI->m_nTimerId);
			pAudioMCI->m_nTimerId = 0;
			pAudioMCI->m_hWndTimer = (HWND)NULL;
		}

		// ricava lo status corrente
		MCI_STATUS_PARMS mci_status_parms;
		mci_status_parms.dwCallback = NULL;
		mci_status_parms.dwReturn = 0;
		mci_status_parms.dwItem = MCI_STATUS_MODE;
		mci_status_parms.dwTrack = 0;

		MCIERROR mcierr = MMSYSERR_NOERROR;
		
		if((mcierr = ::mciSendCommand(pAudioMCI->m_nMCIDeviceId,
								MCI_STATUS,
								MCI_WAIT|MCI_STATUS_ITEM,
								(DWORD)(LPMCI_STATUS_PARMS)&mci_status_parms)
								)==MMSYSERR_NOERROR)
		{
			if(mci_status_parms.dwReturn==MCI_MODE_NOT_READY)
			{
				pAudioMCI->m_mmaPlayerStatus = mmAudioPmClosed;
				TRACEEXPR((_TRACE_FLAG_ERR,__NOFILE__,__NOLINE__,"CMCIAudioWave::WaveTimerProc() MCI_MODE_NOT_READY\n"));
			}
			else if(mci_status_parms.dwReturn==MCI_MODE_PAUSE)
			{
				pAudioMCI->m_mmaPlayerStatus = mmAudioPmPaused;
				TRACEEXPR((_TRACE_FLAG_INFO,__NOFILE__,__NOLINE__,"CMCIAudioWave::WaveTimerProc() MCI_MODE_PAUSE\n"));
			}
			else if(mci_status_parms.dwReturn==MCI_MODE_PLAY)
			{
				pAudioMCI->m_mmaPlayerStatus = mmAudioPmPlaying;
				TRACEEXPR((_TRACE_FLAG_INFO,__NOFILE__,__NOLINE__,"CMCIAudioWave::WaveTimerProc() MCI_MODE_PLAY\n"));

				if((mcierr = mciSendCommand(pAudioMCI->m_nMCIDeviceId,MCI_STOP,MCI_WAIT,NULL))==MMSYSERR_NOERROR)
				{
					pAudioMCI->m_mmaPlayerStatus = mmAudioPmDone;
					TRACEEXPR((_TRACE_FLAG_INFO,__NOFILE__,__NOLINE__,"CMCIAudioCDA::Stop(): stopped\n"));
				}

			}
			else if(mci_status_parms.dwReturn==MCI_MODE_STOP)
			{
				if(pAudioMCI->m_mmaPlayerStatus==mmAudioPmPlaying || pAudioMCI->m_mmaPlayerStatus==mmAudioPmDone)
				{
					pAudioMCI->m_mmaPlayerStatus = mmAudioPmDone;
					TRACEEXPR((_TRACE_FLAG_INFO,__NOFILE__,__NOLINE__,"CMCIAudioWave::WaveTimerProc() mmAudioPmDone\n"));
				}
				else
				{
					pAudioMCI->m_mmaPlayerStatus = mmAudioPmStopped;
					TRACEEXPR((_TRACE_FLAG_INFO,__NOFILE__,__NOLINE__,"CMCIAudioWave::WaveTimerProc() mmAudioPmStopped\n"));
				}
			}
		}
		else
		{
			pAudioMCI->m_mmaPlayerStatus = mmAudioPmUndefined;
		}
	}
}
