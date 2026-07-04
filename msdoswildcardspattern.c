/*$
	msdoswildcardspattern.c
	Luca Piergentili, 26/06/25

	Implementa il match con un pattern (una specifica che puo' contenere wildcards)
	nello stesso modo in cui veniva implementato dall'MS-DOS nelle ricerche sui nomi
	dei files.

	Contiene le versioni ANSI e Wide (Unicode).

	Per definire i pattern di esclusione:
	- per le directory, terminare il pattern con \ o / (es. "D:\\DEV\\grokbackup\\Debug\\").
	- per i file o i nomi di directory specifici, usare i pattern normali (es. *.obj, temp).

	esempio:

		// lista esclusioni
		const char* myExclusions[] = {
			"D:\\DEV\\grokbackup\\Debug\\",	// esclusione ricorsiva della directory
			"debug",                        // esclusione di file per estensione
			"*.obj",                        // esclusione di file per estensione
			"temp"                          // esclusione di directory o file con questo nome
		};
		int numExclusions = sizeof(myExclusions) / sizeof(myExclusions[0]);
		printf("\nexclusions:\n");
		for(int i=0; i < numExclusions; i++) {
			printf("%s\n",myExclusions[i]);
		}
		printf("\n");
		// la directory 'Debug' stessa
		printf("Is 'D:\\DEV\\grokbackup\\Debug' excluded? %s\n",
		msdos_pattern_excluded("D:\\DEV\\grokbackup\\Debug", myExclusions, numExclusions) ? "YES" : "NO");
		// Uun file all'interno della directory 'Debug'
		printf("Is 'D:\\DEV\\grokbackup\\Debug\\grokbackup.vcxproj.FileListAbsolute.txt' excluded? %s\n",
		msdos_pattern_excluded("D:\\DEV\\grokbackup\\Debug\\grokbackup.vcxproj.FileListAbsolute.txt", myExclusions, numExclusions) ? "YES" : "NO");
		// un file in una sottodirectory di 'Debug'
		printf("Is 'D:\\DEV\\grokbackup\\Debug\\subdir\\file.txt' excluded? %s\n",
		msdos_pattern_excluded("D:\\DEV\\grokbackup\\Debug\\subdir\\file.txt", myExclusions, numExclusions) ? "YES" : "NO");
		// un file .obj
		printf("Is 'D:\\Anywhere\\temp.obj' excluded? %s\n",
		msdos_pattern_excluded("D:\\Anywhere\\temp.obj", myExclusions, numExclusions) ? "YES" : "NO");
		// un file in una directory chiamata "temp"
		printf("Is 'C:\\Projects\\my_app\\temp\\data.txt' excluded? %s\n",
		msdos_pattern_excluded("C:\\Projects\\my_app\\temp\\data.txt", myExclusions, numExclusions) ? "YES" : "NO");
		// una directory chiamata "temp"
		printf("Is 'C:\\Projects\\my_app\\temp' excluded? %s\n",
		msdos_pattern_excluded("C:\\Projects\\my_app\\temp", myExclusions, numExclusions) ? "YES" : "NO");
		// un percorso che non dovrebbe essere escluso
		printf("Is 'D:\\DEV\\grokbackup\\source.cpp' excluded? %s\n",
		msdos_pattern_excluded("D:\\DEV\\grokbackup\\source.cpp", myExclusions, numExclusions) ? "YES" : "NO");

		const char* testExclusions2[] = {
			"C:/Program Files/My App/"
		};
		int numExclusions2 = sizeof(testExclusions2) / sizeof(testExclusions2[0]);
		printf("\nexclusion (to check normalization):\n");
		for(int i=0; i < numExclusions2; i++) {
			printf("%s\n",testExclusions2[i]);
		}
		printf("\n");
		printf("Is 'C:\\Program Files\\My App\\data.log' ? %s\n",
		msdos_pattern_excluded("C:\\Program Files\\My App\\data.log", testExclusions2, numExclusions2) ? "YES" : "NO");
		printf("Is 'C:/Program Files/My App/another.txt' ? %s\n",
		msdos_pattern_excluded("C:/Program Files/My App/another.txt", testExclusions2, numExclusions2) ? "YES" : "NO");

		printf("[Press Enter]\n");
		getchar();
*/
#include "pragma.h" 
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <wctype.h>
#include "strings.h"
#include "msdoswildcardspattern.h"

