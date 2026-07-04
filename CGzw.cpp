/*$
	CGzw.cpp
	Classe base per il vecchio codice di interfaccia con la zLib.
	Luca Piergentili, 31/08/96 (stesura originale)
	lpiergentili@yahoo.com

	Vedi le note in CGzw.h
*/
#include "pragma.h"
#include "env.h"
#include "macro.h"
#include "typedef.h"
#include <ctype.h>
#include <direct.h>
#include <errno.h>
#include <io.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "strings.h"
#include "datetime.h"
#include <time.h>
#include <math.h>
#include "window.h"
#include "win32api.h"
#include "gzwhdr.h"
#include "CzLib.h"
#include "CGzw.h"
#include "CFindFile.h"
#include "CFilenameFactory.h"
#include "CWildCards.h"

#include "traceexpr.h"
//#define _TRACE_FLAG			_TRFLAG_TRACEOUTPUT // opzioni: _TRFLAG_NOTRACE, _TRFLAG_TRACEFILE, _TRFLAG_TRACECONSOLE, _TRFLAG_TRACEOUTPUT, _TRFLAG_TRACEBREAKPOINT
#define _TRACE_FLAG			_TRFLAG_NOTRACE // opzioni: _TRFLAG_NOTRACE, _TRFLAG_TRACEFILE, _TRFLAG_TRACECONSOLE, _TRFLAG_TRACEOUTPUT, _TRFLAG_TRACEBREAKPOINT
#define _TRACE_FLAG_INFO	_TRACE_FLAG
#define _TRACE_FLAG_WARN	_TRACE_FLAG
#define _TRACE_FLAG_ERR		_TRACE_FLAG

/*
	array interno per la lista dei tipi di file gia' compressi: il codice usa
	l'opzione di immagazzinamento (copia) invece della compressione per i tipi 
	di files elencati qui, dato che comprimere di nuovo tali files sarebbe solo
	una perdita di tempo
*/
static const char* aCompressedExtensions[] = {
    ".zip", ".rar", ".7z", ".bz2", ".gz", ".tgz", ".tar.gz",
	".xz", ".lz4", ".lzma", ".arc", ".arj",
	".iso", ".cab", ".msi",
	".rpm", ".deb",
    ".jpg", ".jpeg", ".jfif", ".png", ".gif", ".tif", ".tiff", ".webp",
	".mp3", ".acc", ".ogg", ".wma", ".flac", ".ape", ".alac",
	".mp4", ".mkv", ".avi", ".mov", ".wmv", ".flv",
	".docx", ".xlsx", ".pptx", ".odt", ".ods", ".odp",
	".pdf", ".psd",
	".jar"
};

/*
	GZWERRORS
	per l'array codice/descrizione errore
*/
struct GZW_ERRORS {
	int code;
	const char* desc;
};
typedef struct GZW_ERRORS GZWERRORS;

/*
	array interno per associare una descrizione ad ogni codice d'errore
	mantenere allineato con gli enum (a base 0) in CGzw.h
*/
static const GZWERRORS aGzwErrors[] = {
	{GZWE_UNKNOWN_ERROR		,"unknown error"},
	{GZWE_UNKNOWN_OPTION	,"unknown option"},
	{GZWE_WRONG_PARAMETERS	,"wrong parameters"},
	{GZWE_COMPRESS_ERROR	,"compression error"},
	{GZWE_UNCOMPRESS_ERROR	,"uncompress error"},
	{GZWE_LIST_ERROR		,"list error"},
	{GZWE_VIEW_ERROR		,"view error"},
	{GZWE_INVALID_FILENAME	,"invalid filename"},
	{GZWE_NOSUCHFILE		,"no such file"},
	{GZWE_INVALID_FORMAT	,"invalid format"},
	{GZWE_WRONG_PASSWORD	,"wrong password"},
	{GZWE_MALLOC_ERROR		,"allocation memory error"},
	{GZWE_SEARCH_ERROR		,"search error"},
	{GZWE_SCRIPT_ERROR		,"script error"},
	{GZWE_CHECK_ERROR		,"check error"},
	{GZWE_OPEN_ERROR		,"open error"},
	{GZWE_CREATE_ERROR		,"create error"},
	{GZWE_CLOSE_ERROR		,"close error"},
	{GZWE_SEEK_ERROR		,"seek error"},
	{GZWE_READ_ERROR		,"read error"},
	{GZWE_WRITE_ERROR		,"write error"},
	{GZWE_MKDIR_ERROR		,"unable to create directory"},
	{GZWE_UPDATE_ERROR		,"unable to update data"},
	{GZWE_UNDERSIZE			,"undersize"},
	{GZWE_FILE_EXISTS		,"file already exists"},
	{GZW_HALTED				,"halted (execution stopped)"},
	{GZW_DONE				,"done"},
	{GZW_SUCCESS			,"success"}
};

/*
	CGzw()
*/
CGzw::CGzw(HWND hWnd/* = NULL*/)
{
	// le opzioni/flags sono ripartiti tra i campi di GZW e GZWFLAGS (contenuto in GZW)
	memset(&m_Gzw,'\0',sizeof(m_Gzw));

	memset(&m_GzwHeader,'\0',sizeof(m_GzwHeader));

	// i defaults sono: path=none, recurse=no, compress ratio=9, overwrite=no, wholematch=no
	m_Gzw.nCompressRatio = 9;

	// obsoleto, non piu' usato, retaggio di quando veniva chiamato dall'applicativo FoxPro
	if(hWnd)
		m_Gzw.hWnd = hWnd;

	// liste per le inclusioni/esclusioni
	m_pitemListIncludePattern = m_pitemListExcludePattern = (CItemList*)NULL;

	// per l'allocazione dinamica del buffer di I/O di ExtractFile()
	buffer_ptr = NULL;
	malloed = 0;

	TmpClean();
}

/*
	~CGzw()
*/
CGzw::~CGzw()
{
	// rilascia l'allocazione dinamica del buffer di I/O di ExtractFile()
	if(malloed)
		if(buffer_ptr)
			free(buffer_ptr);
}

/*
	SetIncludePattern()

	Include SOLO quelli che fanno match.
	In input si possono specificare piu' elementi, separandoli con ';'.
*/
void CGzw::SetIncludePattern(LPCSTR lpcszWildcards/* = NULL*/,BOOL bIgnoreCase/* = FALSE*/)
{
	// ri-azzera la lista per ogni chiamata
	m_includePattern.Reset();

	// serializza lo skeleton/wildcards in una lista tramite la SplitPattern() della classe CWildCards
	// in seguito, quando dovra' verificare se i files fanno match con lo skeleton/wildcards, scorrera'
	// tale lista usando le Match()/MatchSubString() di CWildCards
	if(lpcszWildcards)
	{
		strcpyn(m_Gzw.szWildcardsIncl,lpcszWildcards,sizeof(m_Gzw.szWildcardsIncl));
		m_Gzw.nWildcardsInclLen = strlen(lpcszWildcards);
		m_pitemListIncludePattern = m_includePattern.SplitPattern(lpcszWildcards);
	}
	else
	{
		memset(m_Gzw.szWildcardsIncl,'\0',sizeof(m_Gzw.szWildcardsIncl));
		m_Gzw.nWildcardsInclLen = 0;
	}
	
	m_includePattern.SetIgnoreCase(bIgnoreCase);
	
	// flag interno usato dalla classe CWildCards per considerare lo spazio come 
	// separatore di piu' elementi all'interno della stringa con il pattern
	m_includePattern.SetIgnoreSpaces(TRUE);
}

/*
	SetExcludePattern()

	Esclude TUTTI quelli che fanno match.
	In input si possono specificare piu' elementi, separandoli con ';'.
*/
void CGzw::SetExcludePattern(LPCSTR lpcszWildcards/* = NULL*/,BOOL bIgnoreCase/* = FALSE*/)
{
	// ri-azzera la lista per ogni chiamata
	m_excludePattern.Reset();

	// serializza lo skeleton/wildcards in una lista tramite la SplitPattern() della classe CWildCards
	// in seguito, quando dovra' verificare se i files fanno match con lo skeleton/wildcards, scorrera'
	// tale lista usando le Match()/MatchSubString() di CWildCards
	if(lpcszWildcards)
	{
		strcpyn(m_Gzw.szWildcardsExcl,lpcszWildcards,sizeof(m_Gzw.szWildcardsExcl));
		m_Gzw.nWildcardsExclLen = strlen(lpcszWildcards);
		m_pitemListExcludePattern = m_excludePattern.SplitPattern(lpcszWildcards);
	}
	else
	{
		memset(m_Gzw.szWildcardsExcl,'\0',sizeof(m_Gzw.szWildcardsExcl));
		m_Gzw.nWildcardsExclLen = 0;
	}
	
	m_excludePattern.SetIgnoreCase(bIgnoreCase);
	
	// flag interno usato dalla classe CWildCards per considerare lo spazio come 
	// separatore di piu' elementi all'interno della stringa per il pattern
	m_excludePattern.SetIgnoreSpaces(TRUE);
}

/*
	SetPassword()
*/
UINT CGzw::SetPassword(LPCSTR lpcszPsw)
{
	// la password puo' essere lunga al massimo GZW_PSW_MAX-1 caratteri
	if(strlen(lpcszPsw) >= GZW_PSW_MAX)
	{
		memset(m_Gzw.szPsw,'\0',sizeof(m_Gzw.szPsw));
		m_Gzw.nPswLen = 0;
		return(GZWE_WRONG_PASSWORD); 
	}

	strcpyn(m_Gzw.szPsw,lpcszPsw,sizeof(m_Gzw.szPsw));
	m_Gzw.nPswLen = strlen(lpcszPsw);

	return(GZW_SUCCESS);
}

/*
	Gzw()
*/
UINT CGzw::Gzw(void)
{
	int nRet = GZWE_UNKNOWN_ERROR;

	if(m_Gzw.nGzwOperation==GZW_COMPRESS)
		nRet = Compress();
	else if(m_Gzw.nGzwOperation==GZW_MOVE)
		nRet = Compress();
	else if(m_Gzw.nGzwOperation==GZW_UNCOMPRESS)
		nRet = Uncompress();
	else if(m_Gzw.nGzwOperation==GZW_EXTRACT)
		nRet = Uncompress();
	else if(m_Gzw.nGzwOperation==GZW_LIST)
		nRet = List();

	return(nRet);
}

/*
	GetErrorDesc()
*/
LPCSTR CGzw::GetErrorDesc(UINT nRet)
{
	// cerca il codice d'errore nell'array
	for(int i=0; i < ARRAY_SIZE(aGzwErrors); i++)
		if(nRet==aGzwErrors[i].code)
			return(aGzwErrors[i].desc);
	return(aGzwErrors[0].desc);
}

