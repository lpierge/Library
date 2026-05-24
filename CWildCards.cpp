/*$
	CWildCards.cpp
	Classe base per gestione skeleton/pattern.
	Luca Piergentili, 26/06/00
	lpiergentili@yahoo.com

	Vedi le note in CWildCards.h
*/
#include "pragma.h"
#include "env.h"
#include "macro.h"
#ifdef _WINDOWS
  #include "window.h"
#endif
#include <stdio.h>
#include <string.h>
#include "strings.h"
#include <stdlib.h>
#include "CNodeList.h"
#include "CWildCards.h"

#include "traceexpr.h"
//#define _TRACE_FLAG			_TRFLAG_TRACEOUTPUT // opzioni: _TRFLAG_NOTRACE, _TRFLAG_TRACEFILE, _TRFLAG_TRACECONSOLE, _TRFLAG_TRACEOUTPUT, _TRFLAG_TRACEBREAKPOINT
#define _TRACE_FLAG			_TRFLAG_NOTRACE // opzioni: _TRFLAG_NOTRACE, _TRFLAG_TRACEFILE, _TRFLAG_TRACECONSOLE, _TRFLAG_TRACEOUTPUT, _TRFLAG_TRACEBREAKPOINT
#define _TRACE_FLAG_INFO	_TRACE_FLAG
#define _TRACE_FLAG_WARN	_TRACE_FLAG
#define _TRACE_FLAG_ERR		_TRACE_FLAG

/*
	CWildCardItem
*/
CWildCardItem::CWildCardItem(char* pString)
{
	ASSERTEXPR(pString);

	int nLen = strlen(pString);
	nLen = nLen <= 0 ? 1 : nLen;
	m_pString = new char[nLen+1];
	memset(m_pString,'\0',nLen+1);
	strcpyn(m_pString,pString,nLen+1);
}

/*
	~CWildCardItem()
*/
CWildCardItem::~CWildCardItem()
{
	if(m_pString)
		delete [] m_pString;
}

/*
	Match()

	Ricerca usando skeleton/pattern a mezzo tra unix e msdos.

	Chiama il codice originale (Florian Schintke) per verificare se la stringa fa match con il pattern specificato 
	(wildcards permesse/opzionali).

	IMPORTANTE: rispettare l'ordine dei parametri (1) pattern, 2) stringa per il test), se si invertono, il match,
	anche se esistente, non viene rilevato.

	Se il check viene effettuato cercando una sottostringa a secca dentro un altra stringa, fallisce. Ad es. la 
	stringa esatta 'Debug' non produce concordanza (match) con la stringa 'C:\TMP\Debug\File.obj' per ovviare, vedi 
	MatchSubString() piu' sotto.

	Se invece il check viene effettuato cercando una sottostringa con caratteri jolly dentro un altra stringa, allora 
	'*Debug*' si produce una concordanza con la stringa 'C:\TMP\Debug\File.obj'.

	In generale, per matchare 'paintlib' dentro la stringa 'c:\dev\paintlib*.obj', bisogna usare '*paintlib*'.
	esempi alernativi:
	- per matchare 'paintlib' solo all'inizio della stringa: 'paintlib*' (matcha 'paintlib*.obj', ma non fa match
	  con 'c:\dev\paintlib*.obj')
	- per matchare 'paintlib' solo alla fine: '*paintlib' (matcha 'c:\dev\paintlib', ma non con '*.obj' dopo)
	- per matchare con variazioni (usando set): '*paint[lL]ib*' (matcha 'paintlib' o 'paintLib', case-insensitive 
	  per quella lettera)
	- per escludere qualcosa: '*paintlib*[!x]*' (matcha se dopo 'paintlib' c'e' qualcosa che non e' 'x')

	Tenere presente che:
	- il matching e' case-sensitive (es. 'Paintlib' non matcha 'paintlib' a meno che non si usi un set come 
	  [Pp]aint[lL]ib)
	- non gestisce escape per caratteri speciali nei set (es. se 'paintlib' contiene '[', va gestito manualmente)
	- se la stringa ha wildcard letterali (come '*.obj'), questi vengono trattati come caratteri normali, non 
	  interpretati (quindi '*' in '*.obj' e' solo un asterisco, non un wildcard)
*/
BOOL CWildCards::Match(const char* skeleton,const char* string)
{
	return(match(0,skeleton,string));
}

