/*$
	CNexgenIPL.cpp
	Classe derivata per interfaccia con NexgenIPL (v.2.9.6).
	Luca Piergentili, 01/09/00
	lpiergentili@yahoo.com

	Questo e' un classico esempio (insieme a CPaintLib.cpp) di "refactoring" per risuscitare
	una libreria e farla tornare a funzionare in un ambiente ormai completamente alieno (Win64).

	Varie parti del codice di interfaccia racchiudono le chiamate con try/catch dato che la libreria
	genera abbbastanza eccezioni.
	Tutta la parte relativa al "martirio" (vedi _MARTYRDOM, etc.) e' semplicemente un esercizio tecnico
	per non far crashare l'applicativo principale durante l'uso della libreria, usando un martire che
	viene appunto immolato al suo posto.

	Ad memoriam - Nemo me impune lacessit.
*/
#include "env.h"
#include "pragma.h"
#include "macro.h"
#include "window.h"
#include "win32api.h"
#include "ImageConfig.h"

#ifdef HAVE_NEXGENIPL_LIBRARY

#include <math.h>
#include <stdio.h>
#include "strings.h"
#include "CImage.h"
#include "CNexgenIPL.h"
#include "libtiff.h"
#include "getopt.h"

#include "traceexpr.h"
//#define _TRACE_FLAG			_TRFLAG_TRACEOUTPUT // opzioni: _TRFLAG_NOTRACE, _TRFLAG_TRACEFILE, _TRFLAG_TRACECONSOLE, _TRFLAG_TRACEOUTPUT, _TRFLAG_TRACEBREAKPOINT
#define _TRACE_FLAG			_TRFLAG_NOTRACE // opzioni: _TRFLAG_NOTRACE, _TRFLAG_TRACEFILE, _TRFLAG_TRACECONSOLE, _TRFLAG_TRACEOUTPUT, _TRFLAG_TRACEBREAKPOINT
#define _TRACE_FLAG_INFO	_TRACE_FLAG
#define _TRACE_FLAG_WARN	_TRACE_FLAG
#define _TRACE_FLAG_ERR		_TRACE_FLAG

#ifdef DEBUG
  #define TRACE_MARTYRDOM 1
#endif

#if defined(_MARTYRDOM)
  #pragma message("compiling CNexgenIPL class in martyrdom mode")
  #if !defined(_PROPHET) && !defined(_MARTYR)
    #error This must be a Martyrdom! But NO prophet NOR martyr defined!
  #endif
#else
  #pragma message("compiling CNexgenIPL class in standard mode")
#endif

/*
	CNexgenIPL()
*/
CNexgenIPL::CNexgenIPL()
{
	// descrizione ultimo errore
	memset(m_szError,'\0',sizeof(m_szError));

	// nome file relativo all'immagine
	memset(m_szFileName,'\0',sizeof(m_szFileName));
	memset(m_szFormat,'\0',sizeof(m_szFormat));

	// header metadati
    memset(&m_InfoHeader,'\0',sizeof(IMAGEHEADERINFO));
    m_InfoHeader.type = NULL_PICTURE;
    m_InfoHeader.xres = -1;
    m_InfoHeader.yres = -1;
    m_InfoHeader.restype = -1;
    m_InfoHeader.compression = -1;
    m_InfoHeader.quality = -1;
    m_InfoHeader.filesize = (unsigned long)-1L;
    m_InfoHeader.width = (unsigned long)-1L;
    m_InfoHeader.height = (unsigned long)-1L;
    m_InfoHeader.memused = 0;

	// puntatore all'oggetto immagine
	m_pImage = NULL;
	//m_NexgenObject

	// tipi files riconosciuti secondo documentazione ufficiale:
	// "Reading: BMP, PCX, PNG, PBM, PPM, PGM, TGA, TIFF, JPEG, GIF (animated), IFF, ILBM, RAS, EPS, ICO, MNG, JNG, WMF, EMF, AMP, PSP, JP2, JPC, YUV, CUT"
	// "Writing: BMP, PCX, PNG, MNG, PBM, PPM, PGM, TGA, TIFF, JPEG, GIF (uncompressed), JP2, YUV, CUT, RAS"
	// mantenere sincronizzato con la determinazione del tipo immagine in Load()
	LPIMAGETYPE p;
	ADDFILETYPE(PNG_PICTURE,	".png",	"Portable Network Graphics Format ",IMAGE_READ_FLAG|IMAGE_WRITE_FLAG|IMAGE_WEB_FLAG,p)
	ADDFILETYPE(JPEG_PICTURE,	".jpg",	"JPEG file ",						IMAGE_READ_FLAG|IMAGE_WRITE_FLAG|IMAGE_WEB_FLAG,p)
	ADDFILETYPE(GIF_PICTURE,	".gif",	"GIF file ",						IMAGE_READ_FLAG|IMAGE_WRITE_FLAG|IMAGE_WEB_FLAG,p)

	ADDFILETYPE(BMP_PICTURE,	".bmp",	"BMP file ",						IMAGE_READ_FLAG|IMAGE_WRITE_FLAG,p)
	ADDFILETYPE(BMP_PICTURE,	".dib",	"DIB file ",						IMAGE_READ_FLAG,p)
	ADDFILETYPE(BMP_PICTURE,	".rle",	"RLE file ",						IMAGE_READ_FLAG,p)
	ADDFILETYPE(ICO_PICTURE,	".ico",	"Icon file ",						IMAGE_READ_FLAG,p)
	ADDFILETYPE(JPEG_PICTURE,	".jpeg","JPEG file ",						IMAGE_READ_FLAG|IMAGE_WRITE_FLAG,p)
	ADDFILETYPE(JPEG_PICTURE,	".jpe",	"JPEG file ",						IMAGE_READ_FLAG|IMAGE_WRITE_FLAG,p)
	ADDFILETYPE(JPEG_PICTURE,	".jif",	"JPEG file ",						IMAGE_READ_FLAG|IMAGE_WRITE_FLAG,p)
	ADDFILETYPE(JPEG_PICTURE,	".jfif","JPEG file ",						IMAGE_READ_FLAG|IMAGE_WRITE_FLAG,p)
	ADDFILETYPE(PCX_PICTURE,	".pcx",	"PCX file ",						IMAGE_READ_FLAG|IMAGE_WRITE_FLAG,p)
	ADDFILETYPE(PGM_PICTURE,	".pgm",	"Portable Graymap File ",			IMAGE_READ_FLAG|IMAGE_WRITE_FLAG,p)
	ADDFILETYPE(PPM_PICTURE,	".ppm",	"PPM file ",						IMAGE_READ_FLAG|IMAGE_WRITE_FLAG,p)
	ADDFILETYPE(TGA_PICTURE,	".tga",	"TARGA file ",						IMAGE_READ_FLAG|IMAGE_WRITE_FLAG,p)
	ADDFILETYPE(TIFF_PICTURE,	".tif",	"TIFF file ",						IMAGE_READ_FLAG|IMAGE_WRITE_FLAG,p)
	ADDFILETYPE(TIFF_PICTURE,	".tiff","TIFF file ",						IMAGE_READ_FLAG|IMAGE_WRITE_FLAG,p)

	// range e valori di default per i filtri implementati
	SetFilterParams("Blur",				0,0,0,			"","",True,&CImageObject::Blur);
	SetFilterParams("Brightness",		-100,100,30,	"","",True,&CImageObject::Brightness);
	SetFilterParams("Contrast",			-100,100,30,	"","",True,&CImageObject::Contrast);
	SetFilterParams("EdgeEnhance",		0,0,0,			"","",True,&CImageObject::EdgeEnhance);
	SetFilterParams("Emboss",			0,0,0,			"","",True,&CImageObject::Emboss);
	SetFilterParams("FindEdge",			0,100,60,		"","",True,&CImageObject::FindEdge);
	SetFilterParams("GammaCorrection",	0,5,3,			"","",True,&CImageObject::GammaCorrection);
	SetFilterParams("Grayscale",		0,0,0,			"","",True,&CImageObject::Grayscale);
	SetFilterParams("Hue",				-180,180,100,	"","",True,&CImageObject::Hue);
//	SetFilterParams("Median",			1,30,1,			"","",True,&CImageObject::Median); // fa quasi sempre il botto
	SetFilterParams("MirrorHorizontal",	0,0,0,			"","",True,&CImageObject::MirrorHorizontal);
	SetFilterParams("MirrorVertical",	0,0,0,			"","",True,&CImageObject::MirrorVertical);
	SetFilterParams("Posterize",		2,255,7,		"","",True,&CImageObject::Posterize);
	SetFilterParams("Rotate90Left",		0,0,0,			"","",True,&CImageObject::Rotate90Left);
	SetFilterParams("Rotate90Right",	0,0,0,			"","",True,&CImageObject::Rotate90Right);
	SetFilterParams("Rotate180",		0,0,0,			"","",True,&CImageObject::Rotate180);
	SetFilterParams("Saturation",		-100,100,100,	"","",True,&CImageObject::Saturation);
	SetFilterParams("Sharpen",			0,0,0,			"","",True,&CImageObject::Sharpen);
}

/*
	ConvertToBPP()
*/
UINT CNexgenIPL::ConvertToBPP(UINT nBitsPerPixel,UINT nFlags/*=0*/,RGBQUAD *pPalette/*=NULL*/,UINT nColors/*=0*/)
{
	// controlla che l'oggetto immagine sia valido
	if(!IsValid(__func__))
		return(GDI_ERROR);

	// ha gia' i BPP richiesti
	int nBPP = m_pImage->GetBitsPerPixel();
	if(nBitsPerPixel==nBPP)
		return(NO_ERROR);

	UINT nRet = GDI_ERROR;
	BOOL bConverted = FALSE;

	// conversione
	switch(nBitsPerPixel)
	{
		case 1:
		case 4:
		case 8:
			bConverted = m_pImage->Quantize(nBitsPerPixel);
			break;

		case 24:
			bConverted = m_pImage->ConvertTo24BPP();
			break;
			
		case 32:
		default:
			bConverted = m_pImage->ConvertTo32BPP();
			break;
	}

	if(bConverted)
		nRet = UpdateHeaderInfo();

	if(nRet!=NO_ERROR)
		SetLastErrorDescriptionEx("%s(): BPP conversion from %d to %d failed",__func__,nBPP,nBitsPerPixel);

	return(nRet);
}

