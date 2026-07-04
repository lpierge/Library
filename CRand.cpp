/*$
	CRand.cpp
	La classe CRand e' un wrapper per il codice originale di Takuji Nishimura e Makoto Matsumoto (vedi sotto).
	La classi CRandom (generazione con servizio crittografico) e CCardsDeck (generazione con distribuzione
	uniforme), entrambe originali, sono posteriori.
	Luca Piergentili, Agosto '03

	Vedi le note in CRand.h
*/

/* 
   A C-program for MT19937, with initialization improved 2002/1/26.
   Coded by Takuji Nishimura and Makoto Matsumoto.

   Before using, initialize the state by using init_genrand(seed)  
   or init_by_array(init_key, key_length).

   Copyright (C) 1997 - 2002, Makoto Matsumoto and Takuji Nishimura,
   All rights reserved.                          

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

     1. Redistributions of source code must retain the above copyright
        notice, this list of conditions and the following disclaimer.

     2. Redistributions in binary form must reproduce the above copyright
        notice, this list of conditions and the following disclaimer in the
        documentation and/or other materials provided with the distribution.

     3. The names of its contributors may not be used to endorse or promote 
        products derived from this software without specific prior written 
        permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
   CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
   PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
   LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


   Any feedback is very welcome.
   http://www.math.keio.ac.jp/matumoto/emt.html
   email: matumoto@math.keio.ac.jp
*/
#include "env.h"
#include "pragma.h"
#include "macro.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "window.h"
#include "algorithm.h"
#include "fastrand.h"
#include "CRand.h"

#include "traceexpr.h"
//#define _TRACE_FLAG			_TRFLAG_TRACEOUTPUT // opzioni: _TRFLAG_NOTRACE, _TRFLAG_TRACEFILE, _TRFLAG_TRACECONSOLE, _TRFLAG_TRACEOUTPUT, _TRFLAG_TRACEBREAKPOINT
#define _TRACE_FLAG			_TRFLAG_NOTRACE // opzioni: _TRFLAG_NOTRACE, _TRFLAG_TRACEFILE, _TRFLAG_TRACECONSOLE, _TRFLAG_TRACEOUTPUT, _TRFLAG_TRACEBREAKPOINT
#define _TRACE_FLAG_INFO	_TRACE_FLAG
#define _TRACE_FLAG_WARN	_TRACE_FLAG
#define _TRACE_FLAG_ERR		_TRACE_FLAG

/* Period parameters */  
#define MATRIX_A 0x9908b0dfUL   /* constant vector a */
#define UPPER_MASK 0x80000000UL /* most significant w-r bits */
#define LOWER_MASK 0x7fffffffUL /* least significant r bits */

/* CODICE MADE IN JAPAN */

/*
	CRand()
*/
CRand::CRand(unsigned long s/* = (unsigned long)-1L*/)
{
	m_nPrevious = 0L;
	mti = N_SEED+1; /* mti==N_SEED+1 means mt[N_SEED] is not initialized */

	if(s!=(unsigned long)-1L)
		init_genrand(s);
}

/*
	init_genrand()

	Initializes mt[N_SEED] with a seed.
*/
void CRand::init_genrand(unsigned long s)
{
    mt[0]= s & 0xffffffffUL;
    for (mti=1; mti<N_SEED; mti++) {
        mt[mti] = 
	    (1812433253UL * (mt[mti-1] ^ (mt[mti-1] >> 30)) + mti); 
        /* See Knuth TAOCP Vol2. 3rd Ed. P.106 for multiplier. */
        /* In the previous versions, MSBs of the seed affect   */
        /* only MSBs of the array mt[].                        */
        /* 2002/01/09 modified by Makoto Matsumoto             */
        mt[mti] &= 0xffffffffUL;
        /* for >32 bit machines */
    }
}

