/*$
	CFreeImage.cpp
	Classe derivata per interfaccia con FreeImage (http://freeimage.sourceforge.io/).
	Luca Piergentili, Nov '25

	Vedi le note in CFreeImage.h.
*/
#include "env.h"
#include "pragma.h"
#include "macro.h"
#include "window.h"
#include "ImageConfig.h"

#ifdef HAVE_FREEIMAGE_LIBRARY

#include <math.h>
#include <stdio.h>
#include <string.h>
#include "strings.h"
#include "CImage.h"
#include "CFreeImage.h"

#ifdef FREEIMAGE_RESURRECTED
	#pragma message("\t\t\t"\
					__FILE__\
					"("\
					STR(__LINE__)\
					"): "\
					__FILE__\
					" using resurrected version of FreeImage")
#else
	#pragma message("\t\t\t"\
					__FILE__\
					"("\
					STR(__LINE__)\
					"): "\
					__FILE__\
					" using original version of FreeImage")
#endif

/*
	CFreeImage()
*/
CFreeImage::CFreeImage()
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

	// tipi files riconosciuti, al momento NON verificati, solo assunti...
	// mantenere sincronizzato con la determinazione del tipo immagine in Load()
	LPIMAGETYPE p;
#ifdef FREEIMAGE_RESURRECTED
	ADDFILETYPE(AVIF_PICTURE,	".avif","AVIF file ",						IMAGE_READ_FLAG|IMAGE_WRITE_FLAG|IMAGE_WEB_FLAG,p)
#endif
	ADDFILETYPE(WEBP_PICTURE,	".webp","WebP file ",						IMAGE_READ_FLAG|                 IMAGE_WEB_FLAG,p)
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
	SetFilterParams("Blur",				0,10,5,			"","",True,&CImageObject::Blur);
	SetFilterParams("Brightness",		-100,100,40,	"","",True,&CImageObject::Brightness);
	SetFilterParams("ColorSwap",COLORSWAP_IDENTITY,COLORSWAP_RANDOM,COLORSWAP_ROTATE_REV,"","",True,&CImageObject::ColorSwap);
//	SetFilterParams("ColorSwap",COLORSWAP_IDENTITY,COLORSWAP_RANDOM,COLORSWAP_RANDOM,"","",True,&CImageObject::ColorSwap);
	SetFilterParams("Contrast",			-100,100,40,	"","",True,&CImageObject::Contrast);
	SetFilterParams("GammaCorrection",	0,10,5,			"","",True,&CImageObject::GammaCorrection);
	SetFilterParams("Grayscale",		0,0,0,			"","",True,&CImageObject::Grayscale);
	SetFilterParams("Hue",				-180,180,100,	"","",True,&CImageObject::Hue);
	SetFilterParams("MirrorHorizontal",	0,0,0,			"","",True,&CImageObject::MirrorHorizontal);
	SetFilterParams("MirrorVertical",	0,0,0,			"","",True,&CImageObject::MirrorVertical);
	SetFilterParams("Negate",			0,0,0,			"","",True,&CImageObject::Negate);
	SetFilterParams("Posterize",		2,255,7,		"","",True,&CImageObject::Posterize);
	SetFilterParams("Rotate90Left",		0,0,0,			"","",True,&CImageObject::Rotate90Left);
	SetFilterParams("Rotate90Right",	0,0,0,			"","",True,&CImageObject::Rotate90Right);
	SetFilterParams("Rotate180",		0,0,0,			"","",True,&CImageObject::Rotate180);
	SetFilterParams("Sharpen",			0,100,50,		"","",True,&CImageObject::Sharpen);

// con la DLL la chiamata gia' viene fatta in proprio, chiamare solo con libreria statica
#ifdef FREEIMAGE_LIB
	FreeImage_Initialise();
#endif
}

/*
	~CFreeImage()
*/
CFreeImage::~CFreeImage()
{
	Unload();

// con la DLL la chiamata gia' viene fatta in proprio, chiamare solo con libreria statica
#ifdef FREEIMAGE_LIB
    FreeImage_DeInitialise();
#endif
}

/*
	GetXRes()

	Risoluzione orizzontale in DPI. FreeImage usa dots per meter (DPM), converte.
*/
float CFreeImage::GetXRes(void)
{
	// controlla che l'oggetto immagine sia valido
	if(!IsValid(__func__))
		return(0.0f);
	
	// 1 metro = 39.37 pollici. DPI = DPM / 39.37
	unsigned int dpm = FreeImage_GetDotsPerMeterX(m_pImage);
	if(dpm==0)
		return((float)m_InfoHeader.xres);

	return((float)dpm / 39.37f);
}

/*
	GetYRes()

	Risoluzione verticale in DPI. FreeImage usa dots per meter (DPM), converte.
*/
float CFreeImage::GetYRes(void)
{
	// controlla che l'oggetto immagine sia valido
	if(!IsValid(__func__))
		return(0.0f);

	unsigned int dpm = FreeImage_GetDotsPerMeterY(m_pImage);
	if(dpm==0)
		return((float)m_InfoHeader.yres);

	return((float)dpm / 39.37f);
}

/*
	GetPhotometric()
*/
PHOTOMETRIC CFreeImage::GetPhotometric(void)
{
	// controlla che l'oggetto immagine sia valido
	if(!IsValid(__func__))
		return(NOPHOTOMETRIC);
	
	switch(FreeImage_GetColorType(m_pImage))
	{
		case FIC_MINISBLACK:
			return(PHOTOMETRICMINISBLACK);
		case FIC_MINISWHITE:
			return(PHOTOMETRICMINISWHITE);
		case FIC_PALETTE:
			return(PHOTOMETRICPALETTE);
		case FIC_RGB:
		case FIC_RGBALPHA:
			return(PHOTOMETRICRGB);
		default:
			// FIC_CMYK, FIC_YCBCR, ecc. non sono supportati dall'enum
			return(NOPHOTOMETRIC);
	}
}

/*
	GetNumColors()
*/
UINT CFreeImage::GetNumColors(void)
{
	// controlla che l'oggetto immagine sia valido
	if(!IsValid(__func__))
		return(0);
	
	return(FreeImage_GetColorsUsed(m_pImage));
}

/*
	ConvertToBPP()
*/
UINT CFreeImage::ConvertToBPP(UINT nBitsPerPixel,UINT nFlags/*=0*/,RGBQUAD *pPalette/*=NULL*/,UINT nColors/*=0*/)
{
	// controlla che l'oggetto immagine sia valido
	if(!IsValid(__func__))
		return(GDI_ERROR);
	
	if(nBitsPerPixel==FreeImage_GetBPP(m_pImage))
		return(NO_ERROR);

	FIBITMAP* pNewDib = NULL;

	switch(nBitsPerPixel)
	{
		case 32:
			pNewDib = FreeImage_ConvertTo32Bits(m_pImage);
			break;
		case 24:
			pNewDib = FreeImage_ConvertTo24Bits(m_pImage);
			break;
		case 16: // usa il formato 5:6:5 (standard Win32 per 16-bit)
			pNewDib = FreeImage_ConvertTo16Bits565(m_pImage); 
			break;
		case 8: // quantizzazione di default
			pNewDib = FreeImage_ConvertTo8Bits(m_pImage);
			break;
		case 4:// quantizzazione di default
			pNewDib = FreeImage_ConvertTo4Bits(m_pImage);
			break;
		case 1: // converte prima in scala di grigi, poi dithering Floyd-Steinberg (standard per 1 bit)
		{
			FIBITMAP* pGreyDib = FreeImage_ConvertToGreyscale(m_pImage);
			if(pGreyDib)
			{
				pNewDib = FreeImage_Dither(pGreyDib, FID_FS); // FID_FS = Floyd-Steinberg
				FreeImage_Unload(pGreyDib);
			}
			break;
		}
		default:
			SetLastErrorDescriptionEx("%s(): invalid BPP value",__func__);
			return(GDI_ERROR);
	}

	if(pNewDib)
	{
		// scarica l'immagine corrente e carica la nuova (convertita)
		FreeImage_Unload(m_pImage);
		m_pImage = pNewDib;
		UpdateHeaderInfo();
		return(NO_ERROR);
	}
	else
	{
		SetLastErrorDescriptionEx("%s(): failed",__func__);
		return(GDI_ERROR);
	}
}

