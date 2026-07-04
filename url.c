/*$
	url.c
	Tutto quanto relativo alle url.
	Luca Piergentili, 16/07/25
*/
#include "pragma.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "strings.h"
#include <ctype.h>
#include "url.h"

#include "traceexpr.h"
#define _TRACE_FLAG			_TRFLAG_NOTRACE /* opzioni: _TRFLAG_NOTRACE, _TRFLAG_TRACEFILE, _TRFLAG_TRACECONSOLE, _TRFLAG_TRACEOUTPUT, _TRFLAG_TRACEBREAKPOINT */
//#define _TRACE_FLAG			_TRFLAG_TRACEOUTPUT /* opzioni: _TRFLAG_NOTRACE, _TRFLAG_TRACEFILE, _TRFLAG_TRACECONSOLE, _TRFLAG_TRACEOUTPUT, _TRFLAG_TRACEBREAKPOINT */
#define _TRACE_FLAG_INFO	_TRACE_FLAG
#define _TRACE_FLAG_WARN	_TRACE_FLAG
#define _TRACE_FLAG_ERR		_TRACE_FLAG

/*
    normalize_unix_path()

	Risolve il pathname, restituendo NULL in caso d'errore.

	Anche se il nome della funzione fa riferimento al file system Unix, la funzione in se' trova uso intensivo nei
	pathname usati per le url.

	La funzione modifica la stringa passata come parametro che deve essere (almeno) <len> (ossia quanto specificato
	dal parametro), il che significa che passando una stringa letterale (es. char* p = "//foo";), si produrra un errore
	di segmentazione perche' le stringhe letterali sono spesso in memoria read-only, usare quindi: 

		char p[MAX_URL_LENGTH+1];
		strcpy(p,"//foo");

	Il chiamante deve dimensionare il parametro <len> sul massimo possibile (es. MAX_URL_LENGTH) perche' la funzione 
	puo' essere usata non solo per i pathname del filesystem, ma anche per le url.

	La gestione di /// (tre o più slash iniziali) e' impostata per collassare a //, se per qualche motivo bisogna mantenere 
	///, la logica initial_slashes dovrebbe essere ulteriormente affinata.

    "/a/b/../c/./d"        /a/c/d
    "/a/b/c/"              /a/b/c/
    "/a/b/../../c"         /c
    "/a/b/c/d.txt"         No changes
    "/"                    /
    "/.."                  /
    "foo/bar/../baz"       foo/baz
    "/home/user/documents/../projects/./my_file.txt" /home/user/projects/my_file.txt
    //a/b/c                //a/b/c  <-- Comportamento desiderato
*/
char* normalize_unix_path(char* path,int len)
{
	ASSERTEXPR(path!=NULL);

	TRACEEXPR((_TRACE_FLAG_INFO,__FILE__,__LINE__,"normalize_unix_path() original   <path>: %s\n",path));

	// controlli rapidi per evitare entrare nella logica sottostante, lunga e pesante
	if(!(strstr(path,"/../") || strstr(path,"/./") || strcmp(path,"..")==0 || strcmp(path,".")==0))
	{
		TRACEEXPR((_TRACE_FLAG_WARN,__FILE__,__LINE__,"normalize_unix_path() normalization NOT required\n"));
		return(path);
	}
	if(strcmp(path,"")==0)
	{
		TRACEEXPR((_TRACE_FLAG_WARN,__FILE__,__LINE__,"normalize_unix_path() got an empty path\n"));
		return(path);
	}

    char* segments[MAX_PATH_SEGMENTS+1] = {0};	/* stack per i puntatori ai segmenti */
    int stack_ptr = 0;

    /* usa strdup per lavorare su una copia pero' il risultato finale viene trascritto
	nel buffer originale che deve essere almeno MAX_URL_LENGTH */
    char* temp_path = _strdup(path);
    if(temp_path==NULL)
	{
		TRACEEXPR((_TRACE_FLAG_ERR,__FILE__,__LINE__,"normalize_unix_path(): strdup() failure\n"));
        return(NULL);
	}

    char* token;
    char* rest = temp_path;

    /* gestisce slash iniziali multipli */
    int initial_slashes = 0;
    while(path[initial_slashes]=='/')
        initial_slashes++;

    /* se ci sono piu' di due slash iniziali, la normalizzazione Unix ne mantiene solo uno (o due se // significativo)
    per un percorso tipo "//a/b", mantiene 2, per "///a/b", dovrebbe essere "//a/b" o "/a/b"
    la specifica URL/URI dice che // e' per l'autorita', qui mantiene i primi 2 slash se presenti, altrimenti 1 se assoluto */
    int starts_with_double_slash = (initial_slashes >= 2);
    int starts_with_single_slash_only = (initial_slashes==1);
    int ends_with_slash = (strlen(path) > 0 && path[strlen(path)-1]=='/');

    /* strsep modifica 'rest' e salta i delimitatori consecutivi, questo significa che "///a/b" diventera' i token "a" e "b"
    i token vuoti derivanti da "//" o "///" vengono ignorati */
    while((token = strsep(&rest,"/"))!=NULL)
	{
        if(strlen(token)==0 || strcmp(token,".")==0)
		{
            continue;			/* ignora segmenti vuoti o "." */
        }
		else if(strcmp(token,"..")==0)
		{
            if(stack_ptr > 0)
                stack_ptr--;	/* risale di un livello */
        }
		else					/* non scende sotto la root virtuale (stack_ptr==0) */
		{
            if(stack_ptr < MAX_PATH_SEGMENTS)
			{
                segments[stack_ptr++] = token; /* aggiunge il segmento allo stack */
            }
			else				/* stack overflow */
			{
				TRACEEXPR((_TRACE_FLAG_ERR,__FILE__,__LINE__,"normalize_unix_path(): stack overflow\n"));
                free(temp_path);
                return(NULL);
            }
        }
    }

    /* ricostruzione del percorso normalizzato nel buffer originale 'path' */
    int current_pos = 0;

    if(stack_ptr==0)
	{
        /*	se lo stack e' vuoto (es. path era ".", "/./", "/../", "///", ecc.), il path 
			normalizzato sara' "/" se iniziava con uno slash od era vuoto, ma con slash finale
			se era un path relativo, come "a/../", allora diventa ""
		*/
        if(starts_with_single_slash_only || starts_with_double_slash || ends_with_slash)
		{
            strcpy(path,"/");	/* se era assoluto o finiva con /, diventa '/' */
        }
		else
		{
            strcpy(path,"");	/* altrimenti diventa una stringa vuota (es. "." -> "") */
        }
		
		TRACEEXPR((_TRACE_FLAG_INFO,__FILE__,__LINE__,"normalize_unix_path() normalized <path> (empty stack): %s\n",path));
        free(temp_path);
		return(path);
    }

    /*	se il path originale iniziava con '//', lo mantiene nella ricostruzione
		questo e'il caso specifico per i path Unix che si comportano come URI/URL
	*/
    if (starts_with_double_slash) {
        path[current_pos++] = '/';
        path[current_pos++] = '/';
    }
    /* altrimenti, se iniziava con un singolo slash, ne aggiunge solo uno */
    else if(starts_with_single_slash_only)
	{
        path[current_pos++] = '/';
    }
    
    for(int i=0; i < stack_ptr; i++)
	{
		/*	aggiunge slash prima del segmento, a meno che non sia il primissimo segmento
			e non sia gia' stato aggiunto uno slash (o due) iniziali
		*/
        if(current_pos > 0 && path[current_pos-1]!='/')
		{
            if(current_pos >= len-1) /* controllo overflow prima di aggiungere slash */
			{ 
				TRACEEXPR((_TRACE_FLAG_ERR,__FILE__,__LINE__,"normalize_unix_path(): no room for slash\n"));
				free(temp_path);
				return(NULL);
            }
            path[current_pos++]='/';
        }
        /* si assicura di non superare la dimensione del buffer originale */
        int len_segment = strlen(segments[i]);
        if(current_pos + len_segment >= len)
		{
			TRACEEXPR((_TRACE_FLAG_ERR,__FILE__,__LINE__,"normalize_unix_path(): normalized path exceeds buffer size\n"));
            free(temp_path);
            return(NULL);
        }
        strcpy(path+current_pos,segments[i]);
        current_pos += len_segment;
    }

    /* aggiunge uno slash finale se il path originale lo aveva e il path risultante non e' solo "/" */
    if(ends_with_slash && current_pos > 0 && path[current_pos-1]!='/')
	{
        if(current_pos < len-1) /* controllo overflow */
		{
            path[current_pos++]='/';
        }
		else
		{
			TRACEEXPR((_TRACE_FLAG_ERR,__FILE__,__LINE__,"normalize_unix_path(): not enough space for trailing slash\n"));
            free(temp_path);
			return(NULL);
        }
    }

    path[current_pos]='\0';

    free(temp_path);

	ASSERTEXPR(path!=NULL);
	TRACEEXPR((_TRACE_FLAG_INFO,__FILE__,__LINE__,"normalize_unix_path() normalized <path>: %s\n",path));
    return(path);
}

