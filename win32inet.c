/*$
	win32inet.c
	Generiche per Internet.
	Luca Piergentili, 02/07/25

	Attenzione: se si incorpora questo file (.c) in un progetto C++/MFC, la inclusione del file "window.h" di cui sotto
	genera, in cascata, la inclusione di "windows.h" e "afx.h", con quest'ultimo generando l'errore:
	#error MFC requires C++ compilation (use a .cpp suffix)
	dato che il file ha estensione .c.
	Per risolvere o si rinomina il file in .cpp o si forza la compilazione in modalita' C++ con l'opzione relativa (/TP).
*/
#include "pragma.h"
#include "env.h"
#include "macro.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "strings.h"
#include <ctype.h>
#include <lmerr.h>
#include "window.h"
#include <wininet.h>
#include "win32inet.h"
#include "url.h"

#include "traceexpr.h"
//#define _TRACE_FLAG			_TRFLAG_TRACEOUTPUT // opzioni: _TRFLAG_NOTRACE, _TRFLAG_TRACEFILE, _TRFLAG_TRACECONSOLE, _TRFLAG_TRACEOUTPUT, _TRFLAG_TRACEBREAKPOINT
#define _TRACE_FLAG			_TRFLAG_NOTRACE // opzioni: _TRFLAG_NOTRACE, _TRFLAG_TRACEFILE, _TRFLAG_TRACECONSOLE, _TRFLAG_TRACEOUTPUT, _TRFLAG_TRACEBREAKPOINT
#define _TRACE_FLAG_INFO	_TRACE_FLAG
#define _TRACE_FLAG_WARN	_TRACE_FLAG
#define _TRACE_FLAG_ERR		_TRACE_FLAG

#define IS_NET_ERROR(n)		(n >= NERR_BASE && n <= MAX_NERR)
#define IS_WINSOCK_ERROR(n)	(n >= WSABASEERR && n <= 11999)
#define IS_WININET_ERROR(n)	(n >= INTERNET_ERROR_BASE && n <= INTERNET_ERROR_LAST)

/*
	WSA_ERRORS
*/
typedef struct _wsaerrors_t {
	int code;
	const char* desc;
} WSA_ERRORS;

/*
	WhichHttpProtocol()

	Verifica che tipo di protocollo specifica la url: se "http" (non sicuro) o "https" (sicuro).
	Da non confondere con il numero di versione http ("HTTP/1.0", "HTTP/1.1", etc.).
	Confronta l'inizio della stringa ed e' case sensitive.

	Restituisce:
	HTTP_PROTOCOL_SECURE per "https" (sicuro)
	HTTP_PROTOCOL_NON_SECURE per protocollo "http" (non sicuro)
	HTTP_PROTOCOL_UNKNOWN se non e' presente nessun protocollo http
*/
HTTP_PROTOCOL WhichHttpProtocol(LPCSTR lpcszUrl)
{
	HTTP_PROTOCOL nProto = HTTP_PROTOCOL_UNKNOWN;

	if(memcmp(lpcszUrl,"http://",7)==0)
		nProto = HTTP_PROTOCOL_NON_SECURE;
	else if(memcmp(lpcszUrl,"https://",8)==0)
		nProto = HTTP_PROTOCOL_SECURE;

	return(nProto);
}

/*
	GetHostFromUrl()
	
	Estrae il nome dell'host da una URL.

	Restituisce un puntatore ad una stringa allocata che deve essere liberata con free() o NULL per errore.
*/
LPSTR GetHostFromUrl(LPCSTR pszUrl)
{
    char szHostName[INTERNET_MAX_HOST_NAME_LENGTH+1] = {0};
    DWORD dwHostNameLen = sizeof(szHostName);
    URL_COMPONENTS urlComp = {0};

    urlComp.dwStructSize = sizeof(urlComp);
    urlComp.lpszHostName = szHostName;
    urlComp.dwHostNameLength = dwHostNameLen;

    if(InternetCrackUrl(pszUrl,0,0,&urlComp))
        return(strdup(szHostName));

    return(NULL);
}

/*
	GetProtoFromUrl()
	
	Estrae il protocollo da una URL.

	Restituisce un puntatore ad una stringa allocata che deve essere liberata con free() o NULL per errore.
*/
LPSTR GetProtoFromUrl(LPCSTR pszUrl)
{
	CHAR szScheme[INTERNET_MAX_SCHEME_LENGTH+1] = {0}; 
	DWORD dwSchemeLen = sizeof(szScheme);
	URL_COMPONENTS urlComp = {0};

	urlComp.dwStructSize = sizeof(urlComp);
	urlComp.lpszScheme = szScheme;
	urlComp.dwSchemeLength = dwSchemeLen;

	if(InternetCrackUrl(pszUrl,0,0,&urlComp))
	{
		size_t totalLen = strlen(szScheme) + 3 + 1;
		char* result = (char*)calloc(totalLen,sizeof(char));
		if(result)
		{
			sprintf(result,"%s://",szScheme);
			return(result);
		}
	}

	return(NULL);
}

