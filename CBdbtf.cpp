/*$
	CBerkeleyDBTransactionsFallback.cpp
	Classi per sostituire il gestore originale della libreria BerkeleyDB per le transazioni (BDBTF).
	Luca Piergentili, Oct '25

	Nota: per disabilitare il warning del compilatore, andare su proprieta' del progetto ed impostare 
	l'opzione: C/C++ -> Code Generation -> Enable C++ Exceptions -> "Yes with SEH Exceptions (/EHa)"
*/
#include "env.h"
#include "pragma.h"
#include "macro.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "strings.h"
#include "typedef.h"
#include "window.h"
#include <process.h>
#include <eh.h>
#include "CFindFile.h"
#include "CBdbtf.h"

#include "traceexpr.h"
//#define _TRACE_FLAG			_TRFLAG_TRACEOUTPUT // opzioni: _TRFLAG_NOTRACE, _TRFLAG_TRACEFILE, _TRFLAG_TRACECONSOLE, _TRFLAG_TRACEOUTPUT, _TRFLAG_TRACEBREAKPOINT
#define _TRACE_FLAG			_TRFLAG_NOTRACE // opzioni: _TRFLAG_NOTRACE, _TRFLAG_TRACEFILE, _TRFLAG_TRACECONSOLE, _TRFLAG_TRACEOUTPUT, _TRFLAG_TRACEBREAKPOINT
#define _TRACE_FLAG_INFO	_TRACE_FLAG
#define _TRACE_FLAG_WARN	_TRACE_FLAG
#define _TRACE_FLAG_ERR		_TRACE_FLAG

/*
	THREAD_CONTEXT

	struttura per il passaggio dei parametri necessari per il thread tramite la funzione statica
*/
struct THREAD_CONTEXT {
    CBerkeleyDBTransactionsFallback* pInstance;	// il puntatore 'this'
    BDBTF_THREAD_PARAMS* pThreadParams;			// i parametri originali
};

/*
	SehTranslator()

	Funzione per il 'traduttore': converte l'errore SEH in un'eccezione C++.
*/
void SehTranslator(unsigned int u,EXCEPTION_POINTERS* pExp)
{
    // rilancia l'eccezione come un oggetto C++ standard
    throw SehException(u);
}

/*
	Startup()

	Apertura tabelle con timeout (tramite il lancio del thread) e logica ripristino per errore.

	Restituisce 0 se ok o 1 per reinizio necessario.
	In caso di errore restituisce: -1, -2, -3, -4 (propri) e -5, -6, -7, -8, -9 (del thread).
*/
int CBerkeleyDBTransactionsFallback::Startup(BDBTF_THREAD_PARAMS* pParams)
{
    unsigned int uThreadID = 0;
    HANDLE hThreadHandle = 0;
    DWORD dwWaitResult = 0L;
    int nReturnCode = 0;

	// la struttura che contiene i dati da passare al thread deve essere allocata dinamicamente se il thread
	// ha vita propria e la funzione che lo lancia non aspetta la sua conclusione, in tal caso se fosse
	// allocata sullo stack locale, al terminare la funzione, la memoria dello stack si perderebbe, invalidando
	// i dati della struttura passati al thread
	// qui il codice che lancia il thread, al contrario, aspetta che il thread finisca, pero' si usa lo stesso
	// la allocazione per buona pratica
	// della delete relativa si incarica il codice qui sotto, dato che il thread potrebbe rimanere appeso
	THREAD_CONTEXT* pContext = new THREAD_CONTEXT;
	if(pContext==NULL)
        return(-1);
	pContext->pInstance = this;
	pContext->pThreadParams = pParams;

    // avvia il thread per l'apertura del database
	hThreadHandle = (HANDLE)_beginthreadex(	NULL,				// attributi di sicurezza
											0,					// dimensione dello stack
											OpenDatabaseFiles,	// funzione (statica) per il thread
											pContext,			// parametri da passare al thread
											0,					// flag di creazione
											&uThreadID			// ID del thread
											);

	// lancio fallito
    if(hThreadHandle==NULL)
        return(-2);

    // attende la fine del thread via timeout
	dwWaitResult = WaitForSingleObject(hThreadHandle,pParams->dwTimeout);
	
	if(dwWaitResult==WAIT_TIMEOUT)
    {
		/*
			se le tabelle hanno scasinato, normalmente le funzioni della BerkeleyDB rimangono appese
			senza generare un eccezione, per cui deve segare le gambe al thread dall'esterno (il thread
			rimane anche lui appeso aspettando la risposta della libreria)
			per questo bisogna usare TerminateThread() (Win32) e non _endthreadex() (CRT)
			la funzione _endthreadex (della CRT) e' la controparte pulita di _beginthreadex, ed il suo 
			scopo e' permettere al thread di terminare in modo controllato e pulito, eseguendo la 
			deallocazione delle strutture interne della CRT (liberando memoria, chiudendo flussi, ecc.) 
			prima di uscire
			pero qui se il thread e' rimasto appeso con la libreria BerkeleyDB, non raggiungera' mai la 
			chiamata a _endthreadex, chiamandola dal watchdog del thread il compilatore darebbe errore 
			perche' solo si puo' chimare dall'interno del thread stesso
		*/
        TerminateThread(hThreadHandle,0);
        
		// la apertura delle tabelle ha fallito, esegue quindi il ripristino: sovrascrive la directory di 
		// produzione con la copia di backup
		for(int i=0; i < pParams->nArrayCount; i++)
			if(CopyDatabaseFiles(pParams->bdbtfArray[i].szBackupDir,pParams->bdbtfArray[i].szProductionDir))
			{
				nReturnCode = 1; // riavvio necessario
			}
			else
			{
				nReturnCode = -3; // errore critico, impossibile ripristinare il database
			}
    }
    else if(dwWaitResult==WAIT_OBJECT_0)
    {
        // il thread ha terminato con successo (tabelle aperte), verifica il codice di ritorno BDB
        if(pParams->nResult==0)
        {
            nReturnCode = 0;
        }
		// il thread ha fallito nell'apertura
        else
        {
            nReturnCode = pParams->nResult; 
        }
    }
	else // gestione altri errori di WaitForSingleObject
	{
		nReturnCode = -4;
    }
    
    CloseHandle(hThreadHandle);

	delete pContext;

	return(nReturnCode);
}