/*
	GetPixel()

	Teoricamente copre quasi la casistica intera, dato che i 16 bpp vengono convertiti a 8 nella Load().
*/
COLORREF CFreeImage::GetPixel(int x,int y)
{
	// controlla che l'oggetto immagine sia valido
	if(!IsValid(__func__))
        return(0);

	if(x >= (int)FreeImage_GetWidth(m_pImage) || y >= (int)FreeImage_GetHeight(m_pImage))
        return(0);

#ifdef FREEIMAGE_RESURRECTED
    FIRGBA8 quad;
#else
    RGBQUAD quad;
#endif
    unsigned bpp = FreeImage_GetBPP(m_pImage);

    if(bpp <= 8) // 1, 4, 8 bpp -> palettizzate
    {
        BYTE index;
        if(FreeImage_GetPixelIndex(m_pImage,x,y,&index))
        {
#ifdef FREEIMAGE_RESURRECTED
            FIRGBA8* palette = FreeImage_GetPalette(m_pImage);
#else
            RGBQUAD* palette = FreeImage_GetPalette(m_pImage);
#endif
            if(palette)
            {
                quad = palette[index];
            }
            else
            {
                // raro: palette mancante (es. grayscale 8bpp non palettizzato)
#ifdef FREEIMAGE_RESURRECTED
                quad.red = quad.green = quad.blue = index;
                quad.alpha = 255;
#else
                quad.rgbRed = quad.rgbGreen = quad.rgbBlue = index;
                quad.rgbReserved = 255;
#endif
            }
        }
        else
        {
            return(0); // errore lettura indice
        }
    }
    else if(bpp==24 || bpp==32) // formati RGB/A standard
    {
        if(!FreeImage_GetPixelColor(m_pImage,x,y,&quad))
            return(0);
    }
    else
    {
        // casi high bit depth: 16, 48, 64, ecc.: FreeImage_GetPixelColor funziona AC/DC..., restituisce nero
        return(0);
    }

    // converte da RGBQUAD (ordine BGR) a COLORREF (ordine RGB)
#ifdef FREEIMAGE_RESURRECTED
    return(RGB(quad.red,quad.green,quad.blue));
#else
    return(RGB(quad.rgbRed,quad.rgbGreen,quad.rgbBlue));
#endif
}

/*
	SetPixel()

	Teoricamente copre quasi la casistica intera, dato che i 16 bpp vengono convertiti a 8 nella Load().
*/
void CFreeImage::SetPixel(int x,int y,COLORREF colorref)
{
	// controlla che l'oggetto immagine sia valido
	if(!IsValid(__func__))
        return;

    if(x >= (int)FreeImage_GetWidth(m_pImage) || y >= (int)FreeImage_GetHeight(m_pImage))
		return;

    unsigned bpp = FreeImage_GetBPP(m_pImage);

#ifdef FREEIMAGE_RESURRECTED
	FIRGBA8 quad;
    quad.red   = GetRValue(colorref);
    quad.green = GetGValue(colorref);
    quad.blue  = GetBValue(colorref);
    quad.alpha = 255;  // alpha opaco (utile per 32 bpp)
#else
    RGBQUAD quad;
    quad.rgbRed   = GetRValue(colorref);
    quad.rgbGreen = GetGValue(colorref);
    quad.rgbBlue  = GetBValue(colorref);
    quad.rgbReserved = 255;  // alpha opaco (utile per 32 bpp)
#endif

    if(bpp >= 24) // 24 bpp o 32 bpp (e anche 48/64 in teoria, ma vedi nota)
    {
        FreeImage_SetPixelColor(m_pImage,x,y,&quad);
    }
    else if(bpp <= 8) // 1, 4, 8 bpp -> palettizzate
    {
#ifdef FREEIMAGE_RESURRECTED
        FIRGBA8* palette = FreeImage_GetPalette(m_pImage);
#else
        RGBQUAD* palette = FreeImage_GetPalette(m_pImage);
#endif
        if(!palette)
            return; // sicurezza (raro)

        // cerca l'indice del colore piu' vicino nella palette
        BYTE bestIndex = 0;
        LONG bestDiff = LONG_MAX;

        for(int i = 0; i < (1 << bpp); ++i) // 2, 16 o 256 entrate
        {
#ifdef FREEIMAGE_RESURRECTED
            LONG dr = palette[i].red   - quad.red;
            LONG dg = palette[i].green - quad.green;
            LONG db = palette[i].blue  - quad.blue;
#else
            LONG dr = palette[i].rgbRed   - quad.rgbRed;
            LONG dg = palette[i].rgbGreen - quad.rgbGreen;
            LONG db = palette[i].rgbBlue  - quad.rgbBlue;
#endif

            LONG diff = dr*dr + dg*dg + db*db; // distanza euclidea al quadrato
            if(diff < bestDiff)
            {
                bestDiff = diff;
                bestIndex = (BYTE)i;
            }
        }

        FreeImage_SetPixelIndex(m_pImage,x,y,&bestIndex);
    }
    // altri casi (48 bpp, ecc.) non gestiti
}

/*
	GetPixels()

	I dati FreeImage sono in ordine BGR o RGB (a seconda del BPP) e normalmente bottom-up, compatibile con DIB.
*/
void* CFreeImage::GetPixels(void)
{
	// controlla che l'oggetto immagine sia valido
	if(!IsValid(__func__))
		return(NULL);
	
	return(FreeImage_GetBits(m_pImage));
}

/*
	GetBMI()

	Restituisce il puntatore alla struttura BITMAPINFO (parte dell'header del DIB).
*/
LPBITMAPINFO CFreeImage::GetBMI(void)
{
	// controlla che l'oggetto immagine sia valido
	if(!IsValid(__func__))
		return(NULL);
	
	LPBITMAPINFO pInfo=(LPBITMAPINFO)FreeImage_GetInfo(m_pImage);
	if(pInfo->bmiHeader.biHeight < 0)
		; // top-down (internamente
	else
		; // bottom-up (internamente)

	// FreeImage_GetInfo restituisce il puntatore alla BITMAPINFO del DIB
	// Nota: la BITMAPINFO e' incapsulata in m_InfoHeader, ma l'interfaccia richiede l'originale 
	// DIB, tuttavia, FreeImage_GetInfo non restituisce un puntatore alla struttura m_InfoHeader, 
	// ma al puntatore interno della FIBITMAP che contiene un BITMAPINFOHEADER valido
	return((LPBITMAPINFO)FreeImage_GetInfo(m_pImage));
}

/*
	GetMemUsed()

	Ritorna la memoria utilizzata dal DIB (bytes di pixel).
*/
UINT CFreeImage::GetMemUsed(void)
{
	// controlla che l'oggetto immagine sia valido
	if(!IsValid(__func__))
		return(0);
	
	// FIBITMAP e una struttura opaca, la dimensione e la linea * altezza
	return((UINT)FreeImage_GetDIBSize(m_pImage));
}

