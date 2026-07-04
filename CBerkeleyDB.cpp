/*$
	CBerkeleyDB.cpp
	Classe base per l'interfaccia con la libreria Berkeley DB 2.7.7 (http://www.sleepycat.com).
	Luca Piergentili, 04/11/99
	lpiergentili@yahoo.com

	Vedi le note in CBerkeleyDB.h.

	Note tecniche:

	- le transazioni falliscono miseramente con la BDB versione 2.7.7, quindi per preservare l'integrita' del
	  DB viene implementata la classe CBerkeleyDBTransactionsFallback() (vedi files CBdbtf.cpp/.h) che cerca 
	  di supplire il problema con un workaround un poco bruto ma efficace

	- quando ci si riferisce alla chiave primaria ('assoluta', 'nascosta', etc.) si tratta della chiave numerica
	  generata automaticamente quando la tabella prevede indici, per poter gestire chiavi duplicate sugli indici
	  secondari

	- le operazioni che modificano le tabelle, usano gli handle se in modo transazionale, o i cursori altrimenti

	- in modo transazionale si usa la commit, altrimenti le Sync...(), non mescolare i due metodi

	- i codice d'errore vengono gestiti con codice numerico
*/
#include "env.h"
#include "pragma.h"
#include "macro.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "strings.h"
#include <stdbool.h>
#include <errno.h>
#if defined(_WINDOWS)
  #include "window.h"
#endif
#include "typedef.h"
#include "traceexpr.h"
#include "db.h"
#include "CBerkeleyDB.h"

#include "traceexpr.h"
//#define _TRACE_FLAG			_TRFLAG_TRACEOUTPUT // opzioni: _TRFLAG_NOTRACE, _TRFLAG_TRACEFILE, _TRFLAG_TRACECONSOLE, _TRFLAG_TRACEOUTPUT, _TRFLAG_TRACEBREAKPOINT
#define _TRACE_FLAG			_TRFLAG_NOTRACE // opzioni: _TRFLAG_NOTRACE, _TRFLAG_TRACEFILE, _TRFLAG_TRACECONSOLE, _TRFLAG_TRACEOUTPUT, _TRFLAG_TRACEBREAKPOINT
#define _TRACE_FLAG_INFO	_TRACE_FLAG
#define _TRACE_FLAG_WARN	_TRACE_FLAG
#define _TRACE_FLAG_ERR		_TRACE_FLAG

// array per codici d'errore, mantenere allineato con definizioni nel .h
struct DB_ERROR {
	int code;
	const char* desc;
};
static const DB_ERROR db_error[] = {
	DB_NO_ERROR,						"No error",	

	// 1000 (info)
	DB_OK,								"Ok",

	// 2000 (status)
	DB_RETCODE_NOTFOUND,				"Not found",
	DB_RETCODE_BOF,						"Bof",
	DB_RETCODE_EOF,						"Eof",

	// 3000 (errori)
	DB_RETCODE_ECREATETABLE,			"Error creating the table",
	DB_RETCODE_EOPENTABLE,				"Error opening the table",
	DB_RETCODE_EOPENCURSOR,				"Error opening the cursor",
	DB_RETCODE_EOPENINDEX,				"Error opening the index",
	DB_RETCODE_EOPENIDXCURSOR,			"Error opening the cursor (index)",
	DB_RETCODE_EOPENDEFINITION,			"Error opening the definition file",

	DB_RETCODE_EALREADYOPEN,			"Table is already open",
	DB_RETCODE_ETABLENOTOPEN,			"Table not open",
	DB_RETCODE_EINDEXREQUIRED,			"Index required",

	DB_RETCODE_ECLOSETABLE,				"Error closing table",
	DB_RETCODE_ECLOSECURSOR,			"Error closing cursor",
	DB_RETCODE_ECLOSEINDEX,				"Error closing index",
	DB_RETCODE_ECLOSEIDXCURSOR,			"Error closing cursor (index)",

	DB_RETCODE_EINVALIDOBJECT,			"Invalid object",
	DB_RETCODE_EINVALIDHANDLE,			"Invalid handle",
	DB_RETCODE_EINVALIDPARAM,			"Invalid parameter",
	DB_RETCODE_EINVALIDRESOURCE,		"Invalid resource",
	DB_RETCODE_EINVALIDCURSOR,			"Invalid cursor",
	DB_RETCODE_EINVALIDINDEX,			"Invalid index",

	DB_RETCODE_EINVALIDFIELDNUMBER,		"Invalid field number",
	DB_RETCODE_EINVALIDFIELDSIZE,		"Invalid field size",
	DB_RETCODE_EINVALIDFIELDTYPE,		"Invalid field type",

	DB_RETCODE_EINVALIDPRIMARYKEYSIZE,	"Invalid primary key size",

	DB_RETCODE_EOUTOFMEMORY,			"Out of memory",

	DB_RETCODE_ELOCKFAILURE,			"Lock failure",
	DB_RETCODE_EALLOCFAILURE,			"Memory allocation failure",
	DB_RETCODE_ESYNCFAILURE,			"Sync failure",

	DB_RETCODE_ESECONDARYKEYNOTFOUND,	"Secondary key not found",

	// >3000 (errori)
	DB_RETCODE_ERROR,					"Error",
	DB_RETCODE_UNKNOWNERROR,			"Unknown error"
};

/*
	CBerkeleyDB()
*/
CBerkeleyDB::CBerkeleyDB()
{
	TRACEEXPR((_TRACE_FLAG_INFO,__FILE__,__LINE__,"CBerkeleyDB: CTOR\n"));

	memset(&m_Database,'\0',sizeof(DATABASE));
	m_pDataBuffer = NULL;
	m_nDataBufferSize = -1;
	m_nRecordSize = -1;
	memset(&m_dbinfo,'\0',sizeof(DB_INFO));
	memset(&m_Key,'\0',sizeof(DBT));
	memset(&m_Data,'\0',sizeof(DBT));
	m_pDbHandle = NULL;
	m_pDbCursor = NULL;
	m_pCurrentCursor = NULL;
	m_nCurrentCursorNumber = -1;
	m_pIdxHandleArray = NULL;
	m_pIdxCursorArray = NULL;
	memset(m_szLastPrimaryKey,'\0',sizeof(m_szLastPrimaryKey));
#if defined(_WINDOWS)
   #if defined(_DEBUG)
      m_bShowErrors = true;
   #else
      m_bShowErrors = false;
   #endif
#endif
	m_szPutKeyBuffer = new char[MAX_KEYSIZE+1];
    m_szGetFieldBuffer = new char[MAX_FIELDSIZE+1];
    memset(m_szPutKeyBuffer,'\0',MAX_KEYSIZE+1);
    memset(m_szGetFieldBuffer,'\0',MAX_FIELDSIZE+1);}

/*
	~CBerkeleyDB()
*/
CBerkeleyDB::~CBerkeleyDB()
{
	TRACEEXPR((_TRACE_FLAG_INFO,__FILE__,__LINE__,"CBerkeleyDB: DTOR\n"));

	Close();

	if(m_szPutKeyBuffer)
		delete [] m_szPutKeyBuffer,m_szPutKeyBuffer = NULL;
	if(m_szGetFieldBuffer)
		delete [] m_szGetFieldBuffer,m_szGetFieldBuffer = NULL;
}

