/*$
	CHttpDownloader.cpp
	Classe per il download via HTTP.
	Evoluzione del vecchio codice usato da CrawlPaper che gestiva solamente HTTP/1.0.
	Supporta il protocollo HTTP/1.1 tramite l'integrazione con l'API WinINet di Windows.
	Luca Piergentili, Giugno '25
*/

//$ TODO: alcune parti (tipo gestione cookies) andrebbero riviste una volta per tutte

#include "pragma.h"
#include <stdio.h>
#include <string.h>
#include "strings.h"
#include <locale.h>
#include <time.h>
#include <stdarg.h>
#include "window.h"
#include "win32api.h"
#include "ipaddress.h"
#include "CWinsock.h"
#include "inet.h"
#include "win32inet.h"
#include "url.h"
#include "urlparser.h"
#include <wininet.h>
#include <shlwapi.h>
#include "CNodeList.h"
#include "CRegKey.h"
#include "CDateTime.h"
#include "CHostCache.h"
#include "CContentDecoder.h"
#include "CHttpDownloader.h"
#include <vector>
#include <string>

#include "traceexpr.h"
#define _TRACE_FLAG			_TRFLAG_TRACEOUTPUT // opzioni: _TRFLAG_NOTRACE, _TRFLAG_TRACEFILE, _TRFLAG_TRACECONSOLE, _TRFLAG_TRACEOUTPUT, _TRFLAG_TRACEBREAKPOINT
//#define _TRACE_FLAG			_TRFLAG_NOTRACE // opzioni: _TRFLAG_NOTRACE, _TRFLAG_TRACEFILE, _TRFLAG_TRACECONSOLE, _TRFLAG_TRACEOUTPUT, _TRFLAG_TRACEBREAKPOINT
#define _TRACE_FLAG_INFO	_TRACE_FLAG
#define _TRACE_FLAG_WARN	_TRACE_FLAG
#define _TRACE_FLAG_ERR		_TRACE_FLAG

/*
	DummyLogMessageCallback()

	Default per funzione per trascrizione log su file. per non dover testare il puntatore ad ogni chiamata.
*/
void DummyLogMessageCallback(DWORD dwFlags,LPCSTR pszHostDomain,LPCSTR pszBaseDir,LPCSTR pszFormat,...)
{
}

/*
	CHttpDownloader()
*/
CHttpDownloader::CHttpDownloader(LogMessageCallback pfnCallback/* = NULL */,DWORD dwFlags/* = 0 */)
{
	// mantenere allineato con il reset di CloseSession()
    m_hInternetSession = NULL;
	m_hConnect = NULL;
    m_hRequest = NULL;
	m_nConnectionType = HTTP_CONNECTION_KEEPALIVE;
	m_httpVersion.dwMajorVersion = 1;
	m_httpVersion.dwMinorVersion = 1;

	m_httpReply.dwHttpCode = 0L;
	m_httpReply.dwStatusCode = 0L;
	m_httpReply.m_serverHttpHeaderList.DeleteAll();

	m_clientHttpHeaderList.DeleteAll();

	m_bHaveValidSession = FALSE;
	m_nOverwrite = HTTP_FLAG_DONOTOVWR;
	m_bReplaceFileExt = FALSE;
	strcpyn(m_szUserAgent,"Mozilla/3.0 (Win95; No encryption) Netscape",sizeof(m_szUserAgent));
	m_bUseCookies = FALSE;
	m_dwCallbackFlags = dwFlags;
	if(pfnCallback)
		m_pfnLogCallback = pfnCallback;
	else
		m_pfnLogCallback = DummyLogMessageCallback;
	m_pfnDatabaseCallback = NULL;
	m_pfnNameRulerCallback = NULL;
	m_pNameRulerContext = NULL;
	memset(m_szBaseDirectory,'\0',sizeof(m_szBaseDirectory));
	GetCurrentDirectory(sizeof(m_szBaseDirectory)-1,m_szBaseDirectory);
	memset(m_szCurrentHost,'\0',sizeof(m_szCurrentHost));
	memset(m_szCloseKeepAliveHost,'\0',sizeof(m_szCloseKeepAliveHost));
	memset(m_szCurrentBaseDomain,'\0',sizeof(m_szCurrentBaseDomain));
	memset(m_szHostForReferer,'\0',sizeof(m_szHostForReferer));

	//m_Socket;
	//m_hostCache;
}

/*
	~CHttpDownloader()

	Chiude automaticamente la sessione.
*/
CHttpDownloader::~CHttpDownloader()
{
	CloseSession();
}

/*
	OpenSession()
	
	Apre la sessione, non serve chiamarla esplicitamente, la classe gestisce la chiamata autonomamente.
*/
DWORD CHttpDownloader::OpenSession(void)
{
	DWORD dwError = 0L;

    if((m_hInternetSession = InternetOpenA(m_szUserAgent,INTERNET_OPEN_TYPE_PRECONFIG,NULL,NULL,0))!=NULL)
	{
		// specifica due timeout differenti per connessione e ricezione
        DWORD dwConnectTimeout = CONNECT_RECEIVE_TIMEOUT_MS;
        InternetSetOption(m_hInternetSession,INTERNET_OPTION_CONNECT_TIMEOUT,&dwConnectTimeout,sizeof(dwConnectTimeout));
        DWORD dwReceiveTimeout = CONNECT_RECEIVE_TIMEOUT_MS;
        InternetSetOption(m_hInternetSession,INTERNET_OPTION_RECEIVE_TIMEOUT,&dwReceiveTimeout,sizeof(dwReceiveTimeout));

		m_bHaveValidSession = TRUE;
    }
	else
	{
		dwError = GetLastError();

		m_bHaveValidSession = FALSE;
	}

	return(dwError);
}

/*
	CloseSession()

	Chiude la sessione, chiamata automaticamente nel distruttore.
*/
void CHttpDownloader::CloseSession(void)
{
    if(m_bHaveValidSession)
	{
		// rilascia la lista degli header ricevuti dal server e, dato che chiude la sessione, anche 
		// quella degli header inviati dal client
		// durante l'esecuzione invece, rilasciare questa'ultima e' compito esclusivo del chiamante
		CleanServerHttpHeaders();
		RemoveClientHttpHeaders();

		// mantenere allineato con la inizializzazione del costruttore
        if(m_hInternetSession!=NULL)
		{
			InternetCloseHandle(m_hInternetSession);
	        m_hInternetSession = NULL;
			m_pfnLogCallback(m_dwCallbackFlags,GetCurrentHost(),GetBaseDirectory(),"WinInet session closed\n");
		}
		m_bHaveValidSession = FALSE;
		if(m_hConnect)
		{
			InternetCloseHandle(m_hConnect);
			m_hConnect = NULL;
		}		
        if(m_hRequest)
		{	
			InternetCloseHandle(m_hRequest);
			m_hRequest = NULL;
		}
		m_nConnectionType = HTTP_CONNECTION_KEEPALIVE;
		m_httpVersion.dwMajorVersion = 1;
		m_httpVersion.dwMinorVersion = 1;

		m_httpReply.dwHttpCode = 0L;
		m_httpReply.dwStatusCode = 0L;
		m_httpReply.m_serverHttpHeaderList.DeleteAll();

		m_clientHttpHeaderList.DeleteAll();

		m_bHaveValidSession = FALSE;
		m_nOverwrite = HTTP_FLAG_DONOTOVWR;
		m_bReplaceFileExt = FALSE;
		strcpyn(m_szUserAgent,"Mozilla/3.0 (Win95; No encryption) Netscape",sizeof(m_szUserAgent));
		m_dwCallbackFlags = 0L;
		m_pfnLogCallback = NULL;
		m_pfnDatabaseCallback = NULL;
		m_pfnNameRulerCallback = NULL;
		m_pNameRulerContext = NULL;
		memset(m_szBaseDirectory,'\0',sizeof(m_szBaseDirectory));
		GetCurrentDirectory(sizeof(m_szBaseDirectory)-1,m_szBaseDirectory);
		memset(m_szCurrentHost,'\0',sizeof(m_szCurrentHost));
		memset(m_szCloseKeepAliveHost,'\0',sizeof(m_szCloseKeepAliveHost));
		memset(m_szCurrentBaseDomain,'\0',sizeof(m_szCurrentBaseDomain));
		memset(m_szHostForReferer,'\0',sizeof(m_szHostForReferer));

		//m_Socket;
		//m_hostCache;
	}
}

/*
	SetHttpVersion()

	Imposta il numero di versione HTTP di riferimento ed il tipo di connessione relativa.
*/
BOOL CHttpDownloader::SetHttpVersion(int nMajor,int nMinor)
{
	m_httpVersion.dwMajorVersion = nMajor;
	m_httpVersion.dwMinorVersion = nMinor;

	// imposta il tipo di connessione (close/keep-alive) automaticamente a seconda della versione HTTP

	// HTTP/1.0
	if(m_httpVersion.dwMinorVersion==0)
		m_nConnectionType = HTTP_CONNECTION_CLOSE;
	// HTTP/1.1
	else if(m_httpVersion.dwMinorVersion==1)
		m_nConnectionType = HTTP_CONNECTION_KEEPALIVE;
	else
		return(FALSE);

	return(TRUE);
}

/*
	SetConnectionType()

	Imposta il tipo di connessione (close/keep-alive), restituendo il valore previo.

	Se si dovessero fare macheggi, occhio all'ordine di chiamata con SetHttpVersion(), perche'
	questa reimposta il tipo di connessione a seconda della versione HTTP.
*/
int CHttpDownloader::SetConnectionType(int nType)
{
	int nPrevious = m_nConnectionType;

	if(nType==HTTP_CONNECTION_CLOSE || nType==HTTP_CONNECTION_KEEPALIVE)
		m_nConnectionType = nType;

	return(nPrevious);
}

/*
	SetUserAgent()

	Imposta la stringa per l'user-agent.
	Passare custom + true per ricavare le info sul SO e costruire una stringa con compatibilita' minima,
	o passare custom + false se la stringa viene gia' completa.
	Se non viene chiamata, la classe usa una stringa minima e preistorica, impostata nel costruttore.
*/
void CHttpDownloader::SetUserAgent(LPCSTR lpcszUserAgent,BOOL bBuildFullString)
{
	if(bBuildFullString)
	{
		char szOsVersion[128] = {0};
		OSVERSIONINFOEX osInfo = {0};
		osInfo.dwOSVersionInfoSize = sizeof(osInfo);

		if(GetVersionEx((LPOSVERSIONINFO)&osInfo))
		{
			snprintf(szOsVersion,sizeof(szOsVersion),"Windows NT %lu.%lu",osInfo.dwMajorVersion,osInfo.dwMinorVersion);
			if(osInfo.wProductType==VER_NT_WORKSTATION)
				strcatn(szOsVersion,"; Workstation",sizeof(szOsVersion));
			else
				strcatn(szOsVersion,"; Server",sizeof(szOsVersion));
			strcatn(szOsVersion,"; x86",sizeof(szOsVersion)); // fuck x64
		}
		else
		{
			strcpy(szOsVersion,"Licensed Windows");
		}

		const char* pszCompatibility = " AppleWebKit/537.36 (KHTML, like Gecko) Chrome/100.0.4896.127 Safari/537.36";

		snprintf(m_szUserAgent,sizeof(m_szUserAgent),"%s (%s) %s",lpcszUserAgent,szOsVersion,pszCompatibility);
	}
	else
	    strcpyn(m_szUserAgent,lpcszUserAgent,sizeof(m_szUserAgent));
	
	m_pfnLogCallback(m_dwCallbackFlags,GetCurrentHost(),GetBaseDirectory(),"User-Agent for WinInet session is: %s\n",m_szUserAgent);
}

/*
	SetCurrentHost()

	Imposta l'host (ed il dominio base) corrente a partire dalla url.
*/
int CHttpDownloader::SetCurrentHost(LPCSTR lpcszUrl)
{
	int nWSAError = WSAHOST_NOT_FOUND;

	// estrae l'host (www.google.com) dalla url (https://www.google.com/index.thml)
	char* pszHost = GetHostFromUrl(lpcszUrl);
	if(pszHost)
	{
		char szIP[IP_ADDRESS_LEN+1] = {0};

		// risolve il nome host ricavando l'ip
		if((nWSAError = CheckHostByName(pszHost,szIP,sizeof(szIP)))==0)
		{
			m_pfnLogCallback(m_dwCallbackFlags,GetCurrentHost(),GetBaseDirectory(),"GetHostFromUrl(%s): %s (%s)\n",lpcszUrl,pszHost,szIP);

			// salva l'host (www.google.com)
			strcpyn(m_szCurrentHost,pszHost,sizeof(m_szCurrentHost));

			// e lo copia nel campo di default per il Referer:
			URLDATA url = {0};
			strcpyn(url.url,lpcszUrl,sizeof(url.url));
			url_parse(&url);
			snprintf(m_szHostForReferer,sizeof(m_szHostForReferer),"%s://%s/",url.proto,m_szCurrentHost);
		
			// ricava il dominio base (google.com)
			char* pszBaseDomain = GetBaseDomainFromHost(pszHost);
			if(pszBaseDomain)
			{
				m_pfnLogCallback(m_dwCallbackFlags,GetCurrentHost(),GetBaseDirectory(),"GetBaseDomainFromHost(%s): %s\n",pszHost,pszBaseDomain);

				// salva il dominio base (google.com)
				strcpyn(m_szCurrentBaseDomain,pszBaseDomain,sizeof(m_szCurrentBaseDomain));

				free(pszBaseDomain);
			}
		}

		free(pszHost);
    }

	return(nWSAError);
}

/*
	CheckHostByName()

	Controlla se l'host e' valido implementando un sistema di cache minimo.

	Restituisce 0 se l'host e' stato risolto (dalla cache o dalla rete), o 
	il codice d'errore Winsock altrimenti.
*/
int CHttpDownloader::CheckHostByName(LPCSTR hostName,LPSTR lpszIP,size_t nIP)
{
	return(m_hostCache.ResolveHost(hostName,lpszIP,nIP));
}

/*
	GetServerHttpHeader()

	Scorre la lista interna degli header ricevuti dal server con la risposta HTTP cercando 
	l'header in questione.

	Restituisce il valore se trovato, o NULL altrimenti.
*/
LPCSTR CHttpDownloader::GetServerHttpHeader(LPCSTR lpcszHeader)
{
	HTTPHEADER* h;
	ITERATOR iter;
	LPCSTR pValue = NULL;

	if((iter = m_httpReply.m_serverHttpHeaderList.First())!=(ITERATOR)NULL)
	{
		do {
			h = (HTTPHEADER*)iter->data;
			if(h)
			{
				if(stricmp(lpcszHeader,h->name)==0)
				{
					pValue = h->value;
					break;
				}
			}
		}
		while((iter = m_httpReply.m_serverHttpHeaderList.Next(iter))!=(ITERATOR)NULL);
	}

	return(pValue);
}

/*
	AddClientHttpHeader()

	Aggiunge alla lista l'header HTTP da inviare al server con la richiesta (GET).
	Se il valore non esiste nella lista lo crea, se gia' esiste lo aggiorna e se 
	viene passato a "", lo elimina dalla lista.

	Restituisce TRUE se riesce, FALSE se fallisce o se si cerca di aggiungere l'header 
	"Connection", dato che il tipo di connessione va impostata tramite il flag relativo,
	non specificando direttamente l'header.
*/
BOOL CHttpDownloader::AddClientHttpHeader(LPCSTR lpcszName,LPCSTR lpcszValue)
{
	HTTPHEADER* h;
	ITERATOR iter;
	BOOL bFound = FALSE;

	if(stricmp(lpcszName,"Connection")==0)
		return(FALSE);

	// cerca il valore, se lo trova lo aggiorna, o lo elimina (se passato a "")
	if((iter = m_clientHttpHeaderList.First())!=(ITERATOR)NULL)
	{
		do {
			h = (HTTPHEADER*)iter->data;
			if(h)
			{
				if(strcmp(lpcszName,h->name)==0)
				{
					bFound = TRUE;
					if(strcmp(lpcszValue,"")==0)
						m_clientHttpHeaderList.Delete(iter);
					else
						strcpyn(h->value,lpcszValue,HTTPHEADER_VALUE_LEN+1);
					break;
				}
			}
		}
		while((iter = m_clientHttpHeaderList.Next(iter))!=(ITERATOR)NULL);
	}
	
	// non l'ha trovato, lo inserisce
	if(!bFound)
	{
		HTTPHEADER* h = NULL;
		if((h = (HTTPHEADER*)m_clientHttpHeaderList.Create())!=NULL)
		{
			m_clientHttpHeaderList.Initialize(h);
			strcpyn(h->name,lpcszName,HTTPHEADER_NAME_LEN+1);
			strcpyn(h->value,lpcszValue,HTTPHEADER_VALUE_LEN+1);
			m_clientHttpHeaderList.Add(h);
		}
	}

	return(TRUE);
}

