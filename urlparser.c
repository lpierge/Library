/*$
	urlparser.c
	Analisi e spezzettamento URL (che si fa presto a dire, ma tra il dire e il fare c'e' di mezzo il mare).
	Luca Piergentili, 12/07/25
*/
#include "pragma.h"
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "strings.h"
#include "url.h"
#include "urlparser.h"

#include "traceexpr.h"
#define _TRACE_FLAG			_TRFLAG_NOTRACE /* opzioni: _TRFLAG_NOTRACE, _TRFLAG_TRACEFILE, _TRFLAG_TRACECONSOLE, _TRFLAG_TRACEOUTPUT, _TRFLAG_TRACEBREAKPOINT */
//#define _TRACE_FLAG			_TRFLAG_TRACEOUTPUT /* opzioni: _TRFLAG_NOTRACE, _TRFLAG_TRACEFILE, _TRFLAG_TRACECONSOLE, _TRFLAG_TRACEOUTPUT, _TRFLAG_TRACEBREAKPOINT */
#define _TRACE_FLAG_INFO	_TRACE_FLAG
#define _TRACE_FLAG_WARN	_TRACE_FLAG
#define _TRACE_FLAG_ERR		_TRACE_FLAG

/*
	url_parse()

	Analizza e suddivide la url nei suoi componenti (un gioiellino di codice!!!).

	Perche' e' necessario il percent-encoding:
	------------------------------------------
	Per evitare ambiguita': un carattere nella url puo' avere 1) funzione strutturale, come il ? che indica l'inizio di una query, 
	oppure 2) funzionare come dato, come un ? che fa parte di una parola chiave in una ricerca. Senza la codifica, un parser non 
	saprebbe come interpretare correttamente l'url.
	
	Tipi di Caratteri da Codificare:
	Ci sono tre categorie di caratteri che devono essere codificati per garantire che l'url sia interpretata correttamente:

	- Caratteri riservati: caratteri che hanno un significato speciale nella sintassi delle URL, come ?, &, #, /, :, _ e =
	  se bisogna usarli come dati (es. nel nome di un file o nel valore di un parametro), allora bisogna codificarli
	  ad es., un URL come https://example.com/file?name e' malformata, mentre https://example.com/file%3Fname e' valida perche' il 
	  ? e' trattato come un carattere normale.

	- Caratteri "unsafe": caratteri che potrebbero essere modificati o mal interpretati dai sistemi che gestiscono le url. Il piu' 
	  comune e' lo spazio, che spesso viene convertito in %20 o in un + per non creare problemi.

	- Caratteri non-ASCII: una url deve essere composta solo da un set limitato di caratteri ASCII. Tutti i caratteri di altre lingue, 
	  come é, ü o ñ, devono essere percent-encoded per garantire la compatibilità universale.

	Poi ci sono casi come questi:
	https://d7hftxdivxxvm.cloudfront.net?height=1200&quality=80&resize_to=fill&src=https%3A%2F%2Fartsy-media-uploads.s3.amazonaws.com%2Fw_-E7LVv4odzxAoETgyrgQ%252F3dphoto_800.jpg&width=1200

	dove lo scopo di codificare percent-encoded caratteri perfettamente validi come "://" all'interno di un parametro (la query cgi) 
	e' quello di preservare la struttura dell'url esterna che li contiene

	considerando la gerarchia di una url si puo' immaginare una url come una matrioska, dove ogni livello ha le sue regole:
	- url esterna: https://d7hftxdivxxvm.cloudfront.net con la sua struttura tipica (https://, l'host, la query ?)
	- parametro della url: src=https%3A%2F%2Fartsy-media-uploads.s3.amazonaws.com%2F... dove l'intero valore del parametro src e'
	considerato un unica stringa di dati dall'url esterna

	ora, se il valore del parametro src non fosse codificato, l'url esterna diventerebbe:
	https://d7hftxdivxxvm.cloudfront.net?height=1200&quality=80&resize_to=fill&src=https://artsy-media-uploads.s3.amazonaws.com/...
	e a questo punto, il parser si confonderebbe, vedrebbe un altra stringa :// e / che non ha una funzione strutturale all'interno della 
	sua query, e il risultato sarebbe imprevedibile, potendo interpretare l'intera stringa successiva come parte del valore del parametro 
	src, o potendo fallire

	quindi, COME REGOLE GENERALE: scomporre l'url e poi applicare il percent decode solo a path, cgi e frammento.

	Note generali:
	--------------
	- il chiamante deve preoccuparsi di azzerare la struttura di input e di copiare la url nel campo relativo

	Diagramma/pseudo codice:
	------------------------
	- copiare url originale in locale

	- controllo x url nulla, eliminazione spazi inziali/finali

	- decodifica url:

	- verificare se ha '@' cercando da destra [1]
		se ha, copiare fino a/o inizio stringa in utente
				cercare ':' in utente
				se ha, copiare fino a '@' in password ed eliminare in utente a partire da ':'
				se non ha, seguente
		se non ha, seguente

	[MODIFICA_DA_VERIFICARE]:
	 prima di verificare proto, etc.:
		- cercare il frammento # e se presente estrarlo nel campo corrispondente
		- cercare la query ? e se presente estrarla nel campo corrispondente
	continuare con l'analisi come previsto (anche se bisognerebbe rimuovere i
	controlli originali per # e ? dato che sono stati appena fatti)

	- verificare se ha proto autoritativo (qui non considera i proto non-auth)
		se ha, estrarre e copiare resto in dominio
		se non ha
			se primo chr e' '/' o './' o '../'
				non ha dominio
				ha path
				copiare tutto in path
			se primo chr NON e' '/' o './' o '../'
				puo' essere dominio??
				puo' essere path??
				puo' essere file??
				copiare in dominio/file/path a seconda dell'euristica
	
	- verificare se ha dominio
		se ha, scorrere cercando '/'
				se trova '/', copiare tutto prima di '/' in dominio, copiare tutto dopo '/' incluso in path
					dopo la copia cercare lo specificatore della porta ':'
					se lo trova, copiare il numero porta ed eliminarlo dal dominio
					se non lo trova, seguente
				se non trova '/'
					seguente
		se non ha
			seguente

	- verificare se ha path
		se ha, cercare il fragmento '#' a partire da destra [2]
			se ha fragmento, salvarlo e troncare il path
		se ha, cercare la query '?'
			se ha query, salvarla e troncare il path
		normalizzare il path per './' e '../'
			se ha, cercare il primo punto a partire da destra (per trovare il file)
				se ha, copiare a ritroso in nome file fino a incontrare '/' o inizio stringa
				se non ha, seguente
			se non ha, seguente
		se ha un path
			se il path termina con '/' -> e' un path, saltare a analizzare file
			se il path non termina con '/' -> non e' un path, quindi copiarlo in nome file senza copiare il '/' iniziale e lasciare
											  come pathname unicamente '/'

	- analisi file (verificare che il file, se prevede un estensione, contenga un nome, ossia non sia solo <punto> + <estensione>)
		solo nome
			copiare in file, in nome ed azzerare estensione
		solo estensione
			copiare estensione in estensione ed azzerare file e nome
		nome + estensione
			copiare nome + estensione in file, nome in nome ed estensione in estensione

	- verifica dei campi della struttura: 
		tutti a 0, url non valida
		almeno uno tra path, file, query e frammento pieno, url valida

	- verifica finale sul nome file/estensione:
		una url come httpwww.pixiv.net/en/artworks/136672408, allo stato attuale del codice, genererebbe i seguenti valori:
		file: httpwww.pixiv.net/en/artworks/136672408
		name: httpwww.pixiv.net
		ext: .net/en/artworks/136672408
		quindi come ultimo controllo deve verificare che i tra campi (file, name, ext) non contengano caratteri invalidi
		come / etc.

	- fine

	[1]	schema://[user[:password]@]host[:port]/path?query#fragment
		schema://host[:port]/path?query#fragment
		[user[:password]@]
		http://user:password@noodle.com:8088/path?query#fragment

	[2] l'RFC stabilisce che il frammento viene sempre dopo la query, se viene prima della query non si considera frammento 
		ma parte della url, se ha 2 # dopo la query la url e' malformata. con la url seguente (malformata) query e file
		rimangono vuoti:
		http://blog.mysite.org/articoli/post#123.html?titolo=FAQ's e Guida&id_categoria=IT/Networking
		url: http://blog.mysite.org/articoli/post#123.html?titolo=FAQ's e Guida&id_categoria=IT/Networking
		proto: http
		host: blog.mysite.org
		port:
		path: /articoli/post
		fragment: 123.html?titolo=FAQ's e Guida&id_categoria=IT/Networking
		query:
		file:
*/
bool url_parse(URLDATA* pUrlData)
{
	char* p;

	/* copia la url da scomporre in locale, il chiamante DEVE copiare la url nel campo della struttura PRIMA di chiamare */
	char url[MAX_URL_LENGTH+1] = {0};
	strcpyn(url,pUrlData->url,sizeof(url));

	/* mettere tutti i controlli PREVI qui */
	strrtrim(url);
	strltrim(url);

	/* inizio estrazione: va per passi successivi, estraendo da sinistra a destra, gerarchicamente, e quello che rimane dopo
	l'estrazione di uno degli items lo copia nel seguente per l'estrazione successiva */

	/* SE e' presente la coppia usr/psw, estrae i valori sostituendoli con un carattere dummy '¿' e 'raggrinza' quindi la url 
	sostituendo i dummy con "" */
	if((p = (char*)strrstr(url,"@"))!=NULL)
	{
		/* distingue tre il carattere '@' usato per specificare la coppia utente/password ed il carattere '@' usato per
		separare i parametri, come in url tipo: https://appinformatica.vtexassets.com/_v/public/assets/v1/bundle/css/asset.min.css?v=3&files=theme,ticnova.appinformatica-store@0.6.77$style.common,ticnova.appinformatica-store@[...]
		qui, il carattere '@', per identificare un nome utente, deve essere compreso tra '://' e '/' */
		if(strinbtw(url,"@","://","/"))
		{
			int n = (p-url);
			int i = 0;
			url[n--] = '¿';
			while(i <= MAX_URL_USERNAME && n >= 0 && url[n]!='/')
			{
				pUrlData->username[i++]=url[n];
				url[n--] = '¿';
			}
			if((p = subst(url,"¿",""))!=NULL)
			{
				strcpyn(url,p,sizeof(url));
				free(p);
			}
			pUrlData->username[i]='\0';
			strrev(pUrlData->username);
			if((p=strchr(pUrlData->username,':'))!=NULL)
			{
				strcpyn(pUrlData->password,p+1,sizeof(pUrlData->password));
				*p = '\0';
			}
		}	
	}

	/*
	[MODIFICA_DA_VERIFICARE]:
	 prima di verificare proto, etc.:
		- cercare il frammento # e se presente estrarlo nel campo corrispondente
		- cercare la query ? e se presente estrarla nel campo corrispondente
	continuare con l'analisi come previsto (anche se bisognerebbe rimuovere i
	controlli originali per # e ? dato che sono stati appena fatti)
	*/
	if((p = strrchr(url,'#'))!=NULL)
	{
		if(p+1 && *(p+1))
		{
			strcpyn(pUrlData->fragment,p+1,sizeof(pUrlData->fragment));
			*p = '\0';
		}
	}
	if((p = strrchr(url,'?'))!=NULL)
	{
		if(p+1 && *(p+1))
		{
			strcpyn(pUrlData->query,p+1,sizeof(pUrlData->query));
			*p = '\0';
		}
	}

	/* SE e' presente il protocollo, lo estrae e copia tutto il resto nell'host */
	if((p = strstr(url,"://"))!=NULL)
	{
		int n = p-url;

		/* invece di usare strcpyn() in modo non ortodosso...
		strcpyn(pUrlData->proto,url,n+1); */

		/* ...copia solo i caratteri necessari */
		memset(pUrlData->proto,'\0',sizeof(pUrlData->proto));
		memcpy(pUrlData->proto,url,n);
		
		strcpyn(pUrlData->host,p+3,sizeof(pUrlData->host));
	}
	/* nessun protocollo o protocollo invalido/non-auth (solo i ':' senza '//'), deve decidere tra dominio, file o directory, vedi sotto */
	else if(!strchr(url,':'))
	{
		const char* common_file_extensions[] = { /* per euristica: estensioni files piu' comuni */
			".html",".htm",	".css",	".js",	".jpg",	".jpeg",".png",	".gif",
			".pdf",	".mp4",	".mp3",	".zip",	".xml",	".json",".txt",	".php",
			".asp",	".aspx",".svg",	".ico",	".webp",".avi",	".wav",	".doc",
			".docx",".xls",	".xlsx",".ppt",	".pptx",".rar",	".exe",	".apk",
			".mov",	".ttf",	".otf",	".eot",	".woff",".woff2",
			NULL
		};
		const char* common_tlds[] = { /* per euristica: domini piu' comuni */
			".com",	".org",	".net",	".de",	".br",	".ru",	".uk",	".jp",
			".it",	".fr",	".nl",	".pl",	".au",	".in",	".ca",	".cz",
			".es",	".ua",	".be",	".ch",	".co",	".ar",	".hu",	".ir",
			".eu",	".ro",	".at",	".gr",	".tr",	".vn",
			NULL
		};

		/* se arriva qui con una url che solo contiene un nome file, non lo considera path perche' non inizia 
		con /, per cui deve distinguere se si tratta di un file o di un domino (es. index.html / google.com) */
		//if(url[0]=='/')
		if((url[0]=='/') || (url[0]=='.' && url[1]=='/') || (url[0]=='.' && url[1]=='.' && url[2]=='/'))
		{
			/* inizia con /, lo considera come path */
			strcpyn(pUrlData->path,url,sizeof(pUrlData->path));
		}
		/* le url di cui sopra, index.html o google.com, non iniziano con /, pero' potrebbero terminare con /,
		quindi applica la prima euristica (vedi sotto) solo se non terminano con /, se invece terminano con /
		applica una seconda euristica per indovinare se si tratta di un dominio + root dir o di un dominio + 
		directory o di una directory */
		else
		{
			/* due euristiche per i due dilemmi: file/dominio e dominio/path
			no ha il / finale: file o dominio
			ha il / finale: dominio+root dir o directory */

			/* file o dominio */
			if(url[strlen(url)-1]!='/')
			{
				char ext[32] = {0};
				char* p = (char*)strrstr(url,".");
				/* se ha un . verifica se si tratta di un file + estensione o dominio, se non ha . non 
				puo' essere un dominio internet (pubblico, globale, si potrebbe esserlo di rete privata),
				lo considera quindi un file senza estensione */
				if(p && strlen(p) < 10)
				{
					strcpyn(ext,p,sizeof(ext));

					/* verifica se puo' essere file x l'estensione */
					for(int i=0; common_file_extensions[i]!=NULL; i++)
						if(stricmp(common_file_extensions[i],ext)==0)
						{
							strcpyn(pUrlData->file,url,sizeof(pUrlData->file));
							break;
						}
					/* verifica se puo' essere host x il dominio */
					for(int i=0; common_tlds[i]!=NULL; i++)
						if(stricmp(common_tlds[i],ext)==0)
						{
							strcpyn(pUrlData->host,url,sizeof(pUrlData->host));
							break;
						}
				}
				else
					strcpyn(pUrlData->file,url,sizeof(pUrlData->file));
			}
			else /* dominio+root dir, o dominio + path, o path, verifica che prevale, se . o / */
			{
				int nDots = strcount(url,".");
				int nBars = strcount(url,"/");
				bool isDomainWithRoot = false;
				bool isDomainWithPath = false;
				bool isPathWithoutDomain = false;

				if(nDots >= nBars && nBars==1 || nDots==1 && nBars==1)
					isDomainWithRoot = true;
				if(nDots <= 1 && nBars > 1)
					isDomainWithPath = true;
				if(nDots <= 0 && nBars >= 1)
					isPathWithoutDomain = true;

				if(isDomainWithRoot)
					strcpyn(pUrlData->host,url,sizeof(pUrlData->host));
				if(isDomainWithPath)
					strcpyn(pUrlData->host,url,sizeof(pUrlData->host));
				if(isPathWithoutDomain)
					strcpyn(pUrlData->path,url,sizeof(pUrlData->path));
			}
		}
	}
	
	/* SE e' presente l'host, lo estrae e copia tutto il resto nel pathname */
	if(*pUrlData->host)
	{	
		char* p = strstr(pUrlData->host,"/");
		if(p)
		{
			strcpyn(pUrlData->path,p,sizeof(pUrlData->path));
			*p = '\0';
		}
		p = strstr(pUrlData->host,":");
		if(p)
		{
			strcpyn(pUrlData->port,p+1,sizeof(pUrlData->port));
			*p = '\0';
		}
	}
	
	/* SE e' presente il pathname, lo estrae e copia tutto il resto nel file */
	if(*pUrlData->path)
	{	
		char* p = (char*)strrstr(pUrlData->path,"#");
		if(p)
		{
			strcpyn(pUrlData->fragment,p+1,sizeof(pUrlData->fragment));
			*p = '\0';
		}
		p = (char*)strchr(pUrlData->path,'?');
		if(p)
		{
			strcpyn(pUrlData->query,p+1,sizeof(pUrlData->query));
			*p = '\0';
		}

		/* normalizza gli eventuali ../ del pathname */
		if(pUrlData->path[0]=='/' && pUrlData->path[1]=='/')
		{
			/* un caso speciale sono le url che iniziano con // ("network-path references"), in tal 
			caso quanto compreso tra // ed il seguente / e' l'host e non una parte del pathname  */
			char* p;
			char host[MAX_URL_HOST+1]={0};
			char path[MAX_URL_PATH+1]={0};
			strcpyn(host,pUrlData->path,sizeof(host));
			p = strstr((host)+2,"/");
			if(p)
			{
				strcpyn(path,p,sizeof(path));
				*p='\0';
				normalize_unix_path(path,sizeof(path));
				strcpyn(pUrlData->path,host,sizeof(host));
				strcatn(pUrlData->path,path,sizeof(path));
			}

		}
		else
			normalize_unix_path(pUrlData->path,sizeof(pUrlData->path));

		/* SE e' presente il file, lo estrae */
		p = (char*)strrstr(pUrlData->path,".");
		if(p)
		{
			int i = 0;
			int n = strlen(pUrlData->path)-1;
			while(n >= 0 && pUrlData->path[n]!='/')
			{
				pUrlData->file[i++] = pUrlData->path[n--];
			}
			pUrlData->file[i] = '\0';
			strrev(pUrlData->file);
			pUrlData->path[++n] = '\0';
		}
	}
	/* SE e' presente il pathname, controlla che non sia un file sin estensione */
	if(*pUrlData->path)
	{
		/* se non termina con '/', assume che sia un file senza estensione */
		if(pUrlData->path[strlen(pUrlData->path)-1]!='/')
		{
			/* copia quanto presente, saltando l'eventuale '/' iniziale, nel nome file, lasciando il pathname a '/'
			se ha un pathname, dovrebbe trovare il '/' iniziale, o il piu' a destra se e' un pathname composto)
			inizia quindi a cercare da destra nel caso in cui fosse un pathname composto) */
			char* p = strrchr(pUrlData->path,'/');
			if(p && *p)
				if(*(p+1)) /* verifica che contenga qualcosa dopo il '/' */
				{
					strcpyn(pUrlData->file,p+1,sizeof(pUrlData->file));
					*p = '\0';
				}
			if(!*pUrlData->path) /* pathname vuoto (era un file), lo imposta quindi a '/' */
				strcpy(pUrlData->path,"/");
		}
	}

	/*	analisi nome file (verificare che il file, se prevede un estensione, contenga un nome, ossia non sia solo <punto> + <estensione>
		solo nome
			copiare in file, in nome ed azzerare estensione
		solo estensione
			copiare estensione in estensione ed azzerare file e nome
		nome + estensione
			copiare nome + estensione in file, nome in nome ed estensione in estensione
	*/
	/* solo nome */
	if(*pUrlData->file && !strchr(pUrlData->file,'.'))
	{
		strcpyn(pUrlData->name,pUrlData->file,sizeof(pUrlData->name));
		strcpy(pUrlData->ext,"");
	}
	if(*pUrlData->file && strchr(pUrlData->file,'.'))
	{
		/* solo estensione */
		if(pUrlData->file[0]=='.')
		{
			strcpyn(pUrlData->ext,pUrlData->file,sizeof(pUrlData->ext));
			strcpy(pUrlData->file,"");
			strcpy(pUrlData->name,"");
		}
		else /* nome + estensione */
		{
			char* p = strrchr(pUrlData->file,'.');
			if(p)
			{
				strcpyn(pUrlData->ext,p,sizeof(pUrlData->ext));
				strcpyn(pUrlData->name,pUrlData->file,sizeof(pUrlData->name));
				p = strchr(pUrlData->name,'.');
				if(p)
					*p = '\0';
			}
		}
	}

	/* applica il percent-decode solo a path, frammento e query, vedi note all'inizio */
	if(*pUrlData->path)
		decode_percent_encoded_string(pUrlData->path);
	if(*pUrlData->fragment)
		decode_percent_encoded_string(pUrlData->fragment);
	if(*pUrlData->query)
		decode_percent_encoded_string(pUrlData->query);

	/*
	per verificare se una risorsa e' una url valida in assoluto, e non relativamente ad un 
	possibile merge con la parent url, controlla se tutti i campi della struttura sono vuoti
	o se sono presenti almeno host, path, file, query e fragment
	*/
	bool isvalidurl = false;
	bool isallempty =		!*(pUrlData->proto)
						&&	!*(pUrlData->host)
						&&	!*(pUrlData->port)
						&&	!*(pUrlData->path)
						&&	!*(pUrlData->file)
						&&	!*(pUrlData->name)
						&&	!*(pUrlData->ext)
						&&	!*(pUrlData->query)
						&&	!*(pUrlData->fragment)
						&&	!*(pUrlData->username)
						&&	!*(pUrlData->password);
	if(isallempty)
	{
		isvalidurl = false;
	}
	else
	{
		if(*(pUrlData->host) || *(pUrlData->path) || *(pUrlData->file) || *(pUrlData->query) || *(pUrlData->fragment))
			isvalidurl = true;
	}

	/* verifica finale sul nome file/estensione */
	if(strchr(pUrlData->file,'/') || strchr(pUrlData->name,'/') || strchr(pUrlData->ext,'/'))
	{
		memset(pUrlData->file,'\0',sizeof(pUrlData->file));
		memset(pUrlData->name,'\0',sizeof(pUrlData->name));
		memset(pUrlData->ext,'\0',sizeof(pUrlData->ext));
		isvalidurl = false;
	}

	return(isvalidurl);
}

