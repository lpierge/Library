/*$
	urlextractor.c
	Estrae le url da un file, ricercando per protocollo/tag HTML/estensione file.
	Luca Piergentili, giugno 25

	Vedi le note in urlextractor.h.
*/
#include "pragma.h"
#include "env.h"
#include "macro.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "strings.h"
#include "window.h"
#include "win32api.h"
#include <wininet.h>
#include "inet.h"
#include "url.h"
#include "urlparser.h"
#include "urlextractor.h"

#include "traceexpr.h"
#define _TRACE_FLAG			_TRFLAG_NOTRACE /* opzioni: _TRFLAG_NOTRACE, _TRFLAG_TRACEFILE, _TRFLAG_TRACECONSOLE, _TRFLAG_TRACEOUTPUT, _TRFLAG_TRACEBREAKPOINT */
//#define _TRACE_FLAG			_TRFLAG_TRACEOUTPUT /* opzioni: _TRFLAG_NOTRACE, _TRFLAG_TRACEFILE, _TRFLAG_TRACECONSOLE, _TRFLAG_TRACEOUTPUT, _TRFLAG_TRACEBREAKPOINT */
#define _TRACE_FLAG_INFO	_TRACE_FLAG
#define _TRACE_FLAG_WARN	_TRACE_FLAG
#define _TRACE_FLAG_ERR		_TRACE_FLAG

#define TAG_BUFFER_SIZE 8129

/*
	compare_asc/desc()

	Per il confronto degli elementi della lista durante l'ordinamento.
*/
static int compare_asc(const void* a,const void* b)
{
    const char** str1_ptr = (const char**)a;
    const char** str2_ptr = (const char**)b;
    return(strcmp(*str1_ptr,*str2_ptr));
}
static int compare_desc(const void* a,const void* b)
{
    const char** str1_ptr = (const char**)a;
    const char** str2_ptr = (const char**)b;
    return(strcmp(*str2_ptr,*str1_ptr));
}

/*
	url_list_sort()

	Ordina la lista delle url in modo ascendente (0) o discendente (1).
*/
void url_list_sort(URLLIST* list,int order)
{
    if(list && list->count > 1)
	{
		/* 0 ascendente, 1 discendente */
		if(order==0)
			qsort(list->urls,list->count,sizeof(char*),compare_asc);
		else if(order==1)
			qsort(list->urls,list->count,sizeof(char*),compare_desc);
	}
}

/*
	url_list_isalreadyin()
	
	Verifica se un URL gia' esiste nella lista, restituendo 1 se si o 0 se no.
	
	Il confronto e' case-insensitive.
*/
int url_list_isalreadyin(const URLLIST* list,const char* url)
{
    for(size_t i=0; i < list->count; ++i)
	{
        if(stricmp(list->urls[i],url)==0)
            return(1);
    }
    
	return(0);
}

/*
	url_list_initialize()

	Inizializza la lista, deve essere la prima funzione chiamata appena dopo la dichiarazione.
*/
void url_list_initialize(URLLIST* list)
{
	if(list)
	{
		list->urls = NULL;
		list->count = 0;
		list->capacity = 0;
	}
}

/*
	url_list_release()

	Rilascia le risorse asociate alla lista.
*/
void url_list_release(URLLIST* list)
{
    if(list==NULL)
		return;

    for(size_t i=0; i < list->count; ++i)
	{
		if(list->urls[i])
			free(list->urls[i]);
    }

	if(list->urls)
		free(list->urls);

    url_list_initialize(list);
}

/*
	url_list_add()

	Aggiunge la url alla lista, controllando se gia' esiste o meno.
	Alloca la stringa per la url da aggiungere alla lista, il chiamante deve passare un buffer locale e NON allocare la stringa.

	Restituisce 0 o -1 in caso d'errore (di allocazione).
*/
int url_list_add(URLLIST* list,const char* url)
{
	/* aggiunge solo se gia' non esiste */
    if(url_list_isalreadyin(list,url))
        return(0);

    /* se la capacita' della lista e' zero o piena, la raddoppia */
    if(list->capacity==0)
	{
        list->capacity = 4; /* capacita' iniziale */
        list->urls = (char**)calloc(list->capacity * sizeof(char*),sizeof(char*));
    }
	else if(list->count==list->capacity)
	{
        list->capacity *= 2; /* raddoppia */
        list->urls = (char**)realloc(list->urls, list->capacity * sizeof(char*));
    }

    if(list->urls==NULL)
        return(-1);

    /* alloca memoria per la nuova url e la copia */
    list->urls[list->count] = (char*)calloc(strlen(url) + 1,sizeof(char));
    if(list->urls[list->count]==NULL)
        return(-1);

	/* insifona la url nella lista */
	strcpy(list->urls[list->count],url);
    list->count++;

    return(0);
}

