/*$
	CPaintLib.cpp
	Classe derivata per l'interfaccia con la libreria paintlib.
	Luca Piergentili, 01/09/00
	lpiergentili@yahoo.com

	Questo e' un classico esempio (insieme a CNexgenIPL.cpp) di "refactoring" per risuscitare
	una libreria e farla tornare a funzionare in un ambiente ormai completamente alieno (Win64).

	Ad memoriam - Nemo me impune lacessit.

	Note:
	- paintLib converte automaticamente a 32 bpp i files caricati, ad eccezione di TIFF a 16 bit (vedi nota seguente), TIFF 
	  CMYK (non RGB), formati con extra channels o photometric non standard, immagini palettizzate ad alta profondita' o non 
	  standard
	- paintLib non supporta immagini grayscale che abbiano piu' di 8 bpp, puo' caricare tali immagini (ad es. un .tiff con
	  16 bpp) ma poi non puo' salvarlo perche' BMP non supporta nativamente 16 bpp grayscale. I formati BMP supportati sono 
	  tipicamente: 1, 4, 8 bpp (con palette) e 16, 24, 32 bpp RGB (senza palette per grayscale a >8 bpp) 
	- nel progetto della libreria paintlib (NON qui), bisogna disabilitare il flag /GS (C/C++ -> generazione codice) perche' 
	  in caso contrario il codice di controllo relativo della libreria di run-time del C (la __fastfail() in gs_report.c) 
	  tratta il membro 'raiseError' della paintlib come sospetto e genera una eccezione che bypassa completamente il meccanismo
	  try/catch di cui sotto.
*/
#include "env.h"
#include "pragma.h"
#include "macro.h"
#include "window.h"
#include "ImageConfig.h"

#ifdef HAVE_PAINTLIB_LIBRARY

#include "strings.h"
#include "CImage.h"
#include "CPaintLib.h"

#include "traceexpr.h"
//#define _TRACE_FLAG			_TRFLAG_TRACEOUTPUT // opzioni: _TRFLAG_NOTRACE, _TRFLAG_TRACEFILE, _TRFLAG_TRACECONSOLE, _TRFLAG_TRACEOUTPUT, _TRFLAG_TRACEBREAKPOINT
#define _TRACE_FLAG			_TRFLAG_NOTRACE // opzioni: _TRFLAG_NOTRACE, _TRFLAG_TRACEFILE, _TRFLAG_TRACECONSOLE, _TRFLAG_TRACEOUTPUT, _TRFLAG_TRACEBREAKPOINT
#define _TRACE_FLAG_INFO	_TRACE_FLAG
#define _TRACE_FLAG_WARN	_TRACE_FLAG
#define _TRACE_FLAG_ERR		_TRACE_FLAG

/*
	CPaintLib()
*/
CPaintLib::CPaintLib()
{
	// descrizione ultimo errore
	memset(m_szError,'\0',sizeof(m_szError));

	// nome file relativo all'immagine e formato
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
	//m_Image
	m_bIsValid = FALSE;

	// tipi files riconosciuti secondo documentazione ufficiale:
	// "Images can be loaded from BMP, GIF, IFF, JPEG, PCX, PGM, PICT, PNG, PSD, SGI, TGA, TIFF and WMF files and saved in BMP, JPEG, PNG and TIFF formats."
	// mantenere sincronizzato con la determinazione del tipo immagine in Load()
	LPIMAGETYPE p;
#ifdef PL_SUPPORT_PNG
	ADDFILETYPE(PNG_PICTURE,	".png",	"Portable Network Graphics Format ",IMAGE_READ_FLAG|IMAGE_WRITE_FLAG|IMAGE_WEB_FLAG,p)
#endif
#ifdef PL_SUPPORT_JPEG
	ADDFILETYPE(JPEG_PICTURE,	".jpg",	"JPEG file ",						IMAGE_READ_FLAG|IMAGE_WRITE_FLAG|IMAGE_WEB_FLAG,p)
	ADDFILETYPE(JPEG_PICTURE,	".jpeg","JPEG file ",						IMAGE_READ_FLAG|IMAGE_WRITE_FLAG,p)
	ADDFILETYPE(JPEG_PICTURE,	".jpe",	"JPEG file ",						IMAGE_READ_FLAG|IMAGE_WRITE_FLAG,p)
	ADDFILETYPE(JPEG_PICTURE,	".jif",	"JPEG file ",						IMAGE_READ_FLAG|IMAGE_WRITE_FLAG,p)
	ADDFILETYPE(JPEG_PICTURE,	".jfif","JPEG file ",						IMAGE_READ_FLAG|IMAGE_WRITE_FLAG,p)
#endif
#ifdef PL_SUPPORT_GIF
	ADDFILETYPE(GIF_PICTURE,	".gif",	"GIF file ",						IMAGE_READ_FLAG|IMAGE_WEB_FLAG,p)
#endif

#ifdef PL_SUPPORT_BMP
	ADDFILETYPE(BMP_PICTURE,	".bmp",	"BMP file ",						IMAGE_READ_FLAG|IMAGE_WRITE_FLAG,p)
#endif
#ifdef PL_SUPPORT_PCX
	ADDFILETYPE(PCX_PICTURE,	".pcx",	"PCX file ",						IMAGE_READ_FLAG,p)
#endif
#ifdef PL_SUPPORT_PICT
	ADDFILETYPE(PICT_PICTURE,	".pic",	"PIC file ",						IMAGE_READ_FLAG,p)
	ADDFILETYPE(PICT_PICTURE,	".pict","PIC file ",						IMAGE_READ_FLAG,p)
	ADDFILETYPE(PICT_PICTURE,	".pct",	"Macintosh Pict Format ",			IMAGE_READ_FLAG,p)
#endif
#ifdef PL_SUPPORT_PGM
	ADDFILETYPE(PGM_PICTURE,	".pgm",	"Portable Graymap File ",			IMAGE_READ_FLAG,p)
#endif
#ifdef PL_SUPPORT_TGA
	ADDFILETYPE(TGA_PICTURE,	".tga",	"TARGA file ",						IMAGE_READ_FLAG,p)
#endif
#ifdef PL_SUPPORT_TIFF
	ADDFILETYPE(TIFF_PICTURE,	".tif",	"TIFF file ",						IMAGE_READ_FLAG|IMAGE_WRITE_FLAG,p)
	ADDFILETYPE(TIFF_PICTURE,	".tiff","TIFF file ",						IMAGE_READ_FLAG|IMAGE_WRITE_FLAG,p)
#endif

	// range e valori di default per i filtri implementati
	SetFilterParams("Contrast",		-100,100,25,"","",True,&CImageObject::Contrast);
	SetFilterParams("Grayscale",	0,0,0,		"","",True,&CImageObject::Grayscale);
	SetFilterParams("Intensity",	-100,100,50,"","",True,&CImageObject::Intensity);
	SetFilterParams("Negate",		0,0,0,		"","",True,&CImageObject::Negate);
	SetFilterParams("Rotate90Left",	0,0,0,		"","",True,&CImageObject::Rotate90Left);
	SetFilterParams("Rotate90Right",0,0,0,		"","",True,&CImageObject::Rotate90Right);
	SetFilterParams("Rotate180",	0,0,0,		"","",True,&CImageObject::Rotate180);
}

