/*$
	CUrlItem.cpp
	Classe per la gestione del file delle URLs (wchg), implementa il caricamento delle
	URLs e la gestione della lista relativa.

	Vedi le note in CUrlItem.h
*/
#include "pragma.h"
#include "env.h"
#include "macro.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "strings.h"
#include "algorithm.h"
#include "window.h"
#include "win32api.h"
#include "csvlib.h"
#include "CNodeList.h"
#include "textdef.h"
#include "CTextFile.h"
#include "url.h"
#include "urlparser.h"
#include "CUrlItem.h"

#include "traceexpr.h"
//#define _TRACE_FLAG			_TRFLAG_TRACEOUTPUT // opzioni: _TRFLAG_NOTRACE, _TRFLAG_TRACEFILE, _TRFLAG_TRACECONSOLE, _TRFLAG_TRACEOUTPUT, _TRFLAG_TRACEBREAKPOINT
#define _TRACE_FLAG			_TRFLAG_NOTRACE // opzioni: _TRFLAG_NOTRACE, _TRFLAG_TRACEFILE, _TRFLAG_TRACECONSOLE, _TRFLAG_TRACEOUTPUT, _TRFLAG_TRACEBREAKPOINT
#define _TRACE_FLAG_INFO	_TRACE_FLAG
#define _TRACE_FLAG_WARN	_TRACE_FLAG
#define _TRACE_FLAG_ERR		_TRACE_FLAG

/*
	CUrlListManager()
*/
CUrlListManager::CUrlListManager()
{
	Clean();
}

/*
	Clean()
*/
void CUrlListManager::Clean(void)
{
	memset(m_szUrlFile,'\0',sizeof(m_szUrlFile));
	m_nCurrent = -1;
	m_nSanitizer = 0;
	memset(m_szError,'\0',sizeof(m_szError));
	m_urlItemList.DeleteAll();
}