/*
	split_cgi_url()

	Suddivide la url cgi nelle due componenti e le aggiunge alla lista delle url.

	Dividere una url cgi e trattare le due componenti non ha senso in generale, dato che la risorsa in se' non e' nessuna
	delle due parti, ma il risultato dell'elaborazione del server a fronte di quanto indicato nel cgi.

	Qui divide e tratta le due parti della cgi come se fossero due risorse a se stanti, perche' una parte puo' essere una
	risorsa (ad es. il file .js) e l'altra pure, come nell'esempio della url di cui sotto, anche se non e' una cosa comune.
	Dato che una url a destra del '?' non e' normale e dato che qui interessa (a destra) solo se referenzia una risorsa,
	controlla che la parte a destra contenga un protocollo per considerarla valida.

	Quando si cercano url nei due lati della query, come qui, occhio a non dimenticarsi di aggiungere (anche) l'url cgi come 
	entita', (ossia l'url intera) se si vuole ricavare il risultato dell'elaborazione del cgi, che altrimenti andrebbe perso.
	
	Come regola generale le funzioni come la presente dovrebbero ricevere la url gia' 'pulita' (decodificata, etc.).

	url cgi tipica:
	https://www.googletagmanager.com/gtag/js?id=G-E2FWW2H4JQ
	e, anche se non e' molto ortodosso, a volte le url cgi possono contenere una url valida nel lato destro, come in:
	https://twitter.com/share?url=https://catalogo.cultura.gov.it/detail/HistoricOrArtisticProperty/0500653472
	per cui si occupa di estrarre quanto contenuto a sinistra/destra del ? e di aggiungere le due url risultanti alla lista.
	
	Ricordarsi che il chiamante DEVE AGGIUNGERE L'URL CGI (ANCHE) COME ENTITA' UNICA, ossia la cgi intera e non spezzata.

	Caso particolare:
	stessa url nei due casi, pero': 1) come presente nel file html, ossia codificata e 2) una volta decodificata (con tre '?'):
	
	1) https://api.whatsapp.com/send?text=Las+abejas+tambi%C3%A9n+se+mudan%3A+%C2%BFqu%C3%A9+hacemos+si+eligen+nuestra+casa%3F+https%3A%2F%2Ftheconversation.com%2Flas-abejas-tambien-se-mudan-que-hacemos-si-eligen-nuestra-casa-143282%3Futm_source%3Dwhatsapp%26utm_medium%3Dbylinewhatsappbutton
	2) https://api.whatsapp.com/send?text=Las abejas también se mudan: ¿qué hacemos si eligen nuestra casa? https://theconversation.com/las-abejas-tambien-se-mudan-que-hacemos-si-eligen-nuestra-casa-143282?utm_source=whatsapp&utm_medium=bylinewhatsappbutton
	
	quando il codice riceve la seconda, estrae la prima parte e la inserisce nella lista, elimina la parte relativa a text= perche' 
	shifta fino a incontrare il protocollo e risolve la ultima parte (una cgi annidata) in modo ricorsivo
*/
bool split_cgi_url(URLLIST* list,const char* url,bool checkurl)
{
	char* p;
	bool bAdded = false;

	/* verifica che sia una url cgi */
	if((p = (char*)strchr(url,'?'))!=NULL)
	{
		char* thaturl;
		int toturls;
		char url1[MAX_URL_LENGTH] = {0};
		char url2[MAX_URL_LENGTH] = {0};

		/* divide la query cgi nelle due componenti */

		/* copia quello che viene dopo il ? */
		strcpyn(url2,p+1,sizeof(url2));

		/* copia quello che viene prima del ? e lo tronca al ? */
		strcpyn(url1,url,sizeof(url1));
		*(p = strchr(url1,'?')) = '\0';
	
		/* per ognuno dei due buffer in cui ha suddiviso la query cgi */
		for(toturls=0; toturls < 2; toturls++)
		{
			thaturl = (toturls==0 ? url1 : url2);

			/* cerca un protocollo valido e controlla che non sia embedded, ossia circondato da testo */
			bool hasproto = false;
			char* protocol;
			int iterator = 0;
			while((protocol = (char*)inet_enum_internet_protocols(&iterator))!=NULL)
			{
				/* trovato, shifta la parte iniziale affinche' il protocollo coincida con inizio stringa */
				if((p = strstr(thaturl,protocol))!=NULL)
				{
					hasproto = true;
					strshift(thaturl,protocol);
					break;
				}
			}

			/* fa lo stesso di quale parte della query cgi (sinistra o destra) si tratti, se non c'e' un protocollo la scarta 
			occhio che questo controllo solo ha senso qui, dato che si tratta delle due componenti della query cgi, tale controllo
			non si puo' applicare alle url non-cgi perche' in tal caso bisogna considerare la possibilita' del merge con la parent 
			url */
			if(!hasproto)
				continue;

			/* controlla che la url (se e' tale), referenzi una risorsa */
			bool isvalidurl = true;
			if(checkurl)
			{
				URLDATA urldata = {0};
				strcpyn(urldata.url,thaturl,sizeof(urldata.url));
				url_parse(&urldata);

				/* per verificare se la risorsa e' una url valida in assoluto, e non relativamente ad un possibile merge con la parent url, 
				controlla che almeno uno tra path, file, query e fragment non sia vuoto, anche se questo lascia fuori le url (valide) che 
				solo contengono un host, come https://d7hftxdivxxvm.cloudfront.net, ma qui non interessa l'host, solo interessa la risorsa 
				completa */
				if(!*urldata.path && !*urldata.file && !*urldata.ext && !*urldata.query && !*urldata.ext)
					isvalidurl = false;

				/* per un ulteriore verifica per sapere se una risorsa e' una url valida in assoluto, e non relativamente ad un possibile merge 
				con la parent url, controlla la composizione del nome file e dell'estensione: nel primo caso, trattandosi di cgi, i caratteri 
				'&' e '=' sono indizi di parametri, non di nome file, e nel secondo caso, considera validi solo A-Z, 0-9, '-' e '.'
				ad es., una url come questa:
				https://assets.guim.co.uk/polyfill.io/v3/polyfill.min.js?rum=0&features=es6,es7,es2017,es2018,es2019,default-3.6,HTMLPictureElement,IntersectionObserver,IntersectionObserverEntry,URLSearchParams,fetch,NodeList.prototype.forEach,navigator.sendBeacon,performance.now,Promise.allSettled&flags=gated&callback=guardianPolyfilled&unknown=polyfill&cacheClear=1
				quando viene divisa in due, e la seconda parte analizzata con url_parse(), quest'ultima produce come output un file:
				"rum=0&features=es6,es7,es2017,es2018,es2019,default-3.6,HTMLPictureElement,IntersectionObserver,IntersectionObserverEntry,URLSearchParams,fetch,NodeList.prototype.forEach,navigator.sendBeacon,performance.now,Promise.allSettled&flags=gated&callback=guardianPolyfilled&unknown=polyfill&cacheClear=1"
				ed un estensione (troncata):
				".allSettled&flags=gated&callback=guardian"
				che chiaramente non lo sono, essendo in realta' l'intera linea i parametri del cgi
				*/
				if(*urldata.file)
					if(strchr(urldata.file,'&') || strchr(urldata.file,'='))
						isvalidurl = false;
				if(*urldata.ext)
				{
					char* p = urldata.ext;
					while(*p)
					{
						if((!isalpha(*p) & !isdigit(*p) && *p!='.' && *p!='-') || p-urldata.ext > 10)
						{
							isvalidurl = false;
							break;
						}
						p++;
					}
				}
			}

			if(isvalidurl)
			{
				/* aggiunge la url alla lista (una per ogni loop), occhio al barbatrucco della chiamata 
				ricorsiva per gestire i casi come quelli della url 'malformata' di cui sopra (con tre '?') */
				if(strchr(thaturl,'?'))
					split_cgi_url(list,thaturl,checkurl);
				else
					bAdded = url_list_add(list,thaturl)==0;
			}
			else
			{
				TRACEEXPR((_TRACE_FLAG_INFO,__FILE__,__LINE__,"split_cgi_url(): discarded url: %s\n",thaturl));
			}
		}
	}

	return(bAdded);
}