/*
	GetBaseDomainFromHost()

	Estrae il dominio base (es. "example.com" da "www.example.com") e restituisce un puntatore ad una stringa 
	allocata che deve essere liberata dal chiamante con free().

	Approccio semplificato che non copre tutti i TLD multistrato.

	Restituisce un puntatore ad una stringa allocata che deve essere liberata con free() o NULL per errore.

	Per distinguere i vari componenti di un indirizzo web come www.example.com:

	- Dominio di primo livello (TLD):
	  la parte finale di un nome a dominio, come .com, .it, .org, ecc.
	- Dominio base (o dominio di secondo livello): 
	  e' il nome che precede il TLD, corrisponde alla parte principale e unica del dominio: nell'esempio, 
	  example. il dominio base completo e' la combinazione di questo e del TLD: example.com
	- Sottodominio: 
	  qualsiasi parte che precede il dominio base: www e' un sottodominio, un'azienda potrebbe usare altri 
	  sottodomini come blog.example.com o shop.example.com.
	- Nome Host (o Hostname):
	  l'indirizzo completo che include tutti i sottodomini e il dominio base, e' il nome che identifica in 
	  modo univoco una risorsa su una rete, es. www.example.com
*/
LPSTR GetBaseDomainFromHost(LPCSTR pszHost)
{
    const char* pHost = pszHost;
    if(strnicmp(pHost,"www.",4)==0)
        pHost += 4;

    const char* pLastDot = strrchr(pHost,'.');
    if(!pLastDot || pLastDot==pHost)
        return(strdup(pszHost));
 
    const char* pSecondLastDot = NULL;
    const char* tempPtr = pLastDot - 1;
    while(tempPtr >= pHost && *tempPtr!='.')
        tempPtr--;

    if(tempPtr < pHost)
        return(strdup(pHost));

    pSecondLastDot = tempPtr;

    return(strdup(pSecondLastDot + 1));
}

/*
	GetPathFromUrl()

	Estrae il percorso da una URL.

	Restituisce un puntatore a una stringa allocata che deve essere liberata con free() o NULL per errore.
*/
LPSTR GetPathFromUrl(LPCSTR pszUrl)
{
    if(pszUrl)
	{
		char szPath[INTERNET_MAX_PATH_LENGTH] = {0};
		DWORD dwPathLen = sizeof(szPath);
		URL_COMPONENTS urlComp = {0};

		urlComp.dwStructSize = sizeof(urlComp);
		urlComp.lpszUrlPath = szPath;
		urlComp.dwUrlPathLength = dwPathLen;

		if(InternetCrackUrl(pszUrl,0,0,&urlComp))
		{
			if(urlComp.dwUrlPathLength!=0)
				return(strdup(szPath));
		}
	}

    return(_strdup("/")); /* default a root path */
}

/*
	SplitContentType()

	Suddivide il valore della Content-Type nei due componenti (quelli separati da '/').
*/
void SplitContentType(LPCSTR lpcszContent,LPSTR lpszContentType,UINT nContentType,LPSTR lpszContentSubType,UINT nContentSubType)
{
    /* cerca il carattere '/' */
    const char* slash = strchr(lpcszContent,'/');

    /* se non c'e' lo slash, copia l'intera stringa nel tipo principale, azzerando il sottotipo */
    if(!slash)
	{
		strcpyn(lpszContentType,lpcszContent,nContentType);
		memset(lpszContentSubType,'\0',nContentSubType);
    }
	else /* formato corretto: ha trovato lo slash  */
	{
		/* copia la prima parte (il tipo principale) */
		strcpyn(lpszContentType,lpcszContent,nContentType);
		char* slash = strchr(lpszContentType,'/');

		/* copia la seconda parte (il sottotipo) e termina la prima */
		strcpyn(lpszContentSubType,slash+1,nContentSubType);
		*slash = '\0';
	}
}

/*
	GetMimeGenreFromExtension()

	Restituisce il genere (TEXT/BINARY) relativo all'estensione del file.
	Assicurarsi che il parametro contenga una estensione valida prima di chiamare la funzione.
*/
MIMEGENRE GetMimeGenreFromExtension(LPCSTR lpcszFilename)
{
	if(lpcszFilename && *lpcszFilename)
	{
		/* cerca l'ultimo '.' per trovare l'estensione */
		LPCSTR pExt = strrchr(lpcszFilename,'.');

		if(pExt)
		{
			MIMETYPE* m;
			int iterator = 0;
			while((m = (MIMETYPE*)inet_enum_mime_types(&iterator))!=NULL)
				if(stricmp(lpcszFilename,m->ext)==0)
						return(m->genre);
		}
	}

	return(GENRE_BINARY);
}

/*
	GetMimeGenreFromContentType()

	Restituisce il genere (TEXT/BINARY) relativo alla Content-Type.
*/
MIMEGENRE GetMimeGenreFromContentType(LPCSTR lpcszContentType)
{
	if(lpcszContentType && *lpcszContentType)
	{
		/* estrae la parte principale del Content-Type (prima del ';') */
		char szMainType[256] = {0};
		strcpyn(szMainType,lpcszContentType,sizeof(szMainType));
		char* semicolon = strchr(szMainType,';');
		if(semicolon)
			*semicolon = '\0';

		MIMETYPE* m;
		int iterator = 0;
		while((m = (MIMETYPE*)inet_enum_mime_types(&iterator))!=NULL)
			if(stricmp(szMainType,m->type)==0)
					return(m->genre);
	}

	return(GENRE_BINARY);
}

