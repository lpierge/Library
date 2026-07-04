/*$
	CBase.cpp
	Classe derivata per l'interfaccia con il database.
	Definisce l'oggetto per il database (CBase) usando la classe che si interfaccia con la libreria (CBerkeleyDB).
	Luca Piergentili, 04/11/99
	lpiergentili@yahoo.com

	Vedi le note in CBase.h

	"Perfection is reached at the point of collapse." (Shawn Lane)
*/
#include "env.h"
#include "pragma.h"
#include "macro.h"
#include <ctype.h>
#include <locale.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "strings.h"
#include "typedef.h"
#if defined(_WINDOWS)
  #include "window.h"
#endif
#include "CSync.h"
#include "CDateTime.h"
#include "CTextFile.h"
#include "CBase.h"
#include "CBerkeleyDB.h"

#include "traceexpr.h"
//#define _TRACE_FLAG			_TRFLAG_TRACEOUTPUT // opzioni: _TRFLAG_NOTRACE, _TRFLAG_TRACEFILE, _TRFLAG_TRACECONSOLE, _TRFLAG_TRACEOUTPUT, _TRFLAG_TRACEBREAKPOINT
#define _TRACE_FLAG			_TRFLAG_NOTRACE // opzioni: _TRFLAG_NOTRACE, _TRFLAG_TRACEFILE, _TRFLAG_TRACECONSOLE, _TRFLAG_TRACEOUTPUT, _TRFLAG_TRACEBREAKPOINT
#define _TRACE_FLAG_INFO	_TRACE_FLAG
#define _TRACE_FLAG_WARN	_TRACE_FLAG
#define _TRACE_FLAG_ERR		_TRACE_FLAG

/*
	CBase()
*/
CBase::CBase()
{
	TRACEEXPR((_TRACE_FLAG_INFO,__FILE__,__LINE__,"CBase: CTOR\n"));

	// il membro m_bIsValid riflette la validita' dell'oggetto per la tabella BerkeleyDB
	m_bIsValid = FALSE;
	m_pDatabase = NULL;
	m_pBerkeleyDB = new CBerkeleyDB();
	ASSERTEXPR(m_pBerkeleyDB);
	if(m_pBerkeleyDB)
	{
		m_bIsValid = TRUE;
		m_pDatabase = m_pBerkeleyDB->GetDatabase();
	}
	ASSERTEXPR(m_bIsValid);
	ASSERTEXPR(m_pDatabase);

	m_nLastError = DB_NO_ERROR;

	m_pTableStruct = NULL;
	m_nTotRows = -1;
	m_pIdxStruct = NULL;
	m_nTotIdx = -1;

	m_pRecordBuffer = NULL;

	bzero(&m_Field,sizeof(m_Field));

	strzero(m_szPrimaryKeyValue,sizeof(m_szPrimaryKeyValue));

	m_bSoftseek = FALSE;

	m_DateTime.SetDateFormat(BRITISH);
	m_DateTime.SetTimeFormat(HHMMSS);
	m_bCentury = TRUE;

#ifdef _USE_FIELD_PICTURES
	m_pPictureNumber = m_pPictureChar = m_pPicturePunct = m_pPictureUserDefined = NULL;
#endif
}

/*
	~CBase()
*/
CBase::~CBase()
{
	TRACEEXPR((_TRACE_FLAG_INFO,__FILE__,__LINE__,"CBase: DTOR\n"));

	Close();

	// record per la tabella
	if(m_pRecordBuffer)
		delete [] m_pRecordBuffer;

	// strutture per creazione dinamica
	if(m_pTableStruct && m_nTotRows!=-1)
	{
		for(int i=0; i < m_nTotRows; i++)
			if(m_pTableStruct[i].field)
				delete [] m_pTableStruct[i].field;
		delete [] m_pTableStruct;
	}
	if(m_pIdxStruct && m_nTotIdx!=-1)
	{
		for(int i=0; i < m_nTotIdx; i++)
		{
			if(m_pIdxStruct[i].field)
				delete [] m_pIdxStruct[i].field;
			if(m_pIdxStruct[i].name)
				delete [] m_pIdxStruct[i].name;
			if(m_pIdxStruct[i].file)
				delete [] m_pIdxStruct[i].file;
		}
		delete [] m_pIdxStruct;
	}

	// struttura per la tabella
	if(m_pDatabase && m_pDatabase->table.row)
		delete [] m_pDatabase->table.row;
	if(m_pDatabase && m_pDatabase->table.index)
		delete [] m_pDatabase->table.index;

	// oggetto per la tabella
	if(m_pBerkeleyDB)
		delete m_pBerkeleyDB;

#ifdef _USE_FIELD_PICTURES
	// pictures
	if(m_pPictureNumber)
		delete [] m_pPictureNumber;
	if(m_pPictureChar)
		delete [] m_pPictureChar;
	if(m_pPicturePunct)
		delete [] m_pPicturePunct;
	if(m_pPictureUserDefined)
		delete [] m_pPictureUserDefined;
#endif
}

/*
	Lock()

	Blocca l'accesso alla tabella tramite un mutex.

	Originariamente la classe esponeva i metodi per il lock/unlock della tabella, per essere usati a livello 
	dell'applicativo, pero' in base al principio di separazione delle responsabilità (SRP), la logica di business 
	(classe B) + applicazione finale) deve essere completamente separata dalla logica di persistenza e concorrenza 
	dei dati che viene implementata qui.

	In altre parole, la protezione dei dati (il lock o, a livello piu' basso, in CBerkeleyDB, la transazione) non 
	e' una proprietà della logica applicativa (classe B) + applicazione finale), ma e' una proprieta' della risorsa 
	che si gestisce qui (ossia la tabella, di cui CBase fornisce il modello astratto).

	Per questo la classe controlla ora direttamente l'accesso esclusivo/condiviso ai dati (ossia alla tabella)
	tramite il meccanismo RAII di lock guard (vedi la classe CLockGuard), qui, e non in CBerkeleyDB, perche' questa
	ultima solo deve occuparsi dell'interfaccia con la libreria BDB.
*/
BOOL CBase::Lock(int nTimeout/*=0*/)
{
	// controlla che il costruttore abbia creato l'oggetto per la tabella
	ASSERTEXPR(IsValid());
	if(!IsValid())
		return(FALSE);

	// nomina, se necessario, il mutex per il lock sulla tabella
	if(!m_mutexTable.GetName())
	{
		char szMutexName[_MAX_PATH+1] = {0};
		snprintf(szMutexName,sizeof(szMutexName),"%s\\%s_Mutex",GetTablePath(),GetTableName());
		m_mutexTable.SetName(szMutexName);
	}

	// effettua il lock
	return(m_mutexTable.Lock(nTimeout));
}

/*
	Unlock()

	Sblocca l'accesso alla tabella precedentemente bloccata tramite un mutex.

	Vedi le note su Lock().
*/
BOOL CBase::Unlock(void)
{
	// controlla che il costruttore abbia creato l'oggetto per la tabella
	ASSERTEXPR(IsValid());
	if(!IsValid())
		return(FALSE);
	
	// sblocca
	return(m_mutexTable.Unlock());
}

/*
	Create()

	Crea e carica le strutture per la tabella e gli indici, associandole all'oggetto per il database.
	Il nome della tabella deve contenere il nome/pathname completo di estensione.

	Se viene passato il nome del file contenente la definizione della tabella, crea e carica le strutture
	dinamicamente (in tal caso i puntatori alle strutture per la tabella e gli indici devono essere a NULL).

	Il file con la definizione della tabella (.def) deve contenere quanto segue (esempio):
	tablename=clienti.db
	totfields=5
	field0=PRIMARY_KEY,C,10,0,0
	field1=NOME,C,32,0,1
	field2=COGNOME,C,32,0,2
	field3=INDIRIZZO,C,64,0,3
	field4=CITTA,C,32,0,4
	totindex=4
	index0=clienti.nome.idx,IDX_NOME,NOME,1
	index1=clienti.cognome.idx,IDX_COGNOME,COGNOME,2
	index2=clienti.indirizzo.idx,IDX_INDIRIZZO,INDIRIZZO,3
	index3=clienti.citta.idx,IDX_CITTA,CITTA,4

	Il campo PRIMARY_KEY (ed indici) va specificato solo se la tabella prevede indici, in caso contrario
	field0 deve riferirsi al primo campo.
	Il campo field[n] specifica: nome campo,dimensione,decimali,progressivo(a base 0).
	Il campo index[n] specifica: nome file indice,nome indice,progressivo del campo(a base 0)

	Nota:
	Il membro IsValid() controlla la validita dell'oggetto per la tabella CBerkeleyDB, pero' per funzioni
	fondamentali come questa, che definiscono la struttura della tabella, in caso di errore bisogna invalidare
	l'oggetto (ossia la variabile m_bIsValid, non l'oggetto CBerkeleyDB in se') per far si che il resto del
	codice, che controlla il membro IsValid() prima di qualsiasi operazione, non effettui nessuna operazione.
	A livello applicativo (classi A), B) e applicazione) il chiamante, al creare la classe di tipo B), dovra'
	controllarne la validita' con il membro IsValid() relativo prima di usarla.

	Accede in modo esclusivo (RAII Lock Guard).
*/
bool CBase::Create(const char* pTableName,CBASE_TABLE* pTable/*=NULL*/,CBASE_INDEX* pIdx/*=NULL*/)
{
	// controlla che il costruttore abbia creato l'oggetto per la tabella
	ASSERTEXPR(IsValid());
	if(!IsValid())
	{
		Invalidate();
		return(FALSE);
	}

	// blocca l'accesso alla tabella (RAII Lock Guard)
	CLockGuard guard(this);
    if(!guard.IsLocked())
    {
        m_nLastError = m_pBerkeleyDB->SetLastError(DB_RETCODE_ELOCKFAILURE);
		Invalidate();
        return(FALSE);
    }

	// controlla il puntatore alla struttura per la tabella (l'oggetto viene creato nel costruttore)
	ASSERTEXPR(m_pDatabase);

#ifdef _USE_FIELD_PICTURES
	// imposta la picture di default
	SetDefaultNumberPicture();
	SetDefaultCharPicture();
	SetDefaultPunctPicture();
	SetDefaultUserPicture();
#endif

	// occhio che le funzioni chiamate non possono effettuare il test sulla validita' dell'oggetto
	int i,n;
	int nTotFields = -1;
	int nTotIndex = -1;
	int nTotSize;
	int nFieldNum;
	char szTableName[_MAX_PATH+1] = {0};

	// se deve caricare la definizione della tabella dal file
	if(stristr(pTableName,".def") || !pTable || !pIdx)
	{
		CBinFile deffile;
		if(deffile.Open(pTableName,FALSE))
		{
			char* p;
			char* pElement;
			char szBuffer[MAX_FIELDSIZE+1] = {0};
			char szDefBuffer[MAX_FIELDSIZE+1] = {0};
			char szDataPath[_MAX_PATH+1] = {0};
			
			// legge in una botta sola e cerca con stristr()
			if(deffile.Read(szDefBuffer,sizeof(szDefBuffer)-1))
			{
				// ricava il pathname da aggiungere al nome tabella/indici
				strcpyn(szDataPath,pTableName,sizeof(szDataPath));
				if((p = strrchr(szDataPath,'\\'))!=NULL)
					*p = '\0';
				else
					szDataPath[0] = '\0';

				// nome della tabella
				if((p = stristr(szDefBuffer,"tablename="))!=NULL)
				{
					p += 10;
					for(i=0; *p && *p!='\r' && *p!='\n' && i < sizeof(szBuffer)-1; i++)
						szBuffer[i] = *p++;
					szBuffer[i] = '\0';
				
					snprintf(szTableName,sizeof(szTableName),"%s%s%s",szDataPath,szDataPath[0]!='\0' ? "\\" : "",szBuffer);
				}

				// totale dei campi
				if((p = stristr(szDefBuffer,"totfields="))!=NULL)
				{
					p += 10;
					for(i=0; *p && isdigit(*p) && i < sizeof(szBuffer)-1; i++)
						szBuffer[i] = *p++;
					szBuffer[i] = '\0';
					nTotFields = atoi(szBuffer);
				}
				
				// crea e riempie la struttura per la definizione della tabella (i campi)
				if(nTotFields > 0)
				{
					m_nTotRows = nTotFields+1;
					if(stristr(szDefBuffer,"PRIMARY_KEY"))
						m_nTotRows--;

					m_pTableStruct = new CBASE_TABLE[m_nTotRows];
					for(i=0; i < m_nTotRows; i++)
					{
						m_pTableStruct[i].field = NULL;
						m_pTableStruct[i].type = 0;
						m_pTableStruct[i].size = 0;
						m_pTableStruct[i].dec = 0;
					}
					
					char szField[16];
					int x=0;
					for(i=0; i < nTotFields; i++)
					{
						sprintf(szField,"field%d=",i);
						if((p = stristr(szDefBuffer,szField))!=NULL)
						{
							p += strlen(szField);
							for(n=0; *p && *p!=',' && *p!='\r' && *p!='\n' && n < sizeof(szBuffer)-1; n++)
								szBuffer[n] = *p++;
							szBuffer[n] = '\0';
							
							// salta il campo interno utilizzato per la chiave primaria
							if(strcmp(szBuffer,"PRIMARY_KEY")!=0)
							{
								pElement = new char[strlen(szBuffer)+1];
								strcpy(pElement,szBuffer);
								m_pTableStruct[x].field = pElement;
								
								p++;
								for(n=0; *p && *p!=',' && *p!='\r' && *p!='\n' && n < sizeof(szBuffer)-1; n++)
									szBuffer[n] = *p++;
								szBuffer[n] = '\0';
								m_pTableStruct[x].type = szBuffer[0];
								
								p++;
								for(n=0; *p && *p!=',' && *p!='\r' && *p!='\n' && n < sizeof(szBuffer)-1; n++)
									szBuffer[n] = *p++;
								szBuffer[n] = '\0';
								m_pTableStruct[x].size = atoi(szBuffer);
								ASSERTEXPR(m_pTableStruct[x].size >= FIELD_CHAR_MINSIZE && m_pTableStruct[x].size <= FIELD_CHAR_MAXSIZE);
								
								p++;
								for(n=0; *p && *p!=',' && *p!='\r' && *p!='\n' && n < sizeof(szBuffer)-1; n++)
									szBuffer[n] = *p++;
								szBuffer[n] = '\0';
								m_pTableStruct[x].dec = atoi(szBuffer);

								x++;
							}
						}
					}
				}

				// totale degli indici associati
				if((p = stristr(szDefBuffer,"totindex="))!=NULL)
				{
					p += 9;
					for(i=0; *p && isdigit(*p) && i < sizeof(szBuffer)-1; i++)
						szBuffer[i] = *p++;
					szBuffer[i] = '\0';
					nTotIndex = atoi(szBuffer);
				}
				else
					nTotIndex = 0;
				
				// crea e riempie la struttura per la definizione degli indici (i campi)
				if(nTotIndex > 0)
				{
					m_nTotIdx = nTotIndex+1;
					m_pIdxStruct = new CBASE_INDEX[m_nTotIdx];
					for(i=0; i < m_nTotIdx; i++)
					{
						m_pIdxStruct[i].file = NULL;
						m_pIdxStruct[i].name = NULL;
						m_pIdxStruct[i].field = NULL;
					}
					
					char szField[16];
					for(i=0; i < nTotIndex; i++)
					{
						sprintf(szField,"index%d=",i);
						if((p = stristr(szDefBuffer,szField))!=NULL)
						{
							p += strlen(szField);
							for(n=0; *p && *p!=',' && *p!='\r' && *p!='\n' && n < sizeof(szBuffer)-1; n++)
								szBuffer[n] = *p++;
							szBuffer[n] = '\0';
							
							pElement = new char[strlen(szBuffer)+(strlen(szDataPath)+1)+1];
							sprintf(pElement,"%s%s%s",szDataPath,szDataPath[0]!='\0' ? "\\" : "",szBuffer);
							m_pIdxStruct[i].file = pElement;
							
							p++;
							for(n=0; *p && *p!=',' && *p!='\r' && *p!='\n' && n < sizeof(szBuffer)-1; n++)
								szBuffer[n] = *p++;
							szBuffer[n] = '\0';
							
							pElement = new char[strlen(szBuffer)+1];
							strcpy(pElement,szBuffer);
							m_pIdxStruct[i].name = pElement;
							
							p++;
							for(n=0; *p && *p!=',' && *p!='\r' && *p!='\n' && n < sizeof(szBuffer)-1; n++)
								szBuffer[n] = *p++;
							szBuffer[n] = '\0';

							pElement = new char[strlen(szBuffer)+1];
							strcpy(pElement,szBuffer);
							m_pIdxStruct[i].field = pElement;
						}
					}
				}
			}

			deffile.Close();
			
			// imposta i parametri con i dati interni
			if(nTotFields!=-1 && nTotIndex!=-1)
			{
				pTableName = szTableName;
				pTable = m_pTableStruct;
				pIdx = m_pIdxStruct;
			}
		}
		else
		{
			m_nLastError = m_pBerkeleyDB->SetLastError(DB_RETCODE_EOPENDEFINITION);
			Invalidate();
			return(FALSE);
		}

		ASSERTEXPR(nTotFields!=-1);
		if(nTotFields==-1)
		{
			m_nLastError = m_pBerkeleyDB->SetLastError(DB_RETCODE_EINVALIDFIELDNUMBER);
			Invalidate();
			return(FALSE);
		}
		ASSERTEXPR(nTotIndex!=-1);
		if(nTotIndex==-1)
		{
			m_nLastError = m_pBerkeleyDB->SetLastError(DB_RETCODE_EINVALIDINDEX);
			Invalidate();
			return(FALSE);
		}
		ASSERTEXPR(pTable);
		if(!pTable)
		{
			m_nLastError = m_pBerkeleyDB->SetLastError(DB_RETCODE_ECREATETABLE);
			Invalidate();
			return(FALSE);
		}
	}

	// a partire da qui, per il codice deve essere indifferente se vengono passate le strutture per la
	// tabella/indici o se vengono create dinamicamente a partire dalla definizione caricata dal file (.def)

	// ricava il numero di campi e la dim. totale del record
	for(i = 0,nTotFields = 0,nTotSize = 0; pTable[i].field && *(pTable[i].field); i++)
	{
		ASSERTEXPR(pTable[i].size >= FIELD_CHAR_MINSIZE && pTable[i].size <= FIELD_CHAR_MAXSIZE);
		nTotFields++;
		nTotSize += pTable[i].size;
	}

	// controlla i valori
	ASSERTEXPR(nTotFields > 0);
	ASSERTEXPR(nTotFields < MAX_FIELDCOUNT);
	if(nTotFields <= 0 || nTotFields >= MAX_FIELDCOUNT)
	{
		m_nLastError = m_pBerkeleyDB->SetLastError(DB_RETCODE_EINVALIDFIELDNUMBER);
		Invalidate();
		return(FALSE);
	}
	ASSERTEXPR(nTotSize > 0);
	if(nTotSize <= 0)
	{
		m_nLastError = m_pBerkeleyDB->SetLastError(DB_RETCODE_EINVALIDFIELDSIZE);
		Invalidate();
		return(FALSE);
	}

	// se la tabella prevede indici imposta la dimensione della chiave primaria, incrementando il numero campi e la dim. del record
	int nPrimaryKeySize = (pIdx==NULL ? 0 : MAX_PRIMARYKEY_SIZE);
	if(nPrimaryKeySize > 0)
	{
		nTotFields++;
		nTotSize += nPrimaryKeySize;
	}

	// tot. campi della tabella
	if((m_pDatabase->table.totfield = nTotFields) > 0)
	{
		// nome file per la tabella
		strcpyn(m_pDatabase->table.filename,pTableName,_MAX_PATH+1);

		// array per la definizione dei campi della tabella
		m_pDatabase->table.row = (ROW*) new char[sizeof(ROW) * nTotFields];
		memset((m_pDatabase->table.row),'\0',sizeof(ROW) * nTotFields);

		// buffer (interno) per il record della tabella
		m_pRecordBuffer = new char[nTotSize + 1];
		memset(m_pRecordBuffer,'\0',nTotSize + 1);

		// imposta la struttura della classe base per la definizione della tabella
		n = 0;
		nFieldNum = 0;

		// se la tabella prevede indici, inserisce il campo per la chiave primaria come primo elemento
		if(nPrimaryKeySize > 0)
		{
			m_pDatabase->table.row[0].num   = 0;
			m_pDatabase->table.row[0].ofs   = n;
			m_pDatabase->table.row[0].name  = (char*)"PRIMARY_KEY";
			m_pDatabase->table.row[0].type  = 'C';
			m_pDatabase->table.row[0].size  = nPrimaryKeySize;
			m_pDatabase->table.row[0].dec   = 0;
			m_pDatabase->table.row[0].value = m_pRecordBuffer;
			m_pDatabase->table.row[0].flags = 0;

			n += nPrimaryKeySize;
		}

		int nMaxFieldSize = 0;

		// definisce i campi della tabella
		for(i = 0; i < (nPrimaryKeySize > 0 ? nTotFields-1 : nTotFields); i++)
		{
			// numero del campo (salta il campo relativo alla chiave primaria)
			nFieldNum = i + (nPrimaryKeySize > 0 ? 1 : 0);

			// progressivo, offset e nome
			m_pDatabase->table.row[nFieldNum].num   = nFieldNum;
			m_pDatabase->table.row[nFieldNum].ofs   = n;
			m_pDatabase->table.row[nFieldNum].name  = pTable[i].field;
			
			// controlla il tipo
			switch(pTable[i].type)
			{
				// carattere (MAX_FIELDSIZE=1024)
				case 'C':
					nMaxFieldSize = FIELD_CHAR_MAXSIZE;
					break;
				
				// ora (6=hhmmss)
				case 'T':
					nMaxFieldSize = FIELD_TIME_MAXSIZE;
					break;
				
				// data (9=<~/+>yyyymmdd)
				case 'D':
					nMaxFieldSize = FIELD_DATE_MAXSIZE;
					break;

				// boolean (1=F|T)
				case 'B':
					nMaxFieldSize = FIELD_BOOLEAN_MAXSIZE;
					break;
				
				// numerici (5,10,17)
				// short (5) - 32768
				case 'S':
					nMaxFieldSize = FIELD_SHORTINT_MAXSIZE;
					break;
				// int (10) - 2147483647
				case 'I':
					nMaxFieldSize = FIELD_INT_MAXSIZE;
					break;
				// long (10) - 2147483647L
				case 'N':
					nMaxFieldSize = FIELD_LONG_MAXSIZE;
					break;
				// unsigned long (10) - 0xffffffffUL
				case 'U':
					nMaxFieldSize = FIELD_UNSIGNEDLONG_MAXSIZE;
					break;
				// double (17=10+,+6)
				case 'R':
					nMaxFieldSize = FIELD_REAL_MAXSIZE;
					break;
				
				default:
					pTable[i].type = '?';
					ASSERTEXPR(pTable[i].type!='?');
					m_nLastError = m_pBerkeleyDB->SetLastError(DB_RETCODE_EINVALIDFIELDTYPE);
					Invalidate();
					return(FALSE);
			}
			m_pDatabase->table.row[nFieldNum].type = pTable[i].type;

			// dimensione del campo
			pTable[i].size = pTable[i].size > nMaxFieldSize ? nMaxFieldSize : pTable[i].size;
			m_pDatabase->table.row[nFieldNum].size = pTable[i].size;

			// decimali (massimo 6 posizioni)
			pTable[i].dec = (pTable[i].type=='R') ? (pTable[i].dec > FIELD_REAL_MAXDECS ? FIELD_REAL_MAXDECS : pTable[i].dec) : 0;
			m_pDatabase->table.row[nFieldNum].dec = pTable[i].dec;

			// offset nel buffer (interno) per il valore del campo
			m_pDatabase->table.row[nFieldNum].value = m_pRecordBuffer + n;
			
			// flags
#ifdef _USE_FIELD_PICTURES
			m_pDatabase->table.row[nFieldNum].flags = CBASE_FLAG_NONE;
#else
			m_pDatabase->table.row[nFieldNum].flags = 0;
#endif

			n += pTable[i].size;
		}
	}

	// ricava il numero di indici
	if(pIdx)
	{
		for(i = 0,nTotIndex = 0; pIdx[i].file && *(pIdx[i].file); i++)
			nTotIndex++;
	}
	else
		nTotIndex = 0;

	// tot. indici della tabella
	if((m_pDatabase->table.totindex = nTotIndex) > 0)
	{
		// array per la definizione degli indici della tabella
		m_pDatabase->table.index = (INDEX*) new char[sizeof(INDEX) * nTotIndex];
		memset(m_pDatabase->table.index,'\0',sizeof(INDEX) * nTotIndex);

		// definisce gli indici della tabella
		for(i = 0; pIdx[i].file && *(pIdx[i].file); i++)
		{
			// nome del file indice
			strcpyn(m_pDatabase->table.index[i].filename,pIdx[i].file,_MAX_PATH+1);

			// ricava il numero del campo relativo alla chiave
			for(n = 0; n < nTotFields; n++)
			{
				if(strcmp(pIdx[i].field,pTable[n].field)==0)
					break;
			}

			if(n==nTotFields)
				n = -1; 

			// nome indice, nome del campo relativo, numero del campo relativo
			if(n >= 0)
			{
				m_pDatabase->table.index[i].name      = pIdx[i].name;
				m_pDatabase->table.index[i].fieldname = pTable[n].field;
				m_pDatabase->table.index[i].fieldnum  = n + 1; // il + 1 serve per saltare il campo relativo alla chiave primaria
			}
			else
			{
				m_pDatabase->table.index[i].name      = pTable[0].field;
				m_pDatabase->table.index[i].fieldname = pTable[0].field;
				m_pDatabase->table.index[i].fieldnum  = 0;
			}
		}
	}

	// salva la definizione della tabella nel file relativo (.def)
	{
		CBinFile def;
		char* p;
		char szDir[_MAX_PATH+1];
		snprintf(szDir,sizeof(szDir),"%s\\%s.def",GetTablePath(),GetTableName());
		
		if(def.Create(szDir))
		{
			int i;
			char szBuffer[MAX_FIELDSIZE+1];
			
			p = (char*)strrchr(pTableName,'\\');
			if(p)
				p++;
			else
				p = (char*)pTableName;
			snprintf(szBuffer,sizeof(szBuffer),"tablename=%s\r\n",p);
			def.Write(szBuffer,(DWORD)strlen(szBuffer));

			snprintf(szBuffer,sizeof(szBuffer),"totfields=%d\r\n",m_pDatabase->table.totfield);
			def.Write(szBuffer,(DWORD)strlen(szBuffer));

			for(i=0; i < m_pDatabase->table.totfield; i++)
			{
				snprintf(szBuffer,
						sizeof(szBuffer),
						"field%d=%s,%c,%d,%d,%d\r\n",
						i,
						m_pDatabase->table.row[i].name,
						m_pDatabase->table.row[i].type,
						m_pDatabase->table.row[i].size,
						m_pDatabase->table.row[i].dec,
						m_pDatabase->table.row[i].num
						);

				def.Write(szBuffer,(DWORD)strlen(szBuffer));
			}

			snprintf(szBuffer,sizeof(szBuffer),"totindex=%d\r\n",m_pDatabase->table.totindex);
			def.Write(szBuffer,(DWORD)strlen(szBuffer));

			for(i=0; i < m_pDatabase->table.totindex; i++)
			{
				p = strrchr(m_pDatabase->table.index[i].filename,'\\');
				if(!p)
					p = m_pDatabase->table.index[i].filename;
				else
					p++;
				snprintf(szBuffer,
						sizeof(szBuffer),
						"index%d=%s,%s,%s,%d\r\n",
						i,
						p,
						m_pDatabase->table.index[i].name,
						m_pDatabase->table.index[i].fieldname,
						m_pDatabase->table.index[i].fieldnum
						);

				def.Write(szBuffer,(DWORD)strlen(szBuffer));
			}

			def.Close();
		}
		else
		{
			m_nLastError = m_pBerkeleyDB->SetLastError(DB_RETCODE_EOPENDEFINITION);
			Invalidate();
			return(FALSE);
		}
	}

	return(TRUE);
}