/*
	Compress()

	Comprime il pattern dei file di input nel .gzw di output.
	Per ognuno dei files di input espansi dal pattern, chiama CompressFile().
	Nel ciclo di elaborazione del pattern di input, se il file non puo' essere aperto salta al successivo, senza 
	generare errore.
	In input puo' essere specificato un nome file, un pattern o un file script (preceduto dal carattere '@'). Il 
	file script e' un normale file ASCII contenente i nomi dei file di input, uno per linea. I nomi dei file presenti 
	nello script possono contenere wildcards.
	Con l'opzione -p viene memorizzato il pathname assoluto del file, mentre con l'opzione -P memorizza il pathname 
	relativo, se non viene specificata ne' una ne' l'altra, allora memorizza solo il nome file, senza nessun pathname.
	L'opzione -p genera un pathname soltanto se il pattern di input non ne contiene gia' uno, mentre l'opzione -P per 
	generare un pathname richiede la presenza dell'opzione -r, dato che il pathname (relativo) viene generato a partire
	dalla directory dove risiedono i file di input.
	Qui le inclusioni/esclusioni (-w e -W, con -f e -F) vengono applicate ai nomi dei file espansi a partire dal pattern
	di input, ad es. se l'input e' *.txt, con -wr* ee -f includera', tra tutti i .txt trovati, solo quelli il cui nome 
	inizia con r*: attenzione al flag -F perche' usa l'intero pathname, non solo il nome file, quindi r* non funzionerebbe
	in questo caso, a meno che si specifichi: -w*\r*.
*/
UINT CGzw::Compress(void)
{
	char		szOpenFlag[5] = {0};
	LPSTR		lpszInFile = NULL;
	char		szInputFileName[_MAX_PATH+1] = {0};
	char		szInputFile[_MAX_PATH+1] = {0};
	char		szOutputFile[_MAX_PATH+1] = {0};
	char		szPathName[_MAX_PATH+1] = {0};
	char		szSkeleton[_MAX_PATH+1] = {0};
	char		szPsw[GZW_PSW_MAX+1] = {0};
	HFILE		hHandle = NULL;
	QWORD		qwTotFileNameLen = 0LL;
	int			nTotFiles = 0;
	QWORD		qwCurrentData = 0LL;
	QWORD		qwLastData = 0LL;
	QWORD		qwTotData = 0LL;
	QWORD		qwTotSize = 0LL;
	QWORD		qwFileSize =0LL;
	long long	llOffset = 0LL;
	long long	llOffset1 = 0LL;
	WORD		wFileDate = 0;
	WORD		wFileTime = 0;
	UINT		nRet = GZW_SUCCESS;
	char		szUniqueRandomSalt[GZW_SALT_MAX] = {0};
	int			nRatio = 0;
	DWORD		dwError = 0L;
	BOOL		bPrevOverwrite = FALSE;

	// totali generali
	m_Gzw.qwTotFilesSize = m_Gzw.qwTotCompressedFilesSize = 0LL;
	m_Gzw.dwTotFiles = 0L;

	// imposta la callback per le chiamate che effettuara' la zLib
	gzsetcallback(CGzw::ProgressCallbackWrapper,this,GZW_CALLBACK_COMPRESS);

	if(!CheckInputOutput(GZW_COMPRESS))
	{
		nRet = GZWE_WRONG_PARAMETERS;
		return(nRet);
	}

	// verifica ed eventualmente crea la directory di output
	if(!EnsureOutDirExists(m_Gzw.szOutputFile,TRUE))
	{
		nRet = GZWE_CREATE_ERROR;
		return(nRet);
	}

	// se deve generare un nome file di output in rotazione, abilita la sovrascrittura per file gia' esistente
	bPrevOverwrite = GetOverwrite();
	if(GetRotate() > 0)
		SetOverwrite(TRUE);

	// controllo per file gia' esistente: -O = overwrite existing, default = DO NOT overwrite, applicato a estrazione (file compresso) e creazione (.gzw di output)
	if(!m_Gzw.bOverwriteExisting)
		if(FileExists(m_Gzw.szOutputFile))
		{
			nRet = GZWE_FILE_EXISTS;
			return(nRet);
		}

	// genera il nome file di output in rotazione
	if(GetRotate() > 0)
	{
		CFilenameFactory fileName;
		char szFilename[_MAX_PATH+1] = {0};
		strcpyn(szFilename,m_Gzw.szOutputFile,sizeof(szFilename));
		fileName.GetNextWithinRange(szFilename,GetRotate(),m_Gzw.szOutputFile,sizeof(m_Gzw.szOutputFile));
		m_Gzw.nOutputFileLen = strlen(m_Gzw.szOutputFile);
	}

	// genera il Salt:
	// la password, nel file di output (.gzw), e' (e deve essere) sempre la stessa per tutti i files di input,
	// motivo per cui la genera DENTRO il ciclo per ogni file, ma la generazione del salt va FUORI dal ciclo 
	// perche' il salt viene generato con una parte unica ed una random, se venisse quindi messo nel ciclo ogni 
	// file ne avrebbe uno diverso, facendo fallire il controllo della password
	// genera solo se e' stata specificata una password
	if(m_Gzw.nPswLen > 0 && m_Gzw.nPswLen < GZW_PSW_MAX)
		::GenerateUniqueRandomSalt(szUniqueRandomSalt,sizeof(szUniqueRandomSalt));

	// questo e' il flag che controlla l'append nel caso venga specificato piu' di un file di input
	// al primo ciclo viene creato un .gz (in realta' .gzw) di output, mentre nei successivi, se ci sono piu' 
	// files di input, tale file .gz viene riaperto in modalita' append
	// tenere presente che, nel lato zLib, quanto viene scritto nel .gz di output con il codice modificato, e'
	// la sequenza: header GZ + header GZW + dati compressi GZ
	BOOL bCreate = TRUE;

	// imposta la callback per l'oggetto CFindFile
	// qui usa la callback perche' deve discriminare sui nomi dei files, al contrario le funzioni di decompressione
	// e lista non usano callback perche' non devono discriminare sul loro input, ma sul contenuto dei files che
	// vengono specificati in input
	CFindFile findFile;
	PFNFINDFILECALLBACK m_lpfnFindFileCallBack = &FindFileCallBackWrapper;
	findFile.SetCallback(m_lpfnFindFileCallBack,reinterpret_cast<void*>(this));

	// input: distingue tra file script e pattern
	lpszInFile = (m_Gzw.szInputFile[0]=='@') ? (LPSTR)ParseScript(m_Gzw.szInputFile+1,&nRet) : m_Gzw.szInputFile;

	while(lpszInFile)
	{
		// espande il pattern e comprime i files di input nel .gzw di output
		findFile.SplitPathName(lpszInFile,szPathName,sizeof(szPathName),szSkeleton,sizeof(szSkeleton));

		// per tutti i fiels di input trovati
		while((lpszInFile = (LPSTR)findFile.FindEx(szPathName,szSkeleton,m_Gzw.bRecursiveSearch))!=NULL)
		{
			// pseudo-multitasking
			::Yield();

			// controlla se il file soddisfa o meno le eventuali dimensioni minime/massime
			if(m_Gzw.nMinsize > 0 || m_Gzw.nMaxsize > 0)
			{
				// ricava la dimensione del file
				QWORD qwFileSize = ::GetFileSizeExtbyName(lpszInFile);

				// se non soddisfa la dim.minima (e' piu' piccolo), lo scarta
				if(m_Gzw.nMinsize > 0)
					if(m_Gzw.nMinsize > qwFileSize)
						continue;

				// se non soddisfa la dim.massima (e' piu' grande), lo scarta
				if(m_Gzw.nMaxsize > 0)
					if(qwFileSize > m_Gzw.nMaxsize)
						continue;
			}

			// imposta nell'header GZW la versione corrente
			memset(&m_GzwHeader,'\0',sizeof(m_GzwHeader));
			memcpy(m_GzwHeader.stGzwHdr.szSignature,GZW_SIGNATURE,GZW_SIGN_LEN);
		
			// inizializza i buffer per i nomi dei file
			memset(szInputFile,'\0',sizeof(szInputFile));		// <- file da comprimere
			if(bCreate)
				memset(szOutputFile,'\0',sizeof(szOutputFile));	// <- file .gzw di output, azzera solo al primo ciclo

			// controlla se e' stata specificata una password
			if(m_Gzw.nPswLen > 0 && m_Gzw.nPswLen < GZW_PSW_MAX)
			{
				// salva la password (in chiaro) e lunghezza relativa (classe) nel campo che diventera' offuscato (header)
				m_GzwHeader.stGzwHdr.nPswLen = m_Gzw.nPswLen;
				memset(&m_GzwHeader.stGzwHdr.szPsw,'\0',sizeof(m_GzwHeader.stGzwHdr.szPsw));
				memcpy(m_GzwHeader.stGzwHdr.szPsw,m_Gzw.szPsw,m_Gzw.nPswLen);

				// copia il Salt generato sopra
				memcpy(m_GzwHeader.stGzwHdr.szSalt,szUniqueRandomSalt,sizeof(m_GzwHeader.stGzwHdr.szSalt));
    
				// offusca, nel campo dell'header, la password (chiave primaria), usando il salt (chiave secondaria)
				memnxor_salt(
					m_GzwHeader.stGzwHdr.szPsw,			// campo header per password offuscata
					m_Gzw.szPsw,						// password in chiaro della classe
					m_GzwHeader.stGzwHdr.nPswLen,		// lughezza password
					m_GzwHeader.stGzwHdr.szSalt,		// salt (chiave secondaria) x offuscamento
					sizeof(m_GzwHeader.stGzwHdr.szSalt)	// lunghezza salt
					);
			}
			else if(m_Gzw.nPswLen >= GZW_PSW_MAX)
			{
				nRet = GZWE_WRONG_PASSWORD;
				return(nRet);
			}

			// copia il nome del file di input nell'header GZW
			strcpyn(szInputFile,lpszInFile,sizeof(szInputFile));
			m_GzwHeader.stGzwHdr.nFileNameLen = strlen(szInputFile);
			
			// imposta (copia) il puntatore al nome file di input della struttura
			memset(szInputFileName,'\0',sizeof(szInputFileName));
			m_GzwHeader.pFileName = szInputFileName;
			memcpy(m_GzwHeader.pFileName,szInputFile,m_GzwHeader.stGzwHdr.nFileNameLen);

			// imposta nell'header GZW il nome del file di input e la sua lunghezza
			if(m_Gzw.nPathScheme==GZW_PATHSCHEME_ABSOLUTE)
			{
				// -p inserisce il pathname assoluto
				if((m_GzwHeader.stGzwHdr.nFileNameLen = AddAbsolutePath(m_GzwHeader.pFileName,m_GzwHeader.stGzwHdr.nFileNameLen)) < 0)
					continue;
			}
			else if(m_Gzw.nPathScheme==GZW_PATHSCHEME_RELATIVE)
			{
				// -P inserisce il pathname relativo
				if((m_GzwHeader.stGzwHdr.nFileNameLen = AddRelativePath(m_GzwHeader.pFileName,m_GzwHeader.stGzwHdr.nFileNameLen)) < 0)
					continue;
			}
			else if(m_Gzw.nPathScheme==GZW_PATHSCHEME_NONE)
			{
				// inserisce solo il nome del file
				if((m_GzwHeader.stGzwHdr.nFileNameLen = StripPath(m_GzwHeader.pFileName,m_GzwHeader.stGzwHdr.nFileNameLen)) < 0)
					continue;
			}
			else
			{
				nRet = GZWE_UNKNOWN_OPTION;
				return(nRet);
			}
			qwTotFileNameLen += (QWORD)m_GzwHeader.stGzwHdr.nFileNameLen;
			
			// non deve includere il .gzw di output (se il file .gzw di output viene specificato nella stessa dir dei files di input)
			if(Diff(szInputFile,szOutputFile)==0)
			{
				qwTotFileNameLen -= (QWORD)m_GzwHeader.stGzwHdr.nFileNameLen;
				continue;
			}

			// ricava data, ora e dimensione del file di input, salta al successivo x errore
			if(::GetFileDateTime(szInputFile,&wFileDate,&wFileTime,&qwFileSize))
				m_GzwHeader.stGzwHdr.qwFileSize = qwFileSize;
			else
			{
				qwTotFileNameLen -= (QWORD)m_GzwHeader.stGzwHdr.nFileNameLen;
				if(m_Gzw.nGzwOperation==GZW_MOVE)
					::DeleteFileToRecycleBin(NULL,szInputFile,FALSE,FALSE);
				continue;
			}

			// imposta il fattore di compressione con quanto specificato dalla classe (default=9), a meno 
			// che non si tratti di un file gia' compresso, in tal caso lo mette a 0 per non perdere tempo
			nRatio = m_Gzw.nCompressRatio;
			if(IsCompressedFile(szInputFile))
				nRatio = 0;

			// ricava il nome da dare file .gzw di output (primo ciclo) ed imposta la modalita' di apertura (creazione)
			if(bCreate)
			{
				bCreate = FALSE;
				
				// copia il nome del file di output (.gzw) nel buffer
				memcpy(szOutputFile,m_Gzw.szOutputFile,m_Gzw.nOutputFileLen);

				// imposta la modalita' (per il .gzw) su create
				snprintf(szOpenFlag,sizeof(szOpenFlag),"wb%d",nRatio);
			}
			else // cicli seguenti, aggiunge i files di input al .gzw creato sopra
			{
				// imposta la modalita' (per il .gzw) su append
				snprintf(szOpenFlag,sizeof(szOpenFlag),"ab%d",nRatio);
			}

			// chiama la callback
			if(m_Gzw.fpCallBack)
				if(!m_Gzw.fpCallBack(GZW_CALLBACK_COMPRESS_BEGIN,(LPARAM)&m_Gzw,(LPARAM)&m_GzwHeader,nRatio))
					return(GZW_HALTED);

			// comprime il file di input nel .gzw
			switch((nRet = CompressFile(szInputFile,szOutputFile,szOpenFlag,&m_GzwHeader)))
			{
				// ok
				case GZW_SUCCESS:
					break;

				// errore durante l'apertura del file di input
				case GZWE_OPEN_ERROR:
					qwTotFileNameLen -= (QWORD)m_GzwHeader.stGzwHdr.nFileNameLen;
					continue;

				// errore durante la creazione/apertura del file di output (.gzw)
				case GZWE_CREATE_ERROR:
				case GZWE_COMPRESS_ERROR:
				case GZWE_CLOSE_ERROR:
					::DeleteFile(szOutputFile);
					return(nRet);

				default:
					return(GZWE_UNKNOWN_ERROR);
			}

			// elimina il file di input per -m
			if(m_Gzw.nGzwOperation==GZW_MOVE)
				::DeleteFileToRecycleBin(NULL,szInputFile,FALSE,FALSE);

			// riapre il .gzw e registra nell'header GZW la dimensione compressa del file di input
			FILE* pStream = NULL;
			if((pStream = _fsopen(szOutputFile,"r+b",_SH_DENYNO)) != NULL) // _SH_DENYNO e' simile a _S_IREAD|_S_IWRITE per la condivisione (se necessario)
			{
				// ricava la dimensione del .gzw
				if(_fseeki64(pStream,0LL,SEEK_END)!=0)
				{
					fclose(pStream);
					return(GZWE_SEEK_ERROR);
				}
				if((qwTotSize = (QWORD)_ftelli64(pStream))==-1LL)
				{
					fclose(pStream);
					return(GZWE_SEEK_ERROR);
				}

				// salva il precedente totale dei dati compressi scritti nel .gzw
				qwLastData = qwTotData;

				// ricava il totale dei dati compressi scritti nel .gzw fino ad ora
				qwTotData = qwTotSize - (((((QWORD)GZ_HEADER_LEN + (QWORD)sizeof(GZWHEADER)) * ((QWORD)nTotFiles + 1LL))) + qwTotFileNameLen);

				// ricava il totale dei dati compressi relativo al file di input
				qwCurrentData = qwTotData - qwLastData;

				// calcola l'offset all'interno del .gzw per rileggere l'header del file di input corrente
				if(nTotFiles==0)
				{
					llOffset = (long long)GZ_HEADER_LEN;
				}
				else
				{
				#if 1
					// versione succinta della formula per il calcolo dell'offset
					llOffset = (((long long )nTotFiles + 1LL) * (long long)GZ_HEADER_LEN) + ((long long)sizeof(GZWHEADER) * (long long)nTotFiles) + ((long long)qwTotFileNameLen - (long long)m_GzwHeader.stGzwHdr.nFileNameLen) + ((long long)qwTotData - (long long)qwCurrentData);
				#else
					// versione estesa della formula per il calcolo dell'offset
					//
					// offset dovuto agli headers fissi (GZ + GZW) per i file precedenti
					long long llOffsetFixedHeaders = (((long long )nTotFiles + 1LL) * (long long)GZ_HEADER_LEN) + 
														 ((long long)sizeof(GZWHEADER) * (long long)nTotFiles);

					// offset dovuto ai nomi file compressi precedenti
					long long llOffsetPrevFileNames = (long long)qwTotFileNameLen - (long long)m_GzwHeader.stGzwHdr.nFileNameLen;

					// offset dovuto ai dati compressi dei file precedenti
					long long llOffsetPrevCompressedData = (long long)qwTotData - (long long)qwCurrentData;

					// l'offset totale e' la somma, che punta all'inizio dell'header GZW del file corrente
					llOffset = llOffsetFixedHeaders + llOffsetPrevFileNames + llOffsetPrevCompressedData;
				#endif
				}

				// posiziona e (ri)legge l'header GZW relativo al file di input
				if(_fseeki64(pStream,llOffset,SEEK_SET)!=0)
				{
					fclose(pStream);
					return(GZWE_SEEK_ERROR);
				}
				if(fread((LPVOID)&m_GzwHeader,sizeof(GZWHEADER),1,pStream)!=1)
				{
					fclose(pStream);
					::DeleteFile(szOutputFile);
					return(GZWE_READ_ERROR);
				}

				// imposta i campi dell'header GZW con la dimensione compressa, la data e l'ora
				m_GzwHeader.stGzwHdr.qwFileCompressedSize = qwCurrentData;
				m_GzwHeader.stGzwHdr.wFileDate = wFileDate;
				m_GzwHeader.stGzwHdr.wFileTime = wFileTime;

				// aggiorna l'header GZW relativo al file di input corrente
				// torna indietro di sizeof(GZWHEADER) byte per sovrascrivere l'header appena letto
				QWORD qwOffsetBack = (QWORD)sizeof(GZWHEADER);
				if(_fseeki64(pStream,qwOffsetBack * -1LL,SEEK_CUR)!=0)
				{
					fclose(pStream);
					return(GZWE_SEEK_ERROR);
				}
				if(fwrite((LPCSTR)&m_GzwHeader,sizeof(GZWHEADER),1,pStream)!=1)
				{
					fclose(pStream);
					::DeleteFile(szOutputFile);
					return(GZWE_UPDATE_ERROR);
				}
    
				// chiude il file .gzw
				fclose(pStream);
			}
			else
			{
				::DeleteFile(szOutputFile);
				return(GZWE_UPDATE_ERROR);
			}

			// incrementa il numero di file scritti nel .gzw
			nTotFiles++;

			// totali generali
			m_Gzw.qwTotFilesSize += m_GzwHeader.stGzwHdr.qwFileSize;
			m_Gzw.qwTotCompressedFilesSize += m_GzwHeader.stGzwHdr.qwFileCompressedSize;
			m_Gzw.dwTotFiles++;

			// chiama la callback
			if(m_Gzw.fpCallBack)
				if(!m_Gzw.fpCallBack(GZW_CALLBACK_COMPRESS_END,(LPARAM)&m_Gzw,(LPARAM)&m_GzwHeader,NULL))
					return(GZW_HALTED);
		}

		// ricava il file successivo
		lpszInFile = m_Gzw.szInputFile[0]=='@' ? (LPSTR)ParseScript(m_Gzw.szInputFile+1,&nRet) : NULL;
	}

	// chiama la callback con i totali generali
	if(m_Gzw.fpCallBack)
		if(!m_Gzw.fpCallBack(GZW_CALLBACK_COMPRESS_TOTAL,(LPARAM)&m_Gzw,(LPARAM)&m_GzwHeader,NULL))
			return(GZW_HALTED);

	// nessun file trovato per il pattern di input specificato
	if(nTotFiles <= 0)
		nRet = GZWE_NOSUCHFILE;

	// se ha generato un nome file di output in rotazione, ripristina il flag per la sovrascrittura
	if(GetRotate() > 0)
		SetOverwrite(bPrevOverwrite);

	return(nRet);
}