/*
	GetMimeFamilyFromExtension()

	Restituisce la family (FAMILY_TEXT, FAMILY_IMAGE, etc.) relativa all'estensione del file.
	Assicurarsi che il parametro contenga una estensione valida prima di chiamare la funzione.
	Un estensione valida significa che a destra la stringa deve terminare con un estensione, non
	puo' essere contenuta all'interno, come succede con le URLs che contengono una query.
	Puo' essere passato un nome file completo o solo l'estensione, il qualsiasi caso l'estensione
	deve includere il punto.
*/
MIMEFAMILY GetMimeFamilyFromExtension(LPCSTR lpcszString)
{
	if(lpcszString && *lpcszString)
	{
		/* cerca l'ultimo '.' per trovare l'estensione */
		LPCSTR pExt = strrchr(lpcszString,'.');

		if(pExt)
		{
			char szExt[16/*_MAX_EXT=256!!!*/+1] = {0};
			strcpyn(szExt,pExt,sizeof(szExt));
			MIMETYPE* m;
			int iterator = 0;
			while((m = (MIMETYPE*)inet_enum_mime_types(&iterator))!=NULL)
				if(stricmp(szExt,m->ext)==0)
						return(m->family);
		}
	}

	return(FAMILY_UNKNOWN);
}

/*
	GetMimeFamilyFromContentType()

	Restituisce la family (FAMILY_TEXT, FAMILY_IMAGE, etc.) relativa alla Content-Type.
*/
MIMEFAMILY GetMimeFamilyFromContentType(LPCSTR lpcszContentType)
{
	if(lpcszContentType && *lpcszContentType)
	{
		/* estrae la parte principale del Content-Type (prima del ';') */
		char szMainType[256] = {0};
		strcpyn(szMainType,lpcszContentType,sizeof(szMainType));
		char* semicolon = strchr(szMainType,';');
		if(semicolon)
			*semicolon = '\0';

		MIMETYPE* m;
		int iterator = 0;
		while((m = (MIMETYPE*)inet_enum_mime_types(&iterator))!=NULL)
			if(stricmp(szMainType,m->type)==0)
					return(m->family);
	}

	return(FAMILY_UNKNOWN);
}

/*
	GetMimeTypeFromMimeFamily()

	Restituisce l'intero blocco MIMETYPE relativo alla family (FAMILY_TEXT, FAMILY_IMAGE, etc.) o NULL per errore.
*/
MIMETYPE* GetMimeTypeFromMimeFamily(MIMEFAMILY mimefamily,int* nIterator)
{
	static MIMETYPE mimetype_unknown = {GENRE_BINARY,FAMILY_APPLICATION,2,"application/octet-stream",".bin","Generic binary data; used when the file type is unknown or arbitrary."};

	MIMETYPE* m;
	while((m = (MIMETYPE*)inet_enum_mime_types(nIterator))!=NULL)
		if(mimefamily==m->family)
				return(m);

	return(&mimetype_unknown);
}

/*
	GetContentTypeFromMimeFamily()

	Restituisce la Content-Type relativa alla family (FAMILY_TEXT, FAMILY_IMAGE, etc.) o NULL per errore.
*/
LPCSTR GetContentTypeFromMimeFamily(MIMEFAMILY mimefamily,int* nIterator)
{
	MIMETYPE* m;
	while((m = (MIMETYPE*)inet_enum_mime_types(nIterator))!=NULL)
		if(mimefamily==m->family)
				return(m->type);

	/*
	non ritorna mai NULL per non scasinare il chiamante
	usa invece il tipo standard per "sconosciuto": application/octet-stream (.bin), che e' il tipo di default
	ufficiale per dati binari generici (RFC 2046: "should be used when a more specific type is inappropriate")
	*/
	return("application/octet-stream");
}

/*
	GetContentTypeFromExtension()

	Restituisce la (prima) Content-Type relativa all'estensione del file o NULL per errore.
	Possono esistere piu' content type a fronte di un estensione, es.:
	.js -> "text/javascript", "application/javascript"
	Assicurarsi che il parametro contenga una estensione valida prima di chiamare la funzione.
*/
LPCSTR GetContentTypeFromExtension(LPCSTR lpcszFilename)
{
	if(lpcszFilename && *lpcszFilename)
	{
		/* cerca l'ultimo '.' per trovare l'estensione */
		LPCSTR pExt = strrchr(lpcszFilename,'.');

		if(pExt)
		{
			MIMETYPE* m;
			int iterator = 0;
			while((m = (MIMETYPE*)inet_enum_mime_types(&iterator))!=NULL)
				if(stricmp(pExt,m->ext)==0)
					return(m->type);
		}
    }

	/*
	non ritorna mai NULL per non scasinare il chiamante
	usa invece il tipo standard per "sconosciuto": application/octet-stream (.bin), che e' il tipo di default
	ufficiale per dati binari generici (RFC 2046: "should be used when a more specific type is inappropriate")
	*/
	return("application/octet-stream");
}