/*
	Open()

	Apre la tabella, gli eventuali indici ed i cursori associati. Per default, crea la tabella se non esiste.
	Puo' essere chiamata piu' volte, per cui predisporre le inizializzazioni in modo adeguato (non assumere
	una sola chiamata, inizializzando/rilasciando solo nel costruttore/distruttore).

	Se la tabella non prevede indici, il primo campo viene considerato come chiave primaria, per cui la tabella
	viene impostata per contenere chiavi duplicate.
	Se la tabella prevede indici, il primo campo viene utilizzato internamente come chiave primaria (assoluta),
	per cui non viene impostato il flag per le chiavi duplicate.
*/
int CBerkeleyDB::Open(u_int32_t flags/*=DB_CREATE*/)
{
	TRACEEXPR((_TRACE_FLAG_INFO,__FILE__,__LINE__,"CBerkeleyDB: Open()\n"));

	int nRet = DB_NO_ERROR;

	// TRANSACTION_ENABLED
	// puntatore all'environment: a NULL se le transazioni non sono abilitate,
	// altrimenti punta all'handle statico (s_pDbEnv) creato da InitEnvironment()
	DB_ENV* pEnv = NULL;

	// controlla che la tabella non sia gia' aperta
	ASSERTEXPR(!IsOpen());
	if(IsOpen())
		return(SetLastError(DB_RETCODE_EALREADYOPEN));

	// se la tabella non prevede indici, considera come chiave primaria il primo campo, per cui deve
	// impostare il flag per poter inserire record duplicati
	// se la tabella prevede indici, il primo campo e' la chiave primaria (nascosta), in tal caso non
	// possono esistere chiavi primarie duplicate (ammesse solo negli indici secondari)
	memset(&m_dbinfo,'\0',sizeof(DB_INFO));
	m_dbinfo.flags = (m_Database.table.totindex > 0) ? 0 : DB_DUP|DB_DUPSORT;
	m_dbinfo.db_cachesize = 32 * 1024 * 1024; // 32 MB di cache interna per BerkeleyDB

	// apre (o crea) la tabella
	if((nRet = db_open(	m_Database.table.filename,DB_BTREE,flags,0664,
						pEnv, // modifica: usa pEnv, che puntera' a NULL o s_pDbEnv, a seconda di TRANSACTION_ENABLED
						&m_dbinfo,&m_pDbHandle))!=DB_NO_ERROR)
	{
		m_pDbHandle = NULL;
		nRet = SetLastError(DB_RETCODE_EOPENTABLE);
		goto done;
	}
	
	// cursore associato alla tabella
	if((nRet = m_pDbHandle->cursor(m_pDbHandle,NULL,&m_pDbCursor,0))!=DB_NO_ERROR)
	{
		m_pDbCursor = NULL;
		nRet = SetLastError(DB_RETCODE_EOPENCURSOR);
		goto done;
	}
	
	// imposta il cursore corrente su quello relativo alla tabella (il numero dell'indice relativo
	// viene impostato a -1 per distinguerlo dai progressivi degli indici secondari: 0...[n])
	m_pCurrentCursor = m_pDbCursor;
	m_nCurrentCursorNumber = -1;

	// alloca il buffer per il record (somma della dimensione dei campi)
	// la linea originale del codice: for(int i = 0,m_nRecordSize = 0; i < m_Database.table.totfield; i++) ha sempre funzionato ma dopo le modifiche BDBTF
	// il compilatore ha considerato "for(int i = 0,m_nRecordSize = 0;" come una dichiarazione annidata sullo stack, considerando m_nRecordSize come locale
	// per cui all'uscita dal ciclo, torna a vedere la m_nRecordSize membro a -1, esattamente come l'aveva inizializzato sopra il costruttore
	m_nRecordSize = 0;
	for(int i = 0; i < m_Database.table.totfield; i++)
		m_nRecordSize += m_Database.table.row[i].size;

	if(m_nRecordSize > 0 && m_nRecordSize <= (MAX_FIELDSIZE*MAX_FIELDCOUNT))
	{
		m_nDataBufferSize = m_nRecordSize + 1;
		m_pDataBuffer = new char[m_nDataBufferSize];
		if(!m_pDataBuffer)
		{
			nRet = SetLastError(DB_RETCODE_EALLOCFAILURE);
			goto done;
		}
		memset(m_pDataBuffer,'\0',m_nDataBufferSize);
	}
	else
	{
		nRet = SetLastError(DB_RETCODE_EINVALIDFIELDSIZE);
		goto done;
	}

	// se la tabella prevede indici
	if(m_Database.table.totindex > 0)
	{
		// per evitare cappellate con la chiave primaria (assoluta)
		// se la tabella non prevede indici, GetPrimaryKeySize() restituisce la dimensione
		// del primo campo, che puo' essere diversa da MAX_PRIMARYKEY_SIZE
		if(GetPrimaryKeySize()!=MAX_PRIMARYKEY_SIZE)
		{
			nRet = SetLastError(DB_RETCODE_EINVALIDPRIMARYKEYSIZE);
			goto done;
		}

		// ricava l'ultima chiave primaria (assoluta) (progressivo numerico)
		// necessario per le eventuali chiamate a Insert()
		ResetPair();
		memset(m_szLastPrimaryKey,'\0',sizeof(m_szLastPrimaryKey));
		if(m_pDbCursor->c_get(m_pDbCursor,&m_Key,&m_Data,DB_LAST)==DB_NO_ERROR)
			memcpy(m_szLastPrimaryKey,m_Key.data,GetPrimaryKeySize());
		else
			m_szLastPrimaryKey[0] = '0';
		ResetPair();

		// alloca gli array per gli handles/cursori degli indici:

		// qui sotto sbagliatissimo (vedi piu' sotto la forma corretta, qui si lascia come esempio di cappellata master), 
		// perche' alloca un array di oggetti DB, non di puntatori, e con il cast '(DB**)' forza il compilatore a trattare 
		// l'array di oggetti DB come un array di puntatori a DB, ma la memoria allocata e' per oggetti, non puntatori, il 
		// che produce random memory corruption:
		//
		// m_pIdxHandleArray = (DB**)new DB[m_Database.table.totindex];
		// m_pIdxCursorArray = (DBC**)new DBC[m_Database.table.totindex];

		// la sintassi della dichiarazione corretta qui va letta come: "new array-of-DB-pointers of size totindex"
		// si possono usare tre forme equivalenti tra loro:
		//
		// forma 1 (quella usata sotto)
		// new DB*[m_Database.table.totindex]
		//
		// forma 2 (piu' esplicita)
		// new (DB*)[m_Database.table.totindex]
		//
		// forma 3 (usando typedef)
		// typedef DB* DB_PTR;
		// new DB_PTR[m_Database.table.totindex]
		//
		m_pIdxHandleArray = new DB*[m_Database.table.totindex];
		m_pIdxCursorArray = new DBC*[m_Database.table.totindex];

		if (!m_pIdxHandleArray || !m_pIdxCursorArray)
		{
			nRet = SetLastError(DB_RETCODE_EALLOCFAILURE);
			goto done;
		}

		memset(m_pIdxHandleArray, '\0', sizeof(DB*)  * m_Database.table.totindex);
		memset(m_pIdxCursorArray, '\0', sizeof(DBC*) * m_Database.table.totindex);

		// apre (o crea) gli indici secondari (possono contenere chiavi duplicate)
		//
		// tabella:		chiave_primaria(unica) + campo1/campo2/campo3/...
		//				chiave_primaria(unica) + campo1/campo2/campo3/...
		//				chiave_primaria(unica) + campo1/campo2/campo3/...
		//				etc.
		//
		// indice campo1:campo1(puo' essere duplicato) + chiave_primaria(unica)
		//				campo1(puo' essere duplicato) + chiave_primaria(unica)
		//				campo1(puo' essere duplicato) + chiave_primaria(unica)
		//				etc.
		//		
		for(int i = 0; i < m_Database.table.totindex; i++)
		{
			// imposta il flag per le chiavi duplicate
			memset(&m_dbinfo,'\0',sizeof(DB_INFO));
			m_dbinfo.flags = DB_DUP|DB_DUPSORT;
			m_dbinfo.db_cachesize = 32 * 1024 * 1024; // 32 MB di cache interna per BerkeleyDB
			
			// apre (o crea) il file per l'indice
			if((nRet = db_open(	m_Database.table.index[i].filename,DB_BTREE,flags,0664,
								pEnv, // modifica: usa pEnv, che puntera' a NULL o s_pDbEnv, a seconda di TRANSACTION_ENABLED
								&m_dbinfo,&m_pIdxHandleArray[i]))!=DB_NO_ERROR)
			{
				m_pIdxHandleArray[i] = NULL;
				nRet = SetLastError(DB_RETCODE_EOPENINDEX);
				goto done;
			}
			// cursore associato all'indice
			if((nRet = m_pIdxHandleArray[i]->cursor(m_pIdxHandleArray[i],NULL,&m_pIdxCursorArray[i],0))!=DB_NO_ERROR)
			{
				m_pIdxCursorArray[i] = NULL;
				nRet = SetLastError(DB_RETCODE_EOPENIDXCURSOR);
				goto done;
			}
		}
	}
	else
	{
		m_pIdxHandleArray = NULL;
		m_pIdxCursorArray = NULL;
	}

	// azzera il record
	ResetRecord();

done:

	// errore durante l'apertura, chiama la Close() per rilasciare e resettare le risorse associate
	if(nRet!=DB_NO_ERROR)
		Close();

	return(nRet);
}

/*
	Create()

	Crea la tabella (la apre azzerandola).
*/
int CBerkeleyDB::Create(void)
{
	int nRet = DB_NO_ERROR;

	ASSERTEXPR(!IsOpen());

	if(!IsOpen())
		nRet = Open(DB_TRUNCATE);
	else
		nRet = DB_RETCODE_EALREADYOPEN;

	if(nRet!=DB_NO_ERROR)
		SetLastError(nRet);

	return(nRet);
}

/*
	Close()

	Chiude la tabella, gli indici ed i cursori associati.
	Puo' essere chiamata piu' volte, per cui predisporre le inizializzazioni in modo adeguato,
	ossia non assumere una sola chiamata, inizializzando e/o rilasciando solo nel costruttore
	e/o distruttore (di questa classe).
*/
int CBerkeleyDB::Close(void)
{
	TRACEEXPR((_TRACE_FLAG_INFO,__FILE__,__LINE__,"CBerkeleyDB: Close()\n"));

	int nRet = DB_NO_ERROR;

	if(!IsOpen())
		return(nRet);

	// chiude i cursori degli indici
	if(m_Database.table.totindex > 0 && m_pIdxCursorArray)
	{
		for(int i = 0; i < m_Database.table.totindex; i++)
		{
			if(m_pIdxCursorArray[i])
			{
				if(m_pIdxCursorArray[i]->c_close(m_pIdxCursorArray[i])!=DB_NO_ERROR)
					if(nRet==DB_NO_ERROR)
						nRet = DB_RETCODE_ECLOSEIDXCURSOR;
			}
		}
	}

	// chiude il cursore della tabella
	if(m_pDbCursor)
		if(m_pDbCursor->c_close(m_pDbCursor)!=DB_NO_ERROR)
			if(nRet==DB_NO_ERROR)
				nRet = DB_RETCODE_ECLOSECURSOR;

	// chiude gli indici
	if(m_Database.table.totindex > 0 && m_pIdxHandleArray)
	{
		for(int i = 0; i < m_Database.table.totindex; i++)
		{
			if(m_pIdxHandleArray[i])
			{
				if(m_pIdxHandleArray[i]->close(m_pIdxHandleArray[i],0)!=DB_NO_ERROR)
					if(nRet==DB_NO_ERROR)
						nRet = DB_RETCODE_ECLOSEINDEX;
			}
		}
	}

	// chiude la tabella
	if(m_pDbHandle)
		if(m_pDbHandle->close(m_pDbHandle,0)!=DB_NO_ERROR)
			if(nRet==DB_NO_ERROR)
				nRet = DB_RETCODE_ECLOSETABLE;

	// resetta handles e cursori
	m_pDbHandle = NULL;
	m_pDbCursor = NULL;
	m_pCurrentCursor = NULL;
	m_nCurrentCursorNumber = -1;

	// rilascia il buffer per il record
	if(m_pDataBuffer)
	{
		delete [] m_pDataBuffer;
		m_pDataBuffer = NULL;
	}

	// rilascia gli array per gli indici
	if(m_Database.table.totindex > 0)
	{
		if(m_pIdxHandleArray)
		{
			delete [] m_pIdxHandleArray;
			m_pIdxHandleArray = NULL;
		}

		if (m_pIdxCursorArray)
		{
			delete [] m_pIdxCursorArray;
			m_pIdxCursorArray = NULL;
		}
	}

	if(nRet!=DB_NO_ERROR)
		SetLastError(nRet);

	return(nRet);
}

