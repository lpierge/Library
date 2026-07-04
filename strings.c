/*$
	strings.c
	Operazioni sulle stringhe.
	Luca Piergentili, 13/09/98
	lpiergentili@yahoo.com
*/
#include "pragma.h"
#include "env.h"
#include "macro.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <stdarg.h>
#include <ctype.h>
#include <time.h>
#include <math.h>
#include "algorithm.h"
#include "fastrand.h"
#include "typedef.h"
#include "typeval.h"
#include "strings.h"

#include "traceexpr.h"
//#define _TRACE_FLAG			_TRFLAG_TRACEOUTPUT // opzioni: _TRFLAG_NOTRACE, _TRFLAG_TRACEFILE, _TRFLAG_TRACECONSOLE, _TRFLAG_TRACEOUTPUT, _TRFLAG_TRACEBREAKPOINT
#define _TRACE_FLAG			_TRFLAG_NOTRACE // opzioni: _TRFLAG_NOTRACE, _TRFLAG_TRACEFILE, _TRFLAG_TRACECONSOLE, _TRFLAG_TRACEOUTPUT, _TRFLAG_TRACEBREAKPOINT
#define _TRACE_FLAG_INFO	_TRACE_FLAG
#define _TRACE_FLAG_WARN	_TRACE_FLAG
#define _TRACE_FLAG_ERR		_TRACE_FLAG

/* interne */
static int _strright(const char*,const char*,int ignorecase);
static int _strleft	(const char*,const char*,int ignorecase);

/*
	memnxor()
	
	Pone in XOR ogni carattere del buffer con tutti i caratteri della password.
	Non e' un semplice XOR, ma un algoritmo di offuscamento dove ogni byte del buffer viene 
	XORato con ogni singolo carattere della password.
	La logica e' un protocollo di offuscamento a chiave derivata customizzato che funziona 
	per verificare se la password fornita dall'utente e' la stessa utilizzata in fase di 
	compressione, senza memorizzare la password in chiaro.

	La lunghezza specifica il numero massimo di caratteri del buffer da mettere in xor.
*/
void memnxor(char* buffer,char* psw,size_t len)
{
	ASSERTEXPR(buffer && psw && len > 0);

	register int i;
	register char* p;

	for(i=0; i < (int)len; i++)
	{
		p = psw;
		for(; *p; p++)
			buffer[i] = (char)(buffer[i]^(*p));
	}
}

/*
    memnxor_salted()
    
    Aumenta la sicurezza dell'offuscamento XOR introducendo un salt.

    buffer: buffer contenente la password (P) in chiaro (che verra' offuscata)
    psw:    chiave principale (P)
    salt:   chiave secondaria (S)
    len:    lunghezza del buffer da offuscare (nPswLen)

	Note:
	Il modo piu' efficace per aumentare la sicurezza di una operazione di offuscamento come quello di memnxor(), 
	e' introdurre un elemento casuale e non segreto, noto come Salt.
	Se due utenti differenti usassero la stessa password, l'offuscamento realizzato da memnxor() sara' identico 
	per entrambi. La soluzione consiste nell'aggiungere un campo casuale e di lunghezza fissa che viene generato 
	casualmente e memorizzato in chiaro.
	Tale campo (il salt) viene quindi usato per modificare l'algoritmo XOR di cui sopra trasformandolo da XOR a 
	chiave statica in un XOR a chiave estesa che dipende sia dalla password che dal salt.
*/
void memnxor_salt(char* buffer,const char* psw,size_t len,const char* salt,size_t salt_len)
{
	ASSERTEXPR(buffer && psw && salt && len > 0 && salt_len > 0);

	register int i;
	register int j = 0; /* contatore per il salt */

	/* outer loop: per ogni byte del buffer (la password da offuscare) */
	for(i = 0; i < (int)len; i++)
	{
		register char* p = (char*)psw;
        
        /* XOR con tutti i caratteri della password (come in memnxor) */
		for(; *p; p++)
			buffer[i] = (char)(buffer[i] ^ (*p));

        /* XOR con il byte del salt, in modo ciclico */
        buffer[i] = (char)(buffer[i] ^ salt[j]);
        
        /* avanza il contatore del salt, resettando a zero quando supera la lunghezza */
        j++;
        if(j >= (int)salt_len)
			j = 0;
	}
}

/*
	strfirst()

	Tra i caratteri della substringa, cerca quello che appare per primo nella stringa.

	La logica consiste in ricercare ognuno dei caratteri della substringa nella stringa e se 
	trovato calcolare la distanza del carattere dall'inizio della stringa e confrontarla con 
	la distanza del precedente, mantenendo come corrente quello con la distanza minore.

	Restituisce il carattere che appare per primo.
*/
char strfirst(const char* str,const char* chars)
{
	ASSERTEXPR(str && chars);

	char c,chr = 0;
	const char* currentchar;
	int n,previousn = (int)strlen(str);
	while((c = *chars))
	{
		if((currentchar = strchr(str,c)))
		{
			n = (int)(currentchar - str);
			if(n < previousn)
			{
				previousn = n;
				chr = *currentchar;
			}
		}
		chars++;
	}
	return(chr);
}

/*
	strshift()
	
	Shifta (a perdita, non a rotazione) la stringa da destra verso sinistra (modificandola), 
	per far si' che inizi a quanto specificato dalla sottostringa, se trovata.

	Restituisce il puntatore alla stringa shiftata o NULL se non ha trovato la substringa.

	Esempio:

	char* str = "questa NON e' una stringa";
	char* sub = "NON";
	strshift(str,sub);
	printf(str); -> "NON e' una stringa"
*/
char* strshift(char* str,const char* substring)
{
	ASSERTEXPR(str && substring);

	/* cerca la sottostringa dentro della stringa */
	char* found = strstr(str,substring);
    
	if(found)
	{
		/* se la sottostringa coincide con l'inizio della stringa, non deve spostare nulla, pero deve restituire un positivo */
		if(found > str)
			memmove(str,found,strlen(found)+1); /* sposta i caratteri da <found> (incluso il NULL finale) a <str> */
		return(found);
    }

    return(NULL);
}

/*
	strsep()

	Estrae i tokens da una stringa considerando i separatori specificati.

	Restituisce il token trovato o NULL a fine ricerca.
	
	Esempio:

	#include <stdio.h>
	#include <string.h>

	int main() {
		char str[] = "Esto es una cadena de ejemplo";
		char *p = str;
		const char *delimiters = " ";
		char *token;

		while((token = strsep(&p,delimiters)))
			printf("Token: %s\n",token);

		return(0);
	}
*/
char *strsep(char **str,const char *delim)
{
	ASSERTEXPR(str && delim);

    char *begin = *str;
    char *end;

	/* ha raggiunto la fine edlla stringa */
    if(!begin)
        return(NULL);

    /* trova il primo delimitatore o la fine della stringa */
    end = begin + strcspn(begin,delim);

    if(*end) {			/* ha trovato un delimitatore */
        *end = '\0';	/* termina il token con NULL */
        *str = end + 1;	/* imposta *str per la prossima chiamata, saltando il delimitatore */
    } else {			/* se non ci sono piu' delimitatori, e' l'ultimo token */
        *str = NULL;	/* non ci sono piu' token */
    }

    return(begin);		/* restituisce l'inizio del token corrente */
}

/*
	strchkc()

	Controlla se la stringa <str> contiene solo i caratteri inclusi nella sottostringa <chrs>.
	Restituisce 1 se la stringa contiene solo i caratteri della sottostringa, 0 se contiene
	caratteri non inclusi nella sottostringa.
*/
int strchkc(const char* str,const char* chrs)
{
	int len_str = (int)strlen(str);

	for(int i=0; i < len_str; i++)
		if(!strchr(chrs,str[i]))
			return(FALSE);

	return(TRUE);
}

/*
	strchks()

	Controlla se la stringa <str> contiene una delle stringhe incluse nell'array <ptrarray>.
	Restituisce 1 se la stringa contiene una della stringhe dell'array o 0 se contiene valori
	non inclusi nell'array.
*/
int strchks(const char* str,const char** ptrarray,size_t arraysize)
{
	int len_str = (int)strlen(str);
	int found = 0;
	for(int i=0; i < (int)arraysize; i++)
		if(strcmp(str,ptrarray[i])==0)
		{
			found = 1;
			break;
		}

	return(found);
}