/*
	RemoveClientHttpHeaders()

	Elimina dalla lista uno (quello specificato) o tutti (se non viene passato 
	nessun nome) gli headers HTTP impostati in precedenza con AddClientHttpHeader().
*/
void CHttpDownloader::RemoveClientHttpHeaders(LPCSTR lpcszName/* = NULL */)
{
	if(lpcszName)
	{
		HTTPHEADER* h;
		ITERATOR iter;

		// cerca il valore, se lo trova lo elimina
		if((iter = m_clientHttpHeaderList.First())!=(ITERATOR)NULL)
		{
			do {
				h = (HTTPHEADER*)iter->data;
				if(h)
				{
					if(strcmp(lpcszName,h->name)==0)
					{
						m_clientHttpHeaderList.Delete(iter);
						break;
					}
				}
			}
			while((iter = m_clientHttpHeaderList.Next(iter))!=(ITERATOR)NULL);
		}
	}
	else
		m_clientHttpHeaderList.DeleteAll();
}

/*
	DownloadUrl()

	Scarica l'url in locale.
	
	Note a proposito di keep-alive:
	InternetOpenA e InternetConnectA: l'apertura della sessione Internet e la connessione all'host devono essere fatte una sola volta. 
	Questi due handle (hSession e hConnect) rappresentano la "linea diretta" con il server e non devono essere chiusi tra una richiesta e l'altra.
	HttpOpenRequestA: Questo handle (hRequest) e' specifico per ogni singola richiesta HTTP. Ogni volta che si vuole scaricare una nuova pagina, si 
	deve creare un nuovo hRequest. Il codice deve mantenere il ciclo GET -> crea hRequest -> invia hRequest -> chiudi hRequest. Chiudere l'handle 
	della richiesta non chiude la connessione TCP keep-alive.
	Sebbene lo standard HTTP/1.1 renda la connessione persistente il comportamento predefinito, molti server, specialmente quelli che si trovano 
	dietro a proxy o che devono interagire con client più vecchi, aggiungono esplicitamente l'header Connection: keep-alive nella risposta per 
	chiarezza.
	A volte il server puo' aggiungere l'header Keep-Alive: timeout=5, max=100, questo header fornisce i dettagli specifici del timeout della 
	connessione. Indica al client che la connessione rimarra' aperta per 5 secondi (timeout=5) e che sara' chiusa dopo un massimo di 100 richieste 
	(max=100).
	Il bot deve fare affidamento non sull'assenza ma sulla presenza di uno specifico header di chiusura: Connection: close nella risposta e' l'unico 
	segnale sicuro che il server invia per indicare du chiudere la connessione e non riutilizzarla. In tutti gli altri casi (sia che il server abbia 
	risposto con un Connection: keep-alive esplicito o che non abbia risposto affatto), il bot puo' assumere che la connessione sia persistente e puo' 
	provare a riutilizzarla per la prossima richiesta.

	Range per codici custom: 4xx e 5xx
	| Range       | Descrizione                                                                          |
	| ----------- | ------------------------------------------------------------------------------------ |
	| **418**     | "I'm a teapot" (Easter Egg di RFC 2324)                                              |
	| **421-424** | Gia' assegnati (Misdirected Request, Unprocessable Entity, Locked, Failed Dependency)|
	| **425**     | Unassigned                                                                           |
	| **426**     | Upgrade Required                                                                     |
	| **428-429** | Già assegnati                                                                        |
	| **430**     | Unassigned                                                                           |
	| **431**     | Request Header Fields Too Large                                                      |
	| **433-450** | Unassigned                                                                           |
	| **451**     | Unavailable For Legal Reasons                                                        |
	| **452-499** | **Disponibili per uso custom**                                                       |
	| **500-599** | Alcuni liberi, ma meno comuni                                                        |
*/
BOOL CHttpDownloader::DownloadUrl(LPSTR lpszUrl,UINT nUrlSize,LPSTR lpszOutputfile,UINT nSize,LPSTR lpszNonCollidingFileName/* = NULL*/,UINT nNonCollidingSize/* = (UINT)-1*/)
{
	// controlla se la sessione Internet e' stata aperta
	if(!m_bHaveValidSession)
	{
		// apre la sessione Internet
		DWORD dwError = 0L;
		if((dwError = OpenSession())==0L)
		{
			m_pfnLogCallback(m_dwCallbackFlags,GetCurrentHost(),GetBaseDirectory(),"WinInet session opened\n");
		}
		else
		{
			m_httpReply.dwStatusCode = dwError;
			m_pfnLogCallback(m_dwCallbackFlags,GetCurrentHost(),GetBaseDirectory(),"error: unable to open the Internet session (%ld)\n",m_httpReply.dwStatusCode);
			return(FALSE);
		}
	}

	// imposta il nome host corrente
	int nWSAError = SetCurrentHost(lpszUrl);
	if(nWSAError!=0)
	{
		m_httpReply.dwStatusCode = nWSAError;
		m_pfnLogCallback(m_dwCallbackFlags,GetCurrentHost(),GetBaseDirectory(),"error: unable to resolve the host from %s (%ld)\n",lpszUrl,m_httpReply.dwStatusCode);
		return(FALSE);
	}

	// controlla se e' stata passata una url valida o un nome file locale
	int nUrlOrFileOfNothing = CheckIfUrlOrfile(lpszUrl,nUrlSize,lpszOutputfile,nSize);
	switch(nUrlOrFileOfNothing)
	{
		// dati non validi
		case -1:
			m_httpReply.dwStatusCode = ERROR_INTERNET_INCORRECT_FORMAT;
			m_pfnLogCallback(m_dwCallbackFlags,GetCurrentHost(),GetBaseDirectory(),"error: invalid url/file format specified (%ld)\n",m_httpReply.dwStatusCode);
			return(FALSE);
		// file
		case 0:
		case 1:
			m_pfnLogCallback(m_dwCallbackFlags,GetCurrentHost(),GetBaseDirectory(),"a local file has been specified instead of an url: %s\n",lpszOutputfile);
			return(TRUE);
		// url
		case 2:
		default:
			break;
	}

	m_pfnLogCallback(m_dwCallbackFlags,GetCurrentHost(),GetBaseDirectory(),"output directory is %s\n",m_szBaseDirectory);
    
    BOOL bOverallSuccess = FALSE;
    int nCurrentRedirects = 0;

	// duplica la url da scaricare (nel caso debbe seguire un reindirizzamento)
    char* pszCurrentUrl = strdup(lpszUrl);
    if(!pszCurrentUrl)
	{
		m_httpReply.dwStatusCode = ERROR_NOT_ENOUGH_MEMORY;
		m_pfnLogCallback(m_dwCallbackFlags,GetCurrentHost(),GetBaseDirectory(),"error: memory allocation failed (%ld)\n",m_httpReply.dwStatusCode);
        return(FALSE);
    }

	// per scomporre l'url (nel ciclo)
    char szHostName[INTERNET_MAX_HOST_NAME_LENGTH] = {0};
    char szUrlPath[INTERNET_MAX_PATH_LENGTH] = {0};
    URL_COMPONENTSA urlComp = {0};
    urlComp.dwStructSize = sizeof(urlComp);
    urlComp.lpszHostName = szHostName;
    urlComp.dwHostNameLength = sizeof(szHostName);
    urlComp.lpszUrlPath = szUrlPath;
    urlComp.dwUrlPathLength = sizeof(szUrlPath);

	// per salvare l'eventuale risposta del server all'header Connection:
	const char* pConnectionType = "";

	// struttura il download in un ciclo per poter ripetere nel caso in cui il server usi il reindirizzamento
	// break alla fine per uscire, o in mezzo per terminare se si verificano errori
	while(TRUE)
	{
		m_pfnLogCallback(m_dwCallbackFlags,GetCurrentHost(),GetBaseDirectory(),"now downloading %s\n",pszCurrentUrl);

		// per re-ciclare in caso di reindirizzamento

        // handle per la richiesta (GET)
		if(m_hRequest)
		{	
			InternetCloseHandle(m_hRequest);
			m_hRequest = NULL;
		}
		// handle per la connessione: close/keep-alive
		if(m_nConnectionType==HTTP_CONNECTION_CLOSE)
			if(m_hConnect)
			{
				InternetCloseHandle(m_hConnect);
				m_hConnect = NULL;
			}
		
		// scompone l'url
        if(!InternetCrackUrlA(pszCurrentUrl,0,0,&urlComp))
		{
            bOverallSuccess = FALSE;
			m_httpReply.dwStatusCode = GetLastError(); // ERROR_INTERNET_INCORRECT_FORMAT;
			m_pfnLogCallback(m_dwCallbackFlags,GetCurrentHost(),GetBaseDirectory(),"error: unable to parse the url %s (%ld)\n",pszCurrentUrl,m_httpReply.dwStatusCode);
            break;
        }

		// apre la connessione HTTP (Connection: close/keep-alive):
		// - Connection: close
		//   apre e chiude la connessione per ogni risorsa che deve scaricare
		// - Connection: keep-alive
		//   apre la connessione e la mantiene aperta fino a che la classe e' in uso (ossia solo la chiude 
		//   nel distruttore), a meno che 1) il server non indichi esplicitamente di chiuderla (invio header 
		//   Connection: close) o 2) quando la risorsa si trova su un host differente all'anteriore, caso in 
		//   cui deve chiudere e riaprire

		// prima chiamata
        if(!m_hConnect)
		{
			m_hConnect = InternetConnectA(m_hInternetSession,urlComp.lpszHostName,urlComp.nPort,NULL,NULL,INTERNET_SERVICE_HTTP,0,0);
			strcpyn(m_szCloseKeepAliveHost,urlComp.lpszHostName,sizeof(m_szCloseKeepAliveHost));
			//TRACEEXPR((_TRACE_FLAG_INFO,__FILE__,__LINE__,"DownloadUrl(): Connection opened: %s (%ld)\n",urlComp.lpszHostName,m_hConnect));
		}

		// chiamate successive
        if(m_hConnect)
		{
			// se la connessione segue aperta, controlla che non sia cambiato l'host 
			// della risorsa, in tal caso deve chiudere e riaprire la connessione
			if(strcmp(urlComp.lpszHostName,m_szCloseKeepAliveHost)!=0)
			{
				strcpyn(m_szCloseKeepAliveHost,urlComp.lpszHostName,sizeof(m_szCloseKeepAliveHost));
				if(m_hConnect)
					InternetCloseHandle(m_hConnect);
				m_hConnect = InternetConnectA(m_hInternetSession,urlComp.lpszHostName,urlComp.nPort,NULL,NULL,INTERNET_SERVICE_HTTP,0,0);
				//TRACEEXPR((_TRACE_FLAG_INFO,__FILE__,__LINE__,"DownloadUrl(): Connection re-opened: %s (%ld)\n",urlComp.lpszHostName,m_hConnect));
			}
		}
		
		// connessione fallita
        if(!m_hConnect)
		{
			bOverallSuccess = FALSE;
			m_httpReply.dwStatusCode = GetLastError(); // ERROR_INTERNET_CANNOT_CONNECT
			m_pfnLogCallback(m_dwCallbackFlags,GetCurrentHost(),GetBaseDirectory(),"error: unable to connect to %s (%ld)\n",urlComp.lpszHostName,m_httpReply.dwStatusCode);
			break;
        }
		
		m_pfnLogCallback(m_dwCallbackFlags,GetCurrentHost(),GetBaseDirectory(),"Internet connection open, connected to %s:%d\n",urlComp.lpszHostName,urlComp.nPort);

		// imposta i flags per la connessione HTTP
		DWORD dwOpenRequestFlags = INTERNET_FLAG_NO_UI | INTERNET_FLAG_RELOAD | INTERNET_FLAG_PRAGMA_NOCACHE | INTERNET_FLAG_NO_CACHE_WRITE;
		//DWORD dwOpenRequestFlags = INTERNET_FLAG_NO_UI;
		//$ durante prove iniziali era commentato al contrario, aggiustare una volta per tutte

		// Connection: close/keep-alive
		if(m_nConnectionType==HTTP_CONNECTION_CLOSE)
			dwOpenRequestFlags &= ~INTERNET_FLAG_KEEP_CONNECTION;
		else if(m_nConnectionType==HTTP_CONNECTION_KEEPALIVE)
			dwOpenRequestFlags |= INTERNET_FLAG_KEEP_CONNECTION;

		// http:// (non sicuro) vs https:// (sicuro)
		HTTP_PROTOCOL httpProto = WhichHttpProtocol(lpszUrl);
		if(httpProto==HTTP_PROTOCOL_NON_SECURE)
			dwOpenRequestFlags &= ~INTERNET_FLAG_SECURE;
		else if(httpProto==HTTP_PROTOCOL_SECURE)
			dwOpenRequestFlags |= INTERNET_FLAG_SECURE;

		// versione HTTP
		const char* pHttpVersion = NULL;
		if(IsHTTPVersion(1,0))
			pHttpVersion = "HTTP/1.0";
		else if(IsHTTPVersion(1,1))
			pHttpVersion = "HTTP/1.1";

		// si assicura che la url richiesta non sia vuota
		const char* requestPath;
		if(!urlComp.lpszUrlPath || !*(urlComp.lpszUrlPath) || urlComp.dwUrlPathLength <= 0)
			requestPath = "/";
		else
			requestPath = urlComp.lpszUrlPath;

		// crea l'handle per la richiesta HTTP
        m_hRequest = HttpOpenRequestA(m_hConnect,"GET",requestPath,pHttpVersion,NULL,NULL,dwOpenRequestFlags,0);
        if(!m_hRequest)
		{
            bOverallSuccess = FALSE;
			m_httpReply.dwStatusCode = GetLastError();
			m_pfnLogCallback(m_dwCallbackFlags,GetCurrentHost(),GetBaseDirectory(),"error: unable to carry out the HTTP request for %s (%ld)\n",urlComp.lpszUrlPath,m_httpReply.dwStatusCode);
            break;
        }
		
		m_pfnLogCallback(m_dwCallbackFlags,GetCurrentHost(),GetBaseDirectory(),"HTTP request accepted for %s\n",urlComp.lpszUrlPath);

		// abilita quanto necessario affinche la zoccola di WinInet 'senta' il numero di versione HTTP da usare nella richiesta
		// occhio che la chiamata e' posizionale, vedi le note relative
		if(!EnableHttpVersioning())
		{
            bOverallSuccess = FALSE;
			m_httpReply.dwStatusCode = GetLastError();
			m_pfnLogCallback(m_dwCallbackFlags,GetCurrentHost(),GetBaseDirectory(),"error: unable to set the HTTP version (%ld)\n",m_httpReply.dwStatusCode);
            break;
        }

		// aggiunge gli headers HTTP
		AddHttpHeadersToRequest(lpszUrl);

		// aggiunge i cookies
		if(m_bUseCookies)
		{
			std::vector<COOKIEINFO> cookiesToSend = LoadCookiesForUrl(m_szBaseDirectory,pszCurrentUrl,GetCurrentHost());
			if(!cookiesToSend.empty())
			{
				std::string cookieHeaderString;
				for(const auto& cookie : cookiesToSend)
				{
					if(!cookieHeaderString.empty())
						cookieHeaderString += "; ";
					cookieHeaderString += cookie.name;
					cookieHeaderString += "=";
					cookieHeaderString += cookie.value;
				}
				if(!cookieHeaderString.empty())
				{
					BOOL bCalloced = FALSE;
					char* pCookieHeader = NULL;
					char szCookieHeader[MAX_COOKIE_LEN+1] = {0};
					size_t len = cookieHeaderString.length() + 10;
					
					if(len >= sizeof(szCookieHeader))
					{
						pCookieHeader = (char*)calloc(len,sizeof(char));
						if(pCookieHeader)
							bCalloced = TRUE;
					}
					else
					{
						bCalloced = FALSE;
						len = sizeof(szCookieHeader);
						pCookieHeader = szCookieHeader;
					}
						
					snprintf(pCookieHeader,len,"Cookie: %s\r\n",cookieHeaderString.c_str());
					HttpAddRequestHeadersA(m_hRequest,pCookieHeader,(DWORD)-1,HTTP_ADDREQ_FLAG_ADD | HTTP_ADDREQ_FLAG_REPLACE);
					if(bCalloced)
						free(pCookieHeader);
				}
			}
			else
			{
				m_pfnLogCallback(m_dwCallbackFlags,GetCurrentHost(),GetBaseDirectory(),"no cookies found for the current url\n");
			}
		}

		// log degli headers HTTP della richiesta (client)
		LogHttpHeaders(m_hRequest,HTTP_QUERY_RAW_HEADERS_CRLF | HTTP_QUERY_FLAG_REQUEST_HEADERS,0);

		// manda la richiesta al server
        if(!HttpSendRequestA(m_hRequest,NULL,0,NULL,0))
		{
			bOverallSuccess = FALSE;
			m_httpReply.dwStatusCode = GetLastError();
			m_pfnLogCallback(m_dwCallbackFlags,GetCurrentHost(),GetBaseDirectory(),"error: unable to send the HTTP request (%ld)\n",m_httpReply.dwStatusCode);
			break;
        }

		// log degli headers HTTP della risposta (server)
        LogHttpHeaders(m_hRequest,HTTP_QUERY_RAW_HEADERS_CRLF,1);

		// azzera la lista per gli headers dal server
		CleanServerHttpHeaders();

		// ricarica la lista per gli headers dal server con quanto appena ricevuto
		LoadServerHttpHeaders();

		// verifica se il server ha risposto indicando lo stato della connessione (se la ha chiuso o lasciata aperta)
		pConnectionType = GetServerHttpHeader("Connection");
		//TRACEEXPR((_TRACE_FLAG_INFO,__FILE__,__LINE__,"DownloadUrl(): Connection: server said: %s\n",pConnectionType ? pConnectionType : "nothing"));
		if(!pConnectionType)
			pConnectionType = "";

		// ricava e legge la risposta del server
		DWORD dwHttpCode = 0L;
        DWORD dwBufSize = sizeof(dwHttpCode);
		if(!HttpQueryInfoA(m_hRequest,HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER,&dwHttpCode,&dwBufSize,NULL))
		{
            bOverallSuccess = FALSE;
			m_httpReply.dwStatusCode = GetLastError();
			m_pfnLogCallback(m_dwCallbackFlags,GetCurrentHost(),GetBaseDirectory(),"error: unable to get the HTTP status code (%ld)\n",m_httpReply.dwStatusCode);
            break;
        }

		// salva il codice di risposta HTTP del server (per il chiamante)
		m_httpReply.dwHttpCode = dwHttpCode;

		m_pfnLogCallback(m_dwCallbackFlags,GetCurrentHost(),GetBaseDirectory(),"HTTP code (server reply) is %ld\n",m_httpReply.dwHttpCode);

// risposte HTTP del server

		/*
		300-399:
			questa classe di codici indica che il client deve compiere un'ulteriore azione per completare la richiesta
			300 Multiple Choices
			301 Moved Permanently: La risorsa richiesta e' stata spostata in modo permanente a una nuova url.
			302 Found: La risorsa e' stata spostata temporaneamente a un altra url. Il client dovrebbe continuare a usare l'url originale per le richieste future.
			303 See Other
			304 Not Modified: La risorsa non e' stata modificata dall'ultima volta che il client l'ha richiesta. Il client deve usare la versione che ha già nella sua cache. Questo codice non ha un corpo di risposta.
			305 Use Proxy
			306 Switch Proxy
			307 Temporary Redirect
			308 Permanent Redirect
		*/
// 304
        if(dwHttpCode==304)
		{
			m_pfnLogCallback(m_dwCallbackFlags,GetCurrentHost(),GetBaseDirectory(),"HTTP 304 received, not modified...\n");
            bOverallSuccess = TRUE; 

			//$ cerca la url per ricavare la Content-Type: del file per rigenerare il nome, ma perche' deve
			// rigenerarlo se gia lo tiene nella tabella a fronte della url??? VERIFICARE CON TRACEEXPR()
			char szFilename[_MAX_FILEPATH+1] = {0};
			char szLastModified[MAX_DATE_STRING+1] = {0};
			char szETag[ETAG_HASH_LEN+1] = {0};
			char szContentType[HTTPHEADER_VALUE_LEN+1] = {0};
			DWORD dwFilesize = 0L;
			BOOL bUrlExists = GetFileByUrl(lpszUrl,szFilename,sizeof(szFilename),szLastModified,sizeof(szLastModified),szETag,sizeof(szETag),szContentType,sizeof(szContentType),dwFilesize);

			// toppa grande
			if(strempty(szContentType))
			{
				ASSERTEXPR(szContentType[0]!=' ');
				strcpy(szContentType,"text/html");
			}

            char szOutputFilename[MAX_PATH_LEN];
            if(!ComposeFilenameFromUrl(pszCurrentUrl,szOutputFilename,sizeof(szOutputFilename),szContentType))
			{
                bOverallSuccess = FALSE;
				m_httpReply.dwStatusCode = ERROR_INVALID_NAME;
				m_pfnLogCallback(m_dwCallbackFlags,GetCurrentHost(),GetBaseDirectory(),"error: unable to derive the file name from the Content-Type: (%ld)\n",m_httpReply.dwStatusCode);
                break; 
            }

			// situa il nome file nella directory base (di output)
            char szFullOutputPath[MAX_PATH_LEN] = {0};
            if(PathCombineA(szFullOutputPath,m_szBaseDirectory,szOutputFilename)==NULL)
			{
                bOverallSuccess = FALSE; 
				m_httpReply.dwStatusCode = GetLastError();
				m_pfnLogCallback(m_dwCallbackFlags,GetCurrentHost(),GetBaseDirectory(),"error: unable to compose the pathname for the filename (%ld)\n",m_httpReply.dwStatusCode);
                break; 
            }

			// imposta il nome del file di output (derivato sopra dalla url) per il chiamante
			strcpyn(lpszOutputfile,szFullOutputPath,nSize);
			m_pfnLogCallback(m_dwCallbackFlags,GetCurrentHost(),GetBaseDirectory(),"local file name will be %s\n",szFullOutputPath);

			m_httpReply.dwStatusCode = ERROR_SUCCESS;
			m_pfnLogCallback(m_dwCallbackFlags,GetCurrentHost(),GetBaseDirectory(),"download not required\n");

			// Cookies
			if(m_bUseCookies)
				HandleReceivedCookies(pszCurrentUrl);
		}
// 300 - 399
		else if(dwHttpCode >= 300 && dwHttpCode < 400)
		{
            if(++nCurrentRedirects > MAX_REDIRECTS)
			{
                bOverallSuccess = FALSE;
				m_httpReply.dwStatusCode = ERROR_HTTP_REDIRECT_FAILED;
				m_pfnLogCallback(m_dwCallbackFlags,GetCurrentHost(),GetBaseDirectory(),"error: too many redirections (%ld)\n",m_httpReply.dwStatusCode);
                break;
            }

			// re-imposta la url per provare a seguire il reindirizzamento
			char szRedirectUrl[INTERNET_MAX_URL_LENGTH] = {0};
            dwBufSize = sizeof(szRedirectUrl);
            if(HttpQueryInfoA(m_hRequest,HTTP_QUERY_LOCATION,szRedirectUrl,&dwBufSize,NULL))
			{
				m_pfnLogCallback(m_dwCallbackFlags,GetCurrentHost(),GetBaseDirectory(),"redirecting to %s (%ld of %d)\n",szRedirectUrl,nCurrentRedirects,MAX_REDIRECTS);
                
                free(pszCurrentUrl);
                pszCurrentUrl = _strdup(szRedirectUrl);
                if(!pszCurrentUrl)
				{
                    bOverallSuccess = FALSE;
					m_httpReply.dwStatusCode = ERROR_NOT_ENOUGH_MEMORY;
					m_pfnLogCallback(m_dwCallbackFlags,GetCurrentHost(),GetBaseDirectory(),"error: not enough memory (%ld)\n",m_httpReply.dwStatusCode);
                    break;
                }

				// nuovo tentativo
                continue;
            }
			else
			{
                bOverallSuccess = FALSE;
				m_httpReply.dwStatusCode = GetLastError();
				m_pfnLogCallback(m_dwCallbackFlags,GetCurrentHost(),GetBaseDirectory(),"error: unable to get the redirection url (%ld)\n",m_httpReply.dwStatusCode);
                break;
            }
        }
		/*
		200-299:
			questa classe di codici indica che la richiesta del client e' stata ricevuta, compresa e accettata con successo dal server.
			I piu' comuni sono:
			200 OK: La richiesta e' stata gestita correttamente e la risposta contiene la risorsa richiesta.
			201 Created: La richiesta e' stata soddisfatta e, come risultato, e' stata creata una nuova risorsa. Usato in risposta a richieste POST.
			202 Accepted: La richiesta e' stata accettata per l'elaborazione, ma l'operazione non e' ancora stata completata. Il risultato potrebbe non essere disponibile immediatamente.
			203 Non-Authoritative Information: Il server e' un proxy che ha ricevuto una risposta 200 OK da un altro server, ma sta restituendo una copia locale modificata o non autoritativa della risorsa.
			204 No Content: La richiesta e' stata elaborata con successo, ma il server non ha alcun contenuto da restituire nel corpo della risposta. Usato per richieste PUT o DELETE.
			205 Reset Content: Il server ha completato la richiesta e chiede al client di resettare la vista del documento, ad esempio per svuotare un modulo.
			206 Partial Content: Il server sta inviando solo una porzione della risorsa richiesta, come specificato dall'header Range nella richiesta del client. Utile per ripristinare i download interrotti.
			207 Multi-Status (WebDAV): La risposta contiene un corpo in formato XML che fornisce informazioni sullo stato per più operazioni.
			208 Already Reported (WebDAV): Un membro di un'operazione e' già stato elencato in una parte precedente della risposta.
			226 IM Used: Il server ha completato una richiesta GET e la risposta e' una rappresentazione della risorsa che incorpora manipolazioni di istanza.
		*/
// 200, 203, 206, 207, 226
		else if(dwHttpCode==200 || dwHttpCode==203 || dwHttpCode==206 || dwHttpCode==207 || dwHttpCode==226)
		{
			m_pfnLogCallback(m_dwCallbackFlags,GetCurrentHost(),GetBaseDirectory(),"HTTP 2xx received, getting data...\n");
            bOverallSuccess = TRUE; 

            // re-deriva il nome del file usando per l'estensione quanto specificato dal server nell'header Content-Type:
            char szOutputFilename[MAX_PATH_LEN];
			const char* pContentType = GetServerHttpHeader("Content-Type");
            if(!ComposeFilenameFromUrl(pszCurrentUrl,szOutputFilename,sizeof(szOutputFilename),pContentType))
			{
                bOverallSuccess = FALSE;
				m_httpReply.dwStatusCode = ERROR_INVALID_NAME;
				m_pfnLogCallback(m_dwCallbackFlags,GetCurrentHost(),GetBaseDirectory(),"error: unable to derive the file name from the Content-Type (%ld)\n",m_httpReply.dwStatusCode);
                break; 
            }

			// situa il nome file nella directory base (di output)
            char szFullOutputPath[MAX_PATH_LEN];
			if(PathCombineA(szFullOutputPath,m_szBaseDirectory,szOutputFilename)==NULL)
			{
                bOverallSuccess = FALSE; 
				m_httpReply.dwStatusCode = GetLastError();
				m_pfnLogCallback(m_dwCallbackFlags,GetCurrentHost(),GetBaseDirectory(),"error: unable to compose the pathname for the filename (%ld)\n",m_httpReply.dwStatusCode);
                break; 
            }

			// sopra ha appena ricavato il nome (completo di pathname) per il file di output (la risorsa da scaricare)
			// qui da la possibilita' al chiamante di modificare tale nome prima di usarlo per creare la copia in locale
			// della risorsa (il chiamante deve restituire TRUE per effettuare il cambio, controlla il buffer di output)
			if(m_pfnNameRulerCallback)
			{
				char szModifiedName[_MAX_PATH+1] = {0};
				if(m_pfnNameRulerCallback(szFullOutputPath,szModifiedName,sizeof(szModifiedName),m_pNameRulerContext) && *szModifiedName)
					strcpyn(szFullOutputPath,szModifiedName,sizeof(szFullOutputPath));
			}

			// imposta il nome del file di output (derivato sopra dalla url) per il chiamante
			strcpyn(lpszOutputfile,szFullOutputPath,nSize);

			// verifica se deve scaricare o meno la risorsa, controllando se sovrascrivere o meno il file eventualmente gia' esistente
			UINT nHttpSkippedCode = 0;
			BOOL bSkip = SkipDownloadByOverwriteFlag(szFullOutputPath,sizeof(szFullOutputPath),nHttpSkippedCode);

			// se richiesto dal chiamante, salva il nome che ha generato sopra per non sovrascrivere il file gia' esistente
			// occhio, se viene passato il parametro (lpszNonCollidingFileName), in esso viene comunque copiato il nome del 
			// file scaricato, originale o l'auto generato non-colliding 
			if(lpszNonCollidingFileName && nNonCollidingSize!=(UINT)-1)
				strcpyn(lpszNonCollidingFileName,szFullOutputPath,nNonCollidingSize);

			// download della risorsa (per overwrite o generazione nuovo nome)
			if(!bSkip)
			{
				m_pfnLogCallback(m_dwCallbackFlags,GetCurrentHost(),GetBaseDirectory(),"local file name will be %s\n",szFullOutputPath);

				// crea il file di output per la risorsa richiesta sopra
				FILE* pOutputFile = NULL;
				if(fopen_s(&pOutputFile,szFullOutputPath,"wb")!=0 || pOutputFile==NULL)
				{
					bOverallSuccess = FALSE; 
    				m_httpReply.dwStatusCode = errno;
					m_pfnLogCallback(m_dwCallbackFlags,GetCurrentHost(),GetBaseDirectory(),"error: unable to create file: %s (%d)\n",szFullOutputPath,m_httpReply.dwStatusCode);
					break; 
				}

				char readBuffer[READ_BUFFER_SIZE+1] = {0};
				DWORD dwBytesRead = 0L;
				QWORD qwTotalBytesDownloaded = 0L; 

				// riceve i dati della risorsa richiesta sopra
				while(InternetReadFile(m_hRequest,readBuffer,sizeof(readBuffer),&dwBytesRead) && dwBytesRead > 0)
				{
					if(fwrite(readBuffer,1,dwBytesRead,pOutputFile)!=dwBytesRead)
					{
						bOverallSuccess = FALSE;
	    				m_httpReply.dwStatusCode = errno;
						m_pfnLogCallback(m_dwCallbackFlags,GetCurrentHost(),GetBaseDirectory(),"error: unable to write data to file (%d)\n",m_httpReply.dwStatusCode);
						break; 
					}
					qwTotalBytesDownloaded += dwBytesRead;
				}
				
				fclose(pOutputFile);

				if(bOverallSuccess) // bOverallSuccess: errore(sopra)/successo(qui) scrittura
				{
					if(GetLastError()!=ERROR_SUCCESS && dwBytesRead==0) // errore ricezione
					{
						m_httpReply.dwStatusCode = GetLastError(); // ERROR_MORE_DATA;
						m_pfnLogCallback(m_dwCallbackFlags,GetCurrentHost(),GetBaseDirectory(),"error: receiving from server (%ld)\n",m_httpReply.dwStatusCode);
					}
					else
					{
						m_httpReply.dwStatusCode = ERROR_SUCCESS;
						m_pfnLogCallback(m_dwCallbackFlags,GetCurrentHost(),GetBaseDirectory(),"download successful, total bytes received %lld\n",qwTotalBytesDownloaded);
					}
				}

				// se il server ha inviato la risorsa in formato compresso in seguito all'header Accept-encoding: del client,
				// allora decomprime la risorsa in modo trasparente per il chiamante
				const char* pEncoding = GetServerHttpHeader("Content-encoding");
				if(pEncoding)
				{
					CContentDecoder decoder;
					ENCODEDFORMAT fmt;
					if(!decoder.DecodeFile(szFullOutputPath,pEncoding,fmt)) // supporta: gzip, deflate, br
					{
						m_httpReply.dwStatusCode = ERROR_INVALID_DATA;
						m_pfnLogCallback(m_dwCallbackFlags,GetCurrentHost(),GetBaseDirectory(),"error: unable to decode %s (%ld)\n",szFullOutputPath,m_httpReply.dwStatusCode);
					}
					else
					{
						m_pfnLogCallback(m_dwCallbackFlags,GetCurrentHost(),GetBaseDirectory(),"%s decoded from %s format\n",szFullOutputPath,fmt==DEFLATE_ENCODED_FORMAT ? "deflate" : (fmt==GZIP_ENCODED_FORMAT ? "gzip" : (fmt==BROTLI_ENCODED_FORMAT ? "brotli" : "unknown")));
					}
				}

				// aggiorna la data di ultima modifica del file ricevuto con quella mandata dal server con l'header Last-Modified
				UpdateFileDateTime(szFullOutputPath);

				// memorizza la url relativa al file passando il pathname completo
				PutFileByUrl(pszCurrentUrl,szFullOutputPath);

				// statistica per Content-Type:
				ContentTypeStats();
			}
#if 0
			else
				m_httpReply.dwHttpCode = nHttpSkippedCode;
#else
			if(nHttpSkippedCode!=0)
				m_httpReply.dwHttpCode = nHttpSkippedCode;
#endif

			// Cookies
			if(m_bUseCookies)
				HandleReceivedCookies(pszCurrentUrl);
        }
// 2xx rimanenti
		else if((dwHttpCode >= 200 && dwHttpCode < 300) && (dwHttpCode!=200 && dwHttpCode!=203 && dwHttpCode!=206 && dwHttpCode!=207 && dwHttpCode!=226))
		{
            bOverallSuccess = FALSE;
	    	m_httpReply.dwHttpCode = dwHttpCode;
			m_pfnLogCallback(m_dwCallbackFlags,GetCurrentHost(),GetBaseDirectory(),"error: unhandled HTTP code (%ld)\n",m_httpReply.dwHttpCode);
		}
		/*
		400-599:
			questa classe di codici indica che c'e' stato un errore del client :
			400 Bad Request: Il server non puo' o non vuole elaborare la richiesta a causa di un errore di sintassi.
			401 Unauthorized: La richiesta non ha le credenziali di autenticazione valide per la risorsa. Questo significa che il client deve prima autenticarsi.
			403 Forbidden: Il server ha capito la richiesta, ma si rifiuta di autorizzarla. A differenza del 401, in questo caso l'autenticazione non cambiera' il risultato.
			404 Not Found: Il server non ha trovato la risorsa richiesta.
			429 Too Many Requests: Il client ha inviato troppe richieste in un determinato lasso di tempo. Viene usato per il rate-limiting.

			questa classe di codici indica che il server non e' riuscito a soddisfare una richiesta apparentemente valida:
			500 Internal Server Error: Un errore generico che indica che qualcosa e' andato storto sul server e non c'e' un codice di errore piu' specifico disponibile.
			502 Bad Gateway: Un server che funge da gateway o proxy ha ricevuto una risposta non valida da un altro server a monte.
			503 Service Unavailable: Il server non e' in grado di gestire la richiesta al momento, spesso a causa di un sovraccarico o di manutenzione.

			Note:
			al ricevere un codice 403 Forbidden o 503 Service Unavailable:
			verificare se l'header del server contiene la stringa "cloudflare" e cercare l'header cf-mitigated, se il suo valore e' "challenge" significa prova non superata

			Header Specifici per i Challenge di Cloudflare:
			cf-mitigated: challenge: Questo e' il segnale più diretto. Indica esplicitamente che Cloudflare ha rilevato un comportamento sospetto e ha inviato un challenge al client.
			Server: cloudflare: La presenza di questo header, unita a un codice di stato 403 Forbidden o 503 Service Unavailable, e' un forte indizio dell'analisi di Cloudflare.
			Server-Timing: In alcuni casi, Cloudflare puo' usare questo header per fornire dettagli sulla gestione del challenge, ad esempio con valori come chlray (abbreviazione di "Cloudflare Ray ID") che indicano una risposta basata su un challenge.
			cf-ray: Questo header e' un identificatore univoco della richiesta per Cloudflare e, se combinato con un 403 Forbidden o 503 Service Unavailable, può indicare un blocco o un challenge.
			Expect-CT: A volte Cloudflare può usare questo o altri header di sicurezza per applicare politiche che, se non rispettate, possono innescare un challenge.

			Header e Segnali Generici:
			WWW-Authenticate: Questo header e' tipicamente associato ai codici di stato 401 Unauthorized e 407 Proxy Authentication Required, ma a volte i sistemi di sicurezza lo usano per richiedere una "challenge" al client. Questo header con un codice 403 o 503, potrebbe essere un segnale di un blocco.
			Retry-After: Questo header indica al client di riprovare la richiesta dopo un certo periodo di tempo. Un server può inviare un codice di stato 503 Service Unavailable insieme a questo header per dirti che il servizio è temporaneamente non disponibile, ma può anche essere parte di una strategia per rallentare o bloccare i bot.
			Content-Type: text/html con un corpo strano: Quando un bot viene bloccato, il server spesso risponde con una pagina HTML. Al ricevere un 403 o 503, se il corpo della risposta contiene del testo che non e' la pagina che ci si aspettava, ma un messaggio di errore, un CAPTCHA o un codice JavaScript, e' un forte indizio di un blocco di sicurezza.

			Altri Segnali di un Blocco:
			401 Unauthorized: Questo codice indica che la richiesta non ha le credenziali di autenticazione valide. Anche se il bot non sta cercando di accedere a un'area protetta da password, alcuni sistemi di sicurezza possono usare questo codice per bloccare i bot che non inviano un cookie o un token di autenticazione.
			429 Too Many Requests: Questo codice e' un chiaro segnale di un blocco basato sul rate-limiting. Indica che il bot ha inviato troppe richieste in un breve lasso di tempo. I server usano questo per prevenire lo scraping aggressivo o gli attacchi DDoS.
			Corpo della Risposta Anomalo: Anche senza un codice 403 o 503, un server puo' rispondere con un 200 OK ma con un corpo della risposta che non corrisponde a cio' che ci si aspetta. Ad esempio una pagina HTML che contiene un messaggio di errore anziche' il contenuto reale, un redirect a una pagina di login o a una pagina di "verifica umana".
			Header X-Bypass-Challenge o Simili: Alcuni sistemi di sicurezza personalizzati possono usare header non standard per comunicare lo stato del challenge. Questi header sono meno comuni, ma se stai analizzando un sito specifico, potrebbero essere un segnale utile.

			Per un bot, la logica di rilevamento del blocco dovrebbe essere un'analisi in due fasi:
			Fase 1: Codici di Stato: Controllare i codici 403, 406, 429 e 503.
			Fase 2: Header e Contenuto: In caso di fallimento, analizzare gli header ed il corpo della risposta per trovare indizi specifici, come l'header cf-mitigated: challenge o una pagina HTML con un CAPTCHA, per identificare il motivo esatto del blocco.
		*/
// 400 - 599
		else
		{
            bOverallSuccess = FALSE;
			m_httpReply.dwHttpCode = dwHttpCode;
			m_pfnLogCallback(m_dwCallbackFlags,GetCurrentHost(),GetBaseDirectory(),"error: HTTP error (%ld)\n",m_httpReply.dwHttpCode);
        }

		break;

    } // while(1)

    // handle per la richiesta (GET)
	if(m_hRequest)
	{
        InternetCloseHandle(m_hRequest);
		m_hRequest = NULL;
		m_pfnLogCallback(m_dwCallbackFlags,GetCurrentHost(),GetBaseDirectory(),"HTTP request closed\n");
    }
	// handle per la connessione: close/keep-alive
	if(m_nConnectionType==HTTP_CONNECTION_CLOSE || stricmp(pConnectionType,"close")==0)
		if(m_hConnect)
		{
			InternetCloseHandle(m_hConnect);
			m_hConnect = NULL;
			m_pfnLogCallback(m_dwCallbackFlags,GetCurrentHost(),GetBaseDirectory(),"Internet connection closed\n");
			//TRACEEXPR((_TRACE_FLAG_INFO,__FILE__,__LINE__,"DownloadUrl(): Connection closed\n"));
		}

    if(pszCurrentUrl)
        free(pszCurrentUrl); 

    return(bOverallSuccess);
}