/*
	Insert()

	Inserisce il record corrente nella tabella.
*/
int CBerkeleyDB::Insert(void)
{
	int nRet = DB_NO_ERROR;

	// TRANSACTION_ENABLED
	// se le transazioni non sono abilitate, pTxn e' NULL
	DB_TXN* pTxn = NULL;

	ASSERTEXPR(IsOpen());
	if(!IsOpen())
		return(SetLastError(DB_RETCODE_ETABLENOTOPEN));

	// ricava i dati relativi alla chiave primaria (valore, dimensione e cursore associato)
	// se la tabella non prevede indici, la chiave primaria coincide con il primo campo della stessa
	char* pPrimaryKeyData   = GetPrimaryKeyData();
	int   nPrimaryKeySize   = GetPrimaryKeySize();
	DBC*  pPrimaryKeyCursor = m_pDbCursor;

	// azzera la struttura key/data usata per l'inserimento
	ResetPair();

	// se la tabella prevede indici, genera la chiave primaria per l'inserimento
	// in caso contrario il primo campo della tabella viene considerato come chiave
	if(m_Database.table.totindex > 0)
	{
		int	nLastKeyLen;
		long nLastKeyNum;
		char szBuffer[MAX_PRIMARYKEY_SIZE+1] = {0};
			
		// nuova chiave primaria = chiave corrente + 1
		nLastKeyNum = atol(m_szLastPrimaryKey) + 1L;
		memset(m_szLastPrimaryKey,'0',sizeof(m_szLastPrimaryKey));
			
		nLastKeyLen = wtfsnprintf(szBuffer,sizeof(szBuffer),"%ld",nLastKeyNum);
		memcpy(m_szLastPrimaryKey + (nPrimaryKeySize-nLastKeyLen),szBuffer,nLastKeyLen);
			
		m_szLastPrimaryKey[nPrimaryKeySize] = '\0';
		memcpy(pPrimaryKeyData,m_szLastPrimaryKey,nPrimaryKeySize);
	}
		
	// passa la chiave (primaria) nella struttura (key)
	m_Key.data = pPrimaryKeyData;
	m_Key.size = nPrimaryKeySize;

	// passa i dati (i campi del record impostati con le PutField()) nella struttura (data)
	RecordToMemory();

	// occhio: tenere a mente che, nonostante quanto segue, a monte la tabella e' stata aperta con DB_DUP|DB_DUPSORT

	// con le transazioni non abilitate, usa il cursore persistente

	// cio' che viene inserito e' la chiave primaria (generata in proprio, se la tabella prevede indici, o il primo campo
	// della tabella, se non sono previsti indici) ed i dati (i campi del record corrente, impostato dal chiamante con le
	// PutField() relative)
	
	// notare che la chiave primaria (generata in proprio e 'nascosta') non entra a formar parte dei dati

	// usa il flag DB_KEYLAST che significa "inserisci questa coppia chiave/dati come l'ultima del set di duplicati per quella 
	// chiave" perche', generando per ogni inserimento una chiave primaria unica (la famosa chiave 'nascosta'), e' sicuro che
	// non possono esistere duplicati, mentrese la tabella non prevede indici (ossia puo' contenere duplicati), allora il flag 
	// semplicemente aggiunge alla fine, come un append
	//
	if(pPrimaryKeyCursor)
	{
		nRet = pPrimaryKeyCursor->c_put(pPrimaryKeyCursor,&m_Key,&m_Data,DB_KEYLAST);
		if(nRet==DB_NO_ERROR)
			nRet = SyncDb();
	}
	else
		nRet = DB_RETCODE_EINVALIDCURSOR;

	// se la tabella prevede indici, li aggiorna inserendo un record con:
	// chiave -> il campo relativo dell'indice secondario, ossia il campo su cui viene costruito l'indice
	// dati -> la chiave primaria
	if(nRet==DB_NO_ERROR)
	{
		char szBuffer[MAX_KEYSIZE+1] = {0};
		char szSecondaryKey[MAX_KEYSIZE+1] = {0};
		int nSecondaryKeyField = -1;
		int nSecondaryKeySize = -1;

		// per ognuno degli indici della tabella
		for(int i = 0; i < m_Database.table.totindex && nRet==DB_NO_ERROR; i++)
		{
			// ricava il numero (progressivo) e la dimensione del campo
			// su cui e' costruito l'indice secondario (chiave secondaria)
			if(nRet==DB_NO_ERROR)
			{
				nSecondaryKeyField = GetSecondaryKeyField(i);
				if(nSecondaryKeyField < 0 || nSecondaryKeyField >= MAX_FIELDCOUNT)
				{
					if(nRet==DB_NO_ERROR)
						nRet = DB_RETCODE_EINVALIDFIELDNUMBER;
				}
			}
			if(nRet==DB_NO_ERROR)
			{
				nSecondaryKeySize = GetSecondaryKeySize(nSecondaryKeyField);
				if(nSecondaryKeySize <= 0 || nSecondaryKeySize > MAX_KEYSIZE)
				{	
					if(nRet==DB_NO_ERROR)
						nRet = DB_RETCODE_EINVALIDFIELDSIZE;
				}
			}

			// inserisce il record nell'indice secondario
			if(nRet==DB_NO_ERROR && nSecondaryKeyField!=-1 && nSecondaryKeySize!=-1)
			{
				// imposta il valore del campo
				memset(szBuffer,'\0',sizeof(szBuffer));
				memset(szSecondaryKey,' ',sizeof(szSecondaryKey));
				memcpy(szBuffer,GetSecondaryKeyData(nSecondaryKeyField),nSecondaryKeySize);
				szBuffer[nSecondaryKeySize] = '\0';
				memcpy(szSecondaryKey,szBuffer,strlen(szBuffer));

				// imposta la chiave (secondaria) con il valore del campo usato come indice secondario
				m_Key.data = szSecondaryKey;
				m_Key.size = nSecondaryKeySize;

				// imposta i dati (la chiave primaria)
				m_Data.data = pPrimaryKeyData;
				m_Data.size = nPrimaryKeySize;

				// vedi le note sopra sull'inserimento nella tabella principale e tenere a mente che gli indici 
				// secondari hanno funzionano cosi':
				// - chiave (Key): il campo indicizzato (es. nome_utente, data creazione) che E' duplicabile
				// - dato (Data): la chiave primaria (PK) assoluta del record (la famosa chiave 'nascosta') che
				//   serve per il lookup, ossia per trovare tutti i duplicati (nome_utente, data creazione, etc.)

				// con transazioni non abilitate, usa il cursore persistente per l'inserimento
				if(m_pIdxCursorArray[i])
				{
					nRet = m_pIdxCursorArray[i]->c_put(m_pIdxCursorArray[i],&m_Key,&m_Data,DB_KEYLAST);
					if(nRet==DB_NO_ERROR)
						nRet = SyncIdx(i);
				}
				else
				{
					if(nRet==DB_NO_ERROR)
						nRet = DB_RETCODE_EINVALIDCURSOR;
				}
			}
		}
	}

	if(nRet!=DB_NO_ERROR)
		SetLastError(nRet);

	return(nRet);
}

/*
	Delete()

	Elimina il record corrente dalla tabella.
*/
int CBerkeleyDB::Delete(void)
{
	int nRet = DB_NO_ERROR;

	// TRANSACTION_ENABLED
	// se le transazioni non sono abilitate, pTxn e' NULL
	DB_TXN* pTxn = NULL;

	ASSERTEXPR(IsOpen());
	if(!IsOpen())
		return(SetLastError(DB_RETCODE_ETABLENOTOPEN));

	// ricava i dati relativi alla chiave primaria (valore, dimensione e cursore associato)
	// se la tabella non prevede indici, la chiave primaria coincide con il primo campo della stessa
	char* pPrimaryKeyData   = GetPrimaryKeyData();
	int   nPrimaryKeySize   = GetPrimaryKeySize();
	DBC*  pPrimaryKeyCursor = m_pDbCursor;

	// elimina il record corrente dal database
	// considera come record corrente quello impostato con l'ultima get sulla tabella (key/data correnti)
	// in altre parole, prima di eliminare un record assicurarsi che esista (tramite una ricerca), o la
	// eliminazione fallira'
	if(pPrimaryKeyCursor)
	{
		// deve sincronizzare perche' la c_del() scasina
		if((nRet = pPrimaryKeyCursor->c_del(pPrimaryKeyCursor,0))==DB_NO_ERROR)
			nRet = SyncDb();
	}
	else
	{
		if(nRet==DB_NO_ERROR)
			nRet = DB_RETCODE_EINVALIDCURSOR;
	}

	// se la tabella prevede indici li aggiorna, eliminando i record che contengono la chiave (primaria) eliminata sopra
	// cerca il primo record (nell'indice secondario possono esistere chiavi duplicate) relativo alla chiave secondaria,
	// ossia il campo su cui e' costruito l'indice, e una volta trovato cerca il record che contenga (come dati) la chiave
	// primaria (in questo caso ne *deve* esiste solo uno)
	if(nRet==DB_NO_ERROR && m_Database.table.totindex > 0)
	{
		char szBuffer[MAX_KEYSIZE+1] = {0};
		char szPrimaryKey[MAX_KEYSIZE+1] = {0};
		char szSecondaryKey[MAX_KEYSIZE+1] = {0};
		int nSecondaryKeyField = -1;
		int nSecondaryKeySize = -1;

		// imposta la chiave primaria (eliminata sopra dalla tabella)
		memset(szBuffer,'\0',sizeof(szBuffer));
		memset(szPrimaryKey,' ',sizeof(szPrimaryKey));
		memcpy(szBuffer,pPrimaryKeyData,nPrimaryKeySize);
		szBuffer[nPrimaryKeySize] = '\0';
		memcpy(szPrimaryKey,szBuffer,strlen(szBuffer));

		// per ognuno degli indici della tabella
		for(int i = 0; i < m_Database.table.totindex && nRet==DB_NO_ERROR; i++)
		{
			memset(szBuffer,'\0',sizeof(szBuffer));
			memset(szSecondaryKey,' ',sizeof(szSecondaryKey));

			// ricava il numero (progressivo) e la dimensione del campo
			// su cui e' costruito l'indice secondario (chiave secondaria)
			if(nRet==DB_NO_ERROR)
			{
				nSecondaryKeyField = GetSecondaryKeyField(i);
				if(nSecondaryKeyField < 0 || nSecondaryKeyField >= MAX_FIELDCOUNT)
				{
					if(nRet==DB_NO_ERROR)
						nRet = DB_RETCODE_EINVALIDFIELDNUMBER;
				}
			}
			if(nRet==DB_NO_ERROR)
			{
				nSecondaryKeySize = GetSecondaryKeySize(nSecondaryKeyField);
				if(nSecondaryKeySize <= 0 || nSecondaryKeySize > MAX_KEYSIZE)
				{
					if(nRet==DB_NO_ERROR)
						nRet = DB_RETCODE_EINVALIDFIELDSIZE;
				}
			}

			// aggiorna l'indice
			if(nRet==DB_NO_ERROR && nSecondaryKeyField!=-1 && nSecondaryKeySize!=-1)
			{
				if(m_pIdxCursorArray[i])
				{
					// ricava il valore del campo usato come chiave secondaria
					memcpy(szBuffer,GetSecondaryKeyData(nSecondaryKeyField),nSecondaryKeySize);
					szBuffer[nSecondaryKeySize] = '\0';
					memcpy(szSecondaryKey,szBuffer,strlen(szBuffer));
						
					// imposta la chiave per la ricerca con il valore del campo usato come chiave secondaria
					m_Key.data = szSecondaryKey;
					m_Key.size = nSecondaryKeySize;

					// La logica di ricerca con c_get, DB_SET e loop con DB_NEXT RESTA con il cursore (m_pIdxCursorArray[i]), 
					// perché le operazioni di navigazione (ricerca per chiave duplicata) non sono transazionali
					// e non influiscono sulla coerenza logica.
					// 
					// cerca la prima chiave (secondaria)
					if((nRet = m_pIdxCursorArray[i]->c_get(m_pIdxCursorArray[i],&m_Key,&m_Data,DB_SET))==DB_NO_ERROR)
					{
						bool bFound = false;

						// scorre i record trovati cercando quello con chiave primaria uguale a quella impostata sopra
						do {
							if(memcmp(szPrimaryKey,(char*)m_Data.data,nPrimaryKeySize)==0)
							{
								// elimina il record dall'indice secondario (deve sincronizzare perche la c_del() scasina)
								if((nRet = m_pIdxCursorArray[i]->c_del(m_pIdxCursorArray[i],0))==DB_NO_ERROR)
								{
									// la sincronizzazione (SyncIdx) e' necessaria SOLO se NON sta in modo transazionale
									// se in modo transazionale, e' il commit finale che gestisce la scrittura su disco
									// eliminare quindi tutte le chiamate a SyncDb() e SyncIdx() (e potenzialmente anche 
									// a DB::sync() su qualsiasi handle) se la macro TRANSACTION_ENABLED e' definita
									/*
									Meccanismo:		Senza Transazioni (Sync manuale).				Con Transazioni (Commit automatico)
									Obiettivo:		Forzare la scrittura dei dati su disco			Garantire che TUTTE le operazioni nel blocco siano scritte 
													immediatamente dopo l'operazione (put, del).	su disco solo ed esclusivamente al momento del Commit.
									Responsabilita':Lo sviluppatore deve chiamare SyncDb() ogni		La libreria BDB gestisce automaticamente il transaction log.
													volta che vuole salvare i dati.	
									Motivazione:	Se il programma crasha, le modifiche non		Se il programma crasha, il Recovery legge il log e annulla o
													sincronizzate vengono perse.					completa l'intera transazione.
									*/
									nRet = SyncIdx(i);
								}

								// in teoria deve esistere un solo record con la stessa chiave primaria
								if(nRet==DB_NO_ERROR)
									bFound = true;
	
								break;
							}
						} while((nRet = m_pIdxCursorArray[i]->c_get(m_pIdxCursorArray[i],&m_Key,&m_Data,DB_NEXT))==DB_NO_ERROR);
						
						if(!bFound)
						{
							if(nRet==DB_NO_ERROR)
								nRet = DB_RETCODE_ESECONDARYKEYNOTFOUND;
						}
						else
							nRet = DB_NO_ERROR;
					}
					else
					{
						if(nRet==DB_NO_ERROR)
							nRet = DB_RETCODE_ESECONDARYKEYNOTFOUND;
					}
				}
				else
				{
					if(nRet==DB_NO_ERROR)
						nRet = DB_RETCODE_EINVALIDCURSOR;
				}
			}
		}
	}

	if(nRet!=DB_NO_ERROR)
		SetLastError(nRet);

	return(nRet);
}