/*
	strichr()

	Cerca la prima occorrenza del carattere nella stringa, ignorando la differenza tra maiusc./minusc.

	Restituisce il puntatore al carattere nella stringa o NULL se non trovato.
*/
char* strichr(const char* str,int c)
{
	ASSERTEXPR(str);

	const char ch = (char)toupper(c);

	for(; toupper(*str)!=ch; ++str)
		if(*str=='\0')
			return(NULL);
	
	return((char*)str);
}

/*
	stristr()
	
	Cerca la prima occorrenza della sottostringa nella stringa, ignorando la differenza tra maiusc./minusc.

	Restituisce il puntatore alla sottostringa nella stringa o NULL se non trovata.
*/
char* stristr(const char* str,const char* substring)
{
	ASSERTEXPR(str && substring);

	for(; (str = strichr(str,*substring))!=NULL; ++str)
	{
		const char *str1,*substring1;

		for(str1 = str,substring1 = substring; ; )
			if(*++substring1=='\0')
				return((char*)str);
			else if(toupper(*++str1)!=toupper(*substring1))
				break;
	}

	return(NULL);
}

/*
	strrstr()
	
	Cerca cerca la prima occorrenza di una sottostringa in una stringa a partire da 
	destra ed avanzando verso sinistra.

	Restituisce il puntatore alla sottostringa nella stringa o NULL se non trovata.
 */
const char* strrstr(const char* str,const char* substring)
{
	ASSERTEXPR(str && substring);

	if(!str)
		return(NULL);
	if(!substring)
		return(NULL);

    const char* last_occurrence = NULL;
    const char* current_search_pos = NULL;

	size_t substring_len = strlen(substring);
    size_t str_len = strlen(str);

    /* gestione substring vuota ("")
    secondo lo standard C (es. strstr), se substring e' vuota, l'occorrenza si considera 
	all'inizio della stringa, quindi in questo caso sara' al contrario, ossia alla fine */
    if(substring_len==0)
        return(str + str_len); /* punta al terminatore NULL di str */

    /* caso limite: substring e' più lunga di str, non la considera quindi una sottostringa */
    if(substring_len > str_len)
        return(NULL);

    /* iterazione per trovare l'ultima occorrenza, inizia la ricerca dalla stringa completa */
    current_search_pos = str;

    /* continua a cercare fino a che strstr trova occorrenze:  aggiorna last_occurrence 
	per ogni iterazione, avanzando di 1 il punto di partenza per la ricerca successiva */
    while((current_search_pos = strstr(current_search_pos, substring))!=NULL)
	{
        last_occurrence = current_search_pos;
        current_search_pos++; 
    }

    return(last_occurrence);
}
/*
// VERSIONE OTTIMIZZATA:
{
    // verifiche iniziali
    if(!str || !substring)
		return(NULL);

    size_t substring_len = strlen(substring);
    size_t str_len = strlen(str);

    if(substring_len==0)
        return(str + str_len);

    if(substring_len > str_len)
        return(NULL);

    // inizia la ricerca dalla posizione piu' a destra possibile
    const char* p = str + str_len - substring_len;

    while(p >= str)
	{
        if(memcmp(p,substring,substring_len)==0)
            return p;
        p--;
    }

    return(NULL);
}
*/

/*
	strextac()
 
	Cerca una estensione come sottostringa in modo esatto.
	Es. cerca estensione di nome file esatta (.jpe), non piu' lunga (.jpeg).
	E' pensata ad hoc per le estensioni di files.

	Restituisce il puntatore alla substringa nella stringa o NULL se non trovata.
 */
char* strextac(const char* str,const char* exact)
{
	ASSERTEXPR(str && exact);

    size_t ext_len = strlen(exact);
    char* found = (char*)str;

    /* cerca l'estensione nella stringa */
    while((found = strstr(found,exact))!=NULL)
	{
        /* verifica il carattere immediatamente dopo la corrispondenza
        l'indice e' ext_len (la lunghezza dell'estensione) */
        char char_after = *(found + ext_len);
        
        /* se il carattere successivo non e' alfanumerico, significa che 
		e' la fine dell'estensione */
        if(!isalnum(char_after))
            return(found);
        
        /* se il carattere successivo e' alfanumerico, non e' una corrispondenza 
		esatta, sposta quindi il puntatore e continua la ricerca */
        found++;
    }

	return(NULL);
}

/*
	strsetn()

	Imposta il contenuto della stringa con il carattere specificato.
	Passare la dimensione totale (reale) della stringa, imposta l'ultimo carattere a '\0'.

	Restituisce il puntatore alla stringa.
*/
char* strsetn(char* str,const char c,size_t size)
{
	ASSERTEXPR(str);

	memset(str,c,size);
	str[size-1] = '\0';
	return(str);
}

/*
	strcpyn()

	Copia una stringa sull'altra (src su dst), controllando la dimensione della stringa di destinazione.
	Passare la dimensione totale (reale) della stringa di destino, imposta l'ultimo carattere a '\0'.

	Restituisce il puntatore alla stringa di destinazione.

	Per evitare sforamenti, anche se puo' sembrare ridondante, dichiarare il buffer con la dimensione 
	desiderata (_MAX_PATH) + 1, ed inizializzarlo a 0, come in:
		char buffer[_MAX_PATH+1] = {0};
	la chiamata a:
		strcpyn(buffer,data,sizeof(buffer));
	con 'data' piu' grande di buffer, assicura che buffer conterra' effettivamente e solamente _MAX_PATH 
	caratteri e che il +1 venga usato per terminare la stringa (senza dover sottrarlo a _MAX_PATH).
*/
char* strcpyn(char* dst,const char* src,size_t size)
{
    ASSERTEXPR(dst);
    ASSERTEXPR(src);
    ASSERTEXPR(size > 0);

	/* scommentare il seguente blocco per scovare l'uso non ortodosso della funzione, ossia quando si usa per copiare troncando a <n> caratteri */
/*	#ifdef _DEBUG
	{
	  size_t src_len = strlen(src);
	  char buffer[512] = {0};
	  char s[129] = {0};
	  memcpy(s,src,src_len > 129 ? 128 : src_len);
	  snprintf(buffer,sizeof(buffer),"%d is too small for %d\n\nfirst 128 bytes of data to be copyed: %s\n",size,src_len,s);
	  ASSERTEXPRMSG(src_len < size,buffer);
	}
	#endif
*/
	if(size <= 0)
		return(dst);
	if(!src || !*src)
	{
		memset(dst,0,size);
		return(dst);
	}

	size_t src_len = strlen(src);
	size_t copy_len = src_len < (size - 1) ? src_len : (size - 1);
    
    memcpy(dst,src,copy_len);
    dst[copy_len] = '\0';
    
    return(dst);
}

/*
	strcpync()

	Copia una stringa sull'altra (src su dst) fino ad incontrare il carattere specificato come
	delimitatore e controllando la dimensione della stringa di destinazione.
	Passare la dimensione totale (reale) della stringa di destino, imposta l'ultimo carattere a '\0'.

	Restituisce il puntatore alla stringa di destinazione.
*/
char* strcpync(char* dst,const char* src,size_t size,char delimiter)
{
    ASSERTEXPR(dst);
    ASSERTEXPR(src);
    ASSERTEXPR(size > 0);

    size_t i = 0;
    while(*src != '\0' && *src != delimiter && i < (size - 1))
	{
        *dst = *src;
        dst++;
        src++;
        i++;
    }
    
	*dst = '\0';

    return(dst);
}

/*
	strcpyleft

	Memorizza una stringa (in un campo piu' piccolo) a partire da sinistra, ossia dall'inizio.

	Es.
	char src[128] = "Luca Piergentili";
	char dst[15] = {0};
	strcpyright(dst,src,sizeof(dst));
	printf("strcpyright() %s\n",dst); // "ca Piergentili"
	strcpyleft(dst,src,sizeof(dst));
	printf("strcpyleft() %s\n",dst);  // "Luca Piergenti"
	
	Restituisce il puntatore al buffer di destinazione.
*/
char* strcpyleft(char* dst,const char* src,size_t size)
{
    ASSERTEXPR(dst);
    ASSERTEXPR(src);
    ASSERTEXPR(size > 0);

    if(!dst || !src || size==0)
        return(NULL);

    size_t len_to_copy = strlen(src);

    /* se la stringa sorgente e' più lunga del buffer, la tronca */
    if(len_to_copy >= size)
        len_to_copy = size - 1; /* lascia spazio per il terminatore nullo */

    /* copia assicurando la terminazione della stringa */
    strncpy(dst,src,len_to_copy);
    dst[len_to_copy] = '\0';

    return(dst);
}