/*
	GetExtensionFromContentType()

	Restituisce la (prima) estensione del file relativa alla Content-Type o NULL per errore.
	Possono esistere piu' estensioni a fronte di una content type, es.:
	"image/jpeg" -> .jpg, .jpeg, etc.
*/
LPCSTR GetExtensionFromContentType(LPCSTR pszContentType)
{
	if(pszContentType && *pszContentType)
	{
		/* estrae la parte principale del Content-Type (prima del ';') */
		char szMainType[256] = {0};
		strcpyn(szMainType,pszContentType,sizeof(szMainType));
		char* semicolon = strchr(szMainType,';');
		if(semicolon)
			*semicolon = '\0';

		MIMETYPE* m;
		int iterator = 0;
		while((m = (MIMETYPE*)inet_enum_mime_types(&iterator))!=NULL)
			if(stricmp(szMainType,m->type)==0)
				return(m->ext);
	}

	/*
	non ritorna mai NULL per non scasinare il chiamante
	usa invece il tipo standard per "sconosciuto": application/octet-stream (.bin), che e' il tipo di default
	ufficiale per dati binari generici (RFC 2046: "should be used when a more specific type is inappropriate")
	*/
	return(".bin");
}

/*
	WSAGetLastErrorString()

	Restituisce la descrizione relativa al codice d'errore Winsock
	(supplisce la mancanza dell'API ufficiale).

	In generale, tenere presente che oggigiorno la WSAGetLastError() e' praticamente un wrapper per GetLastError(),
	quindi se una qualsiasi API (ufficiale) restituisce il suo proprio codice, non sovrascriverlo con una chiamata a
	WSAGetLastError(), perche' questa potrebbe ricavare un qualsiasi codice impostato internamente dall'API.

	L'array in cui cerca deve essere mantenuto in ordine numerico crescente perche' usa una ricerca binaria.
*/
LPCSTR WSAGetLastErrorString(int nError)
{
	/*
	le macro seguenti stanno in Winsock2.h che NON viene incluso quando si definisce WIN32_LEAN_AND_MEAN
	quindi in tal caso deve definirle manualmente (i valori provengono dalle definizioni in winerror.h)
	*/
#ifdef WIN32_LEAN_AND_MEAN
	#define WSA_IO_PENDING          (ERROR_IO_PENDING)
	#define WSA_IO_INCOMPLETE       (ERROR_IO_INCOMPLETE)
	#define WSA_INVALID_HANDLE      (ERROR_INVALID_HANDLE)
	#define WSA_INVALID_PARAMETER   (ERROR_INVALID_PARAMETER)
	#define WSA_NOT_ENOUGH_MEMORY   (ERROR_NOT_ENOUGH_MEMORY)
	#define WSA_OPERATION_ABORTED   (ERROR_OPERATION_ABORTED)
#endif

	/*
	elenco dei codici d'errore Winsock e descrizioni relative, mantenere in ordine crescente
	NON esiste un API ufficiale che restituisca la descrizione a fronte del codice numerico:
	https://learn.microsoft.com/en-us/windows/win32/winsock/windows-sockets-error-codes-2
	*/
	static const WSA_ERRORS wsa_errors_array[] = {
		{WSA_INVALID_HANDLE,		"Specified event object handle is invalid."},			// 6
		{WSA_NOT_ENOUGH_MEMORY,		"Insufficient memory available."},						// 8
		{WSA_INVALID_PARAMETER,		"One or more parameters are invalid."},					// 87
		{WSA_OPERATION_ABORTED,		"Overlapped operation aborted."},						// 995
		{WSA_IO_INCOMPLETE,			"Overlapped I/O event object not in signaled state."},	// 996
		{WSA_IO_PENDING,			"Overlapped operations will complete later."},			// 997
		{WSAEINTR,					"Interrupted function call."},							// 10004
		{WSAEBADF,					"File handle is not valid."},							// 10009
		{WSAEACCES,					"Permission denied."},									// 10013
		{WSAEFAULT,					"Bad address."},										// 10014
		{WSAEINVAL,					"Invalid argument."},									// 10022
		{WSAEMFILE,					"Too many open files."},								// 10024
		{WSAEWOULDBLOCK,			"Resource temporarily unavailable."},					// 10035
		{WSAEINPROGRESS,			"Operation now in progress."},							// 10036
		{WSAEALREADY,				"Operation already in progress."},						// 10037
		{WSAENOTSOCK,				"Socket operation on nonsocket."},						// 10038
		{WSAEDESTADDRREQ,			"Destination address required."},						// 10039
		{WSAEMSGSIZE,				"Message too long."},									// 10040
		{WSAEPROTOTYPE,				"Protocol wrong type for socket."},						// 10041
		{WSAENOPROTOOPT,			"Bad protocol option."},								// 10042
		{WSAEPROTONOSUPPORT,		"Protocol not supported."},								// 10043
		{WSAESOCKTNOSUPPORT,		"Socket type not supported."},							// 10044
		{WSAEOPNOTSUPP,				"Operation not supported."},							// 10045
		{WSAEPFNOSUPPORT,			"Protocol family not supported."},						// 10046
		{WSAEAFNOSUPPORT,			"Address family not supported by protocol family."},	// 10047
		{WSAEADDRINUSE,				"Address already in use."},								// 10048
		{WSAEADDRNOTAVAIL,			"Cannot assign requested address."},					// 10049
		{WSAENETDOWN,				"Network is down."},									// 10050
		{WSAENETUNREACH,			"Network is unreachable."},								// 10051
		{WSAENETRESET,				"Network dropped connection on reset."},				// 10052
		{WSAECONNABORTED,			"Software caused connection abort."},					// 10053
		{WSAECONNRESET,				"Connection reset by peer."},							// 10054
		{WSAENOBUFS,				"No buffer space available."},							// 10055
		{WSAEISCONN,				"Socket is already connected."},						// 10056
		{WSAENOTCONN,				"Socket is not connected."},							// 10057
		{WSAESHUTDOWN,				"Cannot send after socket shutdown."},					// 10058
		{WSAETOOMANYREFS,			"Too many references."},								// 10059
		{WSAETIMEDOUT,				"Connection timed out."},								// 10060
		{WSAECONNREFUSED,			"Connection refused."},									// 10061
		{WSAELOOP,					"Cannot translate name."},								// 10062
		{WSAENAMETOOLONG,			"Name too long."},										// 10063
		{WSAEHOSTDOWN,				"Host is down."},										// 10064
		{WSAEHOSTUNREACH,			"No route to host."},									// 10065
		{WSAENOTEMPTY,				"Directory not empty."},								// 10066
		{WSAEPROCLIM,				"Too many processes."},									// 10067
		{WSAEUSERS,					"User quota exceeded."},								// 10068
		{WSAEDQUOT,					"Disk quota exceeded."},								// 10069
		{WSAESTALE,					"Stale file handle reference."},						// 10070
		{WSAEREMOTE,				"Item is remote."},										// 10071
		{WSASYSNOTREADY,			"Network subsystem is unavailable."},					// 10091
		{WSAVERNOTSUPPORTED,		"Winsock.dll version out of range."},					// 10092
		{WSANOTINITIALISED,			"Successful WSAStartup not yet performed."},			// 10093
		{WSAEDISCON,				"Graceful shutdown in progress."},						// 10101
		{WSAENOMORE,				"No more results."},									// 10102
		{WSAECANCELLED,				"Call has been canceled."},								// 10103
		{WSAEINVALIDPROCTABLE,		"Procedure call table is invalid."},					// 10104
		{WSAEINVALIDPROVIDER,		"Service provider is invalid."},						// 10105
		{WSAEPROVIDERFAILEDINIT,	"Service provider failed to initialize."},				// 10106
		{WSASYSCALLFAILURE,			"System call failure."},								// 10107
		{WSASERVICE_NOT_FOUND,		"Service not found."},									// 10108
		{WSATYPE_NOT_FOUND,			"Class type not found."},								// 10109
		{WSA_E_NO_MORE,				"No more results."},									// 10110
		{WSA_E_CANCELLED,			"Call was canceled."},									// 10111
		{WSAEREFUSED,				"Database query was refused."},							// 10112
		{WSAHOST_NOT_FOUND,			"Host not found."},										// 11001
		{WSATRY_AGAIN,				"Nonauthoritative host not found."},					// 11002
		{WSANO_RECOVERY,			"This is a nonrecoverable error."},						// 11003
		{WSANO_DATA,				"Valid name, no data record of requested type."},		// 11004
		{WSA_QOS_RECEIVERS,			"QoS receivers."},										// 11005
		{WSA_QOS_SENDERS,			"QoS senders."},										// 11006
		{WSA_QOS_NO_SENDERS,		"No QoS senders."},										// 11007
		{WSA_QOS_NO_RECEIVERS,		"QoS no receivers."},									// 11008
		{WSA_QOS_REQUEST_CONFIRMED,	"QoS request confirmed."},								// 11009
		{WSA_QOS_ADMISSION_FAILURE,	"QoS admission error."},								// 11010
		{WSA_QOS_POLICY_FAILURE,	"QoS policy failure."},									// 11011
		{WSA_QOS_BAD_STYLE,			"QoS bad style."},										// 11012
		{WSA_QOS_BAD_OBJECT,		"QoS bad object."},										// 11013
		{WSA_QOS_TRAFFIC_CTRL_ERROR,"QoS traffic control error."},							// 11014
		{WSA_QOS_GENERIC_ERROR,		"QoS generic error."},									// 11015
		{WSA_QOS_ESERVICETYPE,		"QoS service type error."},								// 11016
		{WSA_QOS_EFLOWSPEC,			"QoS flowspec error."},									// 11017
		{WSA_QOS_EPROVSPECBUF,		"Invalid QoS provider buffer."},						// 11018
		{WSA_QOS_EFILTERSTYLE,		"Invalid QoS filter style."},							// 11019
		{WSA_QOS_EFILTERTYPE,		"Invalid QoS filter type."},							// 11020
		{WSA_QOS_EFILTERCOUNT,		"Incorrect QoS filter count."},							// 11021
		{WSA_QOS_EOBJLENGTH,		"Invalid QoS object length."},							// 11022
		{WSA_QOS_EFLOWCOUNT,		"Incorrect QoS flow count."},							// 11023
		{WSA_QOS_EUNKOWNPSOBJ,		"Unrecognized QoS object."},							// 11024
		{WSA_QOS_EPOLICYOBJ,		"Invalid QoS policy object."},							// 11025
		{WSA_QOS_EFLOWDESC,			"Invalid QoS flow descriptor."},						// 11026
		{WSA_QOS_EPSFLOWSPEC,		"Invalid QoS provider-specific flowspec."},				// 11027
		{WSA_QOS_EPSFILTERSPEC,		"Invalid QoS provider-specific filterspec."},			// 11028
		{WSA_QOS_ESDMODEOBJ,		"Invalid QoS shape discard mode object."},				// 11029
		{WSA_QOS_ESHAPERATEOBJ,		"Invalid QoS shaping rate object."},					// 11030
		{WSA_QOS_RESERVED_PETYPE,	"Reserved policy QoS element type."}					// 11031
	};

	int left = 0;
	int right = (sizeof(wsa_errors_array) / sizeof(wsa_errors_array[0])) - 1;
    
	/* ricerca binaria */
	while(left <= right)
	{
		int mid = left + (right - left) / 2;
        
		if(wsa_errors_array[mid].code==nError)
			return(wsa_errors_array[mid].desc);
        
		if(wsa_errors_array[mid].code < nError)
			left = mid + 1;
		else
			right = mid - 1;
	}
    
	return("Unknown Winsock error");
}