#include "traceexpr.h"
//#define _TRACE_FLAG			_TRFLAG_TRACEOUTPUT // opzioni: _TRFLAG_NOTRACE, _TRFLAG_TRACEFILE, _TRFLAG_TRACECONSOLE, _TRFLAG_TRACEOUTPUT, _TRFLAG_TRACEBREAKPOINT
#define _TRACE_FLAG			_TRFLAG_NOTRACE // opzioni: _TRFLAG_NOTRACE, _TRFLAG_TRACEFILE, _TRFLAG_TRACECONSOLE, _TRFLAG_TRACEOUTPUT, _TRFLAG_TRACEBREAKPOINT
#define _TRACE_FLAG_INFO	_TRACE_FLAG
#define _TRACE_FLAG_WARN	_TRACE_FLAG
#define _TRACE_FLAG_ERR		_TRACE_FLAG

#define MAX_PATH_EXCLUDE 4096

/*
	msdos_normalize_path()
 
	Converte tutto in minuscolo cambiando gli \ in / per garantire confronti consistenti.
*/
void msdos_normalize_path(char* path)
{
    for(register int i = 0; path[i]; i++)
	{
        if(path[i]=='\\')
            path[i]='/';
    
		path[i] = tolower((unsigned char)path[i]);
    }
}
void msdos_normalize_pathW(wchar_t* path)
{
	while(*path)
	{
		if(*path == L'\\')
			*path = L'/';
        
		*path = towlower(*path);
		path++;
	}
}

/*
	msdos_pattern_match()

	Controlla se il pathname corrisponde con il pattern, es. c:/temp/log.txt VS c:/temp/*.txt
	Divide il pathname nei suoi componenti e per ognuno di essi controlla se il componente (NON 
	la stringa intera) corrisponde al pattern, es. my_file.txt VS *.txt, o Debug VS Debug.
	
	Crea copie normalizzate ed in minuscolo delle stringhe prima di chiamare msdos_wildcards_match().
	
	Utilizza strtok per la suddivisione dei componenti. Se e' definito _MSC_VER usa strtok_s che e' 
	la versione sicura su Visual Studio, altrimenti usa strtok (che non e' thread-safe e modifica 
	la stringa originale).
*/
bool msdos_pattern_match(const char* fullpath,const char* pattern)
{
    /* crea una copia in locale
	nonostante il fatto che _MAX_PATH dovrebbe essere sufficente per percorsi MS-DOS, usa 
	un buffer sovradimensionato per gestire percorsi piu' lunghi, ad es. url provenienti 
	da siti blogger, notoriamente lunghe
	*/
	#define MAX_PATH_BIGGER 2048

    char fullPathNormalized[MAX_PATH_BIGGER+1] = {0};
    char patternNormalized[MAX_PATH_BIGGER+1] = {0};

    strcpyn(fullPathNormalized,fullpath,sizeof(fullPathNormalized));
    msdos_normalize_path(fullPathNormalized);

    strcpyn(patternNormalized,pattern,sizeof(patternNormalized));
    msdos_normalize_path(patternNormalized);

    /* corrispondenza wildcard generale sull'intero percorso (es. "c:/temp/*.log")
       questo e' utile per pattern che coprono l'intero percorso */
    if(msdos_wildcards_match(fullPathNormalized,patternNormalized))
        return(true);

    /* corrispondenza a livello di componente (es. "debug", "*.bak", "build*")
       estrae i componenti del percorso e li confronta */
    char temp_path[MAX_PATH_BIGGER+1];
    strcpyn(temp_path,fullPathNormalized,sizeof(temp_path));

    char* token;
    char* rest = temp_path;
    char delimiters[] = "/"; /* ha gia' normalizzato i separatori a '/'  */

    while((token = strtok(rest, delimiters))!=NULL)
	{
        if(msdos_wildcards_match(token,patternNormalized))
            return(true);

        rest = NULL;  /* per le chiamate successive di strtok */
    }

    return(false);
}
bool msdos_pattern_matchW(const wchar_t* fullpath,const wchar_t* pattern)
{
	wchar_t fullPathNormalized[MAX_PATH_BIGGER+1] = {0};
	wchar_t patternNormalized[MAX_PATH_BIGGER+1] = {0};

	/* wcsncpy vuole il numero massimo di caratteri da copiare, non i byte
	   protegge il buffer lasciando spazio per il terminatore zero finale manuale */
	wcsncpy(fullPathNormalized,fullpath,MAX_PATH_BIGGER);
	fullPathNormalized[MAX_PATH_BIGGER] = L'\0';
	msdos_normalize_pathW(fullPathNormalized);

	wcsncpy(patternNormalized,pattern,MAX_PATH_BIGGER);
	patternNormalized[MAX_PATH_BIGGER] = L'\0';
	msdos_normalize_pathW(patternNormalized);

	/* corrispondenza wildcard generale sull'intero percorso */
	if(msdos_wildcards_matchW(fullPathNormalized,patternNormalized))
		return(true);

	/* corrispondenza a livello di componente */
	wchar_t temp_path[MAX_PATH_BIGGER+1] = {0};
	wcsncpy(temp_path,fullPathNormalized,MAX_PATH_BIGGER);
	temp_path[MAX_PATH_BIGGER] = L'\0';

	wchar_t* token;
    
	/* wcstok standard C (ISO C) richiede un terzo parametro (un puntatore di contesto)
	   questo elimina la necessita' del vecchio trucco "rest = NULL" ed e' thread-safe */
	wchar_t* context = NULL; 
	wchar_t delimiters[] = L"/"; 

	/* la prima chiamata passa la stringa da dividere */
	token = wcstok(temp_path,delimiters,&context);
	while(token)
	{
		if(msdos_wildcards_matchW(token,patternNormalized))
			return(true);

		/* le chiamate successive passano NULL come primo parametro
		   ma mantengono il 'context' intatto autonomamente */
		token = wcstok(NULL,delimiters,&context);
	}

	return(false);
}