/*
	Load()

	Carica la lista delle URLs dal file (un semplice file di testo, una URL per linea).
	Le linee che iniziano con ';' vengono considerate come commenti.

	Restituisce TRUE se carica correttamente dal file o FALSE altrimenti.
*/
BOOL CUrlListManager::Load(LPCSTR lpcszFilename)
{
	BOOL bRet = FALSE;
	CTextFile textFile;

	// resetta la descrizione per l'ultimo errore
	memset(m_szError,'\0',sizeof(m_szError));

	// memorizza il nome del file di input
	strcpyn(m_szUrlFile,lpcszFilename,sizeof(m_szUrlFile));

	// apre il file specificato
	if(textFile.Open(m_szUrlFile,FALSE))
	{
		char buffer[sizeof(URLITEMRECORD)+1] = {0}; // occhio: la dimensione deve essere la somma delle dimensioni di URLITEMRECORD
		URLITEMRECORD record = {0};

		// mappa il record del file sulla struttura per leggere via formato CSV
		CSV_FIELD_MAP mapping[] = {
			{&record.service[0],sizeof(record.service)},
			{&record.apikey[0],sizeof(record.apikey)},
			{&record.cache[0],sizeof(record.cache)},
			{&record.url[0],sizeof(record.url)},
			{&record.subdir[0],sizeof(record.subdir)},
		};

		// inizializza la lista delle URLs (records)
		if(m_urlItemList.Count() > 0)
			m_urlItemList.DeleteAll();

		// legge dal file x linee
		while(textFile.ReadLine(buffer,sizeof(buffer))!=FILE_EOF)
		{
			// salta i commenti/linee vuote
			strltrim(buffer);
			if(strlen(buffer) <= 0)
				continue;
			if(buffer[0]==';')
				continue;

			// analizza ogni linea del file letta sopra come se fosse un CSV
			if(csv_parse_line(buffer,mapping,sizeof(mapping)/sizeof(mapping[0]))==CSV_SUCCESS)
			{
				// controlla i campi letti dal file

				// <service> valori possibili: 1=picsum, 2=pexels, 3=reddit, 4=danbooru, 5=utente
				// il 0=generico e' riservato ed usato internamente dal programma
				int validservice = 0;
				if(*record.service && isdigit(record.service[0]))
				{
					int service = record.service[0] - '0';
					if(service > 0 && service < 6)
						validservice = 1;
				}
				if(!validservice)
				{
					strcpyne(m_szError,"invalid service id",sizeof(m_szError));
					break;
				}

				// <apikey> valori possibili: la chiave, la chiave + nome login (nella forma chiave:login) o vuoto
				// NON fa controlli sul contenuto

				// <cache> valori possibili: -1=solo una volta, 0=sempre, >0=cache
				int validcache = 0;
				if(*record.cache)
				{
					int nCacheValue = atoi(record.cache);
					if(nCacheValue==0 || nCacheValue==1 || nCacheValue==-1)
						validcache = 1;
				}
				if(!validcache)
				{
					strcpyne(m_szError,"invalid cache value",sizeof(m_szError));
					break;
				}

				// <url> valori possibili: http[...]
				if(!*record.url || !isalpha(record.url[0]) || strncmp(record.url,"http",4)!=0)
				{
					strcpyne(m_szError,"invalid url",sizeof(m_szError));
					break;
				}
				URLDATA urlData = {0};
				strcpyn(urlData.url,record.url,sizeof(urlData.url));
				if(!url_parse(&urlData))
				{
					strcpyne(m_szError,"invalid url",sizeof(m_szError));
					break;
				}
	
				// passa i valori del record nell'elemento URLITEM e lo inserisce nella lista
				URLITEM* urlitem = (URLITEM*)m_urlItemList.Add();
				ASSERTEXPR(urlitem);
				if(urlitem)
				{
					memset(urlitem,'\0',sizeof(URLITEM));

					// id servizio
					urlitem->service = record.service[0] - '0';
					
					// per mantenere traccia dell'id originale, perche' Picsum, Pexels, Reddit e Danbooru passano poi a generico
					urlitem->origin  = urlitem->service;

					// le URLs utente[5] e Picsum[1] sono URLs dirette, il resto (Pexels[2], Reddit[3] e Dabbooru[4]) sono meta-urls (files JSON che contengono URLs)
					urlitem->metaurl = (urlitem->origin==2 || urlitem->origin==3 || urlitem->origin==4) ? TRUE : FALSE;

					// API Key, puo' essere presente o no
					if(*record.apikey)
						strcpyn(urlitem->apikey,record.apikey,sizeof(urlitem->apikey));

					// valore cache
					urlitem->cache = atoi(record.cache);

					// url
					if(*record.url)
						strcpyn(urlitem->url,record.url,sizeof(urlitem->url));
					if(*(urlitem->url))
					{
						// le URLs possono includere parametri nella query, dopo il carattere ? e separati dal carattere &
						// qui permette di includere parametri "custom", che vengono estratti ad uso esclusivo del programma
						// tali parametri vanno specificati dopo il carattere per la "meta-query", ossia ! e con la sintassi
						// usuale (nome parametro + '=' + valore parametro)
						// tali parametri vengono caricati nella struttura e rimossi dalla URL
						char* pMetaQuery = strchr(urlitem->url,'!');
						if(pMetaQuery && *(pMetaQuery+1))
						{
							// local
							char* local = strstr(pMetaQuery,"local=");
							if(local && *(local+6))
								urlitem->metaquery.local = *(local+6);
							// d = do not overwrite (proliferate), o = overwrite the existing file, s = skip existing files 
							// se non specificato assume skip existing only if local is bigger than (or equal to) remote
							if(urlitem->metaquery.local!='d' && urlitem->metaquery.local!='o' && urlitem->metaquery.local!='s')
								urlitem->metaquery.local='\0';

							// connection
							char* connection = strstr(pMetaQuery,"connection=");
							if(connection && *(connection+11))
								urlitem->metaquery.connection = *(connection+11);
							// c = close, k = keep alive
							if(urlitem->metaquery.connection!='c' && urlitem->metaquery.connection!='k')
								urlitem->metaquery.connection='\0';

							// cachebuster
							char* cachebuster = strstr(pMetaQuery,"cachebuster=");
							if(cachebuster && *(cachebuster+12))
							{
								char* p = cachebuster + 12;
								for(int i=0; p && *p && *p!='&' && i < sizeof(urlitem->metaquery.cachebuster)-1; i++)
									urlitem->metaquery.cachebuster[i] = *p++;
							}
							// sdraia la meta-query, e' ad uso del programma, non va lasciata nella query da richiedere al server
							*pMetaQuery = '\0';
						}
					}

					// directory x download
					if(*record.subdir)
					{
						if(strcmp(record.subdir,"auto")==0 && strchr(urlitem->url,'%'))
						{
							strcpyne(m_szError,"auto not allowed for subdir when the URL contains percent-encoded characters",sizeof(m_szError));
							break;
						}
						else
							strcpyn(urlitem->subdir,record.subdir,sizeof(urlitem->subdir));
					}

					urlitem->reply = 0;
					urlitem->shown = Undef;
					bRet = TRUE;

					TRACEEXPR((	_TRACE_FLAG_INFO,__FILE__,__LINE__,
								"CUrlListManager::Load() -> \nservice: %d\norigin: %d\napikey: %s\ncache: %d\nurl: %s\nsubdir: %s\n\n",
								urlitem->service,urlitem->origin,urlitem->apikey,urlitem->cache,urlitem->url,urlitem->subdir));
				}
			}
			else
				strcpyne(m_szError,"wrong CSV format",sizeof(m_szError));
		}

		textFile.Close();
	}
	else
	{
		strcpyne(m_szError,"open file error",sizeof(m_szError));
	}

	// controlla che il file contenga URLs
	if(m_urlItemList.Count() <= 0)
	{
		strcpyne(m_szError,"file is empty",sizeof(m_szError));
		bRet = FALSE;
	}

	TRACEEXPR((_TRACE_FLAG_INFO,__FILE__,__LINE__,"CUrlListManager::Load(): list has %d items\n\n",m_urlItemList.Count()));

	return(bRet);
}