/*
	GetLastInetResponse()

	Recupera informazioni dettagliate sull'ultimo errore o sulla risposta del server, in seguito
	al fallimento di una delle funzioni WinInet.
	E' specifica per protocolli FTP/Gopher, NON serve per protocolli come HTTP.

	Dopo il fallimento di una delle funzioni WinInet bisogna chiamare GetLastError() per ricavare
	il codice relativo, se GetLastError() restituisce ERROR_INTERNET_EXTENDED_ERROR allora si puo'
	usare la InternetGetLastResponseInfo() per recuperare informazioni aggiuntive oltre al semplice 
	codice numerico.

	Il testo restituito non include il carattere di terminazione zero nella lunghezza restituita,
	quindi il buffer passato deve essere grande abbastanza da contenerlo.
*/
BOOL GetLastInetResponse(LPSTR pBuffer,UINT nSize,DWORD* pdwError)
{
	memset(pBuffer,'\0',nSize);
	DWORD dwLength = nSize-1;
	return(InternetGetLastResponseInfo(pdwError,pBuffer,&dwLength));
}

/*
	GetInetErrorString()

	Ricava la descrizione relativa al codice d'errore, restituito da una delle funzioni Winsock o 
	WinInet.
	Se l'API chiamata non restituisce direttamente il codice d'errore, bisognerebbe chiamare 
	GetLastError() ed usare tale codice per passarlo a questa funzione.
	Distingue tre classi di codici, di rete, Winsock e WinInet, cercando nelle DLL relative la
	stringa per la descrizione, solo se non la trova cerca nell'array interno definito sopra.
*/
BOOL GetInetErrorString(DWORD dwError,LPSTR lpBuffer,DWORD dwSize)
{
	HMODULE hModule = NULL;
	BOOL bNeedsFree = FALSE;

	/* verifica se la DLL e' gia caricata, se no prova a caricarla */

	/* errore di rete, 2100 - 2999 */
	if(IS_NET_ERROR(dwError))
	{
		hModule = GetModuleHandle("netmsg.dll");
		if(!hModule)
		{
			hModule = LoadLibraryEx("netmsg.dll",NULL,LOAD_LIBRARY_AS_DATAFILE);
			if(hModule)
				bNeedsFree = TRUE;
		}
	}
	/* errore Winsock, 10000 - 11999 */
	else if(IS_WINSOCK_ERROR(dwError))
	{
		/*
		NON usare WSAGetLastError(): tale funzione su Windows moderni e' un alias di GetLastError(), quindi
		restituisce l'ultimo errore impostato da qualsiasi funzione di sistema.
		Se una delle funzioni WinInet fallisce, gia' imposta il codice d'errore corretto (il dwError qui),
		quindi il valore di WSAGetLastError() potrebbe essere qualsiasi cosa impostato internamente dal sistema.
		int nWSAError = WSAGetLastError();
		*/
		hModule = GetModuleHandle("ws2_32.dll");
		if(!hModule)
		{
			hModule = LoadLibraryEx("ws2_32.dll",NULL,LOAD_LIBRARY_AS_DATAFILE);
			if(hModule)
				bNeedsFree = TRUE;
		}
	}
	/* errore WinInet, 12000 - 12175 */
	else if(IS_WININET_ERROR(dwError)) 
	{
		hModule = GetModuleHandle("wininet.dll");
		if(!hModule)
		{
			hModule = LoadLibraryEx("wininet.dll",NULL,LOAD_LIBRARY_AS_DATAFILE);
			if(hModule)
				bNeedsFree = TRUE;
		}
	}

	DWORD dwFlags = FORMAT_MESSAGE_FROM_HMODULE |	/* cerca nel modulo specificato */
					FORMAT_MESSAGE_FROM_SYSTEM |	/* cerca nel sistema se non trova nel modulo */
					FORMAT_MESSAGE_IGNORE_INSERTS;	/* ignora sequenze di inserimento */

	DWORD dwResult = FormatMessage(	dwFlags,									/* flags per ricerca messaggio */
									hModule,									/* se NULL, FROM_SYSTEM dovrebbe compensare */
									dwError,									/* codice errore (es. 12007) */
									MAKELANGID(LANG_ENGLISH,SUBLANG_ENGLISH_US),/* lingua di default */
									lpBuffer,									/* buffer di output */
									dwSize,										/* dimensione buffer */
									NULL										/* nessun argomento variabile */
									);

	/* FormatMessage() ha recuperato la descrizione per il messaggio d'errore */
	if(dwResult > 0L)
	{
		/* elimina la coppia \r\n aggiunta da FormatMessage() */
		char* p = strpbrk(lpBuffer,"\r\n");
		if(p)
			*p = '\0';
	}
	else /* FormatMessage() NON ha recuperato una mazza, se si tratta di Winsock si appoggia su WSAGetLastErrorString() */
	{
		if(IS_WINSOCK_ERROR(dwError))
			snprintf(lpBuffer,dwSize,"%s",WSAGetLastErrorString(dwError));
		else
			snprintf(lpBuffer,dwSize,"Unknown error code: %lu",dwError);
	}

	/* scarica solo se ha usato LoadLibraryEx() */
	if(bNeedsFree && hModule)
		FreeLibrary(hModule);

	return(dwResult > 0L);
}