/*
	url_combine()

	Ri-combina la url (incompleta) usando i dati della parent url.
	Le due url delle due strutture URLDATA devono essere analizzate previamente con url_parse().

	Restituisce il puntatore alla url ricomposta (parametro #3).

	Note:
	- la ricomposizione si basa sui campi della struttura URLDATA, non sulla url in se', quindi
	  la url deve passare il filtro previo di url_parse()
	- NON modifica i campi delle strutture, il risultato va nel buffer per la url (parametro #3)
	- assicurarsi che la dimensione del buffer per ricomporre l'url sia minimo di MAX_URL_LENGTH
	- il chiamante deve preoccuparsi di passare gli argomenti gia' azzerati
	- 'parziale' e 'relativa' sono sinonimi (di incompleta) quando ci si riferisce a la url figlia 
	  della parent url
*/
char* url_combine(URLDATA* pParent,URLDATA* pUrl,char* pCombinedUrl,size_t nUrlsize/* >= MAX_URL_LENGTH */)
{
	char buffer[MAX_URL_LENGTH+1] = {0};

	/* copia la url (relativa) da ricomponere nella struttura locale*/
	URLDATA url = {0};
	memcpy((void*)&url,(void*)pUrl,sizeof(url));
	
	/* azzera il buffer per il risultato con la url ricomposta (come c..o ha funzionato fino ad ora...) */
	memset(pCombinedUrl,'\0',nUrlsize);

	/* SE l'url (relativa) ha un protocollo, allora e' assoluta e non ereda nulla dalla parent url, la ritorna direttamente */
	if(*url.proto)
	{
		strcatn(pCombinedUrl,url.url,nUrlsize);
		return(pCombinedUrl);
	}

	/* se l'url (relativa) inizia con // ("network-path references"), allora solo ereda il protocollo dalla parent url, il
	resto non si tocca nulla, notare che la stringa che viene subito dopo il //, concettualmente, e' l'host, e NON un pezzo 
	di pathname (normalizza gli eventuali ../ della url prima di restituirla)
	esempio:
	url relativa:	//dominio.mio/path1/path2
	parent url:		http://www.example.com
	risultato:		http://dominio.mio/path1/path2 */
	if(url.path[0]=='/' && url.path[1]=='/')
	{
		strcatn(pCombinedUrl,pParent->proto,nUrlsize);
		strcatn(pCombinedUrl,":",nUrlsize);
		normalize_unix_path(url.url,sizeof(url.url));
		strcatn(pCombinedUrl,url.url,nUrlsize);
		return(pCombinedUrl);
	}

	/* normalizza la url parziale prima di ricomporre, la parent url si suppone che sia gia' stata normalizzata */
	normalize_unix_path(url.path,sizeof(url.path));

	/* se l'url parziale non specifica protocollo, lo ereda, se presente, dalla parent url */
	if(!*url.proto && *pParent->proto)
		strcpyn(url.proto,pParent->proto,sizeof(url.proto));
	if(*url.proto)
	{
		strcatn(pCombinedUrl,url.proto,nUrlsize);
		strcatn(pCombinedUrl,"://",nUrlsize);
	}

	/* se l'url parziale non specifica usr/psw, lo ereda, se presente, dalla parent url */
	if(!*url.username && *pParent->username)
		strcpyn(url.username,pParent->username,sizeof(url.username));
	if(*url.username)
		strcatn(pCombinedUrl,url.username,nUrlsize);

	if(!*url.password && *pParent->password)
		strcpyn(url.password,pParent->password,sizeof(url.password));
	if(*url.password)
	{
		strcatn(pCombinedUrl,":",nUrlsize);
		strcatn(pCombinedUrl,url.password,nUrlsize);
		strcatn(pCombinedUrl,"@",nUrlsize);
	}
	else
	{
		if(*url.username)
			strcatn(pCombinedUrl,"@",nUrlsize);
	}

	/* se l'url parziale non specifica host, lo ereda, se presente, dalla parent url */
	if(!*url.host && *pParent->host)
		strcpyn(url.host,pParent->host,sizeof(url.host));
	if(*url.host)
		strcatn(pCombinedUrl,url.host,nUrlsize);

	/* se l'url parziale non specifica porta, la ereda, se presente, dalla parent url */
	if(!*url.port && *pParent->port)
		strcpyn(url.port,pParent->port,sizeof(url.port));
	if(*url.port)
	{
		strcatn(pCombinedUrl,":",nUrlsize);
		strcatn(pCombinedUrl,url.port,nUrlsize);
	}

	/* SE la url relativa ha un pathname che inizia con / ("absolute-path reference"), allora questo pathname 
	annulla (=sostituisce completamente) quello della parent url
	queste sono url che specificano una risorsa che si trova sullo stesso host e protocollo della parent url, ma 
	con un percorso assoluto rispetto alla root dell'host
	esempio:
	url relativa:	/new/location/page.html
	parent url:		http://example.com/some/path/
	risultato:		http://example.com/new/location/page.html
	*/
	if(*url.path && url.path[0]=='/')
	{
		/* prende come riferimento pUrl->szPath cosi come sta' per l'aggiunta a pCombinedUrl */
		strcpyn(buffer,url.path,sizeof(buffer));
	}
	/* SE la url relativa ha un pathname che NON inizia con / ("relative-path reference") significa che il suo percorso e' 
	relativo alla "directory base" della parent url, ossia che andra' combinato con quello della parent url, in base a come 
	termina la parent url */
	else
	{
		/* se il pathname della URL di base TERMINA con /
		la url relativa viene concatenata direttamente al pathname della url di base
		esempio:
		url relativa: sottocartella/immagine.png
		parent url: http://example.com/cartella/
		risultato: http://example.com/cartella/sottocartella/immagine.png */
		if(pParent->path[strlen(pParent->path)-1]=='/')
		{
			strcatn(buffer,pParent->path,sizeof(buffer));
			strcatn(buffer,url.path,sizeof(buffer));
		}
		/* se il pathname della URL di base NON TERMINA con /
		il segmento finale del pathname della url di base viene rimosso (come se fosse un file o una risorsa figlia) per 
		ottenere la directory di base, la URL relativa viene poi concatenata a questa directory ridotta
		esempio:
		url relativa: nuovo_file.pdf
		parent url: http://example.com/cartella/documento.html
		directory di base effettiva: http://example.com/cartella/
		risultato: http://example.com/cartella/nuovo_file.pdf */
		else if(pParent->path[strlen(pParent->path)-1]!='/')
		{
			char leaf[MAX_URL_LENGTH+1]={0};
			strcpyn(leaf,pParent->path,sizeof(leaf));
			char* p = (char*)strrstr(leaf,"/");
			if(p && *(p+1))
				*(p+1) = '\0';
			strcatn(buffer,leaf,sizeof(buffer));
			strcatn(buffer,url.path,sizeof(buffer));
		}
	}
	strcatn(pCombinedUrl,buffer,nUrlsize);

	if(!*url.file && *pParent->file)
		strcpyn(url.file,pParent->file,sizeof(url.file));
	if(*url.file)
	{
		if(pCombinedUrl[strlen(pCombinedUrl)-1]!='/')
			strcatn(pCombinedUrl,"/",nUrlsize);
		strcatn(pCombinedUrl,url.file,nUrlsize);
	}
	
	/* quando si ricompone una url parziale (relativa), l'eventuale query e frammento 
	presenti nella parent url si eliminano sempre (non vengono ereditati) */

	return(pCombinedUrl);
}