/*
	CompressFile()
	
	Comprime il file di input nel .gzw.
	A seconda della modalita' di apertura, crea o aggiunge al file .gzw il contenuto del file di input.

	Tener presente che il file di output (il .gzw) e' in realta un unico file .gz. Non si tratta di un formato 
	proprietario che incorpora vari files .gz separati, ma di un file .gz (rinominato in .gzw) che concatena 
	piu' di una compressione, intercalando l'header GZW per lo scorrimento del contenuto.
	Il codice usa la libreria zLib per creare il file .gz e comprimere in esso il o i files di input: se si tratta 
	di piu' files di input, le chiamate successive alla prima useranno il flag di aggiunta "a" invece di quello di 
	creazione "w".
	L'header GZW viene passato alle funzioni della zLib affinche' il codice modificato ad hoc dewntro la zLib lo 
	inietti tra l'header originale GZ ed i dati compressi, per poter poi navigare dentro il risultato finale (il 
	.gzw di output).
 
	Quindi: 1 o + files di input -> 1 file .gzw di output "creato" alla prima compressione e "incrementato" durante
	compressioni successive (se + di 1 file di input).
	
	Volendo "aggiungere" piu' compressioni, ossia + files di input, ad un .gzw gia' esistente, cosa che il codice
	al momento non prevede, bisognerebbe modificare il codice sottostante eliminando la madalita' di apertura "w"
	per il primo file, e posizionarsi alla fine del file prima di iniziare ad aggiungere i files di input, usando
	lo stesso codice per scorrere il .gzw come fa ExtractHeader().
*/
UINT CGzw::CompressFile(LPCSTR lpcszInFile,LPCSTR lpcszOutFile,LPCSTR lpcszOpenFlag,GZWHDR* lpHdr)
{
	FILE* fp;
	gzFile gp;
	UINT nRet = GZW_SUCCESS;
	QWORD qwTotalSize = 0LL;

	// apre il file di input
	if((fp = fopen(lpcszInFile,"rb"))==(FILE*)NULL)
		return(GZWE_OPEN_ERROR);

	// calcola la dimensione totale del file di input
    if(_fseeki64(fp,0LL,SEEK_END)!=0)
	{
		fclose(fp);
		return(GZWE_SEEK_ERROR);
	}
    if((qwTotalSize = (QWORD)_ftelli64(fp))==-1LL)
	{
		fclose(fp);
		return(GZWE_SEEK_ERROR);
	}
	if(_fseeki64(fp,0LL,SEEK_SET)!=0)
	{
		fclose(fp);
		return(GZWE_SEEK_ERROR);
	}

	// crea (o apre, se in chiamate successive per piu' files di input) il .gzw
	if((gp = gzopen(lpcszOutFile,lpcszOpenFlag,lpHdr))==(gzFile)NULL)
	{
		fclose(fp);
		return(GZWE_CREATE_ERROR);
	}

	// compressione, a questo punto il .gzw (in realta' .gz) contiene l'header GZ + header GZW + dati compressi
	if(gzcompress(fp,gp,qwTotalSize)!=0)
	{
		fclose(fp);
		gzclose(gp);
		return(GZWE_COMPRESS_ERROR);
	}
    
	// chiude il file di input
	if(fclose(fp)!=0)
		nRet = GZWE_CLOSE_ERROR;

	// chiude il file .gzw
	if(gzclose(gp)!=Z_OK)
		nRet = GZWE_CLOSE_ERROR;

	return(nRet);
}

/*
	Uncompress()
	
	Decomprime il pattern dei files di input (.gzw).
	Per ognuno dei files .gzw espansi dal pattern di input, chiama UncompressFile().
	La decompressione avviene nella directory corrente a meno che non venga specificato, come directory 
	di output, un identificativo di drive o di directory.
	Con l'opzione -p viene ricreato il pathname (memorizzato in compressione) in modo assoluto, ossia
	ignorando l'eventuale directory di output. Cio' significa che se in compressione e' stato specificato
	a:\..., la funzione estrarra' i files contenuti nel .gzw su a:\..., generando errore se i files non
	poosono essere creati.
	Con l'opzione -P viene estratto il pathname (memorizzato in compressione) in modo relativo, ossia a
	partire dalla directory corrente o da quella di output.
	Le list di inclusione/esclusione (-w e -W, con -f e -F) permettono filtrare i files da estrarre.
*/
UINT CGzw::Uncompress(void)
{
	LPSTR lpszInFile;
	UINT nRet = GZWE_NOSUCHFILE;
	char szPathName[_MAX_PATH+1] = {0};
	char szSkeleton[_MAX_PATH+1] = {0};
	CFindFile findFile;

	if(!CheckInputOutput(GZW_UNCOMPRESS))
	{
		nRet = GZWE_WRONG_PARAMETERS;
		return(nRet);
	}

	// verifica ed eventualmente crea la directory di output
	if(!EnsureOutDirExists(m_Gzw.szOutputFile,FALSE))
	{
		nRet = GZWE_CREATE_ERROR;
		return(nRet);
	}

	// espande il pattern e decomprime i files di input
	// non usa una callback per CFindFile perche' qui deve filtrare il contenuto dei files di input,
	// non il loro nome
	findFile.SplitPathName(m_Gzw.szInputFile,szPathName,sizeof(szPathName),szSkeleton,sizeof(szSkeleton));

	while((lpszInFile = (LPSTR)findFile.FindEx(szPathName,szSkeleton,m_Gzw.bRecursiveSearch))!=NULL)
	{
		// pseudo-multitasking
		::Yield();

		// imposta il nome del file .gzw di input da decomprimere
		strcpyn(m_Gzw.szInputFile,lpszInFile,sizeof(m_Gzw.szInputFile));
		m_Gzw.nInputFileLen = strlen(lpszInFile);
		
		// decomprime il file
		nRet = UncompressFile();
	}

	return(nRet);
}

/*
	UncompressFile()
	
	Decomprime il file .gzw.
	Per estrarre solo i file necessari, utilizzare le liste di inclusione/esclusione (-w e -W, con -f e -F).
*/
UINT CGzw::UncompressFile(void)
{
	FILE*	pStreamOut = NULL;
	FILE*	pStreamIn = NULL;
	gzFile	pStreamGz = NULL;
	LPSTR	lpszTmpFile = NULL;
	char	szInputFile[_MAX_PATH+1] = {0};
	char	szOutputFile[_MAX_PATH+1] = {0};
	char	szPassword[GZW_PSW_MAX+1] = {0};
	char	szPsw[GZW_PSW_MAX+1] = {0};
	QWORD	qwFileSize = 0L;
	UINT	nTotFiles = 0;
	UINT	nRet = GZW_SUCCESS;
	CFindFile findFile;

	// totali generali
	m_Gzw.qwTotFilesSize = m_Gzw.qwTotCompressedFilesSize = 0LL;
	m_Gzw.dwTotFiles = 0L;

	// imposta la callback per le chiamate che effettuara' la zLib
	gzsetcallback(CGzw::ProgressCallbackWrapper, this,GZW_CALLBACK_UNCOMPRESS);

	// alloca ed inizializza la struttura per mantenere i valori durante il ciclo di chiamate
	LPGZWHEADERDATA pstHdrData = NULL;
	if((pstHdrData = (LPGZWHEADERDATA)malloc(sizeof(GZWHEADERDATA)))==(LPGZWHEADERDATA)NULL)
	{
		nRet = GZWE_MALLOC_ERROR;
		goto done;
	}
	memset(pstHdrData,'\0',sizeof(GZWHEADERDATA));
	pstHdrData->nRet = GZW_SUCCESS;

	// inizializza l'header
	memset(&m_GzwHeader,'\0',sizeof(m_GzwHeader));

	// carica il nome del file .gzw nel buffer
	memset(szInputFile,'\0',sizeof(szInputFile));
	memcpy(szInputFile,m_Gzw.szInputFile,m_Gzw.nInputFileLen);

	// carica la password nel buffer
	if(m_Gzw.nPswLen > 0)
	{
		memset(szPassword,'\0',sizeof(szPassword));
		memcpy(szPassword,m_Gzw.szPsw,m_Gzw.nPswLen);
	}

	// controlla se il file e' in formato .gzw
	if((pstHdrData->nRet = CheckGzwHeader(szInputFile))!=GZW_SUCCESS)
		goto done;

	// apre il file .gzw
	if((pStreamIn = fopen(szInputFile,"rb"))==(FILE*)NULL)
	{
		pstHdrData->nRet = GZWE_OPEN_ERROR;
		goto done;
	}

	// ricava la dimensione del file .gzw
	if(_fseeki64(pStreamIn,0LL,SEEK_END)!=0)
	{
		pstHdrData->nRet = GZWE_SEEK_ERROR;
		goto done;
	}
	if((qwFileSize = (QWORD)_ftelli64(pStreamIn))==-1LL)
	{
		pstHdrData->nRet = GZWE_SEEK_ERROR;
		goto done;
	}
	if(_fseeki64(pStreamIn,0LL,SEEK_SET)!=0)
	{
		pstHdrData->nRet = GZWE_SEEK_ERROR;
		goto done;
	}

	// inizializza i campi della struttura per mantenere i valori durante le chiamate a ExtractFile()
	pstHdrData->wTotHeaders = 1;
	pstHdrData->pStream = pStreamIn;
	pstHdrData->pstHeader = &m_GzwHeader;
	pstHdrData->qwFileSize = qwFileSize;

	/*
	cicla per ognuno dei file compressi contenuti nel .gzw (soddisfacenti il pattern per -w):
	- lettura dell'header (fisso) del file compresso
	- lettura nell'header (variabile) del nome file/estensione e conversione dell'eventuale password
	- estrazione del file compresso nel corrispondente .tmp
	- decompressione ed eliminazione del .tmp
	*/
	while((lpszTmpFile = (LPSTR)ExtractFile(pstHdrData))!=NULL)
	{
		// pseudo-multitasking
		::Yield();

		// l'intero blocco seguente per decidere il nome del file di output da estrarre dal .gzw, ossia se usare/ricreare o meno il pathname relativo/assoluto
		memset(szOutputFile,'\0',sizeof(szOutputFile));

		if(m_Gzw.nPathScheme==GZW_PATHSCHEME_RELATIVE) // con l'opzione -P ricrea il pathname a partire dalla directory specificata (relativo)
		{
			UINT wCount = 0;
			char szCurdir[_MAX_PATH+1] = {0};

			// nessuna directory di output, ricava quella corrente
			if(m_Gzw.nOutputFileLen==0)
				::GetCurrentDirectory(_MAX_PATH,m_Gzw.szOutputFile);

			// aggiunge lo \ finale alla directory di output
			::EnsureBackslash(m_Gzw.szOutputFile,sizeof(m_Gzw.szOutputFile));
			
			// copia la directory nel buffer
			strncpy(szCurdir,m_Gzw.szOutputFile,sizeof(szCurdir));
			
			// controlla se il nome file nell'header inizia con un nome o con l'identificativo del drive
			if(isalpha(m_GzwHeader.pFileName[0]))
				if(m_GzwHeader.pFileName[1]==':')
					wCount = 1;
					
			// salta gli \ iniziali
			while(!isalpha(m_GzwHeader.pFileName[wCount]))
				wCount++;

			// aggiunge alla directory di output il nome file presente nell'header
			strcatn(szCurdir,(m_GzwHeader.pFileName+wCount),sizeof(szCurdir));

			strcpyn(szOutputFile,szCurdir,sizeof(szOutputFile));
			char* p = strrchr(szOutputFile,'\\');
			if(p)
				*p = '\0';
			
			// ricrea la directory di output
			if(!findFile.CreatePathName(szOutputFile,sizeof(szOutputFile)))
			{
				if(lpszTmpFile)
					::DeleteFile(lpszTmpFile);
				continue;
			}
			
			// imposta il nome file di output
			strcpyn(szOutputFile,szCurdir,sizeof(szOutputFile));
		}
		else if(m_Gzw.nPathScheme==GZW_PATHSCHEME_ABSOLUTE) // con l'opzione -p ricrea il pathname a partire dalla radice (assoluto)
		{
			memset(szOutputFile,'\0',sizeof(szOutputFile));
			memcpy(szOutputFile,m_GzwHeader.pFileName,m_GzwHeader.stGzwHdr.nFileNameLen);
			char* p = strrchr(szOutputFile,'\\');
			if(p)
				*p = '\0';

			// ricrea la directory di output
			if(!findFile.CreatePathName(szOutputFile,sizeof(szOutputFile)))
			{
				if(lpszTmpFile)
					::DeleteFile(lpszTmpFile);
				continue;
			}
				
			// imposta il nome file di output
			memcpy(szOutputFile,m_GzwHeader.pFileName,m_GzwHeader.stGzwHdr.nFileNameLen);
		}
		else if(m_Gzw.nPathScheme==GZW_PATHSCHEME_NONE) // senza le opzioni -p|-P elimina l'eventuale pathname presente nell'header, creando il file nella directory corrente/di output
		{
			// elimina l'eventuale pathname dal nome file presente nell'header, se non riesce passa al file successivo
			if((m_GzwHeader.stGzwHdr.nFileNameLen = StripPath(m_GzwHeader.pFileName,m_GzwHeader.stGzwHdr.nFileNameLen)) < 0)
			{
				if(lpszTmpFile)
					::DeleteFile(lpszTmpFile);
				continue;
			}
					
			// se e' stata specificata una directory di output ricrea il nome per il file di output
			if(m_Gzw.nOutputFileLen!=0)
			{    
				// inserisce la directory di output prima del nome del file
				if((m_Gzw.nOutputFileLen+m_GzwHeader.stGzwHdr.nFileNameLen+1) <= _MAX_PATH)
				{
					memcpy(szOutputFile,m_Gzw.szOutputFile,m_Gzw.nOutputFileLen);
	
					if(m_Gzw.szOutputFile[m_Gzw.nOutputFileLen-1]!='\\')
					{
						szOutputFile[m_Gzw.nOutputFileLen] = '\\';
						memcpy(((szOutputFile+m_Gzw.nOutputFileLen)+1),m_GzwHeader.pFileName,m_GzwHeader.stGzwHdr.nFileNameLen);
					}
					else
						memcpy(szOutputFile+m_Gzw.nOutputFileLen,m_GzwHeader.pFileName,m_GzwHeader.stGzwHdr.nFileNameLen);
				}
				else // directory di output troppo lunga, ripristina solo il nome del file
				{
					memcpy(szOutputFile,m_GzwHeader.pFileName,m_GzwHeader.stGzwHdr.nFileNameLen);
				}
			}
			else // nessuna directory di output, ripristina solo il nome del file
			{
				memcpy(szOutputFile,m_GzwHeader.pFileName,m_GzwHeader.stGzwHdr.nFileNameLen);
			}
		}
		else
		{
			pstHdrData->nRet = GZWE_UNKNOWN_OPTION;
			goto done;
		}

		// controllo per file gia' esistente: -O = overwrite existing, default = DO NOT overwrite, applicato a estrazione (file compresso) e creazione (.gzw di output)
		if(!m_Gzw.bOverwriteExisting)
			if(FileExists(szOutputFile))
			{
				pstHdrData->nRet = GZWE_FILE_EXISTS;
				goto done;
			}

		// chiama la callback
		if(m_Gzw.fpCallBack)
			if(!m_Gzw.fpCallBack(GZW_CALLBACK_UNCOMPRESS_BEGIN,(LPARAM)&m_Gzw,(LPARAM)&m_GzwHeader,NULL))
			{
				pstHdrData->nRet = GZW_HALTED;
				goto done;
			}

		// apre il file .tmp (contenente i dati compressi) creato da ExtractFile()
		if((pStreamGz = gzopen(lpszTmpFile,"rb",&m_GzwHeader))==(gzFile)NULL)
		{
			pstHdrData->nRet = GZWE_OPEN_ERROR;
			goto done;
		}

		/* crea il file di output relativo al .tmp */
		if((pStreamOut = fopen(szOutputFile,"wb"))==(FILE*)NULL)
		{
			pstHdrData->nRet = GZWE_CREATE_ERROR;
			goto done;
		}

		/* decompressione */
		if(gzuncompress(pStreamGz,pStreamOut,m_GzwHeader.stGzwHdr.qwFileSize)!=0)
		{
			pstHdrData->nRet = GZWE_UNCOMPRESS_ERROR;
			goto done;
		}

		/* chiude il .tmp */
		if(gzclose(pStreamGz)!=Z_OK)
		{
			pstHdrData->nRet = GZWE_CLOSE_ERROR;
			goto done;
		}
		else
			pStreamGz = NULL;

		/* chiude il file decompresso */
		if(fclose(pStreamOut)!=0)
		{
			pstHdrData->nRet = GZWE_CLOSE_ERROR;
			goto done;
		}
		else
			pStreamOut = (FILE*)NULL;

		/* reimposta la data/ora originali */
		findFile.SetFileTime(szOutputFile,(WORD)m_GzwHeader.stGzwHdr.wFileDate,(WORD)m_GzwHeader.stGzwHdr.wFileTime);

		/* elimina il file temporaneo */
		if(lpszTmpFile)
			::DeleteFile(lpszTmpFile);
		
		/* incrementa il numero di files estratti */
		nTotFiles++;

		// totali generali
		m_Gzw.qwTotFilesSize += m_GzwHeader.stGzwHdr.qwFileSize;
		m_Gzw.qwTotCompressedFilesSize += m_GzwHeader.stGzwHdr.qwFileCompressedSize;
		m_Gzw.dwTotFiles++;

		// chiama la funzione callback
		if(m_Gzw.fpCallBack)
			if(!m_Gzw.fpCallBack(GZW_CALLBACK_UNCOMPRESS_END,(LPARAM)&m_Gzw,(LPARAM)&m_GzwHeader,NULL))
			{
				pstHdrData->nRet = GZW_HALTED;
				goto done;
			}
	}

done:

	// chiama la callback con i totali generali
	if(m_Gzw.fpCallBack)
		if(!m_Gzw.fpCallBack(GZW_CALLBACK_UNCOMPRESS_TOTAL,(LPARAM)&m_Gzw,(LPARAM)&m_GzwHeader,NULL))
			pstHdrData->nRet = GZW_HALTED;

	if(pStreamIn)
		fclose(pStreamIn);

	if(pStreamOut)
		fclose(pStreamOut);

	if(pStreamGz)
		gzclose(pStreamGz);

	/* elimina il file .gzw per -x */
	if(m_Gzw.nGzwOperation==GZW_EXTRACT)
		::DeleteFileToRecycleBin(NULL,szInputFile,FALSE,FALSE);

	if(pstHdrData)
	{
		nRet = pstHdrData->nRet;
		free(pstHdrData);
	}

	if(nRet!=GZW_SUCCESS)
		if(lpszTmpFile)
			::DeleteFile(lpszTmpFile);

	return(nRet);
}