/*
	SetXRes()
*/
void CPaintLib::SetXRes(float nXRes)
{
	// controlla che l'oggetto immagine sia valido
	if(!IsValid(__func__))
		return;

    // crea l'oggetto PLPoint con il nuovo XRes e il vecchio YRes
    PLPoint res((int)nXRes,(int)m_InfoHeader.yres); 
    
    // chiama l'API
    m_Image.SetResolution(res); 

    // aggiorna la classe base (m_InfoHeader.xres)
	CImage::SetXRes(nXRes);
}

/*
	SetYRes()
*/
void CPaintLib::SetYRes(float nYRes)
{
	// controlla che l'oggetto immagine sia valido
	if(!IsValid(__func__))
		return;

    // crea l'oggetto PLPoint con il vecchio XRes e il nuovo YRes
    PLPoint res((int)m_InfoHeader.xres,(int)nYRes);
    
    // chiama l'API
    m_Image.SetResolution(res);
    
    // aggiorna la classe base (m_InfoHeader.yres)
	CImage::SetYRes(nYRes);
}

/*
	GetDPI()
*/
void CPaintLib::GetDPI(float& nXRes,float& nYRes)
{
	// controlla che l'oggetto immagine sia valido
	if(!IsValid(__func__))
		return;

    PLPoint resolution = m_Image.GetResolution();
	nXRes = (float)resolution.x;
	nYRes = (float)resolution.y;
}

/*
	ConvertToBPP()
*/
UINT CPaintLib::ConvertToBPP(UINT nBitsPerPixel,UINT nFlags/*=0*/,RGBQUAD *pPalette/*=NULL*/,UINT nColors/*=0*/)
{
	DWORD dwResult = GDI_ERROR;

	// controlla che l'oggetto immagine sia valido
	if(!IsValid(__func__))
		goto done;

	// gia' sta ai BPP richiesti
    if(nBitsPerPixel==m_Image.GetBitsPerPixel())
	{
        dwResult = NO_ERROR;
		goto done;
	}

	// conversione
	if(nBitsPerPixel > 8)
	{
        // puntatore alla costante statica da PLPixelFormat
        const PLPixelFormat* pfWanted = NULL; 

        switch(nBitsPerPixel)
        {
            case 16: // 16bpp (R5G6B5 e' la più fedele, 5-6-5)
                pfWanted = &PLPixelFormat::R5G6B5; 
                break;
            case 24: // 24bpp (R8G8B8)
                pfWanted = &PLPixelFormat::R8G8B8; 
                break;
            case 32: // 32bpp (X8R8G8B8 e' un formato Win32 DIB comune, senza alpha channel)
                pfWanted = &PLPixelFormat::X8R8G8B8; 
                break;
            default:
				SetLastErrorDescriptionEx("%s(): unexpected BPP value: %d",__func__,nBitsPerPixel);
                goto done;
        }

        // crea la copia convertita
	    PLWinBmp TempImage;
        TempImage.CreateCopy(m_Image,*pfWanted); 
            
        // scambia le immagini
        m_Image = TempImage;
    }
    else // quantizzazione
    {
        // crea l'oggetto di destinazione
        PLWinBmp *pDestBmp = new PLWinBmp(); 

        // crea l'oggetto per il filtro: PLFilterQuantize produce un'immagine a 8bpp
        PLFilterQuantize Filter(PLDTHPAL_MEDIAN,PLDTH_NONE);
        Filter.Apply(&m_Image,pDestBmp);
        m_Image = *pDestBmp; 
        delete pDestBmp;
    }
    
	dwResult = UpdateHeaderInfo();

done:

    return(dwResult);
}

/*
	GetPixel()

	Il vecchio m_Image.GetPixel() non esiste piu'.
	Si usa GetPixel32() e si converte il risultato PLPixel32 in COLORREF.
*/
COLORREF CPaintLib::GetPixel(int x,int y)
{
	// controlla che l'oggetto immagine sia valido
	if(!IsValid(__func__))
		goto done;

	ASSERTEXPR(m_Image.GetBitsPerPixel()==32);

    if(m_Image.GetBitsPerPixel()==32)
    {
        PLPixel32 p = m_Image.GetPixel32(x,y);

        PLBYTE r = p.GetR();
        PLBYTE g = p.GetG();
        PLBYTE b = p.GetB();

        // ricostruisce e restituisce il COLORREF Win32 (0x00BBGGRR)
        return(RGB(r,g,b));
    }

done:

    // per gli altri formati (1/8/16/24 bpp), restituisce 0 (nero)
    return(0);
}