/*
	GetDIB()
*/
HDIB CNexgenIPL::GetDIB(UINT* pSize/* = NULL*/)
{
	// controlla che l'oggetto immagine sia valido
	if(!IsValid(__func__))
		return((HDIB)NULL);

	HDIB hDib = (HDIB)NULL;

	BITMAPINFOHEADER* pBitmapInfoHdr = (BITMAPINFOHEADER*)GetBMI();
	if(pBitmapInfoHdr)
	{
		if(LockData())
		{
			LPSTR pData = (LPSTR)GetPixels();
			UINT nBitmapSize = (pBitmapInfoHdr->biHeight * GetDIBOrder()) * GetBytesWidth();
			UINT nColorBytes = sizeof(RGBQUAD) * GetNumColors();
			UINT nTotalBytes = sizeof(BITMAPINFOHEADER) + nColorBytes + nBitmapSize;

			if(pSize)
				*pSize = (UINT)nTotalBytes;

			hDib = (HDIB)::GlobalAlloc(GMEM_MOVEABLE|GMEM_DDESHARE|GMEM_ZEROINIT,nTotalBytes);
			if(hDib)
			{
				BITMAPINFOHEADER* pBitmapInfoHeader = (BITMAPINFOHEADER*)::GlobalLock((HGLOBAL)hDib);
				if(pBitmapInfoHeader)
				{
					memcpy(pBitmapInfoHeader,pBitmapInfoHdr,sizeof(BITMAPINFOHEADER)+nColorBytes);
					memcpy((LPSTR)pBitmapInfoHeader+sizeof(BITMAPINFOHEADER)+nColorBytes,pData,nBitmapSize);
					pBitmapInfoHeader->biHeight = GetHeight();
					pBitmapInfoHeader->biSizeImage = nBitmapSize;

					::GlobalUnlock((HGLOBAL)hDib);
				}
			}

			UnlockData();
		}
	}

	if(!hDib)
		SetLastErrorDescriptionEx("%s(): unable to retrieve the DIB section",__func__);

	return(hDib);
}

/*
	SetDIB()
*/
BOOL CNexgenIPL::SetDIB(HDIB hDib,int nOrientation/* = 1*/)
{
	// qui NON bisogna controllare se l'oggetto immagine e' valido perche' lo sta impostando ex novo
	//if(!IsValid(__func__))
	//	return(FALSE);

	if(!hDib)
	{
		SetLastErrorDescriptionEx("%s(): invalid DIB data",__func__);
		return(FALSE);
    }

	BOOL bResult = FALSE;
	BITMAPINFOHEADER* pBitmapInfoHeader = (BITMAPINFOHEADER*)::GlobalLock((HGLOBAL)hDib);	

	if(pBitmapInfoHeader)
	{
		LPSTR pData = NULL;
		UINT nNumColors = 0;
		if(pBitmapInfoHeader->biBitCount <= 8)
			nNumColors = 1 << pBitmapInfoHeader->biBitCount;
		
		pData = ((LPSTR)pBitmapInfoHeader) + (sizeof(BITMAPINFOHEADER) + (sizeof(RGBQUAD)*nNumColors));
		
		if(Create((BITMAPINFO*)pBitmapInfoHeader,pData))
		{
			bResult = TRUE;
		}
		else
		{
			bResult = FromDIBWrapper(hDib,nOrientation); 
		}
				
		::GlobalUnlock((HGLOBAL)hDib);
	}

	if(!bResult)
		SetLastErrorDescriptionEx("%s(): unable to set the DIB section",__func__);

	return(bResult);
}

/*
	FromDIBWrapper()

	Il secondo parametro specifica l'orientazione per ovviare ad un bug (?) della FromDIB().
	Se la provenienza della creazione dell'oggetto e' interna (new CNexgenIPL()...), il DIB e' top-down (-1) ma 
	quando passa per la FromDIB() questa nel rimettere a top-dowm genera un bottom-up, per cui poi deve rigirare 
	l'immagine. Se invece proviene da un DIB standard (bottom-up), l'effetto della FromDIB() non si duplica e 
	quindi non c'e' bisogno di rigirare.
	In defintiva il flag e' per stabilire la provenienza della chiamata, perche' a causa del bug nella Create(),
	per creare un immagine nuova deve per forza importare un DIB con la FromDIB().
	                      
	                  FromDIB() passa a:
	Echo()  crea DIB NEG |  NEG  |  = POS -> dev per rimettere a negativo
	                    -|->    -|->
	Crop()  crea DIB POS |  NEG  |  = NEG -> gia' negativo, no rotazione
*/
BOOL CNexgenIPL::FromDIBWrapper(HDIB hDib,int nOrientation)
{
	// controlla che l'oggetto immagine sia valido
	if(!IsValid(__func__))
		return(FALSE);

	BOOL bFlag = m_pImage->FromDIB(hDib); // per incompatibilita' tra la 2.7 e la 2.9 con la Create()

	if(bFlag)
	{
		// FromDIB() converte a top-down, ma a quanto sembra, chiamandola su un oggetto creato con la propria
		// classe, si ottiene una sorta di neg * neg = pos, per cui poi sotto deve ruotare l'immagine per
		// correggere
		if(nOrientation < 0)
			Rotate180();

		// deve chiamare qui e non nella Create() perche' questa ora e' un semplice wrapper per la SetDIB()
		// che restituisce sempre FALSE in modo tale che la FromDIB() possa impostare correttamente i dati
		UpdateHeaderInfo();
	}

	return(bFlag);
}

/*
	SetPalette()
*/
BOOL CNexgenIPL::SetPalette(UINT nIndex,UINT nColors,RGBQUAD* pPalette)
{
	// controlla che l'oggetto immagine sia valido
	if(!IsValid(__func__))
		return(FALSE);

	BOOL bPalette = FALSE;
	UINT nColorData = GetNumColors();
	if(!nColorData || nIndex!=0 || pPalette==NULL || nColors <= 0 || (nColors+nIndex) > nColorData)
	{
		SetLastErrorDescriptionEx("%s(): unable to set the palette",__func__);
	}
	else
	{
		m_pImage->FillColorTable(pPalette,nColors);
		bPalette = TRUE;
	}

	return(bPalette);
}

/*
	UpdateHeaderInfo()
*/
DWORD CNexgenIPL::UpdateHeaderInfo(void)
{
	// controlla che l'oggetto immagine sia valido
	if(!IsValid(__func__))
		return(GDI_ERROR);

	// BPP originali del file    
	//m_InfoHeader.bppOriginal -> impostato nella Load()

	// ricava le dimensioni dell'immagine, NexgenIPL usa DIB Windows -> stride × height
	// GetBytesWidth() restituisce lo stride, normalmente calcolato con la formula: 
	// ((width * bpp + 31) / 32) * 4;
	m_InfoHeader.bppConverted	= m_pImage->GetBitsPerPixel();
    m_InfoHeader.width			= m_pImage->GetWidth();
    m_InfoHeader.height			= m_pImage->GetHeight();
    m_InfoHeader.bpp			= m_pImage->GetBitsPerPixel();
    m_InfoHeader.memused		= m_pImage->GetImageSize();
    
    // numero di colori nella palette (0 x 32 bpp)
	m_InfoHeader.colors = m_pImage->GetNumColorEntries();

    // estrae risoluzione (DPI) ed usa HResolution e VResolution invece di x e y
    BTResolution resolution = m_pImage->GetResolution(BTMETRIC_METER);
    if(resolution.HResolution > 0 && resolution.VResolution > 0)
	{
        // conversione basata sulla metrica
        switch(resolution.nResolutionMetric) {
            case BTMETRIC_METER: // metri -> Pollici
                m_InfoHeader.xres = (float)(resolution.HResolution * 0.0254);
                m_InfoHeader.yres = (float)(resolution.VResolution * 0.0254);
                m_InfoHeader.restype = RESUNITINCH;
                break;
            case BTMETRIC_CENTIMETER: // centimetri -> Pollici
                m_InfoHeader.xres = (float)(resolution.HResolution * 0.393701);
                m_InfoHeader.yres = (float)(resolution.VResolution * 0.393701);
                m_InfoHeader.restype = RESUNITINCH;
                break;
            case BTMETRIC_INCH: // gia' in pollici
                m_InfoHeader.xres = (float)resolution.HResolution;
                m_InfoHeader.yres = (float)resolution.VResolution;
                m_InfoHeader.restype = RESUNITINCH;
                break;
            case BTMETRIC_NONE:
            default: // default DPI
                m_InfoHeader.xres = 72.0f;
                m_InfoHeader.yres = 72.0f;
                m_InfoHeader.restype = RESUNITINCH;
                break;
        }
    }
    else {
        // default DPI
        m_InfoHeader.xres = 72.0f;
        m_InfoHeader.yres = 72.0f;
        m_InfoHeader.restype = RESUNITINCH;
    }

	// determina il colorSpace
	if(m_InfoHeader.bpp <= 8)
	{
		// le immagini <= 8 bpp sono palette-based per definizione
		// si potrebbe distinguere grayscale palette, ma semanticamente e' sempre palette
		m_InfoHeader.colorSpace = COLOR_PALETTE;
	}
	else
	{
		// truecolor, usa bpp per stimare
		switch(m_InfoHeader.bpp) {
			case 1: // 1 bpp e' sempre palette con 2 colori (bianco/nero), non e' tecnicamente grayscale
				m_InfoHeader.colorSpace = COLOR_PALETTE;
				break;
            
			case 8: // una volta qui (8 bpp e 0 colors) e' un grayscale 8 bpp
				m_InfoHeader.colorSpace = COLOR_GRAYSCALE;
				break;
            
			case 15:
			case 16: // 15/16 bpp: tipicamente RGB 5-6-5 o 5-5-5-1, ma potrebbe essere grayscale a 16 bit (ad es. certi .tiff)
				if(IsTrueGrayscale() || IsVisualGrayscaleStatistical(VISUAL_GRAYSCALE_PARAMS))
					m_InfoHeader.colorSpace = COLOR_GRAYSCALE;
				else
					m_InfoHeader.colorSpace = COLOR_RGB;
				break;
            
			case 24: // 24 bpp: quasi sempre RGB
				m_InfoHeader.colorSpace = COLOR_RGB;
				break;
            
			case 32: // 32 bpp: potrebbe essere RGBA, RGBX, CMYK, etc., senza info aggiuntive e' solo un ipotesi
				m_InfoHeader.colorSpace = COLOR_RGBA;
				break;
            
			case 48: // 48 bpp: RGB a 16 bit per canale
				m_InfoHeader.colorSpace = COLOR_RGB;
				break;
            
			case 64: // 64 bpp: RGBA a 16 bit per canale
				m_InfoHeader.colorSpace = COLOR_RGBA;
				break;
            
			default:
				m_InfoHeader.colorSpace = COLOR_RGB; // ipotesi conservativa
				break;
		}
	}
    
    // compressione e qualita' andrebbero estratte via metadati EXIF/APP via parsing manuale, omette per il momento
    m_InfoHeader.compression = 0;	// nessuna per default
    m_InfoHeader.quality = -1;		// sconosciuta

    return(NO_ERROR);
}