/*
	strcpyright

	Memorizza una stringa (in un campo piu' piccolo) a partire da destra, ossia dalla fine.

	Es.
	char src[128] = "Luca Piergentili";
	char dst[15] = {0};
	strcpyright(dst,src,sizeof(dst));
	printf("strcpyright() %s\n",dst); // "ca Piergentili"
	strcpyleft(dst,src,sizeof(dst));
	printf("strcpyleft() %s\n",dst);  // "Luca Piergenti"
	
	Restituisce il puntatore al buffer di destinazione.
*/
char* strcpyright(char* dst,const char* src,size_t size)
{
    ASSERTEXPR(dst);
    ASSERTEXPR(src);
    ASSERTEXPR(size > 0);

    if(!dst || !src || size==0)
        return(NULL);
    
    size_t src_len = strlen(src);

    /* se la stringa sorgente e' più corta o uguale al buffer, la copia intera */
    if(src_len < size)
        return(strcpy(dst,src));
    
    /* calcola il punto di partenza nella stringa sorgente */
    size_t start_pos = src_len - (size - 1);

    /* copia i caratteri a partire dalla fine della stringa sorgente */
    strncpy(dst, src + start_pos, size - 1);
    
    /* assicura la terminazione della stringa */
    dst[size - 1] = '\0';

    return(dst);
}

/*
	strcatn()

	Concatena una stringa all'altra (src a dst), controllando la dimensione della stringa di destinazione.
	Passare la dimensione totale (reale) della stringa di destino, imposta l'ultimo carattere a '\0'.

	Restituisce il puntatore alla stringa di destinazione.
*/
char* strcatn(char* dst,const char *src,size_t size)
{
	ASSERTEXPR(dst);
	ASSERTEXPR(src);
	ASSERTEXPR(size > 0);
	ASSERTEXPR(strlen(dst) + strlen(src) < size);

	size_t dst_len = strlen(dst);
    size_t src_len = strlen(src);

	/* calculate how many characters we can actually copy */
	size_t remaining_space = size - dst_len - 1;
	size_t copy_len = (src_len < remaining_space) ? src_len : remaining_space;

	/* use memcpy for efficiency and copy the source string */
	memcpy(dst + dst_len,src,copy_len);

	/* ensure the resulting string is always null-terminated */
	dst[dst_len + copy_len] = '\0';
    
	return(dst);
}

/*
	strcmpni()

	Confronta due stringhe per <n> caratteri, ignorando maiuscole/minuscole.

	Restituisce:
	0 se trova un match
	>0 (maggiore di zero) se s1 e' "maggiore" di s2
	<0 (minore di zero) se s1 e' "minore" di s2
*/
int strcmpni(const char* s1,const char* s2,size_t n)
{
	ASSERTEXPR(s1);
	ASSERTEXPR(s2);
	ASSERTEXPR(n > 0);

	while(n-- && *s1 && *s2)
	{
        if(tolower((unsigned char)*s1)!=tolower((unsigned char)*s2))
            return(tolower((unsigned char)*s1) - tolower((unsigned char)*s2));

        s1++;
        s2++;
    }

    if(n==(size_t)-1)
		return(0);

    return(tolower((unsigned char)*s1) - tolower((unsigned char)*s2));
}

/*
	strchrn()

	Cerca un carattere in una stringa di dimensione specificata.
	Non si basa su un terminatore nullo, ma sulla dimensione specificata.

	Restituisce un puntatore alla prima occorrenza trovata o NULL.
 */
const char* strchrn(const char* str,int c,size_t n)
{
	ASSERTEXPR(str);
	ASSERTEXPR(n > 0);
    
    for(size_t i = 0; i < n; ++i)
	{
		if(str[i]==(char)c)
            return(&str[i]);
	}
    
	return(NULL);
}

/*
	strstrn()

	Cerca una sottostringa in una stringa di dimensione specificata.
	Non si basa su un terminatore nullo, ma sulla dimensione specificata.

	Restituisce un puntatore alla prima occorrenza trovata o NULL.
 */
const char* strstrn(const char* string,const char* substring,size_t n)
{
	ASSERTEXPR(string);
	ASSERTEXPR(substring);
	ASSERTEXPR(n > 0);

    size_t substring_len = strlen(substring);

    if(substring_len==0)
        return(string);

    if(n < substring_len)
        return(NULL);

    for(size_t i = 0; i <= n - substring_len; ++i)
	{
		if(string[i]==substring[0])
		{
			if(strncmp(&string[i],substring,substring_len)==0)
                return(&string[i]);
        }
    }

    return(NULL);
}

/*
	strchgc()

	Cambia, nella stringa, un carattere con un altro.
	str	-> stringa da modificare
	c	-> carattere da cambiare
	chr	-> carattere per sostituzione
	Restituisce il numero di cambi effettuati.
*/
int strchgc(char* str,char c,char chr)
{
	if(!str)
		return(0);

	int changed = 0;

	while(*str)
	{
		if(*str==c)
		{
			*str = chr;
			changed++;
		}
		str++;
	}

	return(changed);
}

/*
	strchgs()

	Cambia, nella stringa, tutti i caratteri della sottostringa, se trovati, con il carattere specificato.
	str	-> stringa da modificare
	s	-> sottostringa contenente i caratteri da cambiare
	chr	-> carattere per sostituzione
	Restituisce il numero di cambi effettuati.
*/
int strchgs(char* str,const char* s,char chr)
{
	if(!str || !s)
		return(0);

	int changed = 0;

	while(*str)
	{
		const char* p = s;
		while(*p)
		{
			if(*str==*p)
			{
				*str = chr;
				changed++;
			}
			p++;
		}
		str++;
	}

	return(changed);
}

/*
	subst()

	Sostituisce tutte le occorrenze di una sottostringa all'interno di una stringa con quanto
	specificato.
	
	Alloca dinamicamente memoria per la nuova stringa risultante, il chiamante dovra' liberare
	questa memoria con free().
 
	Se il valore con cui sostuituire e' una stringa vuota (""), allora le occorrenze della 
	sottostringa nella stringa vengono rimosse.

	Restituisce il puntatore alla nuova stringa allocata con il risultato delle sostituzioni 
	o NULL per nessuna occorrenza trovata o errore.
 */
char* subst(const char* str,const char* substring,const char* replace)
{
	ASSERTEXPR(str);
	ASSERTEXPR(substring);
	ASSERTEXPR(replace);

    char* result = NULL;
    const char* current_str_pos = NULL;
    const char* found_pos = NULL;
    size_t str_len = 0;
    size_t substring_len = 0;
    size_t replace_len = 0;
    int count = 0;
    size_t new_length = 0;
    char* current_result_pos = NULL;

    str_len = strlen(str);
    substring_len = strlen(substring);
    replace_len = strlen(replace);

    /* substring vuota */
    if(substring_len==0)
        return(NULL);
 
    /* se quello da cercare e' uguale a quello con cui sostituire, in teoria andrebbe bene cosi',
	ma restituisce NULL perche' altrimenti il chiamante chiamerebbe la free su un oggetto che non
	e' stato alocato */
	if(strcmp(substring,replace)==0)
        return(NULL);

    /* conta le occerrenze */
	current_str_pos = str;
    new_length = str_len;

    while((found_pos = strstr(current_str_pos,substring))!=NULL)
	{
        count++;

		/* calcola preventivamente la lunghezza della stringa risultante per sapere quanto allocare 
		occhio che la stringa risultante puo' essere piu' corta se replace e' piu' corto */
        new_length += (replace_len - substring_len);
        
		/* avanza il puntatore per cercare l'occorrenza successiva */
        current_str_pos = found_pos + substring_len;
    }

    /* nessuna occorrenza */
    if(count==0)
        return(NULL);
 
    /* crea dinamicamente la stringa risultante */
    if((result = (char*)calloc(new_length + 1,sizeof(char)))==NULL)
		return(NULL);

    /* sostituzioni */
    current_str_pos = str;			/* ptr a str originale */
    current_result_pos = result;	/* ptr a dove scrivere nel risultante */

    while((found_pos = strstr(current_str_pos,substring))!=NULL)
	{
        /* copia la parte della stringa prima dell'occorrenza di substring */
        size_t chars_to_copy = found_pos - current_str_pos;
        memcpy(current_result_pos,current_str_pos,chars_to_copy);
        current_result_pos += chars_to_copy;

        /* copia il replace (se non vuoto) */
        if(replace_len > 0)
		{
            memcpy(current_result_pos,replace,replace_len);
            current_result_pos += replace_len;
        }
		else
		{
			/* replace_len e' 0, non copia nulla, ottenendo l'eliminazione */
		}

        /* passa alla prossima substring */
        current_str_pos = found_pos + substring_len;
    }

    /* copia la parte rimanente della stringa (dopo l'ultima sostituzione) */
	/* strcpy(current_result_pos,current_str_pos); */
	/* invece di strcpy... */
	size_t remaining_len = str_len - (current_str_pos - str);
	memcpy(current_result_pos, current_str_pos, remaining_len);
	current_result_pos[remaining_len] = '\0';

    return(result);
}