/*
	SetPixel()
*/
void CPaintLib::SetPixel(int x,int y,COLORREF colorref)
{
	// controlla che l'oggetto immagine sia valido
	if(!IsValid(__func__))
		goto done;

	ASSERTEXPR(m_Image.GetBitsPerPixel()==32);

    if(m_Image.GetBitsPerPixel()==32)
    {
        // estrae i componenti R, G, B dal COLORREF Win32
        PLBYTE r = GetRValue(colorref);
        PLBYTE g = GetGValue(colorref);
        PLBYTE b = GetBValue(colorref);
        
		// crea l'oggetto pixel a 32 bit
        PLPixel32 p(r,g,b,255);

		// lo imposta
        m_Image.SetPixel(x,y,p);
    }

done:

	;
}

/*
	SetPalette()
*/
BOOL CPaintLib::SetPalette(UINT nIndex,UINT nColors,RGBQUAD* pPalette)
{
	// controlla che l'oggetto immagine sia valido
	if(!IsValid(__func__))
		return(FALSE);

	UINT nColorData = GetNumColors();
	
	if(!nColorData || nIndex < 0 || nIndex >= nColorData || pPalette==NULL || nColors <= 0 || (nColors+nIndex) > nColorData)
	{
		SetLastErrorDescriptionEx("%s(): unable to set the palette",__func__);
		return(FALSE);
	}

	for(int i = 0; i < (int)nColors; i++)
		m_Image.SetPaletteEntry(i+nIndex,pPalette[i+nIndex].rgbRed,pPalette[i+nIndex].rgbGreen,pPalette[i+nIndex].rgbBlue,255);
	
	return(TRUE);
}