/*
	Open()
	
	Apre la tabella e gli indici associati, recuperando il primo record.

	NON accede in modo esclusivo.
*/
bool CBase::Open(void)
{
	TRACEEXPR((_TRACE_FLAG_INFO,__FILE__,__LINE__,"CBase: Open()\n"));

	// controlla che il costruttore abbia creato l'oggetto per la tabella
	ASSERTEXPR(IsValid());
	if(!IsValid())
		return(FALSE);

	// apre la tabella e posiziona sul primo record (recuperando i dati)
	bool bIsOpen = m_pBerkeleyDB->IsOpen();
	if(!bIsOpen)
		if(m_pBerkeleyDB->Open()==DB_NO_ERROR)
		{
			bIsOpen = TRUE;
			GetFirst();
		}

	ASSERTEXPR(bIsOpen);
	return(bIsOpen);
}

/*
	Zap()

	Elimina tutti i record della tabella (e degli indici relativi).

	Accede in modo esclusivo (RAII Lock Guard).
*/
bool CBase::Zap(bool bLock/* = true*/)
{
	// controlla che il costruttore abbia creato l'oggetto per la tabella
	ASSERTEXPR(IsValid());
	if(!IsValid())
		return(false);

	// blocca l'accesso alla tabella (RAII Lock Guard)
	if(bLock)
	{
		CLockGuard guard(this);
		if(!guard.IsLocked())
		{
			m_nLastError = m_pBerkeleyDB->SetLastError(DB_RETCODE_ELOCKFAILURE);
			return(false);
		}
	}

	bool bZap = false;

	// chiude prima di azerare
	if(m_pBerkeleyDB->IsOpen())
		m_pBerkeleyDB->Close();

#if 1
	// azzera la taballa ricreandola
	bZap = m_pBerkeleyDB->Create()==DB_NO_ERROR;
	if(bZap)
		m_pDatabase->table.stat.bof = m_pDatabase->table.stat.eof = TRUE;
#else
	// elimina la tabella (insieme agli indici relativi) e la ricrea
	remove(m_pDatabase->table.filename);

	if(m_pDatabase->table.totindex > 0)
		for(int i=0; i < m_pDatabase->table.totindex; i++)
			remove(m_pDatabase->table.index[i].filename);

	if(m_pBerkeleyDB->Open()!=DB_NO_ERROR)
	{
		m_nLastError = m_pBerkeleyDB->SetLastError(DB_RETCODE_EOPENTABLE);
		bZap = false;
	}
	else
		bZap = true;
#endif

	ASSERTEXPR(bZap);
	return(bZap);
}

/*
	Close()

	Chiude la tabella e rilascia le risorse associate.

	Non accede in modo esclusivo.
*/
bool CBase::Close(void)
{
	TRACEEXPR((_TRACE_FLAG_INFO,__FILE__,__LINE__,"CBase: Close()\n"));

	// controlla che il costruttore abbia creato l'oggetto per la tabella
	ASSERTEXPR(IsValid());
	if(!IsValid())
		return(FALSE);

	bool bClose = FALSE;

	if(m_pBerkeleyDB->IsOpen())
		bClose = m_pBerkeleyDB->Close()==DB_NO_ERROR;

	return(bClose);
}

/*
	import/export della tabella via file SDF
	per definizione il file SDF e' il dump della tabella senza includere la chiave primaria, con i campi spaziati
	(ossia paddati a spazio) e senza carattere di separazione tra i campi
	la funzione Dump() permette specificare come esportare, ma gli eventuali cambi produrranno un file illegibile 
	per la funzione Load()
	in poche parole, SDF non e' uguale a CSV
*/

/*
	Dump()

	Scarica nel file SDF quanto contenuto nella tabella.
	La tabella deve essere aperta, altrimenti termina con errore.

	E' possibile specificare se includere o meno la chiave primaria, se usare un carattere come separatore per i campi 
	e se eliminare gli spazi dal valore del campo.

	Notare che Load() assume che il file SDF contenga solo dati paddati, senza separatori tra i campi e senza la chiave 
	primaria.

	Accede in modo esclusivo (RAII Lock Guard).
*/
bool CBase::Dump(const char* pFileName,bool bDumpPrimaryKey/*=FALSE*/,char cSeparator/*=0*/,bool bTrimSpaces/*=FALSE*/)
{
	// controlla che il costruttore abbia creato l'oggetto per la tabella
	ASSERTEXPR(IsValid());
	if(!IsValid())
		return(false);

	m_nLastError = DB_NO_ERROR;

	// blocca l'accesso alla tabella (RAII Lock Guard)
	CLockGuard guard(this);
    if(!guard.IsLocked())
    {
        m_nLastError = m_pBerkeleyDB->SetLastError(DB_RETCODE_ELOCKFAILURE);
        return(false);
    }

	// la tabella deve essere gia' aperta
	ASSERTEXPR(m_pBerkeleyDB->IsOpen());
	if(!m_pBerkeleyDB->IsOpen())
	{
		m_nLastError = m_pBerkeleyDB->SetLastError(DB_RETCODE_ETABLENOTOPEN);
		return(false);
	}

	// numero di campi dela tabella, NON include l'eventuale chiave primaria
	int nFieldCount = GetFieldCount();

	// dimensione della chiave primaria
	int nPrimaryKeySize = 0;
	if(m_pDatabase->table.totindex > 0)
		nPrimaryKeySize = m_pBerkeleyDB->GetPrimaryKeySize();
	else
		nPrimaryKeySize = 0;

	// dimensione del record (non include la dimensione della chiave primaria)
	int nRecordSize = 0;
	for(int i = m_pDatabase->table.totindex > 0 ? 1 : 0; i < m_pDatabase->table.totfield; i++)
		nRecordSize += m_pDatabase->table.row[i].size;
	if(nRecordSize <= 0)
	{
		m_nLastError = m_pBerkeleyDB->SetLastError(DB_RETCODE_EINVALIDFIELDSIZE);
		return(false);
	}

	// alloca il buffer per leggere il campo del record, legge 1 campo x volta, NON legge il record intero
	char szBuffer[MAX_FIELDSIZE+1] = {0};

	// elimina l'eventuale dump anteriore
	remove(pFileName);

	// scarica il contenuto della tabella nel file SDF
	CTextFile textFile;
	if(textFile.Create(pFileName))
	{
		// posiziona ad inizio tabella e cicla fino a eof
		if(GoTop(0))
		{
			while(!Eof())
			{
				// chiave primaria
				if(bDumpPrimaryKey)
				{
					textFile.Write(GetPrimaryKeyValue(),nPrimaryKeySize);
					if(cSeparator!=0)
						textFile.Write(&cSeparator,1);
				}

				// campi
				for(int i = 0; i < nFieldCount; i++)
				{
					GetFieldRaw(i,szBuffer,sizeof(szBuffer));
					
					if(bTrimSpaces)
					{
						strrtrim(szBuffer);
						strltrim(szBuffer);
					}

					textFile.Write(szBuffer,(DWORD)strlen(szBuffer));
	
					if(i!=nFieldCount-1)
					{
						if(cSeparator!=0)
							textFile.Write(&cSeparator,1);
					}
					else
						textFile.Write("\r\n",2);
				}

				GetNext();
			}
		}
		else
		{
			m_nLastError = m_pBerkeleyDB->SetLastError(DB_RETCODE_EOF);
		}

		textFile.Close();
	}
	else
	{
		m_nLastError = m_pBerkeleyDB->SetLastError(DB_RETCODE_EOPENDEFINITION);
	}

	return(m_nLastError==DB_NO_ERROR);
}