/*
	Next()

	Legge il prossimo elemento URLITEM della lista e ne restituisce il puntatore, ruotando sulla lista all'infinito.
	Se la lista e' vuota restituisce NULL.
*/
URLITEM* CUrlListManager::Next(void)
{
	URLITEM* urlitem = NULL;
	int nTot = m_urlItemList.Count();
	TRACEEXPR((_TRACE_FLAG_INFO,__FILE__,__LINE__,"CUrlListManager::Next(): list has %d items\n",nTot));

	if(nTot > 0)
	{
		if(++m_nCurrent >= nTot)
			m_nCurrent = 0;

		urlitem = (URLITEM*)m_urlItemList.GetAt(m_nCurrent);

		ASSERTEXPR(urlitem);
		TRACEEXPR((_TRACE_FLAG_INFO,__FILE__,__LINE__,"CUrlListManager::Next() serving item %d -> url=%s, shown=%d\n",m_nCurrent,urlitem->url,(int)urlitem->shown));
	}
	
	return(urlitem);
}

/*
	Sanitize()

	Ripulisce (ogni <n> chiamate) la lista delle URLs, eliminando quelle gia' elaborate (visualizzate o meno).
*/
int CUrlListManager::Sanitize(int nHowManyTimes)
{
	// e' ora di ripulire la lista
	if(++m_nSanitizer >= nHowManyTimes)
	{
		m_nSanitizer = 0;

#ifdef DEBUG
		int nTot = m_urlItemList.Count();
		TRACEEXPR((_TRACE_FLAG_INFO,__FILE__,__LINE__,"CUrlListManager::Sanitize(): list has %d items BEFORE sanitizing\n",nTot));
#endif
		// scorre la lista cercando ed eliminando gli elementi gia' elaborati (con esito visualizzazione True o False)
		URLITEM* pItem;
		ITERATOR iter = m_urlItemList.First();
		while(iter!=(ITERATOR)NULL)
		{
			pItem = (URLITEM*)(iter->data);
			if(pItem)
			{
				if(pItem->shown!=Undef)
				{
					TRACEEXPR((_TRACE_FLAG_INFO,__FILE__,__LINE__,"CUrlListManager::Sanitize(): deleting shown url -> %s\n",pItem->url));
					m_urlItemList.Delete(iter);
				}
			}
			iter = m_urlItemList.Next(iter);
		}

#ifdef DEBUG
		nTot = m_urlItemList.Count();
		TRACEEXPR((_TRACE_FLAG_INFO,__FILE__,__LINE__,"CUrlListManager::Sanitize(): list has %d items AFTER sanitizing\n",nTot));
#endif
	}

	return(m_urlItemList.Count());
}