/*
	UpdateHeaderInfo()
*/
DWORD CPaintLib::UpdateHeaderInfo(void)
{
	if(!IsValid(__func__))
		return(GDI_ERROR);

	// ricava le dimensioni dell'immagine, paintLib usa DIB Windows -> stride × height
	//m_InfoHeader.bppOriginal -> impostato nella Load()
	m_InfoHeader.bppConverted = m_Image.GetBitsPerPixel();
	m_InfoHeader.width = m_Image.GetWidth();
	m_InfoHeader.height = m_Image.GetHeight();
	m_InfoHeader.bpp = m_Image.GetBitsPerPixel();
	m_InfoHeader.memused = m_Image.GetMemNeeded(m_InfoHeader.width,m_InfoHeader.height,m_InfoHeader.bpp);
    
	// determina il numero di colori nella palette (0 x 32 bpp)
	// codice valido a prescindere dalla libreria
	if(m_InfoHeader.bpp <= 8)
	{
		// immagini palette-based
		if(m_InfoHeader.bpp== 8)
			m_InfoHeader.colors = 256;
		else if(m_InfoHeader.bpp== 4)
			m_InfoHeader.colors = 16;
		else if(m_InfoHeader.bpp== 1)
			m_InfoHeader.colors = 2;
		else
			m_InfoHeader.colors = 1 << m_InfoHeader.bpp;
	}
	else // immagini senza palette
	{
		// in tal caso, m_Image.GetNumColors() restituisce il numero di colori -possibili-,
		// non effettivi, che l'immagine puo' arrivare a tenere a seconda del numero di bpp:
		// 16 bpp -> 65,536 non ha palette, puo' arrivare fino a 65K colori possibili
		// 24 bpp -> 16,777,216 noh ha palette, puo' arrivare fino a 16.7M colori possibili
		// 32 bpp -> 4,294,967,296 noh ha palette, puo' arrivare fino a 4.3M colori possibili
		m_InfoHeader.colors = 0;
	}

	// risoluzione (DPI)
	float nXRes = 0.0f;
	float nYRes = 0.0f;
	GetDPI(nXRes, nYRes);
	CImage::SetXRes(nXRes);
	CImage::SetYRes(nYRes);
	m_InfoHeader.xres = nXRes; //72.0f;
	m_InfoHeader.yres = nYRes; //72.0f;
	m_InfoHeader.restype = RESUNITINCH;
    
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
 
	Crea una nuova immagine DIB (Device Independent Bitmap) a partire da una struttura BITMAPINFO
	e dai dati pixel opzionali.
*/
BOOL CPaintLib::Create(BITMAPINFO* pBitmapInfo,void *pData/* = NULL */)
{
	// qui NON bisogna controllare se l'oggetto immagine e' valido perche' lo sta creando ex novo
	//if(!IsValid(__func__))
	//	return(FALSE);

	Unload();

	BOOL bCreated = FALSE;
	BITMAPINFOHEADER* pBitmapInfoHeader = (BITMAPINFOHEADER*)pBitmapInfo;

    if(pBitmapInfo)
    {
        RGBQUAD* pPalette = pBitmapInfo->bmiColors;

        // verifica che ci siano i metadati minimi
        if(pBitmapInfoHeader) 
        {
            // definisce il formato pixel desiderato (PLPixelFormat) in base al BPP
            const PLPixelFormat* pfWanted = NULL; 
            switch(pBitmapInfoHeader->biBitCount)
            {
                case 1: pfWanted = &PLPixelFormat::L1; break;
                case 8: pfWanted = &PLPixelFormat::I8; break; // I8 = indexed 8-bit
                case 16: pfWanted = &PLPixelFormat::R5G6B5; break;
                case 24: pfWanted = &PLPixelFormat::R8G8B8; break;
                case 32: pfWanted = &PLPixelFormat::X8R8G8B8; break; // X8R8G8B8 = 32bpp senza canale Alpha
                default: return FALSE; // BPP non supportato
            }

            // chiama la Create: alloca la memoria senza inizializzarla
            // m_Image.Create(Width, Height, PixelFormat, pBits=NULL, Stride=0, Resolution)
            m_Image.Create(
                pBitmapInfoHeader->biWidth,
                abs(pBitmapInfoHeader->biHeight),
                *pfWanted,
                0, // pBits = 0: alloca memoria, non la copia/inizializza
                0, // Stride = 0: non necessario se pBits è 0
                PLPoint(pBitmapInfoHeader->biXPelsPerMeter, pBitmapInfoHeader->biYPelsPerMeter) // risoluzione
            );
            
            // copia la Palette (se necessaria e se pPalette esiste)
            if(pBitmapInfoHeader->biBitCount <= 8 && pPalette)
            {
                int nColorData = 1 << pBitmapInfoHeader->biBitCount;
                if(nColorData)
                {
                    void* pPaletteDst = m_Image.GetPalette(); 
                    if(pPaletteDst)
                        memcpy(pPaletteDst,pPalette,nColorData*sizeof(RGBQUAD));
                }
            }

            // copia/inizializza i dati pixel
            unsigned char *pDataDst = m_Image.GetBits();
            
            // calcola WidthEnBytes (stride DIB, 4-byte padded)
            int WidthInBits = pBitmapInfoHeader->biWidth * pBitmapInfoHeader->biBitCount;
            int WidthEnBytes = (((WidthInBits + 31) & ~31) >> 3); 
            int nTotalBytes = abs(pBitmapInfoHeader->biHeight) * WidthEnBytes;
            
            if(pDataDst)
            {
                if(!pData)
                    memset(pDataDst,'\0',nTotalBytes);
                else
                    memcpy(pDataDst,pData,nTotalBytes);

                bCreated = TRUE;
            }
        }
    }

	// marca l'immagine come valida
	if(bCreated)
		m_bIsValid = TRUE;

    // aggiorna l'header
    if(bCreated)
		bCreated = UpdateHeaderInfo()==NO_ERROR;

	if(!bCreated)
		SetLastErrorDescriptionEx("%s(): failed",__func__);

    return(bCreated);
}

/*
	Load()
*/
BOOL CPaintLib::Load(LPCSTR lpcszFileName,DWORD& dwError)
{
	// qui NON bisogna controllare se l'oggetto immagine e' valido perche' lo sta caricando da file
	//if(!IsValid(__func__))
	//	return(FALSE);

	dwError = GDI_ERROR;
	BOOL bLoaded = FALSE;
	BOOL bExcept = FALSE;

	if(!IsSupportedFormat(lpcszFileName))
	{
		SetLastErrorDescriptionEx("%s(): unsupported format: %s",__func__,lpcszFileName);
		return(FALSE);
	}

	Unload();

	__try {

		bLoaded = LoadBitmap(lpcszFileName);
	}
	__except(dwError = GetExceptionCode(),EXCEPTION_EXECUTE_HANDLER) {

		bLoaded = FALSE;
		bExcept = TRUE;
	}

	if(bLoaded && !bExcept)
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
			else if(stricmp(m_szFormat,"JPG")==0 || stricmp(m_szFormat,"JPEG")==0 || stricmp(m_szFormat,"JPE")==0 || stricmp(m_szFormat,"JIF")==0 || stricmp(m_szFormat,"JFIF")==0)
				m_InfoHeader.type = JPEG_PICTURE;
			else if(stricmp(m_szFormat,"PCX")==0)
				m_InfoHeader.type = PCX_PICTURE;
			else if(stricmp(m_szFormat,"PIC")==0 || stricmp(m_szFormat,"PICT")==0 || stricmp(m_szFormat,"PCT")==0)
				m_InfoHeader.type = PICT_PICTURE;
			else if(stricmp(m_szFormat,"PGN")==0)
				m_InfoHeader.type = PGM_PICTURE;
			else if(stricmp(m_szFormat,"PNG")==0)
				m_InfoHeader.type = PNG_PICTURE;
			else if(stricmp(m_szFormat,"TGA")==0)
				m_InfoHeader.type = TGA_PICTURE;
			else if(stricmp(m_szFormat,"TIFF")==0 || stricmp(m_szFormat,"TIF")==0)
				m_InfoHeader.type = TIFF_PICTURE;
		}

		// bpp originali ?, paintLib non trasforma automaticamente a 32 bpp al caricamento del file ??
		m_InfoHeader.bppOriginal = m_Image.GetBitsPerPixel();

		// i PNG RGB a 24 bpp ma a 8-bit x canale (bpc) sono problematici per paintLib (buggata), quindi controlla e nel caso converte a 32 bpp
		if(m_InfoHeader.type==PNG_PICTURE)
		{
			PNG_FORMAT_INFO pngInfo = {0};
			CImage::CheckPNGHeader(m_szFileName,&pngInfo);
			BOOL needsNormalization = (pngInfo.colorType==2 || pngInfo.colorType==3) && pngInfo.bpc==8;
			if(needsNormalization)
			{
				m_InfoHeader.bppOriginal = pngInfo.bpp;

				if(ConvertToBPP(32)==NO_ERROR)
					m_InfoHeader.bppConverted = m_Image.GetBitsPerPixel();
				else
					m_InfoHeader.bppConverted = -1;
			}
		}

		// converte a 32 bpp tutti i <24 bpp in bianco e nero (scala di grigi)
//		if(m_Image.GetBitsPerPixel() < 24 && (IsTrueGrayscale() || IsVisualGrayscaleStatistical(VISUAL_GRAYSCALE_PARAMS)))
		if(m_Image.GetBitsPerPixel() < 24 && IsTrueGrayscale())
		{
			if(ConvertToBPP(32)==NO_ERROR)
				m_InfoHeader.bppConverted = m_Image.GetBitsPerPixel();
			else
				m_InfoHeader.bppConverted = -1;
		}
		else
			m_InfoHeader.bppConverted = -1;

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

		// ricava le dimensioni dell'immagine, paintLib usa DIB Windows -> stride × height
		//m_InfoHeader.bppOriginal -> impostato sopra
		m_InfoHeader.width = m_Image.GetWidth();
		m_InfoHeader.height = m_Image.GetHeight();
		m_InfoHeader.bpp = m_Image.GetBitsPerPixel();
		m_InfoHeader.memused = m_Image.GetMemNeeded(m_InfoHeader.width,m_InfoHeader.height,m_InfoHeader.bpp);

		// determina il numero di colori nella palette (0 x 32 bpp)
		// codice valido a prescindere dalla libreria
		if(m_InfoHeader.bpp <= 8)
		{
			// immagini palette-based
			if(m_InfoHeader.bpp== 8)
				m_InfoHeader.colors = 256;
			else if(m_InfoHeader.bpp== 4)
				m_InfoHeader.colors = 16;
			else if(m_InfoHeader.bpp== 1)
				m_InfoHeader.colors = 2;
			else
				m_InfoHeader.colors = 1 << m_InfoHeader.bpp;
		}
		else // immagini senza palette
		{
			// in tal caso, m_Image.GetNumColors() restituisce il numero di colori -possibili-,
			// non effettivi, che l'immagine puo' arrivare a tenere a seconda del numero di bpp:
			// 16 bpp -> 65,536 non ha palette, puo' arrivare fino a 65K colori possibili
			// 24 bpp -> 16,777,216 noh ha palette, puo' arrivare fino a 16.7M colori possibili
			// 32 bpp -> 4,294,967,296 noh ha palette, puo' arrivare fino a 4.3M colori possibili
			m_InfoHeader.colors = 0;
		}

		// risoluzione (DPI)
		float nXRes = 0.0f;
		float nYRes = 0.0f;
		GetDPI(nXRes, nYRes);
		CImage::SetXRes(nXRes);
		CImage::SetYRes(nYRes);
		m_InfoHeader.xres = nXRes; //72.0f;
		m_InfoHeader.yres = nYRes; //72.0f;
		m_InfoHeader.restype = RESUNITINCH;
    
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

		dwError = NO_ERROR;
	}
	else
	{
		char buffer[1024] = {0};
		if(bExcept)
			SetLastErrorDescriptionEx("%s(): an unexpected exception (0x%08X) has occurred while loading: %s",__func__,dwError,lpcszFileName);
		else
			SetLastErrorDescriptionEx("%s(): unable to load: %s",__func__,lpcszFileName);
	}

	return(bLoaded);
}

