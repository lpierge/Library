/*$
	algorithm.c
	Algoritmi vari.
	Luca Piergentili, Luglio '25
*/
#include "pragma.h"
#include "env.h"
#include "macro.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <ctype.h>
#include <limits.h>
#include <time.h>
#include "fastrand.h"
#include "algorithm.h"

#include "traceexpr.h"
//#define _TRACE_FLAG			_TRFLAG_TRACEOUTPUT // opzioni: _TRFLAG_NOTRACE, _TRFLAG_TRACEFILE, _TRFLAG_TRACECONSOLE, _TRFLAG_TRACEOUTPUT, _TRFLAG_TRACEBREAKPOINT
#define _TRACE_FLAG			_TRFLAG_NOTRACE // opzioni: _TRFLAG_NOTRACE, _TRFLAG_TRACEFILE, _TRFLAG_TRACECONSOLE, _TRFLAG_TRACEOUTPUT, _TRFLAG_TRACEBREAKPOINT
#define _TRACE_FLAG_INFO	_TRACE_FLAG
#define _TRACE_FLAG_WARN	_TRACE_FLAG
#define _TRACE_FLAG_ERR		_TRACE_FLAG

/* prototipi interne */
static void count_digits(const char* str_sequence,int counts[10]);

/*
	swing()

	Oscilla un valore tra min e max.

	Riceve in input:
		puntatore al valore da modificare
		valore minimo consentito (escluso)
		valore massimo consentito (escluso)
		quantità di decremento da applicare
		quantità di incremento da applicare
		puntatore allo stato (true = decremento, false = incremento)

	Restituisce true se il valore e' stato modificato, false se gia' fuori range.
 */
bool swing(int *value,int min,int max,int dec,int inc,bool *status)
{
	/* controlla se entro i limiti */
	if(*value <= min || *value >= max)
		return(false);
    
	/* decremento */
	if(*status)
	{
		if(*value - dec > min)
		{
			*value -= dec;
		}
		else /* passa a incremento */
		{
			*status = false;
			*value += inc;
		}
	} 
	/* incremento */
	else
	{
		if(*value + inc < max)
		{
			*value += inc;
		}
		else /* passa a decremento */
		{
			*status = true;
			*value -= dec;
		}
	}
    
	return(true);
}

/*
	get_weighted_boolean()

	Restituisce un booleano a TRUE in base alle probabilita' specificata con il parametro di input che 
	puo' andare da 0% a 100%. Ad es., per far restituire TRUE con un 10% di probabilita, passare 10.
*/
bool get_weighted_boolean(int nThreshold)
{
	/* se la soglia e' 0, e' sempre falso; se e' 100, e' sempre vero */
	if(nThreshold <= 0)
		return(false);
	if(nThreshold >= 100)
		return(true);

	/* genera 100 valori possibili */
	int nRandomNumber = rand_w(1,100);

	/* se nThreshold e' 10, i numeri da 1 a 10 (esattamente 10 numeri) restituiranno TRUE -> 10/100 = 10% esatto */
	return(nRandomNumber <= nThreshold);
}

/*
	multiset_equality_check()

	Verifica se due set contengono gli stessi elementi con la stessa frequenza (NON con lo stesso 
	ordine):

	"When you check for a sequence of digits or characters like '0143' within another string like 
	'4031', where the order of the characters doesn't matter, but their presence and frequency do, 
	you're performing a 'multiset equality check' (also known as anagram check)."

	Dato che usa combinazioni numeriche per comporre le stringhe di validazione, ovviamente una
	stringa di validazione non potra' avere piu' di 10 caratteri (da 0 a 9).

	Esempio uso pratico:
	Invece di controllare la compatibilita' incrociata delle varie opzioni dalla linea di comando,
	definisce le sequenze di opzioni valide e le confronta con quanto riceve in input.

	In input:
	- l'array di stringhe (char*) che contiene le serie casuali
	- la dimensione dell'array
	- la serie casuale di numeri sciolta da cercare (passata come stringa)
*/
bool multiset_equality_check(const char* array_of_sequences[],int array_size,const char* target_sequence)
{
	int target_len;

	/* lunghezza della serie da cercare */
	if((target_len = (int)strlen(target_sequence))==0)
		return(false);
    
	int target_counts[10] = {0}; /* contatori per la serie da cercare */
	count_digits(target_sequence,target_counts); /* popola i contatori per la serie target */

	/* itera attraverso ogni elemento dell'array di stringhe */
	for(int i = 0; i < array_size; i++)
	{
		const char* current_array_sequence_str = array_of_sequences[i];	/* ottiene la stringa dall'array */
		int current_len = (int)strlen(current_array_sequence_str);

		/* se le lunghezze non coincidono, per definizione non e' un match */
		if(current_len!=target_len)
			continue;

		/* confronta le occorrenze delle cifre */
		int current_counts[10]; /* contatori per l'elemento corrente dell'array */
		count_digits(current_array_sequence_str, current_counts); /* popola i contatori */

		/* confronta i contatori: devono essere identici per un match */
		bool match = true;
		for(int j = 0; j < 10; j++)
		{
			if(current_counts[j]!=target_counts[j])
			{
				match = false; /* trovata una discrepanza nei conteggi delle cifre */
				break;
			}
		}

		if(match)
			return(true);
	}

	return(false);
}