/*
	SetDIB()
*/
BOOL CFreeImage::SetDIB(HDIB hDib,int nOrientation/* = 1*/)
{
	// qui NON bisogna controllare se l'oggetto immagine e' valido perche' lo sta impostando ex novo
	//if(!IsValid(__func__))
	//	return(FALSE);

	if(!hDib)
	{
		SetLastErrorDescriptionEx("%s(): invalid DIB data",__func__);
		return(FALSE);
    }

    BOOL bResult = CImage::SetDIB(hDib);
    
	// FreeImage ha un comportamento inconsistente???, la documentazione dice 
	// bottom-up ma in pratica in SetDIB()/Create() si comporta come top-down
	// cambiare la GetDIBOrder() ???

    // FreeImage sembra avere orientamento invertito
    if(bResult)
		bResult = (MirrorVertical()==NO_ERROR);  // oppure FreeImage_FlipVertical(m_pImage);
    
    return(bResult);
}

/*
	SetPalette()
*/
BOOL CFreeImage::SetPalette(UINT nIndex,UINT nColors,RGBQUAD* pPalette)
{
	// controlla che l'oggetto immagine sia valido
	if(!IsValid(__func__))
		return(FALSE);

	if(!pPalette || FreeImage_GetBPP(m_pImage) > 8)
		return(FALSE);

#ifdef FREEIMAGE_RESURRECTED
	FIRGBA8* currentPalette = FreeImage_GetPalette(m_pImage);
#else
	RGBQUAD* currentPalette = FreeImage_GetPalette(m_pImage);
#endif
	if(!currentPalette)
		return(FALSE);

	for(unsigned int i = 0; i < nColors; i++)
		if(nIndex + i < FreeImage_GetColorsUsed(m_pImage))
		{
#ifdef FREEIMAGE_RESURRECTED
			// copia manuale invertendo i canali da BGR a RGB
			currentPalette[nIndex + i].red   = pPalette[i].rgbRed;
			currentPalette[nIndex + i].green = pPalette[i].rgbGreen;
			currentPalette[nIndex + i].blue  = pPalette[i].rgbBlue;
			currentPalette[nIndex + i].alpha = pPalette[i].rgbReserved;
#else
			// assegnazione diretta
			currentPalette[nIndex + i] = pPalette[i];
#endif
		}

	return(TRUE);
}

/*
	UpdateHeaderInfo()
*/
DWORD CFreeImage::UpdateHeaderInfo(void)
{
	// controlla che l'oggetto immagine sia valido
	if(!IsValid(__func__))
		return(GDI_ERROR);

	// ricava le dimensioni dell'immagine
	m_InfoHeader.bppOriginal= m_InfoHeader.bppConverted = FreeImage_GetBPP(m_pImage);
    m_InfoHeader.width		= FreeImage_GetWidth(m_pImage);
    m_InfoHeader.height		= FreeImage_GetHeight(m_pImage);
    m_InfoHeader.bpp		= FreeImage_GetBPP(m_pImage);
    m_InfoHeader.memused	= FreeImage_GetMemorySize(m_pImage);

	// numero di colori nella palette (0 x 32 bpp)
    m_InfoHeader.colors = FreeImage_GetColorsUsed(m_pImage);

    // risoluzione (DPI)
    m_InfoHeader.xres = (float)FreeImage_GetDotsPerMeterX(m_pImage) * 0.0254f; // DPM -> DPI
    m_InfoHeader.yres = (float)FreeImage_GetDotsPerMeterY(m_pImage) * 0.0254f; // DPM -> DPI

    // determina colorSpace in base al tipo FreeImage
    FREE_IMAGE_COLOR_TYPE colorType = FreeImage_GetColorType(m_pImage);
    switch(colorType) {
        case FIC_RGB:
            m_InfoHeader.colorSpace = (m_InfoHeader.bpp==32) ? COLOR_RGBA : COLOR_RGB;
            break;
        case FIC_PALETTE:
            m_InfoHeader.colorSpace = FreeImage_IsTransparent(m_pImage) ? COLOR_ALPHA : COLOR_PALETTE;
            break;
        case FIC_RGBALPHA:
            m_InfoHeader.colorSpace = COLOR_RGBA;
            break;
        case FIC_CMYK:
            m_InfoHeader.colorSpace = COLOR_CMYK;
            break;
        case FIC_MINISWHITE:
        case FIC_MINISBLACK:
            m_InfoHeader.colorSpace = COLOR_GRAYSCALE;
            break;
        default: // fallback basato su bpp
            if(m_InfoHeader.bpp==32)
                m_InfoHeader.colorSpace = COLOR_RGBA;
            else if(m_InfoHeader.bpp==24)
                m_InfoHeader.colorSpace = COLOR_RGB;
            else if(m_InfoHeader.bpp==8)
                m_InfoHeader.colorSpace = COLOR_PALETTE;
            else
                m_InfoHeader.colorSpace = COLOR_RGB;
    }

	// compressione e qualita' andrebbero estratte via metadati EXIF/APP via parsing manuale, omette per il momento
	m_InfoHeader.compression = 0;	// nessuna per default
	m_InfoHeader.quality = -1;		// sconosciuta

	return(NO_ERROR);
}

/*
	Create()
*/
BOOL CFreeImage::Create(BITMAPINFO* pBitmapInfo, void* pData)
{
	// qui NON bisogna controllare se l'oggetto immagine e' valido perche' lo sta creando ex novo
	//if(!IsValid(__func__))
	//	return(FALSE);

	if(!pBitmapInfo || !pData)
	{
		SetLastErrorDescriptionEx("%s(): invalid data",__func__);
		return(FALSE);
    }

	Unload();

    BITMAPINFOHEADER* pBih = &pBitmapInfo->bmiHeader;
    int width = pBih->biWidth;
    int height = abs(pBih->biHeight);  // biHeight puo' essere negativo (top-down)
    int bpp = pBih->biBitCount;
    
    // crea FIBITMAP
    FIBITMAP* dib = FreeImage_Allocate(width, height, bpp);
    if(!dib)
	{
		SetLastErrorDescriptionEx("%s(): memory allocation failed",__func__);
		return(FALSE);
    }

    // copia palette (se bpp <= 8)
    if (bpp <= 8) {
        RGBQUAD* pPalette = pBitmapInfo->bmiColors;
        int paletteSize = (pBih->biClrUsed > 0) ? pBih->biClrUsed : (1 << bpp);
        
        // FreeImage usa RGBQUAD come la struttura Windows
        // copia direttamente la memoria della palette
#ifdef FREEIMAGE_RESURRECTED
        FIRGBA8* fiPalette = FreeImage_GetPalette(dib);
#else
        RGBQUAD* fiPalette = FreeImage_GetPalette(dib);
#endif
        if (fiPalette) {
            memcpy(fiPalette, pPalette, paletteSize * sizeof(RGBQUAD));
        }
    }
    
    // copia dati pixel
    // FreeImage usa bottom-up (origine in basso sinistra)
    // DIB Windows puo' essere top-down (biHeight negativo) o bottom-up
    
    BYTE* fiBits  = FreeImage_GetBits(dib);
    int fiPitch   = FreeImage_GetPitch(dib);  // bytes per riga (puo' essere negativo)
    BYTE* srcBits = (BYTE*)pData;
    
    // calcola stride DIB source
    int srcStride = WIDTHBYTES((width * bpp),GetAlignment());  // allineato a 4 byte
    
    // se biHeight e' positivo (bottom-up), copia normalmente
    // se biHeight e' negativo (top-down), inverte ordine righe
    BOOL isTopDown = (pBih->biHeight < 0);
    
    for(int y = 0; y < height; y++)
	{
        int srcY = isTopDown ? y : (height - 1 - y);  // inverte se bottom-up
        BYTE* srcLine = srcBits + srcY * srcStride;
        BYTE* dstLine = fiBits + y * abs(fiPitch);  // fiPitch puo' essere negativo
        memcpy(dstLine, srcLine, srcStride);
    }
    
    // sostituisce immagine corrente
    if (m_pImage)
        FreeImage_Unload(m_pImage);
	m_pImage = dib;

	UpdateHeaderInfo();
    
    return TRUE;
}