/*
	url_refill()

	Ricostruisce la url a partire dai dati presenti nella struttura.

	Note:
	- NON modifica i campi della struttura, il risultato va nel buffer per la url (parametro #1)
	- assicurarsi che la dimensione del buffer per ricomporre l'url sia minimo di MAX_URL_LENGTH

	Esempi:
	https://www.example-store.com/prodotti/cerca.php?categoria=elettronica&prezzo_max=500
	https://www.wikipedia.org/wiki/Programmazione#Sintassi_e_struttura
	https://api.github.com/users/octocat/repos?sort=updated&page=2#last-repo
	ftp://utente:password@ftp.server-dati.net/file-importanti/rapporto.pdf
	http://admin:segreto@dev.internal-network.com/configurazioni/impostazioni.html?id_utente=42#log (protocollo://utente:password@host/percorso/risorsa?query#frammento)
*/
char* url_refill(char* url, size_t size, URLDATA* u)
{
	/* azzera la url prima di ricostruirla */
    memset(url,'\0',size);

    /* protocollo */
    strcpyn(url,u->proto,size);
    if(*u->proto)
		strcatn(url,"://",size);

    /* utente e password */
    if(*u->username)
	{
        strcatn(url,u->username,size);
        if(*u->password)
		{
            strcatn(url,":",size);
            strcatn(url,u->password,size);
        }
        strcatn(url,"@",size);
    }
    
    /* host */
    strcatn(url,u->host,size);

    /* numero porta */
    if(*u->port)
	{
        strcatn(url,":",size);
        strcatn(url,u->port,size);
    }

    /* percorso e file */
    if(*u->path)
	{
		/* controlla che il percorso finisca con uno slash */
        strcatn(url,u->path,size);
        if(url[strlen(url)-1]!='/')
            strcatn(url,"/",size);
    }
    strcatn(url,u->file,size);

    /* query */
    if(*u->query)
	{
        strcatn(url,"?",size);
        strcatn(url,u->query,size);
    }

    /* frammento */
    if(*u->fragment)
	{
        strcatn(url,"#",size);
        strcatn(url,u->fragment,size);
    }

    return(url);
}