/*
	substr()

	Sostituisce nel buffer SOLO la prima occorrenza della sottostringa nella stringa con quanto specificato.
	Passare la dimensione totale (reale) del buffer.

	Restituisce 0 se non effettua nessuna sostituzione, 1 in caso contrario.
	
	In input:	stringa originale (dove cercare)
				stringa da cercare e sostituire
				stringa con cui sostituire
				buffer di output
				dim. del buffer di output
*/
#if 0
int substr(const char* str,const char* substring,const char* replace,char* buffer,size_t size)
{
	ASSERTEXPR(str);
	ASSERTEXPR(substring);
	ASSERTEXPR(replace);
	ASSERTEXPR(buffer);
	ASSERTEXPR(size > 0);

	char* p;
	strcpyn(buffer,str,size);

	if((p = strstr(buffer,substring))!=NULL)
	{
		int i = (int)(size - (p-buffer));
		strcpyn(p,replace,i);
		p = (char*)strstr(str,substring) + strlen(substring);
		i = (int)strlen(buffer);
		strcpyn(buffer+i,p,size-i);
		return(1);
	}
	else
		return(0);
}
#else
/* se replace = "", elimina substring */
int substr(const char* str, const char* substring, const char* replace, char* buffer, size_t size)
{
    ASSERTEXPR(str);
    ASSERTEXPR(substring);
    ASSERTEXPR(replace);
    ASSERTEXPR(buffer);
    ASSERTEXPR(size > 0);

    const char* p;
    size_t subLen = strlen(substring);
    size_t replaceLen = strlen(replace);

    /* cerca la sottostringa nell'originale */
    p = strstr(str, substring);

    if (p != NULL)
    {
        /* calcola quanto spazio occupa la parte iniziale (prima della sottostringa) */
        size_t prefixLen = (size_t)(p - str);

        /* se il buffer non puo' contenere nemmeno il prefisso, esce */
        if (prefixLen >= size) prefixLen = size - 1;

        /* copia la parte iniziale nel buffer */
        strncpy(buffer, str, prefixLen);
        buffer[prefixLen] = '\0';

        /* aggiunge la stringa di rimpiazzo (se "" non aggiunge nulla, elimina) */
        if(replaceLen > 0)
            strncat(buffer,replace,size - strlen(buffer) - 1);

        /* accoda il resto della stringa originale dopo la sottostringa */
        const char* restOfStr = p + subLen;
        strncat(buffer,restOfStr,size - strlen(buffer) - 1);

        return(1);
    }

    /* se non trova nulla, copia la stringa originale cosi' com'e' */
    strcpyn(buffer, str, size);

    return(0);
}
#endif

/*
	substrn()

	I couldn't replace text on a string of unknown size so I had the function make a copy instead 
	and that 0 and NULL is used to signal unused parameters:

	substr("some string", 5, 0, NULL)
	returns "string"

	substr("some string", -5, 3, NULL)
	returns "str"

	substr("some string", 4, 0, "thing")
	returns "something"

	free() needs to be called on the string returned after it's use.

	C sample code by OnionKnight
*/
char* substrn(const char* string,size_t pos,size_t len,const char* replace)
{
	ASSERTEXPR(string);

    char* substring;
    size_t i;
    int   length;
 
    if (string == NULL)
        return NULL;
    length = (int)strlen(string);
    if (pos < 0) {
        pos = length + pos;
        if (pos < 0) pos = 0;
    }
    else if (pos > (size_t)length) pos = (size_t)length;
    if (len <= 0) {
        len = length - pos + len;
        if (len < 0) len = length - pos;
    }
    if (pos + len > (size_t)length) len = (size_t)length - pos;
    if (replace != NULL) {
        if ((substring = (char*)calloc(sizeof(*substring)*(length-len+strlen(replace)+1),sizeof(char))) == NULL)
            return NULL;
        for (i = 0; i != pos; i++) substring[i] = string[i];
        pos = pos + len;
        for (len = 0; replace[len]; i++, len++) substring[i] = replace[len];
        for (; string[pos]; pos++, i++) substring[i] = string[pos];
        substring[i] = '\0';
    }
    else {
        if ((substring = (char*)calloc(sizeof(*substring)*(len+1),sizeof(char))) == NULL)
            return NULL;
        len += pos;
        for (i = 0; pos != len; i++, pos++)
            substring[i] = string[pos];
        substring[i] = '\0';
    }
 
    return substring;
}

/*
	strtokargs()

	Suddivide in tokens come con strtok(), pero' con la possibilita' di gestire tokens che includano il
	separatore stesso, sempre e quando vengano circoscritti con un "inclusore".
	Funzionamento e dinamica esattamente uguali a strtok(), includendo il modo in cui va chiamata.

	Esempi:
	<"esci fuori" disse giocando a nascondino>
	la frase puo' essere tokenizzata usando lo spazio e la citazione "esci fuori" viene considerata come
	token unico grazie all'uso del doppio apice come inclusore
	<-d"C:\Users\lpier\Documents\Luca\Pictures\2D\Artisti\Otto Schmidt\Black Cat" -r>
	la linea di comando, contenente nomi file che includono spazi, viene scomposta in due token in base allo
	spazio grazie  all'uso del doppio apice come inclusore
*/
char* strtokargs(char* str,const char separator,const char inclusor)
{
	// statica per preservare lo stato tra le chiamate successive
	static char* next_token = NULL;

	// se viene passata una nuova stringa, reinizializza il puntatore
	if(str!=NULL)
		next_token = str;

	// se arriva alla fine della stringa nelle chiamate precedenti
	if(next_token==NULL || *next_token=='\0')
		return(NULL);

	// prima fase: salta i separatori iniziali
	// all'inizio sara' sicuramente fuori dagli inclusori
	while(*next_token && *next_token==separator)
		next_token++;

	// se dopo aver saltato i separatori arriva a fine stringa, significa che non ci sono piu' token
	if(*next_token=='\0')
		return(NULL);

	// ha trovato l'inizio del token
	char* token_start = next_token;
	int inside_inclusor = 0;

	// seconda fase: scansione del token
	while(*next_token)
	{
		// se trova un inclusore, inverte lo stato
		if(*next_token==inclusor)
		{
			inside_inclusor = !inside_inclusor;
		}
		// se trova un separatore e sta fuori dagli inclusori, il token e' finito
		else if(*next_token==separator && !inside_inclusor)
		{
			*next_token = '\0'; // spezza la stringa in-place
			next_token++;       // posiziona il puntatore per la prossima chiamata
			return(token_start);
		}

		next_token++;
	}

	// se arriva qui, significa che e' arrivato a fine stringa ('\0')
	// gestisce automaticamente anche il caso dell'inclusore non bilanciato,
	// restituendo tutto quello che e' rimasto, fino alla fine
	return(token_start);
}

/*
	strstrpc()

	Rimuove tutte le occorrenze del carattere nella stringa usando la tecnica dello scorrimento a due puntatori.
	(string strip char) 
*/
void strstrpc(char *str,char chr)
{
	if(!str)
		return;

	char *src = str; /* ptr x lettura */
	char *dst = str; /* ptr x scrittura */

	while(*src!='\0')
	{
		/* se il carattere corrente NON e' quello da eliminare */
		if(*src!=chr)
		{
			/* se src e dst sono diversi, sposta il carattere (il controllo src!=dst e' un'ottimizzazione opzionale) */
			*dst = *src;
			dst++;
		}
		src++;
	}

	/* imposta il nuovo terminatore nullo alla fine */
	*dst = '\0';
}