/*
	Create()

	La versione originale non funziona, crea l'oggetto ma fallisce con l'impostazione dei dati.
	Come conseguenza, la SetDIB(), che chiama a sua volta la Create(), solo funziona perche' usa
	FromDIB() come fallback
	Notare che la SetDIB(), se chiama direttamente la FromDIB(), senza passare por il fallimento
	della Create(), genera eccezione.
	Per questo ora il "flusso magico" della versione "furbetta" e' il seguente:
	- Create() prepara m_NexgenObject e m_pImage, restituendo FALSE
	- SetDIB() ottiene FALSE e chiama quindi FromDIB(hDib)
	- FromDIB() funziona perche' l'oggetto e' stato preparato dai preliminari presenti nella Create()
*/
// versione "furbetta" che funziona
BOOL CNexgenIPL::Create(BITMAPINFO* pBitmapInfo, void* pData)
{
	// qui NON bisogna controllare se l'oggetto immagine e' valido perche' lo sta creando ex novo
	//if(!IsValid(__func__))
	//	return(FALSE);

    // prepara l'oggetto, come la versione originale, ma poi 
	// fallisce deliberatamente per forzare SetDIB a usare FromDIB()
	if(!pBitmapInfo)
		return(FALSE);

	Unload();

    // fa tutto cio' che fa l'originale tranne la copia finale dei dati
    m_NexgenObject.GetObjectData().DeleteObject();
	BITMAPINFOHEADER* pBih = (BITMAPINFOHEADER*)pBitmapInfo;
    
    // crea l'oggetto BTCImageData
    if(!m_NexgenObject.GetObjectData().Create(pBih->biWidth,abs(pBih->biHeight),pBih->biBitCount))
		return(FALSE);
    
    // imposta m_pImage
    if(!(m_pImage = m_NexgenObject.GetObjectDataPtr()))
		return(FALSE);
    
    // copia la palette se necessario
    if(pBih->biBitCount <= 8 && pBitmapInfo->bmiColors)
	{
        int nColors = 1 << pBih->biBitCount;
        m_NexgenObject.GetObjectData().FillColorTable(pBitmapInfo->bmiColors,nColors);
    }
    
    // flusso magico: NON copia i dati pixel, lascia invece che SetDIB usi FromDIB()
    
    // ritorna FALSE per forzare SetDIB a usare FromDIB()
	return(FALSE);
}
#if 0
// versione originale, fallisce
BOOL CNexgenIPL::Create(BITMAPINFO *pBitmapInfo,void *pData/* = NULL */)
{
	BOOL bCreated = FALSE;

	if(pBitmapInfo)
	{
		m_NexgenObject.GetObjectData().DeleteObject();

		BITMAPINFOHEADER* pBitmapInfoHeader = (BITMAPINFOHEADER*)pBitmapInfo;
		RGBQUAD* pPalette = pBitmapInfo->bmiColors;
		
		if(pBitmapInfoHeader && pPalette)
		{
			// per incompatibilita' tra la 2.7 e la 2.9 (pDataDst e' a null)
			if(m_NexgenObject.GetObjectData().Create(pBitmapInfoHeader->biWidth,abs(pBitmapInfoHeader->biHeight),pBitmapInfoHeader->biBitCount))
			{
				if((m_pImage = m_NexgenObject.GetObjectDataPtr())!=(BTCImageData*)NULL)
				{
					int nColorData = 0;
					if(pBitmapInfoHeader->biBitCount <= 8)
						nColorData = 1 << pBitmapInfoHeader->biBitCount;
					if(nColorData)
						m_NexgenObject.GetObjectData().FillColorTable(pPalette,nColorData);
					
					unsigned char* pDataDst = m_NexgenObject.GetObjectDataPtr()->GetBits(0,0);
					if(pDataDst)
					{
						int nWidthEnBytes = WIDTHBYTES(pBitmapInfoHeader->biWidth * pBitmapInfoHeader->biBitCount,GetAlignment());
						int nTotalBytes = abs(pBitmapInfoHeader->biHeight) * nWidthEnBytes;
						if(pData)
							memcpy(pDataDst,pData,nTotalBytes);
						bCreated = TRUE;
					}
				}
			}
		}
	}

	return(bCreated);
}
#endif

/*
	BTNexgenIPL32.dll scasina grandemente con alcuni files (basicamente GIF). In base alle prove effettuate, 
	potrebbe essere dovuto al parser dell'header o della palette della GIF che genera una eccezione mentre 
	cerca di capire com'e' fatta l'immagine, ancora prima di estrarne i pixel.
	Il codice di eccezione 0xc0000374 (il piu' comune con i GIF), corrisponde a Heap Corruption, che annulla 
	quindi i vari __try, VEH, try-catch. Quando il sistema operativo (via ntdll.dll) rileva che l'Heap e' stato 
	corrotto, attiva un meccanismo di sicurezza per cui:
	- il sistema NON solleva una normale eccezione SEH che possa essere catturata
	- il Kernel decide che il processo e' diventato pericoloso (potrebbe essere un tentativo di exploit) e lo 
	  abbatte istantaneamente per proteggere l'integrita' del PC
	(es. C:\Users\lpier\Documents\Luca\Pictures\Unclassified\1451580735_083582_1451582826_sumario_normal.gif)

	Per verificare il tipo di eccezione (possono essere varie), se il programma non la intercetta, allora usare
	Win + R e lanciare eventvwr.msc, andare su Registri di Windows -> Applicazione e cercare l'errore (icona in
	rosso), li viene riportato il codice dell'eccezione (ad es.0xc0000005=access violation o 0xc0000409=stack 
	buffer overrun, etc.).

	Qui il meccanismo usato per ovviare al problema e' far immolare un martire per capire se il file grafico puo'
	essere caricato senza eccezioni. Il codice della Load() (il profeta) immola (esegue) il martire e se questo 
	sopravvive (restituendo 0), allora il file si puo' caricare senza problemi. Se invece il martire si sacrifica, 
	allora il sistema operativo restituira', come codice di ritorno, il codice dell'eccezione generata.

	Lo stesso meccanismo viene usato per la Save(), a maggior ragione perche' la Save() non puo' essere wrappata
	con __try/__except.

	Se invece NON viene definita la macro _MARTYRDOM, allora il codice esegue le Load() e Save() standard, senza
	nessun tipo di protezione.
*/

#if defined(_MARTYRDOM)
/*
	GetMartyrName()

	Imposta il nome del martire (Titti.exe) per l'esecuzione sulla stessa directory del profeta (Calimero.exe).
	Questo per evitare fallire se il cazzone di turno esegue il programma principale (Calimero.exe) tramite un
	collegamento diretto.
*/
/* LOCAL */void CNexgenIPL::GetMartyrName(char* szDest,size_t nDestSize)
{
	char szPath[_MAX_PATH+1] = {0};
	char* pLastSlash = NULL;

	// recupera il path completo dell'eseguibile corrente (Calimero.exe)
	GetModuleFileName(NULL,szPath,_MAX_PATH);

	// cerca l'ultimo backslash per isolare la directory
	pLastSlash = strrchr(szPath,'\\');
	if(pLastSlash)
		*(pLastSlash+1) = '\0'; // taglia a partire del backslash

	// concatena il nome del proxy
	strcpyn(szDest,szPath,nDestSize);
	strcatn(szDest,MARTYR_EXECUTABLE_NAME,nDestSize);
}