/*
	Load()
*/
BOOL CFreeImage::Load(LPCSTR lpcszFileName,DWORD& dwError)
{
	// qui NON bisogna controllare se l'oggetto immagine e' valido perche' lo sta caricando da file
	//if(!IsValid(__func__))
	//	return(FALSE);

	dwError = (DWORD)-1L;

	if(!IsSupportedFormat(lpcszFileName))
	{
		SetLastErrorDescriptionEx("%s(): unsupported format: %s",__func__,lpcszFileName);
		return(FALSE);
	}

	Unload(); 

	// ricava il formato del file
	FREE_IMAGE_FORMAT fif = FreeImage_GetFileType(lpcszFileName,0);
	if(fif==FIF_UNKNOWN)
		fif = FreeImage_GetFIFFromFilename(lpcszFileName);
	if(fif==FIF_UNKNOWN)
		return(FALSE);

	m_pImage = FreeImage_Load(fif,lpcszFileName,0); 
	
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
			else if(stricmp(m_szFormat,"AVIF")==0)
				m_InfoHeader.type = AVIF_PICTURE;
			else if(stricmp(m_szFormat,"WEBP")==0)
				m_InfoHeader.type = WEBP_PICTURE;
		}

		// ricava i bpp originali dell'immagine
		m_InfoHeader.bppOriginal = FreeImage_GetBPP(m_pImage);

		// immagine cessa (1 BPP), oppure:
		// immagine (normalmente PNG) ad altissima risoluzione (48, 64, etc BPP)
		// riscala a 24 BPP o tutto il resto fara' il botto
		if(m_InfoHeader.bppOriginal < 8 || m_InfoHeader.bppOriginal > 32)
		{
			FIBITMAP* dib24 = FreeImage_ConvertTo24Bits(m_pImage);
			FreeImage_Unload(m_pImage);
			m_pImage = dib24;
			m_InfoHeader.bppConverted = FreeImage_GetBPP(m_pImage);
		}

		// converte a 8 bpp i 16 bpp /*in bianco e nero (scala di grigi)*/
		if(FreeImage_GetBPP(m_pImage)==16 /*&& (IsTrueGrayscale() || IsVisualGrayscaleStatistical(VISUAL_GRAYSCALE_PARAMS))*/)
		{
			FIBITMAP* dib8 = FreeImage_ConvertTo8Bits(m_pImage);
			FreeImage_Unload(m_pImage);
			m_pImage = dib8;
			m_InfoHeader.bppConverted = FreeImage_GetBPP(m_pImage);
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

		// ricava le dimensioni dell'immagine
        m_InfoHeader.width   = FreeImage_GetWidth(m_pImage);
        m_InfoHeader.height  = FreeImage_GetHeight(m_pImage);
		m_InfoHeader.bpp     = FreeImage_GetBPP(m_pImage);
        m_InfoHeader.memused = FreeImage_GetMemorySize(m_pImage);

		// numero di colori nella palette (0 x 32 bpp)
        m_InfoHeader.colors = FreeImage_GetColorsUsed(m_pImage);

        // risoluzione (DPI)
        m_InfoHeader.xres = (float)FreeImage_GetDotsPerMeterX(m_pImage) * 0.0254f; // DPM -> DPI
        m_InfoHeader.yres = (float)FreeImage_GetDotsPerMeterY(m_pImage) * 0.0254f; // DPM -> DPI

        // determina colorSpace in base al tipo FreeImage
        FREE_IMAGE_COLOR_TYPE colorType = FreeImage_GetColorType(m_pImage);
        switch(colorType) {
            case FIC_RGB:
                m_InfoHeader.colorSpace = (m_InfoHeader.bpp==32) ? COLOR_RGBA : COLOR_RGB;
                break;
            case FIC_PALETTE:
                m_InfoHeader.colorSpace = FreeImage_IsTransparent(m_pImage) ? COLOR_ALPHA : COLOR_PALETTE;
                break;
            case FIC_RGBALPHA:
                m_InfoHeader.colorSpace = COLOR_RGBA;
                break;
            case FIC_CMYK:
                m_InfoHeader.colorSpace = COLOR_CMYK;
                break;
            case FIC_MINISWHITE:
            case FIC_MINISBLACK:
                m_InfoHeader.colorSpace = COLOR_GRAYSCALE;
                break;
            default: // fallback basato su bpp
                if(m_InfoHeader.bpp==32)
                    m_InfoHeader.colorSpace = COLOR_RGBA;
                else if(m_InfoHeader.bpp==24)
                    m_InfoHeader.colorSpace = COLOR_RGB;
                else if(m_InfoHeader.bpp==8)
                    m_InfoHeader.colorSpace = COLOR_PALETTE;
                else
                    m_InfoHeader.colorSpace = COLOR_RGB;
        }

		// compressione e qualita' andrebbero estratte via metadati EXIF/APP via parsing manuale, omette per il momento
		m_InfoHeader.compression = 0;	// nessuna per default
		m_InfoHeader.quality = -1;		// sconosciuta

		dwError = 0L;

		return(TRUE);
	}
	else
		return(FALSE);
}

/*
	Unload()
*/
BOOL CFreeImage::Unload(void)
{
	if(m_pImage)
	{
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
		FreeImage_Unload(m_pImage);
		m_pImage = NULL;

		return(TRUE);
	}
	else
		return(FALSE);
}