/*
	init_by_array()

	Initialize by an array with array-length, init_key is the array for initializing keys, key_length is its length.
*/
void CRand::init_by_array(unsigned long init_key[],int key_length)
{
    int i, j, k;
    init_genrand(19650218UL);
    i=1; j=0;
    k = (N_SEED>key_length ? N_SEED : key_length);
    for (; k; k--) {
        mt[i] = (mt[i] ^ ((mt[i-1] ^ (mt[i-1] >> 30)) * 1664525UL))
          + init_key[j] + j; /* non linear */
        mt[i] &= 0xffffffffUL; /* for WORDSIZE > 32 machines */
        i++; j++;
        if (i>=N_SEED) { mt[0] = mt[N_SEED-1]; i=1; }
        if (j>=key_length) j=0;
    }
    for (k=N_SEED-1; k; k--) {
        mt[i] = (mt[i] ^ ((mt[i-1] ^ (mt[i-1] >> 30)) * 1566083941UL))
          - i; /* non linear */
        mt[i] &= 0xffffffffUL; /* for WORDSIZE > 32 machines */
        i++;
        if (i>=N_SEED) { mt[0] = mt[N_SEED-1]; i=1; }
    }

    mt[0] = 0x80000000UL; /* MSB is 1; assuring non-zero initial array */ 
}

/*
	genrand_int32()

	Generates a random number on [0,0xffffffff]-interval.
*/
unsigned long CRand::genrand_int32(void)
{
    unsigned long y;
    static unsigned long mag01[2]={0x0UL, MATRIX_A};
    /* mag01[x] = x * MATRIX_A  for x=0,1 */

    if (mti >= N_SEED) { /* generate N_SEED words at one time */
        int kk;

        if (mti == N_SEED+1)   /* if init_genrand() has not been called, */
            init_genrand(5489UL); /* a default initial seed is used */

        for (kk=0;kk<N_SEED-M_SEED;kk++) {
            y = (mt[kk]&UPPER_MASK)|(mt[kk+1]&LOWER_MASK);
            mt[kk] = mt[kk+M_SEED] ^ (y >> 1) ^ mag01[y & 0x1UL];
        }
        for (;kk<N_SEED-1;kk++) {
            y = (mt[kk]&UPPER_MASK)|(mt[kk+1]&LOWER_MASK);
            mt[kk] = mt[kk+(M_SEED-N_SEED)] ^ (y >> 1) ^ mag01[y & 0x1UL];
        }
        y = (mt[N_SEED-1]&UPPER_MASK)|(mt[0]&LOWER_MASK);
        mt[N_SEED-1] = mt[M_SEED-1] ^ (y >> 1) ^ mag01[y & 0x1UL];

        mti = 0;
    }
  
    y = mt[mti++];

    /* Tempering */
    y ^= (y >> 11);
    y ^= (y << 7) & 0x9d2c5680UL;
    y ^= (y << 15) & 0xefc60000UL;
    y ^= (y >> 18);

    return y;
}

/*
	genrand_int31()

	Generates a random number on [0,0x7fffffff]-interval.
*/
long CRand::genrand_int31(void)
{
    return (long)(genrand_int32()>>1);
}

/*
	genrand_real1()

	Generates a random number on [0,1]-real-interval.
*/
double CRand::genrand_real1(void)
{
    return genrand_int32()*(1.0/4294967295.0); 
    /* divided by 2^32-1 */ 
}

/*
	genrand_real2()

	Generates a random number on [0,1)-real-interval.
*/
double CRand::genrand_real2(void)
{
    return genrand_int32()*(1.0/4294967296.0); 
    /* divided by 2^32 */
}

/*
	genrand_real3()

	Generates a random number on (0,1)-real-interval.
*/
double CRand::genrand_real3(void)
{
    return (((double)genrand_int32()) + 0.5)*(1.0/4294967296.0); 
    /* divided by 2^32 */
}

/*
	genrand_res53()

	Generates a random number on [0,1) with 53-bit resolution.
*/
double CRand::genrand_res53(void) 
{ 
    unsigned long a=genrand_int32()>>5, b=genrand_int32()>>6; 
    return(a*67108864.0+b)*(1.0/9007199254740992.0); 
} 
/* These real versions are due to Isaku Wada, 2002/01/09 added */