/*
	MatchSubString()

	Chiama il codice aggiunto per verificare se la stringa contiene parte o tutta la sottostringa (skeleton).
	IMPORTANTE: l'ordine dei parametri: 1) skeleton (wildcards NON ammesse), 2) stringa per il test, se si 
	invertono, il match, anche se esistente, non viene rilevato.
	Ricerca usando strstr(), quindi non si possono usare wildcards nello skeleton di ricerca, in tal caso
	usare la funzione Match(), vedi sopra.
*/
BOOL CWildCards::MatchSubString(const char* substring,const char* string)
{
	return(match(1,substring,string));
}

/*
	match()

	Verifica se la stringa fa match con lo skeleton/pattern, discriminando il tipo di ricerca (Match() con 
	wildcards e MatchSubString() con strstr()) a seconda del flag (0/1 il primo parametro).
*/
BOOL CWildCards::match(int searchtype,const char* wildcard,const char* string)
{
	ASSERTEXPR(searchtype >= 0 && searchtype <= 1);
	ASSERTEXPR(wildcard && string);
	if((searchtype < 0 || searchtype > 1) || !wildcard || !string)
		return(FALSE);

	BOOL bMatch = FALSE;

	if(m_bIgnoreSpaces)
	{
		int nLen = 0;
		char* pWildcard = NULL;
		char* pTest = NULL;
		
		if(m_bIgnoreCase)
		{
			nLen = strlen(wildcard)+1;
			pWildcard = new char[nLen];
			if(pWildcard)
			{
				strcpyn(pWildcard,wildcard,nLen);
				strurp(pWildcard);
			}
			nLen = strlen(string)+1;
			pTest = new char[nLen];
			if(pTest)
			{
				strcpyn(pTest,string,nLen);
				strurp(pTest);
			}
		}
		else
		{
			pWildcard = (char*)wildcard;
			pTest = (char*)string;
		}

		if(searchtype==0)
			bMatch = (BOOL)match_floriansk(pWildcard,pTest);
		else if(searchtype==1)
			bMatch = match_substring(pWildcard,pTest);

		if(nLen > 0)
		{
			if(pWildcard)
				delete [] pWildcard,pWildcard = NULL;
			if(pTest)
				delete [] pTest,pTest = NULL;
		}
	}
	else
	{		
		m_listWildcards.DeleteAll();

		int nLen = strlen(wildcard);
		nLen = nLen <= 0 ? 1 : nLen;
		char* pString = new char[nLen+1];

		int nTestLen = 0;
		char* pTest;

		if(m_bIgnoreCase)
		{
			nTestLen = strlen(string)+1;
			pTest = new char[nTestLen];
			if(pTest)
			{
				strcpyn(pTest,string,nTestLen);
				strurp(pTest);
			}
			else
				nTestLen = 0;
		}
		else
			pTest = (char*)string;

		if(pString && pTest)
		{
			CWildCardItem* pWildCard;
			ITERATOR iter;
			
			memset(pString,'\0',nLen+1);

			int i,n;
			for(i=0,n=0; wildcard[i]; i++)
			{
				if(wildcard[i]!=' ')
					pString[n++] = wildcard[i];
				else
				{
					pString[n] = '\0';
					n = 0;
					pWildCard = new CWildCardItem(pString);
					if(pWildCard)
						m_listWildcards.Add(pWildCard);
					memset(pString,'\0',nLen+1);
				}
			}

			if(n > 0 && pString[0]!='\0')
			{
				pWildCard = new CWildCardItem(pString);
				if(pWildCard)
					m_listWildcards.Add(pWildCard);
				memset(pString,'\0',nLen+1);
			}

			if((iter = m_listWildcards.First())!=(ITERATOR)NULL)
			{
				do
				{
					pWildCard = (CWildCardItem*)iter->data;
					if(pWildCard)
					{
						strcpyn(pString,pWildCard->GetString(),nLen+1);
						if(m_bIgnoreCase)
							strurp(pString);
						if(searchtype==0)
						{
							if(match_floriansk(pString,pTest))
							{
								bMatch = TRUE;
								break;
							}
						}
						else if(searchtype==1)
						{
							if(match_substring(pString,pTest))
							{
								bMatch = TRUE;
								break;
							}
						}
					}

					iter = m_listWildcards.Next(iter);
				
				} while(iter!=(ITERATOR)NULL);
			}
			
			delete [] pString,pString = NULL;
		}

		if(nTestLen > 0 && pTest)
			delete [] pTest,pTest = NULL;

		m_listWildcards.DeleteAll();
	}

	return(bMatch);
}