/*
	Replace()

	Sostituisce il campo del record corrente dalla tabella.
*/
int CBerkeleyDB::Replace(int nFieldNum,const char* pCurrentValue,const char* pReplaceValue)
{
	int nRet = DB_NO_ERROR;

	if(!pCurrentValue || !pReplaceValue)
		return(SetLastError(DB_RETCODE_EINVALIDPARAM));

	// se le transazioni non sono abilitate, pTxn e' NULL
	DB_TXN* pTxn = NULL;

	ASSERTEXPR(IsOpen());
	if(!IsOpen())
		return(SetLastError(DB_RETCODE_ETABLENOTOPEN));

	// ricava i dati relativi alla chiave primaria (valore, dimensione e cursore associato)
	// se la tabella non prevede indici, la chiave primaria coincide con il primo campo della stessa
	char* pPrimaryKeyData   = GetPrimaryKeyData();
	int   nPrimaryKeySize   = GetPrimaryKeySize();
	DBC*  pPrimaryKeyCursor = m_pDbCursor;

	// azzera la struttura key/data usata per la sostituzione
	ResetPair();

	// passa la chiave primaria nella struttura (key)
	m_Key.data = pPrimaryKeyData;
	m_Key.size = nPrimaryKeySize;

	// passa i dati (i campi del record impostati con le PutField()) nella struttura (data)
	RecordToMemory();

	// sostituisce il campo del record corrente dal database
	// considera come record corrente quello impostato con l'ultima get sulla tabella (key/data correnti)
	// in altre parole, prima di aggiornare il campo, assicurarsi che il record esista (tramite una ricerca)
	// se la tabella prevede indici, la sostituzione e' relativa al campo impostato con l'ultima PutField()
	// e non viene cambiata la chiave primaria (sostituisce il record)
	// se la tabella non prevede indici, la sostituzione implica l'intero record (in tal caso la chiave
	// primaria coincide col primo campo), quindi elimina il record e lo reinserisce
	if(pPrimaryKeyCursor)
	{
		if(m_Database.table.totindex <= 0)
		{
			// deve sincronizzare perche' la c_del() scasina
			if((nRet = pPrimaryKeyCursor->c_del(pPrimaryKeyCursor,0))==DB_NO_ERROR)
				nRet = SyncDb();
		}
		else
			nRet = DB_NO_ERROR;
			
		if(nRet==DB_NO_ERROR)
		{
			nRet = pPrimaryKeyCursor->c_put(pPrimaryKeyCursor,&m_Key,&m_Data,m_Database.table.totindex <= 0 ? DB_KEYLAST : DB_CURRENT);
			if(nRet==DB_NO_ERROR)
				nRet = SyncDb();
		}
	}
	else
	{
		if(nRet==DB_NO_ERROR)
			nRet = DB_RETCODE_EINVALIDCURSOR;
	}

	// se il campo aggiornato viene usato come indice secondario, deve aggiornare l'indice relativo
	if(nRet==DB_NO_ERROR && m_Database.table.totindex > 0)
	{
		int nIndex = -1;
		int nSecondaryKeySize = -1;

		// controlla se il campo viene usato come chiave per uno
		// degli indici secondari, ricavando l'indice associato
		for(int i = 0; i < m_Database.table.totindex; i++)
			if(nFieldNum==GetSecondaryKeyField(i))
			{
				nIndex = i;
				break;
			}

		// ricava la dimensione del campo
		if(nIndex >= 0)
			for(int i = 0; i < m_Database.table.totfield; i++)
				if(nFieldNum==i)
				{
					nSecondaryKeySize = GetSecondaryKeySize(i);
					break;
				}

		if(nIndex >= 0 && nSecondaryKeySize > 0)
		{
			if(m_pIdxCursorArray[nIndex])
			{
				// imposta la chiave primaria
				char szBuffer[MAX_KEYSIZE+1] = {0};
				char szPrimaryKey[MAX_KEYSIZE+1] = {0};
				memset(szBuffer,'\0',sizeof(szBuffer));
				memset(szPrimaryKey,' ',sizeof(szPrimaryKey));
				memcpy(szBuffer,pPrimaryKeyData,nPrimaryKeySize);
				szBuffer[nPrimaryKeySize] = '\0';
				memcpy(szPrimaryKey,szBuffer,strlen(szBuffer));

				// imposta la chiave (secondaria) per la ricerca sull'indice secondario con il vecchio valore del campo
				char szSecondaryKey[MAX_FIELDSIZE+1] = {0};
					
				memset(szSecondaryKey,' ',sizeof(szSecondaryKey));
				int nCurLen = (int)strlen(pCurrentValue);
				if(nCurLen > nSecondaryKeySize)
					nCurLen = nSecondaryKeySize;
				memcpy(szSecondaryKey,pCurrentValue,nCurLen);
				szSecondaryKey[nSecondaryKeySize] = '\0';

				// azzera la struttura key/data usata per la sostituzione
				ResetPair();

				// passa la chiave secondaria nella struttura (key)
				m_Key.data = szSecondaryKey;
				m_Key.size = nSecondaryKeySize;

				// cerca il record relativo nell'indice secondario (la prima chiave secondaria, possono esistere duplicati)
				if((nRet = m_pIdxCursorArray[nIndex]->c_get(m_pIdxCursorArray[nIndex],&m_Key,&m_Data,DB_SET))==DB_NO_ERROR)
				{
					bool bFound = false;

					// scorre i record trovati cercando quello con chiave primaria uguale a quella impostata sopra
					do {
						if(memcmp(szPrimaryKey,(char*)m_Data.data,nPrimaryKeySize)==0)
						{
							// 3. ELIMINAZIONE DALL'INDICE SECONDARIO

							// record trovato, lo elimina per inserire la coppia nuovo valore del campo + chiave primaria
							// non cicla perche' (in teoria) deve esistere un solo record con la stessa chiave primaria
							if((nRet = m_pIdxCursorArray[nIndex]->c_del(m_pIdxCursorArray[nIndex],0))==DB_NO_ERROR)
							{
								// se l'eliminazione riesce, allora deve distinguere: se in modo NON transazionale, esegue 
								// il blocco sottostante solo se SincIdx() va a buon fine, se in modo transazionale, esegue 
								// il blocco sottostante comunque sia
								// in modalita' transazionale la SincIdx() e' sostituita dalla commit finale

								// deve sincronizzare perche la c_del() scasina
								nRet = SyncIdx(nIndex);

								if(nRet==DB_NO_ERROR)
								{
									// imposta le chiavi per l'inserimento sull'indice secondario: chiave secondaria con il 
									// nuovo valore del campo + chiave primaria
									// usa il cursore comunque sia (transazionale o no), dato che deve gestire i duplicati
									// per questo puo' usare DB_KEYLAST
									memset(szSecondaryKey,' ',sizeof(szSecondaryKey));
									int nRepLen = (int)strlen(pReplaceValue);
									if(nRepLen > nSecondaryKeySize)
										nRepLen = nSecondaryKeySize;
									memcpy(szSecondaryKey,pReplaceValue,nRepLen);
									szSecondaryKey[nSecondaryKeySize] = '\0';
									ResetPair();
									m_Key.data  = szSecondaryKey;
									m_Key.size  = nSecondaryKeySize;
									m_Data.data = szPrimaryKey;
									m_Data.size = nPrimaryKeySize;

									nRet = m_pIdxCursorArray[nIndex]->c_put(m_pIdxCursorArray[nIndex],&m_Key,&m_Data,DB_KEYLAST);
									if(nRet==DB_NO_ERROR)
										nRet = SyncIdx(nIndex);
								}
							}
								
							// in teoria deve esistere un solo record con la stessa chiave primaria
							if(nRet==DB_NO_ERROR)
								bFound = true;

							break;
						}
					} while((nRet = m_pIdxCursorArray[nIndex]->c_get(m_pIdxCursorArray[nIndex],&m_Key,&m_Data,DB_NEXT))==DB_NO_ERROR);
					
					if(!bFound)
					{
						if(nRet==DB_NO_ERROR)
							nRet = DB_RETCODE_ESECONDARYKEYNOTFOUND;
					}
					else
						nRet = DB_NO_ERROR;
				}
				else
				{
					if(nRet==DB_NO_ERROR)
						nRet = DB_RETCODE_ESECONDARYKEYNOTFOUND;
				}
			}
			else
			{
				if(nRet==DB_NO_ERROR)
					nRet = DB_RETCODE_EINVALIDCURSOR;
			}
		}
	}

	if(nRet!=DB_NO_ERROR)
		SetLastError(nRet);

	return(nRet);
}