/*
	strstripchars()

	Rimuove tutte le occorrenze dei caratteri (della seconda stringa) nella stringa (di input) usando la tecnica dello 
	scorrimento a due puntatori.
	Pero', invece di fare n cicli (uno per ogni carattere da eliminare), che renderebbe l'algoritmo inefficiente (O(NxM)),
	usa un singolo ciclo sulla stringa principale e, per ogni carattere, controlla se e' presente nella "lista nera" usando
	strchr().
*/
void strstripchars(char *str,const char *chars)
{
	if(!str || !chars)
		return;

	char *src = str; /* ptr x lettura */
	char *dst = str; /* ptr x di scrittura */

	while(*src!='\0')
	{
		/* strchr() cerca il carattere corrente (*src) nella lista nera, se restituisce NULL, significa che il carattere e'
		di quelli "buoni" e va tenuto.
		*/
		if(strchr(chars,*src)==NULL)
		{
			*dst = *src;
			dst++;
		}
		src++;
	}

	/* termina la stringa nel nuovo punto di interruzione */
	*dst = '\0';
}

/*
	strshuffle()

	Randomizza i caratteri della stringa.

	Note:
	Non passare una stringa letterale (char *p = "abc";) perche' il compilatore la considera 
	read-only, usare invece un buffer (char buf[] = "abc";).
	Se la randomizzazione deve avere livello crittografico, usare rand_s() invece di rand(),
	oppure BCryptGenRandom() su Windows.
*/
char* strshuffle(char *string)
{
	ASSERTEXPR(string);

    size_t n = strlen(string);
    for(size_t i = n - 1; i > 0; --i)
	{
        size_t j = fast_rand_range((unsigned int)i + 1);
        char tmp = string[i];
        string[i] = string[j];
        string[j] = tmp;
    }

	return(string);
}

/*
	strwords()

	Genera parole (stringhe) alfabetiche AC/DC.
*/
char* strwords(char *buffer,size_t size)
{
	ASSERTEXPR(buffer);
	ASSERTEXPR(size > 0);

	#define ALPHABET  "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"
    
	static const char *alpha = ALPHABET;
    int len;

    if(size <= 0)
		return(buffer);

    len = (int)(sizeof(ALPHABET)-1);

	for(int i=0; i < (int)size - 1; ++i)
		buffer[i] = alpha[fast_rand_range(len)];

    buffer[size-1] = '\0';

    return(buffer);
}

/*
	strinbtw()

	Cerca la sottostringa <substring> all'interno della stringa <string>, verificando
	che sia compresa esattamente entro la substringa di inizio <from> e la substringa 
	di fine <to>.

	Esempio:
	per verificare che il carattere '@' venga usato per delimitare la coppia utente
	password all'interno di una url http, come in:
	http://user:password@www.domain.com/resource/data.pdf
	e non sia un separatore di parametri come in:
	https://appinformatica.vtexassets.com/_v/public/assets/v1/bundle/css/asset.min.css?v=3&files=theme,ticnova.appinformatica-store@0.6.77$style.common,ticnova.appinformatica-store@[...]
	il carattere "@" deve essere compreso tra "://" ed il primo "/" dopo di esso:
	strinbtw("http://user:password@www.domain.com/resource/data.pdf","@","://","/") -> true
*/
bool strinbtw(const char* string,const char* substring,const char* from,const char* to)
{
	ASSERTEXPR(string);
	ASSERTEXPR(substring);
	ASSERTEXPR(from);
	ASSERTEXPR(to);

	bool between = false;
	const char* begin = strstr(string,from);
	if(begin)
	{
		const char* end = strstr(begin+strlen(from),to);
		if(end)
		{
			const char* found = strstr(string,substring);
			if(found)
			{
				if(found > begin && found < end)
				{
					between = true;
				}
			}
		}
	}
	return(between);
}

/*
	strrcspn()

	Elimina i caratteri specificati dalla lista di esclusione dal finale 
	della stringa, sempre e solo a partire da destra (ossia dalla fine).
*/
void strrcspn(char* str,const char* chars_to_remove)
{
	ASSERTEXPR(str);
	ASSERTEXPR(chars_to_remove);

    size_t len = strlen(str);

	ASSERTEXPR(len > 0);

    /* parte dalla fine della stringa e si muove all'indietro */
    while(len > 0 && strchr(chars_to_remove, str[len - 1]) != NULL)
        len--;

    /* pone il terminatore nullo nella nuova posizione */
    str[len] = '\0';
}

/*
	strempty()

	Verifica se la stringa e' vuota (blank o '\0').

	Restituisce 1 se la stringa contiene solo spazi o '\0', 0 altrimenti.
*/
int strempty(const char* str)
{
    ASSERTEXPR(str);

    const char* p = str;
    while(*p)
    {
        if(!isspace((unsigned char)*p))
            return(0);
        p++;
    }
    
    /* stringa vuota, sono stati trovati solo spazi o la stringa iniziava con '\0' */
    return(1);
}

/*
	strrtrim()

	Elimina gli spazi finali (alla destra) della stringa.

	Restituisce la nuova lunghezza della stringa.
*/
size_t strrtrim(char* str)
{
	ASSERTEXPR(str);

	register size_t i = strlen(str)-1;
	
	while(i >= 0)
	{
		if(isspace((unsigned char)str[i]))
			str[i] = '\0';
		else
			break;
		i--;
	}
	
	return(i);
}

/*
	strltrim()

	Elimina gli spazi iniziali (alla sinistra) della stringa.

	Restituisce la nuova lunghezza della stringa.
*/
size_t strltrim(char* str)
{
	ASSERTEXPR(str);

	register size_t i = 0;
	register size_t n = strlen(str);
	
	while(i < n)
	{
		if(!isspace((unsigned char)str[i]))
			break;
		i++;
	}

	if(i < n)
	{
		memmove(str,str+i,n-i);
		str[n-i] = '\0';
	}
	else
		i = 0;

	return(n-i);
}

/*
	stralltrim()
	
	Elimina TUTTI gli spazi dalla stringa, (inclusi gli interni).

	Restituisce la nuova lunghezza della stringa.

	Il modo piu' efficiente e' l'uso della tecnica dei due puntatori. L'idea e' di scorrere la stringa 
	una sola volta, usando un puntatore per leggere (read_ptr) ed un altro per scrivere (write_ptr).
	Il puntatore di lettura scorre tutta la stringa dall'inizio alla fine mentre il puntatore di scrittura 
	rimane fermo all'inizio.
	Quando il puntatore di lettura trova un carattere che non e' uno spazio, lo copia nella posizione del 
	puntatore di scrittura e poi incrementa entrambi i puntatori.
	Quando il puntatore di lettura trova uno spazio, viene semplicemente ignorato e solo il puntatore di 
	lettura avanza.
	Questo approccio esegue la sua operazione in una singola passata, rendendolo estremamente veloce ed 
	efficiente, con una complessita' di tempo di O(N).
*/
size_t stralltrim(char* str)
{
	ASSERTEXPR(str);

	char* read_ptr = str;
	char* write_ptr = str;

	while(*read_ptr)
	{
		if(!isspace((unsigned char)*read_ptr))
		{
			*write_ptr = *read_ptr;
			write_ptr++;
		}
		read_ptr++;
	}

	*write_ptr = '\0';

	return((size_t)(write_ptr - str));
}
#if 0
/*
	l'implementazione che segue e' inefficiente dato che per ogni spazio che trova, il codice esegue 
	un intero ciclo per spostare tutti i caratteri successivi di una posizione, il che si traduce in 
	una prestazione scarsa su stringhe lunghe o con molti spazi
*/
size_t stralltrim(char* str)
{
	ASSERTEXPR(str);

	char* p = str;

	while(*p)
	{
		if(isspace((unsigned char)*p))
		{
			char* P = p;
			while(*(P+1))
			{
				*P = *(P+1);
				P++;
			}
			*P = '\0';
			p--;
		}

		p++;
	}

	return(strlen(str));
}
#endif