/*
	parse_json_and_add_to_urllist()

	Estrae la risorsa dall'etichetta json, controlla se si tratta di una url valida e la inserisce nella lista.
	Notare che l'etichetta puo' contenere piu' di una risorsa per volta.

	La differenza rispetto a come elabora gli attributi html, css e javascript (vedi sotto) risiede nel fatto che
	qui non si tratta di un attributo determinato, ma di codice json racchiuso nel tag <script>, identificato dal
	attributo 'type=' del tag e dove le risorse (url) sono identificate da una parola chiave (es. "url").

	Riceve in input il buffer contenente i dati (attributo + dati + delimitatore fine attributo), l'attributo, il 
	carattere usato come delimitatore fine attributo, l'intera stringa di ricerca e la lista a cui aggiungere la url.
*/
bool parse_json_and_add_to_urllist(const char* pTag,const char* pAttribute,char* found,URLLIST* urlList)
{
	/* array delle parole chiave che identificano le risorse (parole e risorse vanno racchiuse tra " e separate da :) */
	const char* jsons[] = {"\"url\"","\"image\"","\"contentUrl\"","\"ImageObject\"","\"VideoObject\"","\"embedUrl\"",NULL};

	int n = 0;
	char url[MAX_URL_LENGTH+1] = {0};
	char* begin = found;

	/* per tutte le parole chiave */
	for(int i=0; jsons[i]!=NULL; i++)
	{
		TRACEEXPR((_TRACE_FLAG_INFO,__FILE__,__LINE__,"parse json: tag: %s\n",jsons[i]));

		/* re-imposta, per la ricerca di ogni parola chiave, il puntatore ai dati json che seguono l'attributo "application/json" del tag <script> */
		found = begin;

		/* il blocco di dati json puo' contenere varie volte la stessa parola chiave */
		while(found && (found = stristr(found,jsons[i]))!=NULL)
		{
			/* salta la parola chiave ed estrae la risorsa (url) */
			found += strlen(jsons[i]);

			while(found && *found!='\"')
				found++;
			
			found++;
				
			n = 0;
			memset(url,'\0',sizeof(url));

			while(found && *found && *found!='\"' && n < sizeof(url))
				url[n++] = *found++;

			TRACEEXPR((_TRACE_FLAG_INFO,__FILE__,__LINE__,"parse json: resource: %s\n",url));

			/* verifica se la risorsa e' una url valida */
			URLPROBABILITY urlProbability = get_url_probability(url);
			if(urlProbability.probability >= 49)
			{
				/* aggiunge la url alla lista affinche' venga scaricato il risultato dell'elaborazione del server e poi
				aggiunge le due parti del cgi per separato, nel caso in cui referenziassero una risorsa a se stante (ad
				es un file .js o qualsiasi altra risorsa) */
				if(url_list_add(urlList,url)!=-1)
				{
					TRACEEXPR((_TRACE_FLAG_INFO,__FILE__,__LINE__,"parse json: added resource: %s\n",url));
					if(strchr(url,'?'))
					{
						split_cgi_url(urlList,url,true);
						TRACEEXPR((_TRACE_FLAG_INFO,__FILE__,__LINE__,"parse json: added cgi resource: %s\n",url));
					}
				}
			}
			else
			{
				TRACEEXPR((_TRACE_FLAG_INFO,__FILE__,__LINE__,"parse json: NOT an url: %s\n",url));
			}
		}
	}

	return(true);
}