/*
	match_floriansk()
	
	0 if wildcard does not match test
	1 if wildcard matches test
*/
int CWildCards::match_floriansk(const char* wildcard,const char* string)
{
	if(strcmp(wildcard,string)==0)
		return(1);

	int fit = 1;
  
	for (; ('\000' != *wildcard) && (1 == fit) && ('\000' != *string); wildcard++) {
      switch (*wildcard)
        {
        case '[':
	  wildcard++; /* leave out the opening square bracket */ 
          fit = set (&wildcard, &string);
	  /* we don't need to decrement the wildcard as in case */
	  /* of asterisk because the closing ] is still there */
          break;
        case '?':
          string++;
          break;
        case '*':
          fit = asterisk (&wildcard, &string);
	  /* the asterisk was skipped by asterisk() but the loop will */
	  /* increment by itself. So we have to decrement */
	  wildcard--;
          break;
        default:
          fit = (int) (*wildcard == *string);
          string++;
        }
    }
  while ((*wildcard == '*') && (1 == fit)) 
    /* here the teststring is empty otherwise you cannot */
    /* leave the previous loop */ 
    wildcard++;
  return (int) ((1 == fit) && ('\0' == *string) && ('\0' == *wildcard));
}

/*
	MatchSubString()

	Per ovviare al problema di Match() di cui sopra.
*/
BOOL CWildCards::match_substring(const char* substring,const char* string)
{
	BOOL bMatch = FALSE;

	if(m_bIgnoreCase)
		bMatch = stristr(string,substring)!=NULL;
	else
		bMatch = strstr(string,substring)!=NULL;

	return(bMatch);
}

/*
	set()
	
	scans a set of characters and returns 0 if the set mismatches at this
	position in the teststring and 1 if it is matching wildcard is set to
	the closing ] and test is unmodified if mismatched and otherwise the
	char pointer is pointing to the next character
*/
int CWildCards::set(const char** wildcard,const char** string)
{
  int fit = 0;
  int negation = 0;
  int at_beginning = 1;

  if ('!' == **wildcard)
    {
      negation = 1;
      (*wildcard)++;
    }
  while ((']' != **wildcard) || (1 == at_beginning))
    {
      if (0 == fit)
        {
          if (('-' == **wildcard) 
              && ((*(*wildcard - 1)) < (*(*wildcard + 1)))
              && (']' != *(*wildcard + 1))
	      && (0 == at_beginning))
            {
              if (((**string) >= (*(*wildcard - 1)))
                  && ((**string) <= (*(*wildcard + 1))))
                {
                  fit = 1;
                  (*wildcard)++;
                }
            }
          else if ((**wildcard) == (**string))
            {
              fit = 1;
            }
        }
      (*wildcard)++;
      at_beginning = 0;
    }
  if (1 == negation)
    /* change from zero to one and vice versa */
    fit = 1 - fit;
  if (1 == fit) 
    (*string)++;

  return (fit);
}