/*
	Backup()

	Salvataggio del database (da directory di produzione a directory di backup).
*/
bool CBerkeleyDBTransactionsFallback::Backup(const char* pProductionDir,const char* pBackupDir)
{
	return(CopyDatabaseFiles(pProductionDir,pBackupDir));
}

/*
	CopyDatabaseFiles()

	Copia i files del database da una directory all'altra, usata indifferentemente per backup o per ripristino.
*/
bool CBerkeleyDBTransactionsFallback::CopyDatabaseFiles(const char* pSourceDir,const char* pDestDir)
{
	bool bRet = TRUE; // nessuna copia = nessun file presente = non errore

	CFindFile findFile;
	char szSkeleton[_MAX_FILEPATH+1] = {0};
	char szSourcePath[_MAX_FILEPATH+1] = {0};
	char szDestPath[_MAX_FILEPATH+1] = {0};
	const char* pFilename;

	// considera tutti i files nella directory specificata (produzione/backup)
	snprintf(szSkeleton,sizeof(szSkeleton),"%s\\*.*",pSourceDir);

	// per ognuno dei files trovati
	while((pFilename = findFile.Find(szSkeleton))!=NULL)
	{
		strcpyn(szSourcePath,pFilename,sizeof(szSourcePath));
		snprintf(szDestPath,sizeof(szDestPath),"%s\\%s",pDestDir,StripPathFromFile(pFilename));

		// copia il file da ... in ...
		if(!CopyFileTo(NULL,szSourcePath,szDestPath,FALSE,FALSE))
			bRet = FALSE;
	}

	return(bRet);
}

/*
	OpenDatabaseFiles()

	Thread per l'apertura delle tabelle.

	Se la tabella e' corrotta, le funzioni della libreria BerkeleyDB possono rimanere appese o provocare un eccezione,
	quindi incapsula l'apertura nel gestore delle eccezioni (C++ e SEH) per evitare di rimanere appeso (il chiamante
	comunque sia lo farebbe fuori) e per evitare un crash x eccezione (il chiamante crascherebbe pure lui, invalidando
	completamente il meccanismo di salvataggio/ripristino delle tabelle).
*/
unsigned __stdcall CBerkeleyDBTransactionsFallback::OpenDatabaseFiles(void* lpParam)
{
    // cast al tipo di contesto
    THREAD_CONTEXT* pContext = (THREAD_CONTEXT*)lpParam;
    
    // controlli di validita'
    if(!pContext || !pContext->pInstance || !pContext->pThreadParams)
    {
		_endthreadex((unsigned int)-5);
		//return((unsigned int)-5);
    }

    // chiama il metodo non-statico per l'esecuzione 'reale' della logica delle operazioni
    unsigned int nRet = pContext->pInstance->OpenDatabaseFiles(pContext->pThreadParams);

	// ogni thread iniziato con _beginthreadex deve concludersi, internamente, con _endthreadex
	_endthreadex(nRet);

    return(nRet);
}