/*
	LoadFromUrl()
*/
BOOL CPaintLib::LoadFromUrl(LPCSTR lpcszUrl)
{
	BOOL bLoaded = TRUE;

	try
	{
		m_bIsValid = TRUE;
		PLJPEGDecoder Decoder;
		Decoder.MakeBmpFromURL(lpcszUrl,&m_Image);
	}
	catch(PLTextException e)
	{
		m_bIsValid = FALSE;
		bLoaded = FALSE;
		SetLastErrorDescriptionEx("%s(): an exception has been raised, code %d, text %s",__func__,e.GetCode(),(const CHAR *)e);
	}

	return(bLoaded);
}

/*
	LoadBitmap()
*/
BOOL CPaintLib::LoadBitmap(LPCSTR lpcszFileName)
{
	try
	{
		m_bIsValid = TRUE;
		PLAnyPicDecoder Decoder;
		Decoder.MakeBmpFromFile(lpcszFileName,&m_Image);
	}
	catch(PLTextException e)
	{
		m_bIsValid = FALSE;
		SetLastErrorDescriptionEx("%s(): an exception has been raised, code %d, text %s",__func__,e.GetCode(),(const CHAR *)e);
	}

	return(m_bIsValid);
}

/*
	Unload()
*/
BOOL CPaintLib::Unload(void)
{
	if(m_bIsValid)
	{
		m_bIsValid = FALSE;

		// nome file relativo all'immagine e formato
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

		// oggetto immagine
		//m_Image
	}

	return(!m_bIsValid);
}

/*
	Save()

	Salva l'immagine corrente con il nome file e nel formato specificati.
	Passare il nome (completo di estensione) con cui salvare il file ed il formato, 
	da indicare	in modo diretto (senza il punto, vedi sotto).

	I formati supportati sono: BMP, JPEG, PNG, TIF.
*/
BOOL CPaintLib::Save(LPCSTR lpcszFileName,LPCSTR lpcszFormat,DWORD dwFlags)
{
	// controlla che l'oggetto immagine sia valido
	if(!IsValid(__func__))
		return(FALSE);

	DWORD dwError = (DWORD)-1L;
	BOOL bSaved = FALSE;
	BOOL bExcept = FALSE;

	__try {

		bSaved = SaveBitmap(lpcszFileName,lpcszFormat,dwFlags);
	}
	__except(dwError = GetExceptionCode(),EXCEPTION_EXECUTE_HANDLER) {

		bSaved = FALSE;
		bExcept = TRUE;
	}

	if(!bSaved)
	{
		if(bExcept)
			SetLastErrorDescriptionEx("%s(): an unexpected exception (0x%08X) has occurred while saving: %s",__func__,dwError,lpcszFileName);
		else
			SetLastErrorDescriptionEx("%s(): unable to save: %s",__func__,lpcszFileName);
	}

	if(bSaved)
		CImage::Flush(lpcszFileName);

	return(bSaved);
}