/*
	parse_item_and_add_to_urllist()

	Estrae la risorsa dall'attributo/funzione/etichetta (html, css, javascript, json), controllando se si tratta di 
	una url valida e inserendola nella lista. Notare che l'attributo/etc. puo' contenere piu' di una risorsa per volta.

	Riceve in input il buffer contenente i dati (attributo + dati + delimitatore fine attributo), l'attributo, 0/1 per
	distinguere tra attributo singola/multi risorsa (1=singola risorsa e 0=multirisorsa), l'intera stringa di ricerca 
	e la lista a cui aggiungere la url.
*/
bool parse_item_and_add_to_urllist(const char* pTag,const char* pAttribute,const char* nSingleRes,char* found,URLLIST* urlList)
{
	/*
	- l'elenco dei delimitatori NON include lo spazio, ne il backslash '\'
	- la virgola solo si include per gli attributi multi-risorsa
	- il ';' si elimina per non perdersi i parametri delle url cgi come in:
	  "https://imagenes.elpais.com/resizer/v2/CGGYEBGZQNF6HJSPWEHVDPQUT4.jfif?auth=d97bdec83ba012dfca011457c9bacce81beb3487b17de4e710363f893680415f&amp;width=414&amp;height=233&amp;focal=915%2C353"
	- le url inserite direttamente nel codice javascript o css come in:
	  var myImage = "http://example.com/image.jpg";
	  potrebbero essere un problema al eliminare il ';' ???
	*/
    const char szDelimSingleRes[] = "\t\n\r\"'<>()[]|^`!";
    const char szDelimMultiRes[]  = "\t\n\r\"'<>()[]|^`,!";
	const char* pDelimiters = strcmp(nSingleRes,"1")==0 ? szDelimSingleRes : szDelimMultiRes;

	/* json va a parte */
	if(stricmp(pAttribute,"\"application/ld+json\"")==0 || stricmp(pAttribute,"\"application/json\"")==0)
		return(parse_json_and_add_to_urllist(pTag,pAttribute,found,urlList));

	/* il parametro #1, pTag, viene allocato e rilasciato dal chiamante, qui lavora su copia locale */
	char szTag[TAG_BUFFER_SIZE + 1] = {0};
	char szUrl[MAX_URL_LENGTH + 1] = {0};
	char* pUrl = NULL;
	URLPROBABILITY urlProbability;

	/*
	se il buffer che riceve in input contenente i dati (attributo + dati + delimitatore fine attributo) e' malformato
	per un motivo qualsiasi, puo' arrivare a contenere piu' di uno (stesso) attributo senza il delimitatore finale,
	es. \"data-url= data-url="google.com" >\", quindi per ovviare non basta saltare l'attributo all'inizio dei dati
	ricevuti in input, ma deve saltare fino a trovare l'ultimo attributo, l'unico della serie con il delimitatore 
	finale
	tenere ben chiaro che qui estrae i dati relativi all'attributo trovato dal chiamante, non ricerca tutti gli
	attributi, come invece fa il chiamante, chiamando per ognuno di essi questa funzione
	*/
	const char* p = pTag;
	int nTotAttributes = strcount(p,pAttribute);
	if(nTotAttributes >= 1)
	do {
		p = strstr(p,pAttribute) + strlen(pAttribute);
		nTotAttributes = strcount(p,pAttribute);

	 } while(nTotAttributes > 1);
	strcpyn(szTag,p,sizeof(szTag));
	p = szTag;

	/*
	gli attributi dei tag che vengono ricercati per estrarre le risorse, includono la risorsa tra doppi apici:

		<form action="https://example.com/submit-form">[...]</form>

	per cui, in un frammento il seguente, dove la parola 'action' e' casualmente usata come parte del nome della 
	url, 'action' non deve essere considerato un attributo html:

		<div [...] data-url="https://www.plaeditions.com/en/module/blockwishlist/action?action=deleteProductFromWishlist" [...]></div>

	controlla quindi che quanto segue il presunto attributo sia una risorsa, verificando che il primo carattere 
	dopo la risorsa (e saltando gli spazi) sia un apice ' o un doppio apice "
	*/
	bool hasmarks = true;
	{
		char* p = szTag;
		if(isspace((unsigned char)*p))
			while(isspace((unsigned char)*p++))
				;
		if(*p!='\"' && *p!='\'')
			hasmarks = false;
	}
	if(!hasmarks)
	{
		TRACEEXPR((_TRACE_FLAG_INFO,__FILE__,__LINE__,"parse: invalid attribute: %s\n",szTag));
		return(false);
	}

	/* analizza la stringa (una volta eliminato l'attributo), cercando la risorsa racchiusa tra i delimitatori */
	do {

		/* azzera il buffer per la risorsa (url) */
		memset(szUrl,'\0',sizeof(szUrl));

		while(p && *p && !strchr(pDelimiters,*p)) /* salta tutto cio' che NON e' delimitatore prima del delimitatore di inizio */
			p++;

		while(p && *p && strchr(pDelimiters,*p)) /* salta tutto cio' che E' delimitatore (di inizio) */
			p++;

		if(p && *p)
		{
			/* fino a che non incontra un nuovo delimitatore (o fine stringa) copia il nome della risorsa nel buffer */
			for(int i=0; p && *p && !strchr(pDelimiters,*p) && i < sizeof(szUrl); i++)
				szUrl[i] = *p++;

			/* delimitatore finale incontrato o fine stringa */
			TRACEEXPR((_TRACE_FLAG_INFO,__FILE__,__LINE__,"parse: resource: %s\n",szUrl));

			/* NON considera lo spazio come un delimitatore (vedi sopra) per questo lo cerca ora: gli attributi che prevedono un elenco 
			di risorse, come srcset o data-*, usano lo spazio per separare i valori tra i blocchi dell'elenco o gli elementi stessi del
			elenco:
				- <img srcset="/images/image-mobile.jpg 480w,/images/image-tablet.jpg 800w,[...]
				- data-images="/img/a.jpg /img/b.jpg"
			quindi ora controlla che la risorsa non sia in realta' un elenco, considerandola tale se trova uno spazio dentro il nome della
			risorsa una volta estratta dal tag
			*/

			/* elimina gli spazi ai bordi per evitare cicli a vuoto piu' sotto */
			strrtrim(szUrl);
			strltrim(szUrl);

			char szRes[MAX_URL_LENGTH + 1] = {0};
			char* pUrl = szUrl;
			char* space;
			int hasspace = 0;

			/* cicla solo se la risorsa contiene spazi interni, ossia piu' elementi invece di uno solo */
			do {
do_again:
				/* si va mangiando gli spazi (ossia estraendo le sottostringhe) da destra a sinistra */
				if((space = strrchr(pUrl,' ')))
				{
					strcpyn(szRes,space+1,sizeof(szRes));
					*space = '\0';
					hasspace=1;
				}
				else
					strcpyn(szRes,pUrl,sizeof(szRes));

				/* controlla che non sia percent-encoded */
				decode_percent_encoded_string(szRes);

				/* verifica se la risorsa e' una url valida */
				urlProbability = get_url_probability(szRes);
				if(urlProbability.probability >= 49)
				{
					/* cerca un protocollo valido e controlla che non sia embedded, ossia circondato da testo */
					/* //$ if(strstr(szRes,"mailto:")) */
					{
						bool hasproto = false;
						char* protocol;
						int iterator = 0;
						while((protocol = (char*)inet_enum_internet_protocols(&iterator))!=NULL)
						{
							/* trovato, shifta la parte iniziale affinche' il protocollo coincida con inizio stringa */
							if((p = strstr(szRes,protocol))!=NULL)
							{
								hasproto = true;
								strshift(szRes,protocol);
								break;
							}
						}
					}

					/* aggiunge la url alla lista affinche' venga scaricato il risultato dell'elaborazione del server e poi
					aggiunge le due parti del cgi per separato, nel caso in cui referenziassero una risorsa a se stante (ad
					es un file .js o qualsiasi altra risorsa) */
					if(url_list_add(urlList,szRes)!=-1)
					{
						TRACEEXPR((_TRACE_FLAG_INFO,__FILE__,__LINE__,"parse: added resource: %s\n",szRes));
						if(strchr(szRes,'?'))
						{
							split_cgi_url(urlList,szRes,true);
							TRACEEXPR((_TRACE_FLAG_INFO,__FILE__,__LINE__,"parse: added cgi resource: %s\n",szRes));
						}
					}
				}
				else
				{
					TRACEEXPR((_TRACE_FLAG_INFO,__FILE__,__LINE__,"parse: NOT an url: %s\n",szRes));
				}

				/* azzera il buffer d'appoggio per copiare la risorsa dopo ogni copia */
				memset(szRes,'\0',sizeof(szRes));

				/* per non ovviare la ultima sottostringa a sinistra quando gia' si e' mangiato tutti gli spazi */
				if(hasspace==1)
				{
					hasspace=0;
					goto do_again;
				}

			} while(strchr(pUrl,' '));

			while(p && *p && strchr(pDelimiters,*p)) /* salta tutto cio' che E' delimitatore (il delimitatore finale della risorsa appena copiata) */
				p++;
		}

	} while(p && *p); /* possono esserci piu' risorse nello stesso tag, continua quindi analizzando la stringa */

	return(true);
}

/*
	url_extract_by_proto()
 
	Analizza un file di testo per estrarre le url relative ai protocolli predefiniti.

	La funzione legge l'intero contenuto del file in un buffer mappato in memoria cercando la presenza dei protocolli 
	internet ed estrendo la url fino a incontrare un carattere delimitatore o la fine del buffer.
	La memoria per le url estratte viene allocata dinamicamente e deve essere liberata dal chiamante usando url_list_release().
	Se si verificano errori, la URLLIST restituita sara' vuota (count = 0, urls = NULL).
 */