/*
	Reindex()

	Ricostruisce l'indice.
*/
int CBerkeleyDB::Reindex(int nIndex/*=-1*/)
{
	int nRet = DB_NO_ERROR;

	// TRANSACTION_ENABLED
	// puntatore locale all'Environment: NULL se le transazioni non sono abilitate,
	// altrimenti punta all'handle statico (s_pDbEnv) creato da InitEnvironment()
	DB_ENV* pEnv = NULL;
	DB_TXN* pTxn = NULL;

	ASSERTEXPR(IsOpen());
	if(IsOpen())
	{
		// la tabella deve prevedere indici
		if(nRet==DB_NO_ERROR)
			if(m_Database.table.totindex <= 0)
			{
				if(nRet==DB_NO_ERROR)
					nRet = DB_RETCODE_EINDEXREQUIRED;
			}

		// controlla il numero dell'indice
		if(nRet==DB_NO_ERROR)
			if(nIndex >= m_Database.table.totindex)
			{
				if(nRet==DB_NO_ERROR)
					nRet = DB_RETCODE_EINVALIDINDEX;
			}

		if(nRet==DB_NO_ERROR)
		{	
			// ricava il cursore corrente
			DBC* pCurrentCursor = m_pCurrentCursor;
			int nCurrentCursorNumber = m_nCurrentCursorNumber;

			// imposta l'intervallo per gli indici da ricostruire
			int nFirst = 0;
			int nLast = m_Database.table.totindex;
			if(nIndex!=-1)
			{
				nFirst = nIndex;
				nLast = nIndex+1;
			}

			char szBuffer[MAX_KEYSIZE+1] = {0};
			char szPrimaryKey[MAX_KEYSIZE+1] = {0};
			int nPrimaryKeySize = GetPrimaryKeySize();
			char szSecondaryKey[MAX_KEYSIZE+1] = {0};
			int nSecondaryKeyField = -1;
			int nSecondaryKeySize = -1;
			DB_INFO db_info = {0};
			db_info.flags = DB_DUP|DB_DUPSORT;

			// per ognuno degli indici da ricostruire
			for(int i = nFirst; i < nLast && nRet==DB_NO_ERROR; i++)
			{
				// ricava il numero (progressivo) e la dimensione del campo
				// su cui e' costruito l'indice secondario (chiave secondaria)
				if(nRet==DB_NO_ERROR)
				{
					nSecondaryKeyField = GetSecondaryKeyField(i);
					if(nSecondaryKeyField < 0 || nSecondaryKeyField >= MAX_FIELDCOUNT)
					{
						if(nRet==DB_NO_ERROR)
							nRet = DB_RETCODE_EINVALIDFIELDNUMBER;
					}
				}
				if(nRet==DB_NO_ERROR)
				{
					nSecondaryKeySize = GetSecondaryKeySize(nSecondaryKeyField);
					if(nSecondaryKeySize <= 0 || nSecondaryKeySize > MAX_KEYSIZE)
					{
						if(nRet==DB_NO_ERROR)
							nRet = DB_RETCODE_EINVALIDFIELDSIZE;
					}
				}

				// chiude, elimina e ricrea (a zero) l'indice corrente
				if(nRet==DB_NO_ERROR)
				{
					nRet = m_pIdxCursorArray[i]->c_close(m_pIdxCursorArray[i]);
					if(nRet!=DB_NO_ERROR)
						SetLastError(nRet);
				}
				if(nRet==DB_NO_ERROR)
				{
					nRet = m_pIdxHandleArray[i]->close(m_pIdxHandleArray[i],0);
					if(nRet!=DB_NO_ERROR)
						SetLastError(nRet);
					remove(m_Database.table.index[i].filename);
				}				
				if(nRet==DB_NO_ERROR)
				{
					memset(&db_info,'\0',sizeof(DB_INFO));
					db_info.flags = DB_DUP|DB_DUPSORT;
					nRet = db_open(	m_Database.table.index[i].filename,DB_BTREE,DB_CREATE,0664,
									pEnv, // modifica: usa pEnv, che puntera' a NULL o s_pDbEnv, a seconda di TRANSACTION_ENABLED
									&db_info,&m_pIdxHandleArray[i]);
					if(nRet!=DB_NO_ERROR)
						SetLastError(nRet);
				}
				if(nRet==DB_NO_ERROR)
				{
					nRet = m_pIdxHandleArray[i]->cursor(m_pIdxHandleArray[i],
														pTxn, // modifica: usa pTxn, che puntera' a NULL o txn_guard.GetTxn(), a seconda di TRANSACTION_ENABLED
														&m_pIdxCursorArray[i],0);
					if(nRet!=DB_NO_ERROR)
						SetLastError(nRet);
				}
				if(nRet==DB_NO_ERROR)
				{
					// azzera la struttura key/data usata per la ricerca
					ResetPair();

					bool bFound = false;

					// cicla per ognuno dei record presenti nella tabella
					if((nRet = m_pDbCursor->c_get(m_pDbCursor,&m_Key,&m_Data,DB_FIRST))==DB_NO_ERROR)
						do {
							// passa i valori alla struttura
							MemoryToRecord();
							
							// ricava il valore della chiave primaria
							memset(szBuffer,'\0',sizeof(szBuffer));
							memset(szPrimaryKey,' ',sizeof(szPrimaryKey));
							memcpy(szBuffer,GetPrimaryKeyData(),nPrimaryKeySize);
							szBuffer[nPrimaryKeySize] = '\0';
							memcpy(szPrimaryKey,szBuffer,strlen(szBuffer));

							// ricava il valore della chiave secondaria
							memset(szBuffer,'\0',sizeof(szBuffer));
							memset(szSecondaryKey,' ',sizeof(szSecondaryKey));
							memcpy(szBuffer,GetSecondaryKeyData(nSecondaryKeyField),nSecondaryKeySize);
							szBuffer[nSecondaryKeySize] = '\0';
							memcpy(szSecondaryKey,szBuffer,strlen(szBuffer));

							// imposta la chiave (secondaria) con il valore del campo usato come indice secondario
							m_Key.data = szSecondaryKey;
							m_Key.size = nSecondaryKeySize;

							// imposta i dati (la chiave primaria)
							m_Data.data = szPrimaryKey;
							m_Data.size = nPrimaryKeySize;

							// inserisce il record nell'indice
							if(m_pIdxCursorArray[i])
								nRet = m_pIdxCursorArray[i]->c_put(m_pIdxCursorArray[i],&m_Key,&m_Data,DB_KEYLAST);
							else
							{
								if(nRet==DB_NO_ERROR)
									nRet = DB_RETCODE_EINVALIDCURSOR;
							}
							if(nRet!=DB_NO_ERROR)
							{
								bFound = false;
								break;
							}
							else
								bFound = true;

							// azzera la struttura key/data usata per la ricerca
							ResetPair();

						} while((nRet = m_pDbCursor->c_get(m_pDbCursor,&m_Key,&m_Data,DB_NEXT))==DB_NO_ERROR);

					if(bFound)
						nRet = DB_NO_ERROR;
				}
			}
			
			// reimposta il cursore anteriore
			if(nRet==DB_NO_ERROR)
			{
				m_pCurrentCursor = pCurrentCursor;
				m_nCurrentCursorNumber = nCurrentCursorNumber;
			}
		}
	}
	else
	{
		if(nRet==DB_NO_ERROR)
			nRet = DB_RETCODE_ETABLENOTOPEN;
	}

	if(nRet!=DB_NO_ERROR)
		SetLastError(nRet);

	return(nRet);
}

/*
	CheckIndex()

	Controlla l'indice.
*/
int CBerkeleyDB::CheckIndex(int nIndex/*=-1*/)
{
	int nRet = DB_NO_ERROR;

	ASSERTEXPR(IsOpen());
	if(IsOpen())
	{
		// la tabella deve prevedere indici
		if(nRet==DB_NO_ERROR)
			if(m_Database.table.totindex <= 0)
			{
				if(nRet==DB_NO_ERROR)
					nRet = DB_RETCODE_EINDEXREQUIRED;
			}

		// controlla il numero dell'indice
		if(nRet==DB_NO_ERROR)
			if(nIndex >= m_Database.table.totindex)
			{
				if(nRet==DB_NO_ERROR)
					nRet = DB_RETCODE_EINVALIDINDEX;
			}

		if(nRet==DB_NO_ERROR)
		{	
			// ricava il cursore corrente
			DBC* pCurrentCursor = m_pCurrentCursor;
			int nCurrentCursorNumber = m_nCurrentCursorNumber;

			// imposta l'intervallo per gli indici da controllare
			int nFirst = 0;
			int nLast = m_Database.table.totindex;
			if(nIndex!=-1)
			{
				nFirst = nIndex;
				nLast = nIndex+1;
			}

			char szBuffer[MAX_KEYSIZE+1] = {0};
			char szPrimaryKey[MAX_KEYSIZE+1] = {0};
			int nPrimaryKeySize = GetPrimaryKeySize();
			char szSecondaryKey[MAX_KEYSIZE+1] = {0};
			int nSecondaryKeyField = -1;
			int nSecondaryKeySize = -1;
			DB_INFO db_info = {0};
			db_info.flags = DB_DUP|DB_DUPSORT;

			// per ognuno degli indici da controllare
			for(int i = nFirst; i < nLast && nRet==DB_NO_ERROR; i++)
			{
				// ricava il numero (progressivo) e la dimensione del campo
				// su cui e' costruito l'indice secondario (chiave secondaria)
				if(nRet==DB_NO_ERROR)
				{
					nSecondaryKeyField = GetSecondaryKeyField(i);
					if(nSecondaryKeyField < 0 || nSecondaryKeyField >= MAX_FIELDCOUNT)
					{
						if(nRet==DB_NO_ERROR)
							nRet = DB_RETCODE_EINVALIDFIELDNUMBER;
					}
				}
				if(nRet==DB_NO_ERROR)
				{
					nSecondaryKeySize = GetSecondaryKeySize(nSecondaryKeyField);
					if(nSecondaryKeySize <= 0 || nSecondaryKeySize > MAX_KEYSIZE)
					{
						if(nRet==DB_NO_ERROR)
							nRet = DB_RETCODE_EINVALIDFIELDSIZE;
					}
				}

				if(nRet==DB_NO_ERROR)
				{
					// azzera la struttura key/data usata per la ricerca
					ResetPair();

					bool bFound = false;

					// cicla per ognuno dei record presenti nella tabella
					if((nRet = m_pDbCursor->c_get(m_pDbCursor,&m_Key,&m_Data,DB_FIRST))==DB_NO_ERROR)
						do {
							// passa i valori alla struttura
							MemoryToRecord();
							
							// ricava il valore della chiave primaria
							memset(szBuffer,'\0',sizeof(szBuffer));
							memset(szPrimaryKey,' ',sizeof(szPrimaryKey));
							memcpy(szBuffer,GetPrimaryKeyData(),nPrimaryKeySize);
							szBuffer[nPrimaryKeySize] = '\0';
							memcpy(szPrimaryKey,szBuffer,strlen(szBuffer));

							// ricava il valore della chiave secondaria
							memset(szBuffer,'\0',sizeof(szBuffer));
							memset(szSecondaryKey,' ',sizeof(szSecondaryKey));
							memcpy(szBuffer,GetSecondaryKeyData(nSecondaryKeyField),nSecondaryKeySize);
							szBuffer[nSecondaryKeySize] = '\0';
							memcpy(szSecondaryKey,szBuffer,strlen(szBuffer));

							// imposta la chiave (secondaria) con il valore del campo usato come indice secondario
							m_Key.data = szSecondaryKey;
							m_Key.size = nSecondaryKeySize;

							// imposta i dati (la chiave primaria)
							m_Data.data = szPrimaryKey;
							m_Data.size = nPrimaryKeySize;

							if(m_pIdxCursorArray[i])
							{
								if((nRet = m_pIdxCursorArray[i]->c_get(m_pIdxCursorArray[i],&m_Key,&m_Data,DB_SET))==DB_NO_ERROR)
								{
									// scorre i record trovati cercando quello con chiave primaria uguale a quella impostata sopra
									do {
										if(memcmp(szPrimaryKey,(char*)m_Data.data,nPrimaryKeySize)==0)
										{
											// in teoria deve esistere un solo record con la stessa chiave primaria
											bFound = true;
											break;
										}
									} while((nRet = m_pIdxCursorArray[i]->c_get(m_pIdxCursorArray[i],&m_Key,&m_Data,DB_NEXT))==DB_NO_ERROR);
								}

								if(!bFound)
								{
									if(nRet==DB_NO_ERROR)
										nRet = DB_RETCODE_ESECONDARYKEYNOTFOUND;
									break;
								}
							}
							else
							{
								if(nRet==DB_NO_ERROR)
									nRet = DB_RETCODE_EINVALIDCURSOR;
							}

							// azzera la struttura key/data usata per la ricerca
							ResetPair();

						} while((nRet = m_pDbCursor->c_get(m_pDbCursor,&m_Key,&m_Data,DB_NEXT))==DB_NO_ERROR);

					if(bFound)
						nRet = DB_NO_ERROR;
				}
			}
			
			// reimposta il cursore anteriore
			if(nRet==DB_NO_ERROR)
			{
				m_pCurrentCursor = pCurrentCursor;
				m_nCurrentCursorNumber = nCurrentCursorNumber;
			}
		}
	}
	else
	{
		if(nRet==DB_NO_ERROR)
			nRet = DB_RETCODE_ETABLENOTOPEN;
	}

	if(nRet!=DB_NO_ERROR)
		SetLastError(nRet);

	return(nRet);
}