/*
	SaveBitmap()
*/
BOOL CPaintLib::SaveBitmap(LPCSTR lpcszFileName,LPCSTR lpcszFormat,DWORD /*dwFlags*/)
{
	// controlla che l'oggetto immagine sia valido
	if(!IsValid(__func__))
		return(FALSE);

	BOOL bSaved = TRUE;

	// BMP_PICTURE
	if(stricmp(lpcszFormat,"BMP")==0)
	{
		try {
			BOOL bConverted = TRUE;
			if(m_Image.GetBitsPerPixel()!=32)
			{
				if(!(bConverted = ConvertToBPP(32)==NO_ERROR))
					bSaved = FALSE;
			}
			if(bConverted)
			{
				PLBmpEncoder Encoder;
				Encoder.MakeFileFromBmp(lpcszFileName,(PLBmp*)&m_Image);
			}
		}
		catch(PLTextException e) {
			bSaved = FALSE;
			SetLastErrorDescriptionEx("%s(): an exception has been raised, code %d, text %s",__func__,e.GetCode(),(const CHAR *)e);
		}
	}
	// JPEG_PICTURE
	else if(stricmp(lpcszFormat,"JPG")==0 || stricmp(lpcszFormat,"JPEG")==0 || stricmp(lpcszFormat,"JPE")==0 || stricmp(lpcszFormat,"JIF")==0 || stricmp(lpcszFormat,"JFIF")==0)
	{
		PLJPEGEncoder encoder;
		try {
			BOOL bConverted = TRUE;
			if(m_Image.GetBitsPerPixel()!=32)
				bConverted = ConvertToBPP(32)==NO_ERROR;
			if(bConverted)
			{
				int nQuality = CImage::GetQuality();
				nQuality = nQuality <= 0 || nQuality > 100 ? 100 : nQuality;
							
				// CORREZIONE SetQuality sull'encoder
				encoder.SetQuality(nQuality);
				encoder.MakeFileFromBmp(lpcszFileName,(PLBmp*)&m_Image);
			}
			else
				bSaved = FALSE;
		}
		catch(PLTextException e) {
			bSaved = FALSE;
			SetLastErrorDescriptionEx("%s(): an exception has been raised, code %d, text %s",__func__,e.GetCode(),(const CHAR *)e);
		}
	}
	// PNG_PICTURE
	else if(stricmp(lpcszFormat,"PNG")==0)
	{
		PLPNGEncoder encoder;
		try {
			encoder.MakeFileFromBmp(lpcszFileName,(PLBmp*)&m_Image);
		}
		catch(PLTextException e) {
			bSaved = FALSE;
			SetLastErrorDescriptionEx("%s(): an exception has been raised, code %d, text %s",__func__,e.GetCode(),(const CHAR *)e);
		}
	}
	// TIFF_PICTURE
	else if(stricmp(lpcszFormat,"TIFF")==0 || stricmp(lpcszFormat,"TIF")==0)
	{
		PLTIFFEncoder encoder;
		try {
			// imposta la compressione (se necessaria)
			int nCompression = CImage::GetCompression();
			encoder.SetCompression((PLWORD)nCompression); 

			// salvataggio semplice (MakeFileFromBmp)
			encoder.MakeFileFromBmp(lpcszFileName,(PLBmp*)&m_Image);
		}
		catch(PLTextException e) {
			bSaved = FALSE;
			SetLastErrorDescriptionEx("%s(): an exception has been raised, code %d, text %s",__func__,e.GetCode(),(const CHAR *)e);
		}
	}

	return(bSaved);
}

/*
	Stretch()
*/
UINT CPaintLib::Stretch(RECT& drawRect,BOOL bAspectRatio/*=TRUE*/)
{
	// controlla che l'oggetto immagine sia valido
	if(!IsValid(__func__))
		return(GDI_ERROR);

	double nFactor = 0.0;
	double nWidth = (double)GetWidth();
	double nHeight = (double)GetHeight();

	// l'algoritmo si adatta al rettangolo (non esce dai bordi, modo fit)
	// logica per calcolare nWidth e nHeight in double basata su drawRect e bAspectRatio
	if(bAspectRatio)
	{
		if(nHeight < (double)drawRect.bottom)
		{
			nFactor = (double)drawRect.bottom/nHeight;
			if(nFactor > 0.0)
			{
				nHeight *= nFactor;
				nWidth *= nFactor;
			}
		}
		if(nWidth < (double)drawRect.right)
		{
			nFactor = (double)drawRect.right/nWidth;
			if(nFactor > 0.0)
			{
				nHeight *= nFactor;
				nWidth *= nFactor;
			}
		}
	}
	else
	{
		nWidth = (double)drawRect.right;
		nHeight = (double)drawRect.bottom;
	}

	if(nHeight > (double)drawRect.bottom)
	{
		nFactor = nHeight/(double)drawRect.bottom;
		if(nFactor > 0.0)
		{
			nHeight /= nFactor;
			nWidth /= nFactor;
		}
	}
	if(nWidth > (double)drawRect.right)
	{
		nFactor = nWidth/(double)drawRect.right;
		if(nFactor > 0.0)
			
		{
			nHeight /= nFactor;
			nWidth /= nFactor;
		}
	}

	// ridimensiona l'immagine
	BOOL bResult = TRUE;

	// conversione necessaria per filtri di ridimensionamento
	int nOriginalBpp = m_Image.GetBitsPerPixel();
	if(nOriginalBpp!=32)
		bResult = ConvertToBPP(32)==NO_ERROR;
		
	if(bResult)
	{
        // conversione dei tipi: da float a int per le dimensioni
        int NewWidth = (int)nWidth;
        int NewHeight = (int)nHeight;

        if(NewWidth <= 0) NewWidth = 1;
        if(NewHeight <= 0) NewHeight = 1;
            
		// bilineare (piu' veloce, meno qualita')
//			PLFilterResizeBilinear Filter(NewWidth,NewHeight);

		// box (ancora piu' veloce, qualita' base)
//			PLFilterResizeBox Filter(NewWidth,NewHeight);

		// Gaussiano (controllo via deviazione standard invece che raggio)
//			PLFilterResizeGaussian Filter(NewWidth,NewHeight,1.0); // w, h, radio

		// finestra di Hamming (il piu' bilanciato)
		// valori per il raggio del kernel di filtraggio: 1.0 valore standard, 0.5 piu' sharp, 1.5 piu' smooth
		// per foto: 1.0 ~ 1.2
		// per screenshot/testo: 0.7 ~ 0.9  
		// per smoothing estremo: 1.5 ~ 2.0
        PLFilterResizeHamming Filter(NewWidth,NewHeight,1.0); // w, h, radio
            
        // applica il filtro su una nuova immagine e poi la sostituisce con la corrente
		PLWinBmp destImage;
		Filter.Apply(&m_Image,&destImage);
		m_Image = destImage;

		bResult = UpdateHeaderInfo()==NO_ERROR;
	}

	if(!bResult)
		SetLastErrorDescriptionEx("%s(): unable to stretch the image",__func__);
	
	return(bResult ? NO_ERROR : GDI_ERROR);
}