/*
	ExtractFile()
	
	Utilizzata per la decompressione del .gzw, estrae dal .gzw i files in esso contenuti creando i .tmp
	relativi da passare poi a gzuncompress().
*/
LPCSTR CGzw::ExtractFile(LPGZWHEADERDATA pstHdrData)
{
	size_t	totread = 0;
	BOOL	bWld = TRUE;
	FILE*	lpTmpFile = (FILE*)NULL;
	char	gz_header_buffer[GZ_HEADER_LEN] = {0};
	char	szTmpName[_MAX_PATH+1] = {0};
	char	szPassword[GZW_PSW_MAX+1] = {0};
	char	szPsw[GZW_PSW_MAX+1] = {0};
	DWORD	dwError = 0L;
	char	szPathName[_MAX_PATH+1] = {0};

	/* imposta il codice di ritorno */
	pstHdrData->nRet = GZW_SUCCESS;
	
	/* imposta il nome file per il .tmp */
	strcpyn(pstHdrData->szTmpName,TmpName(szTmpName,sizeof(szTmpName)),sizeof(pstHdrData->szTmpName));

	// verifica ed eventualmente crea la directory temporanea per il file .tmp
	if(!EnsureOutDirExists(m_Gzw.szOutputFile,FALSE))
	{
		pstHdrData->nRet = GZWE_CREATE_ERROR;
		goto done;
	}

extract: /* loop per scansione su pattern non soddisfatto per inclusioni/esclusioni */

	// pseudo-multitasking
	::Yield();

	/* controlla se e' arrivato alla fine del file .gzw */
	if(pstHdrData->qwFilePointer >= pstHdrData->qwFileSize)
	{
		pstHdrData->nRet = GZW_DONE;
		goto done;
	}
    
	/* posiziona (nel .gzw) il puntatore sull'offset relativo al successivo file compresso */
	if(_fseeki64(pstHdrData->pStream,pstHdrData->qwFilePointer,SEEK_SET)!=0)
	{
		pstHdrData->nRet = GZWE_SEEK_ERROR;
		goto done;
	}
    
	/* controlla che il file GZ compresso dentro il GZW sia in formato GZ valido */
	if(CheckLibraryHeader(pstHdrData->pStream,&totread,TRUE,ZLIB_LIBRARY)!=GZW_SUCCESS)
	{
		pstHdrData->nRet = GZWE_INVALID_FORMAT;
		goto done;
	}
    
	/* legge dal .gzw l'header GZ originale per trascriverlo poi nel .tmp */
	if(fread((void*)gz_header_buffer,1,GZ_HEADER_LEN,pstHdrData->pStream)!=GZ_HEADER_LEN)
	{
		pstHdrData->nRet = GZWE_READ_ERROR;
		goto done;
	}

	/* legge dal .gzw l'header GZW per trascriverlo poi nel .tmp */	
	if(fread((void*)&(pstHdrData->pstHeader->stGzwHdr),1,sizeof(GZWHEADER),pstHdrData->pStream)!=sizeof(GZWHEADER))
	{
		pstHdrData->nRet = GZWE_READ_ERROR;
		goto done;
	}

	/* controlla che il nome file abbia una dimensione consentita (ossia confacente al buffer) */
	if(pstHdrData->pstHeader->stGzwHdr.nFileNameLen > _MAX_PATH)
	{
		pstHdrData->nRet = GZWE_INVALID_FILENAME;
		goto done;
	}

	/* legge dal .gzw il nome del file compresso, per trascriverlo poi nel .tmp e lo associa al puntatore dell'header */
	memset(pstHdrData->szFileName,'\0',sizeof(pstHdrData->szFileName));
	if(fread((void*)pstHdrData->szFileName,1,pstHdrData->pstHeader->stGzwHdr.nFileNameLen,pstHdrData->pStream)!=pstHdrData->pstHeader->stGzwHdr.nFileNameLen)
	{
		pstHdrData->nRet = GZWE_READ_ERROR;
		goto done;
	}
	pstHdrData->pstHeader->pFileName = pstHdrData->szFileName;

	/* carica la password ricevuta in chiaro */
	if(m_Gzw.nPswLen > 0)
	{
		memset(szPassword,'\0',sizeof(szPassword));
		memcpy(szPassword,m_Gzw.szPsw,m_Gzw.nPswLen);
	}

	/* controlla se il file e' protetto da password, identico codice in ListFile */
	if(pstHdrData->pstHeader->stGzwHdr.nPswLen > 0)
	{
		// copia la password (in chiaro) ricevuta dall'utente (m_Gzw.szPsw->szPassword) nel buffer temporaneo (szPsw)
		memset(szPsw,'\0',sizeof(szPsw));
		memcpy(szPsw,szPassword,pstHdrData->pstHeader->stGzwHdr.nPswLen);
		
		// mette in xor la password nel buffer, usando il salt letto dall'header
		// il buffer szPsw contiene ora la password offuscata calcolata
		memnxor_salt(
			pstHdrData->pstHeader->stGzwHdr.szPsw,			// buffer header GZW (chiave in chiaro, uscira' chiave offuscata)
			m_Gzw.szPsw,									// psw ricavata dalla classe (chiave in chiaro)
			pstHdrData->pstHeader->stGzwHdr.nPswLen,		// psw len
			pstHdrData->pstHeader->stGzwHdr.szSalt,			// salt (chiave secondaria casuale)
			sizeof(pstHdrData->pstHeader->stGzwHdr.szSalt)	// lunghezza salt
			);
			
		/* confronta la password calcolata (szPsw) con quella offuscata presente nell'header */
		if(memcmp(szPsw,pstHdrData->pstHeader->stGzwHdr.szPsw,pstHdrData->pstHeader->stGzwHdr.nPswLen)!=0)
		{
			pstHdrData->nRet = GZWE_WRONG_PASSWORD;
			goto done;
		}
	}
	else /* file senza password */
	{
		/* e' stata specificata una password, ma il file non e' protetto da password, errore */
		if(m_Gzw.nPswLen > 0)
		{
			pstHdrData->nRet = GZWE_WRONG_PASSWORD;
			goto done;
		}
	}

	/* controlla inclusioni/esclusioni */
	if(bWld)
		if(CheckInclusions(pstHdrData->pstHeader->pFileName)==0 || CheckExclusions(pstHdrData->pstHeader->pFileName)==0)
			bWld = FALSE;

	// controlla se il file soddisfa o meno le eventuali dimensioni minime/massime
	if(bWld)
		if(m_Gzw.nMinsize > 0 || m_Gzw.nMaxsize > 0)
		{
			// ricava la dimensione del file
			QWORD qwFileSize = pstHdrData->pstHeader->stGzwHdr.qwFileSize;

			// se non soddisfa la dim.minima (e' piu' piccolo), lo scarta
			if(m_Gzw.nMinsize > 0)
				if(m_Gzw.nMinsize > qwFileSize)
					bWld = FALSE;

			// se non soddisfa la dim.massima (e' piu' grande), lo scarta
			if(m_Gzw.nMaxsize > 0)
				if(qwFileSize > m_Gzw.nMaxsize)
					bWld = FALSE;
		}

	// ha passato tutti i controlli, legge quindi i dati compressi dal .gzw e li trascrive nel .tmp per l'estrazione successiva
	if(bWld)
	{
		// crea il .tmp in cui trascrivere il file compresso
		if((lpTmpFile = fopen(pstHdrData->szTmpName,"wb"))==(FILE*)NULL)
		{
			pstHdrData->nRet = GZWE_CREATE_ERROR;
			goto done;
		}

		// iniza la trascrizione copiando l'header GZ
		if(fwrite((void*)gz_header_buffer,1,GZ_HEADER_LEN,lpTmpFile)!=GZ_HEADER_LEN)
		{
			pstHdrData->nRet = GZWE_WRITE_ERROR;
			goto done;
		}

		// trascrive l'header GZW
		if(fwrite((void*)&(pstHdrData->pstHeader->stGzwHdr),1,sizeof(GZWHEADER),lpTmpFile)!=sizeof(GZWHEADER))
		{
			pstHdrData->nRet = GZWE_WRITE_ERROR;
			goto done;
		}

		// trascrive il nome originale del file
		if(fwrite((void*)pstHdrData->szFileName,1,(size_t)pstHdrData->pstHeader->stGzwHdr.nFileNameLen,lpTmpFile)!=(size_t)pstHdrData->pstHeader->stGzwHdr.nFileNameLen)
		{
			pstHdrData->nRet = GZWE_WRITE_ERROR;
			goto done;
		}

		// iniza la trascrizione dei dati compressi in formato GZ dal .gzw al temporaneo
		#define IO_HEAP_SIZE    MEGABYTE	// 1 MB (preferito)
		#define IO_STACK_SIZE   65536		// 64 KB (fallback)
		unsigned long IO_BUFFER_SIZE = 0L;
		unsigned char stack_buffer[IO_STACK_SIZE];
		BOOL bIOError = FALSE;

		// allocazione dinamica del buffer di I/O, il rilascio nel distruttore
		// lo mantiene attivo per migliorare le prestazioni di I/O
		if(!malloed)
			buffer_ptr = (unsigned char*)malloc((size_t)IO_HEAP_SIZE);
		if(!buffer_ptr)
		{
			malloed = 0;
			IO_BUFFER_SIZE = IO_STACK_SIZE;
			buffer_ptr = stack_buffer;
		}
		else
		{
			IO_BUFFER_SIZE = (unsigned long)IO_HEAP_SIZE;
			malloed = 1;
		}

		size_t nBytesToRead = 0;
		QWORD qwBytesRemaining = pstHdrData->pstHeader->stGzwHdr.qwFileCompressedSize;
		QWORD qwTotalWritten = 0LL;
		QWORD qwTotalToCopy = qwBytesRemaining;

		// ciclo per la copia
		while(qwBytesRemaining > 0LL)
		{
			// calcola il % per il progresso
			int nPercent = 0;
			if(qwTotalToCopy > 0LL)
				nPercent = (int)((qwTotalWritten * 100LL) / qwTotalToCopy);

			// chiama la callback
			if(m_Gzw.fpCallBack)
				m_Gzw.fpCallBack(GZW_CALLBACK_UNCOMPRESS_TMP,(LPARAM)nPercent,(LPARAM)pstHdrData->szTmpName,NULL);

			// calcola quanti byte leggere in questa iterazione: non legge mai piu' della dimensione del buffer o dei byte rimanenti
			if(qwBytesRemaining >= IO_BUFFER_SIZE)
				nBytesToRead = IO_BUFFER_SIZE;
			else
				nBytesToRead = (size_t)qwBytesRemaining; // legge solo i byte rimanenti

			// legge dal .gzw (file di input)
			size_t nReadCount = fread(buffer_ptr,1,nBytesToRead,pstHdrData->pStream);
			if(nReadCount==0 && ferror(pstHdrData->pStream))
			{
				pstHdrData->nRet = GZWE_READ_ERROR;
				bIOError = TRUE;
				break;
			}
    
			// scrive nel file temporaneo
			size_t nWriteCount = fwrite(buffer_ptr,1,nReadCount,lpTmpFile);
			if(nWriteCount!=nReadCount)
			{
				pstHdrData->nRet = GZWE_WRITE_ERROR;
				bIOError = TRUE;
				break;
			}
    
			qwBytesRemaining -= nReadCount;
			qwTotalWritten += nReadCount;
		}

		if(m_Gzw.fpCallBack)
			if(!m_Gzw.fpCallBack(GZW_CALLBACK_UNCOMPRESS_TMP,(LPARAM)100L,(LPARAM)pstHdrData->szTmpName,NULL))
			{
				pstHdrData->nRet = GZW_HALTED;
				goto done;
			}

		// da rilasciare nel distruttore, NON qui
		//if(malloed)
			//free(buffer_ptr);

		if(bIOError)
			goto done;
	}
    
	/* aggiorna il totale relativo ai dati compressi letti */
	pstHdrData->qwTotData += pstHdrData->pstHeader->stGzwHdr.qwFileCompressedSize + (QWORD)pstHdrData->pstHeader->stGzwHdr.nFileNameLen;
    
	/* aggiorna l'offset all'interno del file .gzw (headers * tot.headers letti + tot.dati letti + tot.lunghezza nomi file letti) */
	pstHdrData->qwFilePointer = (((QWORD)GZ_HEADER_LEN+(QWORD)sizeof(GZWHEADER)) * (QWORD)pstHdrData->wTotHeaders) + pstHdrData->qwTotData;
	
	/* aggiorna il numero di headers letti per gli skip successivi */
	pstHdrData->wTotHeaders++;
	
	/* chiude il file temporaneo */
	if(lpTmpFile!=(FILE*)NULL)
	{
		if(fclose(lpTmpFile)!=0)
		{
			lpTmpFile = (FILE*)NULL;
			pstHdrData->nRet = GZWE_CLOSE_ERROR;
			goto done;
		}
		lpTmpFile = (FILE*)NULL;
	}

	/* se il file non ha soddisfatto le condizioni di inclusione/esclusione, passa al seguente */
	if(!bWld)
	{
		::DeleteFile(pstHdrData->szTmpName);
		bWld = TRUE;
		goto extract;
	}

done:

	/* chiude il file temporaneo */
	if(lpTmpFile)
		if(fclose(lpTmpFile)!=0)
			pstHdrData->nRet = GZWE_CLOSE_ERROR;

	char* pTmpName = NULL;

	/* errore, (ri)inizializza per le chiamate successive */
	if(pstHdrData->nRet!=GZW_SUCCESS)
	{
		::DeleteFile(pstHdrData->szTmpName);
		pstHdrData->qwFilePointer = 0LL;
		pstHdrData->qwTotData = 0LL;
		pstHdrData->wTotHeaders = 1;
		pTmpName = NULL;
	}
	else
	{
		pTmpName = pstHdrData->szTmpName;
	}

	if(pstHdrData->nRet==GZW_DONE)
		pstHdrData->nRet=GZW_SUCCESS;

	/* ritorna il nome del file .tmp per l'apertura dello stream da passare a gzuncompress() */	
	return(pTmpName);
}