int url_extract_by_proto(URLLIST* foundUrls,const char* filename)
{
	bool hasunicodeseq = false;

    if(foundUrls==NULL || filename==NULL)
        return(URLLIST_ERROR_INVALID_PARAM);

	/*
	- l'elenco dei delimitatori NON include lo spazio, ne il backslash '\'
	- la virgola solo si include per gli attributi multi-risorsa
	- il ';' si elimina per non perdersi i parametri delle url cgi come in:
	  "https://imagenes.elpais.com/resizer/v2/CGGYEBGZQNF6HJSPWEHVDPQUT4.jfif?auth=d97bdec83ba012dfca011457c9bacce81beb3487b17de4e710363f893680415f&amp;width=414&amp;height=233&amp;focal=915%2C353"
	- le url inserite direttamente nel codice javascript o css come in:
	  var myImage = "http://example.com/image.jpg";
	  potrebbero essere un problema al eliminare il ';' ???
	*/
    const char urlDelimiters[] = "\t\n\r\"'<>()[]|^`,!";

	/* mappa in memoria il file di input */
	MAPPEDFILE fileMap = {0};
	strcpyn(fileMap.szFileName,filename,sizeof(fileMap.szFileName));
	if(!OpenMappedFile(&fileMap))
        return(URLLIST_ERROR_CREATEFILEMAP);

	/* puntatore ai dati (buffer) e dimensione (file) */
    char* fileContent = fileMap.pData;
    QWORD fileSize = fileMap.qwFileSize;

    /* puntatore all'interno del buffer e dimensione */
    char* currentPos = fileContent;
    char* endOfFile = fileContent + fileSize;

	/* sovradimensiona il buffer per la url per gestire gli eventuali srcset, vedi sotto */
	int nUrl;
	char szUrl[8192/*MAX_URL_LENGTH+1*/] = {0};
	URLDATA urldata = {0};

	/* codice di ritorno */
	int nRet = URLLIST_ERROR_SUCCESS;

    /* ricerca per tutti i protocolli */
	char shortproto[32] = {0};
	char* protocol;
	int iterator = 0;
	char cFirstDelimiter = 0;

	while((protocol = (char*)inet_enum_internet_protocols(&iterator))!=NULL)
	{
		/* per ogni protocollo, si riposiziona all'inizio buffer (il file mappato in memoria) per iniziare la ricerca */
		currentPos = fileContent;

		/* cerca il protocollo, se non la trova passa al seguente */
		strcpyn(shortproto,protocol,sizeof(shortproto));
		char* p = strchr(shortproto,'/');
		if(p) *p = '\0';

		char* found = strstr(currentPos,shortproto);
		if(!found)
			continue;

		do {
			/* aggiorna il puntatore dentro del buffer per saltare (nel prossimo loop) l'occorrenza appena trovata e 
			proseguire cercando la seguente */
			currentPos = found + strlen(shortproto);

			nUrl = 0;
			memset(szUrl,'\0',sizeof(szUrl));

			/* salva il carattere precedente l'inizio del protocollo, per ottenere il delimitatore iniziale, il ciclo 
			sottostante terminera' l'estrazione della risorsa all'incontrare lo stesso carattere come delimitatore
			finale
			non considerare il delimitatore iniziale e terminare il ciclo per uno qualsiasi dei delimitatori della 
			lista e' un errore logico, dato che il nome della risorsa potrebbe contenere come carattere valido uno dei 
			delimitatori, ad es. "http://[...]/Nome File (14).jpg", facendo si che l'estrazione si interrompa in modo
			prematuro */
			if(*(found-1))
				cFirstDelimiter = *(found-1);
			else
				cFirstDelimiter = 0;
			TRACEEXPR((_TRACE_FLAG_INFO,__FILE__,__LINE__,"by protocol: delimiter,found: [%c,%c]\n",cFirstDelimiter,*found));

			/*
			copia la url fino ad incontrare il delimitatore
			qui il problema e' il tipo di dati che il codice si trova a dover analizzare solo basandosi sulla logica 
			dell'estrazione per protocollo, in concreto i dati json usano una codifica specifica, basata su sequenze
			di escape (\u..., \\, \", \r, etc., vedi le funzioni relative in urlparser.c)
			per questo i controlli con il carattere '\' e le chiamate alle decodifiche specifiche: per non rompere la
			logica della ricerca x protocollo ed essere costretti ad usare un codice ad hoc per gli script/codice json

			nota: come regola generale per il controllo nel while:
			offset = puntatore_con_incremento - puntatore_inizio
			still into = puntatore_con_incremento < (puntatore_inizio + dimensione_totale_del_buffer)
			*/
			while(	   found
					&& (found < fileContent+fileSize)
					&& *found
					&& cFirstDelimiter==0 ? (!strchr(urlDelimiters,*found)) : (*found!=cFirstDelimiter)
					&& nUrl < sizeof(szUrl)-1	/* generale */
					)
			{
				if(*found=='\\')				/* specifico x json */
				{
					if(*(found+1))
						if(strchr(urlDelimiters,*(found+1)))
							break;
				}
				szUrl[nUrl] = *found;			/* generale */
				nUrl++;
				found++;
			}
			while(szUrl[strlen(szUrl)-1]=='\\') /* specifico x json */
				szUrl[strlen(szUrl)-1] = '\0';

			/* aggiorna il puntatore dentro del buffer per proseguire con la seguente */
			currentPos = found;

			/* per url sciolte */
			if(ISSPACE(szUrl[0]))
				strltrim(szUrl),strrtrim(szUrl);

			TRACEEXPR((_TRACE_FLAG_INFO,__FILE__,__LINE__,"by protocol: url: [%d][%s]\n",strlen(szUrl),szUrl));

			/* la decodifica deve avvenire secondo l'ordine usato qui sotto */
			decode_unicode_escape_sequences(szUrl);
			decode_escape_sequences(szUrl);
			decode_percent_encoded_string(szUrl);

			/*
			ora che per l'estrazione della risorsa usa il delimitatore iniziale per arrivare a quello finale e decidere dove 
			inizia e dove finisce l'estrazione della risorsa (vedi sopra), le url contenute nell'attributo srcset vengono incluse
			tutte nello stesso buffer:
			url x download: "https://bikepacking.com/bikes/surly-cross-check-dream-commuter/"
			srcset: "https://bikepacking.com/wp-content/uploads/2020/07/Surly-Cross-Check-Commuter-8-960x641.jpg 960w, [...]"
			causando il seguente errore al considerare il set come una unica url:
			"error 12150 while downloading: https://bikepacking.com/wp-content/uploads/2020/07/Surly-Cross-Check-Commute[...]"
			quindi, se identifica euristicamente (qui siamo in estrazione x protocollo, non per tag html) un srcset, tokenizza
			il set a partire dalla fine (destra) verso l'inizio (sinistra), mangiandosi lo specificatore per la risoluzione, lo
			spazio e la virgola ed estraendo la url una per una in un ciclo fino ad esaurire il set
			*/
			int amount = 0;
			char szThatUrl[MAX_URL_LENGTH+1] = {0};
			while(1)
			{
				/* url(s) presenti nell'attributo html srcset */
				if(strcount(szUrl,shortproto) > 1 && strcount(szUrl," ") > 1 && strcount(szUrl,",") > 1)
				{
					amount=1;
					char* p = szUrl+strlen(szUrl)-1;
					if(p)
					{
						int i=0;
						while(*p && !IS_SPACE(*p))
							p--;
						while(*p && IS_SPACE(*p))
							p--;
						while(*p && !IS_SPACE(*p) && *p!=',')
							szThatUrl[i++] = *p--;
						*p = '\0';
						szThatUrl[i]='\0';
						strrev(szThatUrl);
					}
				}
				else
				{
					amount=0;
					strcpyn(szThatUrl,szUrl,sizeof(szThatUrl));
				}

				/* il controllo se la url e' cgi o meno lo fa dopo aver aggiunto la url alla lista: nel caso in cui la url non
				sia una url normale ma una query cgi, deve comunque aggiungere la query intera per ottenere il risultato della
				elaborazione del server
				in un secondo tempo, se procede, splitta la query e verifica ognuna delle due parti
				qui controlla la validita' dell url in base alla validita' della scomposizione */
				memset(&urldata,'\0',sizeof(urldata));
				strcpyn(urldata.url,szThatUrl,sizeof(urldata.url));
				if(url_parse(&urldata))
				{
					if(url_list_add(foundUrls,szThatUrl)!=-1)
					{
						TRACEEXPR((_TRACE_FLAG_INFO,__FILE__,__LINE__,"by protocol: added resource: [%d][%s]\n",strlen(szThatUrl),szThatUrl));
						if(strchr(szThatUrl,'?'))
						{
							split_cgi_url(foundUrls,szThatUrl,true);
							TRACEEXPR((_TRACE_FLAG_INFO,__FILE__,__LINE__,"by protocol: added cgi resource: [%d][%s]\n",strlen(szThatUrl),szThatUrl));
						}
					}
					else
					{
						nRet = URLLIST_ERROR_ADDURL;
						goto done;
					}
				}

				if(amount==0)
					break;
			}

		} while((found = strstr(currentPos,shortproto))!=NULL);
	}

done:

	CloseMappedFile(&fileMap);

    return(nRet);
}

