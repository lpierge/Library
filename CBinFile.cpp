/*$
	CBinFile.cpp
	Classe base per files binari (SDK/MFC).
	Luca Piergentili, 31/08/98
	lpiergentili@yahoo.com
*/
#include "pragma.h"
#include "env.h"
#include "macro.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "strings.h"
#include "window.h"
#include "CDateTime.h"
#include "CFindFile.h"
#include "CBinFile.h"

#include "traceexpr.h"
//#define _TRACE_FLAG			_TRFLAG_TRACEOUTPUT // opzioni: _TRFLAG_NOTRACE, _TRFLAG_TRACEFILE, _TRFLAG_TRACECONSOLE, _TRFLAG_TRACEOUTPUT, _TRFLAG_TRACEBREAKPOINT
#define _TRACE_FLAG			_TRFLAG_NOTRACE // opzioni: _TRFLAG_NOTRACE, _TRFLAG_TRACEFILE, _TRFLAG_TRACECONSOLE, _TRFLAG_TRACEOUTPUT, _TRFLAG_TRACEBREAKPOINT
#define _TRACE_FLAG_INFO	_TRACE_FLAG
#define _TRACE_FLAG_WARN	_TRACE_FLAG
#define _TRACE_FLAG_ERR		_TRACE_FLAG

/*
	CBinFile()
*/
CBinFile::CBinFile()
{
#ifdef _DEBUG
	m_bShowErrors = TRUE;
#else
	m_bShowErrors = FALSE;
#endif
	m_hHandle = INVALID_HANDLE_VALUE;
	m_dwError = 0L;
	memset(m_szFileName,'\0',sizeof(m_szFileName));
	memset(m_szError,'\0',sizeof(m_szError));
}

/*
	~CBinFile()
*/
CBinFile::~CBinFile()
{
	CBinFile::Close();
}

/*
	Open()

	Apre il file.
	Notare che, se non viene specificato il contrario, crea il file se non esiste.
*/
BOOL CBinFile::Open(LPCSTR	lpcszFileName,
					BOOL	bCreateIfNotExist	/* = TRUE */,
					DWORD	dwAccessMode		/* = GENERIC_READ|GENERIC_WRITE */,
					DWORD	dwShareMode			/* = FILE_SHARE */
					)
{
	BOOL bOpen = FALSE;

	ASSERTEXPR(lpcszFileName);
	if(!lpcszFileName)
		return(bOpen);

	if(m_hHandle==INVALID_HANDLE_VALUE)
	{
		strcpyn(m_szFileName,lpcszFileName,sizeof(m_szFileName));
		
		if((m_hHandle = ::CreateFile(lpcszFileName,dwAccessMode,dwShareMode,NULL,bCreateIfNotExist ? OPEN_ALWAYS : OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL))==INVALID_HANDLE_VALUE)
			SetLastErrorCode(::GetLastError());
		else
			bOpen = TRUE;
	}

	return(bOpen);
}

/*
	OpenMode()

	Apre il file permettendo di specificare il modo:

	CREATE_ALWAYS		Crea sempre un nuovo file.
						Se il file specificato esiste ed è scrivibile, la funzione tronca il file, la funzione ha esito positivo e il codice dell'ultimo errore viene impostato su ERROR_ALREADY_EXISTS (183).
						Se il file specificato non esiste ed è un percorso valido, viene creato un nuovo file, la funzione ha esito positivo e l'ultimo codice di errore è impostato su zero.

	CREATE_NEW			Crea un nuovo file, solo se non esiste già.
						Se il file specificato esiste, la funzione ha esito negativo e l'ultimo codice di errore viene impostato su ERROR_FILE_EXISTS (80).
						Se il file specificato non esiste ed è un percorso valido per un percorso scrivibile, viene creato un nuovo file.

	OPEN_ALWAYS			Apre sempre un file.
						Se il file specificato esiste, la funzione ha esito positivo e l'ultimo codice di errore viene impostato su ERROR_ALREADY_EXISTS (183).
						Se il file specificato non esiste ed è un percorso valido per un percorso scrivibile, la funzione crea un file e l'ultimo codice di errore è impostato su zero.
	
	OPEN_EXISTING		Apre un file o un dispositivo, solo se esiste.
						Se il file o il dispositivo specificato non esiste, la funzione ha esito negativo e l'ultimo codice di errore è impostato su ERROR_FILE_NOT_FOUND (2).

	TRUNCATE_EXISTING	Apre un file e lo tronca in modo che le dimensioni siano pari a zero byte, solo se esiste.
						Se il file specificato non esiste, la funzione ha esito negativo e l'ultimo codice di errore è impostato su ERROR_FILE_NOT_FOUND (2).
						Il processo chiamante deve aprire il file con il bit GENERIC_WRITE impostato come parte del parametro dwDesiredAccess.
*/
BOOL CBinFile::OpenMode(LPCSTR	lpcszFileName,
						DWORD	dwMode				/* = OPEN_EXISTING */,
						DWORD	dwAccessMode		/* = GENERIC_READ|GENERIC_WRITE */,
						DWORD	dwShareMode			/* = FILE_SHARE */
						)
{
	BOOL bOpen = FALSE;

	ASSERTEXPR(lpcszFileName);
	if(!lpcszFileName)
		return(bOpen);

	if(m_hHandle==INVALID_HANDLE_VALUE)
	{
		strcpyn(m_szFileName,lpcszFileName,sizeof(m_szFileName));
		
		if((m_hHandle = ::CreateFile(lpcszFileName,dwAccessMode,dwShareMode,NULL,dwMode,FILE_ATTRIBUTE_NORMAL,NULL))==INVALID_HANDLE_VALUE)
			SetLastErrorCode(::GetLastError());
		else
			bOpen = TRUE;
	}

	return(bOpen);
}