/*
	Load()

	Carica la tabella con quanto contenuto nel file SDF.
	La tabella deve essere chiusa, altrimenti termina con errore.

	Assume che il file SDF contenga solo dati paddati, senza separatori tra i campi e senza il valore della 
	chiave primaria.

	Accede in modo esclusivo (RAII Lock Guard).
*/
bool CBase::Load(const char* pFileName)
{
	// controlla che il costruttore abbia creato l'oggetto per la tabella
	ASSERTEXPR(IsValid());
	if(!IsValid())
		return(false);

	m_nLastError = DB_NO_ERROR;

	// blocca l'accesso alla tabella (RAII Lock Guard)
	CLockGuard guard(this);
    if(!guard.IsLocked())
    {
        m_nLastError = m_pBerkeleyDB->SetLastError(DB_RETCODE_ELOCKFAILURE);
        return(false);
    }

	CTextFile textFile;
	int i,nPrimaryKeySize,nRecordSize;
#ifdef _USE_FIELD_PICTURES
	char szField[MAX_KEYSIZE + 1];
	int nFieldLen;
#endif

	// la tabella deve essere chiusa
	if(m_pBerkeleyDB->IsOpen())
	{
		m_nLastError = m_pBerkeleyDB->SetLastError(DB_RETCODE_EALREADYOPEN);
		return(false);
	}
	// apre la tabella in proprio
	if(m_pBerkeleyDB->Open()!=DB_NO_ERROR)
	{
		m_nLastError = m_pBerkeleyDB->SetLastError(DB_RETCODE_EOPENTABLE);
		return(false);
	}

	// dimensione della chiave primaria
	if(m_pDatabase->table.totindex > 0)
		nPrimaryKeySize = m_pBerkeleyDB->GetPrimaryKeySize();
	else
		nPrimaryKeySize = 0;

	// dimensione del record (salta la chiave primaria)
	nRecordSize = 0;
	for(i = m_pDatabase->table.totindex > 0 ? 1 : 0; i < m_pDatabase->table.totfield; i++)
		nRecordSize += m_pDatabase->table.row[i].size;
	if(nRecordSize <= 0)
	{
		m_pBerkeleyDB->Close();
		m_nLastError = m_pBerkeleyDB->SetLastError(DB_RETCODE_EINVALIDFIELDSIZE);
		return(false);
	}

	// buffer per la lettura del record dal file sdf (dim. record + crlf + null)
	char* pBuffer = new char[nRecordSize + 2 + 1];
	if(!pBuffer)
	{
		m_pBerkeleyDB->Close();
		m_nLastError = m_pBerkeleyDB->SetLastError(DB_RETCODE_EALLOCFAILURE);
		return(false);
	}

	// azzera la tabella
	Zap(false);

	// carica la tabella con il contenuto del file SDF
	if(textFile.Open(pFileName))
	{
		// azzera il buffer interno
		memset(m_pRecordBuffer,' ',nRecordSize + nPrimaryKeySize + 1);

		// legge una linea dal file
		while(textFile.ReadLine(pBuffer,nRecordSize + 2 + 1)!=FILE_EOF)
		{
			// passa la linea nel buffer interno (salta la chiave primaria)
			memcpy(m_pRecordBuffer + nPrimaryKeySize,pBuffer,nRecordSize);

#ifdef _USE_FIELD_PICTURES
			// filtra il contenuto del campo secondo la picture corrente
			for(i = 0; i < m_pDatabase->table.totfield; i++)
			{
				if(m_pDatabase->table.row[i].flags!=CBASE_FLAG_NONE)
				{
					memset(szField,'\0',sizeof(szField));
					memcpy(szField,m_pDatabase->table.row[i].value,m_pDatabase->table.row[i].size);
					nFieldLen = strlen(szField);
						
					SetFieldFormat(szField,nFieldLen,m_pDatabase->table.row[i].flags);
						
					memset(m_pDatabase->table.row[i].value,' ',m_pDatabase->table.row[i].size);
					memcpy(m_pDatabase->table.row[i].value,szField,nFieldLen);
				}
			}
#endif
			// inserisce il record nella tabella
			Insert(false);
		}

		textFile.Close();
	}
	else
	{
		m_nLastError = m_pBerkeleyDB->SetLastError(DB_RETCODE_EOPENDEFINITION);
	}

	if(pBuffer)
		delete [] pBuffer;
	
	m_pBerkeleyDB->Close();

	return(m_nLastError==DB_NO_ERROR);
}

/*
	Insert()

	Inserisce il record corrente nella tabella.

	Accede in modo esclusivo (RAII Lock Guard).
*/
bool CBase::Insert(bool bLock/*=true*/)
{
	// controlla che il costruttore abbia creato l'oggetto per la tabella
	ASSERTEXPR(IsValid());
	if(!IsValid())
		return(FALSE);

	// blocca l'accesso alla tabella (RAII Lock Guard)
	if(bLock)
	{
		CLockGuard guard(this);
		if(!guard.IsLocked())
		{
			m_nLastError = m_pBerkeleyDB->SetLastError(DB_RETCODE_ELOCKFAILURE);
			return(FALSE);
		}
	}

	return(m_pBerkeleyDB->Insert()==0);
}

/*
	Delete()

	Elimina il record corrente dalla tabella.

	Accede in modo esclusivo (RAII Lock Guard).
*/
bool CBase::Delete(bool bLock/*=true*/)
{
	// controlla che il costruttore abbia creato l'oggetto per la tabella
	ASSERTEXPR(IsValid());
	if(!IsValid())
		return(FALSE);

	// blocca l'accesso alla tabella (RAII Lock Guard)
	if(bLock)
	{
		CLockGuard guard(this);
		if(!guard.IsLocked())
		{
			m_nLastError = m_pBerkeleyDB->SetLastError(DB_RETCODE_ELOCKFAILURE);
			return(FALSE);
		}
	}

	return(m_pBerkeleyDB->Delete()==0);
}

/*
	Replace()

	Sostituisce il record corrente della tabella.

	Accede in modo esclusivo (RAII Lock Guard).
*/
bool CBase::Replace(int nFieldNum,const char* pOldValue,const char* pNewValue,bool bLock/*=true*/)
{
	// controlla che il costruttore abbia creato l'oggetto per la tabella
	ASSERTEXPR(IsValid());
	if(!IsValid())
		return(FALSE);

	// blocca l'accesso alla tabella (RAII Lock Guard)
	if(bLock)
	{
		CLockGuard guard(this);
		if(!guard.IsLocked())
		{
			m_nLastError = m_pBerkeleyDB->SetLastError(DB_RETCODE_ELOCKFAILURE);
			return(false);
		}
	}
	
	// aggiorna il campo con il nuovo valore
	PutField(nFieldNum,pNewValue);

	// sostituisce il campo della tabella (se la tabella prevede indici, salta la chiave primaria)
	return(m_pBerkeleyDB->Replace(m_pDatabase->table.totindex > 0 ? nFieldNum + 1 : nFieldNum,pOldValue,pNewValue)==0);
}
bool CBase::Replace(const char* pFieldName,const char* pOldValue,const char* pNewValue,bool bLock/*=true*/)
{
	bool bReplaced = FALSE;
	for(int i=0; i < m_pDatabase->table.totfield; i++)
	{
		if(stricmp(m_pDatabase->table.row[i].name,pFieldName)==0)
		{
			// se la tabella prevede indici, deve sottrarre il campo per la chiave primaria dal conteggio
			i = m_pDatabase->table.totindex > 0 ? i-1 : i;
			bReplaced = Replace(i,pOldValue,pNewValue,bLock);
			break;
		}
	}
	return(bReplaced);
}
bool CBase::Replace(int nFieldNum,bool bOldValue,bool bNewValue,bool bLock/*=true*/)
{
	// controlla che il costruttore abbia creato l'oggetto per la tabella
	ASSERTEXPR(IsValid());
	if(!IsValid())
		return(FALSE);

	// blocca l'accesso alla tabella (RAII Lock Guard)
	if(bLock)
	{
		CLockGuard guard(this);
		if(!guard.IsLocked())
		{
			m_nLastError = m_pBerkeleyDB->SetLastError(DB_RETCODE_ELOCKFAILURE);
			return(false);
		}
	}
	
	char szOld[3] = {0};
	char szNew[3] = {0};
		
	// aggiorna il campo con il nuovo valore
	PutField(nFieldNum,bNewValue);
		
	// trasforma da boolean a char
	FormatField(szOld,sizeof(szOld),bOldValue);
	FormatField(szNew,sizeof(szNew),bNewValue);
		
	// sostituisce il campo della tabella (se la tabella prevede indici, salta la chiave primaria)
	return(m_pBerkeleyDB->Replace(m_pDatabase->table.totindex > 0 ? nFieldNum + 1 : nFieldNum,szOld,szNew)==0);
}
bool CBase::Replace(const char* pFieldName,bool bOldValue,bool bNewValue,bool bLock/*=true*/)
{
	bool bReplaced = FALSE;
	for(int i=0; i < m_pDatabase->table.totfield; i++)
	{
		if(stricmp(m_pDatabase->table.row[i].name,pFieldName)==0)
		{
			// se la tabella prevede indici, deve sottrarre il campo per la chiave primaria dal conteggio
			i = m_pDatabase->table.totindex > 0 ? i-1 : i;
			bReplaced = Replace(i,bOldValue,bNewValue,bLock);
			break;
		}
	}
	return(bReplaced);
}
bool CBase::Replace(int nFieldNum,CDateTime& pOldValue,CDateTime& pNewValue,bool bLock/*=true*/)
{
	// controlla che il costruttore abbia creato l'oggetto per la tabella
	ASSERTEXPR(IsValid());
	if(!IsValid())
		return(FALSE);

	// blocca l'accesso alla tabella (RAII Lock Guard)
	if(bLock)
	{
		CLockGuard guard(this);
		if(!guard.IsLocked())
		{
			m_nLastError = m_pBerkeleyDB->SetLastError(DB_RETCODE_ELOCKFAILURE);
			return(false);
		}
	}
	
	char szOld[32] = {0};
	char szNew[32] = {0};
		
	// aggiorna il campo con il nuovo valore
	PutField(nFieldNum,pNewValue);
		
	// deve ricavare il tipo del campo data (T,D,G)
	char cType = 0;

	// se la tabella prevede indici salta il primo campo (relativo alla chiave primaria)
	int n = m_pDatabase->table.totindex > 0 ? nFieldNum + 1 : nFieldNum;
	for(int i = 0; i < m_pDatabase->table.totfield; i++)
		if(i==n)
		{
			cType = m_pDatabase->table.row[i].type;
			break;
		}
		
	// trasforma da data a char
	FormatField(szOld,sizeof(szOld),pOldValue,cType);
	FormatField(szNew,sizeof(szNew),pNewValue,cType);
		
	// sostituisce il campo della tabella (se la tabella prevede indici, salta la chiave primaria)
	return(m_pBerkeleyDB->Replace(m_pDatabase->table.totindex > 0 ? nFieldNum + 1 : nFieldNum,szOld,szNew)==0);
}
bool CBase::Replace(const char* pFieldName,CDateTime& pOldValue,CDateTime& pNewValue,bool bLock/*=true*/)
{
	bool bReplaced = FALSE;
	for(int i=0; i < m_pDatabase->table.totfield; i++)
	{
		if(stricmp(m_pDatabase->table.row[i].name,pFieldName)==0)
		{
			// se la tabella prevede indici, deve sottrarre il campo per la chiave primaria dal conteggio
			i = m_pDatabase->table.totindex > 0 ? i-1 : i;
			bReplaced = Replace(i,pOldValue,pNewValue,bLock);
			break;
		}
	}
	return(bReplaced);
}
bool CBase::Replace(int nFieldNum,short int nOldValue,short int nNewValue,bool bLock/*=true*/)
{
	// controlla che il costruttore abbia creato l'oggetto per la tabella
	ASSERTEXPR(IsValid());
	if(!IsValid())
		return(FALSE);

	// blocca l'accesso alla tabella (RAII Lock Guard)
	if(bLock)
	{
		CLockGuard guard(this);
		if(!guard.IsLocked())
		{
			m_nLastError = m_pBerkeleyDB->SetLastError(DB_RETCODE_ELOCKFAILURE);
			return(false);
		}
	}
	
	char szOld[32] = {0};
	char szNew[32] = {0};
		
	// aggiorna il campo con il nuovo valore
	PutField(nFieldNum,nNewValue);

	// da numero a short int
	NUMBER Ns = {0};
	Ns.shortint = nOldValue;		
	FormatField(szOld,sizeof(szOld),Ns,shortint_type,nFieldNum);
	Ns.shortint = nNewValue;		
	FormatField(szNew,sizeof(szNew),Ns,shortint_type,nFieldNum);
		
	// sostituisce il campo della tabella (se la tabella prevede indici, salta la chiave primaria)
	return(m_pBerkeleyDB->Replace(m_pDatabase->table.totindex > 0 ? nFieldNum + 1 : nFieldNum,szOld,szNew)==0);
}
bool CBase::Replace(const char* pFieldName,short int nOldValue,short int nNewValue,bool bLock/*=true*/)
{
	bool bReplaced = FALSE;
	for(int i=0; i < m_pDatabase->table.totfield; i++)
	{
		if(stricmp(m_pDatabase->table.row[i].name,pFieldName)==0)
		{
			// se la tabella prevede indici, deve sottrarre il campo per la chiave primaria dal conteggio
			i = m_pDatabase->table.totindex > 0 ? i-1 : i;
			bReplaced = Replace(i,nOldValue,nNewValue,bLock);
			break;
		}
	}
	return(bReplaced);
}
bool CBase::Replace(int nFieldNum,int nOldValue,int nNewValue,bool bLock/*=true*/)
{
	// controlla che il costruttore abbia creato l'oggetto per la tabella
	ASSERTEXPR(IsValid());
	if(!IsValid())
		return(FALSE);

	// blocca l'accesso alla tabella (RAII Lock Guard)
	if(bLock)
	{
		CLockGuard guard(this);
		if(!guard.IsLocked())
		{
			m_nLastError = m_pBerkeleyDB->SetLastError(DB_RETCODE_ELOCKFAILURE);
			return(false);
		}
	}
	
	char szOld[32] = {0};
	char szNew[32] = {0};

	// aggiorna il campo con il nuovo valore
	PutField(nFieldNum,nNewValue);

	// da numero a int
	NUMBER Ni = {0};
	Ni.integer = nOldValue;		
	FormatField(szOld,sizeof(szOld),Ni,integer_type,nFieldNum);
	Ni.integer = nNewValue;		
	FormatField(szNew,sizeof(szNew),Ni,integer_type,nFieldNum);
		
	// sostituisce il campo della tabella (se la tabella prevede indici, salta la chiave primaria)
	return(m_pBerkeleyDB->Replace(m_pDatabase->table.totindex > 0 ? nFieldNum + 1 : nFieldNum,szOld,szNew)==0);
}
bool CBase::Replace(const char* pFieldName,int nOldValue,int nNewValue,bool bLock/*=true*/)
{
	bool bReplaced = FALSE;
	for(int i=0; i < m_pDatabase->table.totfield; i++)
	{
		if(stricmp(m_pDatabase->table.row[i].name,pFieldName)==0)
		{
			// se la tabella prevede indici, deve sottrarre il campo per la chiave primaria dal conteggio
			i = m_pDatabase->table.totindex > 0 ? i-1 : i;
			bReplaced = Replace(i,nOldValue,nNewValue,bLock);
			break;
		}
	}
	return(bReplaced);
}
bool CBase::Replace(int nFieldNum,long nOldValue,long nNewValue,bool bLock/*=true*/)
{
	// controlla che il costruttore abbia creato l'oggetto per la tabella
	ASSERTEXPR(IsValid());
	if(!IsValid())
		return(FALSE);

	// blocca l'accesso alla tabella (RAII Lock Guard)
	if(bLock)
	{
		CLockGuard guard(this);
		if(!guard.IsLocked())
		{
			m_nLastError = m_pBerkeleyDB->SetLastError(DB_RETCODE_ELOCKFAILURE);
			return(false);
		}
	}
	
	char szOld[32] = {0};
	char szNew[32] = {0};

	// aggiorna il campo con il nuovo valore
	PutField(nFieldNum,nNewValue);

	// da numero a long
	NUMBER Nl = {0};
	Nl.longint = nOldValue;		
	FormatField(szOld,sizeof(szOld),Nl,longint_type,nFieldNum);
	Nl.longint = nNewValue;		
	FormatField(szNew,sizeof(szNew),Nl,longint_type,nFieldNum);

	// sostituisce il campo della tabella (se la tabella prevede indici, salta la chiave primaria)
	return(m_pBerkeleyDB->Replace(m_pDatabase->table.totindex > 0 ? nFieldNum + 1 : nFieldNum,szOld,szNew)==0);
}
bool CBase::Replace(const char* pFieldName,long nOldValue,long nNewValue,bool bLock/*=true*/)
{
	bool bReplaced = FALSE;
	for(int i=0; i < m_pDatabase->table.totfield; i++)
	{
		if(stricmp(m_pDatabase->table.row[i].name,pFieldName)==0)
		{
			// se la tabella prevede indici, deve sottrarre il campo per la chiave primaria dal conteggio
			i = m_pDatabase->table.totindex > 0 ? i-1 : i;
			bReplaced = Replace(i,nOldValue,nNewValue,bLock);
			break;
		}
	}
	return(bReplaced);
}
bool CBase::Replace(int nFieldNum,unsigned long nOldValue,unsigned long nNewValue,bool bLock/*=true*/)
{
	// controlla che il costruttore abbia creato l'oggetto per la tabella
	ASSERTEXPR(IsValid());
	if(!IsValid())
		return(FALSE);

	// blocca l'accesso alla tabella (RAII Lock Guard)
	if(bLock)
	{
		CLockGuard guard(this);
		if(!guard.IsLocked())
		{
			m_nLastError = m_pBerkeleyDB->SetLastError(DB_RETCODE_ELOCKFAILURE);
			return(false);
		}
	}
	
	char szOld[32] = {0};
	char szNew[32] = {0};

	// aggiorna il campo con il nuovo valore
	PutField(nFieldNum,nNewValue);

	// da numero a unsigned long
	NUMBER Nu = {0};
	Nu.ulongint = nOldValue;		
	FormatField(szOld,sizeof(szOld),Nu,ulongint_type,nFieldNum);
	Nu.ulongint = nNewValue;		
	FormatField(szNew,sizeof(szNew),Nu,ulongint_type,nFieldNum);

	// sostituisce il campo della tabella (se la tabella prevede indici, salta la chiave primaria)
	return(m_pBerkeleyDB->Replace(m_pDatabase->table.totindex > 0 ? nFieldNum + 1 : nFieldNum,szOld,szNew)==0);
}
bool CBase::Replace(const char* pFieldName,unsigned long nOldValue,unsigned long nNewValue,bool bLock/*=true*/)
{
	bool bReplaced = FALSE;
	for(int i=0; i < m_pDatabase->table.totfield; i++)
	{
		if(stricmp(m_pDatabase->table.row[i].name,pFieldName)==0)
		{
			// se la tabella prevede indici, deve sottrarre il campo per la chiave primaria dal conteggio
			i = m_pDatabase->table.totindex > 0 ? i-1 : i;
			bReplaced = Replace(i,nOldValue,nNewValue,bLock);
			break;
		}
	}
	return(bReplaced);
}
bool CBase::Replace(int nFieldNum,double nOldValue,double nNewValue,bool bLock/*=true*/)
{
	// controlla che il costruttore abbia creato l'oggetto per la tabella
	ASSERTEXPR(IsValid());
	if(!IsValid())
		return(FALSE);

	// blocca l'accesso alla tabella (RAII Lock Guard)
	if(bLock)
	{
		CLockGuard guard(this);
		if(!guard.IsLocked())
		{
			m_nLastError = m_pBerkeleyDB->SetLastError(DB_RETCODE_ELOCKFAILURE);
			return(false);
		}
	}
	
	char szOld[32] = {0};
	char szNew[32] = {0};

	// aggiorna il campo con il nuovo valore
	PutField(nFieldNum,nNewValue);

	// da numero a double
	NUMBER Nr = {0};
	Nr.realnum = nOldValue;		
	FormatField(szOld,sizeof(szOld),Nr,realnum_type,nFieldNum);
	Nr.realnum = nNewValue;		
	FormatField(szNew,sizeof(szNew),Nr,realnum_type,nFieldNum);

	// sostituisce il campo della tabella (se la tabella prevede indici, salta la chiave primaria)
	return(m_pBerkeleyDB->Replace(m_pDatabase->table.totindex > 0 ? nFieldNum + 1 : nFieldNum,szOld,szNew)==0);
}
bool CBase::Replace(const char* pFieldName,double nOldValue,double nNewValue,bool bLock/*=true*/)
{
	bool bReplaced = FALSE;
	for(int i=0; i < m_pDatabase->table.totfield; i++)
	{
		if(stricmp(m_pDatabase->table.row[i].name,pFieldName)==0)
		{
			// se la tabella prevede indici, deve sottrarre il campo per la chiave primaria dal conteggio
			i = m_pDatabase->table.totindex > 0 ? i-1 : i;
			bReplaced = Replace(i,nOldValue,nNewValue,bLock);
			break;
		}
	}
	return(bReplaced);
}