/*
	asterisk()
	
	scans an asterisk
*/
int CWildCards::asterisk(const char** wildcard,const char** string)
{
  /* Warning: uses multiple returns */

  int fit = 1;
  char *oldwildcard, *oldtest;

  /* erase the leading asterisk */
  (*wildcard)++; 
  while (('\000' != (**string))
	 && (('?' == **wildcard) 
	     || ('*' == **wildcard)))
    {
      if ('?' == **wildcard) 
	(*string)++;
      (*wildcard)++;
    }
  /* Now it could be that test is empty and wildcard contains */
  /* aterisks. Then we delete them to get a proper state */
  while ('*' == (**wildcard))
    (*wildcard)++;

  if (('\0' == (**string)) && ('\0' != (**wildcard)))
    return (fit = 0);
  if (('\0' == (**string)) && ('\0' == (**wildcard)))
    return (fit = 1); 
  else
    {
      /* Neither test nor wildcard are empty!          */
      /* the first character of wildcard isn't in [*?] */
      oldwildcard = (char*)*wildcard;
      oldtest = (char*)*string;
      do 
	{
	  if (0 == match_floriansk(*wildcard, (*string)))
	    oldtest++;
	  *wildcard = oldwildcard;
	  *string = oldtest;
	  /* skip as much characters as possible in the teststring */
	  /* stop if a character match occurs */
	  while (((**wildcard) != (**string)) 
		 && ('['  != (**wildcard))
		 && ('\0' != (**string)))
	    (*string)++;
	  oldwildcard = (char*)*wildcard;
	  oldtest = (char*)*string;
	}
      while ((('\0' != **string))? 
	     (0 == match_floriansk (*wildcard, (*string))) 
	     : (0 != (fit = 0)));
      if (('\0' == **string) && ('\0' == **wildcard))
	fit = 1;
      return (fit);
    }
}

/*
	SplitPattern()
	
	Crea una lista contenente i pattern presenti in una stringa, i pattern vanno separati con ';'.
	La creazione/allocazione/rilascio della lista viene gestito internamente, il chiamante puo' usare
	il membro GetItemList() per ricavare il puntatore alla lista.
	Notare che, oltre ad inserire l'elemento estratto dalla stringa, imposta il flag indicante se 
	l'elemento contiene o meno wildcards (?/*), in tal modo si potra' poi selezionare la funzione di 
	ricerca appropiata (Match() o Matchsubstring()) senza dover verificare ogni volta la stringa.
*/
CItemList* CWildCards::SplitPattern(const char* patternStr)
{
	ASSERTEXPR(patternStr);
	if(!patternStr)
		return(NULL);

	// crea la lista per le stringhe estratte dallo skeleton o la riutilizza azzerandola previamente
	if(!m_pInternalItemList)
		m_pInternalItemList = new CItemList();
	else
		m_pInternalItemList->EraseAll();

	ITEM* pItem = NULL;
	char current[_MAX_ITEM+1] = {0};
	int i = 0;

	while(*patternStr)
	{
		if(*patternStr==';')
		{
			if(strlen(current) > 0)
			{
				pItem = (ITEM*)m_pInternalItemList->Create();
				memset(pItem,'\0',sizeof(ITEM));
				/*
				Calcola la dimensione di un membro di una struct senza dover creare un'istanza reale della struct: casta 0 (un 
				puntatore nullo) al tipo di puntatore della struct (struct ITEM*) e poi accede al membro item come se fosse un 
				puntatore.
				Il compilatore calcola la dimensione di item in base alla definizione del tipo, non in base al valore del puntatore 
				nullo: sizeof(((struct ITEM*)0)->item).
				*/
				strcpyn(pItem->item,current,sizeof(((struct ITEM*)0)->item));
				if(strchr(pItem->item,'?') || strchr(pItem->item,'*'))
					pItem->flag = 1;
				else
					pItem->flag = 0;
				m_pInternalItemList->Add(pItem);
				memset(current,'\0',sizeof(current));
				i=0;
			}
		}
		else
		{
			current[i++] = *patternStr;
		}
        
		patternStr++;
	}
    
	if(strlen(current) > 0)
	{
		pItem = (ITEM*)m_pInternalItemList->Create();
		memset(pItem,'\0',sizeof(ITEM));
		strcpyn(pItem->item,current,sizeof(((struct ITEM*)0)->item)-1);
		if(strchr(pItem->item,'?') || strchr(pItem->item,'*'))
			pItem->flag = 1;
		else
			pItem->flag = 0;
		m_pInternalItemList->Add(pItem);
	}

	return(m_pInternalItemList);
}