/*
	GetHttpErrorStatus()

	Restituisce il blocco informativo relativo al codice HTTP.

	L'array in cui cerca deve essere mantenuto in ordine numerico crescente perche' usa una ricerca binaria.
*/
const HTTPSTATUS* GetHttpErrorStatus(DWORD dwError)
{
	/*
	elenco dei codici d'errore HTTP e descrizioni relative
	mantenere in ordine crescente
	NON esiste un API ufficiale che restituisca la descrizione a fronte del codice numerico
	*/
	static const HTTPSTATUS HTTP_STATUS_TABLE[] = {
		/* 1xx Informational */
		{100, "Continue",						"Request received, please continue"},
		{101, "Switching Protocols",			"Switching to new protocol"},
		{102, "Processing",						"WebDAV: Processing request"},
		{103, "Early Hints",					"Preload hints for resources"},
    
		/* 2xx Success */
		{200, "OK",								"Request succeeded"},
		{201, "Created",						"Resource created successfully"},
		{202, "Accepted",						"Request accepted for processing"},
		{203, "Non-Authoritative Information",	"Modified response from origin"},
		{204, "No Content",						"Success, no content to return"},
		{205, "Reset Content",					"Reset document view"},
		{206, "Partial Content",				"Partial resource returned"},
		{207, "Multi-Status",					"WebDAV: Multiple status codes"},
		{208, "Already Reported",				"WebDAV: Already reported in previous response"},

		/* custom */
		{211, "Preserved by Size",				"Local resource skipped because local file bigger than remote"},
		{212, "Preserved by Existence",			"Local resource skipped because already exists"},
		{213, "Overwritten",					"Local resource was overwritten"},
		{214, "Proliferated",					"Local resource not overwritten, created a new one instead"},

		{226, "IM Used",						"Delta encoding applied"},
    
		/* 3xx Redirection */
		{300, "Multiple Choices",				"Multiple options for resource"},
		{301, "Moved Permanently",				"Resource moved permanently"},
		{302, "Found",							"Resource temporarily at different URI"},
		{303, "See Other",						"See other URI using GET"},
		{304, "Not Modified",					"Resource not modified since last request"},
		{305, "Use Proxy",						"Must access through proxy (deprecated)"},
		{307, "Temporary Redirect",				"Temporary redirect, method unchanged"},
		{308, "Permanent Redirect",				"Permanent redirect, method unchanged"},
    
		/* 4xx Client Error */
		{400, "Bad Request",					"Malformed request syntax"},
		{401, "Unauthorized",					"Authentication required"},
		{402, "Payment Required",				"Payment required (reserved)"},
		{403, "Forbidden",						"Server refusing access"},
		{404, "Not Found",						"Resource not found"},
		{405, "Method Not Allowed",				"HTTP method not allowed"},
		{406, "Not Acceptable",					"Cannot satisfy Accept headers"},
		{407, "Proxy Authentication Required",	"Proxy authentication needed"},
		{408, "Request Timeout",				"Server timeout waiting for request"},
		{409, "Conflict",						"Request conflicts with current state"},
		{410, "Gone",							"Resource permanently removed"},
		{411, "Length Required",				"Content-Length header required"},
		{412, "Precondition Failed",			"Precondition in headers failed"},
		{413, "Payload Too Large",				"Request entity too large"},
		{414, "URI Too Long",					"Request URI too long"},
		{415, "Unsupported Media Type",			"Media type not supported"},
		{416, "Range Not Satisfiable",			"Cannot satisfy Range header"},
		{417, "Expectation Failed",				"Expect header expectation failed"},
		{418, "I'm a teapot",					"Easter egg: I'm a teapot (RFC 2324)"},
		{421, "Misdirected Request",			"Request directed at wrong server"},
		{422, "Unprocessable Entity",			"WebDAV: Semantic errors in request (Uranium Pizza)"},
		{423, "Locked",							"WebDAV: Resource locked"},
		{424, "Failed Dependency",				"WebDAV: Previous request failed"},
		{425, "Too Early",						"Risk of replay attack"},
		{426, "Upgrade Required",				"Protocol upgrade required"},
		{428, "Precondition Required",			"Precondition header required"},
		{429, "Too Many Requests",				"Rate limit exceeded"},
		{431, "Request Header Fields Too Large","Headers too large"},
		{451, "Unavailable For Legal Reasons",	"Censorship/legal restriction"},

/* custom */
{499, "Persistence",	"Resource preserved"},
    
		/* 5xx Server Error */
		{500, "Internal Server Error", "Server encountered unexpected condition"},
		{501, "Not Implemented", "Functionality not implemented"},
		{502, "Bad Gateway", "Invalid response from upstream server"},
		{503, "Service Unavailable", "Server temporarily overloaded/down"},
		{504, "Gateway Timeout", "Upstream server timeout"},
		{505, "HTTP Version Not Supported", "HTTP version not supported"},
		{506, "Variant Also Negotiates", "Transparent content negotiation error"},
		{507, "Insufficient Storage", "WebDAV: Storage quota exceeded"},
		{508, "Loop Detected", "WebDAV: Infinite loop detected"},
		{510, "Not Extended", "Policy not satisfied (RFC 2774)"},
		{511, "Network Authentication Required", "Network authentication needed"},
    
		{0, "Unknown HTTP error code", "Unknown HTTP error code"} /* tappo */
	};

	int left = 0;
	int right = (sizeof(HTTP_STATUS_TABLE) / sizeof(HTTPSTATUS)) - 2;
    
	/* ricerca binaria */
	while(left <= right)
	{
		int mid = left + (right - left) / 2;

		if(HTTP_STATUS_TABLE[mid].code==dwError)
			return(&HTTP_STATUS_TABLE[mid]);

		if(HTTP_STATUS_TABLE[mid].code < (int)dwError)
			left = mid + 1;
		else
			right = mid - 1;
	}

	return(&HTTP_STATUS_TABLE[(sizeof(HTTP_STATUS_TABLE) / sizeof(HTTPSTATUS)) - 1]);
}