/*
int main(void)
{
    int i;
    unsigned long init[4]={0x123, 0x234, 0x345, 0x456}, length=4;
    init_by_array(init, length);
    printf("1000 outputs of genrand_int32()\n");
    for (i=0; i<1000; i++) {
      printf("%10lu ", genrand_int32());
      if (i%5==4) printf("\n");
    }
    printf("\n1000 outputs of genrand_real2()\n");
    for (i=0; i<1000; i++) {
      printf("%10.8f ", genrand_real2());
      if (i%5==4) printf("\n");
    }
    return 0;
}
*/

/* CODICE PROPRIO (ORIGINALE) */

/*
	InitializeRandomGenerator()
*/
BOOL CRandom::InitializeRandomGenerator(void)
{
	BOOL bSeeded = FALSE;
	BYTE seedBytes[4] = {0}; /* sufficente per un unsigned int (il seme di srand) */

	/* aggancia il provider del servizio crittografico */
	if(CryptAcquireContextW(&m_hCryptProv,NULL,NULL,PROV_RSA_FULL,CRYPT_VERIFYCONTEXT))
	{
		/* per generare bytes aleatori usando il CSP */
		if(CryptGenRandom(m_hCryptProv,sizeof(seedBytes),seedBytes))
		{
			/* usa i bytes aleatori come seme per srand */
			unsigned int seed = *(unsigned int*)seedBytes;
			srand(seed);
			bSeeded = TRUE;
		}
	}

	/* se CryptGenRandom fallisce, usa la ora corrente come seme */
	if(!bSeeded)
		srand((unsigned int)time(NULL));
		
	return(TRUE);
}

/*
	CleanupRandomGenerator()
*/
BOOL CRandom::CleanupRandomGenerator(void)
{
	if(m_hCryptProv!=0)
	{
		CryptReleaseContext(m_hCryptProv,0);
		m_hCryptProv = 0;
	}
	return(TRUE);
}

/*
	GenerateRandomNumber()

	Genera un numero casuale nell'intervallo specificato.
	La differenza tra usare '/' e '%' nel calcolo risiede nella robustezza e nella qualita' della distribuzione 
	casuale per diverse dimensioni dell'intervallo:
	
	Per intervalli piccoli (minori o comparabili a RAND_MAX), rand() % range + min e' semplice e accettabilmente
	uniforme.

	Per intervalli grandi (specialmente con range di molto maggiore rispetto a RAND_MAX, come nel caso di ULONG), 
	la formula basata sulla divisione con virgola mobile ((double)rand() / (double)RAND_MAX * range + min) e' il 
	modo corretto per garantire una distribuzione casuale uniforme e prevenire il bias.
 */
ULONG CRandom::GenerateRandomNumber(ULONG min,ULONG max)
{
	if(!m_bInitialized)
		m_bInitialized = InitializeRandomGenerator();

	/* se vengono passati i parametri AC/DC */
	if(min > max)
		SWAP(min,max,ULONG);

	/*
	calcola la dimensione dell'intervallo (inclusivo), es. da 1 a 10 ci sono 10 numeri (10 - 1 + 1)
	usa ULONG per l'intervallo per gestire valori grandi
	*/
	ULONG range = max - min + 1;

	/* per evitare la divisione per 0 o intervallo errato (min == max) */
	if(range==0)		/* se min==max e molto grandi, range potrebbe essere 0 */
		return(min);	/* se l'intervallo non e' positivo */

	/*
	Con prototipo: int GenerateRandomNumber(int min,int max)
	Genera un numero aleatorio nell'intervallo [0, range-1] e somma min.
	Utilizza l'operatore modulo (%) con il risultato di rand() per limitare il numero all'intervallo desiderato e poi lo sposta 
	aggiungendo min:
		
	return rand() % range + min;
	
	Con prototipo: ULONG GenerateRandomNumber(ULONG min,ULONG max)
	Genera un numero casuale usando rand() nell'intervallo [0, RAND_MAX], quindi lo mappa nell'intervallo [0, range-1] e infine 
	lo sposta aggiungendo min. Utilizza un cast a (double) per evitare overflow nella moltiplicazione se RAND_MAX * range e' 
	molto grande.
	(double)rand() / (double)RAND_MAX produce un valore compreso tra 0.0 e 1.0, quindi lo moltiplica per range e aggiunge min per
	adattare il valore all'intervallo desiderato:
	*/
	return((ULONG)((double)rand() / (double)RAND_MAX * range + min));
}