/*
testwildcards.main
==================
#! /bin/sh

# The simplest input
./testwildcards "" "" "t"

# Get a simple error
./testwildcards "" "a" "f"

# Single character match
./testwildcards "a" "a" "t"

# Single character mismatch
./testwildcards "a" "b" "f"

# Simple question mark test
./testwildcards "?" "b" "t"

# Doubled question mark test
./testwildcards "??" "bc" "t"

# question mark followed by a character
# matches
./testwildcards "?c" "bc" "t"

# question mark follows a character
# matches
./testwildcards "b?" "bc" "t"

# Simple set test that matches
./testwildcards "[a-z]" "b" "t"

# Simple set test that mismatches
./testwildcards "[A-Z]" "b" "f"

# Simple asterisk test that matches
./testwildcards "*" "a" "t"

# Simple asterisk test that matches
./testwildcards "**" "a" "t"

# asterisk that finishes the wildcard while the 
# remaining teststring is empty
./testwildcards "*" "" "t"

# several asterisks 
# matches
./testwildcards "*bc*hij" "abcdfghij" "t"

# several asterisks 
# mismatches
./testwildcards "*b*a*" "b" "f"

# several asterisks 
# mismatches
./testwildcards "*bc*hik" "abcdfghij" "f"

# asterisk that follows a string that would match the teststring 
# without the asterisk too.
# matches
./testwildcards "abc*" "abc" "t"

# doubled asterisk that follows a string that would match the 
# teststring without the asterisks.
# matches
./testwildcards "abc**" "abc" "t"

# Simple negated set test that matches
# The following set would be a syntax error: [!] because you don't need
# to quote the ! in a single character set, we can assume that after 
# an ! in the first position an element of the set comes. In this case
# the ]. The program would search for the ] that closes the set now.
# If you want to have an exclamation mark in a set do not write it in
# the beginning.
./testwildcards "[!]]" "!" "t"
./testwildcards "[!]]" "]" "f"

# Simple negated set test that matches
./testwildcards "[!abc]" "d" "t"

# Simple negated set test that mismatches
./testwildcards "[!abc]" "b" "f"

# asterisk with following question marks that matches
./testwildcards "*???" "abc" "t"

# asterisk with following question marks that mismatches
./testwildcards "*???" "ab" "f"

# asterisk with following question marks that mismatches
./testwildcards "*???" "abcd" "t"

# asterisk with following question mark and asterisk that mismatches
./testwildcards "*?*" "abcd" "t"

# asterisk with following characters that matches 
./testwildcards "*bc" "abc" "t"

# asterisk with following characters that mismatches 
./testwildcards "*cc" "abc" "f"

# asterisk that finishes the wildcard that does not match
./testwildcards "[a-c]*" "d" "f"

# asterisk followed by a set that matches
./testwildcards "*[a-e]" "d" "t"

# asterisk followed by a symbol that does not match followed by a 
# second asterisk 
# mismatches
./testwildcards "*a*" "de" "f"

# asterisk followed by a set that mismatches
./testwildcards "*[a-c]" "d" "f"

# range that mismatches because
# the character in the teststring is too big 
./testwildcards "[a-c]" "d" "f"

# range that mismatches because
# the character in the teststring is to too small 
./testwildcards "[b-d]" "a" "f"

# set with a ] in it as the first element that matches 
./testwildcards "[]abc]" "b" "t"

# set with a ] in it as the first element that mismatches 
./testwildcards "[]abc]" "d" "f"

# set with a pseudo range in it that isn't one because the
# first character in the range is greater than the last one (of the range) 
# matching
./testwildcards "[z-a]" "-" "t"

# set with a pseudo range in it that isn't one because the
# first character in the range is greater than the last one (of the range) 
# mismatching
./testwildcards "[z-a]" "b" "f"

# set with a - that could be a range but after that the closing ]
# matching
./testwildcards "[A-]" "-" "t"

# set with a - that could be a range but after that the closing ]
# mismatching
./testwildcards "[A-]" "]" "f"

# set with a - as the first element that matches
# Note: theoretically [-a could be a range from [ to a
./testwildcards "[-a]" "-" "t"

# set with a - as the first element that matches
# Note: theoretically [-[ could not be a range from [ to [
#       because the beginning [ doesn't have a smaller code than the [
./testwildcards "[-[]" "-" "t"

# set with a - as the first element that matches
./testwildcards "[-]" "-" "t"

# negated set with a - as the first element that matches
./testwildcards "[!-b]" "a" "t"

# negated set with a - as the first element that matches
./testwildcards "[!-b]" "-" "f"

# set with a - as the first element that mismatches
./testwildcards "[-b]" "a" "f"

# set followed by normal characters
# matches
./testwildcards "[a-g]lorian" "florian" "t"

# set followed by normal characters, but not there in the teststring
# mismatches
./testwildcards "[a-g]*rorian" "f" "f"

# Jenny Sowden <SowdenJ@deluxe-data.co.uk> found a bug in 
# version 1.0 of the routine while processing this pattern. 
# Thank you. (fixed since 10.11.1998 in version 1.1 of wildcards.c)
./testwildcards "*???*" "123" "t"

testwildcards.report
====================
(tt) ['', '']
(ff) ['', 'a']
(tt) ['a', 'a']
(ff) ['a', 'b']
(tt) ['?', 'b']
(tt) ['??', 'bc']
(tt) ['?c', 'bc']
(tt) ['b?', 'bc']
(tt) ['[a-z]', 'b']
(ff) ['[A-Z]', 'b']
(tt) ['*', 'a']
(tt) ['**', 'a']
(tt) ['*', '']
(tt) ['*bc*hij', 'abcdfghij']
(ff) ['*b*a*', 'b']
(ff) ['*bc*hik', 'abcdfghij']
(tt) ['abc*', 'abc']
(tt) ['abc**', 'abc']
(tt) ['[!]]', '!']
(ff) ['[!]]', ']']
(tt) ['[!abc]', 'd']
(ff) ['[!abc]', 'b']
(tt) ['*???', 'abc']
(ff) ['*???', 'ab']
(tt) ['*???', 'abcd']
(tt) ['*?*', 'abcd']
(tt) ['*bc', 'abc']
(ff) ['*cc', 'abc']
(ff) ['[a-c]*', 'd']
(tt) ['*[a-e]', 'd']
(ff) ['*a*', 'de']
(ff) ['*[a-c]', 'd']
(ff) ['[a-c]', 'd']
(ff) ['[b-d]', 'a']
(tt) ['[]abc]', 'b']
(ff) ['[]abc]', 'd']
(tt) ['[z-a]', '-']
(ff) ['[z-a]', 'b']
(tt) ['[A-]', '-']
(ff) ['[A-]', ']']
(tt) ['[-a]', '-']
(tt) ['[-[]', '-']
(tt) ['[-]', '-']
(tt) ['[!-b]', 'a']
(ff) ['[!-b]', '-']
(ff) ['[-b]', 'a']
(tt) ['[a-g]lorian', 'florian']
(ff) ['[a-g]*rorian', 'f']
(tt) ['*???*', '123']
*/