/*
	msdos_pattern_excluded()

	Determina se un percorso deve essere escluso.
	Logica per l'esclusione ricorsiva di directory: se un pattern di esclusione termina con / (dopo la normalizzazione, 
	es. "d:/my_project/debug/"), la funzione controlla se il pathname (normalizzato) inizia con quel pattern, che e' il
	modo più robusto per escludere intere sottodirectory.
	Per tutti gli altri pattern (che non terminano con /, es. *.obj, temp), delega il controllo a msdos_pattern_match().
	Gestisce la copia e la normalizzazione delle stringhe in modo che non vengano modificate le originali.
	
	Accetta un percorso C-style e un array di stringhe C per le esclusioni.
	Deve gestire sia i pattern generici che le esclusioni di intere sottodirectory.
	
	In input il percorso completo del file/directory da controllare, l'array di puntatori con i patterns di esclusione ed
	il numero di patterns nell'array.
*/
bool msdos_pattern_excluded(const char* fullpath, const char* exclusions_array[], int num_exclusions)
{
    /* crea una copia in locale
	nonostante il fatto che _MAX_PATH dovrebbe essere sufficente per percorsi MS-DOS, usa 
	un buffer sovradimensionato per gestire percorsi piu' lunghi, ad es. url provenienti 
	da siti blogger, notoriamente lunghe
	*/
    char fullPathNormalized[4096] = {0};
    char patternNormalized[4096] = {0};

    strcpyn(fullPathNormalized,fullpath,sizeof(fullPathNormalized));
    msdos_normalize_path(fullPathNormalized);

    for(int i = 0; i < num_exclusions; ++i)
	{
        const char* pattern = exclusions_array[i];
        if(!pattern)
			continue;

        memset(patternNormalized,'\0',sizeof(patternNormalized));
        strcpyn(patternNormalized,pattern,sizeof(patternNormalized));
        msdos_normalize_path(patternNormalized);

        size_t pattern_len = strlen(patternNormalized);

        /* LOGICA PER L'ESCLUSIONE RICORSIVA DI DIRECTORY (es. "d:/dev/backup/debug/")
           se il pattern normalizzato termina con '/', lo tratta come un pattern di directory ricorsivo */
        if (pattern_len > 0 && patternNormalized[pattern_len - 1] == '/') {
            /* controlla se 'fullPathNormalized' inizia con 'patternNormalized'
               usa strncmp su percorsi normalizzati per confronto case-insensitive e per separatori */
            if (strncmp(fullPathNormalized, patternNormalized, pattern_len) == 0) {
                return(true); /* il percorso rientra nella directory da escludere */
            }

            /* controlla anche se il 'fullPathNormalized' e' ESATTAMENTE la directory da escludere
               (es. fullPath = "d:/dev/grokbackup/debug" vs pattern = "d:/dev/grokbackup/debug/") */
            char patternNoSlash[4096] = {0};
            strncpy(patternNoSlash, patternNormalized, pattern_len - 1);
            patternNoSlash[pattern_len-1] = '\0';

            if(strcmp(fullPathNormalized,patternNoSlash)==0)
                return(true);
        }
        /* FINE LOGICA ESCLUSIONE DIRECTORY */

        /* controllo generico del pattern, per file/directory specifici o pattern con wildcard generali
           usato per "*.obj", "temp", "build*" */
        if(msdos_pattern_match(fullpath,pattern)) // passa gli originali, msdos_pattern_match li normalizza
            return(true);
    }
    return(false);
}
bool msdos_pattern_excluded(const wchar_t* fullpath,const wchar_t* exclusions_array[],int num_exclusions)
{
    wchar_t fullPathNormalized[MAX_PATH_EXCLUDE] = {0};
    wchar_t patternNormalized[MAX_PATH_EXCLUDE] = {0};

    /* copia sicura del percorso completo (lasciando spazio per il terminatore a fine buffer) */
    wcsncpy(fullPathNormalized,fullpath,MAX_PATH_EXCLUDE-1);
    fullPathNormalized[MAX_PATH_EXCLUDE-1] = L'\0';
    msdos_normalize_pathW(fullPathNormalized);

    for(int i=0; i < num_exclusions; ++i)
    {
        const wchar_t* pattern = exclusions_array[i];
        if(!pattern)
            continue;

        wcsncpy(patternNormalized,pattern,MAX_PATH_EXCLUDE-1);
        patternNormalized[MAX_PATH_EXCLUDE-1] = L'\0';
        msdos_normalize_pathW(patternNormalized);

        size_t pattern_len = wcslen(patternNormalized);

        /* logica per l'esclusione ricorsiva di directory (es. "d:/dev/backup/debug/") */
        if(pattern_len > 0 && patternNormalized[pattern_len-1]==L'/') 
        {
            /* wcsncmp confronta i primi 'pattern_len' caratteri wide */
            if(wcsncmp(fullPathNormalized,patternNormalized,pattern_len)==0)
                return(true);

            /* controlla se il percorso corrisponde esattamente alla directory senza lo slash finale */
            wchar_t patternNoSlash[MAX_PATH_EXCLUDE] = {0};
            
            /* copia fino a pattern_len - 1 caratteri */
            wcsncpy(patternNoSlash,patternNormalized,pattern_len-1);
            patternNoSlash[pattern_len-1] = L'\0';

            /* wcscmp per il confronto esatto di stringhe wide */
            if(wcscmp(fullPathNormalized,patternNoSlash)==0)
                return(true);
        }

        /* controllo generico del pattern */
        if(msdos_pattern_matchW(fullpath,pattern)) 
            return(true);
    }

    return(false);
}