/*
	Save()

	Salva l'immagine corrente con il nome file e nel formato specificati.
	Passare il nome (completo di estensione) con cui salvare il file ed il formato, 
	da indicare in modo diretto (senza il punto, vedi sotto).

	I formati supportati sono quelli definiti all'inizio nel costruttore.
*/
//$ corretto lo svarione, controllava il formato DOPO e non PRIMA, verificare se tutto OK
#if 0
BOOL CFreeImage::Save(LPCSTR lpcszFileName,LPCSTR lpcszFormat,DWORD dwFlags)
{
	DWORD dwError = 0L;
	BOOL bResult = FALSE;

	// controlla che l'oggetto immagine sia valido
	if(!IsValid(__func__))
		return(FALSE);

	__try {
    
		FIBITMAP* pImageToSave = m_pImage;
		BOOL bMustFree = FALSE;

		// auto-fix per il formato sorgente prima del salvataggio
		WORD bpp = FreeImage_GetBPP(m_pImage);
		switch(m_InfoHeader.type)
		{
			case BMP_PICTURE:
				if(bpp > 24)
				{
					pImageToSave = FreeImage_ConvertTo24Bits(m_pImage);
					bMustFree = TRUE;
				}
				break;

			case JPEG_PICTURE:
				if(bpp!=24 && bpp!=8)
				{
					pImageToSave = FreeImage_ConvertTo24Bits(m_pImage);
					bMustFree = TRUE;
				}
				dwFlags |= JPEG_QUALITYGOOD;
				break;

			case GIF_PICTURE:
				if(bpp!=8)
				{
					// converte a 8-bit usando l'algoritmo di quantizzazione
					//FIQ_WUQUANT: algoritmo di Wu (veloce, ottima qualita')
					//FIQ_NNQUANT: Neural Net quantization (piu' lento, ma spesso superiore per immagini fotografiche)
					//FIQ_LFPQUANT: Carsten Klein color quantization
					pImageToSave = FreeImage_ColorQuantize(m_pImage,FIQ_WUQUANT);
					bMustFree = TRUE;
					}
				break;

			case PNG_PICTURE:
				dwFlags |= PNG_Z_BEST_COMPRESSION;
				break;

			case WEBP_PICTURE:
			case AVIF_PICTURE:
				// supportano 32-bit (con Alpha), quindi solitamente non serve conversione
				// per forzare la rimozione dell'alpha per risparmiare spazio convertire a 
				// 24bpp come per JPEG
				if(!FreeImage_IsPluginEnabled(FIF_AVIF))
				{
					SetLastErrorDescriptionEx("%s(): missing plug-in",__func__);
					return(FALSE);
				}
				break;
		}

		// imposta il formato con cui salvare il file, NON usare FreeImage_GetFileType() perche' analizza
		// l'header del file sorgente ed il formato in cui si vuole salvare puo' essere differente
		FREE_IMAGE_FORMAT fif = FreeImage_GetFIFFromFilename(lpcszFileName);
		if(fif==FIF_UNKNOWN)
		{
			SetLastErrorDescriptionEx("%s(): unknown format: %s",__func__,lpcszFileName);
			return(FALSE);
		}
		else if(fif==FIF_AVIF)
		{
			SetLastErrorDescriptionEx("%s(): current version of FreeImageRe has an unbearable AVIF implementation, aborting",__func__);
			return(FALSE);
		}

		// salvataggio
		bResult = FreeImage_Save(fif,pImageToSave,lpcszFileName,(int)dwFlags);
		if(!bResult)
			SetLastErrorDescriptionEx("%s(): failed",__func__);

		// ripulisce se ha creato un buffer temporaneo
		if(bMustFree && pImageToSave)
			FreeImage_Unload(pImageToSave);
	}
	__except(dwError = GetExceptionCode(),EXCEPTION_EXECUTE_HANDLER) {

		SetLastErrorDescriptionEx("%s(): an unexpected exception (0x%08X) has occurred while saving: %s",__func__,dwError,lpcszFileName);
	}

	if(bResult)
		CImage::Flush(lpcszFileName);

	return(bResult);
}
#else
BOOL CFreeImage::Save(LPCSTR lpcszFileName,LPCSTR lpcszFormat,DWORD dwFlags)
{
	DWORD dwError = 0L;
	BOOL bResult = FALSE;

	// controlla che l'oggetto immagine sia valido
	if(!IsValid(__func__))
		return(FALSE);

	// determina prima di tutto il formato di destinazione basandosi sul file in cui sta salvando
	FREE_IMAGE_FORMAT fif = FreeImage_GetFIFFromFilename(lpcszFileName);
	if(fif==FIF_UNKNOWN)
	{
		SetLastErrorDescriptionEx("%s(): unknown format: %s",__func__,lpcszFileName);
		return(FALSE);
	}
	else if(fif==FIF_AVIF)
	{
		SetLastErrorDescriptionEx("%s(): current version of FreeImageRe has an unbearable AVIF implementation, aborting",__func__);
		return(FALSE);
	}

	__try {
    
		FIBITMAP* pImageToSave = m_pImage;
		BOOL bMustFree = FALSE;

		WORD bpp = FreeImage_GetBPP(m_pImage);

		// ora controlla il formato di destinazione (fif), che e' quello che comanda le regole di compatibilita' dei bit
		switch(fif)
		{
			case FIF_BMP:
				if(bpp > 24)
				{
					pImageToSave = FreeImage_ConvertTo24Bits(m_pImage);
					bMustFree = TRUE;
				}
				break;

			case FIF_JPEG:
				if(bpp!=24 && bpp!=8)
				{
					pImageToSave = FreeImage_ConvertTo24Bits(m_pImage);
					bMustFree = TRUE;
				}
				dwFlags |= JPEG_QUALITYGOOD;
				break;

			case FIF_GIF:
				if(bpp!=8)
				{
					// converte a 8-bit usando l'algoritmo di quantizzazione
					pImageToSave = FreeImage_ColorQuantize(m_pImage,FIQ_WUQUANT);
					bMustFree = TRUE;
				}
				break;

			case FIF_PNG:
				dwFlags |= PNG_Z_BEST_COMPRESSION;
				break;

			case FIF_WEBP:
				break;
		}

		// salvataggio
		bResult = FreeImage_Save(fif,pImageToSave,lpcszFileName,(int)dwFlags);
		if(!bResult)
			SetLastErrorDescriptionEx("%s(): failed",__func__);

		// ripulisce se ha creato un buffer temporaneo
		if(bMustFree && pImageToSave)
			FreeImage_Unload(pImageToSave);
	}
	__except(dwError = GetExceptionCode(),EXCEPTION_EXECUTE_HANDLER) {

		SetLastErrorDescriptionEx("%s(): an unexpected exception (0x%08X) has occurred while saving: %s",__func__,dwError,lpcszFileName);
	}

	if(bResult)
		CImage::Flush(lpcszFileName);

	return(bResult);
}
#endif

/*
	Stretch()
*/
UINT CFreeImage::Stretch(RECT& rc, BOOL keepAspectRatio/* = TRUE */)
{
	// controlla che l'oggetto immagine sia valido
	if(!IsValid(__func__))
        return(GDI_ERROR);
    
    // dimensioni attuali dell'immagine
    int currentWidth = FreeImage_GetWidth(m_pImage);
    int currentHeight = FreeImage_GetHeight(m_pImage);
    
    // calcola le dimensioni del rettangolo di destinazione
    int destWidth = rc.right - rc.left;
    int destHeight = rc.bottom - rc.top;
    
    int newWidth, newHeight;
    
	// l'algoritmo riempie tutto il rettangolo, potrebbe uscire dai bordi - FILL
    if(keepAspectRatio)
    {
        float scaleX = (float)destWidth / currentWidth;
        float scaleY = (float)destHeight / currentHeight;
        float scale = (scaleX > scaleY) ? scaleX : scaleY;
        
        newWidth = (int)(currentWidth * scale + 0.5f);
        newHeight = (int)(currentHeight * scale + 0.5f);
    }
    else
    {
        // non mantiene proporzioni (stretch semplice)
        newWidth = destWidth;
        newHeight = destHeight;
    }
    
    // Controlla dimensioni valide
    if(newWidth <= 0) newWidth = 1;
    if(newHeight <= 0) newHeight = 1;
    
    /*
    FILTER_BICUBIC (compromesso qualita'/velocita')
    FILTER_BILINEAR (più veloce)
    FILTER_LANCZOS3 (qualità migliore ma più lento)
    FILTER_BOX (più veloce, qualità minore)
    */
    FREE_IMAGE_FILTER filter = FILTER_LANCZOS3;
    
    FIBITMAP* pNewDib = FreeImage_Rescale(m_pImage, newWidth, newHeight, filter);
    
    if(pNewDib)
    {
        FreeImage_Unload(m_pImage);
        m_pImage = pNewDib;
		UpdateHeaderInfo();
        return(NO_ERROR);
    }
	else
	{
		SetLastErrorDescriptionEx("%s(): unable to stretch the image",__func__);
		return(GDI_ERROR);
	}
}