/*
	List()
	
	Lista il pattern dei files di input (.gzw).
	Per ognuno dei file .gzw espansi dal pattern di input chiama ListFile().
	Le liste di inclusione/esclusione (-w e -W, con -f e -F) permettono selezionare i files da estrarre.
*/
UINT CGzw::List(void)
{
	LPSTR	lpszInFile;
	UINT	nRet = GZWE_NOSUCHFILE;
	char	szPathName[_MAX_PATH+1] = {0};
	char	szSkeleton[_MAX_PATH+1] = {0};
	CFindFile findFile;

	if(!CheckInputOutput(GZW_LIST))
	{
		nRet = GZWE_WRONG_PARAMETERS;
		return(nRet);
	}
		
	// espande il pattern e lista i files di input
	// non usa una callback per CFindFile perche' qui deve filtrare il contenuto dei files di input,
	// non il loro nome
	findFile.SplitPathName(m_Gzw.szInputFile,szPathName,sizeof(szPathName),szSkeleton,sizeof(szSkeleton));

	while((lpszInFile = (LPSTR)findFile.FindEx(szPathName,szSkeleton,m_Gzw.bRecursiveSearch))!=NULL)
	{
		// pseudo-multitasking
		::Yield();

		/* copia il nome del file espanso nella struttura */
		strcpyn(m_Gzw.szInputFile,lpszInFile,sizeof(m_Gzw.szInputFile));
		m_Gzw.nInputFileLen = strlen(lpszInFile);
			
		/* lista/visualizzazione */
		if((nRet = ListFile())!=GZW_SUCCESS)
			break;
	}

	return(nRet);
}

/*
	ListFile()
	
	Lista i file compressi contenuti nel .gzw.
	Per visualizzare solo i file necessari, utilizzare le liste di inclusione/esclusione (-w e -W, con -f e -F).
*/
UINT CGzw::ListFile(void)
{
	FILE*	pStreamOut = NULL;				/* stream per il file decompresso */
	FILE*	pStreamIn = NULL;				/* stream per il file .gzw */
	char	szInputFile[_MAX_PATH+1] = {0};	/* buffer per il file .gzw */
	char	szPassword[GZW_PSW_MAX+1] = {0};/* buffer per la password */
	char	szPsw[GZW_PSW_MAX+1] = {0};		/* buffer per la password */
	QWORD	qwFileSize = 0L;				/* dimensione del file .gzw */
	QWORD	qwTotSize = 0L;					/* totali bytes originali/compressi */
	QWORD	qwTotCompressed = 0L;
	UINT	nTotFiles = 0;					/* totale files */
	char	szLtosO[32] = {0};				/* buffer per ltostr() */
	char	szLtosC[32] = {0};
	char	szDtos[64] = {0};				/* buffer per datetimetostr() */
	BOOL	bHeader = FALSE;				/* flag per intestazione */
	UINT	nRet = GZW_SUCCESS;				/* codice di ritorno */

	// totali generali
	m_Gzw.qwTotFilesSize = m_Gzw.qwTotCompressedFilesSize = 0LL;
	m_Gzw.dwTotFiles = 0L;

	// con il listing NON imposta la callback per le chiamate che effettuara' la zLib
	// gzsetcallback(CGzw::ProgressCallbackWrapper,this,GZW_CALLBACK_LIST);

	// alloca e inizializza la struttura per mantenere i valori durante il ciclo di chiamate
	LPGZWHEADERDATA pstHdrData = NULL;
	if((pstHdrData = (LPGZWHEADERDATA)malloc(sizeof(GZWHEADERDATA)))==(LPGZWHEADERDATA)NULL)
	{
		nRet = GZWE_MALLOC_ERROR;
		goto done;
	}
	memset(pstHdrData,'\0',sizeof(GZWHEADERDATA));
	pstHdrData->nRet = GZW_SUCCESS;

	/* inizializza l'header */
	memset(&m_GzwHeader,'\0',sizeof(m_GzwHeader));

	/* carica il nome del file .gzw nel buffer */
	memset(szInputFile,'\0',sizeof(szInputFile));
	memcpy(szInputFile,m_Gzw.szInputFile,m_Gzw.nInputFileLen);

	/* carica la password nel buffer */
	if(m_Gzw.nPswLen > 0)
	{
		memset(szPassword,'\0',sizeof(szPassword));
		memcpy(szPassword,m_Gzw.szPsw,m_Gzw.nPswLen);
	}

	/* controlla se il file e' in formato .gzw */
	if((pstHdrData->nRet = CheckGzwHeader(szInputFile))!=GZW_SUCCESS)
		goto done;

	/* apre il file .gzw */
	if((pStreamIn = fopen(szInputFile,"rb"))==(FILE*)NULL)
	{
		pstHdrData->nRet = GZWE_OPEN_ERROR;
		goto done;
	}

	/* ricava la dimensione del file .gzw */
	if(_fseeki64(pStreamIn,0LL,SEEK_END)!=0)
	{
		pstHdrData->nRet = GZWE_SEEK_ERROR;
		goto done;
	}
	if((qwFileSize = (QWORD)_ftelli64(pStreamIn))==-1LL)
	{
		pstHdrData->nRet = GZWE_SEEK_ERROR;
		goto done;
	}
	if(_fseeki64(pStreamIn,0LL,SEEK_SET)!=0)
	{
		pstHdrData->nRet = GZWE_SEEK_ERROR;
		goto done;
	}

	/* l'output va via callback, e su file se specificato */
	pStreamOut = NULL;
	if(*(m_Gzw.szOutputFile))
	{
		if((pStreamOut = fopen(m_Gzw.szOutputFile,"wt"))==(FILE*)NULL)
		{
			pstHdrData->nRet = GZWE_CREATE_ERROR;
			goto done;
		}
	}

	// inizializza i campi della struttura per mantenere i valori durante le chiamate a ExtractHeader()
	pstHdrData->wTotHeaders = 1;
	pstHdrData->pStream = pStreamIn;
	pstHdrData->pstHeader = &m_GzwHeader;
	pstHdrData->qwFileSize = qwFileSize;

	// chiama la callback
	if(m_Gzw.fpCallBack)
		if(!m_Gzw.fpCallBack(GZW_CALLBACK_LIST_BEGIN,(LPARAM)&m_Gzw,(LPARAM)&m_GzwHeader,NULL))
		{
			pstHdrData->nRet = GZW_HALTED;
			goto done;
		}

	/*
	cicla per ognuno dei file compressi contenuti nel .gzw
	la chiamata comporta la lettura dell'header del file compresso con il ripristino, 
	nell'header, del nome file/estensione originale e della password posta in xor
	*/
	while(ExtractHeader(pstHdrData))
	{
		// pseudo-multitasking
		::Yield();

		/* controlla se il file e' protetto da password, identico codice in UncompressFile */
		if(m_GzwHeader.stGzwHdr.nPswLen > 0)
		{
			// copia la password (in chiaro) ricevuta dall'utente (m_Gzw.szPsw->szPassword) nel buffer temporaneo (szPsw)
			memset(szPsw,'\0',sizeof(szPsw));
			memcpy(szPsw,szPassword,m_GzwHeader.stGzwHdr.nPswLen);
		
			/* mette in xor la password nel buffer, usando il salt letto dall'header */	
			// il buffer szPsw contiene ora la password offuscata calcolata
			memnxor_salt(
				m_GzwHeader.stGzwHdr.szPsw,			// buffer header GZW (chiave in chiaro, uscira' chiave offuscata)
				m_Gzw.szPsw,						// psw ricavata dalla classe (chiave in chiaro)
				m_GzwHeader.stGzwHdr.nPswLen,		// psw len
				m_GzwHeader.stGzwHdr.szSalt,		// salt (chiave secondaria casuale)
				sizeof(m_GzwHeader.stGzwHdr.szSalt)	// lunghezza salt
				);
			
			/* confronta la password calcolata (szPsw) con quella offuscata presente nell'header */
			if(memcmp(szPsw,m_GzwHeader.stGzwHdr.szPsw,m_GzwHeader.stGzwHdr.nPswLen)!=0)
			{
				pstHdrData->nRet = GZWE_WRONG_PASSWORD;
				goto done;
			}
		}
		else /* file senza password */
		{
			/* e' stata specificata una password, ma il file non e' protetto da password, errore */
			if(m_Gzw.nPswLen > 0)
			{
				pstHdrData->nRet = GZWE_WRONG_PASSWORD;
				goto done;
			}
		}

		/* controlla se il nome file trovato soddisfa le liste di inclusione/esclusione */
		if(CheckInclusions(m_GzwHeader.pFileName)==0 || CheckExclusions(m_GzwHeader.pFileName)==0)
			continue;

		// controlla se il file soddisfa o meno le eventuali dimensioni minime/massime
		if(m_Gzw.nMinsize > 0 || m_Gzw.nMaxsize > 0)
		{
			// ricava la dimensione del file
			QWORD qwFileSize = pstHdrData->pstHeader->stGzwHdr.qwFileSize;

			// se non soddisfa la dim.minima (e' piu' piccolo), lo scarta
			if(m_Gzw.nMinsize > 0)
				if(m_Gzw.nMinsize > qwFileSize)
					continue;

			// se non soddisfa la dim.massima (e' piu' grande), lo scarta
			if(m_Gzw.nMaxsize > 0)
				if(qwFileSize > m_Gzw.nMaxsize)
					continue;
		}
	
		/* intestazione del report */
		if(!bHeader)
		{
			bHeader = TRUE;
			if(pStreamOut)
			{
				fprintf(pStreamOut,"listing of %s:\n\n",szInputFile);
				fprintf(pStreamOut,"%14s %14s %-6s %-4s %11s      %s\n","original size","compressed","saved","date","time","filename");
			}
		}
		
		/* lista/visualizzazione */
		if(pStreamOut)
			fprintf(pStreamOut,
					"%14s %14s  %%%-3d  %-21s %s %s\n",
					qwtos(m_GzwHeader.stGzwHdr.qwFileSize,szLtosO,sizeof(szLtosO)),
					qwtos(m_GzwHeader.stGzwHdr.qwFileCompressedSize,szLtosC,sizeof(szLtosC)),
					(int)llrint(100.0 - (100.0 * m_GzwHeader.stGzwHdr.qwFileCompressedSize) / m_GzwHeader.stGzwHdr.qwFileSize),
					DateTimeToString(m_GzwHeader.stGzwHdr.wFileDate,m_GzwHeader.stGzwHdr.wFileTime,szDtos,sizeof(szDtos)),
					m_GzwHeader.pFileName,
					pstHdrData->nRet!=GZW_SUCCESS ? "(*)" : ""
					);

		/* incrementa i totali */
		qwTotSize += m_GzwHeader.stGzwHdr.qwFileSize;
		qwTotCompressed += m_GzwHeader.stGzwHdr.qwFileCompressedSize;
		nTotFiles++;

		// totali generali
		m_Gzw.qwTotFilesSize += m_GzwHeader.stGzwHdr.qwFileSize;
		m_Gzw.qwTotCompressedFilesSize += m_GzwHeader.stGzwHdr.qwFileCompressedSize;
		m_Gzw.dwTotFiles++;

		// chiama la callback
		if(m_Gzw.fpCallBack)
			if(!m_Gzw.fpCallBack(GZW_CALLBACK_LIST,(LPARAM)&m_Gzw,(LPARAM)&m_GzwHeader,NULL))
			{
				pstHdrData->nRet = GZW_HALTED;
				goto done;
			}
	}

	// chiama la callback
	if(m_Gzw.fpCallBack)
		if(!m_Gzw.fpCallBack(GZW_CALLBACK_LIST_END,(LPARAM)&m_Gzw,(LPARAM)&m_GzwHeader,NULL))
		{
			pstHdrData->nRet = GZW_HALTED;
			goto done;
		}

	/* stampa i totali */
	if(pStreamOut && nTotFiles > 0)
	{
		char szTotSizeFmt[32] = {0};
		char szTotSizeComprFmt[32] = {0};
		char szTotSize_Fmt[32] = {0};
		char szTotSizeCompr_Fmt[32] = {0};
		strsizefmt(szTotSizeFmt,sizeof(szTotSizeFmt),(double)qwTotSize);
		snprintf(szTotSize_Fmt,sizeof(szTotSize_Fmt),"(%s)",szTotSizeFmt);
		strsizefmt(szTotSizeComprFmt,sizeof(szTotSizeComprFmt),(double)qwTotCompressed);
		snprintf(szTotSizeCompr_Fmt,sizeof(szTotSizeCompr_Fmt),"(%s)",szTotSizeComprFmt);
		fprintf(pStreamOut,
				"\n%14s %14s  %%%-3d %22s %d file(s)\n"\
				"%14s %14s\n",
				qwtos(qwTotSize,szLtosO,sizeof(szLtosO)),
				qwtos(qwTotCompressed,szLtosC,sizeof(szLtosC)),
				(int)((qwTotSize > 0) ? llrint(100.0 - (100.0 * qwTotCompressed) / qwTotSize) : 0),
				" ",
				nTotFiles,
				szTotSize_Fmt,
				szTotSizeCompr_Fmt);
	}
	
	// chiama la callback
	if(m_Gzw.fpCallBack)
		if(!m_Gzw.fpCallBack(GZW_CALLBACK_LIST_TOTAL,(LPARAM)&m_Gzw,(LPARAM)&m_GzwHeader,NULL))
		{
			pstHdrData->nRet = GZW_HALTED;
			goto done;
		}

done:

	if(pStreamIn)
		fclose(pStreamIn);

	if(pStreamOut)
		fclose(pStreamOut);

	if(pstHdrData)
	{
		nRet = pstHdrData->nRet;
		free(pstHdrData);
	}

	return(nRet);
}