/*
	CCardsDeck()
*/
CCardsDeck::CCardsDeck(unsigned long ulTot)
{
	if(ulTot < RAND_MAX)
	{
		int nTot = (int)ulTot;
		memset(&m_cardsDeck,'\0',sizeof(m_cardsDeck));
		m_nTot = 0;

		if(nTot < RAND_MAX)
		{
			m_nTot = nTot;

			/* alloca l'array per i valori (il mazzo di carte), il totale di elementi e' il range entro cui randomizzare */
			m_cardsDeck.pool = (int*)malloc(m_nTot * sizeof(int));
			m_cardsDeck.range = m_nTot;

			Initialize();
		}
	}
}

/*
	~CCardsDeck()
*/
CCardsDeck::~CCardsDeck()
{
	if(m_cardsDeck.pool)
		free(m_cardsDeck.pool);
}

/*
	Initialize()
*/
void CCardsDeck::Initialize(void)
{
	/* inizializza l'array (il mazzo di carte) con la progressione numerica, fino ad arrivare al totale (il range) */
	if(m_cardsDeck.pool)
	{
		for(int i=0; i < m_nTot; i++)
			m_cardsDeck.pool[i] = i + 1;
		m_cardsDeck.range = m_nTot;
	}
}

/*
	Shuffle()

	Randomizza (mescola il mazzo di carte) ed estrae il prossimo numero (carta) senza ripetizioni.

	Il primo parametro specifica quante carte deve avere il mazzo (si puo' variare tra una chiamata
	e l'altra durante l'esistenza dell'oggetto, vedi pero' sotto).
	Il secondo parametro viene usato per informare il chiamante su quante carte rimango nel mazzo
	(quando arriva a zero ricomincia il ciclo, vedi sotto).

	Quando termina di estrarre tutte le carte dal mazzo, o quando deve cambiarlo perche' chiamato
	con un valore diverso rispetto all'iniziale, il ciclo di estrazione viene fatto ripartire da zero, 
	motivo per cui possono ripresentarsi gli stessi numeri estratti in precedenza.
*/
int CCardsDeck::Shuffle(int nTotal,int& nLeft)
{
	/* evita divisione per zero e controlla limite per rand() */
	if(nTotal <= 0 || nTotal >= RAND_MAX)
		return(0);

	if(!m_cardsDeck.pool)
		return(0);

	/* se ha consumato tutto l'array (ha estratto tutte le carte dal mazzo), ricomincia dal principio */
	if(m_cardsDeck.range <= 0)
		Initialize();

	/*
	se viene passato un totale entre cui randomizzare diverso da quello iniziale, deve
	cambiare il mazzo di carte, genera quindi un nuovo array rilasciando il precedente
	*/
	if(m_nTot!=nTotal)
	{
		m_nTot = nTotal;
#if 0
		if(m_cardsDeck.pool)
			free(m_cardsDeck.pool);
		m_cardsDeck.pool = (int *)malloc(m_nTot * sizeof(int));
		if(!m_cardsDeck.pool)
			return(0);
#else
		int* pNewCardsDeck = (int*)realloc(m_cardsDeck.pool,m_nTot * sizeof(int));
		if(!pNewCardsDeck)
			return(0);
		m_cardsDeck.pool = pNewCardsDeck;
#endif
		m_cardsDeck.range = m_nTot;
		Initialize();
	}

	//#define USE_RAND_ANSI 1	/* utilizza la rand() standard */
	#define USE_RAND_M 1		/* utilizza fast_rand_range() */

	int nIndex = 0;

	/*
	genera un numero random (indice) compreso nell'intervallo rimanente, ossia estrae una carta tra quelle 
	rimaste nel mazzo
	pero' attenzione: se si usa rand(), allora bisogna implementare la logica del filtro, ma se si volesse
	usare rand_m() per disordinare il piu' possibile i numeri, allora la logica del filtro per evitare il 
	bias non e' necessaria, perche' il metodo dietro rand_m(), ossia la fast_...(), non ha modulo bias dato
	che mappa l'intero intervallo [0, 2[32]-1] in [0, range-1] in modo uniforme, senza scarti
	inoltre, piu' che la rand_m(), in tal caso andrebbe usata la fast_rand_range()
	*/
#if (defined(USE_RAND_ANSI) && defined(USE_RAND_M)) || (!defined(USE_RAND_ANSI) && !defined(USE_RAND_M))
  #error must specify rand() or rand_m() usage
#endif
#if defined(USE_RAND_ANSI)
	#pragma message("\t\t\tUSE_RAND_ANSI defined, using rand()")
	#if 1
		/*
		implementa un filtro per evitare la ricorrenza dei numeri piu' "bassi" nella matematica della divisione,
		un fenomeno conosciuto come Modulo Bias (distorsione del modulo):
		il massimo restituito da rand() e' RAND_MAX, ma se si vuole limitare il risultato ad un valore inferiore,
		allora bisogna usare rand() % <n> (con <n> ovviamente minore di RAND_MAX), ma il tal caso, ogni volta che
		si tocca il limite <n>, il "wrap" fa si che si ricominici a produrre valori a partire dal piu' basso, per
		cui la distribuzione non e' piu' uniforme, e la randomizzazione viene "truccata" dalla matematica del resto
		
		"rand()" (intervallo da 0 a 32767)		"rand() % 7" (intervallo da 0 a 7)
			0										0
			1										1
			2										2
			3										3
			4										4
			5										5
			6										6
			7										0 (ripete)
			8										1 (ripete)
			9										2 (ripete)
			10										3 (ripete)
			...										...
			32767
		
		il filtro consiste quindi nello scartare i numeri che cadono nella "coda" incompleta che crea lo sbilanciamento
		per cui invece di prendere qualsiasi numero restituito da rand(), se il numero cade nell'ultima parte incompleta 
		dell'intervallo, viene scartato e ne viene chiesto un altro
		
		con RAND_MAX = 32767 ed un mazzo di 150 carte, l'errore statistico non si nota ad occhio nudo, ma con software 
		di crittografia, o gestione di casino' online, dove milioni di $$$ dipendono dalla perfezione statistica, o in 
		qualsiasi altro caso dove il numero di carte nel mazzo sia elevato, la "coda" diventa molto piu' significativa 
		rispetto al totale
		*/
		int nLimit = RAND_MAX - (RAND_MAX % m_cardsDeck.range);
		do {
			nIndex = rand();
		} while(nIndex >= nLimit); /* scarta i numeri nella "coda" sbilanciata */

		nIndex = nIndex % m_cardsDeck.range;
	#else
		nIndex = rand() % m_cardsDeck.range;
	#endif
#endif
#if defined(USE_RAND_M)
	#pragma message("\t\t\tUSE_RAND_M defined, using rand_m()")
	nIndex = fast_rand_range(m_cardsDeck.range);  /* gia' unbiased, senza filtro */
#endif

	/* ricava il valore dell'elemento all'indice di cui sopra */
	int nValue = m_cardsDeck.pool[nIndex];

	/* sostituisce il valore estratto con quello dell'ultimo elemento dell'array */
	m_cardsDeck.pool[nIndex] = m_cardsDeck.pool[m_cardsDeck.range - 1];

	/* aggiorna il range di estrazione per la prossima chiamata */
	nLeft = --m_cardsDeck.range;

	TRACEEXPR((_TRACE_FLAG_INFO,__FILE__,__LINE__,"Shuffle(%ld): index: %ld, value: %ld, range: %ld\n",nTotal,nIndex,nValue,m_cardsDeck.range));

	return(nValue);
}