/*
	EnableHttpVersioning()

	Per modificare la versione HTTP da usare con la richiesta (GET) non basta chiamare InternetSetOption(), ma bisogna 
	assicurarsi che il flag 'EnableHttp1_1' di Internet Explorer nel registro sia impostato a 0, nel caso fosse impostato 
	a 1 i tentativi di modificare la versione HTTP falliranno.

	'EnableHttp1_1' e' presente in 
		- Equipo\HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Internet Explorer\AdvancedOptions\HTTP\GENABLE
		- Equipo\HKEY_CURRENT_USER\SOFTWARE\Microsoft\Windows\CurrentVersion\Internet Settings
	e delle due, la chiave da modificare e' la seconda, quindi il codice modifica prima la chiave e chiama dopo la funzione 
	per impostare la versione HTTP da usare

	notare che bisogna seguire questa esatta sequenza di chiamate affinche InternetSetOptions() funzioni:
		InternetConnectA: apre la connessione all'host
		HttpOpenRequestA: crea l'handle della richiesta HTTP
		-> InternetSetOption: imposta l'opzione del protocollo sull'handle della richiesta
		HttpSendRequestA: invia la richiesta HTTP
*/
BOOL CHttpDownloader::EnableHttpVersioning(void)
{
	BOOL bSucceed = FALSE;
	CRegistryKey registry;

	// accede al registro
	LONG lRet = registry.Open(HKEY_CURRENT_USER,"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Internet Settings");
	if(lRet==ERROR_SUCCESS)
	{
		// verifica la chiave
		DWORD dwEnable = 0L;
		lRet = registry.QueryValue(dwEnable,"EnableHttp1_1");
		if(lRet==ERROR_SUCCESS)
		{
			// imposta il flag a seconda della versione HTTP da abilitare
			if(IsHTTPVersion(1,0))
				dwEnable = 0L;
			else if(IsHTTPVersion(1,1))
				dwEnable = 1L;

			// aggiorna la chiave
			lRet = registry.SetValue(dwEnable,"EnableHttp1_1");
		}
		registry.Close();

		//TRACEEXPR((_TRACE_FLAG_INFO,__FILE__,__LINE__,"EnableHttpVersioning(): flag: %d, result: %ld\n",dwEnable,lRet));

		// comunica all'API che ha cambiato il valore
		DWORD dwError = 0L;
		if(!InternetSetOption(m_hRequest,INTERNET_OPTION_HTTP_VERSION,&m_httpVersion,sizeof(m_httpVersion)))
			dwError = GetLastError();

		if(lRet==ERROR_SUCCESS && dwError==0L)
			bSucceed = TRUE;
	}

	// comunica all'API che sono cambiati i valori per la configurazione
	if(bSucceed)
	{
		HINTERNET hInternet = 0;
		BOOL bResult = InternetSetOption(hInternet,INTERNET_OPTION_SETTINGS_CHANGED,NULL,0);
		//TRACEEXPR((_TRACE_FLAG_INFO,__FILE__,__LINE__,"EnableHttpVersioning(): INTERNET_OPTION_SETTINGS_CHANGED: %s\n",bResult ? "OK" : "failure"));
	}

	return(bSucceed);
}