/*
	LoadFile()
	
	Carica il file. Qui e' dove avviene il sacrificio o dove si avvera la profezia. Vedi le note 
	sopra.

	Come esempio per impostare le opzioni (ricordarsi che la __try/__except NON si puo' usare se 
	sono presenti oggetti C++ come la classe BTCDecoderOptionsGIF):

		BTCDecoderOptionsGIF gifOptions;
		gifOptions.SetOption(BTCDecoderOptionsGIF::BTDO_GIF_IMAGEINDEX,1);
		long result = m_NexgenObject.Load(lpcszFileName,&gifOptions);
		if(result!=-1L)
			m_pImage = m_NexgenObject.GetObjectDataPtr();
		else
			m_pImage = NULL;
*/
/* LOCAL */BOOL CNexgenIPL::LoadFile(LPCSTR lpcszFileName)
{
	DWORD dwError = (DWORD)-1L;

	/*
		la LoadFile() viene chiamata dal martire e dal profeta:

		- martire:
		  nell'ottica del martire, e' la unica load chiamata, se fa il botto il profeta ne terra' 
		  conto e non (ri)carichera' il file

		- profeta:
		  viene chiamata direttamente SOLO per i files che NON sono GIF, per quelli in formato GIF 
		  chiama invece il martire (che eseguira' sempre questo stesso codice)

		il meccanismo martire/profeta viene usato solo per i GIF perche' al momento sono gli unici 
		che danno problemi con NexgenIPL, si puo' comunque ampliare o generalizzare per tutti i 
		formati

		occhio: il meccanismo attuale e' INEFFICENTE perche' chiama due volte la LoadFile per i GIF
		(prima il martire e se questi non fa il botto, il profeta), si potrebbe cambiare usando la
		shared memory come fa con Save(), ossia se il martire non fa il botto, allora passa il DIB
		al profeta tramite la memoria condivisa
	*/

	// blocco eseguito dal martire (sempre) e dal profeta (solo se NON si tratta di un GIF)
	__try {

		if(m_NexgenObject.Load(lpcszFileName)!=-1L)
			m_pImage = m_NexgenObject.GetObjectDataPtr();
		else
			m_pImage = NULL;
	}
	__except(dwError = GetExceptionCode(),EXCEPTION_EXECUTE_HANDLER) {
		m_pImage = NULL;
		SetLastErrorDescriptionEx("%s(): an unexpected exception (0x%08X) has occurred while loading: %s",__func__,dwError,lpcszFileName);
	}

#if defined(_PROPHET) /* e' il profeta */

	// MANTENERE UGUALE a versione per _MARTYRDOM non definito (vedi sotto)
	if(m_pImage)
	{
		// salva il nome del file caricato ed imposta il formato in base all'estensione del file
		strcpyn(m_szFileName,lpcszFileName,sizeof(m_szFileName));
		memset(m_szFormat,'\0',sizeof(m_szFormat));
		char* p = strrchr(m_szFileName,'.');
		if(p)
		{
			p++;
			if(p && *p)
			{
				strcpyn(m_szFormat,p,sizeof(m_szFormat));
				strupr(m_szFormat);
			}
		}

		// registra il tipo immagine
		m_InfoHeader.type = NULL_PICTURE;
		if(*m_szFormat)
		{
			if(stricmp(m_szFormat,"BMP")==0 || stricmp(m_szFormat,"DIB")==0 || stricmp(m_szFormat,"RLE")==0)
				m_InfoHeader.type = BMP_PICTURE;
			else if(stricmp(m_szFormat,"GIF")==0)
				m_InfoHeader.type = GIF_PICTURE;
			else if(stricmp(m_szFormat,"ICO") == 0)
				m_InfoHeader.type = ICO_PICTURE;
			else if(stricmp(m_szFormat,"JPG")==0 || stricmp(m_szFormat,"JPEG")==0 || stricmp(m_szFormat,"JPE")==0 || stricmp(m_szFormat,"JIF")==0 || stricmp(m_szFormat,"JFIF")==0)
				m_InfoHeader.type = JPEG_PICTURE;
			else if(stricmp(m_szFormat,"PCX")==0)
				m_InfoHeader.type = PCX_PICTURE;
			else if(stricmp(m_szFormat,"PGM")==0)
				m_InfoHeader.type = PGM_PICTURE;
			else if(stricmp(m_szFormat,"PNG")==0)
				m_InfoHeader.type = PNG_PICTURE;
			else if(stricmp(m_szFormat,"PPM")==0)
				m_InfoHeader.type = PPM_PICTURE;
			else if(stricmp(m_szFormat,"TGA")==0)
				m_InfoHeader.type = TGA_PICTURE;
			else if(stricmp(m_szFormat,"TIFF")==0 || stricmp(m_szFormat,"TIF")==0)
				m_InfoHeader.type = TIFF_PICTURE;
		}

		// salva i bpp originali dell'immagine
		m_InfoHeader.bppOriginal = m_pImage->GetBitsPerPixel();

		// dimensione file
		HANDLE hFile = ::CreateFile(m_szFileName,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
		if(hFile!=INVALID_HANDLE_VALUE)
		{
			m_InfoHeader.filesize = ::GetFileSize(hFile,NULL);
			::CloseHandle(hFile);
		}
		else
		{
			dwError = ::GetLastError();
			SetLastErrorDescriptionEx("%s(): unable to open %s (%ld)",__func__,m_szFileName,dwError);
			return(FALSE);
		}

		// aggiorna il resto dell'header
		UpdateHeaderInfo();
	}

#endif

	return(m_pImage!=NULL);
}

/*
	Load()
	
	Carica il file, distinguendo se l'attore e' il martire o il profeta.
	Vedi le note sopra.
*/
BOOL CNexgenIPL::Load(LPCSTR lpcszFileName,DWORD& dwError)
{
	// qui NON bisogna controllare se l'oggetto immagine e' valido perche' lo sta caricando da file
	//if(!IsValid(__func__))
	//	return(FALSE);

	dwError = (DWORD)-1L;

	if(!IsSupportedFormat(lpcszFileName))
		return(FALSE);

	Unload();

#if defined(_MARTYR) /* e' il martire */
	
	return(LoadFile(lpcszFileName));

#elif defined(_PROPHET) /* e' il profeta */

	// se NON e' un gif, usa la procedura normale
	if(!CheckFileExtension(lpcszFileName,"GIF"))
	{
		dwError = 0L;
		return(this->LoadFile(lpcszFileName));
	}

	// se E' un gif, martirizza
	char szCmd[MAX_CMDLINE+1] = {0};
	char szMartyr[_MAX_PATH+1] = {0};

  #ifdef DEBUG
	// anche se si imposta la directory di lavoro su C:\WCHG, quando si esegue dal
	// debugger, la GetModuleFileName() chiamata da GetMartyrName() restituisce
	// "C:\DEV\wchg\Debug\Calimero.exe", quindi inchioda il nome, perche' il progetto
	// per Titti.exe copia il file in C:\WCHG a fine compilazione
	// in modo Release si suppone sara' sempre la stessa directory (es. C:\WCHG)
	snprintf(szMartyr,sizeof(szMartyr),"\"C:\\WCHG\\%s\"",MARTYR_EXECUTABLE_NAME);
	snprintf(szCmd,sizeof(szCmd),"\"C:\\WCHG\\%s\" \"0:%s\"",MARTYR_EXECUTABLE_NAME,lpcszFileName);
  #else
	GetMartyrName(szMartyr,sizeof(szMartyr));
	snprintf(szCmd,sizeof(szCmd),"\"%s\" \"0:%s\"",szMartyr,lpcszFileName);
  #endif

	STARTUPINFO si = {sizeof(si)};
	PROCESS_INFORMATION pi = {0};

	if(CreateProcess(NULL,szCmd,NULL,NULL,FALSE,0/*CREATE_NO_WINDOW*/,NULL,NULL,&si,&pi))
	{
		DWORD dwExitCode = 65L;
		DWORD dwWaitResult = ::WaitForSingleObject(pi.hProcess,3000);
		if(dwWaitResult==WAIT_TIMEOUT) // il proxy e' rimasto appeso, lo abbatte
			TerminateProcess(pi.hProcess,dwExitCode);
		else
			GetExitCodeProcess(pi.hProcess,&dwExitCode);

		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);

		if(dwExitCode==0L)
		{
			dwError = 0L;
			return(this->LoadFile(lpcszFileName));
		}
		else
		{
			/*
			il martire restituisce:

			0 = ok
			1 = chiamato senza parametri
			2 = errore della funzione (NON e' una eccezione)
			n = codice eccezione:

			0xC0000005	EXCEPTION_ACCESS_VIOLATION
			0xC0000374	STATUS_HEAP_CORRUPTION
			0xC0000094	EXCEPTION_INT_DIVIDE_BY_ZERO
			0xC00000FD	EXCEPTION_STACK_OVERFLOW
			0xC0000409	STATUS_STACK_BUFFER_OVERRUN
			0xC0000135	STATUS_DLL_NOT_FOUND
			[...]
			*/
			dwError = dwExitCode;

			if(dwError < 5)
				SetLastErrorDescriptionEx("%s(): sub-process terminated with code %d",__func__,dwError);
			else
				SetLastErrorDescriptionEx("%s(): sub-process generated an exception (0x%08X)",__func__,dwError);
			return(FALSE);
		}
	}
	else
	{
		dwError = ::GetLastError(); // ERROR_FILE_NOT_FOUND, ERROR_PATH_NOT_FOUND, ERROR_ACCESS_DENIED, ERROR_BAD_EXE_FORMAT, ERROR_SHARING_VIOLATION, etc.
		if(!FileExists(szMartyr))
			dwError = ERROR_INVALID_MODULETYPE;
		SetLastErrorDescriptionEx("%s(): unable to create process (%ld)",__func__,dwError);
	}

	return(FALSE);
#endif

	return(FALSE); // NON arriva mai qui
}
#else /* _MARTYRDOM non definito */
/*
	Load()
*/
BOOL CNexgenIPL::Load(LPCSTR lpcszFileName,DWORD& dwError)
{
	// qui NON bisogna controllare se l'oggetto immagine e' valido perche' lo sta caricando da file
	//if(!IsValid(__func__))
	//	return(FALSE);

	if(!IsSupportedFormat(lpcszFileName))
		return(FALSE);

	Unload();

	__try {

		if(m_NexgenObject.Load(lpcszFileName)!=-1L)
			m_pImage = m_NexgenObject.GetObjectDataPtr();
		else
			m_pImage = NULL;
	}
	__except(dwError = GetExceptionCode(),EXCEPTION_EXECUTE_HANDLER) {
		m_pImage = NULL;
		SetLastErrorDescriptionEx("%s(): an unexpected exception (0x%08X) has occurred while loading: %s",__func__,dwError,lpcszFileName);
	}

	// MANTENERE UGUALE a versione per _MARTYRDOM definito (vedi sopra)
	if(m_pImage)
	{
		// salva il nome del file caricato ed imposta il formato in base all'estensione del file
		strcpyn(m_szFileName,lpcszFileName,sizeof(m_szFileName));
		memset(m_szFormat,'\0',sizeof(m_szFormat));
		char* p = strrchr(m_szFileName,'.');
		if(p)
		{
			p++;
			if(p && *p)
			{
				strcpyn(m_szFormat,p,sizeof(m_szFormat));
				strupr(m_szFormat);
			}
		}

		// registra il tipo immagine
		m_InfoHeader.type = NULL_PICTURE;
		if(*m_szFormat)
		{
			if(stricmp(m_szFormat,"BMP")==0 || stricmp(m_szFormat,"DIB")==0 || stricmp(m_szFormat,"RLE")==0)
				m_InfoHeader.type = BMP_PICTURE;
			else if(stricmp(m_szFormat,"GIF")==0)
				m_InfoHeader.type = GIF_PICTURE;
			else if(stricmp(m_szFormat,"ICO") == 0)
				m_InfoHeader.type = ICO_PICTURE;
			else if(stricmp(m_szFormat,"JPG")==0 || stricmp(m_szFormat,"JPEG")==0 || stricmp(m_szFormat,"JPE")==0 || stricmp(m_szFormat,"JIF")==0 || stricmp(m_szFormat,"JFIF")==0)
				m_InfoHeader.type = JPEG_PICTURE;
			else if(stricmp(m_szFormat,"PCX")==0)
				m_InfoHeader.type = PCX_PICTURE;
			else if(stricmp(m_szFormat,"PGM")==0)
				m_InfoHeader.type = PGM_PICTURE;
			else if(stricmp(m_szFormat,"PNG")==0)
				m_InfoHeader.type = PNG_PICTURE;
			else if(stricmp(m_szFormat,"PPM")==0)
				m_InfoHeader.type = PPM_PICTURE;
			else if(stricmp(m_szFormat,"TGA")==0)
				m_InfoHeader.type = TGA_PICTURE;
			else if(stricmp(m_szFormat,"TIFF")==0 || stricmp(m_szFormat,"TIF")==0)
				m_InfoHeader.type = TIFF_PICTURE;
		}

		// salva i bpp originali dell'immagine
		m_InfoHeader.bppOriginal = m_pImage->GetBitsPerPixel();

		// dimensione file
		HANDLE hFile = ::CreateFile(m_szFileName,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
		if(hFile!=INVALID_HANDLE_VALUE)
		{
			m_InfoHeader.filesize = ::GetFileSize(hFile,NULL);
			::CloseHandle(hFile);
		}
		else
		{
			dwError = ::GetLastError();
			SetLastErrorDescriptionEx("%s(): unable to open %s (%ld)",__func__,m_szFileName,dwError);
			return(FALSE);
		}

		// aggiorna il resto dell'header
		UpdateHeaderInfo();
	}

	return(m_pImage!=NULL);
}
#endif /* _MARTYRDOM */

/*
	Unload()
*/
BOOL CNexgenIPL::Unload(void)
{
	if(m_pImage)
	{
		// nome file relativo all'immagine
		memset(m_szFileName,'\0',sizeof(m_szFileName));
		memset(m_szFormat,'\0',sizeof(m_szFormat));

		// header metadati
		memset(&m_InfoHeader,'\0',sizeof(IMAGEHEADERINFO));
		m_InfoHeader.type		= NULL_PICTURE;
		m_InfoHeader.xres		= -1;
		m_InfoHeader.yres		= -1;
		m_InfoHeader.restype	= -1;
		m_InfoHeader.compression= -1;
		m_InfoHeader.quality	= -1;
		m_InfoHeader.filesize	= (unsigned long)-1L;
		m_InfoHeader.width		= (unsigned long)-1L;
		m_InfoHeader.height		= (unsigned long)-1L;
		m_InfoHeader.memused	= 0;

		// puntatore all'oggetto immagine
		if (m_NexgenObject.GetObjectDataPtr())
			m_NexgenObject.GetObjectDataPtr()->DeleteObject();
		m_pImage = NULL;
	}

	return(m_pImage ? FALSE : TRUE);
}

/*
	Save()

	Salva l'immagine corrente con il nome file e nel formato specificati.
	Passare il nome (completo di estensione) con cui salvare il file ed il formato, 
	da indicare in modo diretto (senza il punto, vedi sotto).

	I formati supportati sono quelli definiti in BTDefines.h:
	BMP, CUT, GIF, JP2, JPEG, MNG, PCX, PNG, PPM, PGM, PBM, RAS, TGA, TIFF.
*/
#ifdef _MARTYRDOM
BOOL CNexgenIPL::Save(LPCSTR lpcszFileName,LPCSTR lpcszFormat,DWORD dwFlags)
{
	// controlla che l'oggetto immagine sia valido
	// se dwFlags!=0 la chiamata proviene dal martire, quindi NON deve
	// controllare perche' l'oggetto e' vuoto, verra' impostato con il
	// DIB ricevuto via shared memory
	if(dwFlags==0)
		if(!IsValid(__func__))
			return(FALSE);

#if defined(_MARTYR) /* e' il martire */

	// il profeta, che ha caricato il file, ricava il DIB dell'immagine e lo condivide
	// nel segmento di shared memory, quindi qui il martire legge il DIB e verifica se
	// puo' salvarlo nel formato specificato
	#ifdef TRACE_MARTYRDOM
		printf("MARTYR: starting Save()\n");
	#endif

	HANDLE hMapFile = NULL;
    void* pSharedMem = NULL;
	HDIB hDib = NULL;

	// apre la memoria condivisa
	hMapFile = OpenFileMapping(FILE_MAP_READ,FALSE,"NexgenIPL");
	if(!hMapFile)
		return(FALSE);
	#ifdef TRACE_MARTYRDOM
		printf("MARTYR: OpenFileMapping OK\n");
	#endif

	// mappa la memoria
	pSharedMem = MapViewOfFile(hMapFile,FILE_MAP_READ,0,0,0);
    if(!pSharedMem)
	{
		CloseHandle(hMapFile);
		return(FALSE);
	}
	#ifdef TRACE_MARTYRDOM
		printf("MARTYR: MapViewOfFile OK\n");
	#endif

	// il DIB inizia dopo i primi 4 byte x la dimensione
	DWORD dwDIBSize = *(DWORD*)pSharedMem;
	void* pDIBData = (BYTE*)pSharedMem+sizeof(DWORD);
	#ifdef TRACE_MARTYRDOM
		printf("MARTYR: dwDIBSize: %ld\n",dwDIBSize);
	#endif

	// alloca memoria per l'handle HDIB
	hDib = (HDIB)GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, dwDIBSize);
	if(!hDib)
	{
		UnmapViewOfFile(pSharedMem);
		CloseHandle(hMapFile);
		return(FALSE);
	}
	#ifdef TRACE_MARTYRDOM
		printf("MARTYR: GlobalAlloc OK\n");
	#endif
    
	// blocca l'handle per ottenere il puntatore
	void* pDibCopy = GlobalLock(hDib);
	if(!pDibCopy)
	{
		GlobalFree(hDib);
		UnmapViewOfFile(pSharedMem);
		CloseHandle(hMapFile);
		return(FALSE);
	}
	#ifdef TRACE_MARTYRDOM
		printf("MARTYR: GlobalLock OK\n");
	#endif

	// copia il DIB dalla memoria condivisa al blocco appena allocato
	CopyMemory(pDibCopy,pDIBData,dwDIBSize);
    
	// sblocca l'handle (ora puo' essere usato da SetDIB)
	GlobalUnlock(hDib);
	#ifdef TRACE_MARTYRDOM
		printf("MARTYR: GlobalUnlock OK\n");
	#endif

	BOOL bSaved = FALSE;
	if(this->SetDIB(hDib))
	{
		MirrorVertical(); //$

		#ifdef TRACE_MARTYRDOM
			printf("MARTYR: SetDIB OK\n");
		#endif
		long lCodecId = this->m_NexgenObject.GetCodecIdFromExtension(lpcszFormat,BTCODECTYPE_ENCODER);
		if(lCodecId!=-1L)
		{
			BTCString bctStrFileName = lpcszFileName;

			// qui non puo' usare la __try/__except
			bSaved = this->m_NexgenObject.Save(bctStrFileName,lCodecId)==BT_S_OK;
			#ifdef TRACE_MARTYRDOM
				printf("MARTYR: Save result: %s\n",bSaved ? "OK" : "FAIL");
			#endif
			if(!bSaved)
				SetLastErrorDescriptionEx("%s(): unable to save: %s",__func__,lpcszFileName);
		}
	}
	else
	{
		#ifdef TRACE_MARTYRDOM
			printf("MARTYR: SetDIB FAIL\n");
		#endif
		GlobalFree(hDib); // libera se fallisce
	}

	// pulizia memoria condivisa
	UnmapViewOfFile(pSharedMem);
	CloseHandle(hMapFile);

	#ifdef TRACE_MARTYRDOM
		printf("MARTYR: ending Save()\n");
	#endif

	return(bSaved);

#elif defined(_PROPHET) /* e' il profeta */

	// martirizza
	// carica il DIB nel segmento di shared memory e lancia il martire per verificare
	// se il file puo' essere salvato nel formato specificato senza generare eccezioni
	DWORD dwError = 0L;
	char szCmd[MAX_CMDLINE+1] = {0};
	char szMartyr[_MAX_PATH+1] = {0};

  #ifdef DEBUG
	// anche se si imposta la directory di lavoro su C:\WCHG, quando si esegue dal
	// debugger, la GetModuleFileName() chiamata da GetMartyrName() restituisce
	// "C:\DEV\wchg\Debug\Calimero.exe", quindi inchioda il nome, perche' il progetto
	// per Titti.exe copia il file in C:\WCHG a fine compilazione
	// in modo Release si suppone sara' sempre la stessa directory (es. C:\WCHG)
	snprintf(szMartyr,sizeof(szMartyr),"\"C:\\WCHG\\%s\"",MARTYR_EXECUTABLE_NAME);
	snprintf(szCmd,sizeof(szCmd),"\"%s\" \"1:%s:%s\"",MARTYR_EXECUTABLE_NAME,lpcszFileName,lpcszFormat);
  #else
	GetMartyrName(szMartyr,sizeof(szMartyr));
	snprintf(szCmd,sizeof(szCmd),"\"%s\" \"1:%s:%s\"",szMartyr,lpcszFileName,lpcszFormat);
  #endif

	#ifdef TRACE_MARTYRDOM
		printf("PROPHET: now running: %s\n",szCmd);
	#endif

	HANDLE hMapFile = NULL;
	void* pSharedMem = NULL;
	void* pDIBBuffer = NULL;
	UINT nDIBSize = 0;
	HDIB hDib = NULL;

	// ottiene l'handle del DIB dell'immagine corrente
	hDib = GetDIB(&nDIBSize);
	if(!hDib)
		return(FALSE);
	if(nDIBSize!=GlobalSize(hDib))
		return(FALSE);
	#ifdef TRACE_MARTYRDOM
		printf("PROPHET: got a DIB, size: %ld\n",nDIBSize);
	#endif

	// blocca l'handle per ottenere un puntatore valido
	pDIBBuffer = GlobalLock(hDib);
	if(!pDIBBuffer)
		return(FALSE);
	#ifdef TRACE_MARTYRDOM
		printf("PROPHET: GlobalLock OK\n");
	#endif

	// crea la memoria condivisa
	hMapFile = CreateFileMapping(INVALID_HANDLE_VALUE,NULL,PAGE_READWRITE,0,nDIBSize,"NexgenIPL");
	if(!hMapFile)
	{
		GlobalUnlock(hDib);
		return(FALSE);
	}
	#ifdef TRACE_MARTYRDOM
		printf("PROPHET: CreateFileMapping OK\n");
	#endif

	// la mappa
	pSharedMem = MapViewOfFile(hMapFile,FILE_MAP_ALL_ACCESS,0,0,nDIBSize);
    if(!pSharedMem)
	{
		CloseHandle(hMapFile);
		GlobalUnlock(hDib);
		return(FALSE);
	}
	#ifdef TRACE_MARTYRDOM
		printf("PROPHET: MapViewOfFile OK\n");
	#endif

	// copia il DIB dalla memoria bloccata alla memoria condivisa
	CopyMemory(pSharedMem,&nDIBSize,sizeof(DWORD));
	CopyMemory((BYTE*)pSharedMem+sizeof(DWORD),pDIBBuffer,nDIBSize);
	#ifdef TRACE_MARTYRDOM
		printf("PROPHET: CopyMemory OK\n");
	#endif
    
	// sblocca l'handle, NON liberarlo dato che e' proprieta' di CImage
	GlobalUnlock(hDib);
    
	STARTUPINFO si = {sizeof(si)};
	PROCESS_INFORMATION pi = {0};
	BOOL bSaved = FALSE;

	#ifdef TRACE_MARTYRDOM
		printf("PROPHET: executing the martyr\n");
	#endif
	if(CreateProcess(NULL,szCmd,NULL,NULL,FALSE,0/*CREATE_NO_WINDOW*/,NULL,NULL,&si,&pi))
	{
		DWORD dwExitCode = 65L;
		DWORD dwWaitResult = ::WaitForSingleObject(pi.hProcess,3000);
		if(dwWaitResult==WAIT_TIMEOUT) // il proxy e' rimasto appeso, lo abbatte
		{
			TerminateProcess(pi.hProcess,dwExitCode);
			#ifdef TRACE_MARTYRDOM
				printf("PROPHET: martyr timeout\n");
			#endif
		}
		else
			GetExitCodeProcess(pi.hProcess,&dwExitCode);

		#ifdef TRACE_MARTYRDOM
			printf("PROPHET: martyr exit code: %ld\n",dwExitCode);
		#endif

		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);

		if(dwExitCode==0L)
		{
			dwError = 0L;
			bSaved = TRUE;
		}
		else
		{
			/*
			0xC0000005	EXCEPTION_ACCESS_VIOLATION
			0xC0000374	STATUS_HEAP_CORRUPTION
			0xC0000094	EXCEPTION_INT_DIVIDE_BY_ZERO
			0xC00000FD	EXCEPTION_STACK_OVERFLOW
			0xC0000409	STATUS_STACK_BUFFER_OVERRUN
			0xC0000135	STATUS_DLL_NOT_FOUND
			[...]
			*/
			dwError = dwExitCode;
			SetLastErrorDescriptionEx("%s(): sub-process generated an exception (0x%08X)",__func__,dwError);
			bSaved = FALSE;
		}
	}
	else
	{
		dwError = ::GetLastError(); // ERROR_FILE_NOT_FOUND, ERROR_PATH_NOT_FOUND, ERROR_ACCESS_DENIED, ERROR_BAD_EXE_FORMAT, ERROR_SHARING_VIOLATION, etc.
		if(!FileExists(szMartyr))
			dwError = ERROR_INVALID_MODULETYPE;
		SetLastErrorDescriptionEx("%s(): unable to create process (%ld)",__func__,dwError);
		bSaved = FALSE;
		#ifdef TRACE_MARTYRDOM
			printf("PROPHET: CreateProcess FAIL\n");
		#endif
	}

	if(pSharedMem)
		UnmapViewOfFile(pSharedMem);
    
	if(hMapFile)
		CloseHandle(hMapFile);

	return(bSaved);