/*
	get_url_probability()

	Verifica euristicamente se la stringa puo' contenere una o piu' risorse (url), assegnando o sottraendo punti a seconda
	se la caratteristica e' o no di tipo url.

	Usa moltiplicazione invece di somma (per di piu' moltiplicando per gli stessi o piu' alti valori) perche' lo scopo non e' 
	piu' scoprire se la stringa e' UNA url, ma verificare se la stringa contiene una o PIU' url, in altre parole, mentre nel 
	primo caso sommava semplicemente le corrispondenze fino a raggiungere (o meno) un totale, qui ora misura la densita' delle 
	probabilita', dove la densita' e' rappresentativa della crescita esponenziale e non lineare, delle probabilita'.
*/
URLPROBABILITY get_url_probability(const char* str)
{
	/* domini comuni di primo livello per il calcolo euristico */
	static const char* commonTlds[] = {".com", ".org", ".net", ".gov", ".edu", ".info", ".biz", ".co.uk"};
	static const int numTlds = sizeof(commonTlds) / sizeof(commonTlds[0]);

    int score = 1;
	int tot = 0;

    URLPROBABILITY result = {0};
    size_t len = strlen(str);

	/* stringa troppo corta */
	if(len < 10)
		score -= 20;

    /* schema url */
    score += strcount(str,"http://") * 40;
    score += strcount(str,"https://") * 40;
    score += strcount(str,"ftp://") * 40;
    
    /* identificatore protocollo internet */
    score += strcount(str, "://") * 15;

    /* domini di primo livello */
    for(int i = 0; i < numTlds; ++i)
        score += strcount(str,commonTlds[i]) * 20;

    if(strchr(str,'.'))
        score += 5;

	tot = strcount(str,"/");
	if(tot > 0)
		score *= 10;

	tot = strcount(str,"?");
	if(tot > 0)
		score *= 10;

	tot = strcount(str,"#");
	if(tot > 0)
		score *= 10;

    /* non considera gli spazi perche' possono essere presenti nei nomi file e nelle url, nelle url NON dovrebbero esserci MA ci sono
	if(strchr(str,' '))
        ;
	*/

    /* normalizza il risultato */
    if(score < 0)
		score = 0;
    if(score > 100)
		score = 100;

    result.probability = score;
    
	return(result);
}