/*
	GetHttpErrorText()

	Helper, usa la GetHttpErrorStatus() per restituire solo la stringa con il testo
	relativo al codice numerico.
*/
const char* GetHttpErrorText(int code)
{
	const HTTPSTATUS* status = GetHttpErrorStatus(code);
	return(status ? status->reason : "Unknown HTTP status code");
}

/*
	GetHttpErrorDescription()

	Helper, usa la GetHttpErrorStatus() per restituire solo la stringa con la descrizione
	relativa al codice numerico.
*/
const char* GetHttpErrorDescription(int code)
{
    const HTTPSTATUS* status = GetHttpErrorStatus(code);
    return(status ? status->description : "Unknown HTTP status code");
}

/*
	MSDosPathToHttpUrl()

	Converte il pathname MS-DOS in formato URL HTTP, passando tutto in minuscolo.
	Per la conversione alloca un buffer che dovra' essere rilasciato dal chiamante.
	
	Es. c:\tmp\test\test1.html -> http://www.c.com/tmp/test/test1.html
*/
LPSTR MSDosPathToHttpUrl(LPCSTR lpcszMSDosPath)
{
	int len = strlen(lpcszMSDosPath) + 20;
	char* url = (char*)calloc(len,sizeof(char));

	if(url)
	{
		/* copia il pathname in locale e converte in minuscolo */
		char* p;
		strcpyn(url,lpcszMSDosPath,len);
		strlrw(url);
		
		/* se il pathname MS-DOS specifica una lettera per l'unita', usa questa lettera per in nome host */
		if((p = strstr(url,":\\"))!=NULL)
		{
			char host[MAX_URL_HOST+1] = {0};
			char drive[5] = {0};
			char driveletter = url[0];
			snprintf(host,sizeof(host),"http://www.%c.com/",driveletter);
			snprintf(drive,sizeof(drive),"%c:\\",driveletter);
			p = subst(url,drive,host);
			if(p)
			{
				strcpyn(url,p,len);
				free(p);
			}
		}

		/* converte gli '\' in '/' */
		p = url;
		while(*p)
		{
			if(*p=='\\')
				*p='/';
			p++;
		}
	}

	return(url);
}

/*
	MSDosPercentEncodeUnallowed()

	Converte i caratteri non consentiti per un nome file a percent-encoded.
	NON converte i caratteri ':' e '\' essenziali per il pathname, considera come unita' logica il nome
	del file o del pathname
*/
LPSTR MSDosPercentEncodeUnallowed(LPCSTR lpcszFilename,LPSTR lpBuffer,int nSize)
{
	/* NON converte: :\ ma SI converte: <>:"/|?* */
	int i = 0;
	const char* p = lpcszFilename;
	char szReserved[] = {"<>\"/|?*"};
	while(p && *p && i < nSize-1)
	{
		if(!strchr(szReserved,*p))
			lpBuffer[i++] = *p++;
		else
		{
			sprintf(&lpBuffer[i],"%%%02X",*p++);
			i+=3;
		}
	}

	return(lpBuffer);
}
