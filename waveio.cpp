/*
	waveio.cpp
	I/O sui files di tipo RIFF WAVE.
	Luca Piergentili, 14/07/03
	lpiergentili@yahoo.com

	Derivato (modificato e riadattato) dal codice di esempio MS (ACMAPP):
	
	Contains routines for opening and closing RIFF WAVE files.
	THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
	EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED WARRANTIES
	OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
	Copyright (C) 1992 - 1997 Microsoft Corporation.  All Rights Reserved.
*/
#include "pragma.h"
#include "env.h"
#include "macro.h"
#include <memory.h>
#include "window.h"
#include <mmsystem.h>
#include <mmreg.h>
#include "waveio.h"

#include "traceexpr.h"
//#define _TRACE_FLAG			_TRFLAG_TRACEOUTPUT // opzioni: _TRFLAG_NOTRACE, _TRFLAG_TRACEFILE, _TRFLAG_TRACECONSOLE, _TRFLAG_TRACEOUTPUT, _TRFLAG_TRACEBREAKPOINT
#define _TRACE_FLAG			_TRFLAG_NOTRACE // opzioni: _TRFLAG_NOTRACE, _TRFLAG_TRACEFILE, _TRFLAG_TRACECONSOLE, _TRFLAG_TRACEOUTPUT, _TRFLAG_TRACEBREAKPOINT
#define _TRACE_FLAG_INFO	_TRACE_FLAG
#define _TRACE_FLAG_WARN	_TRACE_FLAG
#define _TRACE_FLAG_ERR		_TRACE_FLAG