#endif

	return(FALSE); // NON arriva mai qui
}
#else /* _MARTYRDOM non definito */
/*
	Save()
*/
BOOL CNexgenIPL::Save(LPCSTR lpcszFileName,LPCSTR lpcszFormat,DWORD)
{
	BOOL bSaved = FALSE;

	if(m_pImage)
	{
		long lCodecId = m_NexgenObject.GetCodecIdFromExtension(lpcszFormat[0]=='.' ? lpcszFormat+1 : lpcszFormat,BTCODECTYPE_ENCODER);
		if(lCodecId!=-1L)
		{
			BTCString bctStrFileName = lpcszFileName;
			bSaved = m_NexgenObject.Save(bctStrFileName,lCodecId)==BT_S_OK;
		}
	}

	if(bSaved)
		CImage::Flush(lpcszFileName);

	return(bSaved);
}
#endif /* _MARTYRDOM */

/*
	SaveTIFF()
*/
BOOL CNexgenIPL::SaveTIFF(LPCSTR lpcszInputFile)
{
	// controlla che l'oggetto immagine sia valido
	if(!IsValid(__func__))
		return(FALSE);

	DWORD dwError = 0L;

	__try {

		BOOL bRet = FALSE;

		TIFF* tiff = TIFFOpen(lpcszInputFile,"wb");
		if(tiff)
		{
			CImage* pImage = this;
			UINT iBitsPerPixel = pImage->GetBPP();
			UINT nWidth = pImage->GetWidth();
			UINT nHeight = pImage->GetHeight();

			TIFFSetField(tiff,TIFFTAG_BITSPERSAMPLE,iBitsPerPixel);
			TIFFSetField(tiff,TIFFTAG_SAMPLESPERPIXEL,1);
			TIFFSetField(tiff,TIFFTAG_COMPRESSION,pImage->GetCompression());           

			TIFFSetField(tiff,TIFFTAG_IMAGEWIDTH,nWidth);
			TIFFSetField(tiff,TIFFTAG_IMAGELENGTH,nHeight);

			TIFFSetField(tiff,TIFFTAG_XRESOLUTION,(float)pImage->GetXRes());
			TIFFSetField(tiff,TIFFTAG_YRESOLUTION,(float)pImage->GetYRes());
		
			int nURes = pImage->GetURes();
			switch(nURes)
			{
				case RESUNITINCH:
					nURes = RESUNIT_INCH;
					break;
				case RESUNITCENTIMETER:
					nURes = RESUNIT_CENTIMETER;
					break;
				//RESUNITNONE:
				default:
					nURes = RESUNIT_NONE;
					break;
			}
		
			TIFFSetField(tiff,TIFFTAG_RESOLUTIONUNIT,(unsigned short)nURes);
			TIFFSetField(tiff,TIFFTAG_PLANARCONFIG,PLANARCONFIG_CONTIG);
			TIFFSetField(tiff,TIFFTAG_ORIENTATION,ORIENTATION_TOPLEFT);

			// numero pagina, descrizione e colori (mancano)
			#if 0
			TIFFSetField(tiff,TIFFTAG_PAGENAME,???);
			TIFFSetField(tiff,TIFFTAG_IMAGEDESCRIPTION,???);
			TIFFSetField(tiff,TIFFTAG_PRIMARYCHROMATICITIES,???);
			TIFFSetField(tiff,TIFFTAG_WHITEPOINT,???);
			#endif

			unsigned short nPhotometric = (unsigned short)pImage->GetPhotometric();
			switch(nPhotometric)
			{
				case PHOTOMETRICMINISBLACK:
					nPhotometric = PHOTOMETRIC_MINISBLACK;
					break;
				case PHOTOMETRICMINISWHITE:
					nPhotometric = PHOTOMETRIC_MINISWHITE;
					break;
				case PHOTOMETRICPALETTE:
					nPhotometric = PHOTOMETRIC_PALETTE;
					break;
				case PHOTOMETRICRGB:
					nPhotometric = PHOTOMETRIC_RGB;
					break;
			}
		
			TIFFSetField(tiff,TIFFTAG_PHOTOMETRIC,nPhotometric);
		
			UINT nColorData = 0;
			BITMAPINFO* pBitmapInfo = pImage->GetBMI();
		
			if(pBitmapInfo && pBitmapInfo->bmiHeader.biBitCount <= 8)
				nColorData = 1 << pBitmapInfo->bmiHeader.biBitCount;

			if(nPhotometric==PHOTOMETRIC_PALETTE && nColorData)
			{
				unsigned short* red = new unsigned short[nColorData];;
				unsigned short* green = new unsigned short[nColorData];
				unsigned short* blue = new unsigned short[nColorData];

				if(red && green && blue)
				{
					register int i;

					for(i = 0; i < (int)nColorData; i++)
						red[i] = pBitmapInfo->bmiColors[i].rgbRed << 8;
					for(i = 0; i < (int)nColorData; i++)
						green[i] = pBitmapInfo->bmiColors[i].rgbGreen << 8;
					for(i = 0; i < (int)nColorData; i++)
						blue[i] = pBitmapInfo->bmiColors[i].rgbBlue << 8;

					TIFFSetField(tiff,TIFFTAG_COLORMAP,red,green,blue);
					delete [] red;
					delete [] green;
					delete [] blue;
				}
			}

			if(pImage->LockData())
			{
				unsigned char* pData = (unsigned char*)pImage->GetPixels();
				register unsigned int i;
				UINT nWidthBytes = pImage->GetBytesWidth();
			
				switch(iBitsPerPixel)
				{
					case 24:
					case 32:
					{
						#define RGBA_RED 2
						#define RGBA_GREEN 1
						#define RGBA_BLUE 0
						#define RGBA_ALPHA 3

						// todo: check whether (r,g,b) components come in the correct order here...
						int iRGBAPixel = iBitsPerPixel/8;
						BYTE* pBuf = new BYTE[iRGBAPixel*nWidth];
						if(pBuf)
						{
							BYTE* pBufAux = (pImage->GetDIBOrder()==-1) ? pData : pData+((nHeight-1)*nWidthBytes); // top-left
							BOOL bAlpha = iRGBAPixel==4 ? TRUE : FALSE;
					
							if(pImage->GetDIBOrder()==-1)
							{
								for(register int l=0; l < (int)nHeight; l++)
								{
									for(register int c=0; c < (int)nWidth; c++)
									{
										pBuf[c * iRGBAPixel + 0] = pBufAux[c * iRGBAPixel + RGBA_RED];
										pBuf[c * iRGBAPixel + 1] = pBufAux[c * iRGBAPixel + RGBA_GREEN];
										pBuf[c * iRGBAPixel + 2] = pBufAux[c * iRGBAPixel + RGBA_BLUE];
										if(bAlpha)
											pBuf[c * iRGBAPixel + 3] = pBufAux[c * iRGBAPixel + RGBA_ALPHA];
									}
								
									TIFFWriteScanline(tiff,pBuf,l,0);
								
									pBufAux += nWidthBytes;
								}
							}
							else
							{
								for(register int l=0; l < (int)nHeight; l++)
								{
									for(register int c=0; c < (int)nWidth; c++)
									{
										pBuf[c * iRGBAPixel + 0] = pBufAux[c * iRGBAPixel + RGBA_RED];
										pBuf[c * iRGBAPixel + 1] = pBufAux[c * iRGBAPixel + RGBA_GREEN];
										pBuf[c * iRGBAPixel + 2] = pBufAux[c * iRGBAPixel + RGBA_BLUE];
										if(bAlpha)
											pBuf[c * iRGBAPixel + 3] = pBufAux[c * iRGBAPixel + RGBA_ALPHA];
									}

									TIFFWriteScanline(tiff,pBuf,l,0);
									pBufAux -= nWidthBytes;
								}
							}

							delete [] pBuf;
						}
					}
					break;

					default:
					{
						if(pImage->GetDIBOrder()==-1) // top-left
						{	
							for(i=0; i < nHeight; i++)
							{
								TIFFWriteScanline(tiff,pData,i,0);
								pData += nWidthBytes;;
							}
						}
						else
						{
							unsigned char* pBuf = pData+((nHeight-1)*nWidthBytes);
							for(i=0; i < nHeight; i++)
							{
								TIFFWriteScanline(tiff,pBuf,i,0);
								pBuf -= nWidthBytes;
							}
						}
					}
					break;

				}

				TIFFWriteDirectory(tiff);
				pImage->UnlockData();
				bRet = TRUE;
			}

			TIFFClose(tiff);
		}

		if(!bRet)
			SetLastErrorDescriptionEx("%s(): unable to save: %s",__func__,lpcszInputFile);

		return(bRet);
	}
	__except(dwError = GetExceptionCode(),EXCEPTION_EXECUTE_HANDLER) {

		SetLastErrorDescriptionEx("%s(): an unexpected exception (0x%08X) has occurred",__func__,dwError);
		return(FALSE);
	}

	return(FALSE);
}

