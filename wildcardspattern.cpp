/*$
	wildcardspattern.cpp
	Gestione di pattern con wildcards per nomi files.
	Lasciare come codice sciolto, NON convertire in una classe.
	Luca Piergentili, 26/06/25

	Al momento DoesFileNameMatchWithVectorOfPatterns(), che e' la funzione che si incarica di verificare il 
	nomefile/pathname con il vettore di patterns, permette di scegliere tra tre funzioni differenti: quella 
	che sembre piu' adatta allo scopo e' quella che emula la gestione delle wildcards MS-DOS.
*/
#include "pragma.h" 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "strings.h"
#include "window.h"
#include "win32api.h" 
#include "msdoswildcardspattern.h"
#include "wildcardspattern.h"
#include "CWildCards.h"
#include <string>
#include <vector>
#include <algorithm>
#include <cctype>

#include "traceexpr.h"
//#define _TRACE_FLAG			_TRFLAG_TRACEOUTPUT // opzioni: _TRFLAG_NOTRACE, _TRFLAG_TRACEFILE, _TRFLAG_TRACECONSOLE, _TRFLAG_TRACEOUTPUT, _TRFLAG_TRACEBREAKPOINT
#define _TRACE_FLAG			_TRFLAG_NOTRACE // opzioni: _TRFLAG_NOTRACE, _TRFLAG_TRACEFILE, _TRFLAG_TRACECONSOLE, _TRFLAG_TRACEOUTPUT, _TRFLAG_TRACEBREAKPOINT
#define _TRACE_FLAG_INFO	_TRACE_FLAG
#define _TRACE_FLAG_WARN	_TRACE_FLAG
#define _TRACE_FLAG_ERR		_TRACE_FLAG

/*
	ToLower()

	Converte la stringa in minuscolo.
*/
std::string ToLower(const char* str)
{
    std::string lowerStr(str);
    // il casting a (unsigned char) per evitare problemi con caratteri non ASCII
    std::transform(lowerStr.begin(),lowerStr.end(),lowerStr.begin(),[](unsigned char c){return std::tolower(c);});
    return(lowerStr);
}

/*
	SplitPattern()
	
	Carica le esclusioni in un vettore, separare piu' esclusioni con ';'.

	esempio:
	
	// scorre il vettore usando un iteratore
	std::vector<std::string> patterns = SplitPattern("str1;str2;str3");
	std::vector<std::string>::const_iterator it;

	printf("\nSplitPattern(\"str1;str2;str3\")\n");
    for(it = patterns.begin(); it != patterns.end(); ++it) {
        // 'operatore di dereferenziazione (*) sull'iteratore restituisce un riferimento all'elemento
        // poiché e' una std::string, usiamo .c_str() per ottenere un puntatore C-style per printf
        printf("%s\n", it->c_str()); // o (*it).c_str()
    }
	printf("[Press Enter]\n");
	getchar();
*/
std::vector<std::string> SplitPattern(const char* patternStr)
{
    std::vector<std::string> exclusions;
    std::string current;

    while(*patternStr)
	{
        if(*patternStr==';')
		{
            if(!current.empty())
			{
                exclusions.push_back(current);
                current.clear();
            }
        }
		else
		{
            current += *patternStr;
        }
        
		patternStr++;
    }
    
	if(!current.empty())
        exclusions.push_back(current);

	return(exclusions);
}