/*
	Open()

	Apre il file.
	Il lock va fatto manualmente prima delle operazioni di scrittura.
*/
BOOL CBinFileLock::Open(LPCSTR	lpcszFileName,
						BOOL	bCreateIfNotExist	/* = FALSE */,
						DWORD	dwAccessMode		/* = GENERIC_READ|GENERIC_WRITE */,
						DWORD	dwShareMode			/* = FILE_SHARE */
						)
{
	ASSERTEXPR(lpcszFileName);
	if(!lpcszFileName)
		return(FALSE);

	// per il lock
	SetName(lpcszFileName);
	SetTimeout(SYNC_5_SECS_TIMEOUT);

	return(CBinFile::Open(lpcszFileName,bCreateIfNotExist,dwAccessMode,dwShareMode));
}

/*
	Create()

	Crea il file.
	Se il file gia' esiste non fallisce ma lo azzera.
	Se la directory presente nel pathname non esiste la crea.
*/
BOOL CBinFile::Create(	LPCSTR	lpcszFileName,
						DWORD	dwAccessMode/* = GENERIC_READ|GENERIC_WRITE */,
						DWORD	dwShareMode	/* = FILE_SHARE */,
						DWORD	dwAttribute	/* = FILE_ATTRIBUTE_NORMAL */
						)
{
	BOOL bCreated = FALSE;

	ASSERTEXPR(lpcszFileName);
	if(!lpcszFileName)
		return(bCreated);

	if(m_hHandle==INVALID_HANDLE_VALUE || m_hHandle==NULL)
	{
		// se il nomefile contiene un pathname verifica che esista, in caso contrario lo crea
		char* p = NULL;
		char szDirectory[_MAX_FILEPATH+1] = {0};
		strcpyn(szDirectory,lpcszFileName,sizeof(szDirectory));
		if((p = strrchr(szDirectory,'\\'))!=NULL)
		{
			if(*(p+1)) p++;
			if(*p) *p = '\0';
			CFindFile::CreatePathName(szDirectory,sizeof(szDirectory));
		}

		strcpyn(m_szFileName,lpcszFileName,sizeof(m_szFileName));

		if((m_hHandle = ::CreateFile(lpcszFileName,dwAccessMode,dwShareMode,NULL,CREATE_ALWAYS,dwAttribute,NULL))==INVALID_HANDLE_VALUE)
			SetLastErrorCode(::GetLastError());
		else
			bCreated = TRUE;
	}

	return(bCreated);
}

/*
	Close()

	Chiude il file.
*/
BOOL CBinFile::Close(void)
{
	BOOL bClose = FALSE;

	if(m_hHandle!=INVALID_HANDLE_VALUE)
	{
		if(::CloseHandle(m_hHandle))
		{
			bClose = TRUE;
			m_hHandle = INVALID_HANDLE_VALUE;
		}
		else
			SetLastErrorCode(::GetLastError());
	}

	return(bClose);
}

/*
	Read()

	Legge dal file il numero specificato di bytes nel buffer.
	Restituisce il numero di byte letti o FILE_EOF per errore.
*/
DWORD CBinFile::Read(LPVOID lpBuffer,DWORD dwToRead)
{
	ASSERTEXPR(lpBuffer && dwToRead > 0L);
	if(!lpBuffer || dwToRead==0L)
		return(FILE_EOF);

	DWORD dw = 0L;

	if(m_hHandle!=INVALID_HANDLE_VALUE)
	{
		if(!::ReadFile(m_hHandle,lpBuffer,dwToRead,&dw,NULL))
		{
			dw = FILE_EOF;
			SetLastErrorCode(::GetLastError());
		}
	}

	return(dw);
}