/*
	Stretch()
*/
UINT CNexgenIPL::Stretch(RECT& drawRect,BOOL bAspectRatio/*=TRUE*/)
{
	// controlla che l'oggetto immagine sia valido
	if(!IsValid(__func__))
		return(GDI_ERROR);

	BOOL bStretched = FALSE;

	if(m_pImage->GetBitsPerPixel()!=32)
		ConvertToBPP(32);

	// l'algoritmo si adatta al rettangolo (non esce mai dai bordi) - FIT
	// adatta la dimensione dell'immagine al rettangolo (mantenendo le proporzioni originali)
	float nFactor = 0.0;
	float nWidth = (float)GetWidth();
	float nHeight = (float)GetHeight();

	if(bAspectRatio)
	{
		// l'immagine e' piu' piccola del rettangolo, l'allarga
		if(nHeight < (float)drawRect.bottom)
		{
			nFactor = (float)drawRect.bottom/nHeight;
			if(nFactor > 0.0)
			{
				nHeight *= nFactor;
				nWidth *= nFactor;
			}
		}
		if(nWidth < (float)drawRect.right)
		{
			nFactor = (float)drawRect.right/nWidth;
			if(nFactor > 0.0)
			{
				nHeight *= nFactor;
				nWidth *= nFactor;
			}
		}
	}
	else
	{
		nWidth = (float)drawRect.right;
		nHeight = (float)drawRect.bottom;
	}

	// l'immagine e' piu' grande del rettangolo, la riduce
	// la normalizzazione e' necessaria perche' se l'immagine e' stata allargata sopra una delle
	// dimensioni potrebbe eccedere il rettangolo
	if(nHeight > (float)drawRect.bottom)
	{
		nFactor = nHeight/(float)drawRect.bottom;
		if(nFactor > 0.0)
		{
			nHeight /= nFactor;
			nWidth /= nFactor;
		}
	}
	if(nWidth > (float)drawRect.right)
	{
		nFactor = nWidth/(float)drawRect.right;
		if(nFactor > 0.0)
		{
			nHeight /= nFactor;
			nWidth /= nFactor;
		}
	}

	if((int)nHeight <= 0)
		nHeight = 1;
	if((int)nWidth <= 0)
		nWidth = 1;

	BTCImageData::BTResizeFilter Filter;

	// RESIZE_BOX:
	//Filter = BTCImageData::Box;			// ok, cacca in resize

	// RESIZE_BILINEAR:
	Filter = BTCImageData::Bilinear;	// ok, migliore (default)

	// RESIZE_GAUSSIAN:
	//Filter = BTCImageData::Gaussian;	// cacca totale (blur)

	// RESIZE_HAMMING:
	//Filter = BTCImageData::Hamming;		// ok, cacca in resize

	// RESIZE_BLACKMAN:
	//Filter = BTCImageData::Blackman;	// ok, cacca in resize

	DWORD dwError = 0L;

	__try {

		bStretched = m_pImage->Resize((int)nWidth,(int)nHeight,Filter);
	}
	__except(dwError = GetExceptionCode(),EXCEPTION_EXECUTE_HANDLER) {

		SetLastErrorDescriptionEx("%s(): an unexpected exception (0x%08X) has occurred",__func__,dwError);
		return(GDI_ERROR);
	}

	// aggiorna i metadati dell'header
	if(bStretched)
		bStretched = UpdateHeaderInfo()==NO_ERROR;

	if(!bStretched)
		SetLastErrorDescriptionEx("%s(): unable to stretch the image",__func__);

	return(bStretched ? NO_ERROR : GDI_ERROR);
}