/*
	url_remove_params()

	Rimuove dalla URL il parametro specificato dal nome.
	Se si vuole rimuovere piu' di un parametro, separare i nomi con la virgola.
	Copia il risultato nel buffer di output, senza modificare l'originale.
*/
void url_remove_params(const char* szSourceUrl,const char* szParamsToRemove,char* szDestBuffer,size_t nDestSize)
{
	char szTmpUrl[MAX_URL_LENGTH+1] = {0};
	strcpyn(szTmpUrl,szSourceUrl,sizeof(szTmpUrl));

	memset(szDestBuffer,'\0',nDestSize);

	/* separa la URL dai parametri (query string) */
	char* pQueryStart = strchr(szTmpUrl,'?');
	if(!pQueryStart)
	{
		/* nessun parametro presente, copia la URL cosi' com'e' */
		strcpyn(szDestBuffer,szTmpUrl,nDestSize);
		return;
	}

	/* punta all'inizio dei parametri reali */
	*pQueryStart = '\0';
	char* pCurrentQuery = pQueryStart + 1;

	/* per ricostruire la query string pulita */
	char szNewQuery[MAX_URL_LENGTH+1] = {0};
	bool bFirstParam = TRUE;

	/* analizza i parametri esistenti uno per uno (separati da &) */
	char* pContext = NULL;
	char* pParam = strtok_s(pCurrentQuery,"&",&pContext);

	while(pParam!=NULL)
	{
		/* estrae il nome del parametro */
		char szParamName[128] = {0};
		char* pEqual = strchr(pParam,'=');
		size_t nLen = pEqual ? (size_t)(pEqual - pParam) : strlen(pParam);
        
        if(nLen >= sizeof(szParamName))
			nLen = sizeof(szParamName) - 1;
        strncpy_s(szParamName,sizeof(szParamName),pParam,nLen);
        szParamName[nLen] = '\0';

		/* controlla se e' presente nella lista dei parametri da eliminare */
		bool bSkip = FALSE;
		char szBlacklist[256] = {0};
		strcpyn(szBlacklist,szParamsToRemove,sizeof(szBlacklist));

		char* pBlackContext = NULL;
		char* pBlackItem = strtok_s(szBlacklist,",",&pBlackContext);
        
		while(pBlackItem!=NULL)
		{
			/* rimuove eventuali spazi bianchi nella lista dei parametri */
			while(*pBlackItem==' ')
				pBlackItem++;
            
			if(stricmp(szParamName,pBlackItem)==0)
			{
				bSkip = TRUE;
				break;
			}

			pBlackItem = strtok_s(NULL,",",&pBlackContext);
		}

		/* se non e' da eliminare, lo aggiunge alla nuova query string */
		if(!bSkip)
		{
			if(!bFirstParam)
				strcatn(szNewQuery,"&",sizeof(szNewQuery));
			strcatn(szNewQuery,pParam,sizeof(szNewQuery));
			bFirstParam = FALSE;
		}

		pParam = strtok_s(NULL,"&",&pContext);
	}

	/* ricostruisce la URL finale (base + ? + NewQuery) */
	snprintf(szDestBuffer,nDestSize,"%s%s%s",szTmpUrl,(strlen(szNewQuery) > 0) ? "?" : "",szNewQuery);
}