/*
	Blur()

	Il radius (raggio) controlla quanto e' forte il blur:

	significato:
	radius = 0: Nessun blur (immagine originale)
	radius = 1: Blur molto leggero (3x3 pixel area)
	radius = 2: Blur moderato (5x5 pixel area)
	radius = 3: Blur evidente (7x7 pixel area)
	radius = 5: Blur forte (11x11 pixel area)
	radius = 10: Blur molto forte (21x21 pixel area)

	formula:
	Kernel size = radius * 2 + 1
	radius=1 -> 3x3 kernel
	radius=2 -> 5x5 kernel
	radius=3 -> 7x7 kernel

	valori raccomandati:
	#define BLUR_SUBTLE   1  // effetto leggero
	#define BLUR_MEDIUM   3  // effetto medio
	#define BLUR_STRONG   5  // effetto forte
	#define BLUR_EXTREME  10 // effetto molto forte

	limiti pratici:
	Minimo: 0 (no blur)
	Massimo: Dipende dalla dimensione dell'immagine (max 20 ?)
	performance: piu' grande il radius, piu' lento il calcolo
*/
UINT CFreeImage::Blur(void) // radius
{
	// controlla che l'oggetto immagine sia valido
	if(!IsValid(__func__))
		return(GDI_ERROR);

	int nFactor = 0; // <- radius
	double nValue = 0;
	if(GetFilterParams("Blur",nValue))
		nFactor = (int)nValue;
	else
	{
		SetLastErrorDescriptionEx("%s(): ERROR_INVALID_PARAMETER",__func__);
		return(ERROR_INVALID_PARAMETER);
	}

	// converte a 32 bpp
	FIBITMAP* dib32 = FreeImage_ConvertTo32Bits(m_pImage);
	if(!dib32)
	{
		SetLastErrorDescriptionEx("%s(): unable to apply the blur filter",__func__);
		return(GDI_ERROR);
	}
    
	// crea una copia per il risultato
	FIBITMAP* blurred = FreeImage_Clone(dib32);
	if(!blurred)
	{
		FreeImage_Unload(dib32);
		SetLastErrorDescriptionEx("%s(): unable to clone the image",__func__);
		return(GDI_ERROR);
	}

	// applica blur gaussiano manuale
	GaussianBlur32(dib32,blurred,nFactor);
    
	// sostituisce l'immagine originale
	FreeImage_Unload(m_pImage);
	m_pImage = blurred;
	FreeImage_Unload(dib32);
    UpdateHeaderInfo();

	return(NO_ERROR);
}

/*
	GaussianBlur32()
*/
void CFreeImage::GaussianBlur32(FIBITMAP* src, FIBITMAP* dst,int radius)
{
    int width = FreeImage_GetWidth(src);
    int height = FreeImage_GetHeight(src);
    
    // Kernel gaussiano
    float sigma = (float)radius / 2.0f;
    int kernelSize = radius * 2 + 1;
    float* kernel = new float[kernelSize];
    
    // calcola kernel gaussiano
    float sum = 0.0f;
    for(int i = 0; i < kernelSize; i++)
    {
        int x = i - radius;
        kernel[i] = (float)exp(-(x * x) / (2 * sigma * sigma));
        sum += kernel[i];
    }
    
    // normalizza kernel
    for(int i = 0; i < kernelSize; i++)
        kernel[i] /= sum;
    
    // lock bitmaps
    BYTE* srcBits = FreeImage_GetBits(src);
    BYTE* dstBits = FreeImage_GetBits(dst);
    int pitch = FreeImage_GetPitch(src);
    
    // blur orizzontale
    for(int y = 0; y < height; y++)
    {
        BYTE* srcRow = srcBits + y * pitch;
        BYTE* dstRow = dstBits + y * pitch;
        
        for(int x = 0; x < width; x++)
        {
            float r = 0, g = 0, b = 0, a = 0;
            
            for(int k = 0; k < kernelSize; k++)
            {
                int px = x + k - radius;
                if (px < 0) px = 0;
                if (px >= width) px = width - 1;
                
                BYTE* pixel = srcRow + px * 4;
                float weight = kernel[k];
                
                b += pixel[0] * weight;
                g += pixel[1] * weight;
                r += pixel[2] * weight;
                a += pixel[3] * weight;
            }
            
            BYTE* dstPixel = dstRow + x * 4;
            dstPixel[0] = (BYTE)(b + 0.5f);
            dstPixel[1] = (BYTE)(g + 0.5f);
            dstPixel[2] = (BYTE)(r + 0.5f);
            dstPixel[3] = (BYTE)(a + 0.5f);
        }
    }
    
    // blur verticale (sul risultato dell'orizzontale)
    for(int x = 0; x < width; x++)
    {
        for(int y = 0; y < height; y++)
        {
            float r = 0, g = 0, b = 0, a = 0;
            
            for(int k = 0; k < kernelSize; k++)
            {
                int py = y + k - radius;
                if (py < 0) py = 0;
                if (py >= height) py = height - 1;
                
                BYTE* pixel = dstBits + py * pitch + x * 4;
                float weight = kernel[k];
                
                b += pixel[0] * weight;
                g += pixel[1] * weight;
                r += pixel[2] * weight;
                a += pixel[3] * weight;
            }
            
            BYTE* dstPixel = dstBits + y * pitch + x * 4;
            dstPixel[0] = (BYTE)(b + 0.5f);
            dstPixel[1] = (BYTE)(g + 0.5f);
            dstPixel[2] = (BYTE)(r + 0.5f);
            dstPixel[3] = (BYTE)(a + 0.5f);
        }
    }
    
    delete[] kernel;
}

/*
	Brightness()
*/
UINT CFreeImage::Brightness(void)
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

	if(FreeImage_AdjustBrightness(m_pImage,nValue))
		return(NO_ERROR);
	else
	{
		SetLastErrorDescriptionEx("%s(): unable to apply the brightness filter",__func__);
		return(GDI_ERROR);
	}
}

/*
	ColorSwap()
*/
UINT CFreeImage::ColorSwap(void)
{
	// controlla che l'oggetto immagine sia valido
	if(!IsValid(__func__))
		return(GDI_ERROR);

	// non applica a un immagine che e'/sembra grayscale, restituisce errore
	if(IsTrueGrayscale() || CImage::IsVisualGrayscaleStatistical(VISUAL_GRAYSCALE_PARAMS))
	{
		SetLastErrorDescriptionEx("%s(): image is grayscale, filter will not be applied",__func__);
		return(GDI_ERROR);
	}

	// ricava il valore del parametro    
	int nFactor = 0;
	double nValue = 0;
	if(GetFilterParams("ColorSwap",nValue))
		nFactor = (int)nValue;
	else
	{
		SetLastErrorDescriptionEx("%s(): ERROR_INVALID_PARAMETER",__func__);
		return(ERROR_INVALID_PARAMETER);
	}

	// nulla da elaborare, lascia l'immagine come sta'
	if(nFactor==COLORSWAP_IDENTITY)
		return(NO_ERROR);

	if(FreeImage_GetBPP(m_pImage) < 24)
	{
		FIBITMAP* dib24 = FreeImage_ConvertTo24Bits(m_pImage);
		if(!dib24)
		{
			SetLastErrorDescriptionEx("%s(): unable to convert the image",__func__);
			return(GDI_ERROR);
		}
		FreeImage_Unload(m_pImage);
		m_pImage = dib24;
		UpdateHeaderInfo();
	}

	UINT nRet = CImage::ColorSwap();
	if(nRet!=NO_ERROR)
		SetLastErrorDescriptionEx("%s(): unable to apply the colorswap filter",__func__);

	return(nRet);
}

/*
	Contrast()
*/
UINT CFreeImage::Contrast(void)
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

	if(FreeImage_AdjustContrast(m_pImage,nValue))
		return(NO_ERROR);
	else
	{
		SetLastErrorDescriptionEx("%s(): unable to apply the contrast filter",__func__);
		return(GDI_ERROR);
	}
}

/*
	GammaCorrection()
*/
UINT CFreeImage::GammaCorrection(void)
{
	// controlla che l'oggetto immagine sia valido
	if(!IsValid(__func__))
		return(GDI_ERROR);

	double nValue = 0;
	if(!GetFilterParams("GammaCorrection",nValue))
	{
		SetLastErrorDescriptionEx("%s(): ERROR_INVALID_PARAMETER",__func__);
		return(ERROR_INVALID_PARAMETER);
	}

	if(FreeImage_AdjustGamma(m_pImage,nValue))
		return(NO_ERROR);
	else
	{
		SetLastErrorDescriptionEx("%s(): unable to apply the gamma correction filter",__func__);
		return(GDI_ERROR);
	}
}