/*
	get_file_url_probability()

	Euristica per indovinare se la stringa referenzia un file o una url (http), restituisce
	le probabilita' per i due tipi, o {-1,-1} in caso di errore (es. file non trovato).
*/
FILEURLPROBABILITY get_file_url_probability(const char* str)
{
    FILEURLPROBABILITY probability = {0,0};
    
    /* indizi positivi */
    if(strstr(str,"://"))
	{
        probability.url = 100;
        return(probability);
    }

    if(strstr(str,"www."))
        probability.url += 80;

    if(isalpha(str[0]) && str[1]==':' && str[2]=='\\')
	{
        probability.file = 100;
        return(probability);
    }

    if(str[0]=='\\' && str[1]=='\\')
	{
        probability.file = 100;
        return(probability);
    }

    /* indizi negativi/ambigui */
    /* caratteri proibiti per i nomi file Windows e url */
    const char* dosForbiddenChars = "*?\"<>|";

	/* non ms-dos */    
    if(strpbrk(str,dosForbiddenChars))
        probability.file -= 100;
    
	/* url */
    if(strchr(str,'/'))
	{
        probability.file -= 70;
        probability.url += 50;
    }

	/* ms-dos */
    if(strchr(str,'\\'))
	{
        probability.file += 70;
        probability.url -= 50;
    }

    /* indizi deboli */
    if(strchr(str,'.'))
	{
        if(probability.file <= 0 && probability.url <= 0)
		{
            probability.file += 20;
            probability.url += 20;
        }
		else if(probability.file > 0)
		{
            probability.file += 20;
        }
		else if(probability.url > 0)
		{
            probability.url += 20;
        }
    }

    /* normalizzazione punteggi */
    if(probability.file < 0)
		probability.file = 0;
    if(probability.url < 0)
		probability.url = 0;

    probability.file = probability.file > 100 ? 100 : probability.file;
    probability.url = probability.url > 100 ? 100 : probability.url;

	if(strcount(str,".") > 3)
		probability.file = probability.url = -1;

    return(probability);
}