/*
	SetCursor()

	Imposta il cursore (indice) corrente.
	Il cursore selezionato fa riferimento all'indice secondario.
	Per resettare l'indice secondario (accesso alla tabella secondo l'ordine di inserimento, senza indici),
	reimpostare il cursore su quello relativo alla chiave primaria con ResetCursor().
	La coppia GetCursor() e GetCursorNumber() restituiscono -1 quando il cursore corrente e' stato impostato
	sulla chiave primaria della tabella (ossia quando e' stato resettato l'indice secondario).
*/
int CBerkeleyDB::SetCursor(int i)
{
	int nRet = DB_NO_ERROR;

	ASSERTEXPR(IsOpen());
	if(IsOpen())
	{
		if(m_Database.table.totindex > 0)
		{
			if(i >= 0 && i < m_Database.table.totindex)
			{
				m_pCurrentCursor = m_pIdxCursorArray[i];
				m_nCurrentCursorNumber = i;
			}
			else
			{
				if(nRet==DB_NO_ERROR)
					nRet = DB_RETCODE_EINVALIDINDEX;
			}
		}
	}
	else
	{
		if(nRet==DB_NO_ERROR)
			nRet = DB_RETCODE_ETABLENOTOPEN;
	}

	if(nRet!=DB_NO_ERROR)
		SetLastError(nRet);

	return(nRet);
}

/*
	GetPrimaryKeyData()
*/
char* CBerkeleyDB::GetPrimaryKeyData(void) const
{
	ASSERTEXPR(m_Database.table.row && m_Database.table.totfield > 0);

	if(!m_Database.table.row || m_Database.table.totfield <= 0)
		return(NULL);

	return(m_Database.table.row[0].value);
}

/*
	GetSecondaryKeyData()
*/
char* CBerkeleyDB::GetSecondaryKeyData(int nFieldNum) const
{
	ASSERTEXPR(nFieldNum >= 0 && nFieldNum < m_Database.table.totfield);

	return(m_Database.table.row[nFieldNum].value);
}

/*
	GetSecondaryKeySize()
*/
int	CBerkeleyDB::GetSecondaryKeySize(int nFieldNum) const
{
	ASSERTEXPR(nFieldNum >= 0 && nFieldNum < m_Database.table.totfield);

	return(m_Database.table.row[nFieldNum].size);
}

/*
	GetSecondaryKeyField()
*/
int	CBerkeleyDB::GetSecondaryKeyField(int nIndex) const
{
	ASSERTEXPR(nIndex >= 0 && nIndex < m_Database.table.totindex);

	return(m_Database.table.index[nIndex].fieldnum);
}

/*
	Get()

	Recupera la chiave.

	-1	DB_NOTFOUND			Fine della tabella o chiave inesistente.
	-2	DB_KEYEMPTY			Record eliminato (buco nell'indice).Variabile
	?	DB_LOCK_DEADLOCK	Conflitto tra processi. Riprova.Variabile
	?	DB_RUNRECOVERY		Database corrotto. Panico/Reset.Variabile
	?	DB_BUFFER_SMALL		Il buffer di memoria è troppo piccolo per il record.
*/
int CBerkeleyDB::Get(u_int32_t db_goto_flag/*DB_SEARCH(usa DB_SET)|DB_FIRST|DB_NEXT|DB_PREV|DB_LAST*/,u_int32_t db_set_flag/*=DB_SET -> DB_SET|DB_SET_RANGE*/)
{
	int nRet = DB_NO_ERROR;

	ASSERTEXPR(IsOpen());
	if(IsOpen())
	{
		DBC* pCursor = GetCursor();
		int nCursor = GetCursorNumber();

		// se chiamato con il flag relativo a DB_FIRST|DB_NEXT|DB_PREV|DB_LAST, azzera la coppia key/data e posiziona
		// sul record specificato, impostando la coppia key/data
		// se chiamato con il flag relativo a DB_SEARCH, non azzera la coppia key/data dato che deve cercare quanto specificato
		// usando il flag DB_SET|DB_SET_RANGE
		// per quanto riguarda la ricerca della chiave secondaria, se chiamato con DB_FIRST|DB_NEXT|DB_PREV|DB_LAST
		// (deve impostare la coppia key/data) deve andare direttamente al record (puo' esistere solo un record all'inizio,
		// fine, etc.) con DB_SET
		// se chiamato con DB_SEARCH (sta' cercando la chiave specificata da key/data) puo' ricercare uno o il record piu'
		// simile con DB_SET|DB_SET_RANGE
		if(db_goto_flag!=DB_SEARCH)
			ResetPair();

		// recupera la chiave corrente con il cursore (indice) corrente
		// assume che sia stata gia' chiamata PutKey() per impostare il valore della chiave corrente
		if((nRet = pCursor->c_get(pCursor,&m_Key,&m_Data,db_goto_flag==DB_SEARCH ? db_set_flag : db_goto_flag))==DB_NO_ERROR)
		{
			// se il cursore corrente non e' la chiave primaria cerca il record con la chiave secondaria
			if(nCursor >= 0)
				nRet = GetSecondaryKey(nCursor,db_goto_flag!=DB_SEARCH ? DB_SET : db_set_flag);
		}

		// legge il record associato alla chiave nel record
		if(nRet==DB_NO_ERROR)
		{
			MemoryToRecord();
		}
		else
		{
			TRACEEXPR((_TRACE_FLAG_ERR,__FILE__,__LINE__,"Get() failed: cursor=%d, flag=%u, error=%d\n",nCursor,db_goto_flag,nRet));
			ResetRecord();
		}
	}
	else
	{
		if(nRet==DB_NO_ERROR)
			nRet = DB_RETCODE_ETABLENOTOPEN;
	}

	if(nRet!=DB_NO_ERROR)
		SetLastError(nRet);

	return(nRet);
}

/*
	GetRecordNumber()

	Ricava il numero del record corrente.

	Tenere a mente che in realta' non si tratta del numero del record fisico, ma dei dati relativi 
	al cursore, dato che BDB usa il concetto di indici B-Tree, dove i records sono ordinati per 
	chiave e memorizzati in pagine, per cui il loro ordine fisico puo' cambiare, a differenza di 
	una tabella standard dove i records sono memorizzati con un indice sequenziale assoluto.
*/
int CBerkeleyDB::GetRecordNumber(db_recno_t& ulRecNo)
{
	int ret = 0;

	// usa variabili locali, non tocca i membri m_Key e m_Data
	DBT dbt_data = {0};
	DBT dbt_key = {0};
	dbt_key.data = &ulRecNo;
	dbt_key.size = sizeof(db_recno_t);

	// legge la chiave interna del record corrente e la salva in ulRecNo
	ret = m_pDbCursor->c_get(	m_pDbCursor, 
								&dbt_key,		// dove salvare la chiave interna
								&dbt_data,		// dati
								DB_GET_RECNO);	// ricava la posizione del cursore

	return(ret);
}

/*
	SetRecordNumber()

	Imposta il numero del record corrente, ricavandi quindi i dati relativi.

	Vedi le note in GetRecordNumber().
*/
int CBerkeleyDB::SetRecordNumber(db_recno_t ulRecNo)
{
	int ret = 0;
	
	// usa la chiave interna salvata anteriormante
	m_Key.data = &ulRecNo;
	m_Key.size = sizeof(db_recno_t);

	// riposiziona il cursore al rec. no. memorizzato
	ret = m_pDbCursor->c_get(	m_pDbCursor,
								&m_Key,			// chiave interna salvata in precedenza
								&m_Data,		// dati
								DB_SET_RECNO);	// imposta il cursore


	return(ret);
}

/*
	GetRecordCountByCursor()

	Restituisce il totale dei record presenti nella tabella utilizzando dbc->stat().
	Il lock RAII deve essere attivo PRIMA di chiamare questa funzione.

	ATTENZIONE: in Berkeley DB 2.7.7, quando si usa un cursore per scansionare il database (DB_FIRST 
	seguito da DB_NEXT), il motore interno di BDB salta automaticamente i record marcati come eliminati 
	(i 'buchi' o tombstones).
	Mentre db_stat (vedi sotto la versione originale del conteggio) legge i metadati fisici dell'albero 
	(che includono i record eliminati finche la pagina non viene riorganizzata), il cursore esegue una 
	navigazione logica, ottenendo quindi il numero reale di records.

	La soluzione al conteggio potrebbe essere usare il flag DB_RECNUM nella struttura db_info, che assegna 
	un numero d'ordine logico a ogni chiave nell'albero. Pero' se si usa il flag per le chiavi duplicate 
	(DB_DUP), allora la struttura dell'albero diventa molto piu' complessa (sotto-alberi di duplicati) e 
	BerkeleyDB non e' piu' in grado di mantenere un conteggio lineare affidabile che mappi i numeri di 
	record in presenza di duplicati. Per questo i flag DB_RECNUM e DB_DUP sono incompatibili.
*/
unsigned long CBerkeleyDB::GetRecordCountByCursor(void)
{
    unsigned long ulCount = 0L;
    DBC *pCursor = NULL;
    DBT key = {0};
	DBT data = {0};
    int ret = 0;

	// per velocizzare la scansione, si puo' dire a BerkeleyDB di non recuperare i dati, ma solo di 
	// verificare l'esistenza del record, usando il flag DB_DBT_PARTIAL nelle DBT
    data.flags = DB_DBT_PARTIAL;
    data.dlen = data.doff = 0;

    if(m_pDbHandle->cursor(m_pDbHandle,NULL,&pCursor,0)!=0)
		return(0L);

    ret = pCursor->c_get(pCursor,&key,&data,DB_FIRST);
	while(ret==0)
	{
		ulCount++;
        ret = pCursor->c_get(pCursor,&key,&data,DB_NEXT);
    }
    pCursor->c_close(pCursor);

    return(ulCount);
}
/*
	GetRecordCount()

	Restituisce il totale dei record presenti nella tabella utilizzando dbc->stat().
	Il lock RAII deve essere attivo PRIMA di chiamare questa funzione.

	ATTENZIONE: dato che usa l'handle della tabella, conteggia anche i 'buchi', ossia i record
	eliminati, che BerkeleyDB non elimina fisicamente dal b-tree ma semplicemente lascia marcati
	per un prossimo riutilizzo. Vedi sopra la nuova versione per il conteggio.
*/
unsigned long CBerkeleyDB::GetRecordCount(void)
{
	DB_BTREE_STAT* db_stat = NULL;
	unsigned long ulCount = 0L;
	int ret = 0;

	// usa l'handle del database
	if(m_pDbHandle)
	{
		// chiama DB->stat():
		// - primo argomento:	handle del database
		// - secondo argomento:	puntatore alla variabile che ricevera' l'indirizzo della struttura allocata (&db_stat)
		// - terzo argomento:	puntatore alla funzione di allocazione (malloc)
		// - quarto argomento:	flag (0, dato che DB_FAST_STAT non esiste)
		ret = m_pDbHandle->stat(m_pDbHandle,
								&db_stat, 
								(void *(*)(size_t))malloc, // cast esplicito del puntatore a funzione
								0);

		if(ret==0 && db_stat)
		{
			// recupera il conteggio totale dal campo bt_nrecs
			ulCount = db_stat->bt_nrecs; 

			// OCCHIO: libera la memoria allocata da BDB tramite la malloc
			free(db_stat);
		}
	}

	return(ulCount);
}