/*
	ExtractHeader()

	Utilizzata per la lista del .gzw, scorre e legge gli headers dei files compressi.
*/
LPCSTR CGzw::ExtractHeader(LPGZWHEADERDATA pstHdrData)
{
	size_t totread = 0;

	/* imposta i codice di ritorno */
	pstHdrData->nRet = GZW_SUCCESS;
	pstHdrData->pstHeader->pFileName = NULL;

	/* controlla se e' arrivato alla fine del file .gzw */
	if(pstHdrData->qwFilePointer >= pstHdrData->qwFileSize)
	{
		pstHdrData->nRet = GZW_SUCCESS;
		goto done;
	}
    
	/* posiziona (nel .gzw) il puntatore sull'offset relativo al successivo file compresso */
	if(_fseeki64(pstHdrData->pStream,pstHdrData->qwFilePointer,SEEK_SET)!=0)
	{
		pstHdrData->nRet = GZWE_SEEK_ERROR;
		goto done;
	}

	/* legge (e controlla) l'header GZ */    
	if(CheckLibraryHeader(pstHdrData->pStream,&totread,FALSE,ZLIB_LIBRARY)!=GZW_SUCCESS)
	{
		pstHdrData->nRet = GZWE_INVALID_FORMAT;
		goto done;
	}

	/* legge (e controlla) l'header GZW */	
	memset(pstHdrData->pstHeader,'\0',sizeof(GZWHEADER));
	if(fread((void*)&(pstHdrData->pstHeader->stGzwHdr),1,sizeof(GZWHEADER),pstHdrData->pStream)!=sizeof(GZWHEADER))
	{
		pstHdrData->nRet = GZWE_READ_ERROR;
		goto done;
	}

	if(	/*pstHdrData->pstHeader->stGzwHdr.qwFileSize <= 0LL ||*/		// NO: dato che include files a dimensione 0 in compressione
		pstHdrData->pstHeader->stGzwHdr.qwFileCompressedSize <= 0LL || 
		pstHdrData->pstHeader->stGzwHdr.nFileNameLen <= 0 || 
		pstHdrData->pstHeader->stGzwHdr.nFileNameLen > _MAX_PATH)
	{
		pstHdrData->nRet=GZWE_INVALID_FORMAT;	/* imposta il codice di ritorno */
		goto done;
	}

	/* legge dal .gzw il nome del file compresso e lo associa al puntatore dell'header */
	memset(pstHdrData->szFileName,'\0',sizeof(pstHdrData->szFileName));
	if(fread(pstHdrData->szFileName,1,pstHdrData->pstHeader->stGzwHdr.nFileNameLen,pstHdrData->pStream)!=pstHdrData->pstHeader->stGzwHdr.nFileNameLen)
	{
		pstHdrData->nRet = GZWE_READ_ERROR;
		goto done;
	}
	pstHdrData->pstHeader->pFileName = pstHdrData->szFileName;
		
	/* aggiorna il totale relativo ai dati compressi letti (include nel totale il nome del file compresso) */
    pstHdrData->qwTotData += pstHdrData->pstHeader->stGzwHdr.qwFileCompressedSize + (QWORD)pstHdrData->pstHeader->stGzwHdr.nFileNameLen;

	/* aggiorna l'offset all'interno del file .gzw (headers * tot.headers letti + tot.dati letti + tot.lunghezza nomi file letti) */
	pstHdrData->qwFilePointer = (((QWORD)GZ_HEADER_LEN + (QWORD)sizeof(GZWHEADER)) * (QWORD)pstHdrData->wTotHeaders) + pstHdrData->qwTotData;

	/* aggiorna il numero di headers letti per gli skip successivi */
	pstHdrData->wTotHeaders++;

done:

	/* errore, (ri)inizializza per le chiamate successive */
	if(pstHdrData->nRet!=GZW_SUCCESS)
	{
		pstHdrData->qwFilePointer = 0LL;
		pstHdrData->qwTotData = 0LL;
		pstHdrData->wTotHeaders = 1;
		pstHdrData->pstHeader->pFileName = NULL;
	}

	return(pstHdrData->pstHeader->pFileName);
}

/*
	CheckGzwHeader()
	
	Controlla se il file e' in formato .gzw confrontando la signature.
*/
UINT CGzw::CheckGzwHeader(LPCSTR lpszFileName)
{
	FILE* fp;
	GZWHEADER stGzwHdr ={0};
	UINT nRet = GZW_SUCCESS;
	size_t totread = 0;
	
	if(!*lpszFileName)
		return(GZWE_INVALID_FILENAME);

	if((fp = fopen(lpszFileName,"rb"))==(FILE*)NULL)
		return(GZWE_OPEN_ERROR);

	/* legge (e controlla) l'header GZ */
	if(CheckLibraryHeader(fp,&totread,FALSE,ZLIB_LIBRARY)==GZW_SUCCESS)
	{
		/* legge l'header GZW e controlla il formato */
		if(fread(&stGzwHdr,1,sizeof(GZWHEADER),fp)!=sizeof(GZWHEADER))
			nRet = GZWE_READ_ERROR;
		else
			nRet = memcmp(&(stGzwHdr.szSignature),GZW_SIGNATURE,GZW_SIGN_LEN)==0 ? GZW_SUCCESS : GZWE_INVALID_FORMAT;
	}
	else
		nRet = GZWE_INVALID_FORMAT;

	fclose(fp);
	
	return(nRet);
}

/*
	CheckLibraryHeader()
*/
UINT CGzw::CheckLibraryHeader(FILE* pStream,size_t* totread,BOOL rewind,COMPRESSION_LIBRARY_ID nLibID)
{
	UINT nRet = GZWE_INVALID_FORMAT;

	switch(nLibID)
	{
		case ZLIB_LIBRARY:
		{
			// legge (e salta) l'header GZ di 10 byte con una sola operazione I/O
			unsigned char buffer[GZ_HEADER_LEN] = {0};
			*totread = fread(buffer,1,GZ_HEADER_LEN,pStream);
			if(*totread==GZ_HEADER_LEN)
			{
				if(CheckGzHeader(buffer))
					nRet = GZW_SUCCESS;
			}
			if(rewind)
			{
				size_t bytes_read = *totread; // QWORD realmente non necessario, il rewind e' relativo, non sulla dimensione assoluta
				if(_fseeki64(pStream,(long long)bytes_read * -1LL,SEEK_CUR)!=0)
					nRet = GZWE_SEEK_ERROR;
			}
		}
		break;
	}

	return(nRet);
}

/*
	CheckGzHeader()

	Controlla la validita' dell'header GZ e ricava i dati.
*/
BOOL CGzw::CheckGzHeader(const unsigned char* buffer)
{
	GZHEADER gz = {0};

	// magic nuber
    if(buffer[0]!=0x1F || buffer[1]!=0x8B || buffer[2]!=0x08)
		gz.valid   = FALSE;
	// resto dell'header
	else
	{
		gz.valid   = TRUE;
		gz.cm      = buffer[2];
		gz.flg     = buffer[3];
		gz.mtime   = (uint32_t)buffer[4] |
					((uint32_t)buffer[5] << 8)  |
					((uint32_t)buffer[6] << 16) |
					((uint32_t)buffer[7] << 24);
		gz.xfl     = buffer[8];
		gz.os      = buffer[9];

		gz.has_ftext   = (gz.flg & GZ_FLG_FTEXT)    != 0;
		gz.has_fhcrc   = (gz.flg & GZ_FLG_FHCRC)    != 0;
		gz.has_fextra  = (gz.flg & GZ_FLG_FEXTRA)   != 0;
		gz.has_fname   = (gz.flg & GZ_FLG_FNAME)    != 0;
		gz.has_fcomment= (gz.flg & GZ_FLG_FCOMMENT) != 0;
	}

    return(gz.valid);
}

/*
	CheckInputOutput()

	Controlla i parametri di input/output passati alle funzioni di compressione/decompressione/lista.
	Ogni funzione valuta l'input/output con uno schema differente:

	1) compressione:
	input: file/pattern=SI, directory=NO
	output: file=SI, directory=NO
	
	2) decompressione:
	input: file/pattern=SI, directory=NO
	output: directory=SI, file/pattern=NO

	3) listing:
	input: file/pattern=SI
	output: file=SI, directory/pattern=NO
*/
BOOL CGzw::CheckInputOutput(GZW_OPERATION gzwop)
{
	DWORD dwAttributes = 0L;
	
    // pattern input sempre permessi
	BOOL bInputWildcards = (strchr(m_Gzw.szInputFile,'*')!=NULL || strchr(m_Gzw.szInputFile,'?')!=NULL);
	BOOL bOutputWildcards = (strchr(m_Gzw.szOutputFile,'*')!=NULL || strchr(m_Gzw.szOutputFile,'?')!=NULL);

    // l'output non puo' mai contenere un pattern
	if(bOutputWildcards)
        return(FALSE);

	if(gzwop==GZW_COMPRESS || gzwop==GZW_MOVE)
	{
		const char* pInput = NULL;

		// deve essere passato input ed output
		if(!*m_Gzw.szInputFile || !*m_Gzw.szOutputFile)
			return(FALSE);

		// se viene specificato un file script, controlla la sintassi: la chiocciola deve essere
		// il primo carattere, come in '@C:\TMP\SCRIPT.TXT', la sintassi: 'C:\TMP\@SCRIPT.TXT' e'
		// errata
		if((pInput = strchr(m_Gzw.szInputFile,'@')) && m_Gzw.szInputFile[0]!='@')
			return(FALSE);
		if(pInput)
			pInput++;
		else
			pInput = m_Gzw.szInputFile;

		// input: file/pattern = SI, directory = NO
		// se l'input contiene wildcards, non deve controllare
        if(bInputWildcards)
			goto done_input;
		else
		{
			dwAttributes = GetFileAttributes(pInput);

			// se GetFileAttributes fallisce, significa che l'oggetto NON esiste
			// se non esiste, NON puo' essere nemmeno una directory, per cui l'input e' valido
			// (valido nel senso che e' una stringa di caratteri valida)
			if(dwAttributes==INVALID_FILE_ATTRIBUTES)
				goto done_input;

			// se l'oggetto esiste controlla che non sia una directory
			// se e' una directory l'input NON e' valido
			if(dwAttributes & FILE_ATTRIBUTE_DIRECTORY)
				return(FALSE);
		}
        
done_input:

		// output: file = SI, directory/pattern = NO
		if(bOutputWildcards)
			return(FALSE);
		else
		{
			dwAttributes = GetFileAttributes(m_Gzw.szOutputFile);

			// se GetFileAttributes fallisce, significa che l'oggetto NON esiste
			// se non esiste, NON puo' essere nemmeno una directory, per cui l'output e' valido
			// (valido nel senso che e' una stringa di caratteri valida)
			if(dwAttributes==INVALID_FILE_ATTRIBUTES)
				goto done_output;

			// se l'oggetto esiste controlla che non sia una directory
			// se e' una directory l'input NON e' valido
			if(dwAttributes & FILE_ATTRIBUTE_DIRECTORY)
				return(FALSE);
		}

done_output:

		return(TRUE);
	}
	else if(gzwop==GZW_UNCOMPRESS || gzwop==GZW_EXTRACT)
	{
        // input: file/pattern = SI, directory = NO
        if(!bInputWildcards)
        {
            // input deve essere un file esistente e non directory
            dwAttributes = GetFileAttributes(m_Gzw.szInputFile);
            if(dwAttributes==INVALID_FILE_ATTRIBUTES || (dwAttributes & FILE_ATTRIBUTE_DIRECTORY))
                return(FALSE);
        }
        
        // output: directory = SI, file/pattern = NO
        // la directory puo' esistere o meno, ma se esiste NON deve essere un file
		dwAttributes = GetFileAttributes(m_Gzw.szOutputFile);
		if(dwAttributes!=INVALID_FILE_ATTRIBUTES)
		{
            // se esiste e NON e' una directory, allora e' un file, errore
			if(!(dwAttributes & FILE_ATTRIBUTE_DIRECTORY))
				return(FALSE);
		}
		// se non esiste (INVALID_FILE_ATTRIBUTES), e' un nome di directory valido 
		// che deve essere creato dal chiamante
	}
	else if(gzwop==GZW_LIST)
	{
        // input: file/pattern = SI, directory = NO
        if(!bInputWildcards)
        {
			// input deve essere un file esistente e non una directory
            dwAttributes = GetFileAttributes(m_Gzw.szInputFile);
            if(dwAttributes==INVALID_FILE_ATTRIBUTES || (dwAttributes & FILE_ATTRIBUTE_DIRECTORY))
                return(FALSE);
        }
        
		// output (opzionale): file = SI, directory/pattern = NO
        // non puo' essere una directory
		if(*m_Gzw.szOutputFile)
		{
			dwAttributes = GetFileAttributes(m_Gzw.szOutputFile);
			if(dwAttributes!=INVALID_FILE_ATTRIBUTES && (dwAttributes & FILE_ATTRIBUTE_DIRECTORY))
				return(FALSE);
		}
	}
    else
    {
        // azione sconosciuta
        return(FALSE);
    }

	return(TRUE);
}

/*
	ParseScript()
*/
LPCSTR CGzw::ParseScript(LPCSTR lpszFileSpec,UINT* nRet)
{    
	FILE* pStream;
	static QWORD qwFilePointer = 0L;
	static char szFileSpec[_MAX_PATH+1] = {0};
	LPSTR lpszFileName = NULL;
	char* p;

	/* controlla la validita' del nome file */
	if(!*lpszFileSpec)
	{
		*nRet = GZWE_INVALID_FILENAME;
		return(NULL);
	}
	
	/* imposta il ciclo per saltare al file successivo se quello appena letto dallo script non esiste */
	while(TRUE)
	{
		/* apre il file script */
		if((pStream = fopen(lpszFileSpec,"rt"))!=(FILE*)NULL)
		{
			/* (ri)posiziona */
			if(_fseeki64(pStream,qwFilePointer,SEEK_SET)==0)
			{
				/* legge una linea */
	    		if(fgets(szFileSpec,sizeof(szFileSpec)-1,pStream)!=NULL)
	    		{
					/* aggiorna l'offset per la successiva lettura */
					qwFilePointer += (QWORD)strlen(szFileSpec)+1LL;
			    	
					/* elimina il \n di fine riga */
					if((p = strrchr(szFileSpec,'\n'))!=NULL)
						*p = '\0';
					//crlf(szFileSpec);
				
					lpszFileName = szFileSpec;
				}
				else /* eof sul file script */
				{
					qwFilePointer = 0LL; /* (ri)inizializza per le chiamate successive */
				}
			}
			else
			{
				*nRet = GZWE_OPEN_ERROR;
			}
			fclose(pStream);
		}
		else
		{
			*nRet = GZWE_OPEN_ERROR;
		}
		
		break;
	}

	return(lpszFileName);
}

/*
	StripPath()
	
	Elimina il pathname dal nome del file, restituendone la nuova lunghezza.
*/
UINT CGzw::StripPath(LPSTR lpszFileName,UINT wFileLen)
{
	LPSTR	lpPtr;
	char	szBuffer[_MAX_PATH+1] = {0};
	
	/* controlla la validita' del nome file */
	if(!*lpszFileName || wFileLen <= 0 || wFileLen > _MAX_PATH)
		return(0);

	/* se il nome file non contiene un pathname ritorna */
	if(strchr(lpszFileName,'\\')==(char *)NULL)
		return(wFileLen);
	
	/* copia il nome del file */
	strcpyn(szBuffer,lpszFileName,sizeof(szBuffer));
	
	/* inverte il buffer per trovare l'inizio del pathname */
	strrev(szBuffer);
	
	/* cerca l'inizio del pathname */
	if((lpPtr = strchr(szBuffer,'\\'))==NULL)
		lpPtr = strchr(szBuffer,':');
	
	/* se trova un pathname lo elimina */
	if(lpPtr!=NULL)
		*lpPtr = (char)NULL;
		
	/* (re)inverte il buffer */
	strrev(szBuffer);
    
	/* copia il contenuto del buffer sul nome file */
	strcpyn(lpszFileName,szBuffer,wFileLen);

	/* ritorna la lunghezza del nome file */
	return(strlen(lpszFileName));
}

/*
	AddAbsolutePath()
	
	Inserisce il pathname assoluto nel nome del file, restituendone la nuova lunghezza.
	Se il nome file contiene gia' un pathname ritorna, altrimenti inserisce quello relativo
	alla directory corrente.
	Il buffer contenente il nome del file deve essere di _MAX_PATH+1 caratteri.
*/
UINT CGzw::AddAbsolutePath(LPSTR lpszFileName,UINT wFileLen)
{
	size_t	wLen;
	char	szBuffer[_MAX_PATH+1] = {0};
	
	/* controlla la validita' del nome file */
	if(!*lpszFileName || wFileLen <= 0)
		return(0);
	
	/* se il nome file contiene gia' un drive/pathname ritorna */
	if(strchr(lpszFileName,'\\')!=(char *)NULL || strchr(lpszFileName,':')!=(char *)NULL)
		return(wFileLen);

	/* ricava la directory corrente e aggiunge il nome file (lascia lo spazio per il nome file + NULL) */
	::GetCurrentDirectory((_MAX_PATH-wFileLen)-1,szBuffer);
	strcatn(szBuffer,"\\",sizeof(szBuffer));
	
	/* controlla la dimensione del buffer prima della copia (pathname+nomefile) */
	if((wLen = strlen(szBuffer))+wFileLen > _MAX_PATH)
		return(0);

	/* aggiunge al pathname il nome del file */
	strcatn(szBuffer,lpszFileName,sizeof(szBuffer));

	/* copia il contenuto del buffer nel nome file */
	strcpyn(lpszFileName,szBuffer,wFileLen);
		
	/* ritorna la lunghezza del nome file */
	return(strlen(lpszFileName));
}