/*
	url_extract_by_html()

	Analizza un file di testo per estrarre le url contenute nei tag html e nel codice css/javascript/json.
	Notare che per html la ricerca NON avviene per tag, ma per l'attributo del tag, con css e javascript ricerca il
	il nome della funzione e con json le parole chiave.

	La funzione legge l'intero contenuto del file in un buffer mappato in memoria, cercando gli attributi etc. che 
	possono contenere risorse (basicamente url).
	Le url trovate vengono aggiunte alla lista cosi' come sono, la risoluzione completa dell'url deve essere gestita 
	in una fase successiva dal chiamante.
	Le url malformate, offuscate o codificate (es. HTML entities, URL-encoding) non vengono decodificate o considerate.
	La memoria per le url estratte e' allocata dinamicamente e deve essere liberata dal chiamante usando url_list_release().

	BUG: allo stato attuale, da una url come questa:
	<a href="mailto:?subject=Pasar%20por%20Madrid%20%27mat%C3%B3%27%20al%20embajador%20norcoreano%20Kim.%20Y%20ahora%20lo%20%27resucitan%27&amp;body=https://www.elmundo.es/cronica/2019/06/20/5cf6927ffc6c83623a8b47af.html?emk=MAILSHARE">
	estrae questo:
	mailto:?subject=Pasar por Madrid 'mat�' al embajador norcoreano Kim. Y ahora lo 'resucitan'&amp;body=https://www.elmundo.es/cronica/2019/06/20/5cf6927ffc6c83623a8b47af.html?emk=MAILSHARE
	il che dal punto di vista della ricerca dei tag e' corretto, pero' andrebbe controllato con una strshift per esempio ???
 */