/*
	url_subst_param()

	Sostituisce il valore esistente del parametro con quello specificato.
	Copia il risultato nel buffer di output, senza modificare l'originale.
*/
void url_subst_param(const char* szSourceUrl,const char* szParamToSubst,const char* szNewValue,char* szDestBuffer,size_t nDestSize)
{
    char szTmpUrl[MAX_URL_LENGTH+1] = {0};
    strncpy_s(szTmpUrl,sizeof(szTmpUrl),szSourceUrl,_TRUNCATE);

	/* separa la URL dai parametri (query string) */
    char* pQueryStart = strchr(szTmpUrl,'?');
    
    /* se non c'e' una query string, ne crea una nuova per il parametro */
    if(!pQueryStart)
	{
        snprintf(szDestBuffer,nDestSize,"%s?%s=%s",szTmpUrl,szParamToSubst,szNewValue);
        return;
    }

    *pQueryStart = '\0';
    char* pCurrentQuery = pQueryStart + 1;

	/* per ricostruire la query string pulita */
    char szNewQuery[MAX_URL_LENGTH+1] = {0};
    bool bFound = FALSE;
    bool bFirstParam = TRUE;

    /* analizza i parametri esistenti */
    char* pContext = NULL;
    char* pParam = strtok_s(pCurrentQuery,"&",&pContext);

    while(pParam!=NULL)
	{
        /* estrae il nome */
        char szParamName[128] = {0};
        char* pEqual = strchr(pParam,'=');
        size_t nLen = pEqual ? (size_t)(pEqual - pParam) : strlen(pParam);

        if(nLen >= sizeof(szParamName))
			nLen = sizeof(szParamName) - 1;
        strncpy_s(szParamName,sizeof(szParamName),pParam,nLen);
        szParamName[nLen] = '\0';

        if(!bFirstParam)
			strcatn(szNewQuery,"&",sizeof(szNewQuery));

        /* controlla se e' il parametro da sostituire */
        if(stricmp(szParamName,szParamToSubst)==0)
		{
            /* aggiunge il parametro con il nuovo valore */
            strcatn(szNewQuery,szParamName,sizeof(szNewQuery));
            strcatn(szNewQuery,"=",sizeof(szNewQuery));
            strcatn(szNewQuery,szNewValue,sizeof(szNewQuery));
            bFound = TRUE;
        }
		else
		{
            /* aggiunge il parametro originale cosi' com'e' */
            strcatn(szNewQuery,pParam,sizeof(szNewQuery));
        }

        bFirstParam = FALSE;
        pParam = strtok_s(NULL,"&",&pContext);
    }

    /* se il parametro non esisteva nella URL originale, lo aggiunge alla fine */
    if(!bFound)
	{
        if(!bFirstParam)
			strcatn(szNewQuery,"&",sizeof(szNewQuery));
        strcatn(szNewQuery,szParamToSubst,sizeof(szNewQuery));
        strcatn(szNewQuery,"=",sizeof(szNewQuery));
        strcatn(szNewQuery,szNewValue,sizeof(szNewQuery));
    }

    /* ricostruisce la URL */
    snprintf(szDestBuffer,nDestSize,"%s?%s",szTmpUrl,szNewQuery);
}