/*
	Bof()

	Verifica se si trova all'inizio della tabella.

	NON accede in modo esclusivo.
*/
bool CBase::Bof(void) const
{
	// controlla che il costruttore abbia creato l'oggetto per la tabella
	ASSERTEXPR(IsValid());
	if(!IsValid())
		return(TRUE);

	return(m_pDatabase->table.stat.bof);
}

/*
	Eof()

	Verifica se si trova alla fine della tabella.

	NON accede in modo esclusivo.
*/
bool CBase::Eof(void) const
{
	// controlla che il costruttore abbia creato l'oggetto per la tabella
	ASSERTEXPR(IsValid());
	if(!IsValid())
		return(TRUE);

	return(m_pDatabase->table.stat.eof);
}

/*
	GoTop()

	Posiziona all'inizio della tabella, recuperando il record corrente.
	Se non viene specificato altrimenti, utilizza l'indice corrente.

	NON accede in modo esclusivo.
*/
bool CBase::GoTop(int nIndex/* = -1*/)
{
	// controlla che il costruttore abbia creato l'oggetto per la tabella
	ASSERTEXPR(IsValid());
	if(!IsValid())
		return(FALSE);

	return(CBase::GetFirst(nIndex));
}

/*
	GoBottom()

	Posiziona alla fine della tabella, recuperando il record corrente.
	Se non viene specificato altrimenti, utilizza l'indice corrente.

	NON accede in modo esclusivo.
*/
bool CBase::GoBottom(int nIndex/* = -1*/)
{
	// controlla che il costruttore abbia creato l'oggetto per la tabella
	ASSERTEXPR(IsValid());
	if(!IsValid())
		return(FALSE);

	return(CBase::GetLast(nIndex));
}

/*
	GetRecordCount()/GetRecordCountByCursor()

	Restituisce il totale dei record presenti nella tabella. Vedi le note in CBerkeleyDB.cpp.

	NON accede in modo esclusivo.
*/
ULONG CBase::GetRecordCount(void)
{
	// controlla che il costruttore abbia creato l'oggetto per la tabella
	ASSERTEXPR(IsValid());
	if(!IsValid())
		return((ULONG)-1L);

	return(m_pBerkeleyDB->GetRecordCount());
}
ULONG CBase::GetRecordCountByCursor(void)
{
	// controlla che il costruttore abbia creato l'oggetto per la tabella
	ASSERTEXPR(IsValid());
	if(!IsValid())
		return((ULONG)-1L);

	return(m_pBerkeleyDB->GetRecordCountByCursor());
}

/*
	GetCurrent()

	Recupera il record corrente della tabella.
	Se non viene specificato altrimenti, utilizza l'indice corrente.

	Tenere presente che, a differenza delle seguenti, la GetCurrent() e' una funzione di lettura
	e/o accesso, NON di movimento, come sono le GetFirst(), GetLast(), GetNext() e GetPrev().

	NON accede in modo esclusivo.
*/
bool CBase::GetCurrent(int nIndex/*=-1*/)
{
	// controlla che il costruttore abbia creato l'oggetto per la tabella
	ASSERTEXPR(IsValid());
	if(!IsValid())
		return(FALSE);

	// imposta l'indice
	if(nIndex >= 0)
		SetIndex(nIndex);

	// ricava il record corrente
	m_szPrimaryKeyValue[0] = '\0';

	int nCode = m_pBerkeleyDB->Get(DB_CURRENT); // DB_CURRENT/DB_SET

	// ha raggiunto un 'buco' x eliminazione nella tabella (vedi le note in 
	// GetRecordCount()/GetRecordCountByCursor()), quindi deve saltarlo
	if(nCode==DB_KEYEMPTY)
	{
		// prova a scavalcare il buco andando avanti
		nCode = m_pBerkeleyDB->Get(DB_NEXT);
		if(nCode==DB_NOTFOUND || nCode==DB_RETCODE_EOF) 
		{
			// se avanti non c'e' nulla, prova a tornare indietro
			nCode = m_pBerkeleyDB->Get(DB_PREV);
			if(nCode==DB_NOTFOUND || nCode==DB_RETCODE_BOF)
			{
				// se anche indietro e vuoto, la tabella e realmente vuota
				m_pDatabase->table.stat.bof = m_pDatabase->table.stat.eof = TRUE;
				//nCode = DB_NOTFOUND;
				nCode = DB_RETCODE_EOF;
			}
		}
	}

	switch(nCode)
	{
		case DB_NO_ERROR:
		case DB_OK:
			nCode = DB_OK;
			m_pDatabase->table.stat.bof = m_pDatabase->table.stat.eof = FALSE;
			memcpy(m_szPrimaryKeyValue,m_pDatabase->table.row[0].value,sizeof(m_szPrimaryKeyValue)-1);
			m_szPrimaryKeyValue[sizeof(m_szPrimaryKeyValue)-1] = '\0';
			break;

		case DB_RETCODE_BOF:
			m_pDatabase->table.stat.bof = TRUE;
			break;

		case DB_RETCODE_EOF:
			m_pDatabase->table.stat.eof = TRUE;
			break;

		case DB_NOTFOUND:
		case DB_RETCODE_NOTFOUND:
			// m_pDatabase->table.stat.bof = m_pDatabase->table.stat.eof = TRUE;
			break;

		default:
			ASSERTEXPRMSG(nCode!=nCode,"GetCurrent() received an unknown return code from Get()");
			break;
	}

	if(nCode!=DB_OK)
		m_nLastError = m_pBerkeleyDB->SetLastError(nCode);

	return(nCode==DB_OK);
}

/*
	GetFirst()

	Recupera il primo record della tabella.
	Se non viene specificato altrimenti, utilizza l'indice corrente.

	NON accede in modo esclusivo.
*/
bool CBase::GetFirst(int nIndex/*=-1*/)
{
	// controlla che il costruttore abbia creato l'oggetto per la tabella
	ASSERTEXPR(IsValid());
	if(!IsValid())
		return(FALSE);

	// imposta l'indice
	if(nIndex >= 0)
		SetIndex(nIndex);

	// ricava il primo record
	m_szPrimaryKeyValue[0] = '\0';

	int nCode = m_pBerkeleyDB->GetFirst(); // DB_FIRST/DB_SET
	switch(nCode)
	{
		case DB_NO_ERROR:
		case DB_OK:
			nCode = DB_OK;
			m_pDatabase->table.stat.bof = m_pDatabase->table.stat.eof = FALSE;
			memcpy(m_szPrimaryKeyValue,m_pDatabase->table.row[0].value,sizeof(m_szPrimaryKeyValue)-1);
			m_szPrimaryKeyValue[sizeof(m_szPrimaryKeyValue)-1] = '\0';
			break;

		// se al posizionarsi all'inizio della tabella non trova nulla, allora la tabella 
		// e' vuota, quindi deve impostare sia BOF che EOF
		case DB_NOTFOUND:
		case DB_RETCODE_NOTFOUND:
		case DB_RETCODE_BOF:
		case DB_RETCODE_EOF:
			m_pDatabase->table.stat.bof = m_pDatabase->table.stat.eof = TRUE;
			break;

		default:
			ASSERTEXPRMSG(nCode!=nCode,"GetFirst() received an unknown return code from Get()");
			break;
	}
			
	if(nCode!=DB_OK)
		m_nLastError = m_pBerkeleyDB->SetLastError(nCode);

	return(nCode==DB_OK);
}

/*
	GetNext()

	Recupera il record sucessivo della tabella.
	Se non viene specificato altrimenti, utilizza l'indice corrente.

	NON accede in modo esclusivo.
*/
bool CBase::GetNext(int nIndex/*=-1*/)
{
	// controlla che il costruttore abbia creato l'oggetto per la tabella
	ASSERTEXPR(IsValid());
	if(!IsValid())
		return(FALSE);

	// imposta l'indice
	if(nIndex >= 0)
		SetIndex(nIndex);

	// ricava il record successivo
	m_szPrimaryKeyValue[0] = '\0';

	int nCode = m_pBerkeleyDB->GetNext(); // DB_NEXT/DB_PREV
	switch(nCode)
	{
		case DB_NO_ERROR:
		case DB_OK:
			nCode = DB_OK;
			m_pDatabase->table.stat.bof = m_pDatabase->table.stat.eof = FALSE;
			memcpy(m_szPrimaryKeyValue,m_pDatabase->table.row[0].value,sizeof(m_szPrimaryKeyValue)-1);
			m_szPrimaryKeyValue[sizeof(m_szPrimaryKeyValue)-1] = '\0';
			break;

		case DB_RETCODE_BOF:
			nCode = DB_RETCODE_BOF;
			m_pDatabase->table.stat.bof = TRUE;
			break;

		case DB_NOTFOUND:
		case DB_RETCODE_NOTFOUND:
		case DB_RETCODE_EOF:
			nCode = DB_RETCODE_EOF;
			m_pDatabase->table.stat.eof = TRUE;
			break;

		default:
			ASSERTEXPRMSG(nCode!=nCode,"GetNext() received an unknown return code from Get()");
	}
			
	if(nCode!=DB_OK)
		m_nLastError = m_pBerkeleyDB->SetLastError(nCode);

	return(nCode==DB_OK);
}

/*
	GetPrev()

	Recupera il record precedente della tabella.
	Se non viene specificato altrimenti, utilizza l'indice corrente.

	NON accede in modo esclusivo.
*/
bool CBase::GetPrev(int nIndex/*=-1*/)
{
	// controlla che il costruttore abbia creato l'oggetto per la tabella
	ASSERTEXPR(IsValid());
	if(!IsValid())
		return(FALSE);

	// imposta l'indice
	if(nIndex >= 0)
		SetIndex(nIndex);

	// ricava il record precedente
	m_szPrimaryKeyValue[0] = '\0';

	int nCode = m_pBerkeleyDB->GetPrev(); // DB_PREV/DB_SERACH
	switch(nCode)
	{
		case DB_NO_ERROR:
		case DB_OK:
			nCode = DB_OK;
			m_pDatabase->table.stat.bof = m_pDatabase->table.stat.eof = FALSE;
			memcpy(m_szPrimaryKeyValue,m_pDatabase->table.row[0].value,sizeof(m_szPrimaryKeyValue)-1);
			m_szPrimaryKeyValue[sizeof(m_szPrimaryKeyValue)-1] = '\0';
			break;

		case DB_NOTFOUND:
		case DB_RETCODE_NOTFOUND:
		case DB_RETCODE_BOF:
			m_pDatabase->table.stat.bof = TRUE;
			break;

		case DB_RETCODE_EOF:
			m_pDatabase->table.stat.eof = TRUE;
			break;

		default:
			ASSERTEXPRMSG(nCode!=nCode,"GetPrev() received an unknown return code from Get()");
	}
			
	if(nCode!=DB_OK)
		m_nLastError = m_pBerkeleyDB->SetLastError(nCode);

	return(nCode==DB_OK);
}

/*
	GetLast()

	Recupera l'ultimo record della tabella.
	Se non viene specificato altrimenti, utilizza l'indice corrente.

	NON accede in modo esclusivo.
*/
bool CBase::GetLast(int nIndex/*=-1*/)
{
	// controlla che il costruttore abbia creato l'oggetto per la tabella
	ASSERTEXPR(IsValid());
	if(!IsValid())
		return(FALSE);

	// imposta l'indice
	if(nIndex >= 0)
		SetIndex(nIndex);

	// ricava l'ultimo record
	m_szPrimaryKeyValue[0] = '\0';

	int nCode = m_pBerkeleyDB->GetLast(); // DB_LAST/DB_SET
	switch(nCode)
	{
		case DB_NO_ERROR:
		case DB_OK:
			nCode = DB_OK;
			m_pDatabase->table.stat.bof = m_pDatabase->table.stat.eof = FALSE;
			memcpy(m_szPrimaryKeyValue,m_pDatabase->table.row[0].value,sizeof(m_szPrimaryKeyValue)-1);
			m_szPrimaryKeyValue[sizeof(m_szPrimaryKeyValue)-1] = '\0';
			break;

		// se al posizionarsi alla fine della tabella non trova nulla, allora la tabella 
		// e' vuota, quindi deve impostare sia BOF che EOF
		case DB_NOTFOUND:
		case DB_RETCODE_NOTFOUND:
		case DB_RETCODE_BOF:
		case DB_RETCODE_EOF:
			m_pDatabase->table.stat.bof = m_pDatabase->table.stat.eof = TRUE;
			break;

		default:
			ASSERTEXPRMSG(nCode!=nCode,"GetLast() received an unknown return code from Get()");
	}
			
	if(nCode!=DB_OK)
		m_nLastError = m_pBerkeleyDB->SetLastError(nCode);

	return(nCode==DB_OK);
}

/*
	Seek()

	Cerca la chiave (stringa) con l'indice specificato.
	Se si usa con un indice numerico direttamente, senza chiamare il membro relativo al tipo numerico, non formattare a spazi
	il numero nel buffer per la chiave da ricercare, dato che provvede in proprio a formattare a seconda del tipo (stringhe a
	sx e numeri a dx).
	I membri a seguire permettono la chiamata con il valore numerico, senza dover effettuare la formattazione.
	Da usare solo se la tabella prevede indici.
	Se non viene specificato altrimenti, utilizza l'indice corrente.

	NON accede in modo esclusivo.
*/
bool CBase::Seek(const char* pValue,int nIndex/*=-1*/)
{
	// controlla che il costruttore abbia creato l'oggetto per la tabella
	ASSERTEXPR(IsValid());
	if(!IsValid())
		return(FALSE);

	int	nKeySize;
	char szKey[MAX_KEYSIZE+1] = {0};
	int	nFieldNum,nFieldSize,nFieldDec;
	char cFieldType;

	// imposta l'indice corrente
	if(nIndex >= 0)
		SetIndex(nIndex);

	// imposta la chiave, allineando a seconda del tipo del campo (stringhe a sx, numeri a dx)
	m_szPrimaryKeyValue[0] = '\0';
	memset(szKey,' ',sizeof(szKey));
	nKeySize = (int)strlen(pValue);
	nKeySize = nKeySize >= sizeof(szKey) ? sizeof(szKey) : nKeySize;

	nFieldNum  = m_pDatabase->table.index[GetIndex()].fieldnum;
	nFieldSize = m_pDatabase->table.row[nFieldNum].size;
	nFieldDec  = m_pDatabase->table.row[nFieldNum].dec;
	cFieldType = m_pDatabase->table.row[nFieldNum].type;

	switch(cFieldType)
	{
		case 'S':
		case 'I':
		case 'N':
		case 'U':
		case 'R':
			memcpy(szKey + (nFieldSize-nKeySize),pValue,nKeySize);
			break;
		default:
			memcpy(szKey,pValue,nKeySize);
			break;
	}

	// imposta il campo (chiave) per la ricerca
	m_pBerkeleyDB->PutKey(szKey,GetIndex());

	// ricava il record relativo alla chiave
	int nCode = m_pBerkeleyDB->Get(DB_SEARCH,m_bSoftseek ? DB_SET_RANGE : DB_SET);
	switch(nCode)
	{
		case DB_NO_ERROR:
		case DB_OK:
			nCode = DB_OK;
			m_pDatabase->table.stat.bof = m_pDatabase->table.stat.eof = FALSE;
			memcpy(m_szPrimaryKeyValue,m_pDatabase->table.row[0].value,sizeof(m_szPrimaryKeyValue)-1);
			m_szPrimaryKeyValue[sizeof(m_szPrimaryKeyValue)-1] = '\0';
			break;

		case DB_RETCODE_BOF:
			m_pDatabase->table.stat.bof = TRUE;
			break;

		case DB_RETCODE_EOF:
			m_pDatabase->table.stat.eof = TRUE;
			break;

		case DB_NOTFOUND:
		case DB_RETCODE_NOTFOUND:
			break;

		default:
			ASSERTEXPRMSG(nCode!=nCode,"Seek() received an unknown return code from Get()");
	}
		
	if(nCode!=DB_OK)
		m_nLastError = m_pBerkeleyDB->SetLastError(nCode);

	return(nCode==DB_OK);
}
bool CBase::Seek(short int nValue,int nIndex/*=-1*/)
{
	char szKey[MAX_KEYSIZE+1] = {0};
	snprintf(szKey,sizeof(szKey),"%d",nValue);
	return(Seek(szKey,nIndex));
}
bool CBase::Seek(int nValue,int nIndex/*=-1*/)
{
	char szKey[MAX_KEYSIZE+1] = {0};
	snprintf(szKey,sizeof(szKey),"%d",nValue);
	return(Seek(szKey,nIndex));
}
bool CBase::Seek(long nValue,int nIndex/*=-1*/)
{
	char szKey[MAX_KEYSIZE+1] = {0};
	snprintf(szKey,sizeof(szKey),"%ld",nValue);
	return(Seek(szKey,nIndex));
}
bool CBase::Seek(unsigned long nValue,int nIndex/*=-1*/)
{
	char szKey[MAX_KEYSIZE+1] = {0};
	snprintf(szKey,sizeof(szKey),"%lu",nValue);
	return(Seek(szKey,nIndex));
}
bool CBase::Seek(double nValue,int nIndex/*=-1*/)
{
	// deve ricavare il numero di decimali del campo per la formattazione
	char szKey[MAX_KEYSIZE+1] = {0};
	int nIndexNum = GetIndex();
	int nFieldNum = -1;
	int nDec = 0;

	// cerca il campo della tabella relativo all'indice (corrente)
	for(int i = 0; i < m_pDatabase->table.totindex; i++)
		if(i==nIndexNum)
		{
			nFieldNum = m_pDatabase->table.index[i].fieldnum;
			break;
		}

	// ricava il numero di decimali del campo
	if(nFieldNum!=-1)
		nDec = m_pDatabase->table.row[nFieldNum].dec;

	// formatta il valore secondo i decimali del campo
	char szFormat[10] = {0};
	snprintf(szFormat,sizeof(szFormat),"%c.%d%c",'%',nDec,'f');
	snprintf(szKey,sizeof(szKey),szFormat,nValue);

	return(Seek(szKey,nIndex));
}

/*
	SeekPrimaryKey()

	Cerca la chiave primaria.
	La tabella deve prevedere indici.

	NON accede in modo esclusivo.
*/
bool CBase::SeekPrimaryKey(const char* pPrimaryKey)
{
	// controlla che il costruttore abbia creato l'oggetto per la tabella
	ASSERTEXPR(IsValid());
	if(!IsValid())
		return(FALSE);

	return(m_pBerkeleyDB->GetPrimaryKey(pPrimaryKey)==0);
}