int url_extract_by_html(URLLIST* foundUrls,const char* filename)
{
    if(foundUrls==NULL || filename==NULL)
        return(URLLIST_ERROR_INVALID_PARAM);

	/*
	- l'elenco dei delimitatori NON include lo spazio, ne il backslash '\'
	- la virgola solo si include per gli attributi multi-risorsa
	- il ';' si elimina per non perdersi i parametri delle url cgi come in:
	  "https://imagenes.elpais.com/resizer/v2/CGGYEBGZQNF6HJSPWEHVDPQUT4.jfif?auth=d97bdec83ba012dfca011457c9bacce81beb3487b17de4e710363f893680415f&amp;width=414&amp;height=233&amp;focal=915%2C353"
	- le url inserite direttamente nel codice javascript o css come in:
	  var myImage = "http://example.com/image.jpg";
	  potrebbero essere un problema al eliminare il ';' ???
	*/
    const char urlDelimiters[] = "\t\n\r\"'<>()[]|^`,!";
	
	/*
		struttura tag html:
			 <a href="http://google.com">Google</a>
			'<' + 'id tag' + 'spazio' + 'attributo' + '=' + 'delimitatore' + 'risorsa' + 'delimitatore' + '>' + 'descrizione risorsa' + '<' + '/' + 'id_tag' + '>'
		
		struttura funzione css:
			url("www.google.com")
			'nome funzione' + '(' + 'delimitatore' + 'risorsa' + 'delimitatore' + ')'

		struttura json (ricerca la stringa : "application/ld+json" o "application/json" e poi le parole chiave):
			"url" : "[valore]", etc.
		
		il ciclo piu' sotto ricerca per attributo/funzione/parola chiave(json), considerando come risorsa tutto cio' 
		che e' compreso tra i delimitatori, assicurandosi di non oltrepassare la fine dell'attributo o della funzione
		(il carattere specificato nella seconda colonna)
		per il codice json invece cerca come 'attributo' la etichetta usata per il codice json embedded e ricerca poi
		le parole chiavi per estrarre le risorse

		bisogna considerare la virgola un separatore solo quando si sta esaminando un attributo o una funzione che e' 
		esplicitamente progettata per contenere piu' valori, se si sta analizzando un campo che contiene una singola 
		risorsa (come data-url o href), la virgola deve essere trattata come parte del valore stesso
		quindi 1=singola risorsa e 0=multirisorsa
	*/
	const char* htmlTags[][3] = {
			/* identificatore,			delimitatore finale,	multi-risorsa */
/*HTML*/	{"action=",					">",					"1"},
			{"archive=",				">",					"0"}, /* spazio */
			{"background=",				">",					"1"},
			{"cite=",					">",					"1"},
			{"code=",					">",					"1"},
			{"content=",				">",					"1"},
			{"data=",					">",					"1"},
			{"data-href=",				">",					"1"},
			{"data-image=",				">",					"1"},
			{"data-mediabook=",			">",					"1"},
			{"data-mp4=",				">",					"1"},
			{"data-path=",				">",					"1"},
			{"data-share-url=",			">",					"1"},
			{"data-url=",				">",					"1"},
			{"data-video-preview=",		">",					"1"},
			{"data-webm=",				">",					"1"},
			{"form=",					">",					"1"},
			{"formaction=",				">",					"1"},
			{"href=",					">",					"1"},
			{"imagesrcset=",			">",					"0"}, /* virgola */
			{"itemid=",					">",					"1"},
			{"itemtype=",				">",					"1"},
			{"longdesc=",				">",					"1"},
			{"manifest=",				">",					"1"},
			{"ping=",					">",					"0"}, /* spazio */
			{"poster=",					">",					"1"},
			{"profile=",				">",					"1"},
			{"src=",					">",					"1"},
			{"srcset=",					">",					"0"}, /* virgola */
			{"usemap=",					">",					"1"},
			{"xmlns=",					">",					"1"},
/*JSON*/	{"\"application/ld+json\"",	">",					"0"}, /* contenitore */
			{"\"application/json\"",	">",					"0"}, /* contenitore */
/*Css*/		{"url(",					")",					"1"},
			{"image(",					")",					"0"}, /* virgola puo' contenere piu' url */
			{"attr(",					")",					"1"},
			{"local(",					")",					"1"},
			{"cross-fade(",				")",					"0"}, /* virgola */
			{"element(",				")",					"1"},
/*Jvascrpt*/{"setAttribute(",			")",					"1"},
			{"fetch(",					")",					"1"},
			{"XMLHttpRequest(",			")",					"1"},
			{"sendBeacon(",				")",					"1"},
			{"createObjectURL(",		")",					"1"},
			{".src",					";",					"1"}, /* es. elementoImmagine.src = "https://example.com/nuova-immagine.jpg"; */
			{".href",					";",					"1"}  /* es. elementoLink.href = "/nuova-pagina.html"; */
			};
    const int nTotHtmlTags = sizeof(htmlTags) / sizeof(htmlTags[0]);

	/* mappa in memoria il file di input */
	MAPPEDFILE fileMap = {0};
	strcpyn(fileMap.szFileName,filename,sizeof(fileMap.szFileName));
	if(!OpenMappedFile(&fileMap))
        return(URLLIST_ERROR_CREATEFILEMAP);

	/* puntatore ai dati (buffer) e dimensione (file) */
    char* fileContent = fileMap.pData;
    QWORD fileSize = fileMap.qwFileSize;

    /* puntatore all'interno del buffer e dimensione */
    char* currentPos = fileContent;
    char* endOfFile = fileContent + fileSize;

	/* dimensionare enorme il buffer d'appoggio per l'estrazione del tag, per poter estrarre quanto piu' possibile */
	char szTag[TAG_BUFFER_SIZE+1] = {0};
	int nTag;

	/* codice di ritorno */
	int nRet = URLLIST_ERROR_SUCCESS;

	/* inizia la ricerca dell'attributo/funzione/etichetta json (che nel ciclo chiama erroneamente 'tag'...)*/
	const char* tag;
	for(int n=0; n < nTotHtmlTags; n++)
	{
		/* nome dell'attributo/funzione/etichetta dell'elemento corrente dell'array */
		tag = htmlTags[n][0];
		TRACEEXPR((_TRACE_FLAG_INFO,__FILE__,__LINE__,"extract: current tag: %s\n",tag));

		/* per ogni attributo/funzione/etichetta, si riposiziona all'inizio buffer (il file mappato in memoria) per iniziare la ricerca */
		currentPos = fileContent;

		/* cerca l'attributo/funzione/etichetta, se non lo trova passa al seguente */
		char* found = strstr(currentPos,tag);
		if(!found)
		{
			TRACEEXPR((_TRACE_FLAG_INFO,__FILE__,__LINE__,"extract: no content for tag: %s\n",tag));
			continue;
		}

		do {
			/* aggiorna il puntatore dentro del buffer per saltare (nel prossimo loop) l'occorrenza appena trovata e proseguire cercando la seguente */
			currentPos = found + strlen(tag);

			nTag = 0;
			memset(szTag,'\0',sizeof(szTag));

			/* copia il contenuto dell'attributo/funzione, NON la risorsa in se', fino ad incontrare il delimitatore finale
			per il codice json il contenuto che copia e' l'etichetta e l'estrazione della risorsa avviene in seguito
			per il controllo nel while, tenere a mente che:
			offset = puntatore_con_incremento - puntatore_inizio
			still into = puntatore_con_incremento < (puntatore_inizio + dimensione_totale_del_buffer) */
			while(	   found
					&& (found < fileContent+fileSize)
					&& *found
					&& *found!=htmlTags[n][1][0]
					&& nTag < sizeof(szTag)-1
					)
			{
				szTag[nTag] = *found;
				nTag++;
				found++;
				TRACEEXPR((_TRACE_FLAG_INFO,__FILE__,__LINE__,"extract: tag offset = %ld, file size %ld\n",found-fileContent,fileSize));
			}

			/* aggiorna il puntatore dentro del buffer per proseguire con il seguente */
			currentPos = found;

			TRACEEXPR((_TRACE_FLAG_INFO,__FILE__,__LINE__,"extract: tag value: %s\n",szTag));

			/* analizza il contenuto dell'attributo/funzione/etichetta, aggiungendolo alla lista delle url se considera che e' una (o PIU') url */
			parse_item_and_add_to_urllist(szTag,htmlTags[n][0],htmlTags[n][2],found,foundUrls);

		} while((found = strstr(currentPos,tag))!=NULL);
	}

	CloseMappedFile(&fileMap);

    return(nRet);
}

/*
	url_extract_by_ext()

	Analizza un file di testo per estrarre le url relative alle estensioni di file specificate.

	La funzione legge l'intero contenuto del file in un buffer mappato in memoria, cercando le estensioni di file specificate
	nella forma: ".jpg;.png;.html;.css[...]".

	La memoria per le URL estratte e' allocata dinamicamente e deve essere liberata dal chiamante usando url_list_release().

	Occhio ai caratteri che si usano come delimitatore, perche' ad es, includendo { e }, questa url:
	"https://<dominio>/<path>/thumbs/ast-200x150/2023-08/43/aa6cf9c4315cb5f523dd00a546d790825.mp4-200x150-{THUMB_ID}.jpg"
	viene estratta come ".jpg".

	Lo spazio non viene incluso nella lista dei delimitatori perche' puo' apparire nel nome del file, nell'elenco di url del 
	tag srcSet, etc.
 */