/*
	stroutrim()

	Elimina gli spazi iniziali e finali della stringa (non tocca gli interni).

	Restituisce il puntatore alla stringa. //la nuova lunghezza della stringa.
*/
char* /*size_t*/ stroutrim(char* str)
{
	ASSERTEXPR(str);
	if(!str)
		return(NULL);

	char* read_ptr = str;
	char* write_ptr = str;
	char* last_non_space = NULL;
	int leading = 1;

	while(*read_ptr)
	{
		if(!isspace((unsigned char)*read_ptr))
		{
			leading = 0;					/* superati gli spazi iniziali */
			*write_ptr = *read_ptr;
			last_non_space = write_ptr;		/* sovrascrive costantemente l'ultimo carattere valido */
			write_ptr++;
		}
		else if(!leading)
		{
			/* e' uno spazio, ma NON e' iniziale, lo copia temporaneamente, se si rivela uno spazio finale, lo elimina alla fine */
			*write_ptr = *read_ptr;
			write_ptr++;
		}
		read_ptr++;
	}

	/* terminazione e pulizia finale */
	if(last_non_space)
	{
		*(last_non_space + 1) = '\0';
		write_ptr = last_non_space + 1;
	}
	else
	{
		/* la stringa era vuota o solo spazi */
		*str = '\0';
		write_ptr = str;
	}

	/* return((size_t)(write_ptr - str)); */
	return(str);
}

/*
	strstrim()
	
	Riduce in tutta la stringa due o piu' spazi ad uno solo.

	Restituisce la nuova lunghezza della stringa.
*/
size_t strstrim(char* str)
#if 1
{
	ASSERTEXPR(str);

	/* tecnica due puntatori */
	char* read_ptr = str;
	char* write_ptr = str;

	/* salta gli spazi iniziali */
	while (isspace((unsigned char)*read_ptr))
		read_ptr++;

	while(*read_ptr)
	{
		*write_ptr = *read_ptr;
		write_ptr++;

		/* se il carattere corrente e uno spazio, salta tutti gli spazi successivi */
		if(isspace((unsigned char)*read_ptr))
		{
			while(isspace((unsigned char)*read_ptr))
				read_ptr++;
		}
		else
		{
			read_ptr++;
		}
	}

	/* se la stringa finisce con uno spazio */
	if(write_ptr > str && isspace((unsigned char)write_ptr[-1]))
		write_ptr--;

	*write_ptr = '\0';
	
	return((size_t)(write_ptr - str));
}
#else
{
	ASSERTEXPR(str);

	char* p = str;

	while(*p)
	{
		if(isspace((unsigned char)*p))
			if(*(p+1))
				if(isspace((unsigned char)*(p+1)))
				{
					char* P = p;
					while(*(P+1))
					{
						*P = *(P+1);
						P++;
					}
					*P = '\0';
					p = str;
				}

		p++;
	}

	return(strlen(str));
}
#endif

/*
	strrot()

	Ruota di <n> caratteri la stringa verso destra o sinistra restituendo il puntatore alla stringa modificata.
	Non e' il metodo piu' efficente, ma la ricorsione e' sicuramente il metodo piu' elegante.

	Restituisce il puntatore alla stringa.
	
	In input:	la stringa da ruotare
				verso rotazione (-1 a sinistra, 1 a destra, 0 nessuna rotazione)
				numero di caratteri da ruotare

	Note:		char* str = {"questa e' una stringa"};	->	crea una stringa letterale, ossia un valore costante che 
															viene memorizzato nella memoria a sola lettura, per cui 
															passare str a memmove() genera una eccezione
				char str[] = {"questa e' una stringa"};	->	crea una stringa sullo stack locale, quindi su memoria 
															modificabile
*/
char* strrot(char* str,int n,int t)
{
	ASSERTEXPR(str);
	ASSERTEXPR(t > 0);

	char c = '\0';
	size_t len = strlen(str);

	/* void *memmove(void *dest, const void *src, size_t count); */
	switch(n)
	{
		case -1: /* scorre a sinistra <- */
		{
			c = str[0];
			memmove(str,str+1,len-1);
			str[len-1] = c;
		}
		break;

		case 1: /* scorre a destra -> */
		{
			c = str[len-1];
			memmove(str+1,str,len-1);
			str[0] = c;
		}
		break;

		case 0: /* non scorre */
			return(str);

		default: /* non previsto */
			return(NULL);
	}

	/* ripete per le <t> volte (ossia x il numero di rotazioni) */
	return(t > 1 ? strrot(str,n,--t) : str);
}

/*
	strcount()

	Conta e restituisce le occorrenze della substringa nella stringa.
	
	Restituisce il numero di occorrenze o -1 in caso di errore.
*/
int strcount(const char *str,const char *substr)
{
	ASSERTEXPR(str);
	ASSERTEXPR(substr);

    int count = 0;
    const char *temp = str;

    while((temp = strstr(temp,substr))!=NULL)
	{
        count++;
        temp++; /* incrementa il puntatore per evitare l'overlap */
    }

    return(count);
}

/*
	strlrw()

	Converte la stringa in minuscolo e ne restituisce il puntatore.
*/
char* strlrw(char* str)
{
	ASSERTEXPR(str);

    for(register int i=0; str[i]; i++)
		str[i] = (char)tolower((unsigned char)str[i]);

	return(str);
}

/*
	strurp()

	Converte la stringa in maiuscolo e ne restituisce il puntatore.
*/
char* strurp(char* str)
{
	ASSERTEXPR(str);

    for(register int i=0; str[i]; i++)
		str[i] = (char)toupper((unsigned char)str[i]);

	return(str);
}

/*
	strright(), striright(), strleft(), strileft()

	Controllano se la stringa specificata (substr) appare alla destra/sinistra della
	stringa di input (str), distinguendo o meno tra maiuscole e minuscole.

	Restituiscono -1 in caso di errore (stringa vuota o str piu' corta di substr).
*/
int _strright(const char* str,const char* substr,int ignorecase)
{
	ASSERTEXPR(str);
	ASSERTEXPR(substr);

	size_t len = strlen(substr);
	size_t at = strlen(str) - len;
	return(at >= 0 ? (ignorecase ? strnicmp(str+at,substr,len) : strncmp(str+at,substr,len)) : -1);
}

int _strleft(const char* str,const char* substr,int ignorecase)
{
	ASSERTEXPR(str);
	ASSERTEXPR(substr);

	size_t len = strlen(substr);
	return(len > 0 ? (ignorecase ? strnicmp(str,substr,len) : strncmp(str,substr,len)) : -1);
}

int strright(const char* str,const char* substr)
{
	ASSERTEXPR(str);
	ASSERTEXPR(substr);

	return(_strright(str,substr,0));
}

int striright(const char* str,const char* substr)
{
	ASSERTEXPR(str);
	ASSERTEXPR(substr);

	return(_strright(str,substr,1));
}

int strleft(const char* str,const char* substr)
{
	ASSERTEXPR(str);
	ASSERTEXPR(substr);

	return(_strleft(str,substr,0));
}

int strileft(const char* str,const char* substr)
{
	ASSERTEXPR(str);
	ASSERTEXPR(substr);

	return(_strleft(str,substr,1));
}

/*
	strkmgt()

	Converte da stringa (solo numerica o con specificatore KB, MB, GB, TB) a numero.
*/
long long strkmgt(const char* size)
{
	char number[32] = {0};
	int n = 0;
	char id[32] = {0};
	int i = 0;

	const char* p = size;
	while(*p && n < sizeof(number)-1 && i < sizeof(id)-1)
	{
		if(isdigit(*p))
			number[n++] = *p;
		else
			id[i++] = *p;
		p++;
	}

	long long ll = atoll(number);
	stralltrim(id);

	long long ll_number = ll;
	if(stricmp(id,"KB")==0)
		ll_number = ll * 1024LL;
	else if(stricmp(id,"MB")==0)
		ll_number = (ll * 1024LL) * 1024LL;
	else if(stricmp(id,"GB")==0)
		ll_number = ((ll * 1024LL) * 1024LL) * 1024LL;
	else if(stricmp(id,"TB")==0)
		ll_number = (((ll * 1024LL) * 1024LL) * 1024LL) * 1024LL;

	return(ll_number);
}