/*
	GetPrimaryKey()

	Ricerca la chiave primaria (assoluta).
*/
int CBerkeleyDB::GetPrimaryKey(const char* pPrimaryKey)
{
	int nRet = DB_NO_ERROR;
	
	// dato che cerca la chiave primaria (assoluta), la tabella deve prevedere indici
	if(m_Database.table.totindex > 0)
	{
		// azzera la struttura key/data
		ResetPair();
		
		// imposta la chiave per la ricerca
		m_Key.data = (void*)pPrimaryKey;
		m_Key.size = MAX_PRIMARYKEY_SIZE;

		// cerca la chiave primaria (assoluta) sulla tabella principale
		if((nRet = m_pDbCursor->c_get(m_pDbCursor,&m_Key,&m_Data,DB_SET))==DB_NO_ERROR)
		{
			MemoryToRecord();
		}
		else
		{
			if(nRet==DB_NO_ERROR)
				nRet = DB_RETCODE_NOTFOUND;
			ResetRecord();
		}
	}
	else
	{
		if(nRet==DB_NO_ERROR)
			nRet = DB_RETCODE_EINDEXREQUIRED;
	}

	if(nRet!=DB_NO_ERROR)
		SetLastError(nRet);

	return(nRet);
}

/*
	GetSecondaryKey()

	Effettua la ricerca per chiave secondaria.
*/
int CBerkeleyDB::GetSecondaryKey(int nIndex,u_int32_t db_set_flag/*=DB_SET -> DB_SET|DB_SET_RANGE*/)
{
	int	nRet = DB_NO_ERROR;
	
	int	nFieldNum = -1;
	char szBuffer[MAX_KEYSIZE+1] = {0};
	char szPrimaryKey[MAX_KEYSIZE+1] = {0};
	char szSecondaryKey[MAX_KEYSIZE+1] = {0};
	int	nPrimaryKeyLen = -1;
	int nSecondaryKeyLen = -1;
	DBC* pCursor = m_pDbCursor;

	// controlla che il numero dell'indice sia valido
	if(nIndex >= 0 && nIndex < m_Database.table.totindex)
		nFieldNum = m_Database.table.index[nIndex].fieldnum;
	else
	{
		if(nRet==DB_NO_ERROR)
			nRet = DB_RETCODE_EINVALIDINDEX;
	}

	// controlla che il numero del campo sia valido
	if(nRet==DB_NO_ERROR)
	{
		if(nFieldNum >= 0 && nFieldNum < m_Database.table.totfield)
		{
			// la prima ricerca, quella che ha riempito key/data, e' stata effettuata con una delle chiavi
			// secondarie ottenendo in m_Data.data la chiave primaria
			
			// recupera la chiave secondaria (presente in m_Key.data)
			nSecondaryKeyLen = GetSecondaryKeySize(nFieldNum);
			nSecondaryKeyLen = nSecondaryKeyLen > 0 ? nSecondaryKeyLen : MAX_KEYSIZE;
			nSecondaryKeyLen = (nSecondaryKeyLen <= MAX_KEYSIZE) ? nSecondaryKeyLen : MAX_KEYSIZE;
			
			memset(szBuffer,'\0',sizeof(szBuffer));
			memcpy(szBuffer,(char*)m_Key.data,nSecondaryKeyLen);
			
			memset(szSecondaryKey,' ',sizeof(szSecondaryKey));
			memcpy(szSecondaryKey,szBuffer,strlen(szBuffer));
			szSecondaryKey[nSecondaryKeyLen] = '\0';

			// recupera la chiave primaria (presente in m_Data.data)
			nPrimaryKeyLen = GetPrimaryKeySize();
			nPrimaryKeyLen = nPrimaryKeyLen > 0 ? nPrimaryKeyLen : MAX_KEYSIZE;
			nPrimaryKeyLen = (nPrimaryKeyLen <= MAX_KEYSIZE) ? nPrimaryKeyLen : MAX_KEYSIZE;

			memset(szBuffer,'\0',sizeof(szBuffer));
			memcpy(szBuffer,(char*)m_Data.data,nPrimaryKeyLen);
			
			memset(szPrimaryKey,' ',sizeof(szPrimaryKey));
			memcpy(szPrimaryKey,szBuffer,strlen(szBuffer));
			szPrimaryKey[nPrimaryKeyLen] = 0;

			// imposta la coppia dati/dimensione per la chiave con la chiave primaria (assoluta)
			ResetPair();
			m_Key.data = szPrimaryKey;
			m_Key.size = nPrimaryKeyLen;

			// cerca il record con chiave primaria uguale a quella ricavata con la ricerca effettuata
			// (nel chiamante) con l'indice secondario
			// (in altre parole assume che la coppia key/data sia stata riempita dal chiamante con una
			// ricerca per chiave secondaria)
			// non carica/azzera il record (deve farlo il chiamante al ritorno)
			// non effettua nessun ciclo perche' in teoria, nell'indice secondario, la chiave primaria
			// non deve esistere duplicata
			nRet = pCursor->c_get(pCursor,&m_Key,&m_Data,db_set_flag);
		}
		else
		{
			if(nRet==DB_NO_ERROR)
				nRet = DB_RETCODE_EINVALIDFIELDNUMBER;
		}
	}

	if(nRet!=DB_NO_ERROR)
		SetLastError(nRet);

	return(nRet);
}

/*
	PutKey()

	Imposta il valore per la chiave.
*/
void CBerkeleyDB::PutKey(char *pValue,int nIndex)
{
	int	nRet = DB_NO_ERROR;
	
	// da mem a key(.data/.size)
	// imposta il valore della chiave (relativa all'indice) per la ricerca
	int nFieldNum = -1;
	int nKeyLen,nKeySize;

	ResetKeyPair();

	// se la tabella non prevede indici assume il primo campo (base 0)
	if(m_Database.table.totindex > 0)
	{
		if(nIndex >= 0 && nIndex < m_Database.table.totindex)
		{
			nFieldNum = m_Database.table.index[nIndex].fieldnum;
			nFieldNum = nFieldNum >= 0 && nFieldNum < MAX_FIELDCOUNT ? nFieldNum : -1;
		}
	}
	else
		nFieldNum = 0;

	// controlla che il numero del campo sia valido
	if(nFieldNum >= 0 && nFieldNum < m_Database.table.totfield)
	{
		// imposta i valori (dati/dimensione) per la chiave
		nKeySize = m_Database.table.row[nFieldNum].size;
		if(nKeySize <= 0 || nKeySize > MAX_KEYSIZE)
			nKeySize = MAX_KEYSIZE;
		nKeyLen = (int)strlen(pValue);
		if(nKeyLen > nKeySize)
			nKeyLen = nKeySize;

		memset(m_szPutKeyBuffer,' ',MAX_KEYSIZE);
		memcpy(m_szPutKeyBuffer,pValue,nKeyLen);
		m_szPutKeyBuffer[MAX_KEYSIZE] = '\0';

		m_Key.data = m_szPutKeyBuffer;
		m_Key.size = nKeySize;
	}
	else
	{
		if(nRet==DB_NO_ERROR)
			nRet = DB_RETCODE_EINVALIDFIELDNUMBER;
	}

	if(nRet!=DB_NO_ERROR)
		SetLastError(nRet);
}

/*
	GetField()

	Ricava il valore del campo relativo al numero progressivo (a base 0)
    e lo copia nel buffer passato dal chiamante.
*/
bool CBerkeleyDB::GetField(int nFieldNum,char* pBuffer,int nSize)
{
	int	nRet = DB_NO_ERROR;
	
	if(nFieldNum >= 0 && nFieldNum < m_Database.table.totfield)
	{
		int nLen = m_Database.table.row[nFieldNum].size;

		// controlla che il buffer fornito dal chiamante sia sufficiente
		if(nLen <= MAX_FIELDSIZE && nLen < nSize)
		{
			// copia il contenuto del campo del record (formattato a spazi)
			memset(pBuffer,' ',nSize);
			memcpy(pBuffer,m_Database.table.row[nFieldNum].value,nLen);
			
			// termina a NULL subito dopo i dati
			pBuffer[nLen] = '\0';
			
			return(true);
		}
		else
		{
			if(nRet==DB_NO_ERROR)
				nRet = DB_RETCODE_EINVALIDFIELDSIZE;
		}
	}
	else
	{
		if(nRet==DB_NO_ERROR)
			nRet = DB_RETCODE_EINVALIDFIELDNUMBER;
	}

	if(nRet!=DB_NO_ERROR)
		SetLastError(nRet);

	return(false);
}

/*
	PutField()

	Imposta il valore per il campo.
*/
void CBerkeleyDB::PutField(int nFieldNum,const char* pValue)
{
	int	nRet = DB_NO_ERROR;
	
	// da mem a rec->field
	if(nFieldNum >= 0 && nFieldNum < m_Database.table.totfield)
	{
		if(m_Database.table.row[nFieldNum].size <= MAX_FIELDSIZE)
		{
			int nLen = (int)strlen(pValue);
			nLen = nLen > m_Database.table.row[nFieldNum].size ? m_Database.table.row[nFieldNum].size : nLen;

			// passa il contenuto del buffer nel campo del record
			// (prima di effettuare la copia formatta il campo del record a spazi)
			memset(m_Database.table.row[nFieldNum].value,' ',m_Database.table.row[nFieldNum].size);
			memcpy(m_Database.table.row[nFieldNum].value,pValue,nLen);
		}
		else
		{
			if(nRet==DB_NO_ERROR)
				nRet = DB_RETCODE_EINVALIDFIELDSIZE;
		}
	}
	else
	{
		if(nRet==DB_NO_ERROR)
			nRet = DB_RETCODE_EINVALIDFIELDNUMBER;
	}

	if(nRet!=DB_NO_ERROR)
		SetLastError(nRet);
}