/*
	DoesStringMatchWithPattern()

	Funzione ricorsiva per verificare se la stringa soddisfa il pattern (con wildcard '*' e '?').
	Confronta una stringa (testo) con un pattern (modello).
	Gestisce * (zero o più caratteri) e ? (un singolo carattere).

	esempio:

	// stringa che deve soddisfare il pattern - stringa con il pattern (con * e/o ?) che deve essere soddisfatto
	printf("\nDoesStringMatchWithPattern(<actual string>,<pattern for match>):\n");
	printf("DoesStringMatchWithPattern(\"Debug\",\"Debug\") - %s\n",DoesStringMatchWithPattern("Debug","Debug") ? "yes" : "no");
	printf("DoesStringMatchWithPattern(\"Debug\",\"*Debug?\") - %s\n",DoesStringMatchWithPattern("Debug","*Debug?") ? "yes" : "no");
	printf("DoesStringMatchWithPattern(\"EmbeddedDebug\",\"Debug\") - %s\n",DoesStringMatchWithPattern("EmbeddedDebug","Debug") ? "yes" : "no");
	printf("DoesStringMatchWithPattern(\"DebugEmbedded\",\"Debug\") - %s\n",DoesStringMatchWithPattern("DebugEmbedded","Debug") ? "yes" : "no");
	printf("DoesStringMatchWithPattern(\"EmbeddedDebug\",\"*Debug\") - %s\n",DoesStringMatchWithPattern("EmbeddedDebug","*Debug") ? "yes" : "no");
	printf("DoesStringMatchWithPattern(\"DebugEmbedded\",\"Debug*\") - %s\n",DoesStringMatchWithPattern("DebugEmbedded","Debug*") ? "yes" : "no");
	printf("DoesStringMatchWithPattern(\"Debug\",\"\\Debug\\\") - %s\n",DoesStringMatchWithPattern("Debug","\\Debug\\") ? "yes" : "no");
	printf("DoesStringMatchWithPattern(\"Debug\",\"*Debug*\") - %s\n",DoesStringMatchWithPattern("Debug","*Debug*") ? "yes" : "no");
	printf("DoesStringMatchWithPattern(\"BlaBlaDebugBlaBla\",\"*Debug*\") - %s\n",DoesStringMatchWithPattern("BlaBlaDebugBlaBla","*Debug*") ? "yes" : "no");
	printf("DoesStringMatchWithPattern(\"Debug\",\"\\*Debug*\\\") - %s\n",DoesStringMatchWithPattern("Debug","\\*Debug*\\") ? "yes" : "no"); // non accetta * all'interno
	printf("DoesStringMatchWithPattern(\"Deb_g\",\"Deb?g\") - %s\n",DoesStringMatchWithPattern("Deb_g","Deb?g") ? "yes" : "no");
	printf("DoesStringMatchWithPattern(\"Deb_g\",\"Deb*g\") - %s\n",DoesStringMatchWithPattern("Deb_g","Deb*g") ? "yes" : "no");
	printf("DoesStringMatchWithPattern(\"Deb_-_-_g\",\"Deb?g\") - %s\n",DoesStringMatchWithPattern("Deb_-_-_g","Deb?g") ? "yes" : "no");
	printf("DoesStringMatchWithPattern(\"Deb_-_-_g\",\"Deb*g\") - %s\n",DoesStringMatchWithPattern("Deb_-_-_g","Deb*g") ? "yes" : "no");
	printf("[Press Enter]\n");
	getchar();
*/
bool DoesStringMatchWithPattern(const char* text, const char* pattern)
{
    // caso base: se il pattern e' finito
    if(*pattern=='\0')
        return(*text=='\0'); // corrisponde solo se anche il testo e' finito

    // se il carattere corrente del pattern e' '?' o corrisponde letteralmente
    if(*pattern=='?' || (*pattern==*text && *text!='\0'))
        return(DoesStringMatchWithPattern(text + 1, pattern + 1)); // passa al carattere successivo in entrambi

    // se il carattere corrente del pattern e' '*'
    if(*pattern=='*')
	{
		while(*(pattern + 1)=='*')	// ignora eventuali '*' consecutivi nel pattern (es. "**" e' come "*")
			pattern++;
        
        // due possibilita' per '*':
        // 1. '*' non corrisponde a nessun carattere nel testo (text rimane invariato)
        // 2. '*' corrisponde a uno o più caratteri nel testo (text avanza)
        // prova a far corrispondere il resto del pattern con il testo rimanente, avanzando 
		// il testo un carattere alla volta finché non si trova una corrispondenza
        return(DoesStringMatchWithPattern(text,pattern + 1) || (*text!='\0' && DoesStringMatchWithPattern(text + 1,pattern)));
    }

    // se nessuna delle condizioni precedenti e' vera, non c'e' corrispondenza
    return(false);
}