/*
	Grayscale()
*/
UINT CFreeImage::Grayscale(void)
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

	FIBITMAP* pNewDib = FreeImage_ConvertToGreyscale(m_pImage);
	if(pNewDib)
	{
		FreeImage_Unload(m_pImage); 
		m_pImage = pNewDib;
		UpdateHeaderInfo();
		return(NO_ERROR);
	}
	else
	{
		SetLastErrorDescriptionEx("%s(): unable to apply the grayscale filter",__func__);
		return(GDI_ERROR);
	}
}

/*
	Hue()
*/
UINT CFreeImage::Hue(void)
{
	// controlla che l'oggetto immagine sia valido
	if(!IsValid(__func__))
		return(GDI_ERROR);

	double nValue = 0;
	if(!GetFilterParams("Hue",nValue))
	{
		SetLastErrorDescriptionEx("%s(): ERROR_INVALID_PARAMETER",__func__);
		return(ERROR_INVALID_PARAMETER);
	}

	if(fabs(nValue) < 0.01)
		return(NO_ERROR);

	// non applica a un immagine che e'/sembra grayscale, restituisce errore
	if(IsTrueGrayscale() || IsVisualGrayscaleStatistical(VISUAL_GRAYSCALE_PARAMS))
	{
		SetLastErrorDescriptionEx("%s(): image is grayscale, filter will not be applied",__func__);
		return(GDI_ERROR);
	}

	FIBITMAP *pDib24 = NULL;
	if(FreeImage_GetBPP(m_pImage)!=24)
		pDib24 = FreeImage_ConvertTo24Bits(m_pImage);
	else
		pDib24 = FreeImage_Clone(m_pImage);
	if(!pDib24)
	{
		SetLastErrorDescriptionEx("%s(): unable to convert image",__func__);
		return(GDI_ERROR);
    }

	int width = FreeImage_GetWidth(pDib24);
	int height = FreeImage_GetHeight(pDib24);
	int pitch = FreeImage_GetPitch(pDib24); 
	BYTE *bits = FreeImage_GetBits(pDib24);

	// converte gradi a frazione di cerchio (0-1), 360 gradi = 1.0 in spazio HSL
	double degrees = nValue; // gradi: -180 a 180
	double hueShift = degrees / 360.0;
    
	for(int y = 0; y < height; y++) 
	{
		BYTE *pixel = bits + y * pitch;
		for(int x = 0; x < width; x++) 
		{
			BYTE b = pixel[FI_RGBA_BLUE];
			BYTE g = pixel[FI_RGBA_GREEN];
			BYTE r = pixel[FI_RGBA_RED];
            
			double h, s, l;
			CImage::RGBtoHSL(RGB(r, g, b), &h, &s, &l);  // h in 0-1
            
			// applica shift (in unita' 0-1)
			h += hueShift;
            
			// normalizzazione (0-1)
			if(h < 0.0)  h += 1.0;
			if(h >= 1.0) h -= 1.0;
            
			COLORREF newColor = CImage::HLStoRGB(h, l, s);
            
			pixel[FI_RGBA_BLUE]  = GetBValue(newColor);
			pixel[FI_RGBA_GREEN] = GetGValue(newColor);
			pixel[FI_RGBA_RED]   = GetRValue(newColor);

			pixel += 3;
		}
	}
    
	FreeImage_Unload(m_pImage);
	m_pImage = pDib24;
	UpdateHeaderInfo();
    
	return(NO_ERROR);
}

/*
	Negate()
*/
UINT CFreeImage::Negate(void)
{
	// controlla che l'oggetto immagine sia valido
	if(!IsValid(__func__))
		return(GDI_ERROR);

	if(FreeImage_Invert(m_pImage))
		return(NO_ERROR);
	else
	{
		SetLastErrorDescriptionEx("%s(): unable to apply the negate filter",__func__);
		return(GDI_ERROR);
    }
}

/*
	MirrorHorizontal()
	Specchia l'immagine sull'asse Y (FreeImage_FlipHorizontal)
*/
UINT CFreeImage::MirrorHorizontal(void)
{
	// controlla che l'oggetto immagine sia valido
	if(!IsValid(__func__))
		return(GDI_ERROR);

	if(FreeImage_FlipHorizontal(m_pImage))
		return(NO_ERROR);
	else
	{
		SetLastErrorDescriptionEx("%s(): unable to apply the mirror horiz. filter",__func__);
		return(GDI_ERROR);
    }
}

/*
	MirrorVertical()
	Specchia l'immagine sull'asse X (FreeImage_FlipVertical)
*/
UINT CFreeImage::MirrorVertical(void)
{
	// controlla che l'oggetto immagine sia valido
	if(!IsValid(__func__))
		return(GDI_ERROR);

	if(FreeImage_FlipVertical(m_pImage))
		return(NO_ERROR);
	else
	{
		SetLastErrorDescriptionEx("%s(): unable to apply the mirror vert. filter",__func__);
		return(GDI_ERROR);
    }
}

/*
	Posterize()

	Implementato tramite manipolazione manuale dei pixel.

	nFactor = 2 -> solo bianco e nero
	nFactor = 4 -> 4 livelli per canale (64 colori totali)
	nFactor = 8 -> 8 livelli (512 colori)
	nFactor = 250 -> quasi nessun effetto (come deve essere)
*/
UINT CFreeImage::Posterize(void)
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

	// calcola il fattore di quantizzazione
	float quantizeFactor = 255.0f / (nFactor - 1);
    int width = FreeImage_GetWidth(m_pImage);
	int height = FreeImage_GetHeight(m_pImage);
	int bpp = FreeImage_GetBPP(m_pImage);
    
	switch(bpp)
	{
		case 32:
			for(int y = 0; y < height; y++)
			{
				BYTE* bits = FreeImage_GetScanLine(m_pImage,y);
				for(int x = 0; x < width; x++)
				{
					bits[FI_RGBA_BLUE]  = (BYTE)((int)(bits[FI_RGBA_BLUE]  / quantizeFactor + 0.5f) * quantizeFactor);
					bits[FI_RGBA_GREEN] = (BYTE)((int)(bits[FI_RGBA_GREEN] / quantizeFactor + 0.5f) * quantizeFactor);
					bits[FI_RGBA_RED]   = (BYTE)((int)(bits[FI_RGBA_RED]   / quantizeFactor + 0.5f) * quantizeFactor);
					bits += 4;
				}
			}
			break;
        
		case 24:
			for(int y = 0; y < height; y++)
			{
				BYTE* bits = FreeImage_GetScanLine(m_pImage,y);
				for(int x = 0; x < width; x++)
				{
					bits[FI_RGBA_BLUE]  = (BYTE)((int)(bits[FI_RGBA_BLUE]  / quantizeFactor + 0.5f) * quantizeFactor);
					bits[FI_RGBA_GREEN] = (BYTE)((int)(bits[FI_RGBA_GREEN] / quantizeFactor + 0.5f) * quantizeFactor);
					bits[FI_RGBA_RED]   = (BYTE)((int)(bits[FI_RGBA_RED]   / quantizeFactor + 0.5f) * quantizeFactor);
					bits += 3;
				}
			}
			break;
        
		case 8:
			for(int y = 0; y < height; y++)
			{
				BYTE* bits = FreeImage_GetScanLine(m_pImage,y);
				for(int x = 0; x < width; x++)
				{
					*bits = (BYTE)((int)(*bits / quantizeFactor + 0.5f) * quantizeFactor);
					bits++;
				}
			}
			break;

		default:
			// converte e riprocessa
			FIBITMAP* dib24 = FreeImage_ConvertTo24Bits(m_pImage);
			if(!dib24)
			{
				SetLastErrorDescriptionEx("%s(): unable to convert image",__func__);
				return(GDI_ERROR);
			}
			FreeImage_Unload(m_pImage);
			m_pImage = dib24;
			UpdateHeaderInfo();
			return(this->Posterize());
	}

	return(NO_ERROR);
}