/*
	SetLastError()

	Imposta il codice d'errore.
*/
int CBerkeleyDB::SetLastError(int nCode)
{
	bool bInternal = false;

	// normalizza il codice di ritorno (rimappa quelli della BerkeleyDB sui propri)
	if(nCode < DB_OK)
	{
		bInternal = true;

		switch(nCode)
		{
			case DB_NO_ERROR:
				nCode = DB_OK;
				break;
			case DB_NOTFOUND:
				nCode = DB_RETCODE_NOTFOUND;
				break;
			case DB_RUNRECOVERY:
				nCode = DB_RETCODE_EINVALIDHANDLE;
				break;
			case ENOMEM:
				nCode = DB_RETCODE_EOUTOFMEMORY;
				break;
			case EAGAIN:
				nCode = DB_RETCODE_ELOCKFAILURE;
				break;
			case EINVAL:
				nCode = DB_RETCODE_EINVALIDPARAM;
				break;
			case ENOENT:
				nCode = DB_RETCODE_EINVALIDRESOURCE;
				break;
		}
	}

	// codice numerico
	m_Database.table.stat.errnum = nCode;
	
	// codice stringa
	snprintf(m_Database.table.stat.errstr,sizeof(m_Database.table.stat.errstr),"error code: %d",nCode);

	for(int i=0; i < ARRAY_SIZE(db_error); i++)
	{
		if(db_error[i].code==nCode)
		{
			strcpyn(m_Database.table.stat.errstr,db_error[i].desc,sizeof(m_Database.table.stat.errstr));
			break;
		}
	}

#if defined(_WINDOWS)
	//if(nCode >= DB_RETCODE_STATUS_BASE)
	if(nCode >= DB_RETCODE_ERROR_BASE)
		if(m_bShowErrors)
			::MessageBox(NULL,m_Database.table.stat.errstr,bInternal ? "CBerkeleyDB::SetLastError()" : "SetLastError()",MB_OK|MB_ICONERROR|MB_TASKMODAL|MB_SETFOREGROUND|MB_TOPMOST);
#endif

	return(nCode);
}

/*
	ResetLastError()
*/
void CBerkeleyDB::ResetLastError(void)
{
	m_Database.table.stat.errnum = 0;
	memset(&(m_Database.table.stat.errstr),'\0',sizeof(m_Database.table.stat.errstr));
}

/*
	GetLastError()
*/
int CBerkeleyDB::GetLastError(void)
{
	return(m_Database.table.stat.errnum);
}

/*
	GetLastErrorString()
*/
const char* CBerkeleyDB::GetLastErrorString(void)
{
	return((const char*)m_Database.table.stat.errstr);
}

/*
	SyncDatabase()
*/
int	CBerkeleyDB::SyncDatabase(bool bFlush/* = false*/)
{
	return(SyncDb(bFlush));
}

/*
	SyncIndex()
*/
int	CBerkeleyDB::SyncIndex(int nIndex,bool bFlush/* = false*/)
{
	ASSERTEXPR(nIndex < m_Database.table.totindex);

	return(SyncIdx(nIndex,bFlush));
}

/*
	SyncIndexAll()
*/
int	CBerkeleyDB::SyncIndexAll(bool bFlush/* = false*/)
{
	int nRet = DB_NO_ERROR;
	
	for(int i=0; i < m_Database.table.totindex; i++)
	{
		int ret = SyncIdx(i,bFlush);
		if(ret!=DB_NO_ERROR)
			nRet = ret;
	}

	if(nRet!=DB_NO_ERROR)
		SetLastError(nRet);

	return(nRet);
}

/*
	SyncDb()
*/
int CBerkeleyDB::SyncDb(bool bFlush/*=false*/)
{
	int nRet = DB_NO_ERROR;
	
	// ricava il cursore corrente
	DBC* pCurrentCursor = m_pCurrentCursor;
	int nCurrentCursorNumber = m_nCurrentCursorNumber;

	// flush veloce
	if(!bFlush)
	{
		// usa DB::sync() per garantire la registrazione su disco
		if(nRet==DB_NO_ERROR)
			nRet = m_pDbHandle->sync(m_pDbHandle,0); // 0 per sincronizzazione standard
	}
	// flush totale (chiude/riapre l'handle)
	else
	{
		// chiude il cursore
		if(nRet==DB_NO_ERROR)
			nRet = m_pDbCursor->c_close(m_pDbCursor);

		// chiude la tabella (forza sincronizzazione e rilascia l'handle)
		if(nRet==DB_NO_ERROR)
			nRet = m_pDbHandle->close(m_pDbHandle,0);

		// riapre la tabella
		if(nRet==DB_NO_ERROR)
		{
			DB_INFO db_info = {0};
			// logica originale per DB_DUP|DB_DUPSORT
			db_info.flags = (m_Database.table.totindex > 0) ? 0 : DB_DUP|DB_DUPSORT;
            
            // l'environment e' NULL in modalita' non-transazionale
			nRet = db_open(m_Database.table.filename,DB_BTREE,0,0664,NULL,&db_info,&m_pDbHandle);
		}

		// riapre il cursore
		if(nRet==DB_NO_ERROR)
			nRet = m_pDbHandle->cursor(m_pDbHandle,NULL,&m_pDbCursor,0);
	}

	// reimposta il cursore anteriore
	if(nRet==DB_NO_ERROR) /*DB_RETCODE_ESYNCFAILURE*/
	{
		m_pCurrentCursor = pCurrentCursor;
		m_nCurrentCursorNumber = nCurrentCursorNumber;
	}

	if(nRet!=DB_NO_ERROR)
		SetLastError(nRet);

	return(nRet);
}

/*
	SyncIdx()
*/
int CBerkeleyDB::SyncIdx(int nIndex,bool bFlush/*=false*/)
{
	int nRet = DB_NO_ERROR;

	// indice fuori range
	if(nIndex < 0 || nIndex >= m_Database.table.totindex)
		return(SetLastError(DB_RETCODE_EINVALIDINDEX));
	
	// ricava il cursore corrente
	DBC* pCurrentCursor = m_pCurrentCursor;
	int nCurrentCursorNumber = m_nCurrentCursorNumber;

	// flush veloce
	if(!bFlush)
	{
		// Usa DB::sync() sull'handle dell'indice per forzare la scrittura
		if(nRet==DB_NO_ERROR)
			nRet = m_pIdxHandleArray[nIndex]->sync(m_pIdxHandleArray[nIndex],0);
	}
	// flush totale (chiude/riapre l'handle)
	else
	{
		// chiude il cursore
		if(nRet==DB_NO_ERROR)
			nRet = m_pIdxCursorArray[nIndex]->c_close(m_pIdxCursorArray[nIndex]);

		// chiude l'indice (forza sincronizzazione e rilascia l'handle)
		if(nRet==DB_NO_ERROR)
			nRet = m_pIdxHandleArray[nIndex]->close(m_pIdxHandleArray[nIndex],0);

		// riapre l'indice
		if(nRet==DB_NO_ERROR)
		{
			DB_INFO db_info = {0};
			db_info.flags = DB_DUP|DB_DUPSORT;
			nRet = db_open(m_Database.table.index[nIndex].filename,DB_BTREE,0,0664,NULL,&db_info,&m_pIdxHandleArray[nIndex]);
		}

		// riapre il cursore
		if(nRet==DB_NO_ERROR)
			nRet = m_pIdxHandleArray[nIndex]->cursor(m_pIdxHandleArray[nIndex],NULL,&m_pIdxCursorArray[nIndex],0);
	}

	// reimposta il cursore anteriore
	if(nRet==DB_NO_ERROR) /*DB_RETCODE_ESYNCFAILURE*/
	{
		m_pCurrentCursor = pCurrentCursor;
		m_nCurrentCursorNumber = nCurrentCursorNumber;
	}

	if(nRet!=DB_NO_ERROR)
		SetLastError(nRet);

	return(nRet);
}

/*
	ResetPair()

	Azzera la coppia chiave/dati.
*/
void CBerkeleyDB::ResetPair(void)
{
	// se venne richiesto a BerkeleyDB di allocare la memoria per la risposta (DB_DBT_MALLOC), la libera
	if(m_Key.flags & DB_DBT_MALLOC)
	{
		free(m_Key.data);
		m_Key.flags = 0;
	}

	if(m_Data.flags & DB_DBT_MALLOC)
	{
		free(m_Data.data);
		m_Data.flags = 0;
	}

	memset(&m_Key,'\0',sizeof(m_Key));
	memset(&m_Data,'\0',sizeof(m_Data));
}

/*
	ResetRecord()

	Azzera i campi del record.
*/
void CBerkeleyDB::ResetRecord(void)
{
	int nRet = DB_NO_ERROR;

	ASSERTEXPR(IsOpen());
	if(IsOpen())
	{
		for(int i = 0; i < m_Database.table.totfield; i++)
			memset(m_Database.table.row[i].value,'\0',m_Database.table.row[i].size);
	}
	else
		nRet = DB_RETCODE_ETABLENOTOPEN;

	if(nRet!=DB_NO_ERROR)
		SetLastError(nRet);
}

/*
	BlankRecord()

	Svuota i campi del record.
*/
void CBerkeleyDB::BlankRecord(void)
{
	int nRet = DB_NO_ERROR;

	ASSERTEXPR(IsOpen());
	if(IsOpen())
	{
		for(int i = 0; i < m_Database.table.totfield; i++)
			memset(m_Database.table.row[i].value,' ',m_Database.table.row[i].size);
	}
	else
		nRet = DB_RETCODE_ETABLENOTOPEN;

	if(nRet!=DB_NO_ERROR)
		SetLastError(nRet);
}

/*
	MemoryToRecord()

	Passa i dati dal buffer interno al record.
*/
void CBerkeleyDB::MemoryToRecord(void)
{
	// passa i dati dal campo data.data al record
	char* p = (char*)m_Data.data;

	// azzera il record
	ResetRecord();

	if(p && *p)
	{
		int i = 0,ofs = 0;

		// se la tabella prevede indici, copia la chiave primaria nel primo campo del record
		if(m_Database.table.totindex > 0 || m_Database.flags==666)
		{
			memcpy(m_Database.table.row[0].value,(char*)m_Key.data,m_Database.table.row[0].size);
			i++;
			ofs = m_Database.table.row[0].size;
		}

		// passa il contenuto del buffer nei campi del record
		// (notare che i valori presenti nel buffer sono gia' formattati a spazio)
		for(; i < m_Database.table.totfield; i++)
			memcpy(m_Database.table.row[i].value,p + m_Database.table.row[i].ofs - ofs,m_Database.table.row[i].size);
	}
}

/*
	RecordToMemory()

	Passa i dati dal record al buffer interno.
*/
void CBerkeleyDB::RecordToMemory(void)
{
	int i,nSize,nFieldSize;

	// azzera il buffer interno per i dati
	memset(m_pDataBuffer,'\0',m_nDataBufferSize);

	// se la tabella prevede indici salta la chiave primaria (non la include nei dati)
	i = m_Database.table.totindex > 0 ? 1 : 0;

	// passa i campi del record nel buffer interno (i valori sono gia' formattati a spazio)
	for(nSize = 0; i < m_Database.table.totfield; i++)
	{
		nFieldSize = m_Database.table.row[i].size;
		nFieldSize = (nFieldSize < (m_nDataBufferSize - 1 - nSize)) ? nFieldSize : (m_nDataBufferSize - 1 - nSize);

		memcpy(	m_pDataBuffer+nSize,
				m_Database.table.row[i].value,
				nFieldSize
				);

		nSize += nFieldSize;
	}

	// passa il contenuto del buffer interno nella struttura
	ResetDataPair();
	m_Data.data = m_pDataBuffer;
	m_Data.size = nSize;
}