/*
	Write()

	Scrive nel file il contenuto del buffer per il numero specificato di bytes.
	Restituisce il numero di bytes scritti o FILE_EOF per errore.
*/
DWORD CBinFile::Write(LPCVOID lpcBuffer,DWORD dwToWrite)
{
	ASSERTEXPR(lpcBuffer && dwToWrite > 0L);
	if(!lpcBuffer || dwToWrite==0L)
		return(FILE_EOF);

	DWORD dw = 0L;

	if(dwToWrite > 0L)
		if(m_hHandle!=INVALID_HANDLE_VALUE)
		{
			if(!::WriteFile(m_hHandle,lpcBuffer,dwToWrite,&dw,NULL))
			{
				dw = FILE_EOF;
				SetLastErrorCode(::GetLastError());
			}
		}

	return(dw);
}

/*
	WriteEx()

	Scrive nel file il contenuto del buffer per il numero specificato di bytes.
	Restituisce il numero di bytes scritti o FILE_EEOF per errore.
*/
QWORD CBinFileEx::WriteEx(LPCVOID lpcBuffer,QWORD qwToWrite)
{
	#define CHUNK_SIZE 65536LL

	ASSERTEXPR(lpcBuffer && qwToWrite > 0LL);
	if(!lpcBuffer || qwToWrite==0LL)
		return(FILE_EEOF);

	QWORD qw = 0LL;

	if(qwToWrite > 0LL)
		if(m_hHandle!=INVALID_HANDLE_VALUE)
		{
			DWORD dw = 0L;
			QWORD qwTot = 0LL;
			QWORD qwAmount = CHUNK_SIZE;  // la dimensione del buffer non puo' essere QWORD perche Write solo accetta DWORD, in ogni caso spezzetta per copiare per chunks
			char* pFileContent = (char*)lpcBuffer;
			
			if(qwAmount > qwToWrite)
			{
				qw = (QWORD)Write(pFileContent,(DWORD)qwToWrite);
			}
			else
			{
				do
				{
					// calcola dinamicamente il chunk per questo giro
					// se il rimanente e' più piccolo della dimensione del chunk, usa il rimanente
					if((qwToWrite - qwTot) < CHUNK_SIZE)
						qwAmount = (qwToWrite - qwTot);
					else
						qwAmount = CHUNK_SIZE;

					dw = Write(pFileContent,(DWORD)qwAmount);
        
					if(dw==0L || dw==FILE_EOF)
					{
						qw = FILE_EEOF;
						break;
					}

					qwTot += dw;
					pFileContent += dw; // avanza nei dati di quanto effettivamente scritto
				}
				while(qwTot < qwToWrite);

				qw = qwTot; // imposta il valore da restituire
			}
		}

	return(qw);
}

/*
	Seek()

	Posiziona il puntatore all'interno del file.
	Specificare l'offset ed il punto (FILE_BEGIN/CURRENT/END) a partire dal quale posizionare il puntatore.
	Restituisce FILE_EOF per errore.

	Nota: La funzione SetFilePointer() restituisce 0xFFFFFFFF in due casi: quando c'e' un errore reale (es. 
	handle non valido, origine errata) e quando il nuovo offset del file e' esattamente 2^{32}-1, che e' un 
	valore valido se il file e' grande.
	Per distinguere i due casi, la documentazione Microsoft specifica che bisogna controllare GetLastError().
*/
DWORD CBinFile::Seek(LONG lOffset,DWORD dwOrigin)
{
	DWORD dwOffset = FILE_EOF;

	if(m_hHandle!=INVALID_HANDLE_VALUE)
	{
		if((dwOffset = ::SetFilePointer(m_hHandle,lOffset,NULL,dwOrigin))==0xFFFFFFFF)
		{
			// solo se dwErr==NO_ERROR, 0xFFFFFFFF e' un offset valido
			DWORD dwErr = ::GetLastError();
			if(dwErr!=NO_ERROR)
				SetLastErrorCode(dwErr); // dwOffset rimane 0xFFFFFFFF (FILE_EOF)
		}
	}

	return(dwOffset);
}