/*
	GetFileByUrl()

	Invoca la callback fornita dal chiamante per ricavare i dati relativi alla risorsa (Last Modified:, ETag: e Content-Type:).
	La chiave di ricerca per la risorsa e' la url completa.
	Restituisce TRUE se riesce o FALSE altrimenti.
*/
BOOL CHttpDownloader::GetFileByUrl(LPCSTR lpcszUrl,LPSTR szFilename,UINT nFilename,LPSTR szLastModified,UINT nLastModified,LPSTR szETag,UINT nETag,LPSTR szContentType,UINT nContentType,DWORD& dwFilesize)
{
	BOOL bSuccess = FALSE;

	if(m_pfnDatabaseCallback)
	{
		DBCALLBACK dbcb = {0};
		dbcb.action = DBCALLBACK_GET;
		strcpyn(dbcb.url,lpcszUrl,sizeof(dbcb.url));
	
		if((bSuccess = m_pfnDatabaseCallback(&dbcb))==TRUE)
		{
			// formatta, nel campo per il nome, dimensione file + nome, per distinguere 
			// poi, in base alla dimensione, le risorse che il server nomina con lo stesso 
			// nome, nonostante abbiano una risoluzione diversa
			char filesize[FILESIZE_MAX_LEN+1] = {0};
			strcpyn(szFilename,dbcb.file+FILESIZE_MAX_LEN,nFilename);
			memcpy(filesize,dbcb.file,FILESIZE_MAX_LEN);
			dwFilesize = atol(filesize);

			strcpyn(szLastModified,dbcb.date,nLastModified);
			strcpyn(szETag,dbcb.etag,nETag);
			strcpyn(szContentType,dbcb.type,nContentType);
		}
	}
	
	return(bSuccess);
}

/*
	PutFileByUrl()

	Invoca la callback fornita dal chiamante per registrare i dati relativi alla risorsa (Last Modified:, ETag: e Content-Type:).
	La chiave di ricerca per l'inserimento e' la url completa.
	Restituisce TRUE se riesce o FALSE altrimenti.
*/
BOOL CHttpDownloader::PutFileByUrl(LPCSTR lpcszUrl,LPCSTR lpcszFullpathFilename)
{
	BOOL bSuccess = FALSE;

	if(m_pfnDatabaseCallback)
	{
		/*
		ricava la dimensione del file (scaricato) su disco e la dimensione indicata dal server
		devono coincidere, pero' tenere presente che il server puo' omettere l'invio dell'header (a volte a 
		malaleche, come in "Content-Lenght: 0"), in tal caso assume la dimensione del file su disco
		dimensione file + url vengono poi formattate nel campo per la url per poter poi distinguere, nella
		fase successiva, le risorse che il server nomina con lo stesso nome nonostante risoluzione differente
		*/
		DWORD dwFileSize = (DWORD)GetFileSizeExtbyName(lpcszFullpathFilename);
		const char* pContentLenght = GetServerHttpHeader("Content-Lenght");
		DWORD dwContentSize = pContentLenght ? atol(pContentLenght) : 0L;
		// il server non ha mandato l'header, assume la dimensione del file su disco
		if(dwContentSize==0L)
			dwContentSize = dwFileSize;
		// se non combaciano, assume quella del file su disco
		if(dwContentSize!=dwFileSize)
			dwContentSize = dwFileSize;

		// ricava dal server la data/ora di ultima modifica, che potrebbe NON essere presente
		const char* pLastModified = GetServerHttpHeader("Last-Modified");

		// ricava dal server l'ETag, che potrebbe NON essere presente
		const char* pETag = GetServerHttpHeader("ETag");

		/*
		ricava dal server la content type, che potrebbe NON essere presente
		il server NON invia l'header Content-Type: quando la risposta non ha un corpo (body) o quando il client 
		NON ha bisogno che gli venga specificato il tipo di contenuto:

		risposte di stato che non contengono un corpo:
		204 No Content: Indica che la richiesta e' stata eseguita con successo ma non c'e' contenuto da restituire. Il body della risposta e' vuoto.
		304 Not Modified: Significa che la risorsa richiesta non e' cambiata rispetto all'ultima versione del client. Il client non ha bisogno di riscaricare i dati, quindi il server non invia ne' il body ne' gli header correlati come Content-Type:.
		301, 302, 303, 307, 308 (codici di reindirizzamento): In queste risposte, il body e' tipicamente vuoto o contiene un messaggio HTML minimo, ma il focus e' l'header Location: che indica al client dove andare. L'header Content-Type: e' quindi superfluo.
		4xx (errori del client) e 5xx (errori del server): Il server non invia quasi mai un Content-Type: in queste risposte, dato che l'obiettivo non e' fornire una risorsa ma informare il client che c'e' stato un problema. L'eccezione si ha quando il server invia un body con una pagina di errore in HTML, nel qual caso l'header Content-Type: text/html e' presente per informare il client di come interpretare il messaggio di errore.
		*/
		const char* pContentType = GetServerHttpHeader("Content-Type");
	
		DBCALLBACK dbcb = {0};
		dbcb.action = DBCALLBACK_PUT;
		strcpyn(dbcb.url,lpcszUrl,sizeof(dbcb.url));

		// formatta dimensione file + url nello stesso campo
		snprintf(	dbcb.file,
					sizeof(dbcb.file),
					"%-"\
					STR(FILESIZE_MAX_LEN)\
					"ld%s",
					dwContentSize,
					StripPathFromFile(lpcszFullpathFilename)
					);

		if(pLastModified)
			strcpyn(dbcb.date,pLastModified,sizeof(dbcb.date));
		else
		{
			// registrare la data di sistema quando il server non manda la Last-Modifed: 
			// e' NO-GOOD-AT-ALL, quindi in tal caso semplicemente non mandare l'header 
			// If-Modified-Since:
			//
			// CDateTime dateTime(GMT_SHORT,HHMMSS_GMT_SHORT);
			// strcpyn(dbcb.date,dateTime.GetFormattedDate(),sizeof(dbcb.date));
			;
		}
		if(pETag)
			strcpyn(dbcb.etag,pETag,sizeof(dbcb.etag));
	    if(pContentType)
		{
			strcpyn(dbcb.type,pContentType,sizeof(dbcb.type));
			// elimina quanto presente a partire da ';', come in: "application/javascript; charset=utf-8"
			char* p = strchr(dbcb.type,';');
			if(p)
				*p = '\0';
			strrtrim(dbcb.type);
		}

		bSuccess = m_pfnDatabaseCallback(&dbcb);
	}
	
	return(bSuccess);
}