/*
	strsizefmt()

	Converte il valore numerico in una stringa, arrotondando e formattando in KB, etc. 
	stile MS-DOS.
	
	Restituisce il puntatore alla stringa formattata.
*/
char* strsizefmt(char* str,int size,double bytes)
{
	ASSERTEXPR(str);
	ASSERTEXPR(size > 0);
	/* possono esistere files a dimensione 0 */
	/* ASSERTEXPR(bytes > 0.0f); */
	
	/* bytes */
	if(!(bytes > 0.0f))
	{
		strcpyn(str,"0 bytes",size);
	}
	else if(bytes < 1024.0)
	{
		snprintf(str,size,"%d bytes",(int)bytes);
	}
	/* KB */
	else if(bytes < 1048576.0)
	{
		snprintf(str,size,"%.2f KB",bytes / 1024.0);
	}
	/* MB */
	else if(bytes < 1073741824.0)
	{
		snprintf(str,size,"%.2f MB",bytes / 1048576.0);
	}
	/* GB */
	else
	{
		snprintf(str,size,"%.2f GB",bytes / 1073741824.0);
	}
	
	/* //$ manca TB */

	return(str);
}

/* funzione di servizio per strnumfmt(), vedi sotto */
/* formatta un intero con punti ogni 3 cifre (notazione tedesca) */
static void fmtIntDE(long long v, char *out, size_t outSize)
{
    char tmp[40];
    int  len, j = 0, digits = 0;

    len = wtfsnprintf(tmp, sizeof(tmp), "%lld", v);
    for (int i = len - 1; i >= 0; --i) {
        out[j++] = tmp[i];
        if (++digits % 3 == 0 && i) out[j++] = '.';
    }
    out[j] = '\0';
    /* inverti */
    for (int i = 0; i < j / 2; ++i) {
        char c = out[i]; out[i] = out[j - 1 - i]; out[j - 1 - i] = c;
    }
}
/*
	strnumfmt()

	Formatta con punti e virgole a seconda del valore (numerico).

 	Notazioni internazionali per la separazione di migliaia e decimali:
 
	ISO 31-0 / SI – Ufficiale internazionale
	12 345 678,901 23 -> spazio sottile (U+202F) ogni 3 cifre, virgola decimale
 
	Paesi che seguono l’ISO:
	Italia (UNI 8064), Francia, Spagna, Svezia, Russia, Brasile, India, ...

	Varianti nazionali
	Germania, Austria, Paesi Bassi
	12.345.678,901 23 -> punto ogni 3 cifre, virgola decimale

	Paesi anglosassoni
	USA, UK, Canada, Australia
	12,345,678.901 23 -> virgola ogni 3 cifre, punto decimale
 
	Ambiente informatico (XML, JSON, C, SQL)
	12345678.90123 -> nessun separatore migliaia, punto decimale

	esempio:

	#include <stdio.h>
	int main(void)
	{
		char out[64];
		TAGGEDVALUE tv;

		tv.valuetype = QWORD_TYPE;
		tv.value.qwValue = 1234567890123LL;
		printWithCommasDE(&tv, out, sizeof(out));
		puts(out); // 1.234.567.890.123

		tv.valuetype = DOUBLE_TYPE;
		tv.value.dValue = -1234567.890123;
		printWithCommasDE(&tv, out, sizeof(out));
		puts(out); // -1.234.567,890123 

		return 0;
	}
*/
char *strnumfmt(const TAGGEDVALUE *tv,char *buf,size_t bufSize)
{
	ASSERTEXPR(tv);
	ASSERTEXPR(buf);
	ASSERTEXPR(bufSize > 0);

    if(!tv || !buf || bufSize==0)
	{	
		if(buf)
			*buf = '\0';
		return(buf);
	}

    switch(tv->valuetype)
	{
		case WORD_TYPE:
			fmtIntDE(tv->value.wValue, buf, bufSize);
			break;

		case DWORD_TYPE:
			fmtIntDE(tv->value.dwValue, buf, bufSize);
			break;

		case QWORD_TYPE:
			fmtIntDE(tv->value.qwValue, buf, bufSize);
			break;

		case FLOAT_TYPE:
			/* separa parte intera e frazionaria */
			{
				double v   = tv->value.fValue;
				long long ipart = (long long)v;
				/* 3 cifre decimali, arrotondate */
				int   fpart = (int)((v < 0 ? -v : v) * 1000 + 0.5) % 1000;
				char  tmp[20];
				fmtIntDE(ipart, buf, bufSize);
				/* aggiunge virgola e decimali (sempre 3) */
				snprintf(tmp, sizeof(tmp), ",%03d", fpart);
				strncat(buf, tmp, bufSize - strlen(buf) - 1);
			}
			break;

		case DOUBLE_TYPE:
			/* identico a float ma con 6 decimali */
			{
				double v   = tv->value.dValue;
				long long ipart = (long long)v;
				int   fpart = (int)((v < 0 ? -v : v) * 1000000 + 0.5) % 1000000;
				char  tmp[20];
				fmtIntDE(ipart, buf, bufSize);
				snprintf(tmp, sizeof(tmp), ",%06d", fpart);
				strncat(buf, tmp, bufSize - strlen(buf) - 1);
			}
			break;

		default: /* tipo non gestito */
			*buf = '\0';
			break;
    }

    return(buf);
}

/*
	strdupl()

	Duplica la stringa, restituendo il puntatore alla nuova che dovra' essere eliminata dal chiamante.
*/
char* strdupl(const char* str)
{
	ASSERTEXPR(str);

	char* s;
	size_t n = strlen(str);
	s = (char*)calloc(n+1,sizeof(char));
	if(s)
		strcpy(s,str);
	
	return(s);
}

/*
	wtfsnprintf()
	
	La snprintf() di sempre, pero' versione WTF, ossia come la cultura woke ti cambia le carte in tavola da un giorno all'altro, facendoti
	credere che oggigiorno, dopo averlo preso nel didietro, bisogna essere contenti e ringraziare.

	Secondo la pagina ufficiale documentazione IBM:
	https://www.ibm.com/docs/en/i/7.5.0?topic=functions-snprintf-print-formatted-data-buffer
	riporto citazione letterale:
	"Return Value: The snprintf() function returns the number of bytes that are written in the array, not counting the ending null character."

	Tale pagina della documentazione si riferisce all'ambiente IBM OS/400, che implementava snprintf (o varianti non standard come _snprintf)
	seguendo il comportamento dello standard ANSI C89/C90 o delle prime estensioni POSIX. Prima dello standard C99, sui vecchi sistemi, e su 
	Windows con Visual Studio (fino a Visual Studio 2013), la funzione normalmente si chiamava _snprintf e restituiva effettivamente il numero
	di caratteri scritti, oppure -1 se il buffer non era abbastanza grande.

	Con lo standard C99, il comitato woke, formato da individui che ovviamente non scrivono codice, ma solo scartabellano, decise di uniformare
	il comportamento su tutte le piattaforme (Linux, Windows, macOS), decretando che snprintf doveva restituire i caratteri che "avrebbe voluto"
	scrivere (tipico di come pensano i gay). Visual Studio si adeguo' a tale standard a partire dalla versione 2015.

	Ecco quindi una versione mapigliongulo della snprintf, cosi' come e' sempre stata e sempre sara', restituiendo il numero di caratteri che
	vengono "effettivamente" scritti nel buffer.

	Versione "sensata" di snprintf.
	Scrive massimo (n-1) caratteri nel buffer, aggiunge sempre '\0', e restituisce il numero di caratteri effettivamente scritti (escluso '\0').
	Se n==0, non scrive nulla (nemmeno '\0') e restituisce 0.
	Se c'e' errore di formattazione, restituisce -1.
 */
int wtfsnprintf(char *buf,size_t n,const char *fmt,...)
{
	int ret;
	va_list ap;

	if(n==0)
		return(0); /* non c'e' piu' spazio */

	va_start(ap,fmt);
	ret = vsnprintf(buf,n,fmt,ap);
	va_end(ap);

	if(ret < 0)
		return(-1); /* errore interno di formattazione */

	/* 
	vsnprintf restituisce quanti caratteri "avrebbe voluto" scrivere, qui basicamente controlla il suo 
	valore di ritorno e lo "corregge" per restituire solo cio' che e' effettivamente entrato nel buffer
	se e' >= n, ha troncato e scritto solo (n-1) caratteri + '\0'
	se e' < n, ha scritto tutto (ret caratteri + '\0')
	*/
	if((size_t)ret >= n)
		return((int)(n - 1)); /* troncato: effettivi sono n-1 */
	
	return(ret); /* e' entrato tutto... */
}