/*
	Contrast()
*/
UINT CPaintLib::Contrast(void)
{
	// controlla che l'oggetto immagine sia valido
	if(!IsValid(__func__))
		return(GDI_ERROR);

	BOOL bResult = TRUE;

	// conversione a 32bpp necessaria
	int nOriginalBpp = m_Image.GetBitsPerPixel();
	if(nOriginalBpp!=32)
		bResult = ConvertToBPP(32)==NO_ERROR;
		
	if(bResult)
	{
		// ricava il valore del parametro    
		int nContrast = 0;
		double nValue = 0;
		if(GetFilterParams("Contrast",nValue))
			nContrast = (int)nValue;
		else    
		{
			SetLastErrorDescriptionEx("%s(): ERROR_INVALID_PARAMETER",__func__);
			return(ERROR_INVALID_PARAMETER);
		}

		// -100 = contrasto minimo (tutto grigio)
		//    0 = nessuna modifica  
		// +100 = contrasto massimo
		if(nContrast!=0)
		{
			// parametri per il filtro: <contrast> lo calcola rimappando il valore di default del parametro e <offset>
			// lo imposta come punto medio fisso, la paintLib suggerisce di sperimentare con l'offset (non solo 128) 
			// per risultati migliori, soprattutto se l'immagine e' gia' molto chiara/scura
			double contrast = 0;
			PLBYTE offset = 128;

			if(nContrast >= 0)
			{
				// contrasto aumentato: da 1.0 a 4.0
				contrast = 1.0 + (nContrast / 100.0) * 3.0;
			}
			else
			{
				// contrasto diminuito: da 1.0 a 0.1
				contrast = 1.0 + (nContrast / 100.0) * 0.9;
			}

			// crea l'oggetto per il filtro
			PLFilterContrast Filter(contrast,offset);
        
			// crea un bitmap di destinazione separato
			PLWinBmp destImage;
        
			// applica il filtro creando una nuova immagine
			Filter.Apply(&m_Image,&destImage);
        
			// sostituisce l'immagine originale con quella convertita
			m_Image = destImage;
		}

		bResult = UpdateHeaderInfo()==NO_ERROR;
	}

	if(!bResult)
		SetLastErrorDescriptionEx("%s(): unable to apply the contrast filter",__func__);

    return(bResult ? NO_ERROR : GDI_ERROR);
}

/*
	Grayscale()
*/
UINT CPaintLib::Grayscale(void)
{
	// controlla che l'oggetto immagine sia valido
	if(!IsValid(__func__))
		return(GDI_ERROR);

	BOOL bResult = TRUE;

	// non converte un immagine che gia' e'/sembra grayscale, restituisce errore
	if(IsTrueGrayscale() || IsVisualGrayscaleStatistical(VISUAL_GRAYSCALE_PARAMS))
	{
		SetLastErrorDescriptionEx("%s(): image is already grayscale, filter will not be applied",__func__);
		return(GDI_ERROR);
	}

	// conversione a 32bpp necessaria
	int nOriginalBpp = m_Image.GetBitsPerPixel();
	if(nOriginalBpp!=32)
		bResult = ConvertToBPP(32)==NO_ERROR;
		
	if(bResult)
	{
		// crea l'oggetto per il filtro
        PLFilterGrayscale Filter; 
        
		// crea un bitmap di destinazione separato
        PLWinBmp destImage;
        
		// applica il filtro creando una nuova immagine
        Filter.Apply(&m_Image,&destImage);
        
			// sostituisce l'immagine originale con quella convertita
        m_Image = destImage;

		bResult = UpdateHeaderInfo()==NO_ERROR;
    }

	if(!bResult)
		SetLastErrorDescriptionEx("%s(): unable to apply the grayscale filter",__func__);

    return(bResult ? NO_ERROR : GDI_ERROR);
}

/*
	Intensity()
*/
UINT CPaintLib::Intensity(void)
{
	// controlla che l'oggetto immagine sia valido
	if(!IsValid(__func__))
		return(GDI_ERROR);

	BOOL bResult = TRUE;

	// conversione a 32bpp necessaria
	int nOriginalBpp = m_Image.GetBitsPerPixel();
	if(nOriginalBpp!=32)
		bResult = ConvertToBPP(32)==NO_ERROR;
		
	if(bResult)
	{
		// ricava il valore del parametro    
		int nIntensity = 0;
		double nValue = 0;
		if(GetFilterParams("Intensity",nValue))
			nIntensity = (int)nValue;
		else
		{
			SetLastErrorDescriptionEx("%s(): ERROR_INVALID_PARAMETER",__func__);
			return(ERROR_INVALID_PARAMETER);
		}

		// -100 = contrasto minimo (tutto grigio)
		//    0 = nessuna modifica  
		// +100 = contrasto massimo
		if(nIntensity!=0)
		{
			double intensity;
			PLBYTE offset = 128;
			double exponent = 1.0;

			// mappatura: 0 -> 20 (neutro), -100 -> 0, +100 -> 40
			intensity = 20.0 + (nIntensity / 100.0) * 20.0;

			// crea l'oggetto per il filtro
			PLFilterIntensity Filter(intensity,offset,exponent);
        
			// crea un bitmap di destinazione separato
			PLWinBmp destImage;
        
			// applica il filtro creando una nuova immagine
			Filter.Apply(&m_Image,&destImage);
        
			// sostituisce l'immagine originale con quella convertita
			m_Image = destImage;
		}

		bResult = UpdateHeaderInfo()==NO_ERROR;
    }

	if(!bResult)
		SetLastErrorDescriptionEx("%s(): unable to apply the intensity filter",__func__);

    return(bResult ? NO_ERROR : GDI_ERROR);
}