/*
	Find()

	Cerca il record relativo alla chiave.
	Da usare quando la tabella non prevede indici (notare che in tal caso viene considerato come chiave
	l'intero record).

	NON accede in modo esclusivo.
*/
bool CBase::Find(const char* pValue)
{
	// controlla che il costruttore abbia creato l'oggetto per la tabella
	ASSERTEXPR(IsValid());
	if(!IsValid())
		return(FALSE);

	int	nKeySize;
	char szKey[MAX_KEYSIZE+1] = {0};
	int	nFieldNum,nFieldSize;
	char cFieldType;

	// imposta la chiave, allineando a seconda del tipo del campo (stringhe a sx, numeri a dx)
	m_szPrimaryKeyValue[0] = '\0';
	memset(szKey,' ',sizeof(szKey));
	nKeySize = (int)strlen(pValue);
	nKeySize = nKeySize >= sizeof(szKey) ? sizeof(szKey) : nKeySize;

	nFieldNum  = 0;
	nFieldSize = m_pDatabase->table.row[nFieldNum].size;
	cFieldType = m_pDatabase->table.row[nFieldNum].type;

	switch(cFieldType)
	{
		case 'S':
		case 'I':
		case 'N':
		case 'U':
		case 'R':
			memcpy(szKey + (nFieldSize-nKeySize),pValue,nKeySize);
			break;
		default:
			memcpy(szKey,pValue,nKeySize);
			break;
	}

	// imposta il campo (chiave) per la ricerca
	m_pBerkeleyDB->PutKey(szKey,0);

	// ricava il record relativo alla chiave
	int nCode = m_pBerkeleyDB->Get(DB_SEARCH,DB_SET);
	switch(nCode)
	{
		case DB_NO_ERROR:
		case DB_OK:
			nCode = DB_OK;
			m_pDatabase->table.stat.bof = m_pDatabase->table.stat.eof = FALSE;
			break;

		case DB_RETCODE_BOF:
			m_pDatabase->table.stat.bof = TRUE;
			break;

		case DB_RETCODE_EOF:
		default:
			m_pDatabase->table.stat.eof = TRUE;
			break;
	}
		
	if(nCode!=DB_OK)
		m_nLastError = m_pBerkeleyDB->SetLastError(nCode);

	return(nCode==DB_OK);
}

/*
	GetIndex()

	Ricava l'indice corrente (a base 0), o -1 per errore.

	NON accede in modo esclusivo.
*/
int CBase::GetIndex(void)
{
	// controlla che il costruttore abbia creato l'oggetto per la tabella
	ASSERTEXPR(IsValid());
	if(!IsValid())
		return(-1);

	return(m_pBerkeleyDB->GetCursorNumber());
}

/*
	SetIndex()

	Imposta l'indice corrente (a base 0), restituendo il valore anteriore o -1 per errore.

	NON accede in modo esclusivo.
*/
int CBase::SetIndex(int nIndex)
{
	// controlla che il costruttore abbia creato l'oggetto per la tabella
	ASSERTEXPR(IsValid());
	if(!IsValid())
		return(-1);

	int nCurrentIndex = m_pBerkeleyDB->GetCursorNumber();
	m_pBerkeleyDB->SetCursor(nIndex);

	return(nCurrentIndex);
}

/*
	ResetIndex()

	Resetta l'indice corrente (reimposta sulla chiave primaria).

	NON accede in modo esclusivo.
*/
void CBase::ResetIndex(void)
{
	// controlla che il costruttore abbia creato l'oggetto per la tabella
	ASSERTEXPR(IsValid());
	if(!IsValid())
		return;

	m_pBerkeleyDB->ResetCursor();
}

/*
	Reindex()

	Ricrea l'indice specificato, se viene passato -1 ricrea tutti gli indici.

	Accede in modo esclusivo (RAAI Lock Guard).
*/
bool CBase::Reindex(int nIndex/*=-1*/)
{
	// controlla che il costruttore abbia creato l'oggetto per la tabella
	ASSERTEXPR(IsValid());
	if(!IsValid())
		return(FALSE);

	// blocca l'accesso alla tabella (RAII Lock Guard)
	CLockGuard guard(this);
    if(!guard.IsLocked())
    {
        m_nLastError = m_pBerkeleyDB->SetLastError(DB_RETCODE_ELOCKFAILURE);
        return(FALSE);
    }

	return(m_pBerkeleyDB->Reindex(nIndex)==0);
}

/*
	CheckIndex()

	Verifica l'indice specificato, se viene passato -1 verifica tutti gli indici.

	Accede in modo esclusivo (RAII Lock Guard).
*/
bool CBase::CheckIndex(int nIndex/* = -1*/)
{
	// controlla che il costruttore abbia creato l'oggetto per la tabella
	ASSERTEXPR(IsValid());
	if(!IsValid())
		return(FALSE);

	// blocca l'accesso alla tabella (RAII Lock Guard)
	CLockGuard guard(this);
    if(!guard.IsLocked())
    {
        m_nLastError = m_pBerkeleyDB->SetLastError(DB_RETCODE_ELOCKFAILURE);
        return(FALSE);
    }

	return(m_pBerkeleyDB->CheckIndex(nIndex)==0);
}

/*
	GetIndexCount()

	Restituisce il numero di indici della tabella, o -1 per errore.

	NON accede in modo esclusivo.
*/
int CBase::GetIndexCount(void) const
{
	// controlla che il costruttore abbia creato l'oggetto per la tabella
	ASSERTEXPR(IsValid());
	if(!IsValid())
		return(-1);

	return(m_pDatabase->table.totindex);
}

/*
	GetIndexNames()

	Restituisce i nomi degli indici della tabella (da chiamare in un ciclo fino a che non restituisce NULL).

	NON accede in modo esclusivo.
*/
const char* CBase::GetIndexNames(int& nIterator)
{
	// controlla che il costruttore abbia creato l'oggetto per la tabella
	ASSERTEXPR(IsValid());
	if(!IsValid())
		return(NULL);

	char* pIndexName = NULL;

	if(nIterator < m_pDatabase->table.totindex)
	{
		pIndexName = m_pDatabase->table.index[nIterator].name;
		nIterator++;
	}
	else
	{
		pIndexName = NULL;
		nIterator = 0;
	}

	return(pIndexName);
}

/*
	GetIndexNumberByName()

	Restituisce il numero dell'indice (a base 0) relativo al nome, o -1 per errore.

	NON accede in modo esclusivo.
*/
int CBase::GetIndexNumberByName(const char* pIndexName)
{
	// controlla che il costruttore abbia creato l'oggetto per la tabella
	ASSERTEXPR(IsValid());
	if(!IsValid())
		return(-1);

	int nIndexNum = -1;

	if(m_pDatabase->table.totindex > 0)
	{
		for(nIndexNum = 0; nIndexNum < m_pDatabase->table.totindex; nIndexNum++)
			if(strcmp(m_pDatabase->table.index[nIndexNum].name,pIndexName)==0)
				break;
	}

	if(nIndexNum==-1)
        m_nLastError = m_pBerkeleyDB->SetLastError(DB_RETCODE_EINVALIDINDEX);

	return(nIndexNum);
}

/*
	GetIndexNameByNumber()

	Restituisce il nome dell'indice relativo al numero (a base 0), o NULL per errore.

	NON accede in modo esclusivo.
*/
const char* CBase::GetIndexNameByNumber(int nIndex)
{
	// controlla che il costruttore abbia creato l'oggetto per la tabella
	ASSERTEXPR(IsValid());
	if(!IsValid())
		return(NULL);

	char* pIndexName = NULL;

	if(m_pDatabase->table.totindex > 0)
	{
		for(int i = 0; i < m_pDatabase->table.totindex; i++)
			if(i==nIndex)
			{
				pIndexName = m_pDatabase->table.index[i].name;
				break;
			}
	}

	if(!pIndexName)
        m_nLastError = m_pBerkeleyDB->SetLastError(DB_RETCODE_EINVALIDINDEX);

	return(pIndexName);
}

/*
	GetFieldNumberOfIndex()

	Ricava il progressivo del campo (a base 0) relativo all'indice specificato, o -1 per errore.

	NON accede in modo esclusivo.
*/
int CBase::GetFieldNumberOfIndex(int nIndex)
{
	// controlla che il costruttore abbia creato l'oggetto per la tabella
	ASSERTEXPR(IsValid());
	if(!IsValid())
		return(-1);

	int nFieldNum = -1;

	if(m_pDatabase->table.totindex > 0)
		if(nIndex >= 0 && nIndex < m_pDatabase->table.totindex)
			nFieldNum = m_pDatabase->table.index[nIndex].fieldnum - 1;

	if(nFieldNum==-1)
        m_nLastError = m_pBerkeleyDB->SetLastError(DB_RETCODE_EINVALIDINDEX);

	return(nFieldNum);
}

/*
	GetFieldNameOfIndex()

	Ricava il nome del campo (a base 0) relativo all'indice specificato, o NULL per errore.

	NON accede in modo esclusivo.
*/
const char* CBase::GetFieldNameOfIndex(int nIndex)
{
	// controlla che il costruttore abbia creato l'oggetto per la tabella
	ASSERTEXPR(IsValid());
	if(!IsValid())
		return(NULL);

	char* pFieldName = NULL;

	if(m_pDatabase->table.totindex > 0)
		if(nIndex >= 0 && nIndex < m_pDatabase->table.totindex)
			pFieldName = (char*)m_pDatabase->table.index[nIndex].fieldname;

	if(!pFieldName)
        m_nLastError = m_pBerkeleyDB->SetLastError(DB_RETCODE_EINVALIDINDEX);

	return(pFieldName);
}

/*
	GetFieldCount()

	Restituisce il numero di campi della tabella, senza includere l'eventuale chiave primaria.

	NON accede in modo esclusivo.
*/
int CBase::GetFieldCount(void)
{
	// controlla che il costruttore abbia creato l'oggetto per la tabella
	ASSERTEXPR(IsValid());
	if(!IsValid())
		return(0);

	// se la tabella prevede indici salta il primo campo (relativo alla chiave primaria)
	int nFields = m_pDatabase->table.totindex > 0 ? m_pDatabase->table.totfield - 1 : m_pDatabase->table.totfield;

	return(nFields);
}

/*
	GetFieldNames()

	Restituisce i nomi dei campi della tabella (da chiamare in un ciclo fino a che non restituisce NULL),
	senza includere l'eventuale chiave primaria.

	NON accede in modo esclusivo.
*/
const char* CBase::GetFieldNames(int& nIterator)
{
	// controlla che il costruttore abbia creato l'oggetto per la tabella
	ASSERTEXPR(IsValid());
	if(!IsValid())
		return(NULL);

	char* pFieldName = NULL;

	// se la tabella prevede indici salta il primo campo (relativo alla chiave primaria)
	if(nIterator <= 0)
		nIterator = m_pDatabase->table.totindex > 0 ? 1 : 0;

	if(nIterator < m_pDatabase->table.totfield)
	{
		pFieldName = m_pDatabase->table.row[nIterator].name;
		nIterator++;
	}
	else
	{
		pFieldName = NULL;
		nIterator = -1;
	}

	return(pFieldName);
}

/*
	GetFieldNumberByName()

	Restituisce il numero del campo (a base 0) o -1 per errore,
	senza includere l'eventuale chiave primaria.

	NON accede in modo esclusivo.
*/
int CBase::GetFieldNumberByName(const char* pFieldName)
{
	// controlla che il costruttore abbia creato l'oggetto per la tabella
	ASSERTEXPR(IsValid());
	if(!IsValid())
		return(-1);

	int nFieldNum = -1;

	if(m_pDatabase->table.totfield > 0)
	{
		for(nFieldNum = 0; nFieldNum < m_pDatabase->table.totfield; nFieldNum++)
		{
			if(strcmp(m_pDatabase->table.row[nFieldNum].name,pFieldName)==0)
			{
				// se la tabella prevede indici salta il primo campo (relativo alla chiave primaria)
				nFieldNum = m_pDatabase->table.totindex > 0 ? nFieldNum-1 : nFieldNum;
				break;
			}
		}

		if(nFieldNum==m_pDatabase->table.totfield)
			nFieldNum = -1;
	}

	if(nFieldNum==-1)
        m_nLastError = m_pBerkeleyDB->SetLastError(DB_RETCODE_EINVALIDFIELDNUMBER);

	return(nFieldNum);
}

/*
	GetFieldNameByNumber()

	Restituisce il nome del campo relativo al numero (a base 0), o NULL per errore,
	senza includere l'eventuale chiave primaria.

	NON accede in modo esclusivo.
*/
const char* CBase::GetFieldNameByNumber(int nFieldNum)
{
	// controlla che il costruttore abbia creato l'oggetto per la tabella
	ASSERTEXPR(IsValid());
	if(!IsValid())
		return(NULL);

	char* pFieldName = NULL;

	if(m_pDatabase->table.totfield > 0)
	{
		// se la tabella prevede indici salta il primo campo (relativo alla chiave primaria)
		nFieldNum = m_pDatabase->table.totindex > 0 ? nFieldNum + 1 : nFieldNum;
			
		for(int i = 0; i < m_pDatabase->table.totfield; i++)
			if(i==nFieldNum)
			{
				pFieldName = m_pDatabase->table.row[i].name;
				break;
			}
	}

	if(!pFieldName)
        m_nLastError = m_pBerkeleyDB->SetLastError(DB_RETCODE_EINVALIDFIELDNUMBER);

	return(pFieldName);
}

/*
	GetFieldInfoByNumber()

	Restituisce le informazioni relative al campo (passare il progressivo).

	NON accede in modo esclusivo.
*/
bool CBase::GetFieldInfoByNumber(int nFieldNum,char* pFieldName/*=NULL*/,int nSize/*=0*/,int* nFieldType/*=NULL*/,int* nFieldSize/*=NULL*/,int* nFieldDec/*=NULL*/,int* nFieldNumber/*=NULL*/,int* nIndexNum/*=NULL*/)
{
	// controlla che il costruttore abbia creato l'oggetto per la tabella
	ASSERTEXPR(IsValid());
	if(!IsValid())
		return(FALSE);

	bool bGet = FALSE;

	if(pFieldName)   memset(pFieldName,'\0',nSize);
	if(nFieldType)   *nFieldType   = '?';
	if(nFieldSize)   *nFieldSize   = -1;
	if(nFieldDec)    *nFieldDec    = -1;
	if(nFieldNumber) *nFieldNumber = -1;
	if(nIndexNum)    *nIndexNum    = -1;

	if(m_pDatabase->table.totfield > 0)
	{
		// se la tabella prevede indici salta il primo campo (relativo alla chiave primaria)
		nFieldNum = m_pDatabase->table.totindex > 0 ? nFieldNum + 1 : nFieldNum;
			
		for(int i = 0; i < m_pDatabase->table.totfield; i++)
			if(i==nFieldNum)
			{
				if(pFieldName)   strcpyn(pFieldName,m_pDatabase->table.row[i].name,nSize);
				if(nFieldType)   *nFieldType   = m_pDatabase->table.row[i].type;
				if(nFieldSize)   *nFieldSize   = m_pDatabase->table.row[i].size;
				if(nFieldDec)    *nFieldDec    = m_pDatabase->table.row[i].dec;
				if(nFieldNumber) *nFieldNumber = m_pDatabase->table.row[i].num;
				if(nIndexNum && m_pDatabase->table.totindex > 0)
				{
					for(int n = 0; n < m_pDatabase->table.totindex; n++)
						if(m_pDatabase->table.index[n].fieldnum==nFieldNum)
						{
							*nIndexNum = n;
							break;
						}
				}
				bGet = TRUE;
				break;
			}
	}

	if(!bGet)
        m_nLastError = m_pBerkeleyDB->SetLastError(DB_RETCODE_EINVALIDFIELDNUMBER);
	
	return(bGet);
}

/*
	GetFieldInfoByName()

	Restituisce le informazioni relative al campo (passare il nome).

	NON accede in modo esclusivo.
*/
bool CBase::GetFieldInfoByName(const char* pFieldName,int* nFieldType/*=NULL*/,int* nFieldSize/*=NULL*/,int* nFieldDec/*=NULL*/,int* nFieldNumber/*=NULL*/,int* nIndexNum/*=NULL*/)
{
	// controlla che il costruttore abbia creato l'oggetto per la tabella
	ASSERTEXPR(IsValid());
	if(!IsValid())
		return(FALSE);

	bool bGet = FALSE;

	if(nFieldType)   *nFieldType   = '?';
	if(nFieldSize)   *nFieldSize   = -1;
	if(nFieldDec)    *nFieldDec    = -1;
	if(nFieldNumber) *nFieldNumber = -1;
	if(nIndexNum)    *nIndexNum    = -1;

	if(m_pDatabase->table.totfield > 0)
	{
		for(int i = 0; i < m_pDatabase->table.totfield; i++)
			if(strcmp(m_pDatabase->table.row[i].name,pFieldName)==0)
			{
				if(nFieldType)   *nFieldType   = m_pDatabase->table.row[i].type;
				if(nFieldSize)   *nFieldSize   = m_pDatabase->table.row[i].size;
				if(nFieldDec)    *nFieldDec    = m_pDatabase->table.row[i].dec;
				if(nFieldNumber) *nFieldNumber = m_pDatabase->table.row[i].num;
				if(nIndexNum && m_pDatabase->table.totindex > 0)
				{
					for(int n = 0; n < m_pDatabase->table.totindex; n++)
						if(strcmp(m_pDatabase->table.index[n].fieldname,pFieldName)==0)
						{
							*nIndexNum = n;
							break;
						}
				}
				bGet = TRUE;
				break;
			}
	}

	if(!bGet)
        m_nLastError = m_pBerkeleyDB->SetLastError(DB_RETCODE_EINVALIDFIELDNUMBER);
	
	return(bGet);
}

/*
	NOTA:
	dato che il concetto di chiave primaria/indici viene implementato qui, nell'interfaccia (CBase), e non nel motore
	del database (CBerkeleyDB), tutte le funzioni che operano sui campi per numero progressivo invece che per nome,
	devono saltare l'eventuale (primo) campo relativo alla chiave primaria, dato che per il chiamante i numeri progressivi
	per i campi sono sempre a base 0, a prescindere se la tabella usi indici o meno

*/

/*
	GetFieldRaw()

	Ricava il contenuto del campo interno (relativo al record corrente della tabella) come stringa,
	ignorando le formattazioni relative al tipo.
	Specificare il progressivo numerico e non il nome del campo.

	NON accede in modo esclusivo.
*/
const char* CBase::GetFieldRaw(int nFieldNum,char* pBuffer,int nSize)
{
	// controlla che il costruttore abbia creato l'oggetto per la tabella
	ASSERTEXPR(IsValid());
	if(!IsValid())
		return(NULL);

	char* pField = NULL;

	// se la tabella prevede indici salta il primo campo (relativo alla chiave primaria)
	nFieldNum = m_pDatabase->table.totindex > 0 ? nFieldNum + 1 : nFieldNum;

	// restituisce il puntatore al un buffer interno, sovrascritto dalle chiamate successive
	//$ pField = (char*)m_pBerkeleyDB->GetField(nFieldNum);
	if(m_pBerkeleyDB->GetField(nFieldNum,pBuffer,nSize))
		pField = pBuffer;

	if(!pField)
        m_nLastError = m_pBerkeleyDB->SetLastError(DB_RETCODE_EINVALIDFIELDNUMBER);
	
	return(pField);
}