/*
	ContentTypeStats()

	Invoca la callback fornita dal chiamante per registrare la statistica per la Content-Type:.
*/
void CHttpDownloader::ContentTypeStats(void)
{
	if(m_pfnDatabaseCallback)
	{
		const char* pContentType = GetServerHttpHeader("Content-Type");
	
		if(pContentType)
		{
			DBCALLBACK dbcb = {0};
			dbcb.action = DBCALLBACK_STAT;
			strcpyn(dbcb.type,pContentType,sizeof(dbcb.type));
		
			// elimina quanto presente a partire da ';', come in: "application/javascript; charset=utf-8"
			char* p = strchr(dbcb.type,';');
			if(p)
				*p = '\0';
			strrtrim(dbcb.type);
		
			m_pfnDatabaseCallback(&dbcb);
		}
	}
}

/*
	AddHttpHeadersToRequest()

	Ricorre la lista interna degli headers del client creata con AddClientHttpHeader() 
	e li aggiunge alla richiesta HTTP prima di mandarla con la GET.

	Gestisce, in un modo o nell'altro, gli headers:
	- Connection:
	- If-Modified-Since:
	- If-None-Match: (ETag)
	- Referer: (caso speciale)

	OCCHIO: il bot deve ri-mandare al server il contenuto dell'ETag solo nel caso stia
	emulando un user-agent HTTP/1.1, per i preistorici, con HTTP/1.0, l'ETag, anche se
	presente nella tabella per anteriori download con ultra moderni, NON deve essere 
	inviato.
*/
void CHttpDownloader::AddHttpHeadersToRequest(LPSTR lpszUrl)
{
	HTTPHEADER* h;
	ITERATOR iter;
	BOOL bHasCustomIfModifiedSince = FALSE;
	BOOL bHasCustomIfNoneMatch = FALSE;
	BOOL bHasCustomIncludeReferer = FALSE;
	BOOL bIncludeThatHeader = TRUE;
	BOOL bUseUnixTimestamp = FALSE;

	// Connection: [...]
	// re-insifona l'header, per la zoccola di WinInet
	char szHeader[HTTPHEADER_VALUE_LEN+1] = {0};
	snprintf(szHeader,sizeof(szHeader),"Connection: %s\r\n",IIF(m_nConnectionType==HTTP_CONNECTION_CLOSE,"close","keep-alive"));
	HttpAddRequestHeadersA(m_hRequest,szHeader,(DWORD)-1,HTTP_ADDREQ_FLAG_ADD | HTTP_ADDREQ_FLAG_REPLACE);

	// If-Modified-Since: [...], If-None-Match: [...], Referer: [...]
	// aggiunge l'header specificato dal chiamante con AddClientHttpHeader() alla richiesta,
	// a meno che il valore sia "none" (stratagemma per non includerli)
	if((iter = m_clientHttpHeaderList.First())!=(ITERATOR)NULL)
	{
		char szHeader[HTTPHEADER_NAME_LEN+1 + HTTPHEADER_VALUE_LEN+1 + 1] = {0};

		do {
			h = (HTTPHEADER*)iter->data;
			if(h)
			{
				bIncludeThatHeader = TRUE;

				if(stricmp(h->name,"If-Modified-Since")==0)
				{
					bHasCustomIfModifiedSince = TRUE;
					bIncludeThatHeader = IIF(stricmp(h->value,"none")==0,FALSE,TRUE);

					// trucchetto per mandare comunque l'header (per non generare sospetti)
					// ma obbligando il server a farsi rimandare la risorsa in ogni modo
					if(stricmp(h->value,"Thu, 01 Jan 1970 00:00:00 GMT")==0)
					{
						// per farlo entrare nel blocco della if() una volta uscito dal questo while() per poter 
						// ricavare l'ETag
						// anche se sta' barando con la data, insiema a If-Modified-Since deve mandare comunque 
						// l'ETag...
						bHasCustomIfModifiedSince = FALSE;
						// ... e questa per non fargli mandare l'header perche' gia lo ha impostato il chiamante
						bUseUnixTimestamp = TRUE;
					}
				}
				else if(stricmp(h->name,"If-None-Match")==0)
				{
					bHasCustomIfNoneMatch = TRUE;
					bIncludeThatHeader = IIF(stricmp(h->value,"none")==0,FALSE,TRUE);
				}
				else if(stricmp(h->name,"Referer")==0)
				{
					bHasCustomIncludeReferer = TRUE;
					bIncludeThatHeader = IIF(stricmp(h->value,"none")==0,FALSE,TRUE);
				}

				if(bIncludeThatHeader)
				{
					snprintf(szHeader,sizeof(szHeader),"%s: %s\r\n",h->name,h->value);
					HttpAddRequestHeadersA(m_hRequest,szHeader,(DWORD)-1,HTTP_ADDREQ_FLAG_ADD | HTTP_ADDREQ_FLAG_REPLACE);
				}
			}
		}
		while((iter = m_clientHttpHeaderList.Next(iter))!=(ITERATOR)NULL);
	}

	// verifica se la risorsa richiesta con la url (ossia il file) e' gia' presente in locale per 
	// poter costruire gli headers If-Modified-Since: e If-None-Match:
	// verifica in due passi: 1) esistenza nella tabella e solo in seguito 2) esistenza su disco
	// anche se si esclude uno solo, allora li esclude entrambi, o vanno insieme o non vanno
	if(!bHasCustomIfModifiedSince && !bHasCustomIfNoneMatch)
	{
		char szLocalFilename[_MAX_FILEPATH+1] = {0};
		char szFilename[_MAX_FILEPATH+1] = {0};
		char szLastModified[MAX_DATE_STRING+1] = {0};
		char szETag[ETAG_HASH_LEN+1] = {0};
		char szContentType[HTTPHEADER_VALUE_LEN+1] = {0};
		DWORD dwFilesize = 0L;

		// cerca la url nella tabella per ricavare il nome file locale associato (nella forma originale o rinominata)
		BOOL bUrlExists = GetFileByUrl(lpszUrl,szFilename,sizeof(szFilename),szLastModified,sizeof(szLastModified),szETag,sizeof(szETag),szContentType,sizeof(szContentType),dwFilesize);

		// non esiste la url, quindi non esiste il file in locale, quindi salta gli headers If-Modified-Since/If-None-Match
		if(!bUrlExists)
			goto no_if_modified_since_if_none_match;

		// controlla se il file effettivamente esiste in locale, se non esiste salta gli headers If-Modified-Since/If-None-Match

		// OCCHIO: se il server usa lo stesso nome per una risorsa con differenti dimensioni, appena ne scarica una (qualsiasi)
		// il seguente controllo si invalida, perche' FileExists() restituira' TRUE solo basandosi sul nome, non sulla dimensione
		// bisognerebbe fare un check sulla dimensione, controllando che se il file esiste, la dimensione sia la stessa specificata
		// dal server per la risorsa richiesta
		snprintf(szLocalFilename,sizeof(szLocalFilename),"%s\\%s",m_szBaseDirectory,szFilename);

		// - se il file non esiste, non manda l'header e richiede il file
		// - se il file esiste, manda l'header solo se la dimensione del file su disco e' uguale a quella registrata
		// se la dimensione e' diversa e' perche il server usa lo stesso nome per una risorsa con risoluzioni differenti
		// in tal caso richiede nuovamente il file e dipende dalle opzioni del bot (-o, -O) se il file con dimensione
		// diversa ma con stesso nome viene o no sovrascritto
		if(!FileExists(szLocalFilename))
			goto no_if_modified_since_if_none_match;
		else
		{
			DWORD dwSize = (DWORD)GetFileSizeExtbyName(szLocalFilename);
			if(dwSize!=dwFilesize)
				goto no_if_modified_since_if_none_match;
		}

		// manda l'header solo se trova il valore relativo nella tabella:
		// se la data e' vuota perche' il server non la invio', NON deve impostare nessuna data NE' inviare 
		// l'header, in caso contrario romperebbe la logica del server relativamente alla gestione della risorsa
		//
		// se sopra ha barato con la data (bUseUnixTimestamp), NON costruisce il If-Modified-Since, dato che 
		// viene gia' impostato dal chiamante
		if(*szLastModified && !bUseUnixTimestamp)
		{
			// usa la data salvata in precedenza a partire dall'header Last-Modified: (es. Wed, 27 Aug 2025 16:10:03 GMT)
			char szDateHeader[HTTPHEADER_VALUE_LEN+1] = {0};
			snprintf(szDateHeader,sizeof(szDateHeader),"If-Modified-Since: %s\r\n",szLastModified);
			HttpAddRequestHeadersA(m_hRequest,szDateHeader,(DWORD)-1,HTTP_ADDREQ_FLAG_ADD | HTTP_ADDREQ_FLAG_REPLACE);
		}

		// manda l'header solo se e' presente il valore relativo (ETag) e l'user-agent emulato e' un HTTP/1.1
		if(*szETag && IsHTTPVersion(1,1))
		{
			char szETagHeader[HTTPHEADER_VALUE_LEN+1] = {0};
			snprintf(szETagHeader,sizeof(szETagHeader),"If-None-Match: %s\r\n",szETag);
			HttpAddRequestHeadersA(m_hRequest,szETagHeader,(DWORD)-1,HTTP_ADDREQ_FLAG_ADD | HTTP_ADDREQ_FLAG_REPLACE);
		}
	}

no_if_modified_since_if_none_match:

	// Referer: [...]
	// l'header Referer: viene trattato a parte: la classe si incarica di impostarlo con lo stesso host della 
	// richiesta (o con un valore di default se qualcosa e' andato male) solo se il chiamante non lo specifica
	// come regola generale: Referer: si lascia vuoto se la provenienza e' assoluta (ad es. l'utente inizia una 
	// navigazione da zero), mentre si riempie per indicare da dove proviene il riferimento alla risorsa, che 
	// deve essere la url completa dello stesso sito o di un sito differente, in tal caso, se si stanno usando
	// le Sec-Fetch, impostare il valore di Sec-Fetch-Site in modo congruente
	// in ogni caso, se arriva qui NON E' BUON SEGNO, il chiamante DEVE impostare un Referer: a seconda di cio'
	// che sta chiedendo al server e del contesto relativo
	if(!bHasCustomIncludeReferer)
	{
		if(strcmp(m_szHostForReferer,"")!=0)
		{
			// imposta il Referer: con quanto ricavato a inizio sessione
			char szReferer[HTTPHEADER_VALUE_LEN+1] = {"Referer: "};
			strcatn(szReferer,m_szHostForReferer,sizeof(szReferer));
			strcatn(szReferer,"\r\n",sizeof(szReferer));
			HttpAddRequestHeadersA(m_hRequest,szReferer,(DWORD)-1,HTTP_ADDREQ_FLAG_ADD | HTTP_ADDREQ_FLAG_REPLACE);
		}
		else // panico! imposta con fittizio
			HttpAddRequestHeadersA(m_hRequest,"Referer: https://www.google.com/\r\n",(DWORD)-1,HTTP_ADDREQ_FLAG_ADD | HTTP_ADDREQ_FLAG_REPLACE);
	}
}
#if 0
	// si mantiene qui come esempio: 
	// se la data non e' stata salvata, allora la ricava dal file
	if(!*szIfModifiedSince)
	{
		// l'header If-Modified-Since: richiede la data in formato GMT: "Wed, 13 Aug 2025 08:25:16 GMT"
		WORD wDate=0,wTime=0;

		// ricava la data del file in formato MS-DOS, la converte a Gregoriana e la ricostruisce 
		// in formato GMT, il tutto perche' il formato GMT richiede il giorno della settimana
		if(GetFileDateTime(szLocalFilename,&wDate,&wTime,NULL))
		{
			GREGORIANDATETIME g = {0}; //int year,month,day,dayofweek,hour,minute,second;
			g.wDate = wDate;
			g.wTime = wTime;
			FileDateTimeToGregorian(&g);
			CDateTime dateTime(GMT_SHORT,HHMMSS_GMT_SHORT,-1,g.nDay,g.nMonth,g.nYear,g.nHour,g.nMinute,g.nSecond);
			strcpyn(szIfModifiedSince,dateTime.GetFormattedDate(FALSE),sizeof(szIfModifiedSince));
		}
		else
		{
			CDateTime dateTime(GMT_SHORT,HHMMSS_GMT_SHORT);
			strcpyn(szIfModifiedSince,dateTime.GetFormattedDate(TRUE),sizeof(szIfModifiedSince));
		}
	}
#endif

/*
	LoadServerHttpHeaders()

	Carica la lista interna con gli headers HTTP ricevuti dal server.

*/
int CHttpDownloader::LoadServerHttpHeaders(void)
{
	int nTotHeaders = 0;

	// chiama con il parametro a 0 per forzare a farsi dire quanto spazio necessita
    DWORD dwBufSize = 0L;
    HttpQueryInfoA(m_hRequest,HTTP_QUERY_RAW_HEADERS_CRLF,NULL,&dwBufSize,NULL);

	if(GetLastError()==ERROR_INSUFFICIENT_BUFFER)
	{
		char* pszHeaders = (char*)calloc(dwBufSize,sizeof(char));
        if(pszHeaders)
		{
            if(HttpQueryInfoA(m_hRequest,HTTP_QUERY_RAW_HEADERS_CRLF,pszHeaders,&dwBufSize,NULL))
			{
				// per ogni riga dell'header
                char* context = NULL;
                char* line = strtok_s(pszHeaders,"\r\n",&context);
                while(line)
				{
					if(strchr(line,':'))
					{
						nTotHeaders++;
						ParseServerHeaderLine(line);
					}

					line = strtok_s(NULL,"\r\n",&context);
				}
			}
			
			free(pszHeaders);
        }
	}

	return(nTotHeaders);
}

/*
	CleanServerHttpHeaders()
*/
void CHttpDownloader::CleanServerHttpHeaders(void)
{
	m_httpReply.m_serverHttpHeaderList.DeleteAll();
}

/*
	ParseServerHeaderLine()

	Suddivide l'header HTTP ricevuto dal server nella coppia nome/valore.
*/
void CHttpDownloader::ParseServerHeaderLine(LPCSTR line)
{
	// l'header usa la sintassi: <nome>: <valore>[, <valore>[ ,<valore>[...]]]
	// salta gli headers malformati
	const char* colon = strchr(line,':');
	if(colon)
	{
		BOOL bFound = FALSE;
		ITERATOR iter;
		HTTPHEADER* h = NULL;
		char szName[HTTPHEADER_NAME_LEN+1] = {0};
		char szValue[HTTPHEADER_VALUE_LEN+1] = {0};

		// copia il nome dell'header (fino a ':')
		strcpync(szName,line,HTTPHEADER_NAME_LEN+1,':');
		// salta ':' e gli eventuali spazi iniziali
		colon++;
		while(isspace((unsigned char)*colon))
			colon++;
		// copia il valore (puo' essere uno o vari separati da virgola)
		strcpyn(szValue,colon,HTTPHEADER_VALUE_LEN+1);

		// cerca il nome dell'header nella lista, se lo trova ne aggiorna il valore aggiungendo ',' e 
		// concatenando il nuovo valore
		// A MENO CHE non si tratti dell'header Set-Cookie, nel qual caso gli headers vanno elencati
		// uno per uno e NON devono essere concatenati
		if(stricmp(szName,"Set-Cookie")!=0)
			if((iter = m_httpReply.m_serverHttpHeaderList.First())!=(ITERATOR)NULL)
			{
				do {
					h = (HTTPHEADER*)iter->data;
					if(h)
					{
						if(stricmp(szName,h->name)==0)
						{
							bFound = TRUE;
							strcatn(h->value,", ",HTTPHEADER_VALUE_LEN+1);
							strcatn(h->value,szValue,HTTPHEADER_VALUE_LEN+1);
							break;
						}
					}
				}
				while((iter = m_httpReply.m_serverHttpHeaderList.Next(iter))!=(ITERATOR)NULL);
			}
	
		// non l'ha trovato, lo inserisce
		if(!bFound)
		{
			HTTPHEADER* h = NULL;
			if((h = (HTTPHEADER*)m_httpReply.m_serverHttpHeaderList.Create())!=NULL)
			{
				m_httpReply.m_serverHttpHeaderList.Initialize(h);
				strcpyn(h->name,szName,HTTPHEADER_NAME_LEN+1);

				/* occhio perche' ci possono essere headers come Content-Security-Policy che raggiungono e superano i 4Kb
				qui la dimensione per il contenuto dell'header e' definita da HTTPHEADER_VALUE_LEN (attualmente 1024 bytes) */
				strcpyn(h->value,szValue,HTTPHEADER_VALUE_LEN+1);
				m_httpReply.m_serverHttpHeaderList.Add(h);
			}
		}
	}
}

