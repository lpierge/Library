/*$
	CContentDecoder.cpp
	Classe per la decodifica dei files compressi ricevuti dal server HTTP in seguito 
	all'invio dell'header Accept-encoding.
	Supporta tre formati base: gzip, deflate e br; gzip e deflate vengono gestiti qui, 
	br con il codice della DLL relativa: https://github.com/lpierge/BrotliDLL
	Luca Piergentili, Settembre '25

	La sigla "br" sta per "brotli", il formato di compressione sviluppato da Google. Il codice 
	relativo e' probabilmente il peggiore che abbia mai visto durante gli ultimi trenta anni, 
	perfino l'indentazione e' da incubo.
	Il processo per riuscire a compilare il codice brotli come DLL e' stato una Via Crucis.
*/
#include "pragma.h"
#include <string.h>
#include "strings.h"
#include "window.h"
#include "win32api.h"
#include "CzLib.h"
#include "brotlidll.h"
#include "CContentDecoder.h"

#include "traceexpr.h"
#define _TRACE_FLAG			_TRFLAG_TRACEOUTPUT // opzioni: _TRFLAG_NOTRACE, _TRFLAG_TRACEFILE, _TRFLAG_TRACECONSOLE, _TRFLAG_TRACEOUTPUT, _TRFLAG_TRACEBREAKPOINT
//#define _TRACE_FLAG			_TRFLAG_NOTRACE // opzioni: _TRFLAG_NOTRACE, _TRFLAG_TRACEFILE, _TRFLAG_TRACECONSOLE, _TRFLAG_TRACEOUTPUT, _TRFLAG_TRACEBREAKPOINT
#define _TRACE_FLAG_INFO	_TRACE_FLAG
#define _TRACE_FLAG_WARN	_TRACE_FLAG
#define _TRACE_FLAG_ERR		_TRACE_FLAG

// dimensione buffer lettura dati
#define CHUNK 16384

// prototipi funzioni interne
int _ungzipdeflate(FILE *source, FILE *dest,int type/* 0=deflate, 1=gzip*/);
bool ungzipdeflate(const char* inputfile,const char* outputfile,int type/* 0=deflate, 1=gzip*/);

/*
	DecodeFile()
	
	Decodifica il file (ricevuto dal server) in formato compresso.

	In input:
	- il nome file compresso (che viene con l'estensione originale del file, non con quella del formato)
	- la content ricevuta dal server indicante il tipo di compressione ("Content-encoding: <formato>")
	- la variabile per registrare il tipo di formato effettivamente ricevuto.

	Restituisce TRUE se riesce, FALSE altrimenti.
*/
BOOL CContentDecoder::DecodeFile(LPCSTR lpcszFilename,LPCSTR lpcszContentType,ENCODEDFORMAT& fmt)
{
	BOOL bFileOp = FALSE;
	BOOL bSuccess = FALSE;
	fmt = UNKNOWN_ENCODED_FORMAT;
	char szCompressedFileName[_MAX_FILEPATH+1] = {0};
	const char* pExt = "";
	char* p;

	// controlla che sia stato specificato un tipo (la content encoding inviata dal server)
	if(!lpcszContentType || !*lpcszContentType)
		return(bSuccess);

	// controlla che il file da decomprimere esista
	if(!FileExists(lpcszFilename))
		return(bSuccess);

	// salva il nome file rimuovendo l'estensione se presente
	strcpyn(szCompressedFileName,lpcszFilename,sizeof(szCompressedFileName));
	if((p = strrchr(szCompressedFileName,'.'))!=NULL)
		*p = '\0';

	// ricava l'estensione relativa al formato
	if(stricmp(lpcszContentType,"gzip")==0)
	{
		fmt = GZIP_ENCODED_FORMAT;
		pExt = ".gz";
	}
	else if(stricmp(lpcszContentType,"deflate")==0)
	{
		fmt = DEFLATE_ENCODED_FORMAT;
		pExt = ".df";
	}
	else if(stricmp(lpcszContentType,"br")==0)
	{
		fmt = BROTLI_ENCODED_FORMAT;
		pExt = ".br";
	}
	else
		return(bSuccess);

	// elimina, se gia' esistente x download anteriore, il file compresso
	// e rinomina il file di input con l'estensione relativa al formato
	strcatn(szCompressedFileName,pExt,sizeof(szCompressedFileName));
	bFileOp = DeleteFileToRecycleBin(NULL,szCompressedFileName,FALSE,1);
	bFileOp = RenameFileTo(lpcszFilename,szCompressedFileName);

	switch(fmt)
	{
		case GZIP_ENCODED_FORMAT:
			bSuccess = ungzipdeflate(szCompressedFileName,lpcszFilename,1);
			break;
		case DEFLATE_ENCODED_FORMAT:
			bSuccess = ungzipdeflate(szCompressedFileName,lpcszFilename,0);
			break;
		case BROTLI_ENCODED_FORMAT:
			bSuccess = BrotliUncompressFile(szCompressedFileName,lpcszFilename);
			break;
	}

	// elimina il file compresso
	bFileOp = DeleteFileToRecycleBin(NULL,szCompressedFileName,FALSE,1);

	return(bSuccess);
}