/*
	msdos_wildcards_match()

	Funzione per il confronto di stringhe con wildcard * e ?, ricorsiva, segue la logica standard 
	per le wildcard MS-DOS. I parametri di input devono essere già normalizzati ed in minuscolo.

	'text' e' la stringa da confrontare (es. "file.txt", "Debug")
	'pattern' e' il pattern con wildcard (es. "*.txt", "Debug", "build*")
*/
bool msdos_wildcards_match(const char* text, const char* pattern)
{
	if(*pattern=='\0')
        return(*text=='\0');

    if((*pattern=='?' && *text!='\0') || (*pattern==*text && *text!='\0'))
        return(msdos_wildcards_match(text + 1, pattern + 1));
    
    if(*pattern=='*')
	{
        while(*(pattern + 1)=='*')	/* salta i * consecutivi nel pattern */
            pattern++;
        
        /* loop per permettere a * di corrispondere a 0 o più caratteri nel testo */
        while(*text!='\0' || *(pattern + 1)=='\0')
		{
			/* il * corrisponde a 0 o più caratteri attuali, e il resto del pattern corrisponde */
            if(msdos_wildcards_match(text,pattern + 1))
                return(true);

            /* se il testo e' finito, * non puo' corrispondere a piu' caratteri, esce */
			if(*text=='\0')
                break;

            text++; /* prova a far corrispondere * con il prossimo carattere nel testo */
        }

        return(false); /* il loop termina senza corrispondenza */
    }

    return(false);
}
bool msdos_wildcards_matchW(const wchar_t* text,const wchar_t* pattern)
{
    if(*pattern==L'\0')
        return(*text==L'\0');

    /* gestione del carattere singolo '?' o della corrispondenza esatta dei caratteri attuali */
    if((*pattern==L'?' && *text!=L'\0') || (*pattern==*text && *text!=L'\0'))
        return(msdos_wildcards_matchW(text+1,pattern+1));
    
    /* gestione della wildcard '*' */
    if(*pattern==L'*')
    {
        while(*(pattern+1)==L'*') /* salta i * consecutivi nel pattern */
            pattern++;
        
        /* loop per permettere a * di corrispondere a 0 o più caratteri nel testo */
        while(*text!=L'\0' || *(pattern+1)==L'\0')
        {
            /* il * corrisponde a 0 o piu' caratteri attuali, e il resto del pattern corrisponde */
            if(msdos_wildcards_matchW(text,pattern+1))
                return(true);

            /* se il testo e' finito, * non puo' corrispondere a piu' caratteri, esce */
            if(*text==L'\0')
                break;

            text++; /* prova a far corrispondere * con il prossimo carattere nel testo */
        }

        return(false); /* il loop termina senza corrispondenza */
    }

    return(false);
}