/*
	GetFieldRaw()

	Ricava il contenuto del campo interno (relativo al record corrente della tabella) come stringa,
	ignorando le formattazioni relative al tipo.
	Specificare il nome del campo e non il progressivo numerico.

	NON accede in modo esclusivo.
*/
const char* CBase::GetFieldRaw(const char* pFieldName,char* pBuffer,int nSize)
{
	// controlla che il costruttore abbia creato l'oggetto per la tabella
	ASSERTEXPR(IsValid());
	if(!IsValid())
		return(NULL);

	return(GetFieldRaw(GetFieldNumberByName(pFieldName),pBuffer,nSize));
}

/*
	GetFieldUnion()

	Ricava il contenuto del campo interno (relativo al record corrente della tabella), impostando
	la struttura (union) interna.
	Specificare il nome del campo e non il progressivo numerico.

	Restituisce NULL in caso di errore.

	NON accede in modo esclusivo.
*/
FIELD* CBase::GetFieldUnion(int nFieldNum,FIELD* f)
{
	// controlla che il costruttore abbia creato l'oggetto per la tabella
	ASSERTEXPR(IsValid());
	if(!IsValid())
		return(NULL);

	// controlla la validita' del numero del campo
	if(nFieldNum < 0 || nFieldNum > m_pDatabase->table.totfield)
	{
        m_nLastError = m_pBerkeleyDB->SetLastError(DB_RETCODE_EINVALIDFIELDNUMBER);
		return(NULL);
	}

	int nField = -1;
	char cType = 0;
	char szBuffer[MAX_FIELDSIZE+1] = {0};

	memset(f,'\0',sizeof(FIELD));

	// se la tabella prevede indici salta il primo campo (la chiave primaria)
	int n = m_pDatabase->table.totindex > 0 ? nFieldNum + 1 : nFieldNum;
	for(int i=0; i < m_pDatabase->table.totfield; i++)
		if(i==n)
		{
			f->type = cType = m_pDatabase->table.row[i].type;
			f->size = m_pDatabase->table.row[i].size;
			f->dec = m_pDatabase->table.row[i].dec;
			nField = m_pDatabase->table.totindex > 0 ? nFieldNum + 1 : nFieldNum;
			break;
		}

	// copia nel buffer locale il valore del campo della tabella
	if(m_pBerkeleyDB->GetField(nField,szBuffer,sizeof(szBuffer)))
	{
		// formatta il contenuto del buffer nel campo relativo della union a seconda del tipo
		switch(cType)
		{
			// carattere
			case 'C':
				strcpyn(f->field.buffer,szBuffer,sizeof(f->field.buffer));
				break;
				
			// ora
			case 'T':
				memcpy(&(f->field.buffer),szBuffer,2);
				m_DateTime.SetHour(atoi(f->field.buffer));
				memcpy(&(f->field.buffer),szBuffer+2,2);
				m_DateTime.SetMin(atoi(f->field.buffer));
				memcpy(&(f->field.buffer),szBuffer+4,2);
				m_DateTime.SetSec(atoi(f->field.buffer));
				strcpyn(f->field.buffer,m_DateTime.GetFormattedTime(FALSE),sizeof(f->field.buffer));
				break;
				
			// data
			case 'D':
				memcpy(&(f->field.buffer),szBuffer+6,2);
				m_DateTime.SetDay(atoi(f->field.buffer));
				memcpy(&(f->field.buffer),szBuffer+4,2);
				m_DateTime.SetMonth(atoi(f->field.buffer));
				memcpy(&(f->field.buffer),m_bCentury ? szBuffer : szBuffer+2,m_bCentury ? 4 : 2);
				m_DateTime.SetYear(atoi(f->field.buffer));
				strcpyn(f->field.buffer,m_DateTime.GetFormattedDate(FALSE),sizeof(f->field.buffer));
				break;

			// boolean
			case 'B':
				f->field.boolean = (szBuffer[0]=='T' ? TRUE : (szBuffer[0]=='F') ? FALSE : FALSE);
				break;
				
			// short int
			case 'S':
				f->field.shortint = (short int)atoi(szBuffer);
				break;
				
			// int
			case 'I':
				f->field.integer = (int)atoi(szBuffer);
				break;
				
			// long
			case 'N':
				f->field.longint = (long)atol(szBuffer);
				break;
				
			// unsigned long
			case 'U':
				f->field.ulongint = strtoul(szBuffer,NULL,0);
				break;
				
			// double
			case 'R':
				f->field.realnum = (double)atof(szBuffer);
				break;
		}
				
		f = &m_Field;

		return(f);
	}

	return(NULL);
}

/*
	GetFieldUnion()

	Ricava il contenuto del campo interno (relativo al record corrente della tabella), impostando
	la struttura (union) interna.
	Specificare il progressivo numerico e non il nome del campo.

	NON accede in modo esclusivo.
*/
FIELD* CBase::GetFieldUnion(const char* pFieldName,FIELD* f)
{
	// controlla che il costruttore abbia creato l'oggetto per la tabella
	ASSERTEXPR(IsValid());
	if(!IsValid())
		return(NULL);

	return(GetFieldUnion(GetFieldNumberByName(pFieldName),f));
}

/*
	GetField()

	Ricava, come carattere, il contenuto del campo interno (relativo al record corrente della tabella).
	Specificare il progressivo numerico e non il nome del campo.
	Passare la dimensione reale del buffer (ossia sizeof()) e non sizeof(...)-1) perche' qui gia' sottrae
	1 alla dimensione reale per terminare con il NULL.

	NON accede in modo esclusivo.
*/
char* CBase::GetField(int nFieldNum,char* pBuffer,int nSize,bool bTrim/*=FALSE*/)
{
	// controlla che il costruttore abbia creato l'oggetto per la tabella
	ASSERTEXPR(IsValid());
	if(!IsValid())
		return(NULL);

	char* pField = NULL;

	ASSERTEXPR(nFieldNum >= 0);

	if(nFieldNum >= 0)
	{
		// se la tabella prevede indici salta il primo campo (relativo alla chiave primaria)
		nFieldNum = m_pDatabase->table.totindex > 0 ? nFieldNum + 1 : nFieldNum;

		// restituisce il puntatore al buffer fornito dal chiamante
		if(m_pBerkeleyDB->GetField(nFieldNum,pBuffer,nSize))
		{
			if(bTrim)
				strrtrim(pBuffer);
			pField = pBuffer;
		}
	}
    else
		m_nLastError = m_pBerkeleyDB->SetLastError(DB_RETCODE_EINVALIDFIELDNUMBER);
	
	return(pField);
}

/*
	GetField()

	Ricava, come carattere, il contenuto del campo interno (relativo al record corrente della tabella).
	Specificare il nome del campo e non il progressivo numerico.

	NON accede in modo esclusivo.
*/
char* CBase::GetField(const char* pFieldName,char* pBuffer,int nSize,bool bTrim/*=FALSE*/)
{
	return(GetField(GetFieldNumberByName(pFieldName),pBuffer,nSize,bTrim));
}

/*
	GetField()

	Ricava, come boolean, il contenuto del campo interno (relativo al record corrente della tabella).
	Specificare il progressivo numerico e non il nome del campo.

	NON accede in modo esclusivo.
*/
bool CBase::GetField(int nFieldNum,bool& bValue)
{
	// controlla che il costruttore abbia creato l'oggetto per la tabella
	ASSERTEXPR(IsValid());
	if(!IsValid())
		return(FALSE);

	bValue = FALSE;

	if(nFieldNum >= 0)
	{
		// da carattere a boolean
		char szValue[FIELD_BOOLEAN_MAXSIZE+1] = {0};
		const char* pValue = GetFieldRaw(nFieldNum,szValue,sizeof(szValue));

		if(pValue[0]=='F')
			bValue = FALSE;
		else if(pValue[0]=='T')
			bValue = TRUE;
	}
    else
		m_nLastError = m_pBerkeleyDB->SetLastError(DB_RETCODE_EINVALIDFIELDNUMBER);

	return(bValue);
}

/*
	GetField()

	Ricava, come boolean, il contenuto del campo interno (relativo al record corrente della tabella).
	Specificare il nome del campo e non il progressivo numerico.

	NON accede in modo esclusivo.
*/
bool CBase::GetField(const char* pFieldName,bool& bValue)
{
	// controlla che il costruttore abbia creato l'oggetto per la tabella
	ASSERTEXPR(IsValid());
	if(!IsValid())
		return(FALSE);

	return(GetField(GetFieldNumberByName(pFieldName),bValue));
}

/*
	GetField()

	Ricava, come data, il contenuto del campo interno (relativo al record corrente della tabella).
	Specificare il progressivo numerico e non il nome del campo.

	NON accede in modo esclusivo.
*/
CDateTime& CBase::GetField(int nFieldNum,CDateTime& pDateTime)
{
	// controlla che il costruttore abbia creato l'oggetto per la tabella
	ASSERTEXPR(IsValid());
	if(!IsValid())
		return(pDateTime);

	if(nFieldNum >= 0)
	{
		// da carattere a data/ora
		char szField[MAX_FIELDSIZE+1] = {0};
		int nField    = -1;
		char cType    = 0;
		char year[5]  = {0};
		char month[3] = {0};
		char day[3]   = {0};
		char hour[3]  = {0};
		char min[3]   = {0};
		char sec[3]   = {0};

		// ricava il tipo del campo (T,D)
		// se la tabella prevede indici salta il primo campo (relativo alla chiave primaria)
		int n = m_pDatabase->table.totindex > 0 ? nFieldNum + 1 : nFieldNum;
		for(int i = 0; i < m_pDatabase->table.totfield; i++)
			if(i==n)
			{
				cType = m_pDatabase->table.row[i].type;
				nField = m_pDatabase->table.totindex > 0 ? nFieldNum + 1 : nFieldNum;
				break;
			}

		if(nField!=-1)
		{
			// ricava il contenuto del campo
			if(m_pBerkeleyDB->GetField(nField,szField,sizeof(szField)))
			{
				switch(cType)
				{
					// ora (hhmmss)
					case 'T':
						memcpy(hour,szField,2);
						memcpy(min,szField+2,2);
						memcpy(sec,szField+4,2);
						pDateTime.SetHour(atoi(hour));
						pDateTime.SetMin(atoi(min));
						pDateTime.SetSec(atoi(sec));
						break;
				
					// data (yyyymmdd)
					case 'D':
					{
						bool bBC = FALSE;
						char szDate[16] = {0};
						DateFormatFromTable(szField,szDate,sizeof(szDate),YMD_ISO8601,bBC);
						memcpy(year,szField,4);
						memcpy(month,szField+4,2);
						memcpy(day,szField+6,2);
						pDateTime.SetYear(atoi(year));
						pDateTime.SetMonth(atoi(month));
						pDateTime.SetDay(atoi(day));
						break;
					}
				}
			}
		}
	}
    else
		m_nLastError = m_pBerkeleyDB->SetLastError(DB_RETCODE_EINVALIDFIELDNUMBER);

	return(pDateTime);
}

/*
	GetField()

	Ricava, come data, il contenuto del campo interno (relativo al record corrente della tabella).
	Specificare il nome del campo e non il progressivo numerico.

	NON accede in modo esclusivo.
*/
CDateTime& CBase::GetField(const char* pFieldName,CDateTime& pDateTime)
{
	// controlla che il costruttore abbia creato l'oggetto per la tabella
	ASSERTEXPR(IsValid());
	if(!IsValid())
		return(pDateTime);

	return(GetField(GetFieldNumberByName(pFieldName),pDateTime));
}

/*
	GetField()

	Ricava, come short int, il contenuto del campo interno (relativo al record corrente della tabella).
	Specificare il progressivo numerico e non il nome del campo.

	NON accede in modo esclusivo.
*/
short int CBase::GetField(int nFieldNum,short int& nValue)
{
	// controlla che il costruttore abbia creato l'oggetto per la tabella
	ASSERTEXPR(IsValid());
	if(!IsValid())
		return((short int)-1);

	nValue = (short int)-1;

	if(nFieldNum >= 0)
	{
		// da numero a short int
		NUMBER Ns = {0};
		GetNumericField(nFieldNum,Ns,shortint_type);
		nValue = Ns.shortint;
	}
    else
		m_nLastError = m_pBerkeleyDB->SetLastError(DB_RETCODE_EINVALIDFIELDNUMBER);

	return(nValue);
}

/*
	GetField()

	Ricava, come short int, il contenuto del campo interno (relativo al record corrente della tabella).
	Specificare il nome del campo e non il progressivo numerico.

	NON accede in modo esclusivo.
*/
short int CBase::GetField(const char* pFieldName,short int& nValue)
{
	// controlla che il costruttore abbia creato l'oggetto per la tabella
	ASSERTEXPR(IsValid());
	if(!IsValid())
		return((short int)-1);

	return(GetField(GetFieldNumberByName(pFieldName),nValue));
}

/*
	GetField()

	Ricava, come int, il contenuto del campo interno (relativo al record corrente della tabella).
	Specificare il progressivo numerico e non il nome del campo.

	NON accede in modo esclusivo.
*/
int CBase::GetField(int nFieldNum,int& nValue)
{
	// controlla che il costruttore abbia creato l'oggetto per la tabella
	ASSERTEXPR(IsValid());
	if(!IsValid())
		return((int)-1);

	nValue = (int)-1;

	if(nFieldNum >= 0)
	{
		// da numero a int
		NUMBER Ni = {0};
		GetNumericField(nFieldNum,Ni,integer_type);
		nValue = Ni.integer;
	}
    else
		m_nLastError = m_pBerkeleyDB->SetLastError(DB_RETCODE_EINVALIDFIELDNUMBER);

	return(nValue);
}

/*
	GetField()

	Ricava, come int, il contenuto del campo interno (relativo al record corrente della tabella).
	Specificare il nome del campo e non il progressivo numerico.

	NON accede in modo esclusivo.
*/
int CBase::GetField(const char* pFieldName,int& nValue)
{
	// controlla che il costruttore abbia creato l'oggetto per la tabella
	ASSERTEXPR(IsValid());
	if(!IsValid())
		return((int)-1);

	return(GetField(GetFieldNumberByName(pFieldName),nValue));
}

/*
	GetField()

	Ricava, come long, il contenuto del campo interno (relativo al record corrente della tabella).
	Specificare il progressivo numerico e non il nome del campo.

	NON accede in modo esclusivo.
*/
long CBase::GetField(int nFieldNum,long& nValue)
{
	// controlla che il costruttore abbia creato l'oggetto per la tabella
	ASSERTEXPR(IsValid());
	if(!IsValid())
		return((long)-1);

	nValue = (long)-1L;

	if(nFieldNum >= 0)
	{
		// da numero a long
		NUMBER Nl = {0};
		GetNumericField(nFieldNum,Nl,longint_type);
		nValue = Nl.longint;
	}
    else
		m_nLastError = m_pBerkeleyDB->SetLastError(DB_RETCODE_EINVALIDFIELDNUMBER);

	return(nValue);
}

/*
	GetField()

	Ricava, come long, il contenuto del campo interno (relativo al record corrente della tabella).
	Specificare il nome del campo e non il progressivo numerico.

	NON accede in modo esclusivo.
*/
long CBase::GetField(const char* pFieldName,long& nValue)
{
	// controlla che il costruttore abbia creato l'oggetto per la tabella
	ASSERTEXPR(IsValid());
	if(!IsValid())
		return((long)-1);

	return(GetField(GetFieldNumberByName(pFieldName),nValue));
}

/*
	GetField()

	Ricava, come unsigned long, il contenuto del campo interno (relativo al record corrente della tabella).
	Specificare il progressivo numerico e non il nome del campo.

	NON accede in modo esclusivo.
*/
unsigned long CBase::GetField(int nFieldNum,unsigned long& nValue)
{
	// controlla che il costruttore abbia creato l'oggetto per la tabella
	ASSERTEXPR(IsValid());
	if(!IsValid())
		return((unsigned long)-1);

	nValue = (unsigned long)-1L;

	if(nFieldNum >= 0)
	{
		// da numero a unsigned long
		NUMBER Nu = {0};
		GetNumericField(nFieldNum,Nu,ulongint_type);
		nValue = Nu.ulongint;
	}
    else
		m_nLastError = m_pBerkeleyDB->SetLastError(DB_RETCODE_EINVALIDFIELDNUMBER);

	return(nValue);
}

/*
	GetField()

	Ricava, come unsigned long, il contenuto del campo interno (relativo al record corrente della tabella).
	Specificare il nome del campo e non il progressivo numerico.

	NON accede in modo esclusivo.
*/
unsigned long CBase::GetField(const char* pFieldName,unsigned long& nValue)
{
	// controlla che il costruttore abbia creato l'oggetto per la tabella
	ASSERTEXPR(IsValid());
	if(!IsValid())
		return((unsigned long)-1);

	return(GetField(GetFieldNumberByName(pFieldName),nValue));
}

/*
	GetField()

	Ricava, come double, il contenuto del campo interno (relativo al record corrente della tabella).
	Specificare il progressivo numerico e non il nome del campo.

	NON accede in modo esclusivo.
*/
double CBase::GetField(int nFieldNum,double& nValue)
{
	// controlla che il costruttore abbia creato l'oggetto per la tabella
	ASSERTEXPR(IsValid());
	if(!IsValid())
		return((double)-1.0);

	nValue = (double)-1.0;

	if(nFieldNum >= 0)
	{
		// da numero a double
		NUMBER Nr = {0};
		GetNumericField(nFieldNum,Nr,realnum_type);
		nValue = Nr.realnum;
	}
    else
		m_nLastError = m_pBerkeleyDB->SetLastError(DB_RETCODE_EINVALIDFIELDNUMBER);

	return(nValue);
}

/*
	GetField()

	Ricava, come double, il contenuto del campo interno (relativo al record corrente della tabella).
	Specificare il nome del campo e non il progressivo numerico.

	NON accede in modo esclusivo.
*/
double CBase::GetField(const char* pFieldName,double& nValue)
{
	// controlla che il costruttore abbia creato l'oggetto per la tabella
	ASSERTEXPR(IsValid());
	if(!IsValid())
		return((double)-1.0);

	return(GetField(GetFieldNumberByName(pFieldName),nValue));
}