/*
	SeekEx()

	Posiziona il puntatore all'interno del file.
	Specificare l'offset ed il punto (FILE_BEGIN/CURRENT/END) a partire dal quale posizionare il puntatore.
	L'offset e' LONGLONG (e non QWORD) perche', come la Seek(), permette offset negativi.
	Restituisce FILE_EEOF per errore.
*/
LONGLONG CBinFileEx::SeekEx(LONGLONG llOffset,DWORD dwOrigin)
{
	QWORD qwOffset = FILE_EEOF;
	LARGE_INTEGER li = {0};
	LARGE_INTEGER ofs = {0};

	if(m_hHandle!=INVALID_HANDLE_VALUE)
	{
		li.QuadPart = llOffset;
		
		if(!::SetFilePointerEx(m_hHandle,li,&ofs,dwOrigin))
			SetLastErrorCode(::GetLastError());
		else
			qwOffset = ofs.QuadPart;
	}

	return(qwOffset);
}

/*
	GetFileSize()
*/
DWORD CBinFile::GetFileSize(void)
{
	return(m_hHandle!=INVALID_HANDLE_VALUE ? ::GetFileSize(m_hHandle,NULL) : INVALID_FILE_SIZE);
}

/*
	GetFileSizeEx()
*/
QWORD CBinFileEx::GetFileSizeEx(void)
{
	QWORD qwFileSize = FILE_EEOF;

	if(m_hHandle!=INVALID_HANDLE_VALUE)
		qwFileSize = GetFileSizeExtbyHandle(m_hHandle,NULL);

	return(qwFileSize);
}

/*
	GetFileTime()

	Ricava la data/ora UTC del file (assolute, in formato GMT). Se il secondo parametro e' TRUE,
	converte da GMT a local time, aggiungendo o sottraendo la differenza oraria rispetto a GMT:

	- data/ora assolute del file (UTC=GMT): Sun, 06 Nov 1994 08:49:37 GMT (0000)
	- zona oraria locale: GMT +1 (+0100)

	bConvertToLocalTime = FALSE -> Sun, 06 Nov 1994 08:49:37 GMT
	bConvertToLocalTime = TRUE  -> Sun, 06 Nov 1994 09:49:37 +0100
*/
BOOL CBinFile::GetFileTime(CDateTime& datetime,BOOL bConvertToLocalTime/* = FALSE*/)
{
	BOOL bGet = FALSE;

	if(m_hHandle!=INVALID_HANDLE_VALUE)
	{
		FILETIME   gmtfiletime = {0};
		FILETIME   filetime = {0};
		SYSTEMTIME systemtime = {0};

		// ricava la data/ora UTC del file (ossia assolute, GMT)
		if(::GetFileTime(m_hHandle,NULL,NULL,&gmtfiletime))
		{
			if(bConvertToLocalTime)
				::FileTimeToLocalFileTime(&gmtfiletime,&filetime); // converte in locale (aggiunge o sottrae la differenza rispetto a GMT)
			else
				memcpy(&filetime,&gmtfiletime,sizeof(FILETIME)); // nessuna conversione, data/ora assolute (UTC)

			// converte in formato di sistema
			::FileTimeToSystemTime(&filetime,&systemtime);
			
			// formatta la data/ora del file (Day, dd Mon yyyy hh:mm:ss [GMT])
			datetime.SetDateFormat(bConvertToLocalTime ? GMT : GMT_SHORT);
			datetime.SetYear(systemtime.wYear);
			datetime.SetMonth(systemtime.wMonth);
			datetime.SetDay(systemtime.wDay);
			datetime.SetHour(systemtime.wHour);
			datetime.SetMin(systemtime.wMinute);
			datetime.SetSec(systemtime.wSecond);
			
			bGet = TRUE;
		}
	}

	return(bGet);
}

/*
	SetLastErrorCode()

	Imposta il codice d'errore interno.
*/
void CBinFile::SetLastErrorCode(DWORD dwError)
{
	m_dwError = dwError;
	memset(m_szError,'\0',sizeof(m_szError));

	::FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM,
				NULL,
				m_dwError,
				MAKELANGID(LANG_NEUTRAL,SUBLANG_SYS_DEFAULT),
				(LPSTR)m_szError,
				sizeof(m_szError)-1,
				NULL 
				);

	if(m_bShowErrors)
	{
		char szError[1024] = {0};
		snprintf(szError,sizeof(szError),"%s:\n%s",m_szFileName,m_szError);
		::MessageBox(NULL,szError,"CBinFile::SetLastErrorCode()",MB_OK|MB_ICONERROR|MB_TASKMODAL|MB_SETFOREGROUND|MB_TOPMOST);
	}
}