/*
	SkipDownloadByOverwriteFlag()

	Usata da DownloadUrl(), determina se effettuare il download o meno (e se usare o no lo stesso nome di risorsa) a 
	seconda del flag per l'opzione overwrite.

	Vengono definite 4 opzioni:

	0=do not overwrite,
	1=overwrite,
	2=overwrite only if bigger (than local), skip if local bigger than remote
	4=skip if already exists (no matter anything else)

	in base al fatto che il server puo' gestire oggetti con 'risoluzione' (e quindi dimensione) differente pur usando
	lo stesso nome (ad es. immagini con lo stesso nome che risiedono in subdir differenti perche' con una risoluzione
	diversa).

	A seconda quindi del flag, il comportamento e' il seguente:

	0 = se la risorsa gia' esiste in locale, crea un nome con incremento numerico (default) per non sovrascriverla
	1 = sovrascrive la risorsa locale a prescindere da qualsiasi condizione
	2 = sovrascrive la risorsa locale solo se la dimensione e' minore rispetto a quella del server, ossia non sovrascrive
		se in locale la risorsa e' maggiore rispetto a quella del server
	4 = non sovrascrive la risorsa per il mero fatto di esistere gia' in locale (non considera data/ora, etag, etc.)

	Tenere presente che il server non sempre invia l'header Content-Lenght per poter fare il confronto relativo.

	Restituisce TRUE se deve saltare (omettere) il download della risorsa, FALSE altrimenti. Il terzo parametro viene
	usato nei casi in cui restituisce TRUE per poter specificare il motivo dello skip:
	211 - skipped because local file bigger than remote
	212 - skipped because already exists
	213 - overwritten
	214 - not overwritten

	codici nel registro IANA: 209 - 225 Unassigned
	possibili codici		: 304 - Not Modified, 210 - Object Already Exists (non ufficiale)
	codici usati per skip	: 211, 212, 213, 214
*/
BOOL CHttpDownloader::SkipDownloadByOverwriteFlag(LPSTR szResource,UINT nSize,UINT& nHttpSkippedCode)
{
	BOOL bSkip = FALSE;
	BOOL bOverwrite = FALSE;
	BOOL bYetAnotherName = FALSE;

	// verifica se la risorsa esiste in locale e nel caso ricava la dimensione
	BOOL bFileExists = FileExists(szResource);
	QWORD qwFilesize = 0L;
	if(bFileExists)
		qwFilesize = GetFileSizeExtbyName(szResource);

	// verifica se il server ha specificato la dimensione della risorsa
	QWORD qwResSize = 0L;
	const char* pResSize = GetServerHttpHeader("Content-Length");
	if(pResSize)
		qwResSize = atol(pResSize);

	nHttpSkippedCode = 0;

	// gestione del flag per l'overwrite
	if(m_nOverwrite==HTTP_FLAG_OVERWRITE) // overwrite
	{
		bSkip = FALSE;
		bOverwrite = TRUE;
		bYetAnotherName = FALSE;

		nHttpSkippedCode = 213;
	}
	else if(m_nOverwrite==HTTP_FLAG_OVWRIFBIG) // overwrite only if bigger, skip if smaller
	{
		// se il file gia' esiste su disco lo scarica solo se la dimensione in locale e' inferiore a quella del server
		if(bFileExists)
		{
			if(qwResSize > qwFilesize)
			{
				bSkip = FALSE;
				bOverwrite = TRUE;
				bYetAnotherName = FALSE;
			}
			else
			{
				bSkip = TRUE;
				bOverwrite = FALSE;
				bYetAnotherName = FALSE;
			}
		}
		else
		{
			bSkip = FALSE;
			bOverwrite = TRUE;
			bYetAnotherName = FALSE;
		}

		if(bSkip)
			nHttpSkippedCode = 211;
	}
	else if(m_nOverwrite==HTTP_FLAG_DONOTOVWR) // do not overwrite (proliferate)
	{
		if(bFileExists) // deve preservare l'esistente generando un nuovo nome
		{
			bSkip = FALSE;
			bOverwrite = FALSE;
			bYetAnotherName = TRUE;
		}
		else // il file non esiste, deve essere creato con il nome originale
		{
			bSkip = FALSE;
			bOverwrite = TRUE;
			bYetAnotherName = FALSE;
		}

		if(bYetAnotherName)
			nHttpSkippedCode = 214;
	}
	else if(m_nOverwrite==HTTP_FLAG_SKIPEXIST) // skip existing
	{
		if(bFileExists)
		{
			bSkip = TRUE;
			bOverwrite = FALSE;
			bYetAnotherName = FALSE;
		}
		else
		{
			bSkip = FALSE;
			bOverwrite = TRUE;
			bYetAnotherName = FALSE;
		}

		if(bSkip)
			nHttpSkippedCode = 212;
	}

	// genera, se necessario, un nuovo nome file incrementando numericamente l'originale per evitare la sovrascrittura
	if(!bOverwrite && bYetAnotherName)
	{
		char szNewName[_MAX_FILEPATH+1] = {0};
		char* pNewName = YetAnotherFileName(szResource,szNewName,sizeof(szNewName));
		if(pNewName!=NULL)
			strcpyn(szResource,pNewName,nSize);
	}

	return(bSkip);
}

/*
	CheckIfUrlOrfile()

	Usata da DownloadUrl() per verificare se e' stata effettivamente passata una url o un nome 
	file locale (es. un .html che risiede sul disco locale invece che su un server internet).
*/
int CHttpDownloader::CheckIfUrlOrfile(LPSTR lpszUrl,UINT nUrlSize,LPSTR lpszOutputfile,UINT nSize)
{
	int nRet = -1;

	// controlla se viene passato un nome file invece di una url (es. un file .html in locale)
	FILEURLPROBABILITY probability = get_file_url_probability(lpszUrl);
	
	// restituisce 0/1 se e' un file
	if(probability.file > probability.url)
	{
		char szFilename[_MAX_FILEPATH*2] = {0};
		
		// se il nome file ha caratteri non consentiti, li converte a percent-encoded
		MSDosPercentEncodeUnallowed(lpszUrl,szFilename,sizeof(szFilename));

		// la logica e': dato che il file gia' esiste su disco, semplicemente lo copia in quello che ci 
		// si aspetta sia il file di output, trasformandolo in una url, dato che il chiamante potrebbe 
		// necessitarla per usarla come parent-url
		strcpyn(lpszOutputfile,m_szBaseDirectory,nSize);
		RemoveBackslash(lpszOutputfile);
		strcatn(lpszOutputfile,"\\",nSize);
		strcatn(lpszOutputfile,StripPathFromFile(szFilename),nSize);

		// controlla che il file non si trovi gia' nella directory di destinazione, in tal caso la copia
		// di un file su se stesso creerebbe una copia tipo <file> + " - copia".<ext>
		BOOL bCopy = FALSE;
		nRet = 0;
		if(stricmp(szFilename,lpszOutputfile)!=0)
		{
			nRet = 0;
			bCopy = CopyFileTo(NULL,szFilename,lpszOutputfile,0,0);
		}
		else
		{
			nRet = 1;
			bCopy = TRUE;
		}

		if(bCopy)
		{
			char* p = MSDosPathToHttpUrl(szFilename);
			if(p)
			{
				strcpyn(lpszUrl,p,nUrlSize);
				free(p);
			}
		}
	}
	else if(probability.file==0 && probability.url==0)	// restituisce -1 se e' una stringa senza senso, malformata, ne' file ne' url, monnezza
	{
		nRet = -1;
	}
	else
		nRet = 2; // restituisce 2 se e' una url

	return(nRet);
}

/*
	ComposeFilenameFromUrl()

	Genera il nome file a partire dalla url.
	Notare che, per i casi in cui il server referenzia (nel file html) un tipo, ma poi manda la Content-Type 
	di un altro, rinomina il file aggiungendo (NON sostituiendo) l'estensione pertinente.
*/
BOOL CHttpDownloader::ComposeFilenameFromUrl(LPCSTR lpcszUrl,char* pszOutputFilenameBuffer,size_t bufferSize,LPCSTR pszContentType)
{
	//$ RENAME

	// ricava l'estensione da usare per il nome file in base alla Content-Type:
	// tener presente che puo' esistere piu' di una estensione con la stessa
	// Content-Type:, qui semplicemente ricava la prima in ordine di ricerca
	const char* pExt = GetExtensionFromContentType(pszContentType);

	// dei tre campi della struttura (file, name, ext) usa 'file', ossia il nome file completo

	// estrae il nome file dalla url
	char szFilename[_MAX_FILEPATH+1] = {0};
	URLDATA urldata = {0};
	strcpyn(urldata.url,lpcszUrl,sizeof(urldata.url));
	url_parse(&urldata);
	strcpyn(szFilename,urldata.file,sizeof(szFilename));

	// nessun nome file nella url, se e' presente una query prova ad estrarlo da li'
	if(!*szFilename)
	{
		if(*urldata.query)
		{
			char szQuery[MAX_URL_QUERY+1] = {0};
			strcpyn(szQuery,urldata.query,sizeof(szQuery));
			memset(&urldata,'\0',sizeof(urldata));
			strcpyn(urldata.url,szQuery,sizeof(urldata.url));
			url_parse(&urldata);
			strcpyn(szFilename,urldata.file,sizeof(szFilename));
		}
	}

	// ricava la famiglia mime relativa alla Content-Type: per le due fallback a seguire
	MIMEFAMILY mimefamily = GetMimeFamilyFromContentType(pszContentType);

	// senza nome file, imposta uno di default a seconda della famiglia mime
	if(!*szFilename)
		switch(mimefamily)
		{
			case FAMILY_HTML:
			case FAMILY_TEXT:
				strcpyn(szFilename,"index",sizeof(szFilename));
				break;
			case FAMILY_IMAGE:
				strcpyn(szFilename,"picture",sizeof(szFilename));
				break;
			case FAMILY_AUDIO:
				strcpyn(szFilename,"audio",sizeof(szFilename));
				break;
			case FAMILY_VIDEO:
				strcpyn(szFilename,"video",sizeof(szFilename));
				break;
			case FAMILY_APPLICATION:
				strcpyn(szFilename,"application",sizeof(szFilename));
				break;
			case FAMILY_FONT:
				strcpyn(szFilename,"font",sizeof(szFilename));
				break;
			case FAMILY_UNKNOWN:
			default:
				strcpyn(szFilename,"unknown",sizeof(szFilename));
				break;
		}

	// ultimo controllo prima di aggiungere l'estensione: se ha fallito sopra 
	// al ricavarla in base alla Content-Type:, qui ne usa una di rattoppo
	if(!pExt)
		switch(mimefamily)
		{
			case FAMILY_TEXT:
				pExt = ".txt";
				break;
			case FAMILY_IMAGE:
			case FAMILY_AUDIO:
			case FAMILY_VIDEO:
			case FAMILY_FONT:
				pExt = ".bin";
				break;
			case FAMILY_APPLICATION:
				pExt = ".app";
				break;
			default:
				pExt = ".dat";
				break;
		}
		
	// gestisce il caso in cui il server referenzia un tipo e manda la Content-Type di un altro,
	// ed il caso in cui, quando la risorsa ha piu' estensioni (es .jpg e .jpeg), l'estensione della 
	// risorsa (es .jpg) non coincide con quella che ha ricavato sopra a fronte della Content-Type: 
	// ricevuta dal server (es .jpeg)

	// non trova, nel nome file, l'estensione che ha ricavato sopra a fronte della Content-Type:
	if(!strrstr(szFilename,pExt))
	{
		// distingue:
		// - se il file ha un estensione, controlla che la Content-Type: di questa estensione
		// coincida con quella indicata dal server: se si, non aggiunge nulla, se no, aggiunge
		// l'estensione ricavata sopra a quella gia' esistente
		// - se il file non ha un estensione, semplicemente aggiunge quella ricavata sopra, 
		// relativa alla Content-Type: indicata dal server
		char* pExtFile = strrchr(szFilename,'.');
		if(pExtFile)
		{
			const char* pExtContent = GetContentTypeFromExtension(pExtFile);
			if(!pExtContent)
				pExtContent = "";
			if(stricmp(pExtContent,pszContentType)==0)
				pExt = NULL;
		}
		if(pExt)
		{
			if(!m_bReplaceFileExt)
				strcatn(szFilename,pExt,sizeof(szFilename));
			else
				ChangeFileExtension(szFilename,sizeof(szFilename),pExt);
		}
	}

	// copia il risultato finale nel nome file di output
	strcpyn(pszOutputFilenameBuffer,szFilename,bufferSize);

	// elimina gli eventuali caratteri non ammessi
	char unallowedchars[] = "<>:\"/\\|?*";
	char* p = encode_string_to_percent_encoded(pszOutputFilenameBuffer,unallowedchars);
	if(p)
	{
		strcpyn(pszOutputFilenameBuffer,p,bufferSize);
		free(p);
	}

	// TRACEEXPR((_TRACE_FLAG_INFO,__FILE__,__LINE__,"ComposeFilenameFromUrl():\nurl: %s\ncontent-type: %s\nfile: %s\n",lpcszUrl,DENULLIFY(pszContentType),pszOutputFilenameBuffer));

    return(TRUE);
}

/*
	HttpToFileDateTime()

	Converte la data inviata dal server con l'header Last-Modified: dal formato RFC 1123 
	(es. "Wed, 14 Aug 2025 08:30:00 GMT") a FILETIME (MS-DOS).
	Usata per aggiornare la data di ultima modifica della risorsa una volta scaricata in
	locale, vedi sotto.

	Restituisce TRUE se riesce o FALSE altrimenti.
*/
BOOL CHttpDownloader::HttpToFileDateTime(LPCSTR lpcszGMTDate,FILETIME* pFileTime)
{
    SYSTEMTIME st = {0};
	CDateTime dateTime(lpcszGMTDate,GMT_SHORT);
    st.wYear = dateTime.GetYear();
	st.wMonth = dateTime.GetMonth();
	st.wDay = dateTime.GetDay();
	st.wHour = dateTime.GetHour();
	st.wMinute = dateTime.GetMin();
    {
        if(SystemTimeToFileTime(&st,pFileTime))
            return(TRUE);
    }
    return(FALSE);
}