/*
	Negate()
*/
UINT CPaintLib::Negate(void)
{
	// controlla che l'oggetto immagine sia valido
	if(!IsValid(__func__))
		return(GDI_ERROR);

	BOOL bResult = FALSE;

	int nBpp = m_Image.GetBitsPerPixel();
	switch(nBpp)
	{
		case 24:
		case 32:
		{
			// crea l'oggetto per il filtro
            PLFilterVideoInvert Filter; 
            
			// crea un bitmap di destinazione separato
			PLWinBmp destImage;

			// applica il filtro creando una nuova immagine
			Filter.Apply(&m_Image,&destImage);
        
			// sostituisce l'immagine originale con quella convertita
			m_Image = destImage;
			bResult = TRUE;
		}

		default:
			bResult = (CImage::Negate()==NO_ERROR);
			break;
	}

	if(!bResult)
		SetLastErrorDescriptionEx("%s(): unable to apply the negate filter",__func__);

    return(bResult ? NO_ERROR : GDI_ERROR);
}

/*
	Rotate90Left()
*/
UINT CPaintLib::Rotate90Left(void)
{
	// controlla che l'oggetto immagine sia valido
	if(!IsValid(__func__))
		return(GDI_ERROR);

	if(Rotate((double)90.0))
		return(NO_ERROR);

	SetLastErrorDescriptionEx("%s(): unable to apply the rotate 90 left filter",__func__);
	return(GDI_ERROR);
}

/*
	Rotate90Right()
*/
UINT CPaintLib::Rotate90Right(void)
{
	// controlla che l'oggetto immagine sia valido
	if(!IsValid(__func__))
		return(GDI_ERROR);

	if(Rotate((double)-90.0))
		return(NO_ERROR);

	SetLastErrorDescriptionEx("%s(): unable to apply the rotate 90 right filter",__func__);
	return(GDI_ERROR);
}

/*
	Rotate180()
*/
UINT CPaintLib::Rotate180(void)
{
	// controlla che l'oggetto immagine sia valido
	if(!IsValid(__func__))
		return(GDI_ERROR);

	if(Rotate((double)180.0))
		return(NO_ERROR);

	SetLastErrorDescriptionEx("%s(): unable to apply the rotate 180 filter",__func__);
	return(GDI_ERROR);
}

/*
	Rotate()
*/
BOOL CPaintLib::Rotate(double grados)
{
	// controlla che l'oggetto immagine sia valido
	if(!IsValid(__func__))
		return(GDI_ERROR);

	BOOL bResult = TRUE;

	// conversione a 32bpp necessaria
	int nOriginalBpp = m_Image.GetBitsPerPixel();
	if(nOriginalBpp!=32)
		bResult = ConvertToBPP(32)==NO_ERROR;
		
	if(bResult)
	{
		// l'angolo effettivo di rotazione e' -1 x gradi
		int effectiveAngle = (int)(-grados);
    
		// normalizza l'angolo a 0-359 gradi (CCW)
		int normalizedAngle = (effectiveAngle % 360 + 360) % 360;

		PLFilterRotate::AngleType filterAngle;

		if(normalizedAngle==90)
			filterAngle = PLFilterRotate::ninety; // 90 gradi CCW
		else if(normalizedAngle==180)
			filterAngle = PLFilterRotate::oneeighty;
		else if(normalizedAngle==270)
			filterAngle = PLFilterRotate::twoseventy; // 270 gradi CCW (per Rotate(90.0) CW)
		else
		{
			SetLastErrorDescriptionEx("%s(): wrong degrees value",__func__);
			return(FALSE);
		}

		// crea l'oggetto per il filtro
		PLFilterRotate Filter(filterAngle);

		// crea un bitmap di destinazione separato
		PLWinBmp destImage;

		// applica il filtro creando una nuova immagine
		Filter.Apply(&m_Image,&destImage);
        
		// sostituisce l'immagine originale con quella convertita
		m_Image = destImage;
        
		// aggiorna le dimensioni nell'header: larghezza e altezza si scambiano per rotazioni di 90 o 270 gradi
		if(normalizedAngle==90 || normalizedAngle==270)
		{
			int n = m_InfoHeader.width;
			m_InfoHeader.width = m_InfoHeader.height;
			m_InfoHeader.height = n;
		}

		bResult = (UpdateHeaderInfo()==NO_ERROR);
	}
	
	if(!bResult)
		SetLastErrorDescriptionEx("%s(): unable to apply the rotate filter",__func__);

	return(bResult);
}

/*
	IsValid()
*/
BOOL CPaintLib::IsValid(LPCSTR lpcszFunctionName)
{
	if(!m_bIsValid)
	{
		char buffer[128] = {0};
		snprintf(buffer,sizeof(buffer),"%s(): no image loaded",lpcszFunctionName);
		CImage::SetLastErrorDescription(buffer);
		#ifdef DEBUG
			::MessageBox(NULL,GetLastErrorDescription(),GetLibraryName(),MB_ICONERROR|MB_TASKMODAL|MB_SETFOREGROUND|MB_TOPMOST);
		#endif
	}
	return(m_bIsValid);
}

#endif // HAVE_PAINTLIB_LIBRARY