/*
	count_digits()
	
	Conta le occorrenze delle cifre in una stringa.
	Riempie l'array counts con il numero di volte che ogni cifra (0-9) appare nella stringa.
*/
void count_digits(const char* str_sequence,int counts[10])
{
	for(int i = 0; i < 10; i++)
		counts[i] = 0;

	/* itera attraverso la stringa e conta le cifre */
	for(int i = 0; str_sequence[i]!='\0'; i++)
	{
		char digit_char = str_sequence[i];

		/* converte il carattere cifra nel suo valore numerico (es. '3' - '0' = 3) */
		if(digit_char >= '0' && digit_char <= '9')
			counts[digit_char - '0']++;
	}
}

/*
	linear_map()

	Mappatura lineare universale.
	La logica della funzione non si limita a 0-100. Se dovesse esserci un input diverso (es. -50 a +50), 
	basterebbe normalizzare l'input sottraendo il suo minimo: normalized = (input - inMin) / (inMax - inMin).
*/
int linear_map(int inputVal,int outMin,int outMax)
{
	/* trasforma l'input (0-100) in una frazione tra 0.0 e 1.0 */
	double normalized = (double)inputVal / 100.0;

	/* calcola l'ampiezza del range di destinazione (puo' essere negativo) */
	double span = (double)outMax - (double)outMin;

	/* applica la frazione allo span e trasla il punto di partenza */
	double result = (double)outMin + (normalized * span);

	return((int)result);
}

/*
	hash_string_FNV1a()

	Calcola l'hash FNV-1a a 64 bit di una stringa.

	Usa la variante a 64 bit perche' con 64 bit lo spazio di indirizzamento e' cosi' vasto (2^64) che la probabilita'
	di collisione e' praticamente zero.

	- XOR-Multiply: alterna un'operazione di XOR con una moltiplicazione per un numero primo specifico, il che fa si' 
	  che ogni singolo bit della stringa di input influenzi drasticamente il risultato finale
	- Non ha limiti di buffer: processa un carattere alla volta senza richiedere memoria aggiuntiva.
	- Rispetto a un hash a 32 bit (che ha circa 4 miliardi di combinazioni), un hash a 64 bit e' smisurato.
*/
uint64_t hash_string_FNV1a(const char* str)
{
	/* numeri "magici" per FNV-1a 64-bit */
	const uint64_t FNV_OFFSET_BASIS = 0xcbf29ce484222325ULL;
	const uint64_t FNV_PRIME = 0x100000001b3ULL;
	uint64_t hash = FNV_OFFSET_BASIS;

	if(!str)
		return(0);

	/* processa la stringa byte per byte */
	while(*str)
	{
		hash ^= (uint64_t)(unsigned char)(*str++);
		hash *= FNV_PRIME;
	}

	return(hash);
}

/*
	hash_normalized_string_FNV1a()

	Calcola l'hash FNV-1a a 64 bit di una stringa dopo averla normalizzata.

	Come sopra, ma con normalizzazione, per cui NON va usata se l'hash deve essere calcolato esattamente sulla stringa di 
	input.
	Da usare nei casi in cui le stringhe possono variare per elementi secondari (come spazi, punteggiatura, maiuscolo o 
	minuscolo, etc.), esempio tipico nel caso di titoli.
*/
uint64_t hash_normalized_string_FNV1a(const char* str)
{
	/* numeri "magici" per FNV-1a 64-bit */
	const uint64_t FNV_OFFSET_BASIS = 0xcbf29ce484222325ULL;
	const uint64_t FNV_PRIME = 0x100000001b3ULL;
	uint64_t hash = FNV_OFFSET_BASIS;

	if(!str)
		return(0);

	/* processa la stringa byte per byte */
	while(*str)
	{
		/* normalizza al volo: converte il carattere a minuscolo */
		unsigned char c = (unsigned char)tolower((unsigned char)*str++);
        
		/* ignora spazi bianchi consecutivi e punteggiatura */
		if(isspace(c) || ispunct(c))
			continue; 

		hash ^= (uint64_t)c;
		hash *= FNV_PRIME;
	}

	return(hash);
}