/*
	url_get_param()

	Ricava il valore per il parametro specificato.
*/
void url_get_param(const char* szSourceUrl,const char* szParams,char* szValue,size_t nValue)
{
	memset(szValue,'\0',nValue);

	char szUrl[MAX_URL_LENGTH+1] = {0};
	strcpyn(szUrl,szSourceUrl,sizeof(szUrl));

    /* se non c'e' una query string, non puo' esserci nemmeno il parametro */
	char* pQuery = strchr(szUrl,'?');
	if(!pQuery)
		return;

	*pQuery++;

	const char* pDelimiter = "&";
	char* pParam = strtok(pQuery,pDelimiter);

	while(pParam)
	{
		char szParamName[128] = {0};
		char* pEqual = strchr(pParam,'=');
		size_t nLen = pEqual ? (size_t)(pEqual - pParam) : strlen(pParam);
        
		if(nLen >= sizeof(szParamName))
			nLen = sizeof(szParamName) - 1;
		strcpyn(szParamName,pParam,nLen+1);

		if(strcmp(szParamName,szParams)==0)
		{
			char* pValue = pParam + strlen(szParamName) + 1;
			for(int i=0; pValue && *pValue && *pValue!='&' && i < (int)nValue-1; i++,pValue++)
				szValue[i] = *pValue;
			return;
		}

		pParam = strtok(NULL,pDelimiter);
	}
}

/*
	url_add_param_string()

	Aggiunge il parametro ed il valore (stringa) relativo al buffer.
	Notare che "aggiunge", quindi il buffer puo' contenere altri valori precedenti.
*/
bool url_add_param_string(char* szDest,size_t nDestSize,const char* szParam,const char* szValue)
{
    char szPair[MAX_URL_LENGTH+1] = {0};

    /* costruisce la coppia "chiave=valore" */
    snprintf(szPair,sizeof(szPair),"%s=%s",szParam,szValue);

    size_t nCurrentLen = strlen(szDest);
    size_t nPairLen = strlen(szPair);
    
    /* calcola lo spazio necessario: coppia + (eventuale &) + terminatore null */
    size_t nNeeded = nCurrentLen + nPairLen + (nCurrentLen > 0 ? 1 : 0) + 1;

    if(nNeeded > nDestSize)
        return(FALSE);

    /* se il buffer non e' vuoto, aggiunge il separatore '&' */
    if(nCurrentLen > 0)
        strcatn(szDest,"&",nDestSize);

    /* aggiunge la nuova coppia */
    strcatn(szDest,szPair,nDestSize);

    return(TRUE);
}

/*
	url_add_param_number()

	Aggiunge il parametro ed il valore (numerico) relativo al buffer.
	Notare che "aggiunge", quindi il buffer puo' contenere altri valori precedenti.
*/
bool url_add_param_number(char* szDest,size_t nDestSize,const char* szParam,long long nValue)
{
	char szValStr[32] = {0};
	snprintf(szValStr,sizeof(szValStr),"%llu",nValue);
	return(url_add_param_string(szDest,nDestSize,szParam,szValStr));
}


/*
	url_add_params()

	Aggiunge alla URL la stringa con i parametri (nella forma chiave=valore).
*/
bool url_add_params(const char* szSourceUrl,const char* pParams,char* szNewUrl,size_t nNewUrl)
{
	memset(szNewUrl,'\0',nNewUrl);
	strcpyn(szNewUrl,szSourceUrl,nNewUrl);

	// verifica se la URL ha o meno il separatore per la query
	const char* p = strchr(szSourceUrl,'?');
	if(!p)
		strcatn(szNewUrl,"?",nNewUrl);

	int n = strlen(szNewUrl);
	if(szNewUrl[n-1]!='?')
		strcatn(szNewUrl,"&",nNewUrl);

	strcatn(szNewUrl,pParams,nNewUrl);

	return(TRUE);
}