/*
	wioFileOpen()
*/
WIOERR WIOAPI wioFileOpen(LPWAVEIOCB pwio,LPCSTR lpcszFilePath,DWORD fdwOpen)
{
	WIOERR werr;
	HMMIO hmmio;
	MMCKINFO ckRIFF;
	MMCKINFO ck;
	DWORD dw;
#ifdef _DEBUG
	BOOL bAlreadyAsked = FALSE;
#endif

	if(!pwio)
		return(WIOERR_BADPARAM);

	memset(pwio,'\0',sizeof(*pwio));
	werr = WIOERR_FILEERROR;

	pwio->dwFlags = fdwOpen;

	// first try to open the file, etc.. open the given file for reading using buffered I/O
	hmmio = ::mmioOpen((LPSTR)lpcszFilePath,NULL,MMIO_READ | MMIO_ALLOCBUF);
	if(!hmmio)
		goto wio_Open_Error;

	pwio->hmmio = hmmio;

	// locate a 'WAVE' form type...
	ckRIFF.fccType = mmioFOURCC('W','A','V','E');
	if(::mmioDescend(hmmio,&ckRIFF,NULL,MMIO_FINDRIFF))
		goto wio_Open_Error;

	// we found a WAVE chunk--now go through and get all subchunks that we know how to deal with...
	pwio->dwDataSamples = (DWORD)-1L;

#if 0
    if (lrt=riffInitINFO(&wio.pInfo))
    {
        lr=lrt;
        goto wio_Open_Error;
    }
#endif

	while(::mmioDescend(hmmio,&ck,&ckRIFF,0)==MMSYSERR_NOERROR)
	{
#ifdef _DEBUG
		// quickly check for corrupt RIFF file--don't ascend past end!
		if((ck.dwDataOffset + ck.cksize) > (ckRIFF.dwDataOffset + ckRIFF.cksize))
		{
			if(!bAlreadyAsked)
			{
				char szError[1024];
				bAlreadyAsked = TRUE;
				sprintf(	szError,
						"This wave file (%s) might be corrupt. The RIFF chunk.ckid '%.08lX' (data offset at %lu) specifies a "
						"cksize of %lu that extends beyond what the RIFF header cksize of %lu allows. Attempt to load anyway ?",
						lpcszFilePath,
						ck.ckid,
						ck.dwDataOffset,
						ck.cksize,
						ckRIFF.cksize
						);
				if(::MessageBox(NULL,szError,"wioFileOpen()",MB_YESNO|MB_ICONEXCLAMATION|MB_TASKMODAL|MB_SETFOREGROUND|MB_TOPMOST)==IDNO)
				{
					werr = WIOERR_BADFILE;
					goto wio_Open_Error;
				}
			}
		}
#endif

		switch (ck.ckid)
		{
			case mmioFOURCC('L','I','S','T'):
				
				if(mmioFOURCC('I','N','F','O')==ck.fccType)
				{
#if 0
					if(lrt = ::riffReadINFO(hmmio,&ck,wio.pInfo))
					{
						lr = lrt;
						goto wio_Open_Error;
					}
#endif
				}
				break;

			case mmioFOURCC('D','I','S','P'):
#if 0
				riffReadDISP(hmmio, &ck, &(wio.pDisp));
#endif
				break;
                
			case mmioFOURCC('f','m','t',' '):
				
				// !?! another format chunk !?!
				if(pwio->pwfx)
					break;

				// get size of the format chunk, allocate and lock memory for it.
				// we always alloc a complete extended format header (even for PCM headers
				// that do not have the cbSize field defined--we just set it to zero).
				dw = ck.cksize;
				if(dw < sizeof(WAVEFORMATEX))
					dw = sizeof(WAVEFORMATEX);

				pwio->pwfx = (LPWAVEFORMATEX)GlobalAllocPtr(GHND,dw);
				if(!pwio->pwfx)
				{
					werr = WIOERR_NOMEM;
					goto wio_Open_Error;
				}

				// read the format chunk
				werr = WIOERR_FILEERROR;
				dw = ck.cksize;
				if(::mmioRead(hmmio,(HPSTR)pwio->pwfx,dw)!=(LONG)dw)
					goto wio_Open_Error;
				break;

			case mmioFOURCC('d','a','t','a'):
				
				// !?! multiple data chunks !?!
				if(pwio->dwDataBytes!=0L)
					break;

				// just hang on to the total length in bytes of this data chunk.. and the offset to the start of the data
				pwio->dwDataBytes = ck.cksize;
				pwio->dwDataOffset = ck.dwDataOffset;
				break;

			case mmioFOURCC('f','a','c','t'):
				
				// !?! multiple fact chunks !?!
				if(pwio->dwDataSamples!=-1L)
					break;

				// read the first dword in the fact chunk--it's the only info we need (and is currently
				// the only info defined for the fact chunk...)
				// if this fails, dwDataSamples will remain -1 so we will deal with it later...
				::mmioRead(hmmio,(HPSTR)&pwio->dwDataSamples,sizeof(DWORD));
				break;
		}

		// step up to prepare for next chunk..
		::mmioAscend(hmmio,&ck,0);
	}

	// if no fmt chunk was found, then die!
	if(!pwio->pwfx)
	{
		werr = WIOERR_ERROR;
		goto wio_Open_Error;
	}

	// all wave files other than PCM are _REQUIRED_ to have a fact chunk
	// telling the number of samples that are contained in the file. it
	// is optional for PCM (and if not present, we compute it here).
	//  if the file is not PCM and the fact chunk is not found, then fail!
	if(pwio->dwDataSamples==-1L)
	{
		if(pwio->pwfx->wFormatTag==WAVE_FORMAT_PCM)
		{
			pwio->dwDataSamples = pwio->dwDataBytes / pwio->pwfx->nBlockAlign;
		}
		else
		{
			// HACK!
			// although this should be considered an invalid wave file, we
			// will bring up a message box describing the error--hopefully
			// people will start realizing that something is missing???
			//
#ifdef _DEBUG
			char szError[1024];
			sprintf(	szError,
					"This wave file (%s) does not have a 'fact' chunk and requires one. This "
					"is completely invalid and MUST be fixed. Attempt to load it anyway ?",
					lpcszFilePath
					);
			if(::MessageBox(NULL,szError,"wioFileOpen()",MB_YESNO|MB_ICONEXCLAMATION|MB_TASKMODAL|MB_SETFOREGROUND|MB_TOPMOST)==IDNO)
			{
				werr = WIOERR_BADFILE;
				goto wio_Open_Error;
			}
#endif
			// !!! need to hack stuff in here !!!
			pwio->dwDataSamples = 0L;
		}
	}

	//  cool! no problems...
	return(WIOERR_NOERROR);

	//  return error (after minor cleanup)
wio_Open_Error:

	wioFileClose(pwio);
	return(werr);
}

/*
	wioFileClose()
*/
WIOERR WIOAPI wioFileClose(LPWAVEIOCB pwio)
{
	if(!pwio)
		return(WIOERR_BADPARAM);
	
	if(pwio->hmmio)
		::mmioClose(pwio->hmmio,0);

#if 0
	if(pwio->pInfo)
		::riffFreeINFO(&(lpwio->pInfo));

	if(pwio->pDisp)
		::riffFreeDISP(&(lpwio->pDisp));
#endif

	if(pwio->pwfx)
	{
		GlobalFreePtr(pwio->pwfx);
		pwio->pwfx = NULL;
	}

	memset(pwio,'\0',sizeof(*pwio));

	return(WIOERR_NOERROR);
}