/*
	ungzipdeflate()

	Wrapper per la _ungzipdeflate(), per poterla chiamare con i nomi dei files, si occupa
	di aprire e chiudere i files relativi.
*/
bool ungzipdeflate(const char* inputfile,const char* outputfile,int type/* 0=deflate, 1=gzip*/)
{
	bool success = false;

    FILE *inFile = fopen(inputfile,"rb");
    FILE *outFile = fopen(outputfile,"wb");

	if(inFile==(FILE*)NULL || outFile==(FILE*)NULL)
		goto done;

	success = _ungzipdeflate(inFile,outFile,type)==Z_OK;

done:

	if(inFile!=(FILE*)NULL)
		fclose(inFile);
	if(outFile!=(FILE*)NULL)
		fclose(outFile);

	return(success);
}

/*
	_ungzipdeflate()

	La versione attualmente in uso della zLib (1.1.3) non supporta l'inizializzazione automatica per il formato gzip, che include 
	un header ed un footer specifici.
	Quindi, per non aggiornare alla ultima versione della zLib (1.3.1), la soluzione consiste nel saltare l'header gzip, ossia i 
	primi 10 byte che contengono metadati come la "firma" del formato, il metodo di compressione, il nome del file originale e un 
	timestamp.
	Saltare questi byte con fseek(source, 10, SEEK_SET) permette accedere direttamente al flusso dei dati compressi, che e' quello 
	che si aspetta la vecchia versione della zLib.
	La funzione inflateInit2 e' la versione estesa di inflateInit, che offre un controllo maggiore. Usando un parametro windowBits 
	di -15, si istruisce zlib a trattare il flusso di dati come un semplice flusso grezzo (raw deflate), senza cercare header o 
	footer ed eliminando l'errore di formato.
	Una volta che il flusso di dati grezzo (deflate) e' stato decompresso, la libreria zlib si occupa di estrarre e scartare il 
	footer di 8 byte del file gzip, che contiene il checksum e la dimensione originale dei dati.
*/
int _ungzipdeflate(FILE *source, FILE *dest,int type/* 0=deflate, 1=gzip*/)
{
    int ret = Z_OK;
    unsigned have = 0;
    z_stream strm;
    unsigned char in[CHUNK] = {0};
    unsigned char out[CHUNK] = {0};
    
    // salta l'header di 10 byte del file gzip
	if(type==1)
		fseek(source,10,SEEK_SET);

    // inizializza il flusso zlib per la decompressione
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    
    // usa inflateInit2 con -15 per gestire il flusso raw deflate
    ret = inflateInit2(&strm,-15);
    if(ret!=Z_OK)
		return(ret);

    // decomprime finche' non finisce il flusso deflate o il file
    do {
        strm.avail_in = fread(in,1,CHUNK,source);
		ret = ferror(source);
		if(ret)
		{
			inflateEnd(&strm);
            return(ret);
        }

		if(strm.avail_in==0)
			break;
        strm.next_in = in;

        // esegue inflate() finche' il buffer di output non e' pieno
        do {
            strm.avail_out = CHUNK;
            strm.next_out = out;
            ret = inflate(&strm,Z_NO_FLUSH);

            assert(ret!=Z_STREAM_ERROR);

            switch(ret)
			{
                case Z_NEED_DICT:
                case Z_DATA_ERROR:
                case Z_MEM_ERROR:
                    inflateEnd(&strm);
                    return(ret);
            }

            have = CHUNK - strm.avail_out;
            
			if(fwrite(out,1,have,dest)!=have || ferror(dest))
			{
                inflateEnd(&strm);
                return(Z_DATA_ERROR);
            }
        }
		while(strm.avail_out==0);
    }
	while(ret!=Z_STREAM_END);
    
	ret = inflateEnd(&strm);

	return(ret);
}