/*
	Rotate90Left()
*/
UINT CFreeImage::Rotate90Left(void)
{
	// controlla che l'oggetto immagine sia valido
	if(!IsValid(__func__))
		return(GDI_ERROR);

	FIBITMAP* pNewDib = FreeImage_Rotate(m_pImage,90.0);
	if(pNewDib)
	{
		FreeImage_Unload(m_pImage);
		m_pImage = pNewDib;
		UpdateHeaderInfo();
		return(NO_ERROR);
	}
	else
	{
		SetLastErrorDescriptionEx("%s(): unable to apply the rotate 90 left filter",__func__);
		return(GDI_ERROR);
	}
}

/*
	Rotate90Right()
*/
UINT CFreeImage::Rotate90Right(void)
{
	// controlla che l'oggetto immagine sia valido
	if(!IsValid(__func__))
		return(GDI_ERROR);

	FIBITMAP* pNewDib = FreeImage_Rotate(m_pImage,270.0);
	if(pNewDib)
	{
		FreeImage_Unload(m_pImage);
		m_pImage = pNewDib;
		UpdateHeaderInfo();
		return(NO_ERROR);
	}
	else
	{
		SetLastErrorDescriptionEx("%s(): unable to apply the rotate 90 right filter",__func__);
		return(GDI_ERROR);
	}
}

/*
	Rotate180()
*/
UINT CFreeImage::Rotate180(void)
{
	// controlla che l'oggetto immagine sia valido
	if(!IsValid(__func__))
		return(GDI_ERROR);

	FIBITMAP* pNewDib = FreeImage_Rotate(m_pImage,180.0);
	if(pNewDib)
	{
		FreeImage_Unload(m_pImage);
		m_pImage = pNewDib;
		UpdateHeaderInfo();
		return(NO_ERROR);
	}
	else
	{
		SetLastErrorDescriptionEx("%s(): unable to apply the rotate 180 filter",__func__);
		return(GDI_ERROR);
	}
}

/*
    Sharpen()
*/
UINT CFreeImage::Sharpen(void)
{
	// controlla che l'oggetto immagine sia valido
	if(!IsValid(__func__))
		return(GDI_ERROR);

	int nFactor = 0;
	double nValue = 0;
	if(GetFilterParams("Sharpen",nValue))
	{
		nFactor = (int)nValue;
		if(nFactor==0)
			return(NO_ERROR);
	}
	else
	{
		SetLastErrorDescriptionEx("%s(): ERROR_INVALID_PARAMETER",__func__);
		return(ERROR_INVALID_PARAMETER);
	}
    
	return(SharpenUnsharpMask(nFactor));
}

/*
	SharpenUnsharpMask()
*/
UINT CFreeImage::SharpenUnsharpMask(int intensity)
{
    // unsharp mask = originale + (originale - blurred) * fattore

    // crea una copia sfocata (blur gaussiano 3x3)
    FIBITMAP *pBlurred = FreeImage_Clone(m_pImage);
    if(!pBlurred)
	{
		SetLastErrorDescriptionEx("%s(): unable to clone the image",__func__);
		return(GDI_ERROR);
	}
    
    // applica blur semplice (media 3x3)
    int width = FreeImage_GetWidth(m_pImage);
    int height = FreeImage_GetHeight(m_pImage);
    int bpp = FreeImage_GetBPP(m_pImage);
    
    // converte a 24-bit per semplicita'
    if(bpp!=24)
    {
		FIBITMAP *dib24 = FreeImage_ConvertTo24Bits(m_pImage);
        if(!dib24)
        {
            FreeImage_Unload(pBlurred);
			SetLastErrorDescriptionEx("%s(): unable to convert the image",__func__);
            return(GDI_ERROR);
        }
        FreeImage_Unload(m_pImage);
        m_pImage = dib24;
        bpp = 24;
        UpdateHeaderInfo();
    }
    
    if(FreeImage_GetBPP(pBlurred)!=24)
    {
        FIBITMAP *dib24 = FreeImage_ConvertTo24Bits(pBlurred);
        FreeImage_Unload(pBlurred);
        pBlurred = dib24;
        if(!pBlurred)
		{
			SetLastErrorDescriptionEx("%s(): unable to convert the image",__func__);
            return(GDI_ERROR);
		}
    }
    
    // applica blur gaussiano 3x3
    int blurPitch = FreeImage_GetPitch(pBlurred);
    BYTE *blurBits = FreeImage_GetBits(pBlurred);
    
    FIBITMAP *pBlurTemp = FreeImage_Clone(pBlurred);
    if(!pBlurTemp)
    {
        FreeImage_Unload(pBlurred);
		SetLastErrorDescriptionEx("%s(): unable to clone the image",__func__);
		return(GDI_ERROR);
    }
    
    BYTE *blurSrc = FreeImage_GetBits(pBlurTemp);
    int blurSrcPitch = FreeImage_GetPitch(pBlurTemp);
    
    // Kernel blur gaussiano 3x3
    float blurKernel[3][3] = {
        {1/16.0f, 2/16.0f, 1/16.0f},
        {2/16.0f, 4/16.0f, 2/16.0f},
        {1/16.0f, 2/16.0f, 1/16.0f}
    };
    
    for(int y = 1; y < height - 1; y++)
    {
        for(int x = 1; x < width - 1; x++)
        {
            float b = 0, g = 0, r = 0;
            
            for(int ky = -1; ky <= 1; ky++)
            {
                for(int kx = -1; kx <= 1; kx++)
                {
                    BYTE *p = blurSrc + (y + ky) * blurSrcPitch + (x + kx) * 3;
                    float weight = blurKernel[ky + 1][kx + 1];
                    
                    b += p[0] * weight;
                    g += p[1] * weight;
                    r += p[2] * weight;
                }
            }
            
            BYTE *dst = blurBits + y * blurPitch + x * 3;
            dst[0] = (BYTE)min(max((int)(b + 0.5f), 0), 255);
            dst[1] = (BYTE)min(max((int)(g + 0.5f), 0), 255);
            dst[2] = (BYTE)min(max((int)(r + 0.5f), 0), 255);
        }
    }
    
    FreeImage_Unload(pBlurTemp);
    
    // ora applica Unsharp Mask sull'immagine originale
    int pitch = FreeImage_GetPitch(m_pImage);
    BYTE *origBits = FreeImage_GetBits(m_pImage);
    
    // intensita' (0.0 - 2.0 per essere visibile)
    float factor = intensity / 50.0f;
    
    for(int y = 1; y < height - 1; y++)
    {
        for(int x = 1; x < width - 1; x++)
        {
            BYTE *pOrig = origBits + y * pitch + x * 3;
            BYTE *pBlur = blurBits + y * blurPitch + x * 3;
            
            // formula: sharpened = original + (original - blurred) * factor
            int b = pOrig[0] + (int)((pOrig[0] - pBlur[0]) * factor);
            int g = pOrig[1] + (int)((pOrig[1] - pBlur[1]) * factor);
            int r = pOrig[2] + (int)((pOrig[2] - pBlur[2]) * factor);
            
            pOrig[0] = (BYTE)min(max(b, 0), 255);
            pOrig[1] = (BYTE)min(max(g, 0), 255);
            pOrig[2] = (BYTE)min(max(r, 0), 255);
        }
    }
    
    FreeImage_Unload(pBlurred);
    
    return(NO_ERROR);
}

/*
	IsValid()
*/
BOOL CFreeImage::IsValid(LPCSTR lpcszFunctionName)
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

#endif // HAVE_FREEIMAGE_LIBRARY