/*
	ltos(), ultos(), dtos(), ftos(), qwtos()

	Convertono il numero in stringa formattata e ne restituiscono il puntatore.
*/
typedef enum number_type_t {
	long_number,
	unsigned_long_number,
	double_number,
	float_number,
	qword_number
} NUMBER_TYPE_T;

typedef union number_value_t {
	long			lValue;
	unsigned long	ulValue;
	double			dValue;
	float			fValue;
	QWORD			qwValue;
} NUMBER_VALUE_T;

typedef struct number_t {
	NUMBER_TYPE_T	type;
	NUMBER_VALUE_T	value;
} NUMBER_T;

/*
	_ntos()

	Usata per la conversione base da numero a stringa.
*/
char* _ntos(NUMBER_T* n,char* str,size_t size)
{
	ASSERTEXPR(str && size > 0);

	int i = 0;
	char fractional_str[24] = {0};
	char buffer[48] = {0};
	char* p = buffer;
	char* s = str;

	/* formatta a seconda del tipo di numero */
	switch(n->type)
	{
		case long_number:
			snprintf(buffer,sizeof(buffer),"%ld",n->value.lValue);
			break;
		case unsigned_long_number:
			snprintf(buffer,sizeof(buffer),"%lu",n->value.ulValue);
			break;
		/*
		double/float: precisione implicita VS esplicita:

		- "%g": Quando si usa "%g" senza specificare un numero per la precisione (cioe', senza 
		  il punto e un numero dopo), snprintf usa la precisione predefinita per il tipo.
		  Per un float, la precisione predefinita e' 6 cifre significative, ossia che stampera' 
		  un massimo di 6 cifre totali (prima e dopo la virgola).

		- "%.15g": Qui si specifica una precisione esplicita di 15 cifre significative.

		Quest'ultimo formato e' più comunemente usato con i double proprio perche' i double 
		offrono una precisione molto maggiore (tipicamente 15-17 cifre) rispetto ai float (che 
		ne offrono solo 6-7). Usare %.15g con un float e' tecnicamente possibile, ma le cifre 
		extra oltre la settima sarebbero probabilmente rumore o zeri, non dati significativi.
		*/
		case double_number:
		{
            double integer_part;
            double fractional_part = modf(n->value.dValue,&integer_part);
            snprintf(buffer,sizeof(buffer),"%.0f",integer_part);
            snprintf(fractional_str,sizeof(fractional_str),"%.15g",fractional_part); /* formatta la parte decimale e rimuove lo "0." iniziale */
            if(strlen(fractional_str) >= 2 && fractional_str[1]=='.')
                memmove(fractional_str,fractional_str + 2,strlen(fractional_str) - 1);
            break;
        }
		case float_number:
		{
			float integer_part;
			float fractional_part = modff(n->value.fValue, &integer_part);
            snprintf(buffer,sizeof(buffer),"%.0f",integer_part);
            snprintf(fractional_str,sizeof(fractional_str),"%g",fractional_part); /* formatta la parte decimale e rimuove lo "0." iniziale */
            if(strlen(fractional_str) >= 2 && fractional_str[1]=='.')
                memmove(fractional_str,fractional_str + 2,strlen(fractional_str) - 1);
			break;
		}
        case qword_number:
            snprintf(buffer,sizeof(buffer),"%llu",n->value.qwValue); /* formato corretto per unsigned long long (QWORD) */
            break;
	}

	/* formatta il numero (intero) con le virgole */
	memset(str,'\0',size);

	strrev(buffer);

	while(*p)
	{
		while(*p && ((size_t)(s - str) < size) && ++i < 4)
			*s++ = *p++;

		if((size_t)(s - str) >= size)
			break;

		*s++ = ',';
		
		i = 0;
	}

	if(*(s-1)==',')
		--s;

	*s = '\0';

	strrev(str);

	/* aggiunge la parte frazionaria, presente se si trattava di un double */
	if(*fractional_str)
	{
		size_t len = strlen(str);
        wtfsnprintf(str + len, size - len, ".%s", fractional_str);
	}

	return(str);
}

char* ltos(long num,char* str,size_t size)
{
	ASSERTEXPR(str && size > 0);

	NUMBER_T n;
	n.type = long_number;
	n.value.lValue = num;
	return(_ntos(&n,str,size));
}

char* ultos(unsigned long num,char* str,size_t size)
{
	ASSERTEXPR(str && size > 0);

	NUMBER_T n;
	n.type = unsigned_long_number;
	n.value.ulValue = num;
	return(_ntos(&n,str,size));
}

char* dtos(double num,char* str,size_t size)
{
	ASSERTEXPR(str && size > 0);

	NUMBER_T n;
	n.type = double_number;
	n.value.dValue = num;
	return(_ntos(&n,str,size));
}

char* ftos(float num,char* str,size_t size)
{
	ASSERTEXPR(str && size > 0);

	NUMBER_T n;
	n.type = float_number;
	n.value.fValue = num;
	return(_ntos(&n,str,size));
}

char* qwtos(QWORD num,char* str,size_t size)
{
	ASSERTEXPR(str && size > 0);

	NUMBER_T n;
	n.type = qword_number;
	n.value.qwValue = num;
	return(_ntos(&n,str,size));
}

#define CHECK_ASCII(c)		(c >= 0 && c <= 127)
#define CHECK_ANSIEXT(c)	(c >= 128 && c <= 255)

bool is_all_ascii(const char* str)
{
	while(*str)
	{
		if(!CHECK_ASCII(*str))
			return(false);
		str++;
	}

	return(true);
}

bool has_ansi_ext(const char* str)
{
	while(*str)
	{
		if(CHECK_ANSIEXT(*str))
			return(true);
		str++;
	}

	return(false);
}

/*
	replace_non_ansi()

	Sostituisce i caratteri non ANSI con quello specificato.
*/
void replace_non_ansi(char *str,char chr)
{
	ASSERTEXPR(str);
	ASSERTEXPR(isprint(chr));

	if(str)
	{
		int i = 0;
		while(str[i]!='\0')
		{
			/* un carattere e' ANSI se il suo valore rientra nell'intervallo 0-255
			char puo' essere signed o unsigned per default, per cui converte a 
			unsigned char per assicurarsi che il confronto sia nell'intervallo 0-255 */
			if(((unsigned char)str[i] < 0) || ((unsigned char)str[i] > 255))
				str[i] = chr;
			i++;
		}
	}
}

/*
	replace_non_ascii()

	Sostituisce i caratteri non ASCII con quello specificato.
*/
void replace_non_ascii(char *str,char chr)
{
	ASSERTEXPR(str);
	ASSERTEXPR(isascii(chr));

    if(str)
	{
		int i = 0;
		while (str[i] != '\0')
		{
			/* un carattere e' ASCII se il suo valore e' compreso nell'intervallo 0-127
			come sopra converte a unsigned char per assicurarsi dell'intervallo corretto */
			if((unsigned char)str[i] > 127)
				str[i] = chr;

			i++;
		}
    }
}

/*
	delete_non_ansi()

	Elimina i caratteri non ANSI.
*/
void delete_non_ansi(char *str)
{
	ASSERTEXPR(str);

    if(str)
	{
		int read_idx = 0;
		int write_idx = 0;

		while(str[read_idx]!='\0')
		{
			/* copia il carattere se e' ANSI (tra 0 y 255) */
			if(((unsigned char)str[read_idx] >= 0) && ((unsigned char)str[read_idx] <= 255))
			{
				str[write_idx] = str[read_idx];
				write_idx++;
			}
			read_idx++;
		}
		str[write_idx] = '\0';
	}
}

/*
	delete_non_ascii()

	Elimina i caratteri non ASCII.
*/
void delete_non_ascii(char *str)
{
	ASSERTEXPR(str);

    if(str)
	{
		int read_idx = 0;
		int write_idx = 0;

		while(str[read_idx]!='\0')
		{
			/* copia il carattere se e' ASCII (tra 0 y 127) */
			if((unsigned char)str[read_idx] <= 127)
			{
				str[write_idx] = str[read_idx];
				write_idx++;
			}
			read_idx++;
		}
		str[write_idx] = '\0';
	}
}