/*
	AddRelativePath()
	
	Inserisce il pathname relativo nel nome del file, restituendone la nuova lunghezza.
	Il buffer contenente il nome del file deve essere di _MAX_PATH+1 caratteri.
*/
UINT CGzw::AddRelativePath(LPSTR lpszFileName,UINT wFileLen)
{
	LPSTR	lpPtr;
	char	szBuffer[_MAX_PATH+1] = {0};
	
	/* controlla la validita' del nome file */
	if(!*lpszFileName || wFileLen <= 0)
		return(0);

	/* ricava la directory corrente */
	::GetCurrentDirectory(sizeof(szBuffer)-2,szBuffer);
	strcatn(szBuffer,"\\",sizeof(szBuffer));

	/* cerca la directory corrente nel nome del file e la elimina */
	if((lpPtr = stristr(lpszFileName,szBuffer))!=NULL)
	{
		LPSTR lpBuf = szBuffer;

		lpPtr += strlen(szBuffer);

		while(*lpPtr)
			*lpBuf++ = *lpPtr++;

		*lpBuf = (char)NULL;

		strcpyn(lpszFileName,szBuffer,wFileLen);
	}

	/* ritorna la lunghezza del nome file */
	return(strlen(lpszFileName));
}

/*
	Diff()
	
	Controlla che i due nomi non facciano riferimento allo stesso file.
	La lunghezza dei nomi deve essere inferiore a _MAX_PATH caratteri.
	Utilizzata per non includere nel .gzw di output il .gzw stesso quando 
	viene passato *.* come input.
	
	Restituisce: 0 uguali
				 1 diversi
				 2 nome file non valido
				 3 lunghezza nome file eccessiva
*/
UINT CGzw::Diff(LPSTR lpszInFile,LPSTR lpszOutFile)
{
	size_t iInLen,iOutLen;
	size_t iStrcmp;
	char szInBuffer[_MAX_PATH+1] = {0};
	char szOutBuffer[_MAX_PATH+1] = {0};
	
	/* controlla la validita' dei nomi file */
	if(!*lpszInFile || !*lpszOutFile)
		return(2);
     
	/* calcola (e controlla) la lunghezza dei nomi file */
	if((iInLen = strlen(lpszInFile)) >= _MAX_PATH || (iOutLen = strlen(lpszOutFile)) >= _MAX_PATH)
		return(3);
	
	/* copia i nomi file nei buffer locali */
	strcpyn(szInBuffer,lpszInFile,sizeof(szInBuffer));
	strcpyn(szOutBuffer,lpszOutFile,sizeof(szOutBuffer));

	/* elimina l'eventuale pathname e confronta i nomi */
	if(StripPath(szInBuffer,iInLen)==StripPath(szOutBuffer,iOutLen))
		iStrcmp = stricmp(szInBuffer,szOutBuffer)==0 ? 0 : 1;
	else
		iStrcmp = 1;
	
	return(iStrcmp);
}

/*
	TmpName()
	
	Ritorna un nome di file unico, ubicandolo nella directory per i files temporanei.
*/
LPSTR CGzw::TmpName(LPSTR pTmpName,size_t nTmpSize)
{
	const char* lpTmpDir;
	    
	/* ricava la directory per i temporanei */
#ifdef DEBUG
	lpTmpDir = "C:\\TMP\\GZW\\TMP";
#else
	if((lpTmpDir = getenv("TMP"))==NULL)
	{
		if((lpTmpDir = getenv("TEMP"))==NULL)
			lpTmpDir = ".\\";
	}
#endif
	
	/* controlla l'esistenza della directory */
	if(access(lpTmpDir,06)!=0)
		lpTmpDir = ".\\";

	snprintf(	pTmpName,
				nTmpSize,
				"%s%s~gz%lld.tmp",
				lpTmpDir,
				lpTmpDir[strlen(lpTmpDir)-1]!='\\' ? "\\" : "",
				unix_timestamp()
			);

	return(pTmpName);
}

/*
	TmpClean()
	
	Ripulisce la directory temporanea degli eventuali files rimasti appesi.
*/
void CGzw::TmpClean(void)
{
	const char* lpTmpDir;

#ifdef DEBUG
	lpTmpDir = "C:\\TMP\\GZW\\TMP";
#else
	if((lpTmpDir = getenv("TMP"))==NULL)
	{
		if((lpTmpDir = getenv("TEMP"))==NULL)
			lpTmpDir = ".\\";
	}
#endif

	/* controlla l'esistenza della directory */
	if(access(lpTmpDir,06)!=0)
		lpTmpDir = ".\\";

	CFindFile findFile;
	LPSTR lpszInFile;
	char szTmpDir[_MAX_PATH+1] = {0};
	snprintf(	szTmpDir,
				sizeof(szTmpDir),
				"%s%s",
				lpTmpDir,
				lpTmpDir[strlen(lpTmpDir)-1]=='\\' ? "" : "\\");

	while((lpszInFile = (LPSTR)findFile.FindEx(szTmpDir,"~gz*.tmp",FALSE))!=NULL)
	{
		// pseudo-multitasking
		::Yield();

		// imposta il nome del file .gzw di input da decomprimere
		::DeleteFile(lpszInFile);
	}
}

/*
	EnsureOutDirExists()
*/
BOOL CGzw::EnsureOutDirExists(LPCSTR lpcszPath,BOOL bIsFilePath)
{
	char* p;
	DWORD dwError = 0L;
	char szDirectory[_MAX_PATH+1] = {0};
	size_t len = 0;

	// copia il percorso in locale
	strcpyn(szDirectory,lpcszPath,sizeof(szDirectory));

	// se il parametro indica che e' un file, bisogna isolare la directory
	if(bIsFilePath)
	{
		// verifica se si tratta di un percorso
		p = strrchr(szDirectory,'\\');

		// se trova un percorso elimina il nome file
		if(p)
		{
			// elimina l'ultimo \ ed il nome file
			*p = '\0';
		
			// se rimane solo il drive C: allora era una root, aggiunge quindi un backslash per ritornare a C:\
			// e ritorna, non deve creare nessuna directory
			if(isalpha(szDirectory[0]) && szDirectory[1]==':' && strlen(szDirectory)==2)
			{
				strcatn(szDirectory,"\\",sizeof(szDirectory));
				return(TRUE);
			}
		}
		// se non trova nessun \, allora e' solo un nome file, assume la dir corrente e ritorna, non deve creare 
		// nessuna directory
		else
			return(TRUE);
	}
	else
	{
		// e' una directory
		// rimuove lo slash finale se presente, solo se NON e' la root (es. "C:\")
		len = strlen(szDirectory);
		if (len > 3 && szDirectory[len-1] == '\\')
			szDirectory[len-1] = '\0';
	}

	// ora szDirectory contiene il percorso della cartella da verificare/creare
	if(!DoesDirectoryExist(szDirectory,&dwError))
		return(CreatePathname(szDirectory,&dwError));

	return(TRUE);
}

/*
	FindFileCallBackWrapper()

	Wrapper per la callback chiamata dalla Search() di CFindFile, chiama la callback reale tramite il 
	puntatore a <this> ripassato da CFindFile e impostato a suo tempo con la chiamata alla SetCallback 
	di CFindFile.
*/
LONG __stdcall CGzw::FindFileCallBackWrapper(WPARAM wParam,LPARAM lParam1,LPARAM lParam2,LPARAM lParam3)
{
	if(wParam!=NULL) // in wParam il ptr a this
	{
		CGzw* pThis = static_cast<CGzw*>(reinterpret_cast<void*>(wParam));
		if(pThis)
			return(pThis->FindFileCallBack(wParam,lParam1,lParam2,NULL));
	}
	
	return(1L); // come se non fosse successo nulla...
}

/*
	FindFileCallBack()

	Callback reale per la Search() di CFindFile, ritornando 1 aggiunge il file alla lista, con 0 lo scarta.
	Chiamata con il marchingegno di cui sopra via <this>.
	
	Il meccanismo viene implementato per far si che non sembri che e' rimasto appeso nella ricerca, dato che
	si tratta della ricerca che riempie la lista -la Search()-, che puo' metterci tempo per migliaia di files,
	non di quella che ritorna uno per uno i nomi -la FindEx()-.
*/
LONG CGzw::FindFileCallBack(WPARAM /*wParam*/,LPARAM lParam1,LPARAM lParam2,LPARAM lParam3)
{
	if((int)lParam2==0)
	{
		if(m_Gzw.fpCallBack)
			if(!m_Gzw.fpCallBack(GZW_CALLBACK_SEARCH_FILE_BEGIN,NULL,NULL,NULL))
				return(0L);
	}
	else if((int)lParam2==1)
	{
		FINDFILE* f = (FINDFILE*)lParam1;
		if(f)
		{
			if(m_Gzw.fpCallBack)
				if(!m_Gzw.fpCallBack(GZW_CALLBACK_SEARCH_FILE,(LPARAM)f,NULL,NULL))
					return(0L);

			// dato che la callback viene chiamata per ogni file trovato (possono quindi essere migliaia o milioni...), 
			// usa la lunghezza della stringa invece di testare la stringa con if(*m_Gzw.szWildcardsIncl)... perche' il 
			// test su un int e' molto piu' rapido che la deferenzazione di un puntatore, e chiaramente e' molto piu'
			// rapido rispetto a una chiamata a funzione (la CheckInclusions/Exclusions()) per verificare la stringa

			if(m_Gzw.nWildcardsInclLen > 0)
				// il file NON soddisfa le condizioni per essere INCLUSO, lo scarta
				if(CheckInclusions(f->name)==0L)
					return(0L);

			if(m_Gzw.nWildcardsExclLen > 0)
				// il file NON soddisfa le condizioni per essere ESCLUSO, lo scarta
				if(CheckExclusions(f->name)==0L)
					return(0L);
		}
	}
	else if((int)lParam2==2) // chiamata finale, non specifica nessun nome file
	{
		if(m_Gzw.fpCallBack)
			if(!m_Gzw.fpCallBack(GZW_CALLBACK_SEARCH_FILE_END,NULL,NULL,NULL))
				return(0L);
}

	// se passa i controlli di cui sopra, allora lo include
	return(1L);
}

/*
	check inclusione/esclusione:
	se FindEx() aggiungesse alla lista tutti i file che trova, qui si dovrebbe poi spulciare la lista intera per vedere 
	se i files passano il check con il pattern di incl/excl
	mette quindi il controllo nella callback che passa a CFindFile (ossia il controllo avviene dentro la Search(), che e'
	la funzione che riempie la lista di FindEx()), cosi' FindEx() si trovera' nella lista solo i files che soddisfano le 
	condizioni, evitando generare una lista enorme
*/

/*
	CheckInclusions()

	Scorre la lista delle inclusioni verificando se il file fa match (coincide) con almeno una.
	Notare che riceve il pathname completo, ma la verifica la compie sul pathname completo o (solo) sul nome 
	file a seconda dell'opzione corrente (-f/-F).
*/
LONG CGzw::CheckInclusions(LPCSTR lpcszFilename)
{
	if(!m_pitemListIncludePattern)
		return(1L);

	int nTot = m_pitemListIncludePattern->Count();
	if(nTot > 0)
	{
		BOOL bMatch = FALSE;
		ITERATOR iter = m_pitemListIncludePattern->First();
		while(iter!=(ITERATOR)NULL)
		{
			bMatch = FALSE;
			ITEM* pItem = (ITEM*)(iter->data);

			if(pItem)
			{
				// opzioni -f e -F: distingue se verificare il match solo con il nome file o con l'intero pathname
				const char* pMatchString = "";
				if(!m_Gzw.bWholeMatch)
					pMatchString = StripPathFromFile(lpcszFilename);
				else
					pMatchString = lpcszFilename;

				// quando crea la lista delle inclusioni con SplitPattern(), distingue se l'elemento (pItem->item)
				// contiene o meno una wildcard (?/*), impostando il flag relativo (pItem->flag)
				// in tal modo puo' selezionare automaticamente il tipo di verifica da effettuare, ossia quale dei
				// due membri chiamare (Match() o Matchsubstring()) a seconda se il pattern contiene wildcards o 
				// meno
				// nelle chiamate a Match()/Matchsubstring() rispettare l'ordine dei parametri: passare prima lo 
				// skeleton/wildcard e poi stringa da verificare
				if(pItem->flag==1)
				{
					if(!bMatch)
						bMatch = m_includePattern.Match(pItem->item,pMatchString);			// SI puo' contenere caratteri jolly
				}
				else if(pItem->flag==0)
				{
					if(!bMatch)
						bMatch = m_includePattern.MatchSubString(pItem->item,pMatchString);	// NON puo' contenere caratteri jolly, farebbe fallire strstr()
				}
			}

			// ottiene un match tra il nome del file e uno degli elementi della lista delle inclusioni, aggiunge quindi il file
			if(bMatch)
				return(1L);

			iter = m_pitemListIncludePattern->Next(iter);
		}
	}

	// non c'e' stato nessuna match tra il nome del file e gli elementi della lista delle inclusioni, scarta quindi il file
	return(0L);
}

/*
	CheckExclusions()
	
	Scorre la lista delle esclusioni verificando se il file fa match (coincide) con almeno una.
	Notare che riceve il pathname completo, ma la verifica la compie sul pathname completo o (solo) sul nome 
	file a seconda dell'opzione corrente (-f/-F).

	Esempio pratico:
	si vuole comprimere la directory C:\DEV\BerkeleyDB, lasciando fuori tutti i file presenti nelle subdir 
	C:\DEV\BerkeleyDB\build_win32\Debug/Release

	specificando come esclusione "Debug;Release" con il flag -f, che considera solo il nome file, non verrebbe
	escluso nessun file, dato che "Debug" e "Release" sono nomi di directory, non di file, mentre il flag -f 
	obbliga a cercare solo nel nome file

	ora, specificando come esclusione "Debug;Release" con il flag -F, che si' considera l'intero pathname, oltre 
	ad escludere le due subdir, escluderebbe anche tutti i files che eventualmente contengano le due sottostringhe, 
	ad esempio il file C:\DEV\BerkeleyDB\build_win32\copydebug.bat, perche' la ricerca, quando non sono presenti 
	wildcards, avviene con strstr() (vedi differenza tra Match() e MatchSubString()) ricercando nell' intero 
	pathname (grazie a -F)

	quindi, per escludere tutto il contenuto delle subdir "Debug" e "Release", senza escludere files o altri 
	pathnames che contengano "Debug" e/o "Release" come sottostringa, bisogna specificare le esclusioni con 
	il backslash, in modo che vengano 'catturate' solo le directory ed i files in esse contenuti (ovviamente 
	usando il flag -F):

	-r -O -P -F -c -iC:\DEV\BerkeleyDB\*.* -oC:\TMP\DTCOPY\OUTPUT\out.gzw -W\Debug\;\Release\
*/
LONG CGzw::CheckExclusions(LPCSTR lpcszFilename)
{
	if(!m_pitemListExcludePattern)
		return(1L);

	int nTot = m_pitemListExcludePattern->Count();
	if(nTot > 0)
	{
		BOOL bMatch = FALSE;
		ITERATOR iter = m_pitemListExcludePattern->First();
		while(iter!=(ITERATOR)NULL)
		{
			bMatch = FALSE;
			ITEM* pItem = (ITEM*)(iter->data);

			if(pItem)
			{
				// opzioni -f e -F: distingue se verificare il match solo con il nome file o con l'intero pathname
				const char* pMatchString = "";
				if(!m_Gzw.bWholeMatch)
					pMatchString = StripPathFromFile(lpcszFilename);
				else
					pMatchString = lpcszFilename;

				// quando crea la lista delle esclusioni con SplitPattern(), distingue se l'elemento (pItem->item)
				// contiene o meno una wildcard (?/*), impostando il flag relativo (pItem->flag)
				// in tal modo puo' selezionare automaticamente il tipo di verifica da effettuare, ossia quale dei
				// due membri chiamare (Match() o Matchsubstring()) a seconda se il pattern contiene wildcards o 
				// meno
				// nelle chiamate a Match()/Matchsubstring() rispettare l'ordine dei parametri: passare prima lo 
				// skeleton/wildcard e poi stringa da verificare
				if(pItem->flag==1)
				{
					if(!bMatch)
						bMatch = m_excludePattern.Match(pItem->item,pMatchString);			// SI puo' contenere caratteri jolly
				}
				else if(pItem->flag==0)
				{
					if(!bMatch)
						bMatch = m_excludePattern.MatchSubString(pItem->item,pMatchString);	// NON puo' contenere caratteri jolly, farebbe fallire strstr()
				}

				// ottiene un match tra il nome del file e uno degli elementi della lista delle esclusioni, scarta quindi il file			
				if(bMatch)
					return(0L);
			}

			iter = m_pitemListExcludePattern->Next(iter);
		}
	}

	// non c'e' stato nessuna match tra il nome del file e gli elementi della lista delle esclusioni, include quindi il file
	return(1L);
}