/*
	UpdateFileDateTime()

	Aggiorna la data/ora del file locale con quella specificata dal server con l'header Last-Modified:
	Quando la risorsa (il file) viene scaricata in locale, il sistema aggiorna la data/ora di creazione
	con quelle in cui e' avvenuta la creazione in locale, qui invece si aggiorna la data di ultima
	modifica, ossia la data in cui la risorsa fu modificata per l'ultima volta dal suo creatore originale.
*/
void CHttpDownloader::UpdateFileDateTime(LPCSTR lpcszFullpathFilename)
{
	// ricava dal server la data/ora di ultima modifica, potrebbe NON essere presente
	const char* pLastModified;
    if((pLastModified = GetServerHttpHeader("Last-Modified"))!=NULL)
    {
		// la converte, da GMT a FILETIME
		FILETIME ftModified = {0};
        if(HttpToFileDateTime(pLastModified,&ftModified))
        {
			// una volta convertite, aggiorna data e ora del file locale
            HANDLE hFile = CreateFile(lpcszFullpathFilename,GENERIC_WRITE,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
            if(hFile!=INVALID_HANDLE_VALUE)
            {
                SetFileTime(hFile,NULL,NULL,&ftModified);
                CloseHandle(hFile);
            }
        }
    }
}

/*
	CreateBaseDirectoryTree()

	Crea la struttura di directory per il log ed i cookies.
*/
BOOL CHttpDownloader::CreateBaseDirectoryTree(void)
{
	BOOL bRet = FALSE;
	DWORD dwError = 0L;
    char szCookieDirectory[MAX_PATH_LEN+1] = {0};
    char szLogDirectory[MAX_PATH_LEN+1] = {0};

	if(CreatePathname(m_szBaseDirectory,&dwError))
	{
		snprintf(szCookieDirectory,sizeof(szCookieDirectory),"%s\\cookies",m_szBaseDirectory);
		snprintf(szLogDirectory,sizeof(szLogDirectory),"%s\\log",m_szBaseDirectory);
		if(CreatePathname(szCookieDirectory,&dwError))
			bRet = CreatePathname(szLogDirectory,&dwError);
	}

	if(!bRet)
		m_pfnLogCallback(m_dwCallbackFlags,GetCurrentHost(),GetBaseDirectory(),"error: unable to create the directory tree: %s (%ld)\n",m_szBaseDirectory,dwError);

	return(bRet);
}

/*
	HandleReceivedCookies()
*/
void CHttpDownloader::HandleReceivedCookies(LPCSTR pszCurrentUrl)
{
	// estrae l'host dalla url
	char* pszHost = GetHostFromUrl(pszCurrentUrl);
    if(!pszHost)
	{
		m_httpReply.dwStatusCode = WSAHOST_NOT_FOUND;
		m_pfnLogCallback(m_dwCallbackFlags,GetCurrentHost(),GetBaseDirectory(),"error (cookies): unable to resolve the host from %s (%ld)\n",pszCurrentUrl,m_httpReply.dwStatusCode);
		return;
    }

	// chiama con il parametro a 0 per forzare a farsi dire quanto spazio necessita
    DWORD dwBufSize = 0L;
    HttpQueryInfoA(m_hRequest,HTTP_QUERY_SET_COOKIE,NULL,&dwBufSize,NULL);
    if(GetLastError()==ERROR_INSUFFICIENT_BUFFER)
	{
		// alloca memoria e riceve i cookies
        char* pszSetCookieHeader = (char*)calloc(dwBufSize,sizeof(char));
		if(pszSetCookieHeader)
		{
			if(HttpQueryInfoA(m_hRequest,HTTP_QUERY_SET_COOKIE,pszSetCookieHeader,&dwBufSize,NULL))
			{
				m_pfnLogCallback(m_dwCallbackFlags,GetCurrentHost(),GetBaseDirectory(),"headers Set-Cookie received: %s\n",pszSetCookieHeader);

				// notare che HttpQueryInfoA() raggruppa tutti le coppie <nome=valore> ricevute per ogni header Set-Cookie:
				// inviato dal server, e le separa con un ritorno a capo, cicla quindi per tutti i cookies ricevuti
                char* context = NULL;
                char* singleCookieString = strtok_s(pszSetCookieHeader,"\r\n",&context);
                while(singleCookieString!=NULL)
				{
					COOKIEINFO newCookie;
					BOOL bIsCookieExpired = FALSE;
                    if(ParseSetCookieHeader(singleCookieString,&newCookie,bIsCookieExpired))
					{
                        if(strlen(newCookie.domain)==0)
                            strcpy_s(newCookie.domain,sizeof(newCookie.domain),pszHost);

						newCookie.created = time(NULL);
						newCookie.lastused = newCookie.created; 

						SaveCookieToFile(m_szBaseDirectory,&newCookie,bIsCookieExpired);
                    }
					else
					{
						m_httpReply.dwStatusCode = ERROR_HTTP_INVALID_HEADER;
						m_pfnLogCallback(m_dwCallbackFlags,GetCurrentHost(),GetBaseDirectory(),"error (cookies): unable to parse the Set-Cookie: %s\n",singleCookieString);
                    }
                    singleCookieString = strtok_s(NULL,"\r\n",&context);
                }
            }
			else
			{
				m_httpReply.dwStatusCode = ERROR_HTTP_INVALID_HEADER;
				m_pfnLogCallback(m_dwCallbackFlags,GetCurrentHost(),GetBaseDirectory(),"error (cookies): unable to get the Set-Cookie header (%ld)\n",GetLastError());
            }

            free(pszSetCookieHeader);
        }
		else
		{
			m_httpReply.dwStatusCode = ERROR_NOT_ENOUGH_MEMORY;
			m_pfnLogCallback(m_dwCallbackFlags,GetCurrentHost(),GetBaseDirectory(),"error (cookies): memory allocation failed (%ld)\n",m_httpReply.dwStatusCode);
        }
    }
	else if(GetLastError()==ERROR_HTTP_HEADER_NOT_FOUND)
	{
		m_httpReply.dwStatusCode = ERROR_HTTP_HEADER_NOT_FOUND;
		m_pfnLogCallback(m_dwCallbackFlags,GetCurrentHost(),GetBaseDirectory(),"no Set-Cookie header found into server reply\n");
    }
	else
	{
		m_httpReply.dwStatusCode = GetLastError();
		m_pfnLogCallback(m_dwCallbackFlags,GetCurrentHost(),GetBaseDirectory(),"error (cookies): unable to handle the Set-Cookie header (%ld)\n",m_httpReply.dwStatusCode);
    }

    free(pszHost);
}

/*
	ParseSetCookieHeader()
*/
BOOL CHttpDownloader::ParseSetCookieHeader(LPCSTR pszSetCookieString,COOKIEINFO* pCookie,BOOL& bIsCookieExpired)
{
    if(!pszSetCookieString || !pCookie)
		return(FALSE);

    memset(pCookie,'\0',sizeof(COOKIEINFO));
    pCookie->expires = 0;
    pCookie->secure = FALSE;
    pCookie->httpOnly = FALSE;
    strcpyn(pCookie->path,"/",sizeof(pCookie->path));

	// dimensiona per la lunghezza massima
    char szTemp[MAX_COOKIE_LEN+1] = {0};
    strcpyn(szTemp,pszSetCookieString,sizeof(szTemp));

    // tokenizza per ';'
	char* context = NULL;
    char* token = strtok_s(szTemp,";",&context);

    // il primo token e'sempre name=value
    if(token)
	{
		char* equal = strchr(token,'=');
        if(equal)
		{
            size_t nameLen = equal - token;
			memcpy(pCookie->name, token, nameLen < sizeof(pCookie->name) ? nameLen : sizeof(pCookie->name)-1);
            pCookie->name[nameLen] = '\0';

            char* valueStart = equal + 1;
            // rimuove spazi iniziali e finali dal valore
            while(isspace((unsigned char)*valueStart))
				valueStart++;
            char* valueEnd = valueStart + strlen(valueStart) - 1;
            while(valueEnd >= valueStart && isspace((unsigned char)*valueEnd))
				valueEnd--;
            *(valueEnd + 1) = '\0'; // termina la stringa pulita
			strcpyn(pCookie->value,valueStart,sizeof(pCookie->value));
        }
		else
		{
            // formato non valido per name=value
            return(FALSE);
        }
    }
	else
	{
        return(FALSE); // stringa vuota o non valida
    }

    // processa gli altri attributi
    while((token = strtok_s(NULL,";",&context))!=NULL)
	{
        // rimuove spazi iniziali
//        while(isspace(*token))
//			token++;

        char* equal = strchr(token,'=');
        if(equal)
		{
			BOOL bIsExpired = FALSE;
            char attrName[MAX_COOKIE_NAME_LEN+1] = {0};
            char attrValue[MAX_COOKIE_ATTR_LEN+1] = {0};

			strcpyn(attrValue,equal+1,sizeof(attrValue));
			strrtrim(attrValue),strltrim(attrValue);

			strcpyn(attrName,token,sizeof(attrName));
			if((equal = strchr(attrName,'='))!=NULL)
				*equal = '\0';
			strrtrim(attrName),strltrim(attrName);

			// analizza la data ricevuta in formato GMT, puo' essere:
			// Wdy, DD-Mon-YY HH:MM:SS GMT (Standard obsoleto)
			// Wdy, DD Mon YYYY HH:MM:SS GMT (Standard moderno RFC 1123, il più comune)
            if(stricmp(attrName,"expires")==0)
			{
				CDateTime dateTime;
				if(dateTime.LoadFromString(attrValue))
				{
					struct tm tm_info = {0};
					tm_info.tm_mday = dateTime.GetDay();
					tm_info.tm_mon = dateTime.GetMonth();
					tm_info.tm_year = dateTime.GetYear() - 1900;
					tm_info.tm_hour = dateTime.GetHour();
					tm_info.tm_min = dateTime.GetMin();
					tm_info.tm_sec = dateTime.GetSec();
					pCookie->expires = _mkgmtime(&tm_info);

					// se la data sta nel passato, il cookie deve essere eliminato
					if(pCookie->expires <= time(NULL))
						bIsCookieExpired = TRUE;
					else
						bIsCookieExpired = FALSE;
				
					m_pfnLogCallback(m_dwCallbackFlags,GetCurrentHost(),GetBaseDirectory(),"cookie expires: %s -> %lld (current: %lld)",attrValue,(long long)pCookie->expires,(long long)time(NULL));
				}
				else
				{
                    pCookie->expires = 0;
					m_httpReply.dwStatusCode = ERROR_INTERNET_INCORRECT_FORMAT;
					m_pfnLogCallback(m_dwCallbackFlags,GetCurrentHost(),GetBaseDirectory(),"error (cookies): invalid date format: %s (%ld)\n",attrValue,m_httpReply.dwStatusCode);
                }
            }
			else if(stricmp(attrName,"max-age")==0)
			{
				long maxAgeSeconds = atol(attrValue);
				if(maxAgeSeconds > 0)
				{
					pCookie->expires = time(NULL) + maxAgeSeconds;
					bIsCookieExpired = FALSE;
				}
				else
				{
					// per max-age <= 0 il cookie deve essere eliminato
					pCookie->expires = time(NULL) - 1;
					bIsCookieExpired = TRUE;
				}
			}
			else if(strcmp(attrName,"domain")==0)
			{
				strcpyn(pCookie->domain, attrValue, sizeof(pCookie->domain));
            }
			else if(strcmp(attrName,"path")==0)
			{
                strcpyn(pCookie->path,attrValue,sizeof(pCookie->path));
            }
        }
		else
		{
            strlrw(token);
            if(strcmp(token,"secure")==0)
			{
                pCookie->secure = TRUE;
            }
			else if(strcmp(token,"httponly")==0)
			{
                pCookie->httpOnly = TRUE;
            }
        }
    }

	return(TRUE);
}

/*
	DeleteCookieKeys()

	Rimuove tutte le chiavi di un cookie dal file .ini.
*/
BOOL CHttpDownloader::DeleteCookieKeys(LPCSTR pszFullCookieFilePath,const char* sectionName,const COOKIEINFO* pCookie)
{
	// tentativo di eliminare la chiave Name=Value
	BOOL bSuccess = WritePrivateProfileStringA(sectionName,pCookie->name,NULL,pszFullCookieFilePath);
    
	// lista di tutti gli attributi da eliminare
	const char* attributes[] = {"_expires","_secure","_httponly","_created","_lastused"};
	char szAttrKey[MAX_COOKIE_NAME_LEN + 8] = {0};

	// eliminazione di tutti gli attributi correlati
	for(int i = 0; i < sizeof(attributes) / sizeof(attributes[0]); ++i)
	{
		snprintf(szAttrKey,sizeof(szAttrKey),"%s%s",pCookie->name,attributes[i]);
		WritePrivateProfileStringA(sectionName,szAttrKey,NULL,pszFullCookieFilePath);
	}
    
	return(bSuccess);
}

/*
	SaveCookieToFile()

	Salva un singolo cookie in un file .ini specifico per il dominio.
*/
BOOL CHttpDownloader::SaveCookieToFile(LPCSTR pszOutputDirectory,const COOKIEINFO* pCookie,BOOL IsCookieExpired)
{
    if(!pszOutputDirectory || !pCookie || strlen(pCookie->name)==0)
		return(FALSE);

    char szCookieFileName[MAX_PATH_LEN+1] = {0};
    char szCookieDirPath[MAX_PATH_LEN+1] = {0};
    
    if(PathCombineA(szCookieDirPath,pszOutputDirectory,"cookies")==NULL)
	{
		m_httpReply.dwStatusCode = ERROR_INTERNET_INCORRECT_FORMAT;
		m_pfnLogCallback(m_dwCallbackFlags,GetCurrentHost(),GetBaseDirectory(),"error (cookies): unable to combine the pathname for the cookies (%ld)\n",m_httpReply.dwStatusCode);
        return(FALSE);
    }

    char* pszBaseDomain = GetBaseDomainFromHost(pCookie->domain);
    if(!pszBaseDomain || strlen(pszBaseDomain)==0)
	{
		m_httpReply.dwStatusCode = ERROR_INTERNET_INCORRECT_FORMAT;
		m_pfnLogCallback(m_dwCallbackFlags,GetCurrentHost(),GetBaseDirectory(),"error (cookies): invalid cookie domanin, unable to save it: %s (%ld)\n",pCookie->name,m_httpReply.dwStatusCode);
        free(pszBaseDomain);
        return(FALSE);
    }
    
    sprintf_s(szCookieFileName,sizeof(szCookieFileName),"%s.ini",pszBaseDomain);
    char szFullCookieFilePath[MAX_PATH_LEN+1] = {0};
    if(PathCombineA(szFullCookieFilePath,szCookieDirPath,szCookieFileName)==NULL)
	{
		m_httpReply.dwStatusCode = ERROR_INTERNET_INCORRECT_FORMAT;
		m_pfnLogCallback(m_dwCallbackFlags,GetCurrentHost(),GetBaseDirectory(),"error (cookies): unable to combine the pathname for the cookie %s (%ld)\n",pCookie->name,m_httpReply.dwStatusCode);
        free(pszBaseDomain);
        return(FALSE);
    }

	// aggiusta il path per il nome della sezione del .ini
	char szSanitizedPath[MAX_PATH_LEN + 1] = {0};
    
	// usa "/" come default per la root se pCookie->path e' vuoto
	const char* pathForSanitization = pCookie->path[0] == '\0' ? "/" : pCookie->path;

	size_t len = strlen(pathForSanitization);
	if(len > MAX_PATH_LEN)
		len = MAX_PATH_LEN;
    
	// sostituisce ogni carattere non alfanumerico con '_'
	for(size_t i = 0; i < len; ++i) 
	{
		char c = pathForSanitization[i];
		if(isalnum((unsigned char)c)) 
			szSanitizedPath[i] = c;
		else 
			szSanitizedPath[i] = '_'; 
	}
	szSanitizedPath[len] = '\0';
    
	// se il path e' "/blog/" verra' convertito in "_blog_"
	// se il path e' "/" verra' convertito in "_"
	const char* sectionName = szSanitizedPath;

	// eliminazione
    if(IsCookieExpired)
    {
        BOOL bSuccess = DeleteCookieKeys(szFullCookieFilePath,sectionName,pCookie);

        if(bSuccess)
            m_pfnLogCallback(m_dwCallbackFlags,GetCurrentHost(),GetBaseDirectory(),"cookie removed: %s domain: %s, path: %s, file: %s\n",pCookie->name,pCookie->domain,sectionName,szFullCookieFilePath);
        else if (!bSuccess)
            m_pfnLogCallback(m_dwCallbackFlags,GetCurrentHost(),GetBaseDirectory(),"error (cookies): unable to remove cookie %s from %s (%ld)\n",pCookie->name,szFullCookieFilePath,GetLastError());

        free(pszBaseDomain);
        return(bSuccess);
    }

    // se non e' scaduto, salva/aggiorna
    char szExpiresStr[64] = {0};
    if(pCookie->expires==0)
        strcpyn(szExpiresStr,"Session",sizeof(szExpiresStr));
	else
        snprintf(szExpiresStr,sizeof(szExpiresStr),"%lld",(long long)pCookie->expires);
 
	if(!WritePrivateProfileString(sectionName,pCookie->name,pCookie->value,szFullCookieFilePath))
	{
		m_httpReply.dwStatusCode = GetLastError();
		m_pfnLogCallback(m_dwCallbackFlags,GetCurrentHost(),GetBaseDirectory(),"error (cookies): unable to write the cookie %s into %s (%ld)\n",pCookie->name,szFullCookieFilePath,m_httpReply.dwStatusCode);
        free(pszBaseDomain);
        return(FALSE);
    }

    char szExpiresKey[MAX_COOKIE_ATTR_LEN + 8 + 1] = {0};
    snprintf(szExpiresKey,sizeof(szExpiresKey),"%s_expires",pCookie->name);
    if(!WritePrivateProfileString(sectionName,szExpiresKey,szExpiresStr,szFullCookieFilePath))
	{
		m_httpReply.dwStatusCode = GetLastError();
		m_pfnLogCallback(m_dwCallbackFlags,GetCurrentHost(),GetBaseDirectory(),"error (cookies): unable to write the expires fir the cookie %s (%ld)\n",pCookie->name,m_httpReply.dwStatusCode);
    }

    char szSecureKey[MAX_COOKIE_ATTR_LEN + 8]; snprintf(szSecureKey,sizeof(szSecureKey),"%s_secure",pCookie->name);
    char szHttpOnlyKey[MAX_COOKIE_ATTR_LEN + 8]; snprintf(szHttpOnlyKey,sizeof(szHttpOnlyKey),"%s_httponly",pCookie->name);

    WritePrivateProfileStringA(sectionName,szSecureKey,pCookie->secure ? "1" : "0",szFullCookieFilePath);
    WritePrivateProfileStringA(sectionName,szHttpOnlyKey,pCookie->httpOnly ? "1" : "0",szFullCookieFilePath);

	char szCreatedKey[MAX_COOKIE_NAME_LEN + 8] = {0};
	char szLastUsedKey[MAX_COOKIE_NAME_LEN + 8] = {0};
	char szTimeStr[64] = {0};

	snprintf(szCreatedKey,sizeof(szCreatedKey),"%s_created",pCookie->name);
	snprintf(szTimeStr,sizeof(szTimeStr),"%lld",(long long)pCookie->created);
	WritePrivateProfileStringA(sectionName,szCreatedKey,szTimeStr,szFullCookieFilePath);

	snprintf(szLastUsedKey,sizeof(szLastUsedKey),"%s_lastused",pCookie->name);
	snprintf(szTimeStr,sizeof(szTimeStr),"%lld",(long long)pCookie->lastused);
	WritePrivateProfileStringA(sectionName,szLastUsedKey,szTimeStr,szFullCookieFilePath);

	m_pfnLogCallback(m_dwCallbackFlags,GetCurrentHost(),GetBaseDirectory(),"cookie saved: %s=%s (expires: %s) domain: %s, path: %s, file: %s\n",pCookie->name,pCookie->value,szExpiresStr,pCookie->domain,sectionName,szFullCookieFilePath);
    
    free(pszBaseDomain);

    return(TRUE);
}

/*
	LoadCookiesForUrl()

	Carica e filtra i cookie dal file .ini basandosi sulla URL e sull'host correnti.
	Implementa la pulizia dei cookie scaduti (manutenzione della cache).

	OCCHIO: manca la gestione degli errori, si limita a fare return.
*/
std::vector<COOKIEINFO> CHttpDownloader::LoadCookiesForUrl(LPCSTR pszOutputDirectory,LPCSTR lpcszUrl,LPCSTR pszHostDomainForLog)
{
    std::vector<COOKIEINFO> loadedCookies;
    BOOL bIsSecureConnection = FALSE;
    char* pszCurrentHost = NULL;
    char* pszCurrentPath = NULL;
    char* pszBaseDomain = NULL;
	char szSanitizedCurrentPath[MAX_PATH_LEN+1] = {0};
    const char* pathForSanitization = NULL;
	size_t len = 0;
    char szCookieDirPath[MAX_PATH_LEN+1] = {0};
    char szCookieFileName[MAX_PATH_LEN+1] = {0};
    char szFullCookieFilePath[MAX_PATH_LEN+1] = {0};
    char szSectionsBuffer[4096] = {0};
    DWORD dwLen = 0L;

    if(!pszOutputDirectory || !lpcszUrl)
        goto done;

    // estrazione e allocazione delle stringhe della URL corrente
    pszCurrentHost = GetHostFromUrl(lpcszUrl);
    if(!pszCurrentHost)
        goto done;

    pszCurrentPath = GetPathFromUrl(lpcszUrl);
    if(!pszCurrentPath)
        goto done;

    pszBaseDomain = GetBaseDomainFromHost(pszCurrentHost);
    if(!pszBaseDomain)
        goto done;
    
    // controlla se la connessione e' https per il filtro 'Secure'
    bIsSecureConnection = (strnicmp(lpcszUrl,"https://",8)==0);

	// aggiusta il path per il nome della sezione del .ini
	// qui deve replicare esattamente la logica usata in SaveCookieToFile()
	// usa "/" se il path e' vuoto, altrimenti il path stesso
	pathForSanitization = pszCurrentPath[0]=='\0' ? "/" : pszCurrentPath;

	len = strlen(pathForSanitization);
	if(len > MAX_PATH_LEN)
		len = MAX_PATH_LEN;
    
	// sostituisce ogni carattere non alfanumerico con '_'
	for(int i=0; i < (int)len; ++i) 
	{
		char c = pathForSanitization[i];
		if(isalnum((unsigned char)c)) 
			szSanitizedCurrentPath[i] = c;
		else 
			szSanitizedCurrentPath[i] = '_'; 
	}
	szSanitizedCurrentPath[len] = '\0';

    // costruzione del percorso del file .ini
    if(PathCombine(szCookieDirPath,pszOutputDirectory,"cookies")==NULL)
        goto done;

    snprintf(szCookieFileName,sizeof(szCookieFileName),"%s.ini",pszBaseDomain);
    if(PathCombineA(szFullCookieFilePath,szCookieDirPath,szCookieFileName)==NULL)
        goto done;

	m_pfnLogCallback(m_dwCallbackFlags,GetCurrentHost(),GetBaseDirectory(),"loading cookies from %s\n",szFullCookieFilePath);

    if(!PathFileExists(szFullCookieFilePath))
        goto done;

    // itera su tutte le sezioni (path) nel file .ini
    dwLen = GetPrivateProfileSectionNamesA(szSectionsBuffer,sizeof(szSectionsBuffer),szFullCookieFilePath);
    if(dwLen > 0L)
    {
        char* pSection = szSectionsBuffer;
        while(*pSection!='\0')
        {
            const char* sectionName = pSection; // sectionName e' il Path del cookie
            size_t cookiePathLen = strlen(sectionName);
            
            // filtro 1: path matching (verifica se il path del cookie e' un prefisso del path della richiesta)
            if(cookiePathLen > 0 && strnicmp(szSanitizedCurrentPath,sectionName,cookiePathLen)==0)
            {
                // si assicura che il match non sia una sottostringa di un componente
                if(szSanitizedCurrentPath[cookiePathLen]=='\0' || szSanitizedCurrentPath[cookiePathLen]=='_')
                {
                    char szKeysBuffer[4096] = {0};
                    DWORD dwKeysLen = GetPrivateProfileSectionA(sectionName,szKeysBuffer,sizeof(szKeysBuffer),szFullCookieFilePath);

                    if(dwKeysLen > 0L)
                    {
                        char* pKey = szKeysBuffer;
                        while(*pKey!='\0')
                        {
                            char* equal = strchr(pKey,'=');
                            if(equal)
                            {
                                // parsing name=value
                                char keyName[MAX_COOKIE_NAME_LEN+1] = {0};
                                size_t keyLen = equal - pKey;
                                memcpy(keyName,pKey,keyLen < sizeof(keyName) ? keyLen : sizeof(keyName) - 1);
                                keyName[keyLen] = '\0';
                                char* keyValue = equal + 1;

                                // salta le chiavi di attributo (expires, secure, httponly, created, lastused)
                                if(strstr(keyName,"_")!=NULL) 
                                {
                                    pKey += strlen(pKey) + 1;
                                    continue;
                                }

                                // caricamento del cookie base
                                COOKIEINFO currentCookie;
                                ZeroMemory(&currentCookie,sizeof(COOKIEINFO));
                                strcpy_s(currentCookie.name,sizeof(currentCookie.name),keyName);
                                strcpy_s(currentCookie.value,sizeof(currentCookie.value),keyValue);
                                strcpy_s(currentCookie.path,sizeof(currentCookie.path),sectionName);

                                // il campo domain e' l'host base per il file .ini (senza il punto iniziale)
                                strcpy_s(currentCookie.domain, sizeof(currentCookie.domain), pszBaseDomain);
                                
                                // carica gli attributi
                                char szAttrValue[MAX_COOKIE_ATTR_LEN] = {0};
                                char szAttrKey[MAX_COOKIE_NAME_LEN + 8] = {0};


                                // Expires (necessario per il filtro scadenza)
                                sprintf_s(szAttrKey, sizeof(szAttrKey), "%s_expires", currentCookie.name);
                                if (GetPrivateProfileStringA(sectionName, szAttrKey, "", szAttrValue, sizeof(szAttrValue), szFullCookieFilePath) > 0)
                                {
                                    if (strcmp(szAttrValue, "Session") == 0)
                                        currentCookie.expires = 0; // 0 significa Session
                                    else
                                        currentCookie.expires = (time_t)_atoi64(szAttrValue);
                                }
                                
                                // Secure
                                sprintf_s(szAttrKey, sizeof(szAttrKey), "%s_secure", currentCookie.name);
                                if (GetPrivateProfileStringA(sectionName, szAttrKey, "0", szAttrValue, sizeof(szAttrValue), szFullCookieFilePath) > 0)
                                    currentCookie.secure = (strcmp(szAttrValue, "1") == 0);

                                // HttpOnly
                                sprintf_s(szAttrKey, sizeof(szAttrKey), "%s_httponly", currentCookie.name);
                                if (GetPrivateProfileStringA(sectionName, szAttrKey, "0", szAttrValue, sizeof(szAttrValue), szFullCookieFilePath) > 0)
                                    currentCookie.httpOnly = (strcmp(szAttrValue, "1") == 0);
                                    
                                // Created
                                sprintf_s(szAttrKey, sizeof(szAttrKey), "%s_created", currentCookie.name);
                                if (GetPrivateProfileStringA(sectionName, szAttrKey, "0", szAttrValue, sizeof(szAttrValue), szFullCookieFilePath) > 0)
                                    currentCookie.created = (time_t)_atoi64(szAttrValue);

                                // LastUsed
                                sprintf_s(szAttrKey, sizeof(szAttrKey), "%s_lastused", currentCookie.name);
                                if (GetPrivateProfileStringA(sectionName, szAttrKey, "0", szAttrValue, sizeof(szAttrValue), szFullCookieFilePath) > 0)
                                    currentCookie.lastused = (time_t)_atoi64(szAttrValue);

                                BOOL bAddCookie = TRUE;

                                // filtro 2: scadenza (rimozione automatica dal file)
                                if(currentCookie.expires!=0 && time(NULL) > currentCookie.expires)
                                {
                                    // elimina il cookie scaduto dal file
									m_pfnLogCallback(m_dwCallbackFlags,GetCurrentHost(),GetBaseDirectory(),"cookie '%s' expired, removing it from file.",currentCookie.name);
                                    
                                    WritePrivateProfileStringA(sectionName,currentCookie.name,NULL,szFullCookieFilePath);
                                    
                                    // elimina tutti gli attributi correlati
                                    char szKeysToDelete[5][MAX_COOKIE_NAME_LEN + 8]; // max 5 attributi
                                    sprintf_s(szKeysToDelete[0],sizeof(szKeysToDelete[0]),"%s_expires",currentCookie.name);
                                    sprintf_s(szKeysToDelete[1],sizeof(szKeysToDelete[1]),"%s_secure",currentCookie.name);
                                    sprintf_s(szKeysToDelete[2],sizeof(szKeysToDelete[2]),"%s_httponly",currentCookie.name);
                                    sprintf_s(szKeysToDelete[3],sizeof(szKeysToDelete[3]),"%s_created",currentCookie.name);
                                    sprintf_s(szKeysToDelete[4],sizeof(szKeysToDelete[4]),"%s_lastused",currentCookie.name);
                                    
                                    for(int i = 0; i < 5; ++i)
                                        WritePrivateProfileStringA(sectionName, szKeysToDelete[i], NULL, szFullCookieFilePath);
                                    
                                    bAddCookie = FALSE;
                                }

                                // filtro 3: secure (solo su HTTPS)
                                if(bAddCookie && currentCookie.secure && !bIsSecureConnection)
                                    bAddCookie = FALSE;

                                // filtro 4: domain matching
                                if(bAddCookie)
                                {
                                    size_t hostLen = strlen(pszCurrentHost);
                                    size_t cookieDomainLen = strlen(currentCookie.domain);

                                    if(cookieDomainLen > 0)
                                    {
                                        if(hostLen < cookieDomainLen)
                                        {
                                            bAddCookie = FALSE;
                                        }
                                        else
                                        {
                                            if(_strnicmp(pszCurrentHost + hostLen - cookieDomainLen, currentCookie.domain, cookieDomainLen)!=0)
                                            {
                                                bAddCookie = FALSE;
                                            }
                                            else if(hostLen > cookieDomainLen && pszCurrentHost[hostLen - cookieDomainLen - 1]!='.')
                                            {
                                                bAddCookie = FALSE;
                                            }
                                        }
                                    }
                                    // se bAddCookie e' FALSE a causa di domain mismatch, logga
                                    if(!bAddCookie)
                                    {
                                        m_httpReply.dwStatusCode = ERROR_BAD_FORMAT;
                                        m_pfnLogCallback(m_dwCallbackFlags,GetCurrentHost(),GetBaseDirectory(),"error (cookie): domain mismatch for cookie '%s', host: '%s', cookie domain: '%s', discarded (%ld)\n",currentCookie.name,pszCurrentHost,currentCookie.domain,m_httpReply.dwStatusCode);
                                    }
                                }

                                // aggiunge il cookie valido
                                if(bAddCookie)
                                {
                                    // aggiorna l'attributo lastused al momento del caricamento
                                    currentCookie.lastused = time(NULL);
                                    loadedCookies.push_back(currentCookie);
									m_pfnLogCallback(m_dwCallbackFlags, GetCurrentHost(), GetBaseDirectory(), "cookie '%s' loaded successful ('%s').\n", currentCookie.name, currentCookie.value);
                                }
                            }

                            pKey += strlen(pKey) + 1;
                        }
                    }
                }
            }
            
            pSection += strlen(pSection) + 1;
        }
    }

done:

    if(pszCurrentHost)
		free(pszCurrentHost);
    if(pszCurrentPath)
		free(pszCurrentPath);
    if(pszBaseDomain)
		free(pszBaseDomain);

    return(loadedCookies);
}

/*
	LogHttpHeaders()

	Trascrive nel file di log gli headers HTTP.
*/
void CHttpDownloader::LogHttpHeaders(HINTERNET hHandle,DWORD dwInfoLevel,int nType)
{
	LPCSTR pszDescription = nType==0 ? "HTTP request headers" : (nType==1 ? "HTTP reply headers" : "unknown");

	m_pfnLogCallback(m_dwCallbackFlags,GetCurrentHost(),GetBaseDirectory(),"%s (%s):\n",pszDescription,GetCurrentHost());

	// chiama con il parametro a 0 per farsi dire quanto spazio necessita
    DWORD dwBufSize = 0L;
    HttpQueryInfoA(hHandle,dwInfoLevel,NULL,&dwBufSize,NULL);
    if(GetLastError()==ERROR_INSUFFICIENT_BUFFER)
	{
		// alloca per riceve gli headers
		char* pszHeaders = (char*)calloc(dwBufSize,sizeof(char));
		if(pszHeaders)
		{
            if(HttpQueryInfoA(hHandle,dwInfoLevel,pszHeaders,&dwBufSize,NULL))
			{
				// logga ogni riga dell'header separatamente
                int i=0;
				char* context = NULL;
                char* line = strtok_s(pszHeaders,"\r\n",&context);
                while(line!=NULL)
				{
					m_pfnLogCallback(m_dwCallbackFlags,GetCurrentHost(),GetBaseDirectory(),"--> %s\n",line);
					if(i++==0 && nType==0)
						m_pfnLogCallback(m_dwCallbackFlags,GetCurrentHost(),GetBaseDirectory(),"--> User-Agent: %s\n",m_szUserAgent);
                    line = strtok_s(NULL,"\r\n",&context);
                }
            }
			else
			{
				m_httpReply.dwStatusCode = GetLastError();
				m_pfnLogCallback(m_dwCallbackFlags,GetCurrentHost(),GetBaseDirectory(),"error: unable to retrieve %s (%ld)\n",pszDescription,m_httpReply.dwStatusCode);
            }
            free(pszHeaders);
        }
		else
		{
			m_httpReply.dwStatusCode = ERROR_NOT_ENOUGH_MEMORY;
			m_pfnLogCallback(m_dwCallbackFlags,GetCurrentHost(),GetBaseDirectory(),"error: memory allocation failed for %s (%ld)\n",pszDescription,m_httpReply.dwStatusCode);
        }
	}
	else if(GetLastError()==ERROR_HTTP_HEADER_NOT_FOUND)
	{
		m_httpReply.dwStatusCode = ERROR_HTTP_HEADER_NOT_FOUND;
		m_pfnLogCallback(m_dwCallbackFlags,GetCurrentHost(),GetBaseDirectory(), "error: no %s found (%ld)\n",pszDescription,m_httpReply.dwStatusCode);
    }
	else
	{
		m_httpReply.dwStatusCode = GetLastError();
		m_pfnLogCallback(m_dwCallbackFlags,GetCurrentHost(),GetBaseDirectory(), "error: unable to retrieve %s (%ld)\n",pszDescription,m_httpReply.dwStatusCode);
	}
}