/*
	url_translate_HTML_entity()

	Traduce la entita' HTML nel carattere corrispondente.

	Le entita' HTML vengono usate quando una URL contenente parametri query string e' inserita in un documento HTML/XML, JSON, etc.
	In altre parole, quando una URL deve essere inserita in un documento, bisogna codificare i caratteri definiti come entita' HTML,
	ossia <, >, &, " e '.

	Esempio:
	- plain URL originale:		https://example.com/page?param1=value1&param2=value2
	- URL codificata in HTML:	<a href="https://example.com/page?param1=value1&amp;param2=value2">[...]</a>

	Esistono varie entita' HTML, ad es.:
	- simboli matematici: &plus;, &minus;, &times;, &divide;
	- lettere greche: &alpha;, &beta;, &gamma;
	- valute: &euro;, &dollar;, &pound;, &yen;
	- frecce: &larr;, &uarr;, &rarr;, &darr;

	La distinzione tra le 5 predefinite XML (&lt; &gt; &amp; &quot; &apos;) ed il resto risiede nel fatto che le predefinite sono
	hardcoded nel parser XML stesso, ossia funzionano sempre, in qualsiasi documento XML/HTML, senza bisogno di definizioni esterne.
	Mentre le altre entita' HTML (come &plus; &euro; etc.) sono definite nel DTD (Document Type Definition) di HTML, non nel parser 
	XML di base.

	Esepmpio praico del problema:
	<?xml version="1.0"?>
	<!DOCTYPE root [
	  <!ENTITY euro "&#8364;">  <!-- definizione manuale necessaria -->
	]>
	<root>
	  &amp;      <!-- OK: predefinita -->
	  &lt;       <!-- OK: predefinita -->
	  &euro;     <!-- OK solo se definita sopra, altrimenti ERRORE -->
	</root>

	Pero' occhio perche' HTML5 non usa piu' DTD, quindi tutte le entita' carattere (&plus; &minus; &euro; etc.) sono riconosciute 
	nativamente dal parser. Ma in XML puro resta la distinzione cruciale.
*/
void url_translate_HTML_entity(char* url)
{
	static const char* entities[] = {"&lt;","&gt;","&amp;","&quot;","&apos;"};
	static const char* translations[] = {"<",">","&","\"","'"};
    char *p=NULL;
	int n=0;
	for(int i=0; i < ARRAY_SIZE(entities); i++)
	{
		while((p = strstr(url,entities[i]))!=NULL)
		{
			n = strlen(entities[i]);
			*p = translations[i][0];
			memmove(p + 1, p + n, strlen(p + n) + 1);
		}
	}
}

/*
	url_abbreviate()

	Abbrevia la URL eliminando i caratteri a partire dalla meta' della sua lunghezza.
	(a pensarci bene, la funzione e' un poco chorrada)
*/
char* url_abbreviate(const char* pUrl,int nSize,char* pOutput,size_t nOutput)
{
	memset(pOutput,'\0',nOutput);

	int nUrlLen = strlen(pUrl);
	
	if(nUrlLen <= nSize || nSize >= (int)nOutput)
	{
		strcpyn(pOutput,pUrl,nOutput);
		return(pOutput);
	}

	int nHalfSize = (nSize-5) / 2;

	memcpy(pOutput,pUrl,nHalfSize);
	strcatn(pOutput,"[...]",nOutput);

	const char* p = ((pUrl + (nUrlLen-1)) - nHalfSize) + 1;
	strcatn(pOutput,p,nOutput);
	
	return(pOutput);
}

/*
	decode_percent_encoded_string()

	Decodifica (modificandola) una stringa con caratteri percent-encoded.
	Tenere presente che deve lavorare a single-pass (una sola passata e via), non in ciclo, per non rompere la martrioska.

	Restituisce il puntatore alla stringa decodificata (la stessa passata in input) o NULL in caso di errore (es. url malformata).

	Esempi:
	https://www.example.com/documenti%20personali/rapporto%20annuale.pdf?data=2024-07-12%20ore%2014%3A30
	http://blog.mysite.org/articoli/post%23123.html?titolo=FAQ%27s%20e%20Guida&id_categoria=IT%2FNetworking#sezione%20finale
	http://blog.mysite.org/articoli/post#123.html?titolo=FAQ's e Guida&id_categoria=IT/Networking#sezione finale
	ftp://utente%40dominio.com:password%C3%A0speciale@ftpserver.net/backup%20completo/archivio.zip
	https://api.service.io/data?query=value%26another&filter%5Btype%5D=active
	https://www.google.com/search?q=url+encoding+c
	http://example.com/search?q=hello+world&category=programming
*/
char* decode_percent_encoded_string(char* encodedString)
{
    if(encodedString && strchr(encodedString,'%'))
	{
		char* read_ptr = encodedString;
		char* write_ptr = encodedString;
    
		while(*read_ptr!='\0')
		{
			if(*read_ptr=='%')
			{
				/* verifica che ci siano almeno altri 2 caratteri dopo '%' */
				if(!isxdigit((unsigned char)read_ptr[1]) || !isxdigit((unsigned char)read_ptr[2]))
				{
					/* stringa malformata (es. "%A" o "%"), per semplicita' qui solo copia '%' e segue
					implemenare una gestione piu' robusta... */
					*write_ptr++ = *read_ptr++;
				}
				else
				{
					/* converte i due caratteri esadecimali in un singolo byte */
					int val1 = hex_to_int(read_ptr[1]);
					int val2 = hex_to_int(read_ptr[2]);

					/* caratteri non esadecimali dopo '%' (errore, ma improbabile se isxdigit ha avuto successo) */
					if(val1==-1 || val2==-1)
						return(NULL); /* errore nella decodifica */
                
					*write_ptr++ = (char)((val1 << 4) | val2); /* combina i nibble */
					read_ptr += 3; /* avanza il puntatore di lettura di 3 posizioni (%XX) */
				}
			}
			else if(*read_ptr=='+') /* alcune codifiche usano '+' per lo spazio */
			{
				*write_ptr++ = ' ';
				read_ptr++;
			}
			else /* copia il carattere cosi' com'e' */
			{
				*write_ptr++ = *read_ptr++;
			}
		}
		*write_ptr = '\0'; /* termina la stringa decodificata */
	}

    return(encodedString);
}

/*
	decode_percent_encoded_string_a()

	Decodifica una stringa con caratteri percent-encoded allocando dinamicamente la nuova stringa decodificata.
	Tenere presente che deve lavorare a single-pass (una sola passata e via), non in ciclo, per non rompere la martrioska.

	Restituisce il puntatore alla stringa decodificata (allocata dinamicamente) o NULL in caso di errore (es. allocazione
	di memoria fallita).
*/
char* decode_percent_encoded_string_a(const char* encodedString)
{
	char* decodedString = NULL;

    if(encodedString)
	{
		/* la stringa decodificata sara' al massimo della stessa lunghezza dell'originale
		   (o piu' corta se ci sono molte sequenze %XX) */
		size_t len = strlen(encodedString);
		if((decodedString = (char*)calloc(len + 1,sizeof(char)))!=NULL)
		{
			size_t i = 0; /* indice per encodedString */
			size_t j = 0; /* indice per decodedString */

			while(i < len)
			{
				if(encodedString[i]=='%' && (i + 2) < len && isxdigit((unsigned char)encodedString[i+1]) && isxdigit((unsigned char)encodedString[i+2]))
				{
					/* trovata una sequenza %XX */
					int val1 = hex_to_int(encodedString[i+1]);
					int val2 = hex_to_int(encodedString[i+2]);

					/* se entrambi sono validi, decodifica */
					if (val1 != -1 && val2 != -1)
					{
						decodedString[j++] = (char)(val1 * 16 + val2);
						i += 3; /* salta i 3 caratteri (%XX) */
					}
					else
					{
						/* non e' una sequenza esadecimale valida, copia '%' come carattere normale */
						decodedString[j++] = encodedString[i++];
					}
				}
				else
				{
					/* non  un '%', oppure non e' una sequenza %XX valida, copia il carattere cosi' com'e' */
					decodedString[j++] = encodedString[i++];
				}
			}
			decodedString[j] = '\0'; /* termina la stringa decodificata */
		}
	}

    return(decodedString);
}