/*
	ProgressCallbackWrapper()

	Wrapper per la callback chiamata dalla zLib durante le operazioni di I/O su file, chiama 
	la callback reale tramite il puntatore a <this> ripassato impostato in precedenza.
*/
void CGzw::ProgressCallbackWrapper(void* pContext,int nPercent,int nAction)
{
    // recupera l'istanza CGzw dall'indirizzo passato
    CGzw* pThis = static_cast<CGzw*>(pContext);
    
    // chiama il metodo non statico per aggiornare il progresso
    pThis->ProgressCallback(nPercent,nAction);
}

/*
	ProgressCallback()

	Callback reale per la zLib.
	Chiamata con il marchingegno di cui sopra via <this>.
*/
void CGzw::ProgressCallback(int nPercent,int nAction)
{
	// qui avviene la chiamata per GZW_CALLBACK_COMPRESS e GZW_CALLBACK_UNCOMPRESS con la 
	// percentuale di avanzamento, ricevuta dal codice della zLib
	// nel resto del codice (in questo modulo) solo usa GZW_CALLBACK_[...]_BEGIN/_END/_TOTAL
	// per GZW_CALLBACK_LIST non viene ricevuta nessuna chiamata dalla zLib dato che
	// l'elaborazione avviene qui
	// ignora l'eventuale halt ricevuto dalla callback (quando ritorna 0) perche' non deve
	// interrompere l'elaborazione nella zLib per non pregiudicare l'I/O sul file corrente
	if(m_Gzw.fpCallBack)
		if(!m_Gzw.fpCallBack(nAction,(LPARAM)&m_Gzw,(LPARAM)&m_GzwHeader,(LPARAM)nPercent))
			;
}

/*
	IsCompressedFile()

	Verifica se il file e' in formato compresso in base all'estensione.
*/
BOOL CGzw::IsCompressedFile(LPCSTR lpcszFilename)
{
	if(!lpcszFilename)
		return(FALSE);

	// cerca l'inizio dell'estensione
	const char* ext = strrchr(lpcszFilename,'.'); 
    
	// senza estensione o estensione troppo corta
	if(!ext || ext[1]=='\0')
		return(FALSE);

	// cerca l'estensione nell'array
	for(int i=0; i < ARRAY_SIZE(aCompressedExtensions); i++)
		if(stricmp(ext,aCompressedExtensions[i])==0)
			return(TRUE);

	return(FALSE);
}

/*
	GzwCallback()

	Callback per le funzioni di compressione/decompressione/lista.
	Non e' un membro della classe (CGzw) ma semplicemente il codice per gestire le callback
	della classe reindirizzando l'output (il progresso) su stdout.
*/
LONG GzwCallback(WPARAM wParam,LPARAM lParam1,LPARAM lParam2,LPARAM lParam3)
{
	static int halted = 0;
	if(wParam==GZW_CALLBACK_HALT)
		halted = 1;
	if(halted)
		return(0L);

	static int header = 0;

	// RICERCA FILES, va a parte
	// wParam  = id callback
	// lParam1 = ptr a FINDFILE*/NULL
	// lParam2 = NULL
	// lParam3 = NULL
	if(wParam==GZW_CALLBACK_SEARCH_FILE_BEGIN || wParam==GZW_CALLBACK_SEARCH_FILE || wParam==GZW_CALLBACK_SEARCH_FILE_END)
	{
		static int wide = 0;
		static int nTotFiles = 0;

		static long nUpdateInterval = 1;
		static long nLastUpdateThreshold = 0; 

		if(wParam==GZW_CALLBACK_SEARCH_FILE_BEGIN)
		{
			nTotFiles = 0;
		}
		else if(wParam==GZW_CALLBACK_SEARCH_FILE)
		{
			nTotFiles++;

			if(nTotFiles < 100)
				nUpdateInterval = 1; 
			else if(nTotFiles < 1000)
				nUpdateInterval = 16;
			else if(nTotFiles < 10000)
				nUpdateInterval = 128;
			else
				nUpdateInterval = 256;

			if(nTotFiles >= (nLastUpdateThreshold + nUpdateInterval))
			{
				wide = printf("\rsearching... (%ld files)", nTotFiles);
				nLastUpdateThreshold = nTotFiles;
			}
		}
		else if(wParam==GZW_CALLBACK_SEARCH_FILE_END)
		{
			printf("\r%*s\r",wide+1," ");
			nTotFiles = 0;
			wide = 0;

			nUpdateInterval = 1;
			nLastUpdateThreshold = 0; 
		}

		return(1L);
	}

	// COPIA TEMPORANEO, va a parte
	// wParam  = id callback
	// lParam1 = valore % progresso
	// lParam2 = ptr const char* a nome file
	// lParam3 = NULL
	if(wParam==GZW_CALLBACK_UNCOMPRESS_TMP)
	{
		static int wide = 0;
		wide = printf(	"\rcopying to temp file: %s (%d%%)",
						(LPCSTR)lParam2,
						(int)lParam1
						);

		if((int)lParam1==100)
		{
			printf("\r%*s\r",wide+1," ");
			wide = 0;
		}

		return(1L);
	}

	// TOTALI GLOBALI, va a parte
	// wParam  = id callback
	// lParam1 = ptr a GZW*
	// lParam2 = ptr a GZWHDR*
	// lParam3 = NULL
	if(wParam==GZW_CALLBACK_COMPRESS_TOTAL || wParam==GZW_CALLBACK_UNCOMPRESS_TOTAL || wParam==GZW_CALLBACK_LIST_TOTAL)
	{
		GZW* pGzw = (GZW*)lParam1;

		if(pGzw && pGzw->dwTotFiles > 0)
		{
			// n -> n,nnn,nnn.nn
			char szTotSize[32] = {0};
			char szTotSizeCompr[32] = {0};
			qwtos(pGzw->qwTotFilesSize,szTotSize,sizeof(szTotSize));
			qwtos(pGzw->qwTotCompressedFilesSize,szTotSizeCompr,sizeof(szTotSizeCompr));
			QWORD qwSaved = pGzw->qwTotFilesSize - pGzw->qwTotCompressedFilesSize;

			// n -> bytes/KB/MB/GB
			char szTotSizeFmt[32] = {0};
			char szTotSizeComprFmt[32] = {0};
			char szTotSavedFmt[32] = {0};
			strsizefmt(szTotSizeFmt,sizeof(szTotSizeFmt),(double)pGzw->qwTotFilesSize);
			strsizefmt(szTotSizeComprFmt,sizeof(szTotSizeComprFmt),(double)pGzw->qwTotCompressedFilesSize);
			strsizefmt(szTotSavedFmt,sizeof(szTotSavedFmt),(double)qwSaved);


			if(wParam==GZW_CALLBACK_COMPRESS_TOTAL || wParam==GZW_CALLBACK_LIST_TOTAL)
				printf(	"\n%ld files, total original size %s (%s), compressed %s (%s), %d%% (%s) saved\n\n",
						pGzw->dwTotFiles,
						szTotSize,
						szTotSizeFmt,
						szTotSizeCompr,
						szTotSizeComprFmt,
						(int)llrint(100.0 - (100.0 * pGzw->qwTotCompressedFilesSize) / pGzw->qwTotFilesSize),
						szTotSavedFmt
						);
			else if(wParam==GZW_CALLBACK_UNCOMPRESS_TOTAL)
				//printf(	"\n%ld files, total compressed size %s (%s), uncompressed %s (%s), %d%% saved\n\n",
				printf(	"\n%ld files, total compressed size %s (%s), uncompressed %s (%s)\n\n",
						pGzw->dwTotFiles,
						szTotSizeCompr,
						szTotSizeComprFmt,
						szTotSize,
						szTotSizeFmt/*,
						(int)llrint(100.0 - (100.0 * pGzw->qwTotCompressedFilesSize) / pGzw->qwTotFilesSize)*/
						);
		}

		header = 0;

		return(1L);
	}

	// COMPRESS, UNCOMPRESS, LIST: a partire da qui in lParam1 il ptr all'header GZWHDR
	// wParam  = id callback
	// lParam1 = ptr a GZW*
	// lParam2 = ptr a GZWHDR*
	// lParam3 = [...]
	GZW* gzw = (GZW*)lParam1;
	GZWHDR* gzwhdr = (GZWHDR*)lParam2;

	if(gzw && gzwhdr)
	{
		char szqwFileSize[32] = {0};
		char szqwFileCompressedSize[32] = {0};
		TAGGEDVALUE tv;
		memset(&tv,'\0',sizeof(tv));

		tv.valuetype = QWORD_TYPE;
		tv.value.qwValue = gzwhdr->stGzwHdr.qwFileSize;
		strnumfmt(&tv,szqwFileSize,sizeof(szqwFileSize));

		tv.valuetype = QWORD_TYPE;
		tv.value.qwValue = gzwhdr->stGzwHdr.qwFileCompressedSize;
		strnumfmt(&tv,szqwFileCompressedSize,sizeof(szqwFileCompressedSize));

		// COMPRESS
		if(wParam==GZW_CALLBACK_COMPRESS_BEGIN || wParam==GZW_CALLBACK_COMPRESS || wParam==GZW_CALLBACK_COMPRESS_END)
		{
			static int nRatio = 0;

			if(wParam==GZW_CALLBACK_COMPRESS_BEGIN)
			{
				if(!header)
				{
					header = 1;
					printf(	"\r %s\n\n%14s %14s %4s\n",
							gzw->szOutputFile,
							"original size",
							"compressed",
							"saved"
							);
				}

				nRatio = (int)lParam3;
				printf("\r%s: %s (%d%%)",nRatio==0 ? "storing" : "adding",gzwhdr->pFileName,0);
			}
			else if(wParam==GZW_CALLBACK_COMPRESS)
			{
				int nProgress = (int)lParam3;
				printf("\r%s: %s (%d%%)",nRatio==0 ? "storing" : "adding",gzwhdr->pFileName,nProgress);
			}
			else if(wParam==GZW_CALLBACK_COMPRESS_END)
			{
				char szTotSizeFmt[32] = {0};
				char szTotSizeComprFmt[32] = {0};
				char szSave[32] = {0};
				snprintf(szSave,sizeof(szSave),"%d%%",(int)llrint(100.0 - (100.0 * gzwhdr->stGzwHdr.qwFileCompressedSize) / gzwhdr->stGzwHdr.qwFileSize));
				strsizefmt(szTotSizeFmt,sizeof(szTotSizeFmt),(double)gzwhdr->stGzwHdr.qwFileSize);
				strsizefmt(szTotSizeComprFmt,sizeof(szTotSizeComprFmt),(double)gzwhdr->stGzwHdr.qwFileCompressedSize);
				printf(	"\r%14s %14s %4s %s\n",
						szTotSizeFmt,
						szTotSizeComprFmt,
						szSave,
						gzwhdr->pFileName
						);

				nRatio = 0;
			}
		}
		// UNCOMPRESS
		else if(wParam==GZW_CALLBACK_UNCOMPRESS_BEGIN || wParam==GZW_CALLBACK_UNCOMPRESS || wParam==GZW_CALLBACK_UNCOMPRESS_END)
		{
			static char szName[_MAX_PATH+1] = {0};
			static int nProgress = 0;
			static int wide = 0;

			if(wParam==GZW_CALLBACK_UNCOMPRESS_BEGIN)
			{
				if(!header)
				{
					header = 1;
					printf(	"\r %s\n\n%14s %14s %4s\n",
							gzw->szInputFile,
							"compressed",
							"uncompressed",
							"saved"
							);
				}

				strcpyn(szName,gzw->szInputFile,sizeof(szName));
				wide = printf("\rextracting: %s",szName);
			}
			else if(wParam==GZW_CALLBACK_UNCOMPRESS)
			{
				nProgress = (int)lParam3;
				wide = printf("\rextracting: %s (%d%%)",szName,nProgress);
			}
			else if(wParam==GZW_CALLBACK_UNCOMPRESS_END)
			{
				char szTotSizeFmt[32] = {0};
				char szTotSizeComprFmt[32] = {0};
				char szSave[32] = {0};
				snprintf(szSave,sizeof(szSave),"%d%%",(int)llrint(100.0 - (100.0 * gzwhdr->stGzwHdr.qwFileCompressedSize) / gzwhdr->stGzwHdr.qwFileSize));
				strsizefmt(szTotSizeFmt,sizeof(szTotSizeFmt),(double)gzwhdr->stGzwHdr.qwFileSize);
				strsizefmt(szTotSizeComprFmt,sizeof(szTotSizeComprFmt),(double)gzwhdr->stGzwHdr.qwFileCompressedSize);
				printf("\r%*s\r",wide+1," ");
				printf(	"\r%14s %14s %4s %s\n",
						szTotSizeComprFmt,
						szTotSizeFmt,
						szSave,
						gzwhdr->pFileName
						);

				memset(szName,'\0',sizeof(szName));
				nProgress = 0;
				wide = 0;
			}
		}
		// LIST
		else if(wParam==GZW_CALLBACK_LIST || wParam==GZW_CALLBACK_LIST_BEGIN || wParam==GZW_CALLBACK_LIST_END)
		{
			static int wide = 0;

			if(wParam==GZW_CALLBACK_LIST_BEGIN)
			{
				if(!header)
				{
					header = 1;
					printf(	"\r %s\n\n%14s %14s %4s\n",
							gzw->szInputFile,
							"original size",
							"compressed",
							"saved"
							);
				}
			}
			else if(wParam==GZW_CALLBACK_LIST)
			{
				char szTotSizeFmt[32] = {0};
				char szTotSizeComprFmt[32] = {0};
				char szSave[32] = {0};
				snprintf(szSave,sizeof(szSave),"%d%%",(int)llrint(100.0 - (100.0 * gzwhdr->stGzwHdr.qwFileCompressedSize) / gzwhdr->stGzwHdr.qwFileSize));
				strsizefmt(szTotSizeFmt,sizeof(szTotSizeFmt),(double)gzwhdr->stGzwHdr.qwFileSize);
				strsizefmt(szTotSizeComprFmt,sizeof(szTotSizeComprFmt),(double)gzwhdr->stGzwHdr.qwFileCompressedSize);
				printf("\r%*s\r",wide+1," ");
				printf(	"\r%14s %14s %4s %s\n",
						szTotSizeFmt,
						szTotSizeComprFmt,
						szSave,
						gzwhdr->pFileName
						);
			}
			else if(wParam==GZW_CALLBACK_LIST_END)
			{
				wide = 0;
				header = 0;
			}
		}
	}

	return(1L);
}