/*
	GetNumericField()

	Utilizzata internamente per convertire il valore del campo (stringa) in numerico.
	Specificare il progressivo numerico e non il nome del campo.

	NON accede in modo esclusivo.
*/
void CBase::GetNumericField(int nFieldNum,NUMBER& value,NUMTYPE type)
{
	// controlla che il costruttore abbia creato l'oggetto per la tabella
	ASSERTEXPR(IsValid());
	if(!IsValid())
		return;

	if(nFieldNum >= 0)
	{
		char szField[MAX_FIELDSIZE+1] = {0};

		// se la tabella prevede indici salta il primo campo (relativo alla chiave primaria)
		nFieldNum = m_pDatabase->table.totindex > 0 ? nFieldNum + 1 : nFieldNum;

		if(nFieldNum >= 0 && nFieldNum < m_pDatabase->table.totfield)
		{
			m_pBerkeleyDB->GetField(nFieldNum,szField,sizeof(szField));

			switch(type)
			{
				// short int
				case shortint_type:
					value.shortint = (short int)atoi(szField);
					break;

				// int
				case integer_type:
					value.integer = (int)atoi(szField);
					break;

				// long
				case longint_type:
					value.longint = (long)atol(szField);
					break;
				
				// unsigned long
				case ulongint_type:
					value.ulongint = strtoul(szField,NULL,0);
					break;

				// double
				case realnum_type:
					value.realnum = (double)atof(szField);
					break;
			}
		}
	}
    else
		m_nLastError = m_pBerkeleyDB->SetLastError(DB_RETCODE_EINVALIDFIELDNUMBER);
}

/*
	PutField()

	Imposta il campo interno (relativo al record corrente della tabella) con la stringa passata in input.
	La chiamata modifica il campo interno, NON il campo della tabella, da aggiornare con i metodi
	relativi (insert, update, etc.).
	Specificare il progressivo numerico e non il nome del campo.

	NON accede in modo esclusivo.
*/
void CBase::PutField(int nFieldNum,const char* pValue)
{
	// controlla che il costruttore abbia creato l'oggetto per la tabella
	ASSERTEXPR(IsValid());
	if(!IsValid())
		return;

	if(nFieldNum >= 0)
	{
		// se la tabella prevede indici salta il primo campo (relativo alla chiave primaria)
		nFieldNum = m_pDatabase->table.totindex > 0 ? nFieldNum + 1 : nFieldNum;

		if(nFieldNum >= 0 && nFieldNum < m_pDatabase->table.totfield)
		{
			char szField[MAX_KEYSIZE+1] = {0};
			strcpyn(szField,pValue,sizeof(szField));

#ifdef _USE_FIELD_PICTURES
			// formatta il contenuto del buffer prima di passarlo nel campo
			if(m_pDatabase->table.row[nFieldNum].flags!=CBASE_FLAG_NONE)
				SetFieldFormat(szField,strlen(szField),m_pDatabase->table.row[nFieldNum].flags);
#endif

			// passa il contenuto del buffer nel campo interno (relativo al campo della tabella)
			m_pBerkeleyDB->PutField(nFieldNum,szField);
		}
	}
    else
		m_nLastError = m_pBerkeleyDB->SetLastError(DB_RETCODE_EINVALIDFIELDNUMBER);
}

/*
	PutField()

	Imposta il campo interno (relativo al record corrente della tabella) con la stringa passata in input.
	La chiamata modifica il campo interno, NON il campo della tabella, da aggiornare con i metodi
	relativi (insert, update, etc.).
	Specificare il nome del campo e non il progressivo numerico.

	NON accede in modo esclusivo.
*/
void CBase::PutField(const char* pFieldName,const char* pValue)
{
	// controlla che il costruttore abbia creato l'oggetto per la tabella
	ASSERTEXPR(IsValid());
	if(!IsValid())
		return;

	PutField(GetFieldNumberByName(pFieldName),pValue);
}

/*
	PutField()

	Imposta il campo interno (relativo al record corrente della tabella) con il boolean passato in input.
	La chiamata modifica il campo interno, NON il campo della tabella, da aggiornare con i metodi
	relativi (insert, update, etc.).
	Specificare il progressivo numerico e non il nome del campo.

	NON accede in modo esclusivo.
*/
void CBase::PutField(int nFieldNum,bool bValue)
{
	// controlla che il costruttore abbia creato l'oggetto per la tabella
	ASSERTEXPR(IsValid());
	if(!IsValid())
		return;

	if(nFieldNum >= 0)
	{
		// da boolean a carattere
		char szValue[3] = {0};
		FormatField(szValue,sizeof(szValue),bValue);
		PutField(nFieldNum,szValue);
	}
    else
		m_nLastError = m_pBerkeleyDB->SetLastError(DB_RETCODE_EINVALIDFIELDNUMBER);
}

/*
	PutField()

	Imposta il campo interno (relativo al record corrente della tabella) con il boolean passato in input.
	La chiamata modifica il campo interno, NON il campo della tabella, da aggiornare con i metodi
	relativi (insert, update, etc.).
	Specificare il nome del campo e non il progressivo numerico.

	NON accede in modo esclusivo.
*/
void CBase::PutField(const char* pFieldName,bool bValue)
{
	// controlla che il costruttore abbia creato l'oggetto per la tabella
	ASSERTEXPR(IsValid());
	if(!IsValid())
		return;

	PutField(GetFieldNumberByName(pFieldName),bValue);
}

/*
	PutField()

	Imposta il campo interno (relativo al record corrente della tabella) con la data passata in input.
	La chiamata modifica il campo interno, NON il campo della tabella, da aggiornare con i metodi
	relativi (insert, update, etc.).
	Specificare il progressivo numerico e non il nome del campo.

	NON accede in modo esclusivo.
*/
void CBase::PutField(int nFieldNum,CDateTime& pDateTime)
{
	// controlla che il costruttore abbia creato l'oggetto per la tabella
	ASSERTEXPR(IsValid());
	if(!IsValid())
		return;

	if(nFieldNum >= 0)
	{
		char szValue[32] = {0};
		
		// ricava il tipo del campo (T,D,G)
		char cType = 0;

		// se la tabella prevede indici salta il primo campo (relativo alla chiave primaria)
		int n = m_pDatabase->table.totindex > 0 ? nFieldNum + 1 : nFieldNum;
		for(int i = 0; i < m_pDatabase->table.totfield; i++)
			if(i==n)
			{
				cType = m_pDatabase->table.row[i].type;
				break;
			}

		// formatta da data a char
		FormatField(szValue,sizeof(szValue),pDateTime,cType);

		PutField(nFieldNum,szValue);
	}
    else
		m_nLastError = m_pBerkeleyDB->SetLastError(DB_RETCODE_EINVALIDFIELDNUMBER);
}

/*
	PutField()

	Imposta il campo interno (relativo al record corrente della tabella) con la data passata in input.
	La chiamata modifica il campo interno, NON il campo della tabella, da aggiornare con i metodi
	relativi (insert, update, etc.).
	Specificare il nome del campo e non il progressivo numerico.

	NON accede in modo esclusivo.
*/
void CBase::PutField(const char* pFieldName,CDateTime& pDateTime)
{
	// controlla che il costruttore abbia creato l'oggetto per la tabella
	ASSERTEXPR(IsValid());
	if(!IsValid())
		return;

	PutField(GetFieldNumberByName(pFieldName),pDateTime);
}

/*
	PutField()

	Imposta il campo interno (relativo al record corrente della tabella) con lo short int passato in input.
	La chiamata modifica il campo interno, NON il campo della tabella, da aggiornare con i metodi
	relativi (insert, update, etc.).
	Specificare il progressivo numerico e non il nome del campo.

	NON accede in modo esclusivo.
*/
void CBase::PutField(int nFieldNum,short int nValue)
{
	// controlla che il costruttore abbia creato l'oggetto per la tabella
	ASSERTEXPR(IsValid());
	if(!IsValid())
		return;

	if(nFieldNum >= 0)
	{
		// da numero a short int
		NUMBER Ns = {0};
		Ns.shortint = nValue;		
		PutNumericField(nFieldNum,Ns,shortint_type);
	}
    else
		m_nLastError = m_pBerkeleyDB->SetLastError(DB_RETCODE_EINVALIDFIELDNUMBER);
}

/*
	PutField()

	Imposta il campo interno (relativo al record corrente della tabella) con lo short int passato in input.
	La chiamata modifica il campo interno, NON il campo della tabella, da aggiornare con i metodi
	relativi (insert, update, etc.).
	Specificare il nome del campo e non il progressivo numerico.

	NON accede in modo esclusivo.
*/
void CBase::PutField(const char* pFieldName,short int nValue)
{
	// controlla che il costruttore abbia creato l'oggetto per la tabella
	ASSERTEXPR(IsValid());
	if(!IsValid())
		return;

	PutField(GetFieldNumberByName(pFieldName),(short int)nValue);
}

/*
	PutField()

	Imposta il campo interno (relativo al record corrente della tabella) con l'int passato in input.
	La chiamata modifica il campo interno, NON il campo della tabella, da aggiornare con i metodi
	relativi (insert, update, etc.).
	Specificare il progressivo numerico e non il nome del campo.

	NON accede in modo esclusivo.
*/
void CBase::PutField(int nFieldNum,int nValue)
{
	// controlla che il costruttore abbia creato l'oggetto per la tabella
	ASSERTEXPR(IsValid());
	if(!IsValid())
		return;

	if(nFieldNum >= 0)
	{
		// da numero a int
		NUMBER Ni = {0};
		Ni.integer = nValue;		
		PutNumericField(nFieldNum,Ni,integer_type);
	}
    else
		m_nLastError = m_pBerkeleyDB->SetLastError(DB_RETCODE_EINVALIDFIELDNUMBER);
}

/*
	PutField()

	Imposta il campo interno (relativo al record corrente della tabella) con l'int passato in input.
	La chiamata modifica il campo interno, NON il campo della tabella, da aggiornare con i metodi
	relativi (insert, update, etc.).
	Specificare il nome del campo e non il progressivo numerico.

	NON accede in modo esclusivo.
*/
void CBase::PutField(const char* pFieldName,int nValue)
{
	// controlla che il costruttore abbia creato l'oggetto per la tabella
	ASSERTEXPR(IsValid());
	if(!IsValid())
		return;

	PutField(GetFieldNumberByName(pFieldName),(int)nValue);
}

/*
	PutField()

	Imposta il campo interno (relativo al record corrente della tabella) con il long passato in input.
	La chiamata modifica il campo interno, NON il campo della tabella, da aggiornare con i metodi
	relativi (insert, update, etc.).
	Specificare il progressivo numerico e non il nome del campo.

	NON accede in modo esclusivo.
*/
void CBase::PutField(int nFieldNum,long nValue)
{
	// controlla che il costruttore abbia creato l'oggetto per la tabella
	ASSERTEXPR(IsValid());
	if(!IsValid())
		return;

	if(nFieldNum >= 0)
	{
		// da numero a long
		NUMBER Nl = {0};
		Nl.longint = nValue;
		PutNumericField(nFieldNum,Nl,longint_type);
	}
    else
		m_nLastError = m_pBerkeleyDB->SetLastError(DB_RETCODE_EINVALIDFIELDNUMBER);
}

/*
	PutField()

	Imposta il campo interno (relativo al record corrente della tabella) con il long passato in input.
	La chiamata modifica il campo interno, NON il campo della tabella, da aggiornare con i metodi
	relativi (insert, update, etc.).
	Specificare il nome del campo e non il progressivo numerico.

	NON accede in modo esclusivo.
*/
void CBase::PutField(const char* pFieldName,long nValue)
{
	// controlla che il costruttore abbia creato l'oggetto per la tabella
	ASSERTEXPR(IsValid());
	if(!IsValid())
		return;

	PutField(GetFieldNumberByName(pFieldName),(long)nValue);
}

/*
	PutField()

	Imposta il campo interno (relativo al record corrente della tabella) con l'unsigned long passato in input.
	La chiamata modifica il campo interno, NON il campo della tabella, da aggiornare con i metodi
	relativi (insert, update, etc.).
	Specificare il progressivo numerico e non il nome del campo.

	NON accede in modo esclusivo.
*/
void CBase::PutField(int nFieldNum,unsigned long nValue)
{
	// controlla che il costruttore abbia creato l'oggetto per la tabella
	ASSERTEXPR(IsValid());
	if(!IsValid())
		return;

	if(nFieldNum >= 0)
	{
		// da numero a unsigned long
		NUMBER Nu = {0};
		Nu.ulongint = nValue;
		PutNumericField(nFieldNum,Nu,ulongint_type);
	}
    else
		m_nLastError = m_pBerkeleyDB->SetLastError(DB_RETCODE_EINVALIDFIELDNUMBER);
}

/*
	PutField()

	Imposta il campo interno (relativo al record corrente della tabella) con l'unsigned long passato in input.
	La chiamata modifica il campo interno, NON il campo della tabella, da aggiornare con i metodi
	relativi (insert, update, etc.).
	Specificare il nome del campo e non il progressivo numerico.

	NON accede in modo esclusivo.
*/
void CBase::PutField(const char* pFieldName,unsigned long nValue)
{
	// controlla che il costruttore abbia creato l'oggetto per la tabella
	ASSERTEXPR(IsValid());
	if(!IsValid())
		return;

	PutField(GetFieldNumberByName(pFieldName),(unsigned long)nValue);
}

/*
	PutField()

	Imposta il campo interno (relativo al record corrente della tabella) con il double passato in input.
	La chiamata modifica il campo interno, NON il campo della tabella, da aggiornare con i metodi
	relativi (insert, update, etc.).
	Specificare il progressivo numerico e non il nome del campo.

	NON accede in modo esclusivo.
*/
void CBase::PutField(int nFieldNum,double nValue)
{
	// controlla che il costruttore abbia creato l'oggetto per la tabella
	ASSERTEXPR(IsValid());
	if(!IsValid())
		return;

	if(nFieldNum >= 0)
	{
		// da numero a double
		NUMBER Nr = {0};
		Nr.realnum = nValue;
		PutNumericField(nFieldNum,Nr,realnum_type);
	}
    else
		m_nLastError = m_pBerkeleyDB->SetLastError(DB_RETCODE_EINVALIDFIELDNUMBER);
}

/*
	PutField()

	Imposta il campo interno (relativo al record corrente della tabella) con il double passato in input.
	La chiamata modifica il campo interno, NON il campo della tabella, da aggiornare con i metodi
	relativi (insert, update, etc.).
	Specificare il nome del campo e non il progressivo numerico.

	NON accede in modo esclusivo.
*/
void CBase::PutField(const char* pFieldName,double nValue)
{
	// controlla che il costruttore abbia creato l'oggetto per la tabella
	ASSERTEXPR(IsValid());
	if(!IsValid())
		return;

	PutField(GetFieldNumberByName(pFieldName),(double)nValue);
}

/*
	PutNumericField()

	Utilizzata internamente per convertire il valore del campo (numerico) in stringa.
	Il valore impostato e' quello relativo al record corrente (imposta con quanto ricevuto in input).
	Specificare il progressivo numerico e non il nome del campo.

	NON accede in modo esclusivo.
*/
void CBase::PutNumericField(int nFieldNum,NUMBER value,NUMTYPE type)
{
	// controlla che il costruttore abbia creato l'oggetto per la tabella
	ASSERTEXPR(IsValid());
	if(!IsValid())
		return;

	if(nFieldNum >= 0)
	{
		char szField[MAX_FIELDSIZE+1] = {0};

		// formatta da numero a char
		FormatField(szField,sizeof(szField),value,type,nFieldNum);
		
		PutField(nFieldNum,(const char*)szField);
	}
    else
		m_nLastError = m_pBerkeleyDB->SetLastError(DB_RETCODE_EINVALIDFIELDNUMBER);
}

/*
	FormatField()

	Formatta il valore (boolean, data, numerico) nel buffer.

	NON accede in modo esclusivo.
*/
char* CBase::FormatField(char* pBuffer,int nSize,bool bValue)
{
	// controlla che il costruttore abbia creato l'oggetto per la tabella
	ASSERTEXPR(IsValid());
	if(!IsValid())
		return(pBuffer);

	snprintf(pBuffer,nSize,"%c",bValue ? 'T' : 'F');
	return(pBuffer);
}
char* CBase::FormatField(char* pBuffer,int nSize,CDateTime& pDateTime,char cType)
{
	// controlla che il costruttore abbia creato l'oggetto per la tabella
	ASSERTEXPR(IsValid());
	if(!IsValid())
		return(pBuffer);

	switch(cType)
	{
		// ora (hhmmss)
		case 'T':
			snprintf(pBuffer,nSize,"%.2d%.2d%.2d",pDateTime.GetHour(),pDateTime.GetMin(),pDateTime.GetSec());
			break;
		
		// data (~/+yyyymmdd)
		case 'D':
			snprintf(pBuffer,nSize,"%c%.4d%.2d%.2d",pDateTime.GetBC() ? '~' : '+',pDateTime.GetYear(),pDateTime.GetMonth(),pDateTime.GetDay());		
			break;
	}

	return(pBuffer);
}
char* CBase::FormatField(char* pBuffer,int nSize,NUMBER value,NUMTYPE type,int nFieldNum)
{
	// controlla che il costruttore abbia creato l'oggetto per la tabella
	ASSERTEXPR(IsValid());
	if(!IsValid())
		return(pBuffer);

	char cFieldType;
	int  nFieldNumber,nFieldLen,nFieldSize,nFieldDec;
	char szBuffer[MAX_FIELDSIZE+1] = {0};
	char szFormat[10] = {0};

	nFieldNumber = nFieldNum;

	// se la tabella prevede indici salta il primo campo (relativo alla chiave primaria)
	nFieldNum = m_pDatabase->table.totindex > 0 ? nFieldNum + 1 : nFieldNum;

	if(nFieldNum >= 0 && nFieldNum < m_pDatabase->table.totfield)
	{
		memset(pBuffer,' ',nSize);

		// tipo, dimensione e decimali del campo
		cFieldType = m_pDatabase->table.row[nFieldNum].type;
		nFieldSize = m_pDatabase->table.row[nFieldNum].size;
		nFieldDec  = m_pDatabase->table.row[nFieldNum].dec;
		nFieldDec  = nFieldDec > 6 ? 6 : nFieldDec;

		switch(type)
		{
			// short int
			case shortint_type:
				snprintf(szBuffer,sizeof(szBuffer),"%d",value.shortint);
				break;
			
			// int
			case integer_type:
				snprintf(szBuffer,sizeof(szBuffer),"%d",value.integer);
				break;
			
			// long
			case longint_type:
				snprintf(szBuffer,sizeof(szBuffer),"%ld",value.longint);
				break;
			
			// unsigned long
			case ulongint_type:
				snprintf(szBuffer,sizeof(szBuffer),"%lu",value.ulongint);
				break;
			
			// double
			case realnum_type:
				snprintf(szFormat,sizeof(szFormat),"%c.%d%c",'%',nFieldDec,'f');
				snprintf(szBuffer,sizeof(szBuffer),szFormat,value.realnum);
				break;
		}

		nFieldLen = strlen(szBuffer);
		nFieldLen = nFieldLen > nFieldSize ? nFieldSize : nFieldLen;
		
		// allineamento (stringhe a sx, numeri a dx)
		switch(cFieldType)
		{
			case 'S':
			case 'I':
			case 'N':
			case 'U':
			case 'R':
				memcpy(pBuffer + (nFieldSize-nFieldLen),szBuffer,nFieldLen);
				break;
			default:
				memcpy(pBuffer,szBuffer,nFieldLen);
				break;
		}

		pBuffer[nFieldSize] = '\0';
	}
    else
		m_nLastError = m_pBerkeleyDB->SetLastError(DB_RETCODE_EINVALIDFIELDNUMBER);

	return(pBuffer);
}