/*
	encode_string_to_percent_encoded()

	Codifica in percent-encoded i caratteri specificati.

	Restituisce un puntatore alla stringa codificata (allocata dinamicamente) o NULL in caso di errore 
	(es. allocazione memoria fallita).
*/
char* encode_string_to_percent_encoded(const char* str,const char* chars_to_encode)
{
	char* encodedString = NULL;

	if(str && chars_to_encode)
	{
		/* la stringa codificata puo' essere al massimo 3 volte piu' lunga (es. ' ' -> '%20') +1 per il terminatore null */
		size_t original_len = strlen(str);
		size_t max_encoded_len = original_len * 3 + 1; 
		
		if((encodedString = (char*)calloc(max_encoded_len,sizeof(char)))!=NULL)
		{
			size_t i = 0; /* indice per str */
			size_t j = 0; /* indice per encodedString */

			while(i < original_len)
			{
				unsigned char c = str[i];

				/* controlla se il carattere e' nella lista di quelli da codificare
				nota: strchr restituisce un puntatore se trova il carattere, NULL altrimenti */
				if(strchr(chars_to_encode,c)!=NULL)
				{
					/* codifica il carattere in %XX, sprintf scrive nel buffer, avanza 'j' di 3 */
					sprintf(&encodedString[j],"%%%02X",c);
					j += 3;
				}
				else
				{
					/* copia il carattere cosi' com'e' */
					encodedString[j++] = c;
				}
				i++;
			}
			encodedString[j] = '\0';
		}
    }

    return(encodedString);
}

/*
	decode_escape_sequences()

	Decodifica (modificando la stringa) i caratteri di escape JSON piu' comuni.
	Non gestisce la decodifica Unicode (\uXXXX), vedi funzione seguente.
 */
void decode_escape_sequences(char* str)
{
#if 1 /* versione..., sicura??? */
    char* read_ptr = str;
    char* write_ptr = str;

    while(*read_ptr)
	{
        if(*read_ptr=='\\')
		{
            read_ptr++; /* salta il backslash  */
            
            /* se il backslash e' l'ultimo carattere, non decodifica e si ferma */
            if(!*read_ptr)
                break;
            
            /* gestisce le sequenze di escape previste */
            switch(*read_ptr)
			{
                case '"':
                case '\\':
                case '/':
                    *write_ptr++ = *read_ptr++;
                    break;
                case 'b':
                    *write_ptr++ = '\b';
                    read_ptr++;
                    break;
                case 'f':
                    *write_ptr++ = '\f';
                    read_ptr++;
                    break;
                case 'n':
                    *write_ptr++ = '\n';
                    read_ptr++;
                    break;
                case 'r':
                    *write_ptr++ = '\r';
                    read_ptr++;
                    break;
                case 't':
                    *write_ptr++ = '\t';
                    read_ptr++;
                    break;
                default:
                    /* se la sequenza non e' valida, copia solo il carattere successivo, ignorando 
					   il backslash, in tal modo non lascia caratteri di escape non decodificati */
                    *write_ptr++ = *read_ptr++;
                    break;
            }
        }
		else
		{
            /* se non c'e' un backslash, copia il carattere così com'e' */
            *write_ptr++ = *read_ptr++;
        }
    }

    *write_ptr = '\0';
#else
    char* read_ptr = str;
    char* write_ptr = str;

    while(*read_ptr)
	{
		if(*read_ptr=='\\')
		{
            read_ptr++; // salta il backslash
            
            /* gestisce le sequenze di escape richieste */
            switch(*read_ptr)
			{
                case '"':
                case '\\':
                case '/':
                    *write_ptr = *read_ptr;
                    read_ptr++;
                    write_ptr++;
                    break;
                case 'b':
                    *write_ptr = '\b';
                    read_ptr++;
                    write_ptr++;
                    break;
                case 'f':
                    *write_ptr = '\f';
                    read_ptr++;
                    write_ptr++;
                    break;
                case 'n':
                    *write_ptr = '\n';
                    read_ptr++;
                    write_ptr++;
                    break;
                case 'r':
                    *write_ptr = '\r';
                    read_ptr++;
                    write_ptr++;
                    break;
                case 't':
                    *write_ptr = '\t';
                    read_ptr++;
                    write_ptr++;
                    break;
                default:
                    /* per sequenze sconosciute, copia il backslash come carattere letterale per evitare di perdere dati */
                    *write_ptr = '\\';
                    write_ptr++;
                    break;
            }
        }
		else
		{
            /* non c'e' un backslash, copia il carattere cosi' com'e' */
            *write_ptr = *read_ptr;
            read_ptr++;
            write_ptr++;
        }
    }
	
    *write_ptr = '\0';
#endif
}

/*
	decode_unicode_escape_sequences()

	Decodifica (modificando la stringa) i caratteri di escape Unicode (\uXXXX).

	Restituisce il puntatore alla stringa decodificata (quella ricevuta in input) 
	o NULL se non trova sequenze Unicode o per errore.
 */
char* decode_unicode_escape_sequences(char* str)
{
	if(!strstr(str,"\\u"))
        return(NULL);

    char* read_ptr = str;
    char* write_ptr = str;

    while(*read_ptr)
	{
        /* controlla se e' l'inizio di una sequenza di escape Unicode */
        if(*read_ptr=='\\' && *(read_ptr + 1)=='u')
		{
            /* verifica che ci siano i 4 caratteri esadecimali successivi */
            if(isxdigit(*(read_ptr + 2)) && isxdigit(*(read_ptr + 3)) && isxdigit(*(read_ptr + 4)) && isxdigit(*(read_ptr + 5)))
			{
                /* converte i 4 caratteri esadecimali in un valore intero */
                int val = hex_to_int(*(read_ptr + 2)) * 4096;	/* 16^3 */
                val += hex_to_int(*(read_ptr + 3)) * 256;		/* 16^2 */
                val += hex_to_int(*(read_ptr + 4)) * 16;		/* 16^1 */
                val += hex_to_int(*(read_ptr + 5));			/* 16^0 */

                /* scrive il carattere decodificato */
                *write_ptr = (char)val;

                /* sposta il puntatore di lettura di 6 posizioni (\uXXXX) */
                read_ptr += 6;
                write_ptr++;
            }
			else
			{
                /* sequenza malformata, copia il backslash e prosegue */
                *write_ptr = *read_ptr;
                read_ptr++;
                write_ptr++;
            }
        }
		else
		{
            /* copia il carattere non codificato così com'e' */
            *write_ptr = *read_ptr;
            read_ptr++;
            write_ptr++;
        }
    }

    *write_ptr = '\0';

    return(str);
}

/*
	hex_to_int()

	Converte un carattere esadecimale a intero.

	Restituisce il valore intero del carattere esadecimale (0-15) o -1 se 
	il carattere non e' una cifra esadecimale valida.
*/
int hex_to_int(char c)
{
    if(c >= '0' && c <= '9')
	{
        return c - '0';
    }
	else if(c >= 'a' && c <= 'f')
	{
        return 10 + (c - 'a');
    }
	else if(c >= 'A' && c <= 'F')
	{
        return 10 + (c - 'A');
    }
    
	return -1;
}
