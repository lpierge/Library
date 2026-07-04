/*$
	fastrand.c
	Randomizzazione veloce.
	Luca Piergentili, Luglio '25
*/
#include "pragma.h"
#include "env.h"
#include "macro.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <limits.h>
#include <intrin.h>  /* necessario per __rdtsc() (MSVC) */
#include "datetime.h"
#include "fastrand.h"

#include "traceexpr.h"
//#define _TRACE_FLAG			_TRFLAG_TRACEOUTPUT // opzioni: _TRFLAG_NOTRACE, _TRFLAG_TRACEFILE, _TRFLAG_TRACECONSOLE, _TRFLAG_TRACEOUTPUT, _TRFLAG_TRACEBREAKPOINT
#define _TRACE_FLAG			_TRFLAG_NOTRACE // opzioni: _TRFLAG_NOTRACE, _TRFLAG_TRACEFILE, _TRFLAG_TRACECONSOLE, _TRFLAG_TRACEOUTPUT, _TRFLAG_TRACEBREAKPOINT
#define _TRACE_FLAG_INFO	_TRACE_FLAG
#define _TRACE_FLAG_WARN	_TRACE_FLAG
#define _TRACE_FLAG_ERR		_TRACE_FLAG

/*
	rand_w()

	Calcola e restituisce un numero random compreso (inclusivamente) nell'intervallo specificato.

	Non limita a RAND_MAX i valori per gli int come fanno invece come rand() e rand_m(), per cui
	possono essere passati e restituiti valori fino a INT_MAX.
*/
int rand_w(int start,int end)
{
	/* se riceve i numeri AC/DC (ad es. invertiti) */
	if(start > end) 
	{
		int temp = start;
		start = end;
		end = temp;
	}

	return((int)fast_rand_range_between(start,end));
}

/*
	Xorshift
	vedi le note in fastrand.h
*/

/*
per inizializzazione

il cast da long long a unsigned int calcola il modulo
notare che UINT_MAX tipicamente vale 4294967295 (2[32] - 1), quindi modulo 4294967296 (2[32])
se il long long e' 5.000.000.000 -> 5.000.000.000 % 4.294.967.296 (UINT_MAX+1) = 705.032.704

in C il compilatore ha bisogno di sapere il valore esatto delle variabili globali o statiche nel
momento in cui compila il codice perche' le inizializza in fase di compilazione, non in fase di 
esecuzione, come invece fa il C++, quindi compilando come codice C, si produce l'errore C2099 
("l'inizializzatore non e' una costante") dato che __rdtsc() e' una funzione intrinseca che legge 
il Time Stamp Counter della CPU a runtime (NON e' una costante)

questo significa che se si compila come linguaggio C, bisogna chiamare la funzione init_xorshift32()
per inizializzare il seme, mentre se si compila come C++ non e' necessario
*/
#ifdef __cplusplus
	unsigned int xorshift32_state = (unsigned int)__rdtsc();
#else
	unsigned int xorshift32_state = (unsigned int)196508263005;
#endif

/*
	init_xorshift32()

	Inizializza il seme per il generatore.

	La chiamata e'/non e' necessaria a seconda se si compila per C o per C++, vedi le note sopra.
*/
void init_xorshift32(void)
{
	static bool seeded = false;
	if(!seeded)
	{
		/* M$: "__rdtsc is used to read the processor's time-stamp counter, which counts the number of clock cycles since the last reset" */
		xorshift32_state = (unsigned int)__rdtsc();

		/* altamente improbabile ma per sicurezza, dato che con 0 il generatore rimarrebbe bloccato per sempre */
		if(xorshift32_state==0)
			xorshift32_state = (unsigned int)unix_timestamp();

		seeded = true;
	}
}