/*
	Blur()
*/
UINT CNexgenIPL::Blur(void)
{
	// controlla che l'oggetto immagine sia valido
	if(!IsValid(__func__))
		return(GDI_ERROR);

	DWORD dwError = 0L;

	__try {

		BOOL bBpp = m_pImage->GetBitsPerPixel()==32;
		if(!bBpp)
			bBpp = ConvertToBPP(32)==NO_ERROR;
		if(bBpp)
		{
			if(m_pImage->Blur())
				return(NO_ERROR);
		}
	}
	__except(dwError = GetExceptionCode(),EXCEPTION_EXECUTE_HANDLER) {

		SetLastErrorDescriptionEx("%s(): an unexpected exception (0x%08X) has occurred",__func__,dwError);
		return(GDI_ERROR);
	}

	SetLastErrorDescriptionEx("%s(): unable to apply the blur filter",__func__);
	return(GDI_ERROR);
}

/*
	Brightness()
*/
UINT CNexgenIPL::Brightness(void)
{
	// controlla che l'oggetto immagine sia valido
	if(!IsValid(__func__))
		return(GDI_ERROR);

	int nFactor = 0;
	double nValue = 0;
	if(GetFilterParams("Brightness",nValue))
		nFactor = (int)nValue;
	else
	{
		SetLastErrorDescriptionEx("%s(): ERROR_INVALID_PARAMETER",__func__);
		return(ERROR_INVALID_PARAMETER);
	}

	DWORD dwError = 0L;

	__try {

		if(m_pImage->AdjustBrightness(nFactor))
			return(NO_ERROR);
	}
	__except(dwError = GetExceptionCode(),EXCEPTION_EXECUTE_HANDLER) {

		SetLastErrorDescriptionEx("%s(): an unexpected exception (0x%08X) has occurred",__func__,dwError);
		return(GDI_ERROR);
	}

	SetLastErrorDescriptionEx("%s(): unable to apply the brightness filter",__func__);
	return(GDI_ERROR);
}

/*
	Contrast()
*/
UINT CNexgenIPL::Contrast(void)
{
	// controlla che l'oggetto immagine sia valido
	if(!IsValid(__func__))
		return(GDI_ERROR);

	int nFactor = 0;
	double nValue = 0;
	if(GetFilterParams("Contrast",nValue))
		nFactor = (int)nValue;
	else
	{
		SetLastErrorDescriptionEx("%s(): ERROR_INVALID_PARAMETER",__func__);
		return(ERROR_INVALID_PARAMETER);
	}

	DWORD dwError = 0L;

	__try {

		if(m_pImage->AdjustContrast(nFactor))
			return(NO_ERROR);
	}
	__except(dwError = GetExceptionCode(),EXCEPTION_EXECUTE_HANDLER) {

		SetLastErrorDescriptionEx("%s(): an unexpected exception (0x%08X) has occurred",__func__,dwError);
		return(GDI_ERROR);
	}

	SetLastErrorDescriptionEx("%s(): unable to apply the contrast filter",__func__);
	return(GDI_ERROR);
}

/*
	EdgeEnhance()
*/
UINT CNexgenIPL::EdgeEnhance(void)
{
	// controlla che l'oggetto immagine sia valido
	if(!IsValid(__func__))
		return(GDI_ERROR);

	DWORD dwError = 0L;

	__try {

		BOOL bBpp = m_pImage->GetBitsPerPixel() >= 24;
		if(!bBpp)
			bBpp = ConvertToBPP(32)==NO_ERROR;
		if(bBpp)
			if(m_pImage->EdgeEnhance())
				return(NO_ERROR);
	}
	__except(dwError = GetExceptionCode(),EXCEPTION_EXECUTE_HANDLER) {

		SetLastErrorDescriptionEx("%s(): an unexpected exception (0x%08X) has occurred",__func__,dwError);
		return(GDI_ERROR);
	}

	SetLastErrorDescriptionEx("%s(): unable to apply the edge enhance filter",__func__);
	return(GDI_ERROR);
}

/*
	Emboss()
*/
UINT CNexgenIPL::Emboss(void)
{
	// controlla che l'oggetto immagine sia valido
	if(!IsValid(__func__))
		return(GDI_ERROR);

	DWORD dwError = 0L;

	__try {

		BOOL bBpp = m_pImage->GetBitsPerPixel() >= 24;
		if(!bBpp)
			bBpp = ConvertToBPP(32)==NO_ERROR;
		if(bBpp)
			if(m_pImage->Emboss())
				return(NO_ERROR);
	}
	__except(dwError = GetExceptionCode(),EXCEPTION_EXECUTE_HANDLER) {

		SetLastErrorDescriptionEx("%s(): an unexpected exception (0x%08X) has occurred",__func__,dwError);
		return(GDI_ERROR);
	}

	SetLastErrorDescriptionEx("%s(): unable to apply the emboss filter",__func__);
	return(GDI_ERROR);
}

/*
	FindEdge()
*/
UINT CNexgenIPL::FindEdge(void)
{
	// controlla che l'oggetto immagine sia valido
	if(!IsValid(__func__))
		return(GDI_ERROR);

	int nFactor = 0;
	double nValue = 0;
	if(GetFilterParams("FindEdge",nValue))
		nFactor = (int)nValue;
	else
	{
		SetLastErrorDescriptionEx("%s(): ERROR_INVALID_PARAMETER",__func__);
		return(ERROR_INVALID_PARAMETER);
	}

	DWORD dwError = 0L;

	__try {

		BOOL bBpp = m_pImage->GetBitsPerPixel() >= 24;
		if(!bBpp)
			bBpp = ConvertToBPP(32)==NO_ERROR;
		if(bBpp)
			if(m_pImage->FindEdge(nFactor))
				return(NO_ERROR);
	}
	__except(dwError = GetExceptionCode(),EXCEPTION_EXECUTE_HANDLER) {

		SetLastErrorDescriptionEx("%s(): an unexpected exception (0x%08X) has occurred",__func__,dwError);
		return(GDI_ERROR);
	}

	SetLastErrorDescriptionEx("%s(): unable to apply the find edge filter",__func__);
	return(GDI_ERROR);
}

/*
	GammaCorrection()
*/
UINT CNexgenIPL::GammaCorrection(void)
{
	// controlla che l'oggetto immagine sia valido
	if(!IsValid(__func__))
		return(GDI_ERROR);

	int nFactor = 0;
	double nValue = 0;
	if(GetFilterParams("GammaCorrection",nValue))
		nFactor = (int)nValue;
	else
	{
		SetLastErrorDescriptionEx("%s(): ERROR_INVALID_PARAMETER",__func__);
		return(ERROR_INVALID_PARAMETER);
	}

	DWORD dwError = 0L;

	__try {

		if(m_pImage->AdjustGamma(nFactor,nFactor,nFactor))
			return(NO_ERROR);
	}
	__except(dwError = GetExceptionCode(),EXCEPTION_EXECUTE_HANDLER) {

		SetLastErrorDescriptionEx("%s(): an unexpected exception (0x%08X) has occurred",__func__,dwError);
		return(GDI_ERROR);
	}

	SetLastErrorDescriptionEx("%s(): unable to apply the gamma correction filter",__func__);
	return(GDI_ERROR);
}

/*
	Grayscale()
*/
UINT CNexgenIPL::Grayscale(void)
{
	// controlla che l'oggetto immagine sia valido
	if(!IsValid(__func__))
		return(GDI_ERROR);

	// non converte un immagine che gia' e'/sembra grayscale, restituisce errore
	if(IsTrueGrayscale() || IsVisualGrayscaleStatistical(VISUAL_GRAYSCALE_PARAMS))
	{
		SetLastErrorDescriptionEx("%s(): image is already grayscale, filter will not be applied",__func__);
		return(GDI_ERROR);
	}

	DWORD dwError = 0L;

	__try {

		if(m_pImage->Grayscale())
		{
			UpdateHeaderInfo();
			return(NO_ERROR);
		}
	}
	__except(dwError = GetExceptionCode(),EXCEPTION_EXECUTE_HANDLER) {

		SetLastErrorDescriptionEx("%s(): an unexpected exception (0x%08X) has occurred",__func__,dwError);
		return(GDI_ERROR);
	}

	SetLastErrorDescriptionEx("%s(): unable to apply the grayscale filter",__func__);
	return(GDI_ERROR);
}