int url_extract_by_ext(URLLIST* foundUrls,const char* filename,const char* exts)
{
    if(foundUrls==NULL || filename==NULL)
        return(URLLIST_ERROR_INVALID_PARAM);

	/*
	- l'elenco dei delimitatori NON include lo spazio, ne il backslash '\'
	- la virgola solo si include per gli attributi multi-risorsa
	- il ';' si elimina per non perdersi i parametri delle url cgi come in:
	  "https://imagenes.elpais.com/resizer/v2/CGGYEBGZQNF6HJSPWEHVDPQUT4.jfif?auth=d97bdec83ba012dfca011457c9bacce81beb3487b17de4e710363f893680415f&amp;width=414&amp;height=233&amp;focal=915%2C353"
	- le url inserite direttamente nel codice javascript o css come in:
	  var myImage = "http://example.com/image.jpg";
	  potrebbero essere un problema al eliminare il ';' ???
	*/
	const char urlDelimiters[] = "\t\n\r\"'<>()[]|\\^`,!";

	/* mappa in memoria il file di input */
	MAPPEDFILE fileMap = {0};
	strcpyn(fileMap.szFileName,filename,sizeof(fileMap.szFileName));
	if(!OpenMappedFile(&fileMap))
        return(URLLIST_ERROR_CREATEFILEMAP);

	/* puntatore ai dati (buffer) e dimensione (file) */
    char* fileContent = fileMap.pData;
    QWORD fileSize = fileMap.qwFileSize;

    /* puntatore all'interno del buffer e dimensione */
	char* currentPos = NULL;
	char* found = NULL;
	char* ext = NULL;
	int iterator = 0;
	char szExt[16] = {0};
	char* pExt = (char*)exts;
	char szUrl[MAX_URL_LENGTH+1] = {0};

	/* codice di ritorno */
	int nRet = URLLIST_ERROR_SUCCESS;

    /* per tutte le estensioni specificate */
	while(1)
	{
		/* estrae l'estensione dalla lista ricevuta in input */
		memset(szExt,'\0',sizeof(szExt));
		for(int i=0; pExt && *pExt && *pExt!=';' && i < sizeof(szExt); i++)
			szExt[i] = *pExt++;
		pExt++;

		/* termina se ha estratto tutte le estensioni previste dalla stringa */
		if(*szExt)
		{
			/* ricava l'estensione (viene con il '.') */
			ext = szExt;

			/* per ogni estensione, si riposiziona all'inizio buffer (il file mappato in memoria) per iniziare la ricerca */
			currentPos = fileContent;

			/* cerca la estensione, se non la trova passa alla seguente
			la strextac() per far si che le ricerche per .htm e .html non si sovrappongano
			(ossia, non mi consideri valido .html se sta' cercando .html) */
			found = strextac(currentPos,ext);
			if(!found)
				continue;

			do {
				/* aggiorna il puntatore dentro del buffer per saltare (nel prossimo
				loop) l'occorrenza appena trovata e proseguire cercando la seguente */
				currentPos = found + strlen(ext);

				/* occhio: se il webmaster scrive, a proposito, una url tipo:
				https://<dominio>/<path>/thumbs/ast-200x150/2023-08/43/aa6cf9c4315cb5f523dd00a546d790825.mp4-200x150-
				e' ovvio che il "-200x150-" finale specifica qualcosa per il server, pero', a effetto del bot, questa stringa finale 
				solo e' rumore che va eliminato per poter estrarre la url correttamente
				quindi: per cercare la fine della url in un algoritmo di estrazione basato sull'estensione del file, non ci si puo' basare 
				sulla ricerca del delimitatore finale, perche' in questo caso quello che per il bot dovrebbe essere un delimitatore ('-') e'
				di fatto un carattere valido per le url, MA, piu' semplicemente, si deve tagliare la url 'decorata' dal webmaster giusto 
				dopo la lunghezza dell'estensione (.mp4)
				l'unico inconveniente e' che, cosi facendo, le url cgi (?) vengono tagliate fuori, quindi, prima di tagliare, bisogna 
				assicurarsi di controllare questo caso speciale
				i frammenti (#) non sono un problema perche' solo specificano un etichetta dentro della risorsa (il file html), non sono 
				una risorse per se', quindi vanno semplicemente ignorati (ossia tagliati fuori dalla url)
				*/
				bool bIsCgiUrl = false;
				char* savefound = found;

				/* se e' una utl cgi, taglia alla fine del (cercando il) delimitatore (finale) */
				while(found && *found && !strchr(urlDelimiters,*found))
				{
					if(*found=='?')
						bIsCgiUrl = true;
					found++;
				}
				found--;

				/* se non e' una url cgi, taglia alla lunghezza dell'estensione trovata, NON ricerca il delimitatore finale */
				if(!bIsCgiUrl)
				{
					found = savefound;
					found += strlen(ext)-1; /* occhio: il -1 per la stringa C 0 based */
				}

				int n = 0;
				bool bIsValidUrl = false;

				/* una volta raggiunta/determinata la fine della url (vedi sopra) retrocede cercando il delimitatore iniziale */
				while(found && *found && (found >= fileContent) && !strchr(urlDelimiters,*found))
				{
					found--;
					n++; /* lunghezza url */
				}

				memset(szUrl,'\0',sizeof(szUrl));
				memcpy(szUrl,found+1,n < sizeof(szUrl)-1 ? n : sizeof(szUrl)-1);

				/* controlli minimi sulla url prima di inserirla nella lista */

				/* url sciolte, come in questo frammento di codice html:
				"srcSet="https://[...]b65dbbe18.png?optimizer=image&amp;width=16&amp;quality=75 16w, https://[...]b65dbbe18.png?optimizer=image&amp;width=32&amp;quality=75 32w, [...]"
				la sintassi di HTML5 prevede il tag 'srcSet' per elencare piu' url, separate da una virgola: il ciclo di estrazione sopra, al non considerare 
				piu' lo spazio come un separatore, lo incorpora alla url, in questo caso separandole in base alla virgola, quindi qui sotto controlla che la
				url non si porti appresso uno di questi spazi trovati sopra */
				if(ISSPACE(szUrl[0]))
					strltrim(szUrl);
				if(ISSPACE(szUrl[strlen(szUrl)-1]))
					strrtrim(szUrl);

				/* solo estensione (ad es. un blocco dati JSON che descrive un immagine), non la include nella lista */
				bIsValidUrl = szUrl[0]!='.';

				/* aggiunge la url alla lista */
				if(bIsValidUrl)
				{
					/* aggiunge la url alla lista */
					n = url_list_add(foundUrls,szUrl);
					TRACEEXPR((_TRACE_FLAG_INFO,__FILE__,__LINE__,"by ext -> url_list_add(%s)\n",szUrl));
					if(bIsCgiUrl)
						bIsCgiUrl = split_cgi_url(foundUrls,szUrl,true);
				}

				/* aggiunta fallita */
				if(n==-1)
				{
					nRet = URLLIST_ERROR_ADDURL;
					goto done;
				}
			} while((found = strstr(currentPos,ext))!=NULL);
		}
		else
			break;
	}

done:

	CloseMappedFile(&fileMap);

    return(nRet);
}