/*
	OpenDatabaseFiles()

	Versione 'reale' (non statica) del thread.
*/
unsigned int CBerkeleyDBTransactionsFallback::OpenDatabaseFiles(BDBTF_THREAD_PARAMS* pParams)
{
	unsigned int nRet = 0;

	// riceve in input (l'indirizzo del) puntatore all'oggetto per la tabella (bdbtfArray[i].ppDB, vedi sotto) 
	// usa una copia locale temporanea per poter eliminare l'oggetto in proprio se qualcosa va male
	// se tutto va bene, allora imposta il puntatore ricevuto dal chiamante con l'indirizzo dell'oggetto allocato qui
    CDBServiceBase* pDB_Local = NULL;

	// imposta il 'traduttore' per catturare le eccezioni di sistema e lanciarle come eccezioni C++, notare che la
	// impostazione e' locale (ossia circoscritta) al thread, quindi una volta terminato il codice relativo, il
	// meccanismo ritorna automaticamente ai gestori di default
	_set_se_translator(SehTranslator);

	/*	try/catch e' solo per le eccezioni C++, le eccezioni strutturate (ossia di sistema) vengono gestite tramite il 
		Structured Exception Handling (SEH), usando la sintassi __try / __except di Microsoft, e non vengono viste dai 
		blocchi try-catch del C++
		dato che qui si sta usando C++ ed il codice e' gia' avvolto in un try-catch (...), il modo piu' diretto per 
		intercettare l'errore SEH e' usare la funzione di hook fornita da Microsoft, la _set_se_translator(), ed a tale
		scopo bisogna implementare una funzione che catturi l'eccezione SEH e la 'rilanci' come un'eccezione C++ standard
		(vedi sopra la classe SehException e la funzione SehException) ed installare il meccanismo tramite una sola chiamata
		alla funzione Win32 _set_se_translator(SehTranslator), a questo punto il codice puo' catturare l'eccezione SEH 
		tradotta in C++
	*/
	BDBTF_FACTORY_ITEM* bdbtfArray = pParams->bdbtfArray;

	// per ognuna delle tabelle (classi B) presenti nell'array
	for(int i=0; i < pParams->nArrayCount && bdbtfArray[i].pfnCreate!=NULL; i++)
	{
		nRet = 0;

		try {
			// prova ad aprire la tabella: sta' chiamando la funzione factory stabilita dal chiamante
			// deve essere una diversa per ogni oggetto (ossia per ogni tabella)
			pDB_Local = bdbtfArray[i].pfnCreate(bdbtfArray[i].szTableName,bdbtfArray[i].szProductionDir);

			// verifica che creazione/apertura sia andata a buon fine
			if(!pDB_Local)
				nRet = (unsigned int)-6;

			// chiama la funzione factory per il check sulla tabella
			if(nRet==0 && pDB_Local)
				nRet = bdbtfArray[i].pfnCheck(pDB_Local)==TRUE ? 0 : (unsigned int)-7;

			// imposta il puntatore fornito dal chiamante con l'oggetto di cui sopra
			if(nRet!=0)
			{
				if(pDB_Local)
					delete pDB_Local;
				*(bdbtfArray[i].ppDB) = NULL;
			}
			else
				*(bdbtfArray[i].ppDB) = pDB_Local;
		
		}
		catch (SehException& e) { // intercetta le violazioni SEH

				if(pDB_Local)
					delete pDB_Local;
				*(bdbtfArray[i].ppDB) = NULL;

				nRet = e.nCode;
				nRet = (unsigned int)-8;
		}
		catch (...) { // intercetta le violazioni C++

				if(pDB_Local)
					delete pDB_Local;
				*(bdbtfArray[i].ppDB) = NULL;

				nRet = (unsigned int)-9;
		}
	}

/*	// per provare il fallimento: fa scattare il timeout del chiamante per uccidere il thread   
	unsigned long l=0L;
	while(1)
	{
		Sleep(1000);
		printf("\rwasting time... (%ld)",++l);
	} */

	pParams->nResult = nRet;

	return(nRet);
}
