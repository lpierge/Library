/*$
	CTextFile.cpp
	Classe derivata per interfaccia file di testo (SDK).
	Luca Piergentili, 06/07/98
	lpiergentili@yahoo.com
*/
#include "env.h"
#include "pragma.h"
#include "macro.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include "window.h"
#include "CBinFile.h"
#include "CTextFile.h"

#include "traceexpr.h"
//#define _TRACE_FLAG			_TRFLAG_TRACEOUTPUT // opzioni: _TRFLAG_NOTRACE, _TRFLAG_TRACEFILE, _TRFLAG_TRACECONSOLE, _TRFLAG_TRACEOUTPUT, _TRFLAG_TRACEBREAKPOINT
#define _TRACE_FLAG			_TRFLAG_NOTRACE // opzioni: _TRFLAG_NOTRACE, _TRFLAG_TRACEFILE, _TRFLAG_TRACECONSOLE, _TRFLAG_TRACEOUTPUT, _TRFLAG_TRACEBREAKPOINT
#define _TRACE_FLAG_INFO	_TRACE_FLAG
#define _TRACE_FLAG_WARN	_TRACE_FLAG
#define _TRACE_FLAG_ERR		_TRACE_FLAG

/*
	ReadLine()

	Legge una linea dal file di input.
	Elimina la coppia CRLF dal buffer di lettura restituendo la lunghezza della linea,
	restituisce FILE_EOF per fine file.
*/
DWORD CTextFile::ReadLine(LPSTR lpBuffer,DWORD dwBufferSize)
{
	ASSERTEXPR(lpBuffer && dwBufferSize > 0L);
	if(!lpBuffer || dwBufferSize==0L)
		return(0L);

    DWORD dwRead = 0L;
    char* pCrlf = NULL;
    LONG lCrlfSkip = 0L;
    
    // salva la posizione iniziale
    DWORD dwStartOffset = CBinFile::Seek(0L, FILE_CURRENT);

    // legge il blocco (lasciando spazio per lo zero finale)
    dwRead = CBinFile::Read(lpBuffer,dwBufferSize-1L);
	if(dwRead==0 || dwRead==FILE_EOF)
		return(FILE_EOF);

    // cerca il terminatore di riga
    if((pCrlf = (char*)memchr(lpBuffer,'\r',dwRead))!=NULL)
    {
        // verifica se c'e' un \n dopo \r, stando attenti ai confini
        if(((DWORD)(pCrlf-lpBuffer)+1 < dwRead) && (*(pCrlf+1)=='\n'))
            lCrlfSkip = 2L;
        else
            lCrlfSkip = 1L;
    }
    else
    {
        // prova a cercare solo \n (stile Unix)
        if((pCrlf = (char*)memchr(lpBuffer,'\n',dwRead))!=NULL)
            lCrlfSkip = 1L;
    }

    if(pCrlf)
    {
        // trovata una riga: calcola la lunghezza e tronca
        dwRead = (DWORD)(pCrlf - lpBuffer);
        lpBuffer[dwRead] = '\0';
    }
    else
    {
        // riga piu' lunga del buffer o fine file senza CRLF
        lpBuffer[dwRead] = '\0';
        lCrlfSkip = 0L;
    }

    // riposiziona il puntatore del file esattamente dopo il CRLF
    CBinFile::Seek((LONG)(dwStartOffset + dwRead + lCrlfSkip), FILE_BEGIN);

    return(dwRead);
}

/*
	WriteLine()

	Scrive una linea nel file di input.
	Restituisce il numero di caratteri scritti.
*/
DWORD CTextFile::WriteLine(LPCSTR lpcBuffer,DWORD dwToWrite/* = (DWORD)-1L */)
{
	ASSERTEXPR(lpcBuffer);
	if(!lpcBuffer)
		return(0L);

	// calcola la lunghezza della stringa
	if(dwToWrite==(DWORD)-1L)
		dwToWrite = (DWORD)strlen(lpcBuffer);

    // controlla l'esistenza del CRLF per evitare troncamenti errati
	char* pCrlf;
	if((pCrlf = (char*)memchr(lpcBuffer,'\r',dwToWrite))!=(char*)NULL)
	{
        // verifica prima di accedere a pCrlf + 1
		if((DWORD)(pCrlf - lpcBuffer) + 1 < dwToWrite)
        {
            if(*(pCrlf + 1)=='\n')
            {
                // trovato CRLF, taglia la stringa originale per non scrivere doppioni
                dwToWrite = (DWORD)(pCrlf - lpcBuffer);
            }
        }
	}

	// trascrive
	if((dwToWrite = CBinFile::Write(lpcBuffer,dwToWrite))!=FILE_EOF)
		if(CBinFile::Write("\r\n",2L)!=FILE_EOF)
			dwToWrite += 2L;

	return(dwToWrite);
}

/*
	WriteFormattedLine()

	Scrive una linea nel file di input formattandola secondo il formato e aggiungendo la coppia
	CRLF alla fine della linea. Il contenuto della linea, una volta formattata, non puo' superare
	i 4192 caratteri.
	Restituisce il numero di caratteri scritti.
*/
DWORD CTextFile::WriteFormattedLine(LPCSTR pFmt,...)
{
	ASSERTEXPR(pFmt);
	if(!pFmt)
		return(0L);

	va_list pArgs;
	char buffer[4192] = {0};
	int nLen = 0;

	// questa forma di ricavare gli argomenti assume che gli argomenti siano sempre passati nello stack in ordine contiguo
	// (__cdecl), tuttavia se per ottimizzazione o altro il compilatore decidesse di usare una convenzione diversa (come 
	// __fastcall o ottimizzazioni particolari del linker), i primi argomenti potrebbero essere passati nei registri della 
	// CPU (EAX, EDX, ECX) e non nello stack, in tal caso &pFmt + sizeof(pFmt) andrebbe a leggere un'area dello stack che 
	// potrebbe contenere qualsiasi cosa:
	//pArgs = (LPSTR)&pFmt + sizeof(pFmt);

	// inizializza gli argomenti variabili correttamente
	va_start(pArgs,pFmt);
    
	// formatta direttamente nel buffer finale
	nLen = vsnprintf(buffer,sizeof(buffer),pFmt,pArgs);
    
	va_end(pArgs);

	// se nLen e' negativo, c'e' stato un errore di formattazione
	if(nLen < 0)
		return(0L);

	// se l'output e' stato troncato (nLen >= sizeof(buffer)), nLen conterra' comunque il numero di caratteri necessari
	// lo limita alla dimensione reale del buffer
	DWORD dwActualLen = (DWORD)nLen;
	if(dwActualLen >= sizeof(buffer))
		dwActualLen = sizeof(buffer) - 1;

	// usa WriteLine per aggiungere il CRLF in modo coerente
	return(WriteLine(buffer,dwActualLen));
}