/*
	DoesFileNameMatchWithPattern()
	
	Confronta il nome del file (con o senza pathname) con un pattern (wildcards).

	Occhio: il codice qui sotto controlla (fa il match) per nome file, NON per pathname completo
	le funzioni di ricerca files analizzano (scorrono) sempre per nome (che puo' essere di file o
	di directory), questo perche' ricorrono le dir una per volta, quindi per escludere i pathnames 
	che ad es. contengano la stringa 'Debug', come in: 'C:\TMP\Debug\...', come pattern non si puo' 
	passare '*\Debug\*' ma bisogna passare 'Debug'

	fallisce al verificare le (sub)stringhe dentro un pathname, es c:\debug\file.txt *debug*

	esempio (per questa e le seguenti);

	// stringa che deve soddisfare il pattern - stringa con il pattern (con * e ?) che deve essere soddisfatto
	printf("\nDoesFileNameMatchWithPattern(<filename>,<pattern for match>):\n");
	printf("DoesFileNameMatchWithPattern(\"c:\\debug\",            \"debug\") - %s\n",		
			 DoesFileNameMatchWithPattern("c:\\debug",              "debug") ? "yes" : "no");
	printf("DoesFileNameMatchWithPattern(\"c:\\debug\\02.jpg\",    \"debug\") - %s\n",		
			 DoesFileNameMatchWithPattern("c:\\debug\\02.jpg",      "debug") ? "yes" : "no");
	printf("DoesFileNameMatchWithPattern(\"c:\\debug\\02.jpg\",  \"\\debug\\\") - %s\n",	
			 DoesFileNameMatchWithPattern("c:\\debug\\02.jpg",    "\\debug\\") ? "yes" : "no");
	printf("DoesFileNameMatchWithPattern(\"c:\\debug\\02.jpg\", \"*\\debug\\*\") - %s\n",
			 DoesFileNameMatchWithPattern("c:\\debug\\02.jpg",   "*\\debug\\*") ? "yes" : "no");
	printf("DoesFileNameMatchWithPattern(\"c:\\debug\\02.jpg\",   \"*debug*\") - %s\n",	
			DoesFileNameMatchWithPattern("c:\\debug\\02.jpg",      "*debug*") ? "yes" : "no");
	printf("DoesFileNameMatchWithPattern(\"c:\\tmp\\aa.jpg\",      \"*.jpg\") - %s\n",	
			 DoesFileNameMatchWithPattern("c:\\tmp\\aa.jpg",        "*.jpg") ? "yes" : "no");
	printf("DoesFileNameMatchWithPattern(\"c:\\tmp\\aa.jpg\",     \"a*.jpg\") - %s\n",	
			 DoesFileNameMatchWithPattern("c:\\tmp\\aa.jpg",       "a*.jpg") ? "yes" : "no");
	printf("DoesFileNameMatchWithPattern(\"c:\\tmp\\aa.jpg\",     \"a?.jpg\") - %s\n",	
			 DoesFileNameMatchWithPattern("c:\\tmp\\aa.jpg",       "a?.jpg") ? "yes" : "no");

	printf("\nDoesFileNameMatchWithPatternEx(<filename>,<pattern for match>):\n");
	printf("DoesFileNameMatchWithPatternEx(\"c:\\debug\",            \"debug\") - %s\n",		
			 DoesFileNameMatchWithPatternEx("c:\\debug",              "debug") ? "yes" : "no");
	printf("DoesFileNameMatchWithPatternEx(\"c:\\debug\\02.jpg\",    \"debug\") - %s\n",		
			 DoesFileNameMatchWithPatternEx("c:\\debug\\02.jpg",      "debug") ? "yes" : "no");
	printf("DoesFileNameMatchWithPatternEx(\"c:\\debug\\02.jpg\",  \"\\debug\\\") - %s\n",	
			 DoesFileNameMatchWithPatternEx("c:\\debug\\02.jpg",    "\\debug\\") ? "yes" : "no");
	printf("DoesFileNameMatchWithPatternEx(\"c:\\debug\\02.jpg\", \"*\\debug\\*\") - %s\n",	
			 DoesFileNameMatchWithPatternEx("c:\\debug\\02.jpg",   "*\\debug\\*") ? "yes" : "no");
	printf("DoesFileNameMatchWithPatternEx(\"c:\\debug\\02.jpg\",   \"*debug*\") - %s\n",		
			 DoesFileNameMatchWithPatternEx("c:\\debug\\02.jpg",     "*debug*") ? "yes" : "no");
	printf("DoesFileNameMatchWithPatternEx(\"c:\\tmp\\aa.jpg\",      \"*.jpg\") - %s\n",	
			 DoesFileNameMatchWithPatternEx("c:\\tmp\\aa.jpg",        "*.jpg") ? "yes" : "no");
	printf("DoesFileNameMatchWithPatternEx(\"c:\\tmp\\aa.jpg\",     \"a*.jpg\") - %s\n",	
			 DoesFileNameMatchWithPatternEx("c:\\tmp\\aa.jpg",       "a*.jpg") ? "yes" : "no");
	printf("DoesFileNameMatchWithPatternEx(\"c:\\tmp\\aa.jpg\",     \"a?.jpg\") - %s\n",	
			 DoesFileNameMatchWithPatternEx("c:\\tmp\\aa.jpg",       "a?.jpg") ? "yes" : "no");

	printf("\nDoesFileNameMatchWithPatternMSDOS(<filename>,<pattern for match>):\n");
	printf("DoesFileNameMatchWithPatternMSDOS(\"c:\\debug\",            \"debug\") - %s\n",		
			 DoesFileNameMatchWithPatternMSDOS("c:\\debug",              "debug") ? "yes" : "no");
	printf("DoesFileNameMatchWithPatternMSDOS(\"c:\\debug\\02.jpg\",    \"debug\") - %s\n",		
			 DoesFileNameMatchWithPatternMSDOS("c:\\debug\\02.jpg",      "debug") ? "yes" : "no");
	printf("DoesFileNameMatchWithPatternMSDOS(\"c:\\debug\\02.jpg\",  \"\\debug\\\") - %s\n",	
			 DoesFileNameMatchWithPatternMSDOS("c:\\debug\\02.jpg",    "\\debug\\") ? "yes" : "no");
	printf("DoesFileNameMatchWithPatternMSDOS(\"c:\\debug\\02.jpg\", \"*\\debug\\*\") - %s\n",	
			 DoesFileNameMatchWithPatternMSDOS("c:\\debug\\02.jpg",   "*\\debug\\*") ? "yes" : "no");
	printf("DoesFileNameMatchWithPatternMSDOS(\"c:\\debug\\02.jpg\",   \"*debug*\") - %s\n",		
			 DoesFileNameMatchWithPatternMSDOS("c:\\debug\\02.jpg",     "*debug*") ? "yes" : "no");
	printf("DoesFileNameMatchWithPatternMSDOS(\"c:\\tmp\\aa.jpg\",      \"*.jpg\") - %s\n",	
			 DoesFileNameMatchWithPatternMSDOS("c:\\tmp\\aa.jpg",        "*.jpg") ? "yes" : "no");
	printf("DoesFileNameMatchWithPatternMSDOS(\"c:\\tmp\\aa.jpg\",     \"a*.jpg\") - %s\n",	
			 DoesFileNameMatchWithPatternMSDOS("c:\\tmp\\aa.jpg",       "a*.jpg") ? "yes" : "no");
	printf("DoesFileNameMatchWithPatternMSDOS(\"c:\\tmp\\aa.jpg\",     \"a?.jpg\") - %s\n",	
			 DoesFileNameMatchWithPatternMSDOS("c:\\tmp\\aa.jpg",       "a?.jpg") ? "yes" : "no");
	printf("[Press Enter]\n");
	getchar();
*/
bool DoesFileNameMatchWithPattern(const char* fullPath, const char* pattern)
{
	static CWildCards wld;
	wld.SetIgnoreCase(TRUE);
	return(wld.Match(pattern,StripPathFromFile(fullPath)));
}