/*
1. Inserimento di una Data (A.C. / B.C.)

	Caso A: Inserimento di una data A.C. (2025/10/13)
	La data e' gia' nel corretto ordine lessicografico.
	Formato Standard: 20251013
	Antepone il segno A.C. (+).
	Storage nel DB: +20251013

	Caso B: Inserimento di una data B.C. (100 a.C. / 01/01)
	Questa data deve essere invertita.
	Passaggio 1: Formato Standard
	La data e' 100 a.C., 1 Gennaio (formato: YYYYMMDD).
	01000101
	Passaggio 2: Applicazione del Complemento a Nove
	Si sottrae ogni singola cifra da 9.
	Cifra Originale	0	1	0	0	0	1	0	1
	Risultato (9-X)	9	8	9	9	9	8	9	8
	Risultato del Complemento: 98999898
	Passaggio 3: Preparazione Storage
	Antepone il segno B.C. (~).
	Storage nel DB: ~98999898

2. Recupero di una Data (A.C. / B.C.)

	Caso A: Recupero di una data A.C.
	Valore dal DB: +20251013
	Check Segno: Il primo carattere e' +. ? E' A.C.
	Output: Prendere i caratteri da 1 a 8 (20251013) e formattare nel formato richiesto dal chiamante (es. 13/10/2025).

	Caso B: Recupero di una data B.C.
	Valore dal DB: ~98999898
	Check Segno: Il primo carattere e' ~. ? E' B.C., bisogna invertire.
	Applicazione del Complemento a Nove (Inversione)
	Sottrae di nuovo ogni cifra da 9 (l'operazione e' reversibile):
	Cifra memorizzata	9	8	9	9	9	8	9	8
	Risultato (9-X)	0	1	0	0	0	1	0	1
	Risultato Decodificato: 01000101
	Output: Prendere il risultato (01000101), formattarlo (01/01/0100), ed aggiungere l'indicatore "B.C." o "a.C.".

Come si ordinano le stringhe memorizzate nel B-Tree:
	Data Cronologica	Valore Memorizzato	Ordine Lessicografico
	100 a.C.			~98999898			1° (Piu' piccolo)
	1 a.C.				~99989898			2° (Medio)
	1 d.C.				+00010101			3° (Medio)
	100 d.C.			+01000101			4° (Piu' grande)
Risultato: l'ordinamento del B-Tree e' esattamente l'ordinamento cronologico grazie al complemento a nove.
(il complemento a nove di una cifra e' semplicemente 9 - la cifra)
*/

/*
	DateFormatForTable()

	Riformatta la data fornita dall'applicativo (che deve essere specificata in uno dei formati di 
	cui sotto, definiti in CDateTime.h), nel formato richiesto per la tabella ('A/B-C + YYYYMMDD').
	Per il parametro AC/BC assume FALSE se non specificato diversamente.

	La classe dell'ultimo livello dell'interfaccia, (la classe B), quella che definisce le operazioni
	logiche sulla tabella), deve chiamare la funzione per convertire la data dell'applicazione
	(parametro 1) nel formato per la tabella (parametri 2 e 3).

	Alla PutField() andra' poi passato il risultato della conversione per l'inserimento nella tabella.

	Gli unici formati validi per la data dell'applicativo (vedi definizione in CDateTime.h) sono:

	ANSI_SHORT,			// "yyyymmdd"

	ANSI,				// "yyyy.mm.dd"
	JAPAN,				// "yyyy/mm/dd"
	YMD,				// "yyyy/mm/dd"
	YMD_ISO8601,		// "yyyy-mm-dd"

	BRITISH,			// "dd/mm/yyyy"
	FRENCH,				// "dd/mm/yyyy"
	GERMAN,				// "dd.mm.yyyy"
	ITALIAN,			// "dd-mm-yyyy"
	DMY,				// "dd/mm/yyyy"

	AMERICAN = 0,		// "mm/dd/yyyy"
	USA,				// "mm-dd-yyyy"
	MDY,				// "mm/dd/yyyy"

	GMT_SHORT,			// "Day, dd Mon yyyy hh:mm:ss"				(assumendo GMT, ossia convertendo l'UTC in GMT)
	GMT,				// "Day, dd Mon yyyy hh:mm:ss <-|+>nnnn"	(con l'UTC, ossia il <-|+>nnnn, locale)
	GMT_TZ,				// "Day, dd Mon yyyy hh:mm:ss <-|+>nnnn TZ"	(con l'UTC, ossia il <-|+>nnnn, locale, dove TZ e' l'identificativo di tre caratteri per l'UTC)
*/
bool CBase::DateFormatForTable(const char* pInputDate,char* pOutputDate,size_t nDateSize,const DATEFORMAT dateFormat,bool bBC/* = FALSE*/)
{
	CDateTime dateTime;

	// il sizeof di pOutputDate, deve essere maggiore di FIELD_DATE_MINSIZE
	if(nDateSize <= FIELD_DATE_MINSIZE)
		return(FALSE);

	switch(dateFormat)
	{
		case ANSI_SHORT:	// "yyyymmdd"

		case ANSI:			// "yyyy.mm.dd"
		case JAPAN:			// "yyyy/mm/dd"
		case YMD:			// "yyyy/mm/dd"
		case YMD_ISO8601:	// "yyyy-mm-dd"

		case BRITISH:		// "dd/mm/yyyy"
		case FRENCH:		// "dd/mm/yyyy"
		case GERMAN:		// "dd.mm.yyyy"
		case ITALIAN:		// "dd-mm-yyyy"
		case DMY:			// "dd/mm/yyyy"

		case AMERICAN:		// "mm/dd/yyyy"
		case USA:			// "mm-dd-yyyy"
		case MDY:			// "mm/dd/yyyy"

		case GMT_SHORT:		// "Day, dd Mon yyyy hh:mm:ss"				(assumendo GMT, ossia convertendo l'UTC in GMT)
		case GMT:			// "Day, dd Mon yyyy hh:mm:ss <-|+>nnnn"	(con l'UTC, ossia il <-|+>nnnn, locale)
		case GMT_TZ:		// "Day, dd Mon yyyy hh:mm:ss <-|+>nnnn TZ"	(con l'UTC, ossia il <-|+>nnnn, locale, dove TZ e' l'identificativo di tre caratteri per l'UTC)
			dateTime.LoadFromString(pInputDate,dateFormat);
			break;

		default:
			return(FALSE);
	}

	if(bBC)
	{
		char szDate[16] = {0};
		snprintf(szDate,sizeof(szDate),"%4d%2d%2d",dateTime.GetYear(),dateTime.GetMonth(),dateTime.GetDay());
		DateComplement(szDate,8);
		snprintf(pOutputDate,nDateSize,"%c%s",'~',szDate);
	}
	else
		snprintf(pOutputDate,nDateSize,"%c%04d%02d%02d",'+',dateTime.GetYear(),dateTime.GetMonth(),dateTime.GetDay());

	return(TRUE);
}

/*
	DateFormatFromTable()

	Riformatta la data presente della tabella (parametro 1) nel buffer di output (parametri 2 e 3), secondo il formato richiesto 
	dall'applicativo (parametro 4) ed impostando il flag per AC/BC.
	
	La data della tabella e' quella ricavata con GetField().

	Vedi le note per DateFormatForTable().
*/
bool CBase::DateFormatFromTable(const char* pInputDate,char* pOutputDate,size_t nDateSize,const DATEFORMAT dateFormat,bool& bBC) const
{
	// il sizeof di pOutputDate, deve essere maggiore di FIELD_DATE_MINSIZE
	if(nDateSize <= FIELD_DATE_MINSIZE)
		return(FALSE);

	switch(dateFormat)
	{
		case ANSI_SHORT:	// "yyyymmdd"

		case ANSI:			// "yyyy.mm.dd"
		case JAPAN:			// "yyyy/mm/dd"
		case YMD:			// "yyyy/mm/dd"
		case YMD_ISO8601:	// "yyyy-mm-dd"

		case BRITISH:		// "dd/mm/yyyy"
		case FRENCH:		// "dd/mm/yyyy"
		case GERMAN:		// "dd.mm.yyyy"
		case ITALIAN:		// "dd-mm-yyyy"
		case DMY:			// "dd/mm/yyyy"

		case AMERICAN:		// "mm/dd/yyyy"
		case USA:			// "mm-dd-yyyy"
		case MDY:			// "mm/dd/yyyy"

		case GMT_SHORT:		// "Day, dd Mon yyyy hh:mm:ss"				(assumendo GMT, ossia convertendo l'UTC in GMT)
		case GMT:			// "Day, dd Mon yyyy hh:mm:ss <-|+>nnnn"	(con l'UTC, ossia il <-|+>nnnn, locale)
		case GMT_TZ:		// "Day, dd Mon yyyy hh:mm:ss <-|+>nnnn TZ"	(con l'UTC, ossia il <-|+>nnnn, locale, dove TZ e' l'identificativo di tre caratteri per l'UTC)
		{
			if(pInputDate[0]=='~')
			{
				char szDate[16] = {0};
				strcpyn(szDate,pInputDate+1,sizeof(szDate));
				DateComplement(szDate,8);
				CDateTime dateTime(szDate,ANSI_SHORT);
				strcpyn(pOutputDate,dateTime.ConvertDate(ANSI_SHORT,dateFormat,szDate,"00:00:00"),nDateSize);
				bBC = TRUE;
			}
			else
			{
				CDateTime dateTime(pInputDate+1,ANSI_SHORT);
				strcpyn(pOutputDate,dateTime.ConvertDate(ANSI_SHORT,dateFormat,pInputDate+1,"00:00:00"),nDateSize);
				bBC = FALSE;
			}
			break;
		}

		default:
			return(FALSE);
	}

	return(TRUE);
}

/*
	DateComplement()

	Effettua il complemento a 9 sfruttando l'aritmetica ASCII in tre passaggi:

	1. szDateYYYYMMDD[i] - '0': Converte il carattere cifra nel suo valore numerico intero (es. se szDateYYYYMMDD[i] e' '3', il risultato e' 3).
	2. '9' - (risultato del punto 1): Calcola il complemento (es. '9' - 3 = il valore ASCII del carattere '6'). La sottrazione tra un carattere (il cui valore ASCII e' noto) e un intero da' come risultato un intero, che in questo caso e' esattamente il codice ASCII della cifra complementata.
	3. (char): Esegue il cast per riportare l'intero risultante al tipo char per l'assegnazione.
*/
void CBase::DateComplement(char* szDateYYYYMMDD,size_t size) const
{
	// applica il complemento a 9 direttamente (9 - valore numerico) e lo riconverte in char
	for(size_t i = 0; i < size; i++)
		szDateYYYYMMDD[i] = (char)('9' - (szDateYYYYMMDD[i] - '0'));
}

/*
	SetNumberPicture()

	Imposta la picture numerica.
*/
#ifdef _USE_FIELD_PICTURES
bool CBase::SetNumberPicture(const char* pPicture)
{
	bool bPict = FALSE;

	if(IsValid())
		if(m_pPictureNumber)
		{
			memset(m_pPictureNumber,'\0',CBASE_MAX_NUMBER_PICTURE + 1);
			strcpyn(m_pPictureNumber,pPicture,CBASE_MAX_NUMBER_PICTURE + 1);
			bPict = TRUE;
		}

	return(bPict);
}
#endif

/*
	SetCharPicture()

	Imposta la picture carattere.
*/
#ifdef _USE_FIELD_PICTURES
bool CBase::SetCharPicture(const char* pPicture)
{
	bool bPict = FALSE;

	if(IsValid())
		if(m_pPictureChar)
		{
			memset(m_pPictureChar,'\0',CBASE_MAX_CHAR_PICTURE + 1);
			strcpyn(m_pPictureChar,pPicture,CBASE_MAX_CHAR_PICTURE + 1);
			bPict = TRUE;
		}

	return(bPict);
}
#endif

/*
	SetPunctPicture()

	Imposta la picture per la punteggiatura.
*/
#ifdef _USE_FIELD_PICTURES
bool CBase::SetPunctPicture(const char* pPicture)
{
	bool bPict = FALSE;

	if(IsValid())
		if(m_pPicturePunct)
		{
			memset(m_pPicturePunct,'\0',CBASE_MAX_PUNCT_PICTURE + 1);
			strcpyn(m_pPicturePunct,pPicture,CBASE_MAX_PUNCT_PICTURE + 1);
			bPict = TRUE;
		}

	return(bPict);
}
#endif

/*
	SetUserPicture()

	Imposta la picture definita dall'utente.
*/
#ifdef _USE_FIELD_PICTURES
bool CBase::SetUserPicture(const char* pPicture)
{
	bool bPict = FALSE;

	if(IsValid())
		if(m_pPictureUserDefined)
		{
			memset(m_pPictureUserDefined,'\0',CBASE_MAX_USER_PICTURE + 1);
			strcpyn(m_pPictureUserDefined,pPicture,CBASE_MAX_USER_PICTURE + 1);
			bPict = TRUE;
		}

	return(bPict);
}
#endif

/*
	SetFieldFlags()

	Imposta i flags per il campo.
*/
#ifdef _USE_FIELD_PICTURES
void CBase::SetFieldFlags(int nFieldNum,unsigned long nFlags)
{
	if(IsValid())
	{
		// se la tabella prevede indici salta il primo campo (relativo alla chiave primaria)
		nFieldNum = m_pDatabase->table.totindex > 0 ? nFieldNum + 1 : nFieldNum;	
		if(nFieldNum >= 0 && nFieldNum < m_pDatabase->table.totfield)
			m_pDatabase->table.row[nFieldNum].flags = nFlags;
	}
}
#endif

/*
	SetDefaultNumberPicture()

	Imposta la picture numerica di default.
*/
#ifdef _USE_FIELD_PICTURES
void CBase::SetDefaultNumberPicture(void)
{
	if(!m_pPictureNumber)
		m_pPictureNumber = new char[CBASE_MAX_NUMBER_PICTURE + 1];
	
	if(m_pPictureNumber)
	{
		memset(m_pPictureNumber,'\0',CBASE_MAX_NUMBER_PICTURE + 1);
		//strcpyn(m_pPictureNumber,"0123456789",CBASE_MAX_NUMBER_PICTURE + 1);
	}
}
#endif

/*
	SetDefaultCharPicture()

	Imposta la picture carattere di default.
*/
#ifdef _USE_FIELD_PICTURES
void CBase::SetDefaultCharPicture(void)
{
	if(!m_pPictureChar)
		m_pPictureChar = new char[CBASE_MAX_CHAR_PICTURE + 1];
	
	if(m_pPictureChar)
	{
		memset(m_pPictureChar,'\0',CBASE_MAX_CHAR_PICTURE + 1);
		//strcpyn(m_pPictureChar," abcdefghilmnopqrstuvzABCDEFGHILMNOPQRSTUVZ",CBASE_MAX_CHAR_PICTURE + 1);
	}
}
#endif

/*
	SetDefaultPunctPicture()

	Imposta la picture per la punteggiatura di default.
*/
#ifdef _USE_FIELD_PICTURES
void CBase::SetDefaultPunctPicture(void)
{
	if(!m_pPicturePunct)
		m_pPicturePunct = new char[CBASE_MAX_PUNCT_PICTURE + 1];
	
	if(m_pPicturePunct)
	{
		memset(m_pPicturePunct,'\0',CBASE_MAX_PUNCT_PICTURE + 1);
		//strcpyn(m_pPicturePunct,".,:;",CBASE_MAX_PUNCT_PICTURE + 1);
	}
}
#endif

/*
	SetDefaultUserPicture()

	Imposta la picture utente di default.
*/
#ifdef _USE_FIELD_PICTURES
void CBase::SetDefaultUserPicture(void)
{
	if(!m_pPictureUserDefined)
		m_pPictureUserDefined = new char[CBASE_MAX_USER_PICTURE + 1];
	
	if(m_pPictureUserDefined)
		memset(m_pPictureUserDefined,'\0',CBASE_MAX_USER_PICTURE + 1);
}
#endif

/*
	SetFieldFormat()

	Formatta il contenuto del campo a seconda dei flags.
*/
#ifdef _USE_FIELD_PICTURES
void CBase::SetFieldFormat(char* pField,int nSize,unsigned long nFlags)
{
	if(IsValid())
	{
		if(nFlags & CBASE_FLAG_TOUPPER)
			strurp(pField);
		if(nFlags & CBASE_FLAG_TOLOWER)
			strlrw(pField);
		if((nFlags & CBASE_FLAG_NUMBER && *m_pPictureNumber) || (nFlags & CBASE_FLAG_CHAR && *m_pPictureChar) || (nFlags & CBASE_FLAG_PUNCT && *m_pPicturePunct) || (nFlags & CBASE_FLAG_USERDEFINED && *m_pPictureUserDefined))
			SetFieldFormatByPicture(pField,nSize,nFlags);
	}
}
#endif

/*
	SetFieldFormatByPicture()

	Formatta il contenuto del campo secondo la picture.
*/
#ifdef _USE_FIELD_PICTURES
void CBase::SetFieldFormatByPicture(char* pField,int nSize,unsigned long nFlags)
{
	if(IsValid())
	{
		char szBuffer[MAX_FIELDSIZE+1];
		register char* p = szBuffer;
		register int i;

		memset(szBuffer,' ',sizeof(szBuffer));

		for(i = 0; pField[i] && i < nSize && i < sizeof(szBuffer); i++)
		{
			if(nFlags & CBASE_FLAG_NUMBER)
				if(strchr(m_pPictureNumber,pField[i]))
				{
					*p++ = pField[i];
					continue;
				}
			
			if(nFlags & CBASE_FLAG_CHAR)
				if(strchr(m_pPictureChar,pField[i]))
				{
					*p++ = pField[i];
					continue;
				}
			
			if(nFlags & CBASE_FLAG_PUNCT)
				if(strchr(m_pPictureChar,pField[i]))
				{
					*p++ = pField[i];
					continue;
				}
			
			if(nFlags & CBASE_FLAG_USERDEFINED)
				if(strchr(m_pPictureUserDefined,pField[i]))
				{
					*p++ = pField[i];
					continue;
				}
		}

		memset(pField,' ',nSize);

		if((i = p-szBuffer) > 0)
		{
			// allineamento (stringhe a sx, numeri a dx), occhio: == e non &
			if(nFlags==CBASE_FLAG_NUMBER)
				memcpy(pField + (nSize-i),szBuffer,i);
			else
				memcpy(pField,szBuffer,i);
		}
	}
}
#endif