/*
	Hue()
*/
UINT CNexgenIPL::Hue(void)
{
	// controlla che l'oggetto immagine sia valido
	if(!IsValid(__func__))
		return(GDI_ERROR);

	// non applica a un immagine che e'/sembra grayscale, restituisce errore
	if(IsTrueGrayscale() || IsVisualGrayscaleStatistical(VISUAL_GRAYSCALE_PARAMS))
	{
		SetLastErrorDescriptionEx("%s(): image is already grayscale, filter will not be applied",__func__);
		return(GDI_ERROR);
	}

	int nFactor = 0;
	double nValue = 0;
	if(GetFilterParams("Hue",nValue))
		nFactor = (int)nValue;
	else
	{
		SetLastErrorDescriptionEx("%s(): ERROR_INVALID_PARAMETER",__func__);
		return(ERROR_INVALID_PARAMETER);
	}

	DWORD dwError = 0L;

	__try {

		if(m_pImage->AdjustHue(nFactor))
			return(NO_ERROR);
	}
	__except(dwError = GetExceptionCode(),EXCEPTION_EXECUTE_HANDLER) {

		SetLastErrorDescriptionEx("%s(): an unexpected exception (0x%08X) has occurred",__func__,dwError);
		return(GDI_ERROR);
	}

	SetLastErrorDescriptionEx("%s(): unable to apply the hue filter",__func__);
	return(GDI_ERROR);
}

/*
	Median()

	Il filtro Median si usa sopratutto per la riduzione del rumore preservando i bordi.
	Per ogni pixel dell'immagine, il filtro:
	- prende una finestra quadrata (kernel) di dimensioni N×N centrata sul pixel
	- raccoglie tutti i valori dei pixel in quella finestra
	- ordina questi valori (separatamente per ogni canale R, G, B)
	- sostituisce il valore originale del pixel con il valore mediano della lista ordinata

	Nella pratica gli effetti sono: eliminazione dei pixel bianchi/neri isolati e migliore
	preservazione dei bordi, per cui si suele usare in:
	- fotografia digitale, per rimuovere grana dalle foto scarse
	- medical imaging, per pulire immagini MRI/CT preservando dettagli
	- OCR preprocessing, per rimuovere punti isolati per migliorare riconoscimento testo
	- restauro foto, per eliminare graffi e polvere dalle foto scannerizzate

	Qui, sorprendentemente, NexgenIPL produce l'effetto opposto, ossia una immagine
	completamente pixelata, come se si trattasse di un filtro Mosaic, per cui lo elimina 
	ed usa la fallback provvista da CImage.
*/
/*
UINT CNexgenIPL::Median(void)
{
}
*/

/*
	MirrorHorizontal()
*/
UINT CNexgenIPL::MirrorHorizontal(void) 
{
	// controlla che l'oggetto immagine sia valido
	if(!IsValid(__func__))
		return(GDI_ERROR);

	DWORD dwError = 0L;

	__try {

		if(m_pImage->Mirror())
			return(NO_ERROR);
	}
	__except(dwError = GetExceptionCode(),EXCEPTION_EXECUTE_HANDLER) {

		SetLastErrorDescriptionEx("%s(): an unexpected exception (0x%08X) has occurred",__func__,dwError);
		return(GDI_ERROR);
	}

	SetLastErrorDescriptionEx("%s(): unable to apply the mirror horiz. filter",__func__);
	return(GDI_ERROR);
}

/*
	MirrorVertical()
*/
UINT CNexgenIPL::MirrorVertical(void) 
{
	// controlla che l'oggetto immagine sia valido
	if(!IsValid(__func__))
		return(GDI_ERROR);

	DWORD dwError = 0L;

	__try {

		if(m_pImage->Flip())
			return(NO_ERROR);
	}
	__except(dwError = GetExceptionCode(),EXCEPTION_EXECUTE_HANDLER) {

		SetLastErrorDescriptionEx("%s(): an unexpected exception (0x%08X) has occurred",__func__,dwError);
		return(GDI_ERROR);
	}

	SetLastErrorDescriptionEx("%s(): unable to apply the mirror vert. filter",__func__);
	return(GDI_ERROR);
}

/*
	Negate()
*/
UINT CNexgenIPL::Negate(void)
{
	// controlla che l'oggetto immagine sia valido
	if(!IsValid(__func__))
		return(GDI_ERROR);

	DWORD dwError = 0L;

	__try {

		BOOL bBpp = m_pImage->GetBitsPerPixel()==32;
		if(!bBpp)
			bBpp = ConvertToBPP(32)==NO_ERROR;
		if(bBpp)
		{
			if(m_pImage->Negate())
				return(NO_ERROR);
		}
	}
	__except(dwError = GetExceptionCode(),EXCEPTION_EXECUTE_HANDLER) {

		SetLastErrorDescriptionEx("%s(): an unexpected exception (0x%08X) has occurred",__func__,dwError);
		return(GDI_ERROR);
	}

	SetLastErrorDescriptionEx("%s(): unable to apply the negate filter",__func__);
	return(GDI_ERROR);
}

/*
	Posterize()
*/
UINT CNexgenIPL::Posterize(void)
{
	// controlla che l'oggetto immagine sia valido
	if(!IsValid(__func__))
		return(GDI_ERROR);

	int nFactor = 0;
	double nValue = 0;
	if(GetFilterParams("Posterize",nValue))
		nFactor = (int)nValue;
	else
	{
		SetLastErrorDescriptionEx("%s(): ERROR_INVALID_PARAMETER",__func__);
		return(ERROR_INVALID_PARAMETER);
	}

	DWORD dwError = 0L;

	__try {

		if(m_pImage->Posterize(nFactor))
		{
			UpdateHeaderInfo();    
			return(NO_ERROR);
		}
	}
	__except(dwError = GetExceptionCode(),EXCEPTION_EXECUTE_HANDLER) {

		SetLastErrorDescriptionEx("%s(): an unexpected exception (0x%08X) has occurred",__func__,dwError);
		return(GDI_ERROR);
	}

	SetLastErrorDescriptionEx("%s(): unable to apply the posterize filter",__func__);
	return(GDI_ERROR);
}

/*
	Rotate90Left()
*/
UINT CNexgenIPL::Rotate90Left(void)
{
	// controlla che l'oggetto immagine sia valido
	if(!IsValid(__func__))
		return(GDI_ERROR);

	DWORD dwError = 0L;

	__try {

		if(m_pImage->Rotate(90,BTCImageData::Left))
		{
			UpdateHeaderInfo();    
			return(NO_ERROR);
		}
	}
	__except(dwError = GetExceptionCode(),EXCEPTION_EXECUTE_HANDLER) {

		SetLastErrorDescriptionEx("%s(): an unexpected exception (0x%08X) has occurred",__func__,dwError);
		return(GDI_ERROR);
	}

	SetLastErrorDescriptionEx("%s(): unable to apply the rotate 90 left filter",__func__);
	return(GDI_ERROR);
}

/*
	Rotate90Right()
*/
UINT CNexgenIPL::Rotate90Right(void)
{
	// controlla che l'oggetto immagine sia valido
	if(!IsValid(__func__))
		return(GDI_ERROR);

	DWORD dwError = 0L;

	__try {

		if(m_pImage->Rotate(90,BTCImageData::Right))
		{
			UpdateHeaderInfo();    
			return(NO_ERROR);
		}
	}
	__except(dwError = GetExceptionCode(),EXCEPTION_EXECUTE_HANDLER) {

		SetLastErrorDescriptionEx("%s(): an unexpected exception (0x%08X) has occurred",__func__,dwError);
		return(GDI_ERROR);
	}

	SetLastErrorDescriptionEx("%s(): unable to apply the rotate 90 right filter",__func__);
	return(GDI_ERROR);
}

/*
	Rotate180()
*/
UINT CNexgenIPL::Rotate180(void)
{
	// controlla che l'oggetto immagine sia valido
	if(!IsValid(__func__))
		return(GDI_ERROR);

	DWORD dwError = 0L;

	__try {

		if(m_pImage->Rotate(180,BTCImageData::Right))
		{
			UpdateHeaderInfo();    
			return(NO_ERROR);
		}
	}
	__except(dwError = GetExceptionCode(),EXCEPTION_EXECUTE_HANDLER) {

		SetLastErrorDescriptionEx("%s(): an unexpected exception (0x%08X) has occurred",__func__,dwError);
		return(GDI_ERROR);
	}

	SetLastErrorDescriptionEx("%s(): unable to apply the rotate 180 filter",__func__);
	return(GDI_ERROR);
}

/*
	Saturation()
*/
UINT CNexgenIPL::Saturation(void)
{
	// controlla che l'oggetto immagine sia valido
	if(!IsValid(__func__))
		return(GDI_ERROR);

	DWORD dwError = 0L;
	int nFactor = 0;
	double nValue = 0.0;
	if(GetFilterParams("Saturation", nValue))
		nFactor = (int)nValue;
	else
	{
		SetLastErrorDescriptionEx("%s(): ERROR_INVALID_PARAMETER",__func__);
		return(ERROR_INVALID_PARAMETER);
	}

	__try {

		BOOL bSuccess = m_pImage->AdjustSaturation(nFactor);
		if(!bSuccess)
			bSuccess = m_pImage->AdjustHLS(0,0,nFactor); // hue=0 (nessun cambio), lightness=0 (nessun cambio), saturation=nFactor
		if(bSuccess)
			return(NO_ERROR);
	}
	__except(dwError = GetExceptionCode(),EXCEPTION_EXECUTE_HANDLER) {

		SetLastErrorDescriptionEx("%s(): an unexpected exception (0x%08X) has occurred",__func__,dwError);
		return(GDI_ERROR);
	}

	SetLastErrorDescriptionEx("%s(): unable to apply the saturation filter",__func__);
	return(GDI_ERROR);
}

/*
	Sharpen()
*/
UINT CNexgenIPL::Sharpen(void)
{
	// controlla che l'oggetto immagine sia valido
	if(!IsValid(__func__))
		return(GDI_ERROR);

	DWORD dwError = 0L;

	__try {

		if(m_pImage->Sharpen()) // occhio: fallisce con alcuni files anche cambiando bpp
			return(NO_ERROR);
	}
	__except(dwError = GetExceptionCode(),EXCEPTION_EXECUTE_HANDLER) {

		SetLastErrorDescriptionEx("%s(): an unexpected exception (0x%08X) has occurred",__func__,dwError);
		return(GDI_ERROR);
	}

	SetLastErrorDescriptionEx("%s(): unable to apply the sharpen filter",__func__);
	return(GDI_ERROR);
}

/*
	IsValid()
*/
BOOL CNexgenIPL::IsValid(LPCSTR lpcszFunctionName)
{
	if(!m_pImage)
	{
		char buffer[128] = {0};
		snprintf(buffer,sizeof(buffer),"%s(): no image loaded",lpcszFunctionName);
		CImage::SetLastErrorDescription(buffer);
		#ifdef DEBUG
			::MessageBox(NULL,GetLastErrorDescription(),GetLibraryName(),MB_ICONERROR|MB_TASKMODAL|MB_SETFOREGROUND|MB_TOPMOST);
		#endif
	}
	return(m_pImage!=NULL);
}

#endif // HAVE_NEXGENIPL_LIBRARY