/*
	DoesFileNameMatchWithPatternEx()

	Confronta il nome del file (con o senza pathname) con un pattern (wildcards).
*/
bool DoesFileNameMatchWithPatternEx(const char* fullPath, const char* pattern)
{
    if(!fullPath || !pattern)
        return(false);

    std::string patternLower = ToLower(pattern);
    std::string fullPathLower = ToLower(fullPath);

    // primo controllo: verifica se l'intero percorso (fullPath) corrisponde al pattern
    // questo e' utile per pattern specifici di percorso, come "C:\temp\*.log" o "D:\Progetti\*\escludi.dll"
    if (DoesStringMatchWithPattern(fullPathLower.c_str(), patternLower.c_str())) {
        return true;
    }

    // secondo controllo: se il pattern non corrisponde all'intero percorso, scompone il percorso in componenti 
	// (nomi di directory e nomi file) e controlla se *qualsiasi* di questi componenti corrisponde al pattern
    // questo e' utile per pattern generici come "*.obj", "temp", "bin"

    // crea una copia modificabile del percorso per strtok_s
    char pathBuffer[_MAX_PATH+1];
    strcpyn(pathBuffer, fullPathLower.c_str(),sizeof(pathBuffer));

    char* token = NULL;
    char* next_token = NULL; // per strtok_s

    // usa strtok_s per dividere il percorso in componenti, usando sia '\' che '/'
    token = strtok_s(pathBuffer, "\\/", &next_token);

    while(token!=NULL)
	{
        // se il componente corrente (nome di directory o nome del file) corrisponde al pattern
        if(DoesStringMatchWithPattern(token, patternLower.c_str()))
            return(true); // trovata una corrispondenza

        // passa al componente successivo
        token = strtok_s(NULL,"\\/",&next_token);
    }

    return(false);
}

/*
	DoesFileNameMatchWithPatternMSDOS()

	Confronta il nome del file (con o senza pathname) con un pattern (wildcards).
*/
bool DoesFileNameMatchWithPatternMSDOS(const char* fullPath, const char* pattern)
{
	return(msdos_pattern_match(fullPath,pattern));
}

/*
	DoesFileNameMatchWithVectorOfPatterns()
	
	Verifica se il nome file concorda con i pattern(s) del vettore.
*/
bool DoesFileNameMatchWithVectorOfPatterns(const char* name,const std::vector<std::string>& exclusions,int matchtype)
{
	bool match = false;

	for(const auto& pattern : exclusions)
	{
		switch(matchtype) {
			case WILDCARDS_MATCH_MSDOS:
				match = DoesFileNameMatchWithPatternMSDOS(name,pattern.c_str());
				break;
			case WILDCARDS_MATCH_EXTENDED:
				match = DoesFileNameMatchWithPatternEx(name,pattern.c_str());
				break;
			case WILDCARDS_MATCH_STANDARD:
			default:
				match = DoesFileNameMatchWithPattern(name,pattern.c_str());
				break;
		}

		if(match)
			break;
	}

    return(match);
}
