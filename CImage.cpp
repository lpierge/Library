/*$
	CImage.cpp
	Classe base per l'interfaccia con le librerie.
	Luca Piergentili, 01/09/00
	lpiergentili@yahoo.com

	Vedi le note in CImage.h

	Ad memoriam - Nemo me impune lacessit.

	TODO:
	- revisione generale, sopratutto i filtri
	- aggiustare LockData/UnlockData, attualmente inconsistenti/inesistenti
	- decidere se lasciare qui ConvertToBPP() o se implementarla solo nelle derivate
*/

// min e max definite da Windows come macro vanno in conflitto con le min max di std::
// quindi o si definisce NOMINMAX in modo che Windows non definisca le macro (ma si 
// ricevera' errore quando si usino), o si usa la sintassi (str::max)(...) dato che 
// racchiudere il simbolo tra parentesi evita la macro sostituzione del preprocessore
//#define NOMINMAX

#include "env.h"
#include "pragma.h"
#include "macro.h"
#include "algorithm.h"
#include "fastrand.h"
#include <string.h>
#include <time.h>
#define _USE_MATH_DEFINES 1
#include <math.h>
#ifndef M_PI
  #define M_PI 3.14159265358979323846
#endif
#include "window.h"
#include "win32api.h"
#include "CImage.h"
#include "CNodeList.h"

// JPG
#if 1
  // libjpeg.h: header custom, include #pragma comment,lib ed i files specificati nell'altro ramo della if
  // paintLib.lib non definisce i simboli jpeg, quindi puo' referenziare la libreria libjpeg.lib
  #include "libjpeg.h"
#else
  #include "jpeglib.h"
  #include "jmemsrc.h"
#endif

// TIFF
#if 1
  // libtiff.h: header custom, include #pragma comment,lib ed i files specificati nell'altro ramo della if
  // paintLib.lib gia' contiene la definizione dei simboli tiff, quindi se usa la #pragma comment,lib per istruire 
  // il linker ad usare libtiff.lib, questi dara' errore per simbolo duplicato (gia' presente in paintLib.lib)
  // per questo include manualmente solo gli headers
  #include "libtiff.h"
#else
  #include "libtiff/tiff.h"
  #include "libtiff/tiffio.h"
  #include "libtiff/tiffiop.h"
  #include "tif_msrc.h"
#endif

// WebP
// libwebp.h: header custom, include #pragma comment,lib ed i files include necessari
#include "libwebp.h"

// HEIF (per HEIC/AVIF)
//libheif.h: header custom, include #pragma comment,lib ed i files include necessari
#include "libheif.h"

#include <algorithm>	// per std::sort
#include <vector>		// per std::vector
#include <utility>		// per std::pair

#include "traceexpr.h"
//#define _TRACE_FLAG			_TRFLAG_TRACEOUTPUT // opzioni: _TRFLAG_NOTRACE, _TRFLAG_TRACEFILE, _TRFLAG_TRACECONSOLE, _TRFLAG_TRACEOUTPUT, _TRFLAG_TRACEBREAKPOINT
#define _TRACE_FLAG			_TRFLAG_NOTRACE // opzioni: _TRFLAG_NOTRACE, _TRFLAG_TRACEFILE, _TRFLAG_TRACECONSOLE, _TRFLAG_TRACEOUTPUT, _TRFLAG_TRACEBREAKPOINT
#define _TRACE_FLAG_INFO	_TRACE_FLAG
#define _TRACE_FLAG_WARN	_TRACE_FLAG
#define _TRACE_FLAG_ERR		_TRACE_FLAG

/*
	CImage()
*/
CImage::CImage()
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

	// il puntatore all'oggetto immagine lo creano le derivate
	// ...

	// i tipi files riconosciuti li definiscono le derivate
	// ...

	// range e valori di default per i filtri implementati
 	SetFilterParams("Blur",				0,1,0.15,		"","",False,&CImageObject::Blur);
 	SetFilterParams("Brightness",		-100,100,40,	"","",False,&CImageObject::Brightness);
	SetFilterParams("ColorShift",		0,0,0,			"-255:255;-255:255;-255:255;0:2;T/F","20;10;-30;1;FALSE",False,&CImageObject::ColorShift);
	SetFilterParams("ColorSwap",		COLORSWAP_IDENTITY,COLORSWAP_RANDOM,COLORSWAP_ROTATE_REV,"","",False,&CImageObject::ColorSwap); // rotazione colori
//	SetFilterParams("ColorSwap",		COLORSWAP_IDENTITY,COLORSWAP_RANDOM,COLORSWAP_RANDOM,"","",False,&CImageObject::ColorSwap); // grayscale con puntinato
	SetFilterParams("Contrast",			-100,100,40,	"","",False,&CImageObject::Contrast);
	SetFilterParams("Echo",				0,0,0,			"0:10;0:100;0:100;T/F","3;25;50;TRUE",False,&CImageObject::Echo);
	SetFilterParams("Equalize",			0,0,0,			"0:2;0.0:1.0","2;0.5",False,&CImageObject::Equalize);
	SetFilterParams("GammaCorrection",	0,10,0.8,		"","",False,&CImageObject::GammaCorrection);
	SetFilterParams("GhostTrail",		0,0,0,			"1:10;1:50;0.0:1.0;T/F","3;15;0.9f;TRUE",False,&CImageObject::GhostTrail);
	SetFilterParams("Grain",			0,100,30,		"","",False,&CImageObject::Grain);
	SetFilterParams("HalftoneBW",		0,0,0,			"","",False,&CImageObject::HalftoneBW);
	SetFilterParams("HalftoneColor",	0,0,0,			"","",False,&CImageObject::HalftoneColor);
	SetFilterParams("Hue",				-180,180,90,	"","",False,&CImageObject::Hue);
	SetFilterParams("JitterHorizontal",	0,16,4,			"","",False,&CImageObject::JitterHorizontal);
	SetFilterParams("JitterSinusoidal",	0,50,15,		"","",False,&CImageObject::JitterSinusoidal);
	SetFilterParams("Median",			0,1,0.5,		"","",False,&CImageObject::Median);
	SetFilterParams("MirrorHorizontal",	0,0,0,			"","",False,&CImageObject::MirrorHorizontal);
	SetFilterParams("MirrorVertical",	0,0,0,			"","",False,&CImageObject::MirrorVertical);
	SetFilterParams("Negate",			0,0,0,			"","",False,&CImageObject::Negate);
	SetFilterParams("Noise",			0,0,0,			"0:2;0:100","0;40",False,&CImageObject::Noise);
	SetFilterParams("Pixelate",			2,32,8,			"","",False,&CImageObject::Pixelate);
	SetFilterParams("PixelSort",		0,0,0,			"0:3;0:3;0:255;0:255","1;3;128;255",False,&CImageObject::PixelSort);
	SetFilterParams("Posterize",		2,255,7,		"","",False,&CImageObject::Posterize); // 2 extra strong, degradando rapidamente a partire da 10
	SetFilterParams("Saturation",		-100,100,50,	"","",False,&CImageObject::Saturation);
	SetFilterParams("Sharpen",			-100,100,50,	"","",False,&CImageObject::Sharpen);
	SetFilterParams("Test",				0,0,0,			"","",False,&CImageObject::Test);
}

/*
	GetFileName()
*/
LPCSTR CImage::GetFileName(void)
{
	char *p = strrchr(m_szFileName,'\\');
	if(p)
		p++;
	if(!p)
		p = m_szFileName;
	return(p);
}

/*
	SetURes()
*/
void CImage::SetURes(UINT nURes)
{
	if(m_InfoHeader.restype!=(int)nURes && m_InfoHeader.restype!=RESUNITNONE && nURes!=RESUNITNONE && m_InfoHeader.restype!=-1)
	{
		switch(nURes)
		{
			case RESUNITINCH:
			{
				// centimetri
				float nXRes = GetXRes() / 2.54f;
				float nYRes = GetYRes() / 2.54f;
				SetXRes(nXRes);
				SetYRes(nYRes);
				break;
			}

			case RESUNITCENTIMETER:
			{
				// inch
				float nXRes = GetXRes() * 2.54f;
				float nYRes = GetYRes() * 2.54f;
				SetXRes(nXRes);
				SetYRes(nYRes);
				break;
			}

			default:
				return;
		}
	}

	m_InfoHeader.restype = nURes;
}

/*
	GetPhotometric()
*/
PHOTOMETRIC CImage::GetPhotometric(void)
{
	BITMAPINFO* pBmi = GetBMI();
	BITMAPINFOHEADER* pBitmapInfoHeader = (BITMAPINFOHEADER*)pBmi;
	PHOTOMETRIC photometric = NOPHOTOMETRIC;
		
	if(pBitmapInfoHeader)
	{
		switch(pBitmapInfoHeader->biBitCount)
		{
			case 1:
			{
				if(pBmi->bmiColors[0].rgbRed==0 && pBmi->bmiColors[0].rgbGreen==0 && pBmi->bmiColors[0].rgbBlue==0)
					photometric = PHOTOMETRICMINISBLACK;
				else
					photometric = PHOTOMETRICMINISWHITE;
				break;
			}

			case 4:
			case 8:
				photometric = PHOTOMETRICPALETTE;
				break;
			
			default:
				photometric = PHOTOMETRICRGB;
				break;
		}
	}

	return(photometric);
}

/*
	GetNumColors()
*/
UINT CImage::GetNumColors(void)
{
    UINT nNumColors = 0;
    
    // verifica prima se esiste un valore esplicito in biClrUsed
    LPBITMAPINFO pBMI = GetBMI();
    if(pBMI && pBMI->bmiHeader.biClrUsed > 0)
        return(pBMI->bmiHeader.biClrUsed);
    
    // altrimenti, usa il massimo teorico
    switch(GetBPP())
    {
        case 1:  nNumColors = 2;   break;
        case 4:  nNumColors = 16;  break;
        case 8:  nNumColors = 256; break;
        // 24/32-bit: nNumColors rimane 0
    }
    
    return(nNumColors);
}

/*
	CountBWColors()
*/
BOOL CImage::CountBWColors(unsigned int* pColors,unsigned char nNumColors)
{
	BOOL bCount = FALSE;
	int  nBitsPerPixel  = GetBPP();
	int  nHeight        = GetHeight();
	int  nWidth         = GetWidth();
	int  nColBytesReal  = (nWidth * nBitsPerPixel) / 8; 
	int  nBitsEnd       = (nWidth * nBitsPerPixel) % 8;
	int  nColBytes      = ((((nWidth * nBitsPerPixel) + 31) & ~31) >> 3);
	int  nRestBytesAlig = nColBytes - nColBytesReal - (nBitsEnd ? 1 : 0);
	
	if(LockData())
	{
		unsigned char* pData = (unsigned char*)GetPixels();
		if(pData)
		{
			memset(pColors,'\0',sizeof(unsigned int) * nNumColors);

			switch(nBitsPerPixel)
			{
				case 1:
				case 4:
				case 8:
				{
					unsigned char mask = 0xff;
					unsigned int nBitsByte = 8 / nBitsPerPixel;
					mask >>= (8 - nBitsPerPixel);

					for(int i = 0; i < nHeight ; i++)
					{
						for(int j = 0; j < nColBytesReal; j++)
						{
							unsigned char uByte = *pData++;
							for(int k = 0; k < (int)nBitsByte; k++)
							{
								unsigned char uColor = (unsigned char)(uByte & mask);
								if(uColor < nNumColors)
									pColors[uColor]++;
								uByte >>= nBitsPerPixel;
							}
						}
						if(nBitsEnd)
						{
							unsigned char uByte = *pData++;
							for(int k = 0; k < nBitsEnd; k++)
							{
								unsigned char uColor = (unsigned char)(uByte & mask);
								if(uColor < nNumColors)
									pColors[uColor]++;
								uByte >>= nBitsPerPixel;

							}
						}
						pData += nRestBytesAlig;
					}
					
					bCount = TRUE;
				}
				break;

				case 16:
				case 24:
				case 32:
				default:
					SetLastErrorDescriptionEx("%s(): image must have more than 8 BPP",__func__);
					break;
			}
		}

		UnlockData();
	}
	
	return(bCount);
}

/*
	CountRGBColors()
*/
BOOL CImage::CountRGBColors(COLORREF* pColors,unsigned int* pCountColors,unsigned char nNumColors)
{
	BOOL bCount = FALSE;
	int  nBitsPerPixel  = GetBPP();
	int  iBytesPerPixel = nBitsPerPixel / 8;
	int  nHeight        = GetHeight();
	int  nWidth         = GetWidth();
	int  nColBytesReal  = (nWidth* nBitsPerPixel) / 8; 
	int  nBitsEnd       = (nWidth* nBitsPerPixel) % 8;
	int  nColBytes      = ((((nWidth * nBitsPerPixel) + 31) & ~31) >> 3);
	int  nRestBytesAlig = nColBytes - nColBytesReal - (nBitsEnd ? 1 : 0);
	
	if(LockData())
	{
		unsigned char* pData = (unsigned char*)GetPixels();
		if(pData)
		{
			memset(pCountColors,'\0',sizeof(unsigned int) * nNumColors);
			switch(nBitsPerPixel)
			{
				case 1:
				case 4:
				case 8:
				case 16:
					SetLastErrorDescriptionEx("%s(): image must have more than 16 BPP",__func__);
					break;

				case 24:
				case 32:
					{
						for(int i = 0; i < nHeight ; i++)
						{
							for(int j = 0; j < nWidth; j++)
							{
								for(int k = 0; k < nNumColors; k++)
								{
									if(GetRValue(pColors[k])==pData[RGB_RED] && GetGValue(pColors[k])==pData[RGB_GREEN] && GetBValue(pColors[k])==pData[RGB_BLUE])
									{
										pCountColors[k]++;
										break;
									}
								}
								
								pData += iBytesPerPixel;
							}

							pData += nRestBytesAlig;
						}
						
						bCount = TRUE;
					}
					break;

				default:
					break;
			}
		}

		UnlockData();
	}
	
	return(bCount);
}

/*
	ConvertToBPP()
*/
UINT CImage::ConvertToBPP(UINT nBitsPerPixel,UINT nFlags/*=0*/,RGBQUAD *pPalette/*=NULL*/,UINT nColors/*=0*/)
{
	UINT nRet = GDI_ERROR;	
	BOOL bLocked = FALSE;

	void* pVoid = NULL;
	HDC hDC = NULL;
	BITMAPINFO* pBitmapInfoSrc = NULL;
	BITMAPINFO* pBitmapInfo = NULL;
	HBITMAP hBitmap = NULL;
	HBITMAP hOldBitmap = NULL;
	HPALETTE hPal = NULL;
	HPALETTE hOldPal = NULL;
	UINT nColorData = 0;

	if(LockData())
	{
		bLocked = TRUE;

		if(nBitsPerPixel <= 8)
		{
			nColorData = 1 << nBitsPerPixel;
			if(!pPalette)
				goto done;
		}

		hDC = ::CreateCompatibleDC(NULL);
		if(!hDC)
			goto done;

		pBitmapInfoSrc = GetBMI();
		if(!pBitmapInfoSrc)
			goto done;

		pBitmapInfo = (BITMAPINFO*)new char[sizeof(BITMAPINFOHEADER)+(nColorData*sizeof(RGBQUAD))];
		if(!pBitmapInfo)
			goto done;

		memset(pBitmapInfo,'\0',sizeof(BITMAPINFOHEADER)+(nColorData*sizeof(RGBQUAD)));
		pBitmapInfo->bmiHeader.biBitCount    = (unsigned short)nBitsPerPixel;
		pBitmapInfo->bmiHeader.biHeight      = GetHeight();
		pBitmapInfo->bmiHeader.biWidth       = GetWidth();
		pBitmapInfo->bmiHeader.biPlanes      = 1;
		pBitmapInfo->bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
		pBitmapInfo->bmiHeader.biCompression = BI_RGB;
		
		if(pPalette && nColorData)
			memcpy(pBitmapInfo->bmiColors,pPalette,min(nColorData,nColors));

		hBitmap = ::CreateDIBSection(hDC,(LPBITMAPINFO)pBitmapInfo,DIB_RGB_COLORS,&pVoid,0,0);
		if(!hBitmap)
			goto done;

		hOldBitmap = (HBITMAP)::SelectObject(hDC,hBitmap);
		
		hPal = CreateDIBPalette((BITMAPINFO*)pBitmapInfoSrc);
		if(hPal)
		{
			hOldPal = ::SelectPalette(hDC,hPal,FALSE);
			::RealizePalette(hDC);
		}

		// ex CImageParams.h, aggiustare		
		#define QUANTIZE_FIXEDPALETTE				0x01		// use the fixed palette
		#define QUANTIZE_OPTIMIZEDPALETTE			0x02		// create an optimized palette, or if you supply a palette in the pPalette parameter, supply optimized colors for specified entries in the palette.
		#define QUANTIZE_NETSCAPEPALETTE			0x40		// use the fixed palette that is employed by Netscape Navigator and by Microsoft Internet Explorer
		#define QUANTIZE_USERPALETTE				0x10		// use the palette specified in the pPalette parameter without supplying optimized colors
		#define QUANTIZE_IDENTITYPALETTE			0x08		// insert the Windows system palette
		#define QUANTIZE_FASTMATCHPALETTE			0x20		// use a predefined table to speed conversion using your own palette
		#define QUANTIZE_BYTEORDERBGR				0x04		// use BGR color order. This flag only has meaning when going to 16 bits per pixel or higher.
		#define QUANTIZE_BYTEORDERRGB				0x00		// use RGB color. This flag only has meaning when going to 16 bits per
		#define QUANTIZE_BYTEORDERGRAY				0x80		// grayscale.Destination bitmap should be 12 or 16-bit grayscale. 12 and 16-bit grayscale images are only supported in the Medical Express edition
		#define QUANTIZE_NODITHERING				0x00000000	// Use nearest color matching
		#define QUANTIZE_FLOYDSTEINDITHERING		0x00010000	// Use Floyd-Steinberg dithering
		#define QUANTIZE_STUCKIDITHERING			0x00020000	// Use Stucki dithering
		#define QUANTIZE_BURKESDITHERING			0x00030000	// Use Burkes dithering
		#define QUANTIZE_SIERRADITHERING			0x00040000	// Use Sierra dithering
		#define QUANTIZE_STEVENSONARCEDITHERING		0x00050000	// Use Stevenson Arce dithering
		#define QUANTIZE_JARVISDITHERING			0x00060000	// Use Jarvis dithering
		#define QUANTIZE_ORDEREDDITHERING			0x00070000	// Use ordered dithering, which is faster but less accurate than other dithering methods
		#define QUANTIZE_CLUSTEREDDITHERING			0x00080000	// Use clustered dithering

		int nCurrentMode = HALFTONE;
		if((nFlags & 0xffff0000)==QUANTIZE_NODITHERING)
		{
			switch(GetPhotometric())
			{
				case PHOTOMETRICMINISBLACK:
					nCurrentMode = WHITEONBLACK;
					break;
				case PHOTOMETRICMINISWHITE:
					nCurrentMode = BLACKONWHITE;
					break;
				default:
					nCurrentMode = COLORONCOLOR;
					break;
			}
		}
		
		int nMode = ::SetStretchBltMode(hDC,nCurrentMode);
		nRet = ::StretchDIBits(	hDC,
								0,
								0,
								pBitmapInfo->bmiHeader.biWidth,
								pBitmapInfo->bmiHeader.biHeight,
								0,
								0,
								pBitmapInfoSrc->bmiHeader.biWidth,
								pBitmapInfo->bmiHeader.biHeight,
								(LPSTR)GetPixels(),
								(BITMAPINFO*)pBitmapInfoSrc,
								DIB_RGB_COLORS,
								SRCCOPY
								);
		::SetStretchBltMode(hDC,nMode);
		
		::SelectObject(hDC,hOldBitmap);
		
		bLocked = !UnlockData();

		if(nRet!=GDI_ERROR)
			nRet = NO_ERROR;

		if(nRet == NO_ERROR)
		{
			nRet = GDI_ERROR;
			if(Create(pBitmapInfo, NULL) && LockData())
			{
				// controlla che l'header sia BI_RGB prima di estrarre i pixel
				BITMAPINFO* pTargetBMI = GetBMI();
				pTargetBMI->bmiHeader.biCompression = BI_RGB; 
        
				nRet = ::GetDIBits(hDC, hBitmap, 0, GetHeight(), GetPixels(), pTargetBMI, DIB_RGB_COLORS) ? NO_ERROR : GDI_ERROR;
        
				// dopo GetDIBits Windows potrebbe aver cambiato biCompression
				// se lo ha fatto, lo resettia a BI_RGB
				pTargetBMI->bmiHeader.biCompression = BI_RGB;
        
				bLocked = !UnlockData(TRUE);
			}
		}

		if(hOldPal)
			::SelectPalette(hDC,hOldPal,TRUE);
	}

done:

	if(hDC)
		::DeleteDC(hDC);
	if(pBitmapInfo)
		delete [] pBitmapInfo;
	if(hBitmap)
		::DeleteObject(hBitmap);			
	if(hPal)
		::DeleteObject(hPal);
	
	if(bLocked)
		UnlockData();

	if(nRet==GDI_ERROR)
		SetLastErrorDescriptionEx("%s(): unable to convert to %d BPP",__func__,nBitsPerPixel);

	return(nRet);
}

/*
	GetDIBNumColors()
*/
WORD CImage::GetDIBNumColors(LPSTR lpbi)
{
	WORD wNumColors = 0;
	WORD wBitCount = 0;

	if(IS_WIN30_DIB(lpbi))
	{
		DWORD dwClrUsed = ((LPBITMAPINFOHEADER)lpbi)->biClrUsed;
		if(dwClrUsed!=0)
			return((WORD)dwClrUsed);
	}

	if(IS_WIN30_DIB(lpbi))
		wBitCount = ((LPBITMAPINFOHEADER)lpbi)->biBitCount;
	else
		wBitCount = ((LPBITMAPCOREHEADER)lpbi)->bcBitCount;

	switch(wBitCount)
	{
		case 1:
			wNumColors = 2;
			break;
		case 4:
			wNumColors = 16;
			break;
		case 8:
			wNumColors = 256;
			break;
	}

	return(wNumColors);
}

/*
	CreateDIBPalette()
*/
HPALETTE CImage::CreateDIBPalette(LPBITMAPINFO lpbmi)
{
    if(!lpbmi)
        return(NULL);
    
    // determina tipo di DIB
    BOOL bIsWin30DIB = IS_WIN30_DIB((LPSTR)lpbmi);
    UINT headerSize = lpbmi->bmiHeader.biSize;
    
    // calcola numero di colori (con limiti)
    WORD wNumColors = GetDIBNumColors((LPSTR)lpbmi);
    
    // se non e' DIB a palette (24/32bpp), restituisce NULL
    if(wNumColors==0)
	{
		SetLastErrorDescriptionEx("%s(): no colors",__func__);
        return NULL;
    }
    
    // limita numero di colori a valori ragionevoli
    // BITMAPCOREINFO supporta max 256 colori (8bpp)
    // BITMAPINFO supporta teoricamente più colori ma limitia
    const WORD MAX_PALETTE_COLORS = 256;
    if(wNumColors > MAX_PALETTE_COLORS)
        wNumColors = MAX_PALETTE_COLORS;
    
    // calcola dimensione memoria necessaria (con overflow check)
    QWORD allocSize = (QWORD)sizeof(LOGPALETTE) + (QWORD)sizeof(PALETTEENTRY) * (QWORD)wNumColors;
    
    if(allocSize > 0xFFFFFFFFUL) // > DWORD_MAX
	{
		SetLastErrorDescriptionEx("%s(): memory allocation failed",__func__);
        return NULL;
    }
    
    // alloca memoria per LOGPALETTE
    HANDLE hLogPal = ::GlobalAlloc(GHND,(SIZE_T)allocSize);
    if(!hLogPal)
	{
		SetLastErrorDescriptionEx("%s(): memory allocation failed",__func__);
        return NULL;
    }
    
    LPLOGPALETTE lpPal = (LPLOGPALETTE)::GlobalLock((HGLOBAL)hLogPal);
    if(!lpPal)
	{
		SetLastErrorDescriptionEx("%s(): memory lock failed",__func__);
        ::GlobalFree((HGLOBAL)hLogPal);
        return NULL;
    }
    
    // inizializza palette
    lpPal->palVersion = PALVERSION;
    lpPal->palNumEntries = wNumColors;
    
    HPALETTE hPal = NULL;
    BOOL bSuccess = TRUE;
    
    // copia colori con bound checking
    if(bIsWin30DIB)
	{
        // BITMAPINFO (Windows 3.0+ style)
        // verifica che ci siano abbastanza colori nel BITMAPINFO
        if(lpbmi->bmiHeader.biClrUsed > 0 && (DWORD)wNumColors > lpbmi->bmiHeader.biClrUsed)
		{
            wNumColors = (WORD)lpbmi->bmiHeader.biClrUsed;
            lpPal->palNumEntries = wNumColors;
        }
        
        for(WORD i = 0; i < wNumColors && i < MAX_PALETTE_COLORS; i++)
		{
            if(i * sizeof(RGBQUAD) + sizeof(BITMAPINFOHEADER) <= headerSize)
			{
                lpPal->palPalEntry[i].peRed   = lpbmi->bmiColors[i].rgbRed;
                lpPal->palPalEntry[i].peGreen = lpbmi->bmiColors[i].rgbGreen;
                lpPal->palPalEntry[i].peBlue  = lpbmi->bmiColors[i].rgbBlue;
                lpPal->palPalEntry[i].peFlags = 0;
            }
			else
			{
                // colore fuori bounds, usa nero
                lpPal->palPalEntry[i].peRed = 0;
                lpPal->palPalEntry[i].peGreen = 0;
                lpPal->palPalEntry[i].peBlue = 0;
                lpPal->palPalEntry[i].peFlags = 0;
            }
        }
    }
	else
	{
        // BITMAPCOREINFO (OS/2 style) - RGBTRIPLE
        LPBITMAPCOREINFO lpbmc = (LPBITMAPCOREINFO)lpbmi;
        
        // verifica dimensione header
        if(headerSize < sizeof(BITMAPCOREHEADER))
		{
            bSuccess = FALSE;
        }
		else
		{
            for(WORD i = 0; i < wNumColors && i < MAX_PALETTE_COLORS; i++)
			{
                if(i * sizeof(RGBTRIPLE) + sizeof(BITMAPCOREHEADER) <= headerSize)
				{
                    lpPal->palPalEntry[i].peRed   = lpbmc->bmciColors[i].rgbtRed;
                    lpPal->palPalEntry[i].peGreen = lpbmc->bmciColors[i].rgbtGreen;
                    lpPal->palPalEntry[i].peBlue  = lpbmc->bmciColors[i].rgbtBlue;
                    lpPal->palPalEntry[i].peFlags = 0;
                }
				else
				{
                    lpPal->palPalEntry[i].peRed = 0;
                    lpPal->palPalEntry[i].peGreen = 0;
                    lpPal->palPalEntry[i].peBlue = 0;
                    lpPal->palPalEntry[i].peFlags = 0;
                }
            }
        }
    }
    
    // crea palette Windows
    if(bSuccess)
        hPal = ::CreatePalette(lpPal);
	else
		SetLastErrorDescriptionEx("%s(): unable to create the colors palette",__func__);
    
    ::GlobalUnlock((HGLOBAL)hLogPal);
    ::GlobalFree((HGLOBAL)hLogPal);
    
    return hPal;
}

/*
	GetDIB()
*/
HDIB CImage::GetDIB(UINT* pSize/* = NULL*/)
{
    HDIB hDib = NULL;
    int bpp = (int)GetBPP();
    int width = GetWidth();
    int height = GetHeight();
    int stride = (int)GetBytesWidth();
    void* pPixels = GetPixels();
    int numColors = 0;
    int paletteSize = 0;
    QWORD largeBitmapSize = 0LL;
    QWORD largeTotalSize = 0LL;
    DWORD totalSize = 0L;
	DWORD bitmapSize = 0L;
    BITMAPINFOHEADER* pDstBmi = NULL;
    LPBITMAPINFO pSrcBmi = NULL;
    BYTE* pDstPixels = (BYTE*)NULL;
	int dibOrder = 1;

    if(!pPixels || width <= 0 || height <= 0 || bpp <= 0)
        goto done;
    
    // calcola dimensioni con controllo overflow
    if(bpp <= 8)
	{
        numColors = GetNumColors();
        if(numColors==0)
            numColors = 1 << bpp;
        numColors = min(numColors,256); // limita a 256 colori max
    }
    
    paletteSize = (bpp <= 8) ? (sizeof(RGBQUAD) * numColors) : 0;
    
    // controllo overflow per bitmapSize
    largeBitmapSize = (QWORD)stride * (QWORD)height;
    if(largeBitmapSize > 0xFFFFFFFFUL)
	{
		SetLastErrorDescriptionEx("%s(): bitmap size overflow",__func__);
        goto done;
	}

    bitmapSize = (DWORD)largeBitmapSize;
    
    // controllo overflow per totalSize
    largeTotalSize = (QWORD)sizeof(BITMAPINFOHEADER) + (QWORD)paletteSize + largeBitmapSize;
    if(largeTotalSize > 0xFFFFFFFFUL)
	{
		SetLastErrorDescriptionEx("%s(): total size overflow",__func__);
        goto done;
	}

    totalSize = (DWORD)largeTotalSize;
	if(pSize)
		*pSize = (UINT)totalSize;
    
    // alloca l'HDIB
    hDib = (HDIB)::GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, totalSize);
    if(!hDib)
	{
		SetLastErrorDescriptionEx("%s(): memory allocation failed",__func__);
        goto done;
	}
    
    // copia i dati
    pDstBmi = (BITMAPINFOHEADER*)::GlobalLock((HGLOBAL)hDib);
    if(!pDstBmi)
	{
        ::GlobalFree((HGLOBAL)hDib);
		hDib = NULL;
		SetLastErrorDescriptionEx("%s(): memory lock failed",__func__);
        goto done;
    }
    
    // copia o crea BITMAPINFO
    pSrcBmi = GetBMI();
    if(pSrcBmi && pSrcBmi->bmiHeader.biSize >= sizeof(BITMAPINFOHEADER))
    {
        // calcola dimensione corretta dell'header sorgente
        int srcHeaderSize = pSrcBmi->bmiHeader.biSize;
        int copySize = min(srcHeaderSize, (int)sizeof(BITMAPINFOHEADER)) + paletteSize;
        
        memcpy(pDstBmi,pSrcBmi,copySize);
        
        // assicura dimensioni corrette
        pDstBmi->biWidth = width;
        pDstBmi->biHeight = height;
        pDstBmi->biSizeImage = bitmapSize;
        
        // assicura biClrUsed corretto
        if(bpp <= 8 && pDstBmi->biClrUsed==0)
            pDstBmi->biClrUsed = numColors;
    }
    else
    {
        // crea il BITMAPINFOHEADER
        memset(pDstBmi,'\0',sizeof(BITMAPINFOHEADER));
        pDstBmi->biSize = sizeof(BITMAPINFOHEADER);
        pDstBmi->biWidth = width;
        pDstBmi->biHeight = height;
        pDstBmi->biPlanes = 1;
        pDstBmi->biBitCount = (WORD)bpp;
        pDstBmi->biCompression = BI_RGB;
        pDstBmi->biSizeImage = bitmapSize;
        
        if(bpp <= 8)
		{
            pDstBmi->biClrUsed = numColors;
            
            // palette grayscale di default
            RGBQUAD* pPalette = (RGBQUAD*)(pDstBmi + 1);
            for(int i = 0; i < numColors; i++)
			{
                BYTE gray = (BYTE)((i * 255) / ((numColors > 1) ? (numColors - 1) : 1));
                pPalette[i].rgbRed = gray;
                pPalette[i].rgbGreen = gray;
                pPalette[i].rgbBlue = gray;
                pPalette[i].rgbReserved = 0;
            }
        }
    }
    
    // forza il formato standard HDIB: bottom-up (biHeight positivo)
    pDstBmi->biHeight = abs(pDstBmi->biHeight);
    
    // copia pixel con correzione orientamento
    pDstPixels = (BYTE*)(pDstBmi + 1) + paletteSize;
    dibOrder = GetDIBOrder();
    
    if(dibOrder < 0)
	{
		// source top-down (es. NexgenIPL) -> inverte righe per bottom-up HDIB
        for(int y = 0; y < height; y++)
		{
            BYTE* srcLine = (BYTE*)pPixels + y * stride;
            BYTE* dstLine = pDstPixels + (height - 1 - y) * stride;
            memcpy(dstLine, srcLine, stride);
        }
    }
    else
	{
        // source bottom-up (es. PaintLib, FreeImage) -> copia diretta
        memcpy(pDstPixels,pPixels,bitmapSize);
    }
    
    ::GlobalUnlock((HGLOBAL)hDib);

done:

	if(!hDib)
		SetLastErrorDescriptionEx("%s(): unable to retrieve the DIB section",__func__);
    
	return(hDib);
}

/*
	SetDIB()
*/
BOOL CImage::SetDIB(HDIB hDib,int nOrientation/* = 1*/)
{
	BOOL bResult = FALSE;
	BITMAPINFOHEADER* pBmiHeader = NULL;
    int width = 0;
    int height = 0;
    int bpp = 0;
	BOOL isHdibTopDown = FALSE;
    int paletteColors = 0;
    int paletteSize = 0;
    int headerSize = 0;
    BYTE* pPixelData = (BYTE*)NULL;
    int stride = 0;
    QWORD expectedImageSize = 0LL;
    DWORD dibSize = 0L;
    QWORD minExpectedSize = 0LL;

    if(!hDib)
	{
		SetLastErrorDescriptionEx("%s(): invalid DIB data",__func__);
        goto done;
    }

    // lock HDIB
    pBmiHeader = (BITMAPINFOHEADER*)GlobalLock((HGLOBAL)hDib);
    if(!pBmiHeader)
	{
		SetLastErrorDescriptionEx("%s(): DIB lock failed",__func__);
        goto done;
    }
    
    // validazione BITMAPINFOHEADER
    if(	pBmiHeader->biSize < sizeof(BITMAPINFOHEADER)	||
		pBmiHeader->biWidth <= 0						|| 
		pBmiHeader->biWidth > 0x7FFF					||
		pBmiHeader->biBitCount <= 0						||
		pBmiHeader->biBitCount > 32)
	{
		GlobalUnlock((HGLOBAL)hDib);
        goto done;
	}
    
    width = pBmiHeader->biWidth;
    height = abs(pBmiHeader->biHeight);
    bpp = pBmiHeader->biBitCount;
    
    if(height <= 0 || height > 0x7FFF)
	{
        GlobalUnlock((HGLOBAL)hDib);
        goto done;
    }
    
    // orientamento HDIB
    isHdibTopDown = (pBmiHeader->biHeight < 0);
    
    // dimensioni palette
    paletteColors = 0;
    if (bpp <= 8)
	{
        if(pBmiHeader->biClrUsed > 0)
            paletteColors = (int)pBmiHeader->biClrUsed;
        else
            paletteColors = 1 << bpp;
        paletteColors = min(paletteColors,256);  // limita a 256 colori max
    }
    paletteSize = paletteColors * sizeof(RGBQUAD);
    
    // calcola offset dati pixel
    // headerSize potrebbe essere BITMAPV4/5HEADER
    headerSize = max((int)pBmiHeader->biSize,(int)sizeof(BITMAPINFOHEADER));
    pPixelData = (BYTE*)pBmiHeader + headerSize + paletteSize;
    
    // verifica la dimensione dei dati
    stride = WIDTHBYTES((width * bpp),GetAlignment());
    expectedImageSize = (QWORD)stride * (QWORD)height;
    
    // dimensione totale del DIB
    dibSize = (DWORD)GlobalSize((HGLOBAL)hDib);
    minExpectedSize = (QWORD)headerSize + (QWORD)paletteSize + expectedImageSize;
    if(dibSize < (DWORD)minExpectedSize)
	{
        // DIB troppo piccolo per i dati dichiarati
        GlobalUnlock((HGLOBAL)hDib);
		SetLastErrorDescriptionEx("%s(): DIB size mismatch",__func__);
        goto done;
    }
    
    // crea il BITMAPINFO da passare a Create() allocando il buffer necessario
	{
    int bmiSize = headerSize + paletteSize;
    std::vector<BYTE> bmiBuffer(bmiSize);
    memcpy(bmiBuffer.data(),pBmiHeader,bmiSize);

    // Correggi alcuni campi per sicurezza
    BITMAPINFO* pBitmapInfo = (BITMAPINFO*)bmiBuffer.data();
    pBitmapInfo->bmiHeader.biWidth = width;
    pBitmapInfo->bmiHeader.biHeight = height;
    if(bpp <= 8 && pBitmapInfo->bmiHeader.biClrUsed==0)
        pBitmapInfo->bmiHeader.biClrUsed = paletteColors;
    
    // crea l'immagine
    bResult = Create(pBitmapInfo,pPixelData);
    }
    
    // orientamento
    if(bResult)
	{
        int libDibOrder = GetDIBOrder();
        BOOL isLibTopDown = (libDibOrder < 0);
        
        // OCCHIO: se HDIB e libreria hanno orientamento opposto
        if(isHdibTopDown!=isLibTopDown)
            MirrorVertical();
    }
    
    GlobalUnlock((HGLOBAL)hDib);

done:

	if(!bResult)
		SetLastErrorDescriptionEx("%s(): unable to set the DIB section",__func__);

    return(bResult);
}

/*
	GetHeaderInfo()

	Ricava le informazioni basiche dell'immagine, riempiendo la struttura IMAGE_HEADERINFO.

	Dopo varie vicissitudini, si e' resa statica per poter essere usata stand-alone (ossia senza 
	dover istanziare un oggetto).

	La logica originale viene ora implementata nelle derivate tramite la UpdateHeaderInfo(), che
	solo aggiorna alcuni metadati, dato che ogni derivata imposta i dati relativi all'immagine 
	nell'header durante il caricamento del file.
*/
/* STATIC LOCAL */DWORD CImage::GetHeaderInfo(LPCSTR lpcszFileName,LPIMAGEHEADERINFO pHeaderInfo)
{
	DWORD dwRet = NO_ERROR;

    // inizializza la struttura
	memset(pHeaderInfo,'\0',sizeof(IMAGEHEADERINFO));
	pHeaderInfo->type = NULL_PICTURE;
	pHeaderInfo->xres = -1;
	pHeaderInfo->yres = -1;
	pHeaderInfo->restype = -1;
	pHeaderInfo->compression = -1;
	pHeaderInfo->quality = -1;
	pHeaderInfo->filesize = (unsigned long)-1L;
	pHeaderInfo->width = (unsigned long)-1L;
	pHeaderInfo->height = (unsigned long)-1L;
    
    HANDLE hHandle = INVALID_HANDLE_VALUE;
    HANDLE hMap = INVALID_HANDLE_VALUE;
    LPVOID pMap = NULL;
	DWORD dwSize = 0L;
    DWORD dwRead = 0L;
    BYTE* pData = NULL;

	// GetLastError() -> 6=invalid handle, 1006=invalid file (no volume/area to read) -> file 0 size (EOF)

	// apre il file
    if((hHandle = ::CreateFile(lpcszFileName,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL))==INVALID_HANDLE_VALUE)
	{
		dwRet = GetLastError();
		goto done;
	}

	// dimensione del file
	dwSize = pHeaderInfo->filesize = ::GetFileSize(hHandle,NULL);
    dwRead = dwSize;

	// crea la mappatura per il file in modo da poter leggere i dati come se si trattasse di un buffer in memoria
	if((hMap = ::CreateFileMapping(hHandle,NULL,PAGE_READONLY,0,0,NULL))==NULL)
	{
		dwRet = GetLastError();
		goto done;
	}

    if((pMap = ::MapViewOfFile(hMap,FILE_MAP_READ,0,0,0))==NULL)
	{
		dwRet = GetLastError();
		goto done;
	}

    pData = (BYTE*)pMap;

    // riconoscimento formato

    // BMP
    if(pHeaderInfo->type==NULL_PICTURE)
    {
        if(dwSize >= sizeof(BITMAPFILEHEADER))
        {
            BITMAPFILEHEADER* pBi = (BITMAPFILEHEADER*)pData;
            if(pBi->bfType == 0x4d42)
                pHeaderInfo->type = BMP_PICTURE;
        }
    }

    // JPEG/JFIF
    if(pHeaderInfo->type==NULL_PICTURE)
    {
        if(dwSize >= 3 && pData[0] == 0xFF && pData[1] == 0xD8 && pData[2] == 0xFF)
            pHeaderInfo->type = JPEG_PICTURE;
    }

    // GIF
    if(pHeaderInfo->type==NULL_PICTURE)
    {
        if(dwSize >= 4)
        {
            ULONG GIFSig = *((ULONG*)pData);
            if(GIFSig == 0x38464947)
                pHeaderInfo->type = GIF_PICTURE;
        }
    }

    // TIFF
    if(pHeaderInfo->type==NULL_PICTURE)
    {
        if(dwSize >= 4)
        {
            ULONG TIFFSig = *((ULONG *)pData);
            if(TIFFSig == 0x002A4949 || TIFFSig == 0x2A004D4D)
                pHeaderInfo->type = TIFF_PICTURE;
        }
    }

    // PNG
    if(pHeaderInfo->type==NULL_PICTURE)
    {
        if(dwSize >= 4 && pData[0] == 0x89 && pData[1] == 0x50 && 
            pData[2] == 0x4E && pData[3] == 0x47)
            pHeaderInfo->type = PNG_PICTURE;
    }

	// WebP (libwebp)
	if(pHeaderInfo->type==NULL_PICTURE)
	{
		if(dwSize >= 12)
		{
			// RIFFXXXXWEBP
			if(	pData[0]=='R' && pData[1]=='I' && pData[2]=='F'  && pData[3]=='F' && 
				pData[8]=='W' && pData[9]=='E' && pData[10]=='B' && pData[11]=='P')
				pHeaderInfo->type = WEBP_PICTURE;
		}
	}

	// HEIC/HEIF/AVIF (libheif)
    if(pHeaderInfo->type==NULL_PICTURE && dwSize >= 12)
    {
        if(pData[4]=='f' && pData[5]=='t' && pData[6]=='y' && pData[7]=='p')
        {
            // HEIC/HEIF signatures
            if((pData[8] == 'h' && pData[9] == 'e' && pData[10] == 'i' && pData[11] == 'c') ||
               (pData[8] == 'h' && pData[9] == 'e' && pData[10] == 'i' && pData[11] == 'x') ||
               (pData[8] == 'm' && pData[9] == 'i' && pData[10] == 'f' && pData[11] == '1') ||
               (pData[8] == 'm' && pData[9] == 's' && pData[10] == 'f' && pData[11] == '1') ||
               (pData[8] == 'h' && pData[9] == 'e' && pData[10] == 'v' && pData[11] == 'c') ||
               (pData[8] == 'h' && pData[9] == 'e' && pData[10] == 'v' && pData[11] == 'x'))
            {
                pHeaderInfo->type = HEIC_PICTURE;
            }
            // AVIF signatures
            else if((pData[8] == 'a' && pData[9] == 'v' && pData[10] == 'i' && pData[11] == 'f') ||
                    (pData[8] == 'a' && pData[9] == 'v' && pData[10] == 'i' && pData[11] == 's'))
            {
                pHeaderInfo->type = AVIF_PICTURE;
            }
        }
    }

    // PGM
    if(pHeaderInfo->type==NULL_PICTURE)
    {
        if(dwSize >= 2 && pData[0] == 0x50 && ((pData[1] == 0x32) || (pData[1] == 0x35)))
            pHeaderInfo->type = PGM_PICTURE;
    }

    // PCX
    if(pHeaderInfo->type==NULL_PICTURE)
    {
        if(dwSize >= 3 && pData[0] == 0x0A && pData[2] == 0x01)
            pHeaderInfo->type = PCX_PICTURE;
    }

    // PICT
    if(pHeaderInfo->type==NULL_PICTURE)
    {
        if(dwRead > 540)
        {
            BYTE* pPictSig = (BYTE*)(pData+0x20a);
            if((pPictSig[0]==0x00 && pPictSig[1]==0x11 && pPictSig[2]==0x02 && pPictSig[3]==0xFF) || 
                (pPictSig[0]==0x00 && pPictSig[1]==0x11 && pPictSig[2]==0x01) || 
                (pPictSig[0]==0x11 && pPictSig[1]==0x01 && pPictSig[2]==0x01 && pPictSig[3]==0x00))
                pHeaderInfo->type = PICT_PICTURE;
        }
    }

    // TGA (senza signature)
    if(pHeaderInfo->type==NULL_PICTURE && dwSize >= 18)
    {
        BOOL bCouldBeTGA = TRUE;
        if(*(pData+1) > 1)
            bCouldBeTGA = FALSE;
        BYTE TGAImgType = *(pData+2);
        if((TGAImgType > 11) || (TGAImgType > 3 && TGAImgType < 9))
            bCouldBeTGA = FALSE;
        BYTE TGAColMapDepth = *(pData+7);
        if(TGAColMapDepth != 8 && TGAColMapDepth != 15 &&
            TGAColMapDepth != 16 && TGAColMapDepth != 24 && 
            TGAColMapDepth != 32 && TGAColMapDepth != 0)
            bCouldBeTGA = FALSE;
        BYTE TGAPixDepth = *(pData+16);
        if(TGAPixDepth != 8 && TGAPixDepth != 15 && 
            TGAPixDepth != 16 && TGAPixDepth != 24 && TGAPixDepth != 32)
            bCouldBeTGA = FALSE;
        if(bCouldBeTGA)
            pHeaderInfo->type = TGA_PICTURE;
    }
        
    // estrazione dati per ogni formato
                
    if(pHeaderInfo->type==JPEG_PICTURE)
    {
        jpeg_decompress_struct cinfo;
        jpeg_error_mgr jerr;
        cinfo.err = jpeg_std_error (&jerr);
        jerr.error_exit = jpeg_error_exit;
                    
        jpeg_create_decompress(&cinfo);
        jpeg_mem_src(&cinfo, pData, dwSize, (void*)pData);
        jpeg_read_header(&cinfo, 0);
                
        // Dimensioni
        pHeaderInfo->width = cinfo.image_width;
        pHeaderInfo->height = cinfo.image_height;
                    
        // Calcola BPP totale in base al colore space
        switch(cinfo.jpeg_color_space) {
            case JCS_GRAYSCALE:
                pHeaderInfo->bpp = cinfo.data_precision; // 8 o 12
                pHeaderInfo->colors = 0;
                break;
            case JCS_RGB:
            case JCS_YCbCr:
                pHeaderInfo->bpp = cinfo.data_precision * 3; // 24 o 36
                pHeaderInfo->colors = 0;
                break;
            case JCS_CMYK:
            case JCS_YCCK:
                pHeaderInfo->bpp = cinfo.data_precision * 4; // 32 o 48
                pHeaderInfo->colors = 0;
                break;
            default:
                pHeaderInfo->bpp = cinfo.data_precision * 3; // Default
                pHeaderInfo->colors = 0;
                break;
        }

		// nella sezione JPEG
		switch(cinfo.jpeg_color_space) {
			case JCS_GRAYSCALE: pHeaderInfo->colorSpace = COLOR_GRAYSCALE;	break;
			case JCS_RGB:       pHeaderInfo->colorSpace = COLOR_RGB;		break;
			case JCS_YCbCr:     pHeaderInfo->colorSpace = COLOR_YCBCR;		break;
			case JCS_CMYK:      pHeaderInfo->colorSpace = COLOR_CMYK;		break;
			case JCS_YCCK:      pHeaderInfo->colorSpace = COLOR_YCBCR;		break; // o creare tipo separato
			default:            pHeaderInfo->colorSpace = COLOR_RGB;		break;
		}
                    
        // qualita'
        int nQuality = 0;
        if(cinfo.quant_tbl_ptrs != NULL)
        {
            nQuality = cinfo.quant_tbl_ptrs[0]->quantval[61];
                    
            int nMin, nMax;

            //GetQualityRange(nMin, nMax);
			nMin = nMax = -1;

            if(nMax == 255)
                nQuality = 100;
            else
            {
                if(nQuality == 255)
                    nQuality = 1;
                else 
                {
                    if(nQuality > 100) 
                        nQuality = 5000 / nQuality;
                    else
                        nQuality = (200 - nQuality) / 2;
                }
            }
            pHeaderInfo->quality = nQuality;
        }

        // risoluzione
        int nDPI = cinfo.X_density;
        if(nDPI <= 1)
            nDPI = 72;
        pHeaderInfo->xres = (float)nDPI;
                    
        nDPI = cinfo.Y_density;
        if(nDPI <= 1)
            nDPI = 72;
        pHeaderInfo->yres = (float)nDPI;
                    
        switch(cinfo.density_unit)
        {
            case 0:  pHeaderInfo->restype = RESUNITNONE; break;
            case 1:  pHeaderInfo->restype = RESUNITINCH; break;
            case 2:  pHeaderInfo->restype = RESUNITCENTIMETER; break;
        }
                    
        jpeg_destroy_decompress(&cinfo);
    }
    else if(pHeaderInfo->type==TIFF_PICTURE)
    {
        TIFFSetWarningHandler(NULL);
        TIFF* tif = TIFFOpenMem(pData, dwSize, NULL);
        if(tif)
        {
            uint32 width = 0, height = 0;
            uint16 bitsPerSample = 0, samplesPerPixel = 0, photometric = 0;
                        
            TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &width);
            TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &height);
            pHeaderInfo->width = width;
            pHeaderInfo->height = height;
                        
            if(TIFFGetField(tif, TIFFTAG_BITSPERSAMPLE, &bitsPerSample))
            {
                if(TIFFGetField(tif, TIFFTAG_SAMPLESPERPIXEL, &samplesPerPixel))
                {
                    pHeaderInfo->bpp = bitsPerSample * samplesPerPixel;
                }
                else
                {
                    pHeaderInfo->bpp = bitsPerSample * 3; // assume 3 canali
                }
            }
                        
            // controlla se e' palette
            if(TIFFGetField(tif,TIFFTAG_PHOTOMETRIC,&photometric))
            {
				switch(photometric)
				{
					case PHOTOMETRIC_MINISWHITE:
					case PHOTOMETRIC_MINISBLACK:
						pHeaderInfo->colorSpace = COLOR_GRAYSCALE;
						break;
					case PHOTOMETRIC_RGB:
						pHeaderInfo->colorSpace = COLOR_RGB;
						break;
					case PHOTOMETRIC_PALETTE:
						pHeaderInfo->colorSpace = COLOR_PALETTE;
						break;
					// non definito nella versione della libtiff usata attualemente:
					//case PHOTOMETRIC_CMYK:
					//	pHeaderInfo->colorSpace = COLOR_CMYK;
					//	break;
					case PHOTOMETRIC_CIELAB:
						pHeaderInfo->colorSpace = COLOR_LAB;
						break;
					case PHOTOMETRIC_LOGL:
					case PHOTOMETRIC_LOGLUV:
					default:
						pHeaderInfo->colorSpace = COLOR_GRAYSCALE;
						break;
				}

                if(photometric==PHOTOMETRIC_PALETTE)
                {
                    uint16* colorMap[3];
                    if(TIFFGetField(tif, TIFFTAG_COLORMAP, &colorMap[0], &colorMap[1], &colorMap[2]))
                    {
                        // la palette TIFF ha 256 colori (8-bit)
                        pHeaderInfo->colors = 256;
                    }
                }
                else
                {
                    pHeaderInfo->colors = 0;
                }
            }

            float nRes;
            nRes = 0.0;
            TIFFGetField(tif, TIFFTAG_XRESOLUTION, &nRes);
            if(nRes <= 1.0)
                nRes = 300.0;
            pHeaderInfo->xres = nRes;

            nRes = 0.0;
            TIFFGetField(tif, TIFFTAG_YRESOLUTION, &nRes);
            if(nRes <= 1.0)
                nRes = 300.0;
            pHeaderInfo->yres = nRes;

            TIFFGetFieldDefaulted(tif,TIFFTAG_RESOLUTIONUNIT,&pHeaderInfo->restype);
            switch(pHeaderInfo->restype)
            {
                case RESUNIT_NONE:       pHeaderInfo->restype = RESUNITNONE;		break;
                case RESUNIT_INCH:       pHeaderInfo->restype = RESUNITINCH;		break;
                case RESUNIT_CENTIMETER: pHeaderInfo->restype = RESUNITCENTIMETER;	break;
                default:                 pHeaderInfo->restype = RESUNITNONE;		break;
            }

            unsigned short int nComp;
            if(TIFFGetField(tif,TIFFTAG_COMPRESSION,&nComp))
                pHeaderInfo->compression = nComp;

            TIFFClose(tif);
        }
    }
    else if(pHeaderInfo->type==BMP_PICTURE && dwSize >= sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER))
    {
        BITMAPINFOHEADER* bih = (BITMAPINFOHEADER*)(pData + sizeof(BITMAPFILEHEADER));
                    
        // dimensioni
        pHeaderInfo->width = bih->biWidth;
        pHeaderInfo->height = abs(bih->biHeight);
                    
        // BPP
        pHeaderInfo->bpp = bih->biBitCount;
                    
        // colori (per immagini palette)
        if(bih->biBitCount <= 8)
        {
            if(bih->biClrUsed > 0)
                pHeaderInfo->colors = bih->biClrUsed;
            else
                pHeaderInfo->colors = 1 << bih->biBitCount;
        }
        else
        {
            pHeaderInfo->colors = 0;
        }
                    
        // risoluzione
        float nDPI;
        nDPI = 0;
        if(bih->biXPelsPerMeter > 0)
            nDPI = ((float)bih->biXPelsPerMeter / 39.37f);
        if(nDPI <= 1)
            nDPI = 150;
        pHeaderInfo->xres = nDPI;

        nDPI = 0;
        if(bih->biYPelsPerMeter > 0)
            nDPI = ((float)bih->biYPelsPerMeter / 39.37f);
        if(nDPI <= 1)
            nDPI = 150;
        pHeaderInfo->yres = nDPI;
        pHeaderInfo->restype = RESUNITINCH;

		// determina colorSpace per BMP
		if(bih->biBitCount <= 8)
			pHeaderInfo->colorSpace = COLOR_PALETTE;
		else if(bih->biBitCount == 24)
			pHeaderInfo->colorSpace = COLOR_RGB;
		else if(bih->biBitCount == 32)
		{
			// controlla se ha alpha channel (molti BMP 32-bit non hanno alpha)
			if(bih->biCompression == BI_BITFIELDS && dwSize >= sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + 12)
			{
				// controlla maschere colore per vedere se c'e' alpha
				DWORD* bitFields = (DWORD*)(pData + sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER));
				if(bitFields[3] != 0) // maschera alpha presente
					pHeaderInfo->colorSpace = COLOR_RGBA;
				else
					pHeaderInfo->colorSpace = COLOR_RGB;
			}
			else
			{
				pHeaderInfo->colorSpace = COLOR_RGB; // assume RGB, non RGBA
			}
		}
		else
			pHeaderInfo->colorSpace = COLOR_RGB; // default
    }
    else if(pHeaderInfo->type==PNG_PICTURE)
    {
        // PNG: i primi 8 byte sono signature, poi viene l'IHDR chunk
		if (dwSize >= 33) // 8 signature + 4 length + 4 type (es. IHDR) + <n> data (per IHDR sono sempre 13) + 4 CRC
		{
		    BYTE* p = pData + 8;

		    // legge length (big-endian)
		    DWORD length = (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];

			// check che sia IHDR e length corretto
		    if (length == 13 && p[4] == 'I' && p[5] == 'H' && p[6] == 'D' && p[7] == 'R')
			{
				BYTE* ihdrData = p + 8;

				pHeaderInfo->width =
					(ihdrData[0] << 24) |
					(ihdrData[1] << 16) |
					(ihdrData[2] << 8)  |
					ihdrData[3];

				pHeaderInfo->height =
					(ihdrData[4] << 24) |
					(ihdrData[5] << 16) |
					(ihdrData[6] << 8)  |
					ihdrData[7];

				int bitDepth  = ihdrData[8];
				int colorType = ihdrData[9];
                            
				// calcola BPP totale
                switch(colorType)
				{
                    case 0: // grayscale
                        pHeaderInfo->bpp = bitDepth;
                        pHeaderInfo->colors = 0;
                        break;
                    case 2: // RGB
                        pHeaderInfo->bpp = bitDepth * 3;
                        pHeaderInfo->colors = 0;
                        break;
                    case 3: // palette
                        pHeaderInfo->bpp = bitDepth; // bits per indice palette
                        pHeaderInfo->colors = 1 << bitDepth; // 2^bitDepth colori
                        break;
                    case 4: // grayscale+Alpha
                        pHeaderInfo->bpp = bitDepth * 2;
                        pHeaderInfo->colors = 0;
                        break;
                    case 6: // RGB+Alpha
                        pHeaderInfo->bpp = bitDepth * 4;
                        pHeaderInfo->colors = 0;
                        break;
                    default:
                        pHeaderInfo->bpp = bitDepth * 3;
                        pHeaderInfo->colors = 0;
                        break;
                }

				// per colorSpace
				switch(colorType)
				{
					case 0:  pHeaderInfo->colorSpace = COLOR_GRAYSCALE;	break;
					case 2:  pHeaderInfo->colorSpace = COLOR_RGB;		break;
					case 3:  pHeaderInfo->colorSpace = COLOR_PALETTE;	break;
					case 4:  pHeaderInfo->colorSpace = COLOR_GRAYSCALE;	break;
					case 6:  pHeaderInfo->colorSpace = COLOR_RGBA;		break;
					default: pHeaderInfo->colorSpace = COLOR_RGB;		break;
				}
            }
        }
    }
    else if(pHeaderInfo->type==GIF_PICTURE && dwSize >= 13)
    {
        // GIF: byte 6-7 = width, byte 8-9 = height (little-endian)
        pHeaderInfo->width = (pData[7] << 8) | pData[6];
        pHeaderInfo->height = (pData[9] << 8) | pData[8];
                    
        // Global Color Table size
        int colorTableSize = 1 << (((pData[10] >> 4) & 0x07) + 1);
        pHeaderInfo->bpp = 8; // GIF e' sempre 8-bit per indice palette
        pHeaderInfo->colors = colorTableSize;
					
		pHeaderInfo->colorSpace = COLOR_PALETTE; // GIF e' sempre palette-based
    }
    else if(pHeaderInfo->type==TGA_PICTURE && dwSize >= 18)
    {
        // TGA: byte 12-13 = width, byte 14-15 = height (little-endian)
        pHeaderInfo->width = (pData[13] << 8) | pData[12];
        pHeaderInfo->height = (pData[15] << 8) | pData[14];
                    
        int pixelDepth = pData[16]; // Bits per pixel
        pHeaderInfo->bpp = pixelDepth;
                    
        // controlla se e' palette-based
        BYTE imageType = pData[2];
        if(imageType==1 || imageType==9) // palette-based
        {
            int colorMapLength = (pData[5] << 8) | pData[4];
            pHeaderInfo->colors = colorMapLength;
        }
        else
        {
            pHeaderInfo->colors = 0;
        }

		// per colorSpace
		//BYTE pixelDepth = pData[16];
    
		// determina colorSpace in base al tipo di immagine e profondita'
		if(imageType==1 || imageType==9) // palette-based
			pHeaderInfo->colorSpace = COLOR_PALETTE;
		else if(imageType==2 || imageType==10) // RGB
		{
			if(pixelDepth==32)
				pHeaderInfo->colorSpace = COLOR_RGBA;
			else
				pHeaderInfo->colorSpace = COLOR_RGB;
		}
		else if(imageType==3 || imageType==11) // grayscale
			pHeaderInfo->colorSpace = COLOR_GRAYSCALE;
		else
			pHeaderInfo->colorSpace = COLOR_RGB; // default
    }
    else if(pHeaderInfo->type==PCX_PICTURE && dwSize >= 16)
    {
        // PCX: byte 8-9 = xmax, byte 10-11 = ymax (little-endian)
        unsigned short xmin = (pData[5] << 8) | pData[4];
        unsigned short xmax = (pData[9] << 8) | pData[8];
        unsigned short ymin = (pData[7] << 8) | pData[6];
        unsigned short ymax = (pData[11] << 8) | pData[10];
                    
        pHeaderInfo->width = xmax - xmin + 1;
        pHeaderInfo->height = ymax - ymin + 1;
                    
        int bitsPerPixel = pData[3];
        pHeaderInfo->bpp = bitsPerPixel;
                    
        // PCX palette e' alla fine del file
        pHeaderInfo->colors = (bitsPerPixel <= 8) ? 256 : 0;

		// colorSpace
		if(bitsPerPixel==8)
			pHeaderInfo->colorSpace = COLOR_PALETTE; // PCX 8-bit usa palette
		else if(bitsPerPixel==24)
			pHeaderInfo->colorSpace = COLOR_RGB;
		else
			pHeaderInfo->colorSpace = COLOR_GRAYSCALE; // 1, 4 bit sono grayscale
    }
    else if(pHeaderInfo->type==PGM_PICTURE)
    {
        // PGM: formato testo ASCII
        char* str = (char*)pData;
        int offset = 0;
                    
        if(str[0]=='P' && (str[1]=='5' || str[1]=='2'))
        {
            offset = 2;
                        
            // salta whitespace
            while(offset < (int)dwSize && isspace(str[offset]))
				offset++;
                        
            // legge width
            int width = 0;
            while(offset < (int)dwSize && isdigit(str[offset]))
            {
                width = width * 10 + (str[offset] - '0');
                offset++;
            }
            pHeaderInfo->width = width;
                        
            // salta whitespace
            while(offset < (int)dwSize && isspace(str[offset]))
				offset++;
                        
            // legge height
            int height = 0;
            while(offset < (int)dwSize && isdigit(str[offset]))
            {
                height = height * 10 + (str[offset] - '0');
                offset++;
            }
            pHeaderInfo->height = height;
                        
            // salta whitespace
            while(offset < (int)dwSize && isspace(str[offset]))
				offset++;
                        
            // legge max value
            int maxVal = 0;
            while(offset < (int)dwSize && isdigit(str[offset]))
            {
                maxVal = maxVal * 10 + (str[offset] - '0');
                offset++;
            }
                        
            // PGM e' sempre grayscale
            pHeaderInfo->bpp = (maxVal <= 255) ? 8 : 16;
            pHeaderInfo->colors = 0;
		}
					
		// per colorSpace
		pHeaderInfo->colorSpace = COLOR_GRAYSCALE; // PGM e' sempre grayscale
    }
    else if(pHeaderInfo->type==WEBP_PICTURE)
    {
        // usa libwebp per ottenere le informazioni
        WebPBitstreamFeatures features;
        VP8StatusCode status = WebPGetFeatures(pData, dwSize, &features);
        
        if(status == VP8_STATUS_OK)
        {
            pHeaderInfo->width = features.width;
            pHeaderInfo->height = features.height;
            
            // BPP: 24 per RGB, 32 per RGBA
            pHeaderInfo->bpp = features.has_alpha ? 32 : 24;
            
            // determina colorspace
            if(features.has_alpha)
                pHeaderInfo->colorSpace = COLOR_RGBA;
            else
                pHeaderInfo->colorSpace = COLOR_RGB;
            
            pHeaderInfo->colors = 0;
            
            // tipo di compressione WebP (0=UNDEFINED, 1=LOSSY, 2=LOSSLESS, 3=LOSSY+ALPHA)
            pHeaderInfo->compression = features.format;
            
            // qualita' stimata
            // non c'e' un modo diretto per ottenere la qualita' da WebPBitstreamFeatures
            // La qualita' e' persa nella compressione, possiamo solo dire se e' lossy/lossless
            if(features.format == 1) // LOSSY
            {
                pHeaderInfo->quality = 85; // valore default stimato
            }
            else if(features.format == 2) // LOSSLESS
            {
                pHeaderInfo->quality = 100; // Lossless = qualita' massima
            }
            
            // risoluzione default (WebP non ha risoluzione nei metadati)
            pHeaderInfo->xres = 72.0f;
            pHeaderInfo->yres = 72.0f;
            pHeaderInfo->restype = RESUNITINCH;
        }
        else
        {
            // fallback: parsing minimale della VP8/VP8L header
            if(dwSize >= 30)
            {
                // per WebP lossless (VP8L)
                if(pData[12] == 'V' && pData[13] == 'P' && pData[14] == '8' && pData[15] == 'L')
                {
                    if(dwSize >= 25)
                    {
                        unsigned int width = 1 + ((pData[21] << 8) | pData[20]);
                        unsigned int height = 1 + ((pData[23] << 8) | pData[22]);
                        pHeaderInfo->width = width & 0x3FFF;
                        pHeaderInfo->height = height & 0x3FFF;
                        pHeaderInfo->bpp = 24; // assume RGB
                        pHeaderInfo->colorSpace = COLOR_RGB;
                        pHeaderInfo->compression = 2; // LOSSLESS
                        pHeaderInfo->quality = 100;
                    }
                }
                // per WebP lossy (VP8)
                else if(pData[12] == 'V' && pData[13] == 'P' && pData[14] == '8' && pData[15] == ' ')
                {
                    if(dwSize >= 26)
                    {
                        unsigned int width = (pData[26] << 8) | pData[25];
                        unsigned int height = (pData[28] << 8) | pData[27];
                        pHeaderInfo->width = width & 0x3FFF;
                        pHeaderInfo->height = height & 0x3FFF;
                        pHeaderInfo->bpp = 24;
                        pHeaderInfo->colorSpace = COLOR_RGB;
                        pHeaderInfo->compression = 1; // LOSSY
                        pHeaderInfo->quality = 85; // Default
                    }
                }
            }
        }
    }
    else if(pHeaderInfo->type==HEIC_PICTURE || pHeaderInfo->type==AVIF_PICTURE /*|| pHeaderInfo->type==HEIF_PICTURE*/)
    {
        // usa libheif per HEIC/HEIF/AVIF (versione semplificata)
        struct heif_context* ctx = heif_context_alloc();
        if(ctx)
        {
            struct heif_error err = heif_context_read_from_memory(ctx, pData, dwSize, NULL);
            if(err.code == heif_error_Ok)
            {
                struct heif_image_handle* handle = NULL;
                err = heif_context_get_primary_image_handle(ctx, &handle);
                if(err.code == heif_error_Ok && handle)
                {
                    // dimensioni (funzioni base che dovrebbero esistere)
                    pHeaderInfo->width = heif_image_handle_get_width(handle);
                    pHeaderInfo->height = heif_image_handle_get_height(handle);
                    
                    // profondita' colore - controllo semplificato
                    int hasAlpha = 0;
                    
                    // prova a vedere se ha canale alpha
                    // metodo 1: controlla se ha piu' di un canale
                    int nChannels = 3; // Assume RGB
                    
                    // metodo 2: cerca informazioni sul colore nei metadati
                    // per semplicita', assume valori comuni
                    pHeaderInfo->bpp = 24; // assume 8-bit per canale RGB
                    
                    // colorspace - assume RGB per HEIC/AVIF
                    pHeaderInfo->colorSpace = COLOR_RGB;
                    
                    // usa valori di default ragionevoli
                    if(pHeaderInfo->width > 0 && pHeaderInfo->height > 0)
                    {
                        int metadata_count = 0;
                        
                        pHeaderInfo->colors = 0;
                        pHeaderInfo->compression = -1;
                        pHeaderInfo->quality = -1;
                        
                        pHeaderInfo->xres = 72.0f;
                        pHeaderInfo->yres = 72.0f;
                        pHeaderInfo->restype = RESUNITINCH;
                    }
                    
                    heif_image_handle_release(handle);
                }
            }
            heif_context_free(ctx);
        }
        else
        {
            // fallback: parsing minimale ISOBMFF per dimensioni
            DWORD offset = 0;
            while(offset < dwSize - 8)
            {
                DWORD boxSize = (pData[offset] << 24) | (pData[offset+1] << 16) | (pData[offset+2] << 8) | pData[offset+3];
                if(boxSize==0 || boxSize < 8)
					break;
                
                DWORD boxType = (pData[offset+4] << 24) | (pData[offset+5] << 16) | (pData[offset+6] << 8) | pData[offset+7];
                
                // 'ispe' box (image spatial extents)
                if(boxType == 0x69737065 && offset + 20 <= dwSize) // 'ispe'
                {
                    pHeaderInfo->width = (pData[offset+12] << 24) | (pData[offset+13] << 16) | (pData[offset+14] << 8) | pData[offset+15];
                    pHeaderInfo->height = (pData[offset+16] << 24) | (pData[offset+17] << 16) | (pData[offset+18] << 8) | pData[offset+19];
                    pHeaderInfo->bpp = 24; // assume RGB
                    pHeaderInfo->colorSpace = COLOR_RGB;
                    pHeaderInfo->colors = 0;
                    break;
                }
                offset += boxSize;
            }
        }
    }
    
	if(pHeaderInfo->type!=NULL_PICTURE)
	{
		// calcola memoria approssimativa usata (solo per riferimento)
		if(pHeaderInfo->width > 0 && pHeaderInfo->height > 0 && pHeaderInfo->bpp > 0)
			pHeaderInfo->memused = pHeaderInfo->width * pHeaderInfo->height * (pHeaderInfo->bpp / 8);
	}
	else
		dwRet = GDI_ERROR;
done:

	if(pMap)
		::UnmapViewOfFile(pMap);
	if(hMap!=INVALID_HANDLE_VALUE)
		::CloseHandle(hMap);
	if(hHandle!=INVALID_HANDLE_VALUE)
		::CloseHandle(hHandle);

	if(dwRet==GDI_ERROR)
	{
		char buffer[128] = {0};
		snprintf(buffer,sizeof(buffer),"GetHeaderInfo(): unable to retrieve info");
#ifdef DEBUG
		::MessageBox(NULL,buffer,"CImage",MB_ICONERROR|MB_TASKMODAL|MB_SETFOREGROUND|MB_TOPMOST);
#endif
	}

	return(dwRet);
}

/*
	CheckPNGHeader()
*/
/* STATIC LOCAL */int CImage::CheckPNGHeader(LPCSTR lpcszFileName,PNG_FORMAT_INFO* pngInfo)
{
	int nResult = 0;
    HANDLE hFile;
    DWORD bytesRead;
    BYTE header[64];
	DWORD length;
    
    hFile = CreateFile(lpcszFileName, GENERIC_READ, FILE_SHARE_READ, NULL, 
                       OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    
    if (hFile == INVALID_HANDLE_VALUE) {
        goto done;
    }
    
    // leggi i primi 50 byte (per essere sicuri di prendere IHDR)
    if(!ReadFile(hFile, header, 50, &bytesRead, NULL) || bytesRead < 30)
	{
        CloseHandle(hFile);
        goto done;
    }
    
    CloseHandle(hFile);
    
    // verifica firma PNG
    static const BYTE pngSig[8] = {0x89,'P','N','G',0x0D,0x0A,0x1A,0x0A};
    if(memcmp(header, pngSig, 8)!=0)
        goto done;

    // cerca il chunk IHDR
    
    // offset calcolati:
    // byte 8-11: chunk length (dovrebbe essere 13)
    // byte 12-15: chunk type "IHDR" (ASCII)
    // byte 16-19: width
    // byte 20-23: height
    // byte 24: bit depth
    // byte 25: color type
    
    // verifica che sia davvero IHDR
    if(header[12] != 'I' || header[13] != 'H' || header[14] != 'D' || header[15] != 'R')
        goto done;

	// difesa contro file corrotti
	length =
    ((DWORD)header[8] << 24) |
    ((DWORD)header[9] << 16) |
    ((DWORD)header[10] << 8) |
    header[11];

	// IHDR deve avere lunghezza 13
    if(length!=13)
        goto done;

    if(memcmp(header + 12,"IHDR",4)!=0)
        goto done;

	{
    DWORD width =
        ((DWORD)header[16] << 24) |
        ((DWORD)header[17] << 16) |
        ((DWORD)header[18] << 8) |
        (DWORD)header[19];

    DWORD height =
        ((DWORD)header[20] << 24) |
        ((DWORD)header[21] << 16) |
        ((DWORD)header[22] << 8) |
        (DWORD)header[23];

    BYTE bitDepth  = header[24];
    BYTE colorType = header[25];

    int channels;

    switch(colorType)
    {
        case 0: channels = 1; break; // grayscale
        case 2: channels = 3; break; // RGB
        case 3: channels = 1; break; // indexed
        case 4: channels = 2; break; // grayscale + alpha
        case 6: channels = 4; break; // RGBA
        default:
        goto done;
    }

    int bpp = bitDepth * channels;

    pngInfo->width = width;
    pngInfo->height = height;
    pngInfo->channels = channels;
    pngInfo->bpc = bitDepth;
    pngInfo->bpp = bpp;
    pngInfo->colorType = colorType;
	}

	nResult = 1;

done:

	if(nResult==0)
	{
		char buffer[128] = {0};
		snprintf(buffer,sizeof(buffer),"CheckPNGHeader(): unable to retrieve info");
#ifdef DEBUG
		::MessageBox(NULL,buffer,"CImage",MB_ICONERROR|MB_TASKMODAL|MB_SETFOREGROUND|MB_TOPMOST);
#endif
	}

    return(nResult);
}

/*
	GetColorSpace()
*/
/* LOCAL */LPCSTR CImage::GetColorSpace(void)
{
	return(GetColorSpace((COLOR_TYPE_STD)m_InfoHeader.colorSpace));
}

/*
	GetColorSpace()
*/
/* STATIC LOCAL */LPCSTR CImage::GetColorSpace(COLOR_TYPE_STD color_type)
{
	static const char* colorType[] = {
		"grayscale",
		"color palette",
		"RGB 24bit",
		"RGB+Alpha 32bit",
		"CMYK",
		"YCbCr (JPEG)",
		"CIE L*a*b*",
		"alpha/mask",
		"unknown color space"
	};

	color_type = (color_type >= COLOR_GRAYSCALE && color_type <=COLOR_ALPHA) ? color_type : (COLOR_TYPE_STD)COLOR_UNKNOWN;

	return(colorType[color_type]);
}

/*
	CreateBitmap()
*/
/* LOCAL */HBITMAP CImage::CreateBitmap(void)
{
	BOOL bResult = FALSE;

    int bpp = (int)GetBPP();
    int width = GetWidth();
    int height = GetHeight();
    int stride = (int)GetBytesWidth();
    void* pPixels = GetPixels();
    QWORD largeSize = 0LL;
	DWORD imageSize = 0L;
	BITMAPINFOHEADER bih = {0};
    LPBITMAPINFO pbmi = NULL;
    BYTE bmiBuffer[sizeof(BITMAPINFOHEADER) + 256 * sizeof(RGBQUAD)] = {0};
	HBITMAP hBitmap = NULL;
    HDC hDC = NULL;
    void* pDibBits = NULL;

    if(!pPixels || width <= 0 || height <= 0 || bpp <= 0)
	{
		SetLastErrorDescriptionEx("%s(): invalid bitmap data",__func__);
        goto done;
    }

    // controllo overflow per biSizeImage
    // stride * height potrebbe eccedere DWORD_MAX per immagini molto grandi
    largeSize = (QWORD)stride * (QWORD)height;
    if(largeSize > 0xFFFFFFFFUL)	// >4GB
	{
		SetLastErrorDescriptionEx("%s(): bitmap size overflow",__func__);
        goto done;
    }
    
    imageSize = (DWORD)largeSize;
    
    // prepara BITMAPINFOHEADER (top-down: biHeight negativo)
    bih.biSize = sizeof(BITMAPINFOHEADER);
    bih.biWidth = width;
    bih.biHeight = -height; // negativo (top-down)
    bih.biPlanes = 1;
    bih.biBitCount = (WORD)bpp;
    bih.biCompression = BI_RGB;
    bih.biSizeImage = imageSize;
    
    // gestione palette (per bpp <= 8)
    if(bpp <= 8)
    {
        pbmi = (LPBITMAPINFO)bmiBuffer;
        memcpy(&pbmi->bmiHeader,&bih,sizeof(BITMAPINFOHEADER));
        
        LPBITMAPINFO existingBMI = GetBMI();
        if(existingBMI && existingBMI->bmiHeader.biBitCount <= 8)
        {
            int numColors = GetNumColors();
            if(numColors==0)
                numColors = 1 << bpp;

            // limita a massimo 256 colori
            numColors = min(numColors,256);
            memcpy(pbmi->bmiColors,existingBMI->bmiColors,numColors * sizeof(RGBQUAD));
        }
        else
        {
            int numColors = 1 << bpp;
            numColors = min(numColors,256);
            for(int i = 0; i < numColors; i++)
            {
                BYTE gray = (BYTE)((i * 255) / ((numColors > 1) ? (numColors - 1) : 1));
                pbmi->bmiColors[i].rgbRed = gray;
                pbmi->bmiColors[i].rgbGreen = gray;
                pbmi->bmiColors[i].rgbBlue = gray;
                pbmi->bmiColors[i].rgbReserved = 0;
            }
        }
    }
    else
    {
        pbmi = (LPBITMAPINFO)&bih;
    }
    
    // crea DIB section (top-down, vedi sopra)
    hDC = ::CreateCompatibleDC(NULL);
    if(!hDC)
	{
		SetLastErrorDescriptionEx("%s(): unable to create the DC",__func__);
        goto done;
    }
    
    hBitmap = ::CreateDIBSection(hDC,pbmi,DIB_RGB_COLORS,&pDibBits,NULL,0);
    if(!hBitmap || !pDibBits)
    {
        ::DeleteDC(hDC);
		SetLastErrorDescriptionEx("%s(): unable to create the DIB section",__func__);
        goto done;
    }
    
    // copia i dati
    if(GetDIBOrder()==-1) // source top-down (es. NexgenIPL), copia diretta
    {
        memcpy(pDibBits,pPixels,imageSize);
    }
    else // source bottom-up (es. PaintLib, FreeImage) o default, inverte le righe per la copia
    {
        for(int y = 0; y < height; y++)
        {
            BYTE* srcLine = (BYTE*)pPixels + y * stride;
            BYTE* dstLine = (BYTE*)pDibBits + (height - 1 - y) * stride;
            memcpy(dstLine, srcLine, stride);
        }
    }

	bResult = TRUE;

done:
    
	if(hDC)
		::DeleteDC(hDC);

	if(!bResult)
		SetLastErrorDescriptionEx("%s(): unable to create the bitmap",__func__);

    return(hBitmap);
}

/*
	Flush()
*/
/* LOCAL */BOOL CImage::Flush(LPCSTR lpcszFileName)
{
	ASSERTEXPR(lpcszFileName);

	BOOL bFlushed = FALSE;

	// forza il sistema a svuotare i buffer di scrittura su disco
	HANDLE hFile = CreateFile(	lpcszFileName, 
								GENERIC_READ | GENERIC_WRITE, 
								0, // accesso esclusivo
								NULL,
								OPEN_EXISTING, 
								FILE_ATTRIBUTE_NORMAL, 
								NULL);

	if(hFile!=INVALID_HANDLE_VALUE)
	{
		FlushFileBuffers(hFile); // forza il commit dei dati
		CloseHandle(hFile);
		bFlushed = TRUE;
	}

	ASSERTEXPR(bFlushed);

	return(bFlushed);
}

/*
	Draw()
*/
BOOL CImage::Draw(HDC hDC, int x, int y)
{
	if(!GetPixels() || !GetBMI())
	{
		SetLastErrorDescriptionEx("%s(): unable to draw the bitmap",__func__);
		return FALSE;
	}

	int nW = GetWidth();
	int nH = GetHeight();

	// Usiamo StretchDIBits per la massima compatibilità con i vari BPP
	return ::StretchDIBits(
	hDC,
	x, y, nW, nH,           // Destinazione
	0, 0, nW, nH,           // Sorgente
	GetPixels(),
	GetBMI(),
	DIB_RGB_COLORS,
	SRCCOPY) != GDI_ERROR;
}

/*
	Copy()
*/
HANDLE CImage::Copy(RECT& rect)
{
	BOOL bResult = FALSE;
	int width = 0;
	int height = 0;
	HDIB hDIB = NULL;
	DWORD dwError = 0L;

	// calcola dimensioni
	width = rect.right - rect.left;
	height = rect.bottom - rect.top;
	
	// valida
	if(rect.left < 0 || rect.top < 0 || width <= 0 || height <= 0)
	{
		SetLastErrorDescriptionEx("%s(): invalid rectangle", __func__);
		return NULL;
	}

	bResult = TRUE;

	if(bResult)
	{
		HDC hDC = NULL;
		HBITMAP hBitmap = NULL;
		HBITMAP hOldBitmap = NULL;
		BITMAPINFO* pDestBMI = NULL;
		void* pDibBits = NULL;
		BOOL bLocked = TRUE;
		HPALETTE hPal = NULL;
		HPALETTE hOldPal = NULL;
    
		do {
			int srcBpp = GetBPP();
			int srcWidth = GetWidth();
			int srcHeight = GetHeight();
			int destNumColors = GetNumColors();

			// validazione bounds
			if(rect.right > srcWidth || rect.bottom > srcHeight)
			{
				dwError = ERROR_INVALID_PARAMETER;
				break;
			}
        
			int destWidth = width;
			int destHeight = height;
			int destBpp = srcBpp;

			// calcolo stride
			int destStride = ((destWidth * destBpp + 31) / 32) * 4;

			// calcola dimensioni
			DWORD dwHeaderSize = sizeof(BITMAPINFOHEADER);
			DWORD dwPaletteEntries = (destBpp <= 8) ? (1 << destBpp) : 0;
			DWORD dwPaletteSize = dwPaletteEntries * sizeof(RGBQUAD);
			DWORD dwImageSize = (DWORD)destStride * (DWORD)destHeight;
			DWORD dwTotalSize = dwHeaderSize + dwPaletteSize + dwImageSize;

			if (destWidth == 0 || destHeight == 0 || dwImageSize == 0) {
				dwError = ERROR_INVALID_PARAMETER;
				break;
			}

			// alloca DIB destinazione
			hDIB = (HDIB)::GlobalAlloc(GHND, (SIZE_T)dwTotalSize);
			if(!hDIB)
			{
				dwError = GetLastError();
				SetLastErrorDescriptionEx("%s(): memory allocation failed", __func__);
				break;
			}
        
			pDestBMI = (BITMAPINFO*)::GlobalLock(hDIB);
			if(!pDestBMI)
			{
				dwError = GetLastError();
				::GlobalFree(hDIB);
				hDIB = NULL;
				SetLastErrorDescriptionEx("%s(): memory lock failed", __func__);
				break;
			}
        
			// prepara BITMAPINFO destinazione
			memset(&pDestBMI->bmiHeader, 0, dwHeaderSize);
			pDestBMI->bmiHeader.biSize = dwHeaderSize;
			pDestBMI->bmiHeader.biWidth = destWidth;
			pDestBMI->bmiHeader.biHeight = destHeight;
			pDestBMI->bmiHeader.biPlanes = 1;
			pDestBMI->bmiHeader.biBitCount = (WORD)destBpp;
			pDestBMI->bmiHeader.biCompression = BI_RGB;
			pDestBMI->bmiHeader.biSizeImage = dwImageSize;
			pDestBMI->bmiHeader.biClrUsed = dwPaletteEntries;
			pDestBMI->bmiHeader.biClrImportant = 0;

			// passo critico: copia la palette dalla sorgente
			if(destBpp <= 8)
			{
				LPBITMAPINFO pSrcBMI = GetBMI();
				if(pSrcBMI)
				{
					// determina quanti colori ha la sorgente
					int srcColors = (pSrcBMI->bmiHeader.biClrUsed > 0) ? 
									pSrcBMI->bmiHeader.biClrUsed : 
									(1 << pSrcBMI->bmiHeader.biBitCount);
					
					// limita al massimo supportato
					int copyColors = min(srcColors, (int)dwPaletteEntries);
					
					// copia la palette originale
					memcpy(pDestBMI->bmiColors, pSrcBMI->bmiColors, 
						   copyColors * sizeof(RGBQUAD));
				}
			}
        
			// crea DC compatibile
			hDC = ::CreateCompatibleDC(NULL);
			if(!hDC)
			{
				dwError = GetLastError();
				SetLastErrorDescriptionEx("%s(): unable to create the DC", __func__);
				break;
			}
        
			// crea DIBSection con la palette
			hBitmap = ::CreateDIBSection(hDC,pDestBMI,DIB_RGB_COLORS,&pDibBits,NULL,0);
			if(!hBitmap)
			{
				dwError = GetLastError();
				SetLastErrorDescriptionEx("%s(): unable to create the DIB section", __func__);
				break;
			}
        
			hOldBitmap = (HBITMAP)::SelectObject(hDC, hBitmap);
			
			// per immagini indicizzate, deve selezionare anche la palette
			if(destBpp <= 8)
			{
				// crea una palette logica a partire dalla palette RGB
				LOGPALETTE* pLogPal = (LOGPALETTE*)malloc(sizeof(LOGPALETTE) + dwPaletteEntries * sizeof(PALETTEENTRY));
				if(pLogPal)
				{
					pLogPal->palVersion = 0x300;
					pLogPal->palNumEntries = (WORD)dwPaletteEntries;
					
					for(int i = 0; i < (int)dwPaletteEntries; i++)
					{
						pLogPal->palPalEntry[i].peRed = pDestBMI->bmiColors[i].rgbRed;
						pLogPal->palPalEntry[i].peGreen = pDestBMI->bmiColors[i].rgbGreen;
						pLogPal->palPalEntry[i].peBlue = pDestBMI->bmiColors[i].rgbBlue;
						pLogPal->palPalEntry[i].peFlags = 0;
					}
					
					hPal = ::CreatePalette(pLogPal);
					free(pLogPal);
					
					if(hPal)
					{
						hOldPal = ::SelectPalette(hDC, hPal, FALSE);
						::RealizePalette(hDC);
					}
				}
				else
					SetLastErrorDescriptionEx("%s(): memory allocation failed", __func__);
			}
        
			// determina ordine dei pixel sorgente
			int srcY;
			int srcYStart;
			int srcNumLines;
			
			if(GetDIBOrder() < 0) // Top-down
			{
				srcY = rect.top;
				srcYStart = 0;
				srcNumLines = srcHeight;
			}
			else // Bottom-up
			{
				srcY = srcHeight - rect.bottom;
				srcYStart = 0;
				srcNumLines = srcHeight;
			}
        
			// usa StretchDIBits che gestisce meglio le palette
			bResult = ::StretchDIBits(
				hDC,
				0, 0, destWidth, destHeight,
				rect.left, srcY, width, height,
				GetPixels(),
				GetBMI(),
				DIB_RGB_COLORS,
				SRCCOPY) != GDI_ERROR;
        
			if(!bResult)
			{
				dwError = GetLastError();
				break;
			}
			
			// estrae i bit finali dalla DIBSection
			// non sovrascrivere la palette
			BYTE* pDestPixels = (BYTE*)pDestBMI + dwHeaderSize + dwPaletteSize;
			
			::GdiFlush();
			
			// se i pixel sono gia' nel posto giusto, siamo a posto
			if(pDibBits != pDestPixels)
			{
				// copia manualmente
				memcpy(pDestPixels, pDibBits, dwImageSize);
			}
			
			bResult = TRUE;
        
		} while(false);
    
		if(hDC)
		{
			if(hOldPal)
				::SelectPalette(hDC, hOldPal, FALSE);
			if(hOldBitmap)
				::SelectObject(hDC, hOldBitmap);
			::DeleteDC(hDC);
		}
    
		if(hBitmap)
			::DeleteObject(hBitmap);
		
		if(hPal)
			::DeleteObject(hPal);
    
		if(bLocked)
			UnlockData();
    
		if(!bResult && hDIB)
		{
			if(pDestBMI)
				::GlobalUnlock(hDIB);
			::GlobalFree(hDIB);
			hDIB = NULL;
		}
		else if(pDestBMI)
		{
			::GlobalUnlock(hDIB);
		}
    }

	if(!bResult)
		SetLastErrorDescriptionEx("%s(): unable to copy the image (%lu)", __func__, dwError);

    return hDIB;
}

/*
	Paste()
*/
BOOL CImage::Paste(int x, int y, HANDLE hDIB)
{
	BOOL bResult = (!hDIB || !GetPixels()) ? FALSE : TRUE;

	if(bResult)
	{
		LPBITMAPINFO pSrcBMI = (LPBITMAPINFO)GlobalLock((HGLOBAL)hDIB);
		if (!pSrcBMI) 
		{
			SetLastErrorDescriptionEx("%s(): memory lock failed", __func__);
			return FALSE;
		}

		// sorgente
		int nSrcW = pSrcBMI->bmiHeader.biWidth;
		int nSrcH = abs(pSrcBMI->bmiHeader.biHeight);
		int nSrcBPP = pSrcBMI->bmiHeader.biBitCount;
		int nSrcStride = ((nSrcW * nSrcBPP + 31) / 32) * 4;
		
		DWORD dwSrcPaletteSize = pSrcBMI->bmiHeader.biClrUsed * sizeof(RGBQUAD);
		BYTE* pSrcPixels = (BYTE*)pSrcBMI + pSrcBMI->bmiHeader.biSize + dwSrcPaletteSize;
		
		// palette sorgente (per immagini indicizzate)
		RGBQUAD* pSrcPalette = (nSrcBPP <= 8) ? pSrcBMI->bmiColors : NULL;
		int nSrcPaletteEntries = (nSrcBPP <= 8) ? 
			((pSrcBMI->bmiHeader.biClrUsed > 0) ? pSrcBMI->bmiHeader.biClrUsed : (1 << nSrcBPP)) : 0;

		// destinazione
		BYTE* pDestPixels = (BYTE*)GetPixels();
		int nDestW = GetWidth();
		int nDestH = GetHeight();
		int nDestBPP = GetBPP();
		int nDestStride = GetBytesWidth();
		int nDestBytes = nDestBPP / 8;

		// orientamento
		LPBITMAPINFO pDestBMI = GetBMI();
		bool bSrcBottomUp = pSrcBMI->bmiHeader.biHeight > 0;
		bool bDestBottomUp = pDestBMI ? (pDestBMI->bmiHeader.biHeight > 0) : true;

		// copia con orientamento corretto
		for(int i = 0; i < nSrcH; i++)
		{
			int curY = y + i;
			if(curY < 0 || curY >= nDestH)
				continue;

			int destY = bDestBottomUp ? (nDestH - 1 - curY) : curY;
			if(destY < 0 || destY >= nDestH)
				continue;
			
			BYTE* pDestRow = pDestPixels + (destY * nDestStride);

			// calcola riga sorgente con la stessa logica della PasteWithOuterShadow
			int srcY;
			if(bSrcBottomUp)
			{
				// Bottom-up: la prima riga in memoria e' l'ultima dell'immagine
				srcY = nSrcH - 1 - i; // inverte l'ordine
			}
			else
			{
				// Top-down: corrispondenza diretta
				srcY = i;
			}
			
			if(srcY < 0 || srcY >= nSrcH)
				continue;
			
			BYTE* pSrcRow = pSrcPixels + (srcY * nSrcStride);
			BYTE* pDestPos = pDestRow + (x * nDestBytes);

			// copia pixel per pixel in base al formato
			if(nSrcBPP==24)
			{
				for(int j = 0; j < nSrcW; j++)
				{
					if((x + j) >= nDestW)
						break;
					
					pDestPos[0] = pSrcRow[j * 3 + 0]; // B
					pDestPos[1] = pSrcRow[j * 3 + 1]; // G
					pDestPos[2] = pSrcRow[j * 3 + 2]; // R
					
					if(nDestBytes == 4)
						pDestPos[3] = 255;
					
					pDestPos += nDestBytes;
				}
			}
			else if(nSrcBPP==32)
			{
				for(int j = 0; j < nSrcW; j++)
				{
					if((x + j) >= nDestW)
						break;
					
					((DWORD*)pDestPos)[0] = ((DWORD*)pSrcRow)[j];
					
					pDestPos += nDestBytes;
				}
			}
			else if(nSrcBPP == 8 && pSrcPalette)
			{
				for(int j = 0; j < nSrcW; j++)
				{
					if((x + j) >= nDestW)
						break;
					
					BYTE index = pSrcRow[j];
					
					if(index < nSrcPaletteEntries)
					{
						pDestPos[0] = pSrcPalette[index].rgbBlue;
						pDestPos[1] = pSrcPalette[index].rgbGreen;
						pDestPos[2] = pSrcPalette[index].rgbRed;
					}
					else
					{
						pDestPos[0] = pDestPos[1] = pDestPos[2] = 0;
					}
					
					if(nDestBytes == 4)
						pDestPos[3] = 255;
					
					pDestPos += nDestBytes;
				}
			}
		}

		GlobalUnlock((HGLOBAL)hDIB);
	}

	if(!bResult)
		SetLastErrorDescriptionEx("%s(): unable to paste the image",__func__);

    return(bResult);
}

/*
	PasteWithOuterShadow()
*/
/* LOCAL */BOOL CImage::PasteWithOuterShadow(int x, int y, HANDLE hDIB, int nShadow, RECT rcTarget, COLORREF clrShadow)
{
	BOOL bResult = (!hDIB || !GetPixels()) ? FALSE : TRUE;

	if(bResult)
	{
		// estrae le componenti del colore ombra scelto
		BYTE shR = GetRValue(clrShadow);
		BYTE shG = GetGValue(clrShadow);
		BYTE shB = GetBValue(clrShadow);

		LPBITMAPINFO pSrcBMI = (LPBITMAPINFO)GlobalLock((HGLOBAL)hDIB);
		if (!pSrcBMI)
		{
			SetLastErrorDescriptionEx("%s(): memory lock failed", __func__);
			return FALSE;
		}
		
		// info sorgente
		int nSrcW = pSrcBMI->bmiHeader.biWidth;
		int nSrcH = abs(pSrcBMI->bmiHeader.biHeight);
		int nSrcBPP = pSrcBMI->bmiHeader.biBitCount;
		int nSrcStride = ((nSrcW * nSrcBPP + 31) / 32) * 4;
		
		// calcola inizio pixel sorgente (dopo header + palette)
		DWORD dwSrcPaletteSize = pSrcBMI->bmiHeader.biClrUsed * sizeof(RGBQUAD);
		BYTE* pSrcPixels = (BYTE*)pSrcBMI + pSrcBMI->bmiHeader.biSize + dwSrcPaletteSize;
		
		// palette sorgente (per immagini indicizzate)
		RGBQUAD* pSrcPalette = (nSrcBPP <= 8) ? pSrcBMI->bmiColors : NULL;
		int nSrcPaletteEntries = (nSrcBPP <= 8) ? 
			((pSrcBMI->bmiHeader.biClrUsed > 0) ? pSrcBMI->bmiHeader.biClrUsed : (1 << nSrcBPP)) : 0;
		
		// info destinazione (sempre 32 bit)
		BYTE* pDestPixels = (BYTE*)GetPixels();
		int nDestW = GetWidth();
		int nDestH = GetHeight();
		int nDestBPP = GetBPP(); // dovrebbe essere 32
		int nDestStride = GetBytesWidth();
		int nDestBytes = nDestBPP / 8;
		
		LPBITMAPINFO pDestBMI = GetBMI();
		bool bDestBottomUp = pDestBMI ? (pDestBMI->bmiHeader.biHeight > 0) : true;
		bool bSrcBottomUp = pSrcBMI->bmiHeader.biHeight > 0;

		// fase 1: ombra con colore e sfumatura dolce
		// (questa fase lavora solo sullo sfondo)
		if(nShadow > 0)
		{
			float fNShadow = (float)nShadow;

			for(int i = -nShadow; i < nSrcH + nShadow; i++)
			{
				int curY = (int)y + i;
				if(curY < 0 || curY >= nDestH)
					continue; 
            
				int memoryY = bDestBottomUp ? (nDestH - 1 - curY) : curY;
				BYTE* pDestRow = pDestPixels + (memoryY * nDestStride);

				for(int j = -nShadow; j < nSrcW + nShadow; j++)
				{
					// se sta dentro la foto, non applica l'ombra
					if(i >= 0 && i < nSrcH && j >= 0 && j < nSrcW)
						continue;

					int curX = (int)x + j;
					if(curX < 0 || curX >= nDestW)
						continue;

					int dx = (j < 0) ? -j : (j >= nSrcW ? j - (nSrcW - 1) : 0);
					int dy = (i < 0) ? -i : (i >= nSrcH ? i - (nSrcH - 1) : 0);

					// calcolo distanza
					float fDist = (dx > 0 && dy > 0) ? sqrtf((float)(dx*dx + dy*dy)) : (float)(dx > 0 ? dx : dy);

					if(fDist < fNShadow)
					{
						float t = fDist / fNShadow;
						float fStrength = powf(1.0f - t, 2.0f); 
						float fAlpha = 1.0f - fStrength;

						BYTE* pDestPixel = pDestRow + (curX * nDestBytes);

						// LERP (Linear Interpolation)
						pDestPixel[0] = (BYTE)(pDestPixel[0] * fAlpha + shB * fStrength);
						pDestPixel[1] = (BYTE)(pDestPixel[1] * fAlpha + shG * fStrength);
						pDestPixel[2] = (BYTE)(pDestPixel[2] * fAlpha + shR * fStrength);
						// pDestPixel[3] rimane invariato (alpha channel)
					}
				}
			}
		}

		// fase 2: foto nitida
		for(int i = 0; i < nSrcH; i++)
		{
			int curY = (int)y + i;
			if(curY < 0 || curY >= nDestH)
				continue;
			
			int memoryY = bDestBottomUp ? (nDestH - 1 - curY) : curY;
			BYTE* pDestRow = pDestPixels + (memoryY * nDestStride);

			// calcola la riga sorgente in base all'orientamento
			int srcY;
			if(bSrcBottomUp)
			{
				// Bottom-up: la prima riga in memoria e' l'ultima dell'immagine
				srcY = nSrcH - 1 - i; // inverte l'ordine
			}
			else
			{
				// Top-down: corrispondenza diretta
				srcY = i;
			}
	
			// verifica bounds
			if(srcY < 0 || srcY >= nSrcH)
				continue;

			BYTE* pSrcRow = pSrcPixels + (srcY * nSrcStride);

			for(int j = 0; j < nSrcW; j++)
			{
				int curX = (int)x + j;
				if(curX < 0 || curX >= nDestW)
					continue;
				
				BYTE* pDestPixel = pDestRow + (curX * nDestBytes);
				
				// gestione in base al BPP sorgente
				if(nSrcBPP == 24)
				{
					// 24 bit: copia diretta RGB
					BYTE* pSrcPixel = pSrcRow + (j * 3);
					pDestPixel[0] = pSrcPixel[0]; // B
					pDestPixel[1] = pSrcPixel[1]; // G
					pDestPixel[2] = pSrcPixel[2]; // R
				}
				else if(nSrcBPP == 32)
				{
					// 32 bit: copia BGRA
					((DWORD*)pDestPixel)[0] = ((DWORD*)pSrcRow)[j];
				}
				else if(nSrcBPP == 8 && pSrcPalette)
				{
					// 8 bit indicizzato (GIF): converte indice in RGB usando la palette
					BYTE index = pSrcRow[j]; // un byte = indice
					
					// verifica che l'indice sia valido
					if(index < nSrcPaletteEntries)
					{
						pDestPixel[0] = pSrcPalette[index].rgbBlue;  // B
						pDestPixel[1] = pSrcPalette[index].rgbGreen; // G
						pDestPixel[2] = pSrcPalette[index].rgbRed;   // R
					}
					else
					{
						// indice non valido: nero
						pDestPixel[0] = 0;
						pDestPixel[1] = 0;
						pDestPixel[2] = 0;
					}
				}
				else if((nSrcBPP == 4 || nSrcBPP == 1) && pSrcPalette)
				{
					// 4 o 1 bit: formati packed
					int pixelsPerByte = 8 / nSrcBPP;
					BYTE mask = (1 << nSrcBPP) - 1;
					
					int byteOffset = j / pixelsPerByte;
					int bitShift;
					if(nSrcBPP==1)
					{
						bitShift = 7 - (j % 8); // ordine corretto per 1 bpp
					}
					else
					{
						bitShift = (j % pixelsPerByte) * nSrcBPP;
					}
					
					BYTE index = (pSrcRow[byteOffset] >> bitShift) & mask;
					
					if(index < nSrcPaletteEntries)
					{
						pDestPixel[0] = pSrcPalette[index].rgbBlue;
						pDestPixel[1] = pSrcPalette[index].rgbGreen;
						pDestPixel[2] = pSrcPalette[index].rgbRed;
					}
					else
					{
						pDestPixel[0] = pDestPixel[1] = pDestPixel[2] = 0;
					}
				}
				else
				{
					// formato non supportato: nero
					pDestPixel[0] = 0;
					pDestPixel[1] = 0;
					pDestPixel[2] = 0;
				}
				
				// imposta alpha a opaco (se destinazione ha alpha)
				if(nDestBytes == 4)
					pDestPixel[3] = 255;
			}
		}

		GlobalUnlock((HGLOBAL)hDIB);
    }

	if(!bResult)
		SetLastErrorDescriptionEx("%s(): unable to paste the image",__func__);

    return(bResult);
}

/*
    Crop()

    Ritaglio manuale di un blocco di pixel via GDI.
    Restituisce un nuovo oggetto CImage della dimensione richiesta.
*/
CImageObject* CImage::Crop(int x, int y, int width, int height)
{
	CImage* pNewImage = NULL;

    // preparazione metadati originali
    int srcWidth  = GetWidth();
    int srcHeight = GetHeight();
    int bpp       = GetBPP();
    int srcStride = GetBytesWidth(srcWidth, bpp, 4); // stride dell'immagine originale (allineato 4 byte)
    
	if(bpp <= 8)
	{
		SetLastErrorDescriptionEx("%s(): more than 8 BPP required",__func__);
		return(pNewImage);
	}

    // validazione coordinate
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x + width > srcWidth)   width  = srcWidth - x;
    if (y + height > srcHeight) height = srcHeight - y;

    // calcolo dimensioni nuovo DIB ritagliato
    int dstStride = WIDTHBYTES(width * bpp, GetAlignment());
    int numColors = (bpp <= 8) ? GetNumColors() : 0;
    if (bpp <= 8 && numColors == 0) numColors = 1 << bpp;
    int paletteSize = numColors * sizeof(RGBQUAD);
    
    DWORD totalSize = sizeof(BITMAPINFOHEADER) + paletteSize + (dstStride * height);

    // Allocazione HDIB di destinazione
    HDIB hDstDib = (HDIB)::GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, totalSize);
    if(hDstDib)
	{
		BITMAPINFOHEADER* pDstBmi = (BITMAPINFOHEADER*)::GlobalLock((HGLOBAL)hDstDib);
		BYTE* pSrcBits = (BYTE*)GetPixels();

		if(pDstBmi && pSrcBits)
		{
			// inizializzazione header destinazione
			pDstBmi->biSize        = sizeof(BITMAPINFOHEADER);
			pDstBmi->biWidth       = width;
			pDstBmi->biHeight      = height; // lo crea Bottom-Up (standard Windows)
			pDstBmi->biPlanes      = 1;
			pDstBmi->biBitCount    = (WORD)bpp;
			pDstBmi->biCompression = BI_RGB;
			pDstBmi->biSizeImage   = dstStride * height;
			pDstBmi->biClrUsed     = numColors;

			// copia palette (se presente)
			if (paletteSize > 0) {
				LPBITMAPINFO pSrcBmi = GetBMI();
				if (pSrcBmi) memcpy(pDstBmi + 1, pSrcBmi->bmiColors, paletteSize);
			}

			// copia pixel riga x riga
			BYTE* pDstBits = (BYTE*)(pDstBmi) + sizeof(BITMAPINFOHEADER) + paletteSize;
			int dibOrder = GetDIBOrder(); 

			for(int i = 0; i < height; i++)
			{
				int srcY;
    
				// se dibOrder < 0 (Top-Down), la riga 'y' e' la prima da leggere
				// se dibOrder > 0 (Bottom-Up), la riga 'y' e' (Height - 1 - y)
				if(dibOrder < 0) 
					srcY = y + i; 
				else 
					srcY = (srcHeight - 1) - (y + i); 

				// puntatori:
				// sorgente: usa srcStride calcolato correttamente
				BYTE* pSrcRow = pSrcBits + (srcY * srcStride) + (x * (bpp / 8));
    
				// destinazione: l'HDIB di Windows DEVE essere Bottom-Up
				// quindi la riga 'i' del ritaglio deve finire nella posizione (height - 1 - i)
				BYTE* pDstRow = pDstBits + ((height - 1 - i) * dstStride);
    
				// copia:
				// copia esattamente i byte dei pixel
				// il resto della riga (padding) e' gia' zero grazie a GMEM_ZEROINIT
				memcpy(pDstRow, pSrcRow, width * (bpp / 8));
			}
		}
		::GlobalUnlock((HGLOBAL)hDstDib);

		// creazione nuovo oggetto tramite Factory e SetDIB
		pNewImage = CreateImage(); // crea istanza della classe derivata corretta
		if(pNewImage)
		{
			if(!pNewImage->SetDIB(hDstDib))
			{
				delete pNewImage;
				pNewImage = NULL;
			}
		}

		::GlobalFree((HGLOBAL)hDstDib);
	}

	if(!pNewImage)
		SetLastErrorDescriptionEx("%s(): unable to crop the image",__func__);

	return pNewImage;
}

// Helper: Funzione Sinc
double Sinc(double x)
{
    if(x==0.0)
		return 1.0;
    x *= M_PI;
    return(sin(x) / x);
}

// Helper: Kernel Lanczos3
double Lanczos3_Kernel(double x)
{
    double ax = fabs(x);
    if(ax < 3.0)
        return(Sinc(ax) * Sinc(ax / 3.0));
    return(0.0);
}

// revisata per libs, funziona
CImage* CImage::_RescaleLanczos3(int newWidth, int newHeight)
{
    if(newWidth <= 0 || newHeight <= 0)
		return NULL;

    int srcWidth = this->GetWidth();
    int srcHeight = this->GetHeight();
    int bpp = this->GetBPP();
    int cpp = (bpp > 24) ? 4 : 3; 

    // usiamo GetBytesWidth() della classe base per essere sicuri dello stride sorgente
    int srcStride = this->GetBytesWidth(); 
    int bytesPerPixel = cpp;

    BYTE* pSrcBits = (BYTE*)this->GetPixels();
    if(!pSrcBits)
		return NULL;

    int dibOrder = GetDIBOrder();

    // allocazione immagine di destinazione
    int dstStride = WIDTHBYTES(newWidth * bpp, GetAlignment());
    DWORD headerSize = sizeof(BITMAPINFOHEADER);
    DWORD imageSize = dstStride * newHeight;
    
    HDIB hDstDib = (HDIB)::GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, headerSize + imageSize);
    if(!hDstDib)
		return NULL;

    BITMAPINFOHEADER* pDstBmi = (BITMAPINFOHEADER*)::GlobalLock((HGLOBAL)hDstDib);
    pDstBmi->biSize = sizeof(BITMAPINFOHEADER);
    pDstBmi->biWidth = newWidth;
    pDstBmi->biHeight = newHeight;
    pDstBmi->biPlanes = 1;
    pDstBmi->biBitCount = (WORD)bpp;
    pDstBmi->biCompression = BI_RGB;
    pDstBmi->biSizeImage = imageSize;

    BYTE* pDstBits = (BYTE*)(pDstBmi) + headerSize;

    // buffer temporaneo (orizzontale)
    int tmpStride = newWidth * bytesPerPixel; 
    std::vector<float> tmpBuffer(tmpStride * srcHeight);

    double scaleX = (double)srcWidth / newWidth;

    // passaggio 1: oriz.
    for(int y = 0; y < srcHeight; y++)
	{
        // gestione ribaltamento riga (DIB Order)
        int realSrcY = (dibOrder < 0) ? y : (srcHeight - 1 - y);
        BYTE* pSrcRow = pSrcBits + (realSrcY * srcStride);
        float* pTmpRow = &tmpBuffer[y * tmpStride];

        for(int x = 0; x < newWidth; x++)
		{
            double srcX = (x + 0.5) * scaleX - 0.5;
            int x1 = (int)floor(srcX) - 2;

            double r = 0, g = 0, b = 0, a = 0, weightSum = 0;

            for(int ix = x1; ix <= x1 + 5; ix++)
			{
                int safeX = (ix < 0) ? 0 : (ix >= srcWidth ? srcWidth - 1 : ix);
                double weight = Lanczos3_Kernel(srcX - ix);
                
                BYTE* pPixel = pSrcRow + (safeX * bytesPerPixel);
                b += pPixel[0] * weight;
                g += pPixel[1] * weight;
                r += pPixel[2] * weight;
                if (bytesPerPixel == 4) a += pPixel[3] * weight;
                
                weightSum += weight;
            }

            if(weightSum != 0)
			{
                int targetIdx = x * bytesPerPixel;
                pTmpRow[targetIdx + 0] = (float)(b / weightSum);
                pTmpRow[targetIdx + 1] = (float)(g / weightSum);
                pTmpRow[targetIdx + 2] = (float)(r / weightSum);
                if(bytesPerPixel == 4)
					pTmpRow[targetIdx + 3] = (float)(a / weightSum);
            }
        }
    }

    // passaggio 2: vert.
    double scaleY = (double)srcHeight / newHeight;

    for(int x = 0; x < newWidth; x++)
	{
        for(int y = 0; y < newHeight; y++)
		{
            double srcY = (y + 0.5) * scaleY - 0.5;
            int y1 = (int)floor(srcY) - 2;

            double r = 0, g = 0, b = 0, a = 0, weightSum = 0;

            for(int iy = y1; iy <= y1 + 5; iy++)
			{
                int safeY = (iy < 0) ? 0 : (iy >= srcHeight ? srcHeight - 1 : iy);
                double weight = Lanczos3_Kernel(srcY - iy);
                
                float* pTmpPixel = &tmpBuffer[(safeY * tmpStride) + (x * bytesPerPixel)];
                b += pTmpPixel[0] * weight;
                g += pTmpPixel[1] * weight;
                r += pTmpPixel[2] * weight;
                if (bytesPerPixel == 4) a += pTmpPixel[3] * weight;

                weightSum += weight;
            }

            // Scrittura finale nella DIB (Bottom-Up di Windows)
            int dstY = (newHeight - 1 - y); 
            BYTE* pDstPixel = pDstBits + (dstY * dstStride) + (x * bytesPerPixel);

            if(weightSum!=0)
			{
                pDstPixel[0] = (BYTE)__max(0.0, __min(255.0, b / weightSum));
                pDstPixel[1] = (BYTE)__max(0.0, __min(255.0, g / weightSum));
                pDstPixel[2] = (BYTE)__max(0.0, __min(255.0, r / weightSum));
                if(bytesPerPixel==4)
					pDstPixel[3] = (BYTE)__max(0.0, __min(255.0, a / weightSum));
            }
        }
    }

    ::GlobalUnlock((HGLOBAL)hDstDib);

    CImage* pNewImage = CreateImage();
    if(pNewImage)
	{
        if(!pNewImage->SetDIB(hDstDib))
		{
            delete pNewImage;
            pNewImage = NULL;
        }
    }

    ::GlobalFree((HGLOBAL)hDstDib);
    return pNewImage;
}

// risoluzione della tabella: 3000 campioni per coprire l'intervallo [0,3]
#define LANCZOS_LUT_RES 3000
static double g_Lanczos3LUT[LANCZOS_LUT_RES + 1];
static bool g_bLanczosInit = false;

// funzione Helper Sinc migliorata
static double Sinc_LUT(double x)
{
    if(x < 1e-9 && x > -1e-9)
		return 1.0;
    double temp = x * M_PI;
    return sin(temp) / temp;
}

// inizializza la tabella (da chiamare una volta sola)
void InitLanczos3LUT(void)
{
    if(g_bLanczosInit)
		return;
    for(int i = 0; i <= LANCZOS_LUT_RES; i++)
	{
        double x = (double)i * 3.0 / (double)LANCZOS_LUT_RES;
        g_Lanczos3LUT[i] = Sinc_LUT(x) * Sinc_LUT(x / 3.0);
    }
    g_bLanczosInit = true;
}

// Kernel ultra-veloce tramite LUT
inline double Lanczos3_Kernel_Fast(double x)
{
    double ax = (x < 0) ? -x : x;
    if(ax >= 3.0)
		return 0.0;
    // mappa ax [0, 3] -> indice [0, 3000]
    // (LANCZOS_LUT_RES / 3.0) = 1000.0
    int index = (int)(ax * 1000.0); 
    return g_Lanczos3LUT[index];
}

/*
	RescaleLanczos3()
*/
CImage* CImage::RescaleLanczos3(int newWidth,int newHeight)
{
	CImage* pNewImage = NULL;
	BOOL bResult = (newWidth <= 0 || newHeight <= 0) ? FALSE : TRUE;

	if(bResult)
	{
		if(GetBPP() < 24)
			bResult = ConvertToBPP(32)==NO_ERROR;
	}

	if(bResult)
	{
		InitLanczos3LUT();

		int srcWidth = this->GetWidth();
		int srcHeight = this->GetHeight();
		int bpp = this->GetBPP();
		int cpp = (bpp > 24) ? 4 : 3; 

		//int srcStride = this->GetBytesWidth(); 
		int srcStride = WIDTHBYTES((srcWidth * bpp),GetAlignment());
		int bytesPerPixel = cpp;

		BYTE* pSrcBits = (BYTE*)this->GetPixels();
		if(pSrcBits)
		{
			int dibOrder = GetDIBOrder();

			// allocazione immagine di destinazione (DIB Win32)
			int dstStride = WIDTHBYTES(newWidth * bpp, GetAlignment());
			DWORD headerSize = sizeof(BITMAPINFOHEADER);
			DWORD imageSize = dstStride * newHeight;
    
			HDIB hDstDib = (HDIB)::GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, headerSize + imageSize);
			if(hDstDib)
			{
				BITMAPINFOHEADER* pDstBmi = (BITMAPINFOHEADER*)::GlobalLock((HGLOBAL)hDstDib);
				pDstBmi->biSize = sizeof(BITMAPINFOHEADER);
				pDstBmi->biWidth = newWidth;
				pDstBmi->biHeight = newHeight;
				pDstBmi->biPlanes = 1;
				pDstBmi->biBitCount = (WORD)bpp;
				pDstBmi->biCompression = BI_RGB;
				pDstBmi->biSizeImage = imageSize;

				BYTE* pDstBits = (BYTE*)(pDstBmi) + headerSize;

				// buffer temporaneo (orizzontale)
				// usa float per mantenere la precisione durante i passaggi
				int tmpStride = newWidth * bytesPerPixel; 
				std::vector<float> tmpBuffer(tmpStride * srcHeight);

				double scaleX = (double)srcWidth / newWidth;

				// passaggio 1: oriz. (sorgente -> buffer float)
				for(int y = 0; y < srcHeight; y++)
				{
					int realSrcY = (dibOrder < 0) ? y : (srcHeight - 1 - y);
					BYTE* pSrcRow = pSrcBits + (realSrcY * srcStride);
					float* pTmpRow = &tmpBuffer[y * tmpStride];

					for(int x = 0; x < newWidth; x++)
					{
						double srcX = (x + 0.5) * scaleX - 0.5;
						int x1 = (int)floor(srcX) - 2;

						double r = 0, g = 0, b = 0, a = 0, weightSum = 0;

						for (int ix = x1; ix <= x1 + 5; ix++) {
							int safeX = (ix < 0) ? 0 : (ix >= srcWidth ? srcWidth - 1 : ix);
                
							// usa la LUT qui
							double weight = Lanczos3_Kernel_Fast(srcX - ix);
                
							BYTE* pPixel = pSrcRow + (safeX * bytesPerPixel);
							b += pPixel[0] * weight;
							g += pPixel[1] * weight;
							r += pPixel[2] * weight;
							if (bytesPerPixel == 4) a += pPixel[3] * weight;
                
							weightSum += weight;
						}

						if(weightSum!=0)
						{
							int targetIdx = x * bytesPerPixel;
							pTmpRow[targetIdx + 0] = (float)(b / weightSum);
							pTmpRow[targetIdx + 1] = (float)(g / weightSum);
							pTmpRow[targetIdx + 2] = (float)(r / weightSum);
							if(bytesPerPixel==4)
								pTmpRow[targetIdx + 3] = (float)(a / weightSum);
						}
					}
				}

				// passaggio 2: vert. (buffer float -> destinazione DIB)
				double scaleY = (double)srcHeight / newHeight;

				for(int x = 0; x < newWidth; x++)
				{
					for(int y = 0; y < newHeight; y++)
					{
						double srcY = (y + 0.5) * scaleY - 0.5;
						int y1 = (int)floor(srcY) - 2;

						double r = 0, g = 0, b = 0, a = 0, weightSum = 0;

						for(int iy = y1; iy <= y1 + 5; iy++)
						{
							int safeY = (iy < 0) ? 0 : (iy >= srcHeight ? srcHeight - 1 : iy);
                
							// usa la LUT qui
							double weight = Lanczos3_Kernel_Fast(srcY - iy);
                
							float* pTmpPixel = &tmpBuffer[(safeY * tmpStride) + (x * bytesPerPixel)];
							b += pTmpPixel[0] * weight;
							g += pTmpPixel[1] * weight;
							r += pTmpPixel[2] * weight;
							if (bytesPerPixel == 4) a += pTmpPixel[3] * weight;

							weightSum += weight;
						}

						// scrittura finale nella DIB (Bottom-Up)
						int dstY = (newHeight - 1 - y); 
						BYTE* pDstPixel = pDstBits + (dstY * dstStride) + (x * bytesPerPixel);

						if(weightSum!=0)
						{
							pDstPixel[0] = (BYTE)__max(0.0, __min(255.0, b / weightSum));
							pDstPixel[1] = (BYTE)__max(0.0, __min(255.0, g / weightSum));
							pDstPixel[2] = (BYTE)__max(0.0, __min(255.0, r / weightSum));
							if(bytesPerPixel==4)
								pDstPixel[3] = (BYTE)__max(0.0, __min(255.0, a / weightSum));
						}
					}
				}

				::GlobalUnlock((HGLOBAL)hDstDib);

				pNewImage = CreateImage();
				if(pNewImage)
				{
					if(!pNewImage->SetDIB(hDstDib))
					{
						delete pNewImage;
						pNewImage = NULL;
					}
				}

				::GlobalFree((HGLOBAL)hDstDib);
			}
		}
	}

	if(!pNewImage)
		SetLastErrorDescriptionEx("%s(): unable to rescale the image",__func__);

    return(pNewImage);
}

/*
	SetLastErrorDescriptionEx()
*/
void CImage::SetLastErrorDescriptionEx(LPCSTR pszFormat,...)
{
	char buffer[1024] = {0};

    va_list args;
    va_start(args,pszFormat);
    vsnprintf(buffer,sizeof(buffer),pszFormat,args);
    va_end(args);

	SetLastErrorDescription(buffer);
	if(*m_szFileName)
	{
		char msg[1024] = {0};
		snprintf(msg,sizeof(msg),"%s\n(filename is: %s)",buffer,StripPathFromFile(m_szFileName));
		strcpyn(buffer,msg,sizeof(buffer));
		SetLastErrorDescription(buffer);
	}

#ifdef DEBUG
	::MessageBox(NULL,GetLastErrorDescription(),GetLibraryName(),MB_ICONERROR|MB_TASKMODAL|MB_SETFOREGROUND|MB_TOPMOST);
#endif
}

// FILTRI --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

/*
	EnumFilters()
*/
/* LOCAL */LPCSTR CImage::EnumFilters(int& iterator)
{
	while(iterator >= 0 && iterator < ARRAY_SIZE(m_filterParams))
	{
		if(m_filterParams[iterator].min!=-1 && m_filterParams[iterator].max!=-1 && m_filterParams[iterator].value!=-1)
			return(m_filterParams[iterator++].function);
		else
			iterator++;
	}
	return(NULL);
}

/*
	EnumValidFilters()
*/
/* LOCAL */LPCSTR CImage::EnumValidFilters(int& iterator,int& index)
{
	while(iterator >= 0 && iterator < ARRAY_SIZE(m_filterParams))
	{
		if(m_filterParams[iterator].min!=-1 && m_filterParams[iterator].max!=-1 && m_filterParams[iterator].value!=-1)
		{
			index++;
			return(m_filterParams[iterator++].function);
		}
		else
			iterator++;
	}
	return(NULL);
}

/*
	CountFilters()
*/
/* LOCAL */int CImage::CountFilters(void)
{
	int nTotFilters = 0;
	for(int i = 0; i < ARRAY_SIZE(m_filterParams); i++)
		if(m_filterParams[i].min!=-1 && m_filterParams[i].max!=-1 && m_filterParams[i].value!=-1)
			nTotFilters++;
	return(nTotFilters);
}

/*
	Blur()
*/
UINT CImage::Blur(void)
{
	BOOL bResult = TRUE;

    UINT bpp = GetBPP();
	if(bpp < 24)
		bResult = ConvertToBPP(24)==NO_ERROR;
		
	if(bResult)
	{
		// ottiene parametro intensity (0.0 - 1.0)
		double intensity = 0.0;
		if(!GetFilterParams("Blur", intensity))
		{
			SetLastErrorDescriptionEx("%s(): ERROR_INVALID_PARAMETER",__func__);
			return(ERROR_INVALID_PARAMETER);
		}
    
		// validazione/check
		if(intensity < 0.001)
			return(NO_ERROR);
		if(intensity > 1.0)
			intensity = 1.0;

		// dati immagine
		BYTE* pPixels = (BYTE*)GetPixels();
		UINT width = GetWidth();
		UINT height = GetHeight();
		UINT stride = GetBytesWidth();
		int bytesPerPixel = bpp / 8;

		if(!pPixels || width==0 || height==0)
		{
			SetLastErrorDescriptionEx("%s(): invalid data",__func__);
			return(GDI_ERROR);
		}

		// calcolo del raggio (mappato 0.0-1.0 -> 1-30 pixel)
		// ridotto a 30 per una progressione piu' naturale
		int radius = (int)(intensity * 30.0);
		if(radius < 1)
			radius = 1;

		// allocazione buffer temporanei
		UINT bufferSize = stride * height;
		std::vector<BYTE> tempSrc(bufferSize);
		std::vector<BYTE> tempIntermediate(bufferSize);
    
		// copia l'originale nel buffer sorgente
		memcpy(tempSrc.data(), pPixels, bufferSize);

		// passaggio 1: sfocatura oriz.
		int divisor = (2 * radius + 1);

		for(UINT y = 0; y < height; y++)
		{
			for(int ch = 0; ch < 3; ch++) // Solo R, G, B
			{
				BYTE* pSrcRow = tempSrc.data() + (y * stride) + ch;
				BYTE* pIntRow = tempIntermediate.data() + (y * stride) + ch;

				long sum = 0;
				// inizializzazione sliding window
				for(int i = -radius; i <= radius; i++)
					sum += pSrcRow[Clamp(i, 0, width - 1) * bytesPerPixel];

				for(UINT x = 0; x < width; x++)
				{
					pIntRow[x * bytesPerPixel] = (BYTE)(sum / divisor);
                
					// shift finestra
					sum -= pSrcRow[Clamp((int)x - radius, 0, (int)width - 1) * bytesPerPixel];
					sum += pSrcRow[Clamp((int)x + radius + 1, 0, (int)width - 1) * bytesPerPixel];
				}
			}
		}

		// passaggio 2: sfocatura vert.
		for(UINT x = 0; x < width; x++)
		{
			for(int ch = 0; ch < 3; ch++)
			{
				long sum = 0;
				// inizializzazione sliding window verticale
				for(int i = -radius; i <= radius; i++)
				{
					int row = Clamp(i, 0, (int)height - 1);
					sum += tempIntermediate[row * stride + x * bytesPerPixel + ch];
				}

				for(UINT y = 0; y < height; y++)
				{
					// scrive direttamente sul buffer originale dell'immagine
					pPixels[y * stride + x * bytesPerPixel + ch] = (BYTE)(sum / divisor);

					// shift finestra
					int top = Clamp((int)y - radius, 0, (int)height - 1);
					int bottom = Clamp((int)y + radius + 1, 0, (int)height - 1);
					sum -= tempIntermediate[top * stride + x * bytesPerPixel + ch];
					sum += tempIntermediate[bottom * stride + x * bytesPerPixel + ch];
				}
			}
		}
	}

    return(NO_ERROR);
}

/*
	Brightness()

	Imposta la luminosita' dell'immagine a seconda del parametro:

	-100 = immagine completamente nera
	  0  = nessuna modifica
	+100 = massima luminosita' senza perdita di dettagli
*/
#if 1
// versione ibrida
UINT CImage::Brightness(void)
{
	// ricava il valore del parametro
    int nFactor = 0;
    double nValue = 0;
    if(GetFilterParams("Brightness",nValue))
        nFactor = (int)nValue;
	else
	{
		SetLastErrorDescriptionEx("%s(): ERROR_INVALID_PARAMETER",__func__);
		return(ERROR_INVALID_PARAMETER);
	}
    
    if(nFactor!=0)
    {    
        int width = GetWidth();
        int height = GetHeight();
        
        // soglia per attivare anti-clipping (es: oltre +70)
        bool useAntiClipping = (nFactor > 70);
        
        for(int y = 0; y < height; y++)
        {
            for(int x = 0; x < width; x++)
            {
                COLORREF c = GetPixel(x, y);
                
                int r = GetRValue(c);
                int g = GetGValue(c);
                int b = GetBValue(c);
                
                float factor = 1.0f + (nFactor / 100.0f);
                
                if(useAntiClipping)
                {
                    // anti-clipping solo per alti valori
                    int maxChannel = max(max(r, g), b);
                    if(maxChannel * factor > 255.0f)
                    {
                        // scala proporzionalmente
                        float scale = 255.0f / (maxChannel * factor);
                        factor *= scale;
                    }
                }
                
                r = (int)(r * factor);
                g = (int)(g * factor);
                b = (int)(b * factor);
                
                // clamping
                r = min(max(r, 0), 255);
                g = min(max(g, 0), 255);
                b = min(max(b, 0), 255);
                
                c = RGB(r, g, b);
                SetPixel(x, y, c);
            }
        }
    }
    
    return(NO_ERROR);
}
#else
// versione x max luminosita' e ottimizzata con anti-clipping:
// (in teoria x  preservare dettagli e relazioni cromatiche)
UINT CImage::Brightness(void)
{
	// ricava il valore del parametro
    int nFactor = 0;
    double nValue = 0;
    if(GetFilterParams("Brightness",nValue))
        nFactor = (int)nValue;
	else
	{
		SetLastErrorDescriptionEx("%s(): ERROR_INVALID_PARAMETER",__func__);
		return(ERROR_INVALID_PARAMETER);
	}
    
    if(nFactor!=0)
    {    
        int width = GetWidth();
        int height = GetHeight();
        
        for(int y = 0; y < height; y++)
        {
            for(int x = 0; x < width; x++)
            {
                COLORREF c = GetPixel(x, y);
                
                int r = GetRValue(c);
                int g = GetGValue(c);
                int b = GetBValue(c);
                
                if(nFactor > 0)
                {
                    // Aumento luminosità: preserva le relazioni tra colori
                    // Evita clipping scalando proporzionalmente tutti i canali
                    int maxChannel = max(max(r, g), b);
                    if(maxChannel > 0)
                    {
                        // Calcola quanto possiamo aumentare senza superare 255
                        float availableHeadroom = 255.0f / maxChannel;
                        float actualFactor = 1.0f + (nFactor / 100.0f);
                        
                        if(actualFactor > availableHeadroom)
                        {
                            // Scaliamo per evitare clipping
                            float scale = availableHeadroom / actualFactor;
                            actualFactor *= scale;
                        }
                        
                        r = (int)(r * actualFactor);
                        g = (int)(g * actualFactor);
                        b = (int)(b * actualFactor);
                    }
                }
                else
                {
                    // Diminuzione luminosità: formula semplice
                    float darkenFactor = 1.0f + (nFactor / 100.0f);
                    r = (int)(r * darkenFactor);
                    g = (int)(g * darkenFactor);
                    b = (int)(b * darkenFactor);
                }
                
                // Clamping garantito
                r = min(max(r, 0), 255);
                g = min(max(g, 0), 255);
                b = min(max(b, 0), 255);
                
                c = RGB(r, g, b);
                SetPixel(x, y, c);
            }
        }
    }
    
    return(NO_ERROR);
}
#endif

/*
	ColorSwap()

	Scambia i colori dell'immagine a seconda del parametro che indica il tipo di rotazione da
	effettuare con i colori RGB: RGB -> RGB, RGB -> BGR, etc. (vedi macro COLORSWAP_[...]).

	Il modo COLORSWAP_RANDOM converte in grayscale con punteggiatura a colori R,G,B.
*/
UINT CImage::ColorSwap(void)
{
	// non applica a un immagine che e'/sembra grayscale, restituisce errore
	if(IsTrueGrayscale() || IsVisualGrayscaleStatistical(VISUAL_GRAYSCALE_PARAMS))
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
    
    static const int permTable[6][3] = {
        {0, 1, 2}, // COLORSWAP_IDENTITY (0)
        {2, 1, 0}, // COLORSWAP_RB (1)
        {1, 2, 0}, // COLORSWAP_ROTATE_FWD (2)
        {0, 2, 1}, // COLORSWAP_GB (3)
        {2, 0, 1}, // COLORSWAP_ROTATE_REV (4)
        {1, 0, 2}  // COLORSWAP_RG (5)
    };
    
    int width = GetWidth();
    int height = GetHeight();
    
    // inizializza x modalita' random
//	if(nFactor==COLORSWAP_RANDOM)
//		srand((unsigned int)time(NULL));

    for(int y = 0; y < height; y++)
    {
        for(int x = 0; x < width; x++)
        {
            COLORREF c = GetPixel(x, y);
            
            int r = GetRValue(c);
            int g = GetGValue(c);
            int b = GetBValue(c);
            
            int newR, newG, newB;
            int nIndex;
            if(nFactor==COLORSWAP_RANDOM)
//				nIndex = rand() % 6;			// 0-5, totale valori possibili: 6
				nIndex = fast_rand_range(6);	// 0-5, totale valori possibili: 6
			else
				nIndex = nFactor;
            const int* perm = permTable[nIndex];
            newR = (perm[0] == 0) ? r : (perm[0] == 1) ? g : b;
            newG = (perm[1] == 0) ? r : (perm[1] == 1) ? g : b;
            newB = (perm[2] == 0) ? r : (perm[2] == 1) ? g : b;
            
            SetPixel(x, y, RGB(newR, newG, newB));
        }
    }
    
    return(NO_ERROR);
}

/*
	ColorShift()

	Wrapper per ricavare ed impostare i parametri.
*/
UINT CImage::ColorShift(void)
{
	// non applica a un immagine che e'/sembra grayscale, restituisce errore
	if(IsTrueGrayscale() || IsVisualGrayscaleStatistical(VISUAL_GRAYSCALE_PARAMS))
	{
		SetLastErrorDescriptionEx("%s(): image is grayscale, filter will not be applied",__func__);
		return(GDI_ERROR);
	}

	// ricava i valori dei parametri
    char szValues[32] = {0};
    double nValue = 0;
    if(!GetFilterParams("ColorShift",nValue,szValues,sizeof(szValues)))
	{
		SetLastErrorDescriptionEx("%s(): ERROR_INVALID_PARAMETER",__func__);
		return(ERROR_INVALID_PARAMETER);
	}

	int redShift = 0;
	int greenShift = 0;
	int blueShift = 0;
	int shiftMode = 0;
	BOOL preserveLuminance = FALSE;

	const char* delimiter = ";";
    char* token = strtok(szValues,delimiter);

    for(int i=0; token!=NULL; i++)
	{
		switch(i) {
			case 0:	redShift = atoi(token);   break;
			case 1:	greenShift = atoi(token); break;
			case 2:	blueShift = atoi(token);  break;
			case 3:	shiftMode = atoi(token);  break;
			case 4:	if(strcmp(token,"TRUE")==0)
						preserveLuminance = TRUE;
					else if(strcmp(token,"FALSE")==0)
						preserveLuminance = FALSE;
					else
						return(ERROR_INVALID_PARAMETER);
					break;
		}
		token = strtok(NULL,delimiter);
    }

	if(	(redShift < -255   || redShift > 255)	||
		(greenShift < -255 || greenShift > 255)	||
		(blueShift < -255  || blueShift > 255)	||
		(shiftMode < 0     || shiftMode > 2))
		return(ERROR_INVALID_PARAMETER);

	ColorShift(redShift,greenShift,blueShift,shiftMode,preserveLuminance);

	return(NO_ERROR);
}

/*
	ColorShift()

	Effettua il "viraggio" (shift dei colori) dell'immagine a seconda dei parametri:

	- valore (0-255) per la componente R del colore
	- valore (0-255) per la componente G del colore
	- valore (0-255) per la componente B del colore
	- modo per lo shift ("viraggio")
	- boolean per mantenere la luminosita' originale (vedi note sotto)

	Esempi:
	ColorShift(30, 0, 0, 1, FALSE);		// spostamento a rosso
	ColorShift(-30, 0, 30, 1, FALSE);	// + blu -rosso
	ColorShift(20, 10, -30, 1, FALSE);	// seppia
	ColorShift(100, 0, 0, 1, FALSE);	// test: solo rosso
	ColorShift(0, 0, 100, 1, FALSE);	// test: solo blu
*/
UINT CImage::ColorShift(int redShift,int greenShift,int blueShift,int mode,BOOL preserveLuminance)
{
	// scambia R/B a seconda della libreria
    if(!IsRGBOrder())
	{
        int temp = redShift;
        redShift = blueShift;
        blueShift = temp;
    }
    
    // ottiene dati immagine
    void* pPixels = GetPixels();
    UINT width = GetWidth();
    UINT height = GetHeight();
    UINT bpp = GetBPP();
    UINT stride = GetBytesWidth();
    
    if(!pPixels || width==0 || height==0 || bpp < 24)
	{
		SetLastErrorDescriptionEx("%s(): invalid data",__func__);
		return(GDI_ERROR);
	}
    
    int bytesPerPixel = bpp / 8;
    
    // applica color shift
    for(UINT y = 0; y < height; y++)
	{
        BYTE* row = (BYTE*)pPixels + y * stride;
        
        for(UINT x = 0; x < width; x++)
		{
            BYTE* pixel = row + x * bytesPerPixel;
            
            // salva valori originali se serve preservare luminanza
            int origR = pixel[0];
            int origG = pixel[1];
            int origB = pixel[2];
            
            int newR, newG, newB;
            
            // applica shift in base alla modalita'
            switch (mode)
			{
                case 0: // addizione semplice (wrap-around)
				{
                    newR = (origR + redShift) & 0xFF;
                    newG = (origG + greenShift) & 0xFF;
                    newB = (origB + blueShift) & 0xFF;
                    break;
				}
                case 1: // addizione con clamp (no wrap-around)
				{
                    newR = Clamp(origR + redShift, 0, 255);
                    newG = Clamp(origG + greenShift, 0, 255);
                    newB = Clamp(origB + blueShift, 0, 255);
                    break;
				}
                case 2: // moltiplicazione (scaling)
				{
                    float rFactor = (redShift + 100) / 100.0f;
                    float gFactor = (greenShift + 100) / 100.0f;
                    float bFactor = (blueShift + 100) / 100.0f;
                    
                    newR = Clamp((int)(origR * rFactor), 0, 255);
                    newG = Clamp((int)(origG * gFactor), 0, 255);
                    newB = Clamp((int)(origB * bFactor), 0, 255);
                    break;
				}
                default:
                    newR = origR; newG = origG; newB = origB;
            }

#if 0
// originale buggato -> con TRUE scurisce, con FALSE schiarisce, non mantiene

// "Prendi i nuovi colori e scalali per avere vecchia luminanza"
//newR = newR × (origLum/newLum)
// problema: se newLum piccolo -> divisione grande -> overshoot

           // preserva luminanza se richiesto
            if (preserveLuminance) {
                // calcola luminanza originale e nuova
                int origLum = (origR * 30 + origG * 59 + origB * 11) / 100;
                int newLum = (newR * 30 + newG * 59 + newB * 11) / 100;
                
                if (newLum != 0) {
                    // scala i nuovi colori per mantenere luminanza originale
                    float scale = (float)origLum / newLum;
                    newR = Clamp((int)(newR * scale), 0, 255);
                    newG = Clamp((int)(newG * scale), 0, 255);
                    newB = Clamp((int)(newB * scale), 0, 255);
                }
            }
#else
// correzione, pero' solo matematicamente, produce effetto paradossale -> rompe il viraggio e sposta tutto a grigi
// perche' cerca di mantenere la luminance originale, che dipende dal valore di RGB, quando RGB ha cambiato
// ossia, non si puo' cambiare uno senza cambiare anche l'altro

// "Scala i colori ORIGINALI dello stesso rapporto di cambiamento luminanza"
//newR = origR × (newLum/origLum)
// moltiplica originali per quanto e' cambiata la luminanza totale

/*           if (preserveLuminance) {
                // calcola luminanza originale e nuova
                int origLum = (origR * 30 + origG * 59 + origB * 11) / 100;
                int newLum = (newR * 30 + newG * 59 + newB * 11) / 100;
                
                if (origLum != 0) {  // CAMBIATO: origLum invece di newLum!
                    // scala i nuovi colori per ripristinare luminanza originale
                    float scale = (float)newLum / origLum;  // INVERTITO!
                    // ora scala i valori ORIGINALI, non quelli nuovi
                    newR = Clamp((int)(origR * scale), 0, 255);
                    newG = Clamp((int)(origG * scale), 0, 255);
                    newB = Clamp((int)(origB * scale), 0, 255);
                }
            }
*/

//versione smart-clamp, mantiene il viraggio e luminosita, non appiattisce/scurisce
if(preserveLuminance)
{
    int origLum = (origR*30 + origG*59 + origB*11) / 100;
    int newLum = (newR*30 + newG*59 + newB*11) / 100;
    
    // se nuovo pixel e' piu' luminoso, riduce proporzionalmente
    if (newLum > origLum) {
        float reduce = (float)origLum / newLum;
        newR = (int)(origR + (newR - origR) * reduce);
        newG = (int)(origG + (newG - origG) * reduce);
        newB = (int)(origB + (newB - origB) * reduce);
    }
}

#endif
            // applica nuovi valori
            pixel[0] = (BYTE)newR;
            pixel[1] = (BYTE)newG;
            pixel[2] = (BYTE)newB;
            
            // il caanale Alpha (se presente) rimane inalterato
        }
    }
    
    return(NO_ERROR);
}

// Helper per clamp
int CImage::Clamp(int value, int minVal, int maxVal)
{
    if(value < minVal)
		return minVal;
    if(value > maxVal)
		return maxVal;
    return value;
}

/*
	Contrast()

	Imposta il contrasto dell'immagine a seconda del parametro:

    -100 = contrasto minimo (tutto grigio)
       0 = nessuna modifica  
    +100 = contrasto massimo
*/
UINT CImage::Contrast(void)
{
	// ricava il valore del parametro    
    int nFactor = 0;
    double nValue = 0;
    if(GetFilterParams("Contrast",nValue))
        nFactor = (int)nValue;
	else    
	{
		SetLastErrorDescriptionEx("%s(): ERROR_INVALID_PARAMETER",__func__);
		return(ERROR_INVALID_PARAMETER);
	}

    if(nFactor!=0)
    {
        int width = GetWidth();
        int height = GetHeight();
        
        // converte a fattore moltiplicativo
        float contrastFactor = (100.0f + nFactor) / 100.0f;
        
        for(int y = 0; y < height; y++)
        {
            for(int x = 0; x < width; x++)
            {
                COLORREF c = GetPixel(x, y);
                
                // formula contrasto standard: (valore - 128) * fattore + 128
                int r = (int)((GetRValue(c) - 128) * contrastFactor + 128);
                int g = (int)((GetGValue(c) - 128) * contrastFactor + 128);
                int b = (int)((GetBValue(c) - 128) * contrastFactor + 128);
                
                // clamping
                r = min(max(r, 0), 255);
                g = min(max(g, 0), 255);
                b = min(max(b, 0), 255);
                
                c = RGB(r, g, b);
                SetPixel(x, y, c);
            }
        }
    }
    
    return(NO_ERROR);
}

/*
	Echo()

	Wrapper per ricavare ed impostare i parametri.
*/
UINT CImage::Echo(void)
{
	// ricava i valori dei parametri
    char szValues[32] = {0};
    double nValue = 0;
    if(!GetFilterParams("Echo",nValue,szValues,sizeof(szValues)))
	{
		SetLastErrorDescriptionEx("%s(): ERROR_INVALID_PARAMETER",__func__);
		return(ERROR_INVALID_PARAMETER);
	}

	int numLayers = 0;
	int zoomPercent = 0;
	int opacityPercent = 0;
	BOOL applyBlur = FALSE;

	const char* delimiter = ";";
    char* token = strtok(szValues,delimiter);

    for(int i=0; token!=NULL; i++)
	{
		switch(i) {
			case 0:	numLayers = atoi(token);      break;
			case 1:	zoomPercent = atoi(token);    break;
			case 2:	opacityPercent = atoi(token); break;
			case 3:	if(strcmp(token,"TRUE")==0)
						applyBlur = TRUE;
					else if(strcmp(token,"FALSE")==0)
						applyBlur = FALSE;
					else
						return(ERROR_INVALID_PARAMETER);
					break;
		}
		token = strtok(NULL,delimiter);
    }

	if(	(numLayers < 0 || numLayers > 10)			||
		(zoomPercent < 0 || zoomPercent > 100)		||
		(opacityPercent < 0 || opacityPercent > 100))
		return(ERROR_INVALID_PARAMETER);

	if(Echo(numLayers,zoomPercent,opacityPercent,applyBlur)!=NO_ERROR)
	{
		//SetLastErrorDescriptionEx("%s(): memory allocation failed",__func__);
        return(GDI_ERROR);
	}
	
	return(NO_ERROR);
}

/*
	Echo()

	Produce un effetto "eco" zoomato e concentrico a seconda dei parametri:

	- numero di layer con effetto eco
	- % per il zoom
	- % per l'opacita' del layer
	- boolean per applicare o meno il filtro Blur
*/
UINT CImage::Echo(int numLayers,int zoomPercent,int opacityPercent,BOOL applyBlur)
{
    // salva l'originale come HDIB
    HDIB hOriginalDIB = GetDIB();
    if(!hOriginalDIB)
		return(GDI_ERROR);
    
    UINT origWidth = GetWidth();
    UINT origHeight = GetHeight();
    
    // per ogni layer
    for(int i = 1; i <= numLayers; i++)
	{
        float scale = 1.0f + (zoomPercent / 100.0f * i);
        float alpha = (opacityPercent / 100.0f) / i;
        
        // crea un nuovo oggetto immagine per il layer
        CImage* pLayer = CreateImage();
        if(!pLayer)
		{
            ::GlobalFree(hOriginalDIB);
			return(GDI_ERROR);
        }
        
        // carica il DIB originale nel layer
        if(!pLayer->SetDIB(hOriginalDIB,-1))
		{
			delete pLayer;
            ::GlobalFree(hOriginalDIB);
			return(GDI_ERROR);
        }
        
        // applica lo zoom usando Stretch()
        RECT rcNew;
        rcNew.left = 0L;
        rcNew.top = 0L;
        rcNew.right = (LONG)(origWidth * scale);
        rcNew.bottom = (LONG)(origHeight * scale);
        
        if(pLayer->Stretch(rcNew,TRUE)!=NO_ERROR) // TRUE = mantiene aspect ratio
		{
			delete pLayer;
            ::GlobalFree(hOriginalDIB);
			return(GDI_ERROR);
        }
        
        // applica blur
        if(applyBlur)
			pLayer->Blur();
        
        // calcola l'offset per centrare
        int layerWidth = pLayer->GetWidth();
        int layerHeight = pLayer->GetHeight();
        int offsetX = ((int)origWidth - layerWidth) / 2;
        int offsetY = ((int)origHeight - layerHeight) / 2;
        
        // fa il blend del layer sull'immagine corrente
        int startY = max(0, offsetY);
        int endY = min((int)origHeight, offsetY + layerHeight);
        int startX = max(0, offsetX);
        int endX = min((int)origWidth, offsetX + layerWidth);
        
        for(int y = startY; y < endY; y++)
		{
            for(int x = startX; x < endX; x++)
			{
                int layerX = x - offsetX;
                int layerY = y - offsetY;
                
                COLORREF layerColor = pLayer->GetPixel(layerX, layerY);
                COLORREF currentColor = GetPixel(x, y);
                COLORREF blended = BlendColors(currentColor,layerColor,alpha);
                SetPixel(x,y,blended);
            }
        }
        
        delete pLayer;
    }
    
	::GlobalFree(hOriginalDIB);

    return(NO_ERROR);
}

/*
	BlendColors()

	Helper per Echo().
*/
COLORREF CImage::BlendColors(COLORREF color1,COLORREF color2,float alpha)
{
    int r1 = GetRValue(color1);
    int g1 = GetGValue(color1);
    int b1 = GetBValue(color1);
    
    int r2 = GetRValue(color2);
    int g2 = GetGValue(color2);
    int b2 = GetBValue(color2);
    
    int r = (int)(r1 * (1.0f - alpha) + r2 * alpha);
    int g = (int)(g1 * (1.0f - alpha) + g2 * alpha);
    int b = (int)(b1 * (1.0f - alpha) + b2 * alpha);
    
    return RGB(r,g,b);
}

/*
	Equalize()
*/
UINT CImage::Equalize(void)
{
	// ricava i valori dei parametri
	char szValues[32] = {0};
	double nValue = 0;
	if(!GetFilterParams("Equalize",nValue,szValues,sizeof(szValues)))
	{
		SetLastErrorDescriptionEx("%s(): ERROR_INVALID_PARAMETER",__func__);
		return(ERROR_INVALID_PARAMETER);
	}

	int eqType = 0;
	float eqIntensity = 0.0f;

	const char* delimiter = ";";
    char* token = strtok(szValues,delimiter);

    for(int i=0; token!=NULL; i++)
	{
		switch(i) {
			case 0:	eqType		= atoi(token);	break;
			case 1:	eqIntensity	= (float)atof(token);	break;
		}
		token = strtok(NULL,delimiter);
    }

	if(	(eqType < 0 || eqType > 2) || (eqIntensity < 0.0 || eqIntensity > 1.0))
		return(ERROR_INVALID_PARAMETER);

	switch(eqType) {
		case 0:  return(EqualizeGHE());
		case 1:  return(EqualizeRGB());
		case 2:  return(EqualizeRGBIntensity(eqIntensity));
		default:
			SetLastErrorDescriptionEx("%s(): unknown equalize method",__func__);
			return(GDI_ERROR);
	}
}

/*
    EqualizeGHE()
	
	Global Histogram Equalization (GHE) on Luminance Channel
    Equalizzazione dell'istogramma applicata alla luminosita' (L in HSL).
    Mantiene hue e saturazione originali, equalizzando solo la luminosita'.
    Migliora il contrasto globale rivelando dettagli in ombre e luci.
    Particolarmente efficace per immagini sottosposte o sovraesposte.
    Nessun parametro: l'equalizzazione e' automatica basata sull'istogramma.
	Converte RGB a HSL/HSV o a luminosita' (Y).
	Equalizza solo il canale di luminosita'.
	Riconverte mantenendo hue/saturazione originali.
	Risultato atteso:
	immagini scure: dettagli nelle ombre diventano visibili
	immagini chiare: dettagli nelle luci vengono recuperati
	immagini contrastate: contrasto migliorato globalmente
	immagini già bilanciate: cambiamento minimo, naturale
*/
UINT CImage::EqualizeGHE(void)
{
    // verifica il formato
    UINT bpp = GetBPP();
    if (bpp < 24)
	{
		SetLastErrorDescriptionEx("%s(): at least 24 BPP required",__func__);
        return(ERROR_WRONG_BPP_FORMAT);
    }

    void* pPixels = GetPixels();
    UINT width = GetWidth();
    UINT height = GetHeight();
    UINT stride = GetBytesWidth();
    
    if(!pPixels || width==0 || height==0)
	{
		SetLastErrorDescriptionEx("%s(): invalid image data",__func__);
        return(GDI_ERROR);
	}
    
    int dibOrder = GetDIBOrder();
    bool topDown = (dibOrder==1);
    
    int bytesPerPixel = bpp / 8;
    bool hasAlpha = (bytesPerPixel==4);
    
    // FASE 1: calcola istogramma della luminosita'
    // --------------------------------------------
    int hist[256] = {0};  // Istogramma luminosità (0-255)
    int totalPixels = width * height;
    
    // prima passata: calcola luminosita' e riempie istogramma
    for (UINT y = 0; y < height; y++)
    {
        BYTE* line = (BYTE*)pPixels + y * stride;
        
        for (UINT x = 0; x < width; x++)
        {
            int idx = x * bytesPerPixel;
            
            // estrae RGB
            BYTE r = line[idx + 2];
            BYTE g = line[idx + 1];
            BYTE b = line[idx + 0];
            
            // calcola luminosita' (formula percezione umana)
            // L = 0.299*R + 0.587*G + 0.114*B
            int luminance = (int)(0.299f * r + 0.587f * g + 0.114f * b);
            
            // clamping
            if (luminance < 0) luminance = 0;
            if (luminance > 255) luminance = 255;
            
            hist[luminance]++;
        }
    }
    
    // FASE 2: calcola funzione di distribuzione cumulativa (CDF)
    // ----------------------------------------------------------
    int cdf[256] = {0};      // CDF
    int cdfMin = INT_MAX;    // valore minimo non-zero della CDF
    int cdfMax = 0;          // valore massimo della CDF (dovrebbe essere totalPixels)
    
    // calcola CDF e trova min/max
    int cumulative = 0;
    for (int i = 0; i < 256; i++)
    {
        cumulative += hist[i];
        cdf[i] = cumulative;
        
        if (hist[i] > 0 && cdf[i] < cdfMin)
            cdfMin = cdf[i];
    }
    cdfMax = cumulative;  // = totalPixels
    
    // FASE 3: crea tabella di mappatura per equalizzazione
    // ----------------------------------------------------
    BYTE map[256]; // tabella di lookup: vecchia luminosita' ? nuova luminosita'
    
    int cdfRange = cdfMax - cdfMin;
    if (cdfRange == 0)
    {
        // caso speciale: tutti i pixel hanno la stessa luminosita'
        for (int i = 0; i < 256; i++)
            map[i] = (BYTE)i;
    }
    else
    {
        // formula equalizzazione: h(v) = round((cdf(v) - cdfMin) * 255 / cdfRange)
        for (int i = 0; i < 256; i++)
        {
            if (hist[i] == 0)
            {
                map[i] = (BYTE)i; // valori non presenti rimangono invariati
            }
            else
            {
                int value = (int)(((cdf[i] - cdfMin) * 255.0f) / cdfRange);
                if (value < 0) value = 0;
                if (value > 255) value = 255;
                map[i] = (BYTE)value;
            }
        }
    }
    
    // FASE 4: applica equalizzazione mantenendo hue/saturazione
    // ---------------------------------------------------------
    for (UINT y = 0; y < height; y++)
    {
        BYTE* line = (BYTE*)pPixels + y * stride;
        
        for (UINT x = 0; x < width; x++)
        {
            int idx = x * bytesPerPixel;
            
            // estrae RGB originale
            BYTE r = line[idx + 2];
            BYTE g = line[idx + 1];
            BYTE b = line[idx + 0];
            
            // converte RGB a HSL
            double H, S, L;
            RGBtoHSL(RGB(r, g, b), &H, &S, &L);
            
            // converte L (0-1) a valore 0-255
            int oldLuminance = (int)(L * 255.0);
            if (oldLuminance < 0) oldLuminance = 0;
            if (oldLuminance > 255) oldLuminance = 255;
            
            // applica mappatura equalizzata alla luminosita'
            int newLuminance = map[oldLuminance];
            
            // converte nuova luminosita' (0-255) a L (0-1)
            double newL = newLuminance / 255.0;
            
            // converte HSL nuovo a RGB
            COLORREF newColor = HLStoRGB(H, newL, S);
            
            // applica nuovo colore
            line[idx + 2] = GetRValue(newColor);  // Red
            line[idx + 1] = GetGValue(newColor);  // Green  
            line[idx + 0] = GetBValue(newColor);  // Blue
            // alpha rimane invariato
        }
    }
    
    return(NO_ERROR);
}

/*
    EqualizeRGB()

    Equalizzazione dell'istogramma applicata separatamente ai canali R, G, B.
    Produce cambi di colore drammatici con effetti artistici forti.
    
    Ogni canale di colore viene equalizzato indipendentemente, cambiando
    i rapporti tra R:G:B e creando nuove combinazioni cromatiche.
    
    Ideale per effetti artistici, reinterpretazioni creative, wallpaper astratti.
    
    Nessun parametro: l'equalizzazione e' automatica basata sugli istogrammi.
*/
UINT CImage::EqualizeRGB(void)
{
    // 1. Verifica formato immagine
    UINT bpp = GetBPP();
    if (bpp < 24)
	{
		SetLastErrorDescriptionEx("%s(): at least 24 BPP required",__func__);
        return(ERROR_WRONG_BPP_FORMAT);
    }

    void* pPixels = GetPixels();
    UINT width = GetWidth();
    UINT height = GetHeight();
    UINT stride = GetBytesWidth();
    
    if (!pPixels || width == 0 || height == 0)
        return(GDI_ERROR);
    
    int dibOrder = GetDIBOrder();
    bool topDown = (dibOrder==1);
    
    int bytesPerPixel = bpp / 8;
    bool hasAlpha = (bytesPerPixel == 4);

    // FASE 1: calcola istogrammi separati per R, G, B
    // -----------------------------------------------
    int histR[256] = {0};
    int histG[256] = {0};
    int histB[256] = {0};
    
    int totalPixels = width * height;
    
    // prima passata: calcola istogrammi
    for (UINT y = 0; y < height; y++)
    {
        BYTE* line = (BYTE*)pPixels + y * stride;
        
        for (UINT x = 0; x < width; x++)
        {
            int idx = x * bytesPerPixel;
            
            histB[line[idx + 0]]++;  // Blue
            histG[line[idx + 1]]++;  // Green
            histR[line[idx + 2]]++;  // Red
        }
    }
    
    // FASE 2: calcola CDF per ciascun canale
    // --------------------------------------
    int cdfR[256] = {0}, cdfG[256] = {0}, cdfB[256] = {0};
    int cdfMinR = INT_MAX, cdfMinG = INT_MAX, cdfMinB = INT_MAX;
    int cdfMaxR = 0, cdfMaxG = 0, cdfMaxB = 0;
    
    // Red
    int cumulative = 0;
    for (int i = 0; i < 256; i++)
    {
        cumulative += histR[i];
        cdfR[i] = cumulative;
        
        if (histR[i] > 0 && cdfR[i] < cdfMinR)
            cdfMinR = cdfR[i];
    }
    cdfMaxR = cumulative;
    
    // Green
    cumulative = 0;
    for (int i = 0; i < 256; i++)
    {
        cumulative += histG[i];
        cdfG[i] = cumulative;
        
        if (histG[i] > 0 && cdfG[i] < cdfMinG)
            cdfMinG = cdfG[i];
    }
    cdfMaxG = cumulative;
    
    // Blue
    cumulative = 0;
    for (int i = 0; i < 256; i++)
    {
        cumulative += histB[i];
        cdfB[i] = cumulative;
        
        if (histB[i] > 0 && cdfB[i] < cdfMinB)
            cdfMinB = cdfB[i];
    }
    cdfMaxB = cumulative;
    
    // FASE 3: crea tabelle di mappatura per ciascun canale
    // ----------------------------------------------------
    BYTE mapR[256], mapG[256], mapB[256];
    
    // Red
    int rangeR = cdfMaxR - cdfMinR;
    if (rangeR == 0)
    {
        for (int i = 0; i < 256; i++) mapR[i] = (BYTE)i;
    }
    else
    {
        for (int i = 0; i < 256; i++)
        {
            if (histR[i] == 0)
                mapR[i] = (BYTE)i;
            else
            {
                int value = (int)(((cdfR[i] - cdfMinR) * 255.0f) / rangeR);
                if (value < 0) value = 0;
                if (value > 255) value = 255;
                mapR[i] = (BYTE)value;
            }
        }
    }
    
    // Green
    int rangeG = cdfMaxG - cdfMinG;
    if (rangeG == 0)
    {
        for (int i = 0; i < 256; i++) mapG[i] = (BYTE)i;
    }
    else
    {
        for (int i = 0; i < 256; i++)
        {
            if (histG[i] == 0)
                mapG[i] = (BYTE)i;
            else
            {
                int value = (int)(((cdfG[i] - cdfMinG) * 255.0f) / rangeG);
                if (value < 0) value = 0;
                if (value > 255) value = 255;
                mapG[i] = (BYTE)value;
            }
        }
    }
    
    // Blue
    int rangeB = cdfMaxB - cdfMinB;
    if (rangeB == 0)
    {
        for (int i = 0; i < 256; i++) mapB[i] = (BYTE)i;
    }
    else
    {
        for (int i = 0; i < 256; i++)
        {
            if (histB[i] == 0)
                mapB[i] = (BYTE)i;
            else
            {
                int value = (int)(((cdfB[i] - cdfMinB) * 255.0f) / rangeB);
                if (value < 0) value = 0;
                if (value > 255) value = 255;
                mapB[i] = (BYTE)value;
            }
        }
    }
    
    // FASE 4: applica mappatura a ciascun canale
    // ------------------------------------------
    for (UINT y = 0; y < height; y++)
    {
        BYTE* line = (BYTE*)pPixels + y * stride;
        
        for (UINT x = 0; x < width; x++)
        {
            int idx = x * bytesPerPixel;
            
            // applica mappatura a ciascun canale
            line[idx + 0] = mapB[line[idx + 0]];  // Blue
            line[idx + 1] = mapG[line[idx + 1]];  // Green
            line[idx + 2] = mapR[line[idx + 2]];  // Red
            // alpha rimane invariato (se presente)
        }
    }
    
    return(NO_ERROR);
}

/*
    EqualizeRGBIntensity()

    Equalizzazione RGB separata con intensità regolabile.
    
    Parametro: intensity (0.0 - 1.0 in virgola mobile)
        0.0  = nessun effetto (immagine originale)
        0.5  = metà dell'effetto equalizzato
        1.0  = equalizzazione RGB completa
    
    Combina l'originale con l'equalizzato in proporzione all'intensità.
    Permette un controllo fine sull'effetto artistico.
*/
UINT CImage::EqualizeRGBIntensity(double intensity)
{
    if(intensity < 0.0 || intensity > 1.0)
	{
		SetLastErrorDescriptionEx("%s(): ERROR_INVALID_PARAMETER",__func__);
		return(ERROR_INVALID_PARAMETER);
	}
    
    if (intensity < 0.001)
        return(NO_ERROR);
    
    UINT bpp = GetBPP();
    if (bpp < 24)
	{
		SetLastErrorDescriptionEx("%s(): at least 24 BPP required",__func__);
        return(ERROR_WRONG_BPP_FORMAT);
    }
    
    void* pPixels = GetPixels();
    UINT width = GetWidth();
    UINT height = GetHeight();
    UINT stride = GetBytesWidth();
    
    if (!pPixels || width == 0 || height == 0)
        return(GDI_ERROR);
    
    int dibOrder = GetDIBOrder();
    bool topDown = (dibOrder == 1);
    
    int bytesPerPixel = bpp / 8;
    bool hasAlpha = (bytesPerPixel == 4);
    
    // crea copia dei dati ORIGINALI per il blend
    UINT bufferSize = stride * height;
    std::vector<BYTE> originalBuffer(bufferSize);
    memcpy(originalBuffer.data(), pPixels, bufferSize);
    
    // FASE 1: calcola istogrammi dai dati originali
    // ---------------------------------------------
    int histR[256] = {0};
    int histG[256] = {0};
    int histB[256] = {0};
    
    int totalPixels = width * height;
    
    // calcola istogrammi dall'originale
    for (UINT y = 0; y < height; y++)
    {
        const BYTE* line = originalBuffer.data() + y * stride;
        
        for (UINT x = 0; x < width; x++)
        {
            int idx = x * bytesPerPixel;
            
            histB[line[idx + 0]]++;  // Blue
            histG[line[idx + 1]]++;  // Green
            histR[line[idx + 2]]++;  // Red
        }
    }
    
    // FASE 2: calcola CDF e tabelle di mappatura
    // ------------------------------------------
    BYTE mapR[256], mapG[256], mapB[256];
    
    // inizializza mappe con valori identita' (caso base)
    for (int i = 0; i < 256; i++)
    {
        mapR[i] = (BYTE)i;
        mapG[i] = (BYTE)i;
        mapB[i] = (BYTE)i;
    }
    
    // Red
    int cdfR[256] = {0};
    int cdfMinR = INT_MAX;
    int cumulative = 0;
    
    for (int i = 0; i < 256; i++)
    {
        cumulative += histR[i];
        cdfR[i] = cumulative;
        
        if (histR[i] > 0 && cdfR[i] < cdfMinR)
            cdfMinR = cdfR[i];
    }
    
    int cdfMaxR = cumulative;
    int rangeR = cdfMaxR - cdfMinR;
    
    if(rangeR > 0)
    {
        for(int i = 0; i < 256; i++)
        {
            if(histR[i] > 0)
            {
                int value = (int)(((cdfR[i] - cdfMinR) * 255.0f) / rangeR);
                if (value < 0) value = 0;
                if (value > 255) value = 255;
                mapR[i] = (BYTE)value;
            }
        }
    }
    
    // Green
    int cdfG[256] = {0};
    int cdfMinG = INT_MAX;
    cumulative = 0;
    
    for(int i = 0; i < 256; i++)
    {
        cumulative += histG[i];
        cdfG[i] = cumulative;
        
        if(histG[i] > 0 && cdfG[i] < cdfMinG)
            cdfMinG = cdfG[i];
    }
    
    int cdfMaxG = cumulative;
    int rangeG = cdfMaxG - cdfMinG;
    
    if(rangeG > 0)
    {
        for(int i = 0; i < 256; i++)
        {
            if(histG[i] > 0)
            {
                int value = (int)(((cdfG[i] - cdfMinG) * 255.0f) / rangeG);
                if (value < 0) value = 0;
                if (value > 255) value = 255;
                mapG[i] = (BYTE)value;
            }
        }
    }
    
    // Blue
    int cdfB[256] = {0};
    int cdfMinB = INT_MAX;
    cumulative = 0;
    
    for(int i = 0; i < 256; i++)
    {
        cumulative += histB[i];
        cdfB[i] = cumulative;
        
        if(histB[i] > 0 && cdfB[i] < cdfMinB)
            cdfMinB = cdfB[i];
    }
    
    int cdfMaxB = cumulative;
    int rangeB = cdfMaxB - cdfMinB;
    
    if(rangeB > 0)
    {
        for(int i = 0; i < 256; i++)
        {
            if(histB[i] > 0)
            {
                int value = (int)(((cdfB[i] - cdfMinB) * 255.0f) / rangeB);
                if (value < 0) value = 0;
                if (value > 255) value = 255;
                mapB[i] = (BYTE)value;
            }
        }
    }
    
    // FASE 3: applica blend tra originale e equalizzato
    // -------------------------------------------------
    float intensityF = (float)intensity;
    
    // caso ottimizzato per intensità 1.0 (equalizzazione completa)
    if(intensityF >= 0.999f)
    {
        for(UINT y = 0; y < height; y++)
        {
            BYTE* dstLine = (BYTE*)pPixels + y * stride;
            const BYTE* srcLine = originalBuffer.data() + y * stride;
            
            for(UINT x = 0; x < width; x++)
            {
                int idx = x * bytesPerPixel;
                
                // Leggi valori originali
                BYTE origB = srcLine[idx + 0];
                BYTE origG = srcLine[idx + 1];
                BYTE origR = srcLine[idx + 2];
                
                // Applica mappatura completa
                dstLine[idx + 0] = mapB[origB];  // Blue
                dstLine[idx + 1] = mapG[origG];  // Green
                dstLine[idx + 2] = mapR[origR];  // Red
            }
        }
    }
    else
    {
        // blend lineare per intensita' < 1.0
        for(UINT y = 0; y < height; y++)
        {
            BYTE* dstLine = (BYTE*)pPixels + y * stride;
            const BYTE* srcLine = originalBuffer.data() + y * stride;
            
            for(UINT x = 0; x < width; x++)
            {
                int idx = x * bytesPerPixel;
                
                // legge valori originali
                BYTE origB = srcLine[idx + 0];
                BYTE origG = srcLine[idx + 1];
                BYTE origR = srcLine[idx + 2];
                
                // calcola valori equalizzati
                BYTE eqB = mapB[origB];
                BYTE eqG = mapG[origG];
                BYTE eqR = mapR[origR];
                
                // blend lineare: original * (1-intensity) + equalized * intensity
                dstLine[idx + 0] = (BYTE)(origB * (1.0f - intensityF) + eqB * intensityF);  // Blue
                dstLine[idx + 1] = (BYTE)(origG * (1.0f - intensityF) + eqG * intensityF);  // Green
                dstLine[idx + 2] = (BYTE)(origR * (1.0f - intensityF) + eqR * intensityF);  // Red
                
                // alpha rimane invariato (copiato dall'originale)
                if(hasAlpha)
                    dstLine[idx + 3] = srcLine[idx + 3];
            }
        }
    }
    
    return(NO_ERROR);
}

/*
	GammaCorrection()

	Correzione Gamma a seconda del parametro (0.0-10.0):
	
	nValue   | Output    | Descrizione
	---------|-----------|-------------------
	0.1      | 251       | Quasi bianco
	0.2      | 244       | Bianchissimo
	0.3      | 235       | Molto chiaro
	0.4      | 224       | Chiaro
	0.5      | 211       | Grigio chiaro
	1.0      | 128       | Grigio medio (invariato)
	2.0      | 64        | Grigio scuro
	3.0      | 32        | Molto scuro
	4.0      | 16        | Quasi nero
	5.0      | 8         | Nero
	10.0     | 0.5 -> 0  | Nero puro

	La correzione gamma e' una trasformazione per correggere la relazione tra i valori numerici dei 
	pixel e la luminosita' percepita dall'occhio umano o riprodotta da un dispositivo
	in altre parole, serve a correggere la differenza tra il segnale elettronico che rappresenta un
	immagine e la luce effettivamente emessa o percepita
	
	da una parte, l'occhio umano non risponde in modo lineare alla luce: percepiamo piccole differenze 
	nelle ombre meglio che nelle alte luci, e la nostra percezione della luminosita e' basicamente
	logaritmica
	dall'altra, monitor, TV, proiettori, etc. hanno una risposta non lineare tra il segnale elettrico 
	in ingresso e l'intensita' luminosa emessa e senza correzione, le immagini apparirebbero troppo 
	scure o con contrasto scorretto
	la correzione gamma permette di allocare piu' bit per le ombre (dove l'occhio e' piu' sensibile) 
	e meno per le luci, ottimizzando la rappresentazione digitale
*/
UINT CImage::GammaCorrection(void)
{
    // ricava il valore del parametro (ora un valore gamma double significativo)
    double nValue = 0;
    if(!GetFilterParams("GammaCorrection", nValue))
	{
		SetLastErrorDescriptionEx("%s(): ERROR_INVALID_PARAMETER",__func__);
		return(ERROR_INVALID_PARAMETER);
	}
    
    // validazione del parametro gamma
    if(nValue <= 0.0)
    {
        // Gamma <= 0 e' matematicamente non definito
        // Gamma = 0 renderebbe tutto bianco (tranne il nero puro)
        // per sicurezza, restituisce errore o usa gamma=1
        return(ERROR_INVALID_PARAMETER);
        // oppure: nValue = 1.0; // x tolleranza
    }
    
    // non limitiamo piu' a 10.0 Gamma=15 e' matematicamente valido
    // (anche se renderà l'immagine quasi nera)
    
    // calcoliamo la lookup table in modo numericamente stabile
    unsigned char lut[256];
    
    if(nValue == 1.0)
    {
        // caso ottimizzato: gamma=1, nessuna modifica
        for(int i = 0; i < 256; i++)
            lut[i] = (unsigned char)i;
    }
    else
    {
        // formula numericamente stabile:
        // prima normalizza a [0,1], poi applica gamma, poi denormalizza
        for(int i = 0; i < 256; i++)
        {
            // normalizza a [0,1]
            double normalized = i / 255.0;
            
            // applica gamma correction
            double corrected = pow(normalized, nValue);
            
            // denormalizza a [0,255] e arrotonda
            double val = corrected * 255.0;
            
            // clamping e arrotondamento
            if(val < 0.0) val = 0.0;
            if(val > 255.0) val = 255.0;
            lut[i] = (unsigned char)(val + 0.5);
        }
    }
    
    // applica la trasformazione all'immagine
    RECT r;
    r.top = r.left = 0L;
    r.right = GetWidth();
    r.bottom = GetHeight();
    
    for(int y = r.top; y < r.bottom; y++)
    {
        for(int x = r.left; x < r.right; x++)
        {
            COLORREF c = GetPixel(x,y);
            
            int R = lut[GetRValue(c)];
            int G = lut[GetGValue(c)];
            int B = lut[GetBValue(c)];
            
            SetPixel(x, y, RGB(R, G, B));
        }
    }
    
    return(NO_ERROR);
}

/*
	GhostTrail()

	Wrapper per ricavare ed impostare i parametri.
*/
UINT CImage::GhostTrail(void)
{
	// ricava i valori dei parametri
    char szValues[32] = {0};
    double nValue = 0;
    if(!GetFilterParams("GhostTrail",nValue,szValues,sizeof(szValues)))
	{
		SetLastErrorDescriptionEx("%s(): ERROR_INVALID_PARAMETER",__func__);
		return(ERROR_INVALID_PARAMETER);
	}

	int numGhosts = 0;
	int maxOffset = 0;
	float baseAlpha = 0.0f;
	BOOL chromatic = FALSE;

    const char* delimiter = ";";
    char* token = strtok(szValues,delimiter);

    for(int i=0; token!=NULL; i++)
	{
		switch(i) {
			case 0:	numGhosts = atoi(token); break;
			case 1:	maxOffset = atoi(token); break;
			case 2:	baseAlpha = (float)atof(token); break;
			case 3:	if(strcmp(token,"TRUE")==0)
						chromatic = TRUE;
					else if(strcmp(token,"FALSE")==0)
						chromatic = FALSE;
					else
						return(ERROR_INVALID_PARAMETER);
					break;
		}
		token = strtok(NULL,delimiter);
    }

	if(	(numGhosts < 1 || numGhosts > 10)		||
		(maxOffset < 1 || maxOffset > 50)		||
		(baseAlpha < 0.0f || baseAlpha > 1.0f)	)
		return(ERROR_INVALID_PARAMETER);

	GhostTrail(numGhosts,maxOffset,baseAlpha,chromatic);
	
	return(NO_ERROR);
}

/*
	GhostTrail()

	Produce un effetto "fantasma" con "auree" intorno all'immagine a seconda dei parametri:

	- numero di fantasmi (auree)
	- dimensione dello scostamento dell'aura dall'immagine
	- fattore per la trasparenza dell'aura
	- se l'aura deve essere con colori originali o RGB

	Esempi:
	effetto ghosting/multiple exposure:
	GhostTrail(3, 8, 0.4f, FALSE);  // 3 ghost, offset max 8px, alpha 0.4
	GhostTrail(3, 18, 0.9f, TRUE);  // 3 ghost, offset max 8px, alpha 0.4

	effetto chromatic aberration:
	GhostTrail(3, 5, 0.6f, TRUE);   // R/G/B separati
	GhostTrail(3, 15, 0.9f, TRUE);   // R/G/B separati
*/
UINT CImage::GhostTrail(int numGhosts,int maxOffset,float baseAlpha,BOOL chromatic)
{
	if(GetBPP() < 24)
	{
		SetLastErrorDescriptionEx("%s(): at least 24 BPP required",__func__);
		return(ERROR_INVALID_PARAMETER);
	}

	if(numGhosts==0 && maxOffset==0 && baseAlpha==0.0f)
        return(NO_ERROR);
    
    void* pPixels = GetPixels();
    UINT width = GetWidth();
    UINT height = GetHeight();
    UINT bpp = GetBPP();
    UINT stride = GetBytesWidth();
    
    if(!pPixels || width==0 || height==0)
	{
		SetLastErrorDescriptionEx("%s(): invalid image data",__func__);
        return(GDI_ERROR);
	}
    
    // crea una copia dei dati originali
    UINT bufferSize = stride * height;
    std::vector<BYTE> originalBuffer(bufferSize);
    memcpy(originalBuffer.data(), pPixels, bufferSize);

    // calcola parametri per ogni ghost
    for(int ghost = 1; ghost <= numGhosts; ghost++)
	{
        // offset casuali ma con pattern
        int offsetX, offsetY;
        
        if(chromatic)
		{
            int colorIndex = (ghost - 1) % 3;  // 0=R, 1=G, 2=B
            float angle = 0;
			float distanceMultiplier = 0.0f; 
/*
			Tener sempre presente che l'angolo determina il posizionamento destra/sinistra,
			alto/basso, del layer, cosi' come la funzione mirror con l'orientamento.

			Esempi di angoli (p=pi greco):
			Angolo (gradi)	Angolo (rad)	cos		sin		Direzione
			0°				0.0				1.0		0.0		Destra pura
			45°				p/4				0.707	0.707	Destra-Basso (diagonale)
			90°				p/2				0.0		1.0		In basso puro
			135°			3p/4			-0.707	0.707	Sinistra-Basso
			180°			p				-1.0	0.0		Sinistra pura
			225°			5p/4			-0.707	-0.707	Sinistra-Alto
			270°			3p/2			0.0		-1.0	In alto puro
			315°			7p/4			0.707	-0.707	Destra-Alto
*/
#if 0
			// per un effetto "triangolo" (RGB ai vertici di triangolo):
			switch (colorIndex) {
				case 0:  // Rosso - alto-destra
					angle = 45.0f * (3.14159f / 180.0f);  // 45°
					break;
				case 1:  // Verde - basso-sinistra
					angle = 225.0f * (3.14159f / 180.0f);  // 225° (45° + 180°)
					break;
				case 2:  // Blu - basso-destra  
					angle = 135.0f * (3.14159f / 180.0f);  // 135°
					break;
			}
#endif
#if 0
			// per spostare tutti e 3 i layer in diagonale (stessa direzione, es: destra-basso):
			switch (colorIndex) {
				case 0:  // Rosso
					angle = 45.0f * (3.14159f / 180.0f);  // Destra-Basso
					break;
				case 1:  // Verde
					angle = 45.0f * (3.14159f / 180.0f);  // Stessa direzione
					break;
				case 2:  // Blu  
					angle = 45.0f * (3.14159f / 180.0f);  // Stessa direzione
					break;
			}
#endif
#if 1
			// per chromatic aberration classico (RGB separati ma stessa linea)
			// classico effetto "color fringe" dove R, G, B sono separati ma sulla stessa linea orizzontale
			switch (colorIndex) {
				case 0:  // Rosso
					angle = 0.0f;  // a destra
					distanceMultiplier = 1.0f;
					break;
				case 1:  // Verde
					angle = 0.0f;  // stessa direzione (destra)
					distanceMultiplier = 1.5f;
					break;
				case 2:  // Blu
					angle = 0.0f;  // stessa direzione (destra)
					//angle = 180.0f * (3.14159f / 180.0f);  // a sinistra (lato opposto)
					distanceMultiplier = 2.0f;
					break;
			}
#endif
#if 0
			// originale, in cerchio
            switch (colorIndex) {
                case 0:  // Rosso
                    angle = 0.0f;  // Destra
                    break;
                case 1:  // Verde
                    angle = 90.0f * (3.14159f / 180.0f);  // In basso
                    break;
                case 2:  // Blu
                    angle = 180.0f * (3.14159f / 180.0f);  // Sinistra
                    break;
            }
#endif
            
            // aumenta offset per ghost successivi dello stesso colore
            int colorGroup = (ghost - 1) / 3;  // Gruppo di 3 (R,G,B)
			float distance = (float)(maxOffset * (colorGroup + 1)) / ((numGhosts + 2) / 3);

			// multipñicatore per la distanza, per separare i layer proporzionalmente
			float actualDistance = 0.0f;
			if(distanceMultiplier!=0.0f)
				actualDistance = distance * distanceMultiplier;
			else
				actualDistance = distance;

            offsetX = (int)(cos(angle) * actualDistance/*distance*/);
            offsetY = (int)(sin(angle) * actualDistance/*distance*/);
        }
		else
		{
            // offset alternati: destra/sinistra, alto/basso
            if(ghost % 2 == 0)
			{
                offsetX = (maxOffset * ghost) / numGhosts;
                offsetY = (maxOffset * ghost) / (2 * numGhosts);
            }
			else
			{
                offsetX = -(maxOffset * ghost) / numGhosts;
                offsetY = -(maxOffset * ghost) / (2 * numGhosts);
            }
        }
        
        // alpha decrescente per ghost successivi, più forte per chromatic
        float alpha = chromatic ? (baseAlpha * 0.8f) : (baseAlpha / ghost);
        
        // applica il ghost
        Ghosterize((BYTE*)pPixels,originalBuffer.data(),width,height,stride,bpp,offsetX,offsetY,alpha,chromatic,ghost);
    }
    
    return(TRUE);
}

/*
	Ghosterize()

	Helper per GhostTrail().
*/
void CImage::Ghosterize(BYTE* dst,const BYTE* src,UINT width,UINT height,UINT stride,UINT bpp,int offsetX,int offsetY,float alpha,BOOL chromatic,int ghostIndex)
{
    int bytesPerPixel = bpp / 8;
    
    for(UINT y = 0; y < height; y++)
	{
        int srcY = y - offsetY;
        
        if(srcY >= 0 && srcY < (int)height)
		{
            BYTE* dstLine = dst + y * stride;
            const BYTE* srcLine = src + srcY * stride;
            
            for(UINT x = 0; x < width; x++)
			{
                int srcX = x - offsetX;
                
                if(srcX >= 0 && srcX < (int)width)
				{
                    int dstIdx = x * bytesPerPixel;
                    int srcIdx = srcX * bytesPerPixel;
                    
                    if(chromatic)
					{
                        // ogni ghost modifica TUTTI i canali, ma con enfasi su uno
                        int primaryChannel = (ghostIndex - 1) % 3;
                        
                        for(int c = 0; c < 3; c++)
						{
                            float channelAlpha = alpha;
                            
                            // canale primario: alpha pieno
                            // canali secondari: alpha ridotto
                            if(c != primaryChannel)
                                channelAlpha = alpha * 0.3f;  // effetto minore
                            
                            dstLine[dstIdx + c] = (BYTE)(
                                dstLine[dstIdx + c] * (1.0f - channelAlpha) +
                                srcLine[srcIdx + c] * channelAlpha
                            );
                        }
                        
                        // per canale Alpha (se presente)
                        if(bytesPerPixel==4)
						{
                            // alpha molto poco influenzato
                            float alphaAlpha = alpha * 0.1f;
                            dstLine[dstIdx + 3] = (BYTE)(
                                dstLine[dstIdx + 3] * (1.0f - alphaAlpha) +
                                srcLine[srcIdx + 3] * alphaAlpha
                            );
                        }
                    }
					else
					{
                        // ghost normale: tutti i canali
                        for(int c = 0; c < 3; c++)
						{
                            dstLine[dstIdx + c] = (BYTE)(
                                dstLine[dstIdx + c] * (1.0f - alpha) +
                                srcLine[srcIdx + c] * alpha
                            );
                        }
                        
                        // gestione Alpha channel per 32bpp
                        if(bytesPerPixel==4)
						{
                            // alpha meno influenzato
                            float alphaAlpha = alpha * 0.3f;
                            dstLine[dstIdx + 3] = (BYTE)(
                                dstLine[dstIdx + 3] * (1.0f - alphaAlpha) +
                                srcLine[srcIdx + 3] * alphaAlpha
                            );
                        }
                    }
                }
            }
        }
    }
}

/*
	Grain()

	Imposta la granulosita' dell'immagine a seconda del parametro.

	Aggiunge lo stesso valore casuale a (piu' o meno) tutti i canali, cambiando cambia la luminosita', 
	non i colori, e dando come risultato un rumore granuloso (come pellicola fotografica ad alto ISO).
	I canali verde e blu hanno variazioni ridotte/opposte -> leggera alterazione cromatica.
*/
UINT CImage::Grain(void)
{
	// ricava il valore del parametro
    int nFactor = 0;
    double nValue = 0;
    if(GetFilterParams("Grain",nValue))
        nFactor = (int)nValue;
	else
	{
		SetLastErrorDescriptionEx("%s(): ERROR_INVALID_PARAMETER",__func__);
		return(ERROR_INVALID_PARAMETER);
	}
    
    int width = GetWidth();
    int height = GetHeight();
    
    // converte l'intensita' 0-100 in un range di drift sensato
    // nIntensity = 0   -> maxDrift = 0
    // nIntensity = 50  -> maxDrift = 64
    // nIntensity = 100 -> maxDrift = 128
    int maxDrift = (nFactor * 128) / 100;
    
    srand((unsigned int)time(NULL));
    
    for(int y = 0; y < height; y++)
    {
        for(int x = 0; x < width; x++)
        {
            COLORREF c = GetPixel(x, y);
            int r = GetRValue(c);
            int g = GetGValue(c);
            int b = GetBValue(c);
            
            // drift correlato per mantenere coerenza
            int baseDrift = (rand_m() % (maxDrift * 2)) - maxDrift;
            
            // variazioni leggermente diverse per ogni canale
            // per creare anche una leggera alterazione cromatica
            int driftR = baseDrift;
            //int driftG = baseDrift + (rand() % 7) - 3;
            //int driftB = baseDrift + (rand() % 7) - 3;
            int driftG = baseDrift + fast_rand_range(7) - 3;
            int driftB = baseDrift + fast_rand_range(7) - 3;

            int newR = r + driftR;
            int newG = g + driftG;
            int newB = b + driftB;
            
            // clamping piu' efficiente
            if(newR < 0) newR = 0;
            if(newG < 0) newG = 0;
            if(newB < 0) newB = 0;
            
            if(newR > 255) newR = 255;
            if(newG > 255) newG = 255;
            if(newB > 255) newB = 255;
            
            SetPixel(x,y,RGB(newR,newG,newB));
        }
    }
    
    return(NO_ERROR);
}

/*
	HalftoneBW()

	Applica il filtro halftone (o mezzatinta) all'immagine.

	Halftone e' una tecnica che simula una gradazione di toni continui usando solo punti di dimensioni 
	variabili o distanze diverse.
	E' un inganno visivo che sfrutta la limitata risoluzione dell'occhio umano per percepire sfumature 
	che in realta' non esistono.
	Ad es., se si dovesse disegnare una foto in bianco e nero usando solo una penna nera, ossia senza
	poter usare colori grigi, il trucco consistirebbe in disegnare le zone chiare usando punti piccoli e 
	distanziati e le zone scure usando punti grandi e ravvicinati.
	L'occhio, da lontano, mescolerebbe i punti neri con lo sfondo bianco ottenendo l'illusione del grigio.
*/
// versione bianco e nero, halftone su luminosita'
// (halftone classico in "scala di grigi")
UINT CImage::HalftoneBW(void)
{
	// solo per immagini a colori (24/32-bit) o grayscale (8-bit)
	if(GetBPP() < 8)
	{
		SetLastErrorDescriptionEx("%s(): at least 8 BPP required",__func__);
		return(ERROR_WRONG_BPP_FORMAT);
	}

    // matrice di Bayer 4x4 (piu' semplice)
    static const int bayerMatrix[4][4] = {
        { 0,  8,  2, 10 },
        {12,  4, 14,  6 },
        { 3, 11,  1,  9 },
        {15,  7, 13,  5 }
    };
    
    // scala la matrice a 0-255
    const int scale = 17; // 15 * 17 = 255

    int width = GetWidth();
    int height = GetHeight();
    
    for(int y = 0; y < height; y++)
    {
        for(int x = 0; x < width; x++)
        {
            COLORREF c = GetPixel(x, y);
            
            // calcola luminosita' (formula standard)
            int brightness = (GetRValue(c) * 299 + GetGValue(c) * 587 + GetBValue(c) * 114) / 1000;
            
            // soglia basata su matrice di Bayer
            int threshold = bayerMatrix[y % 4][x % 4] * scale;
            
            // applica halftone: punto nero o bianco basato su luminosità
            int value = (brightness > threshold) ? 255 : 0;
            
            // crea effetto halftone in scala di grigi
            SetPixel(x, y, RGB(value, value, value));
        }
    }
    
	UpdateHeaderInfo();

    return(NO_ERROR);
}

/*
	HalftoneColor()
*/
#if 1
// versione dithering ordered
// (dithering a colori, mantiene separati R, G, B)
UINT CImage::HalftoneColor(void)
{
	// solo per immagini a colori (24/32-bit) o grayscale (8-bit)
    if(GetBPP() < 8)
	{
		SetLastErrorDescriptionEx("%s(): at least 8 BPP required",__func__);
		return(ERROR_WRONG_BPP_FORMAT);
	}

	// non applica a un immagine che e'/sembra grayscale, restituisce errore
	if(IsTrueGrayscale() || IsVisualGrayscaleStatistical(VISUAL_GRAYSCALE_PARAMS))
	{
		SetLastErrorDescriptionEx("%s(): image is grayscale, filter will not be applied",__func__);
		return(GDI_ERROR);
	}
    
    // matrice di dithering 8x8 (Bayer matrix)
    static const int ditherMatrix[8][8] = {
        { 0, 48, 12, 60,  3, 51, 15, 63 },
        {32, 16, 44, 28, 35, 19, 47, 31 },
        { 8, 56,  4, 52, 11, 59,  7, 55 },
        {40, 24, 36, 20, 43, 27, 39, 23 },
        { 2, 50, 14, 62,  1, 49, 13, 61 },
        {34, 18, 46, 30, 33, 17, 45, 29 },
        {10, 58,  6, 54,  9, 57,  5, 53 },
        {42, 26, 38, 22, 41, 25, 37, 21 }
    };
    
    // soglia di dithering (0-63 scala a 0-255)
    const int scale = 4; // 64 * 4 = 256
    
    int width = GetWidth();
    int height = GetHeight();

    for(int y = 0; y < height; y++)
    {
        for(int x = 0; x < width; x++)
        {
            COLORREF c = GetPixel(x, y);
            
            int r = GetRValue(c);
            int g = GetGValue(c);
            int b = GetBValue(c);
            
            // soglia basata sulla matrice di dithering
            int threshold = ditherMatrix[y % 8][x % 8] * scale;
            
            // applica dithering a ogni canale
            int newR = (r > threshold) ? 255 : 0;
            int newG = (g > threshold) ? 255 : 0;
            int newB = (b > threshold) ? 255 : 0;
            
            // per effetto "newspaper halftone" classico, si potrebbe usare solo 
			// luminosita', ma qui mantiene i canali separati per effetto a colori
            SetPixel(x, y, RGB(newR, newG, newB));
        }
    }
    
    return(NO_ERROR);
}
#else
// versione dithering Floyd-Steinberg
// (dithering a colori, senza GDI)
UINT CImage::HalftoneColor(void)
{
	// solo per immagini a colori (24/32-bit) o grayscale (8-bit)
    if(GetBPP() < 8)
	{
		SetLastErrorDescriptionEx("%s(): at least 8 BPP required",__func__);
		return(ERROR_WRONG_BPP_FORMAT);
	}

	// non applica a un immagine che e'/sembra grayscale, restituisce errore
	if(IsTrueGrayscale() || IsVisualGrayscaleStatistical(VISUAL_GRAYSCALE_PARAMS))
	{
		SetLastErrorDescriptionEx("%s(): image is grayscale, filter will not be applied",__func__);
		return(GDI_ERROR);
	}
    
    int width = GetWidth();
    int height = GetHeight();

    // crea buffer temporaneo per error diffusion
    std::vector<float> errorR(width * height, 0);
    std::vector<float> errorG(width * height, 0);
    std::vector<float> errorB(width * height, 0);
    
    // prima passata: converte a grayscale e prepara errori
    for(int y = 0; y < height; y++)
    {
        for(int x = 0; x < width; x++)
        {
            COLORREF c = GetPixel(x, y);
            
            // converti a luminosita' (grayscale)
            float gray = 0.299f * GetRValue(c) + 0.587f * GetGValue(c) + 0.114f * GetBValue(c);
            
            int idx = y * width + x;
            errorR[idx] = GetRValue(c) - gray;
            errorG[idx] = GetGValue(c) - gray;
            errorB[idx] = GetBValue(c) - gray;
        }
    }
    
    // seconda passata: applica dithering ordered (matrice 4x4)
    const int ditherMatrix[4][4] = {
        { 0,  8,  2, 10},
        {12,  4, 14,  6},
        { 3, 11,  1,  9},
        {15,  7, 13,  5}
    };
    
    for(int y = 0; y < height; y++)
    {
        for(int x = 0; x < width; x++)
        {
            COLORREF c = GetPixel(x, y);
            
            // calcola valore con errore accumulato
            float r = GetRValue(c) + errorR[y * width + x];
            float g = GetGValue(c) + errorG[y * width + x];
            float b = GetBValue(c) + errorB[y * width + x];
            
            // applica soglia con matrice dithering
            int threshold = ditherMatrix[y % 4][x % 4] * 16; // scala a 0-240
            
            int newR = (r > threshold) ? 255 : 0;
            int newG = (g > threshold) ? 255 : 0;
            int newB = (b > threshold) ? 255 : 0;
            
            // diffonde errore (Floyd-Steinberg)
            float errR = r - newR;
            float errG = g - newG;
            float errB = b - newB;
            
            // diffusione errori ai pixel vicini
            if(x + 1 < width)
            {
                int idx = y * width + (x + 1);
                errorR[idx] += errR * 7.0f / 16.0f;
                errorG[idx] += errG * 7.0f / 16.0f;
                errorB[idx] += errB * 7.0f / 16.0f;
            }
            
            if(y + 1 < height)
            {
                if(x > 0)
                {
                    int idx = (y + 1) * width + (x - 1);
                    errorR[idx] += errR * 3.0f / 16.0f;
                    errorG[idx] += errG * 3.0f / 16.0f;
                    errorB[idx] += errB * 3.0f / 16.0f;
                }
                
                int idx = (y + 1) * width + x;
                errorR[idx] += errR * 5.0f / 16.0f;
                errorG[idx] += errG * 5.0f / 16.0f;
                errorB[idx] += errB * 5.0f / 16.0f;
                
                if(x + 1 < width)
                {
                    int idx = (y + 1) * width + (x + 1);
                    errorR[idx] += errR * 1.0f / 16.0f;
                    errorG[idx] += errG * 1.0f / 16.0f;
                    errorB[idx] += errB * 1.0f / 16.0f;
                }
            }
            
            SetPixel(x, y, RGB(newR, newG, newB));
        }
    }
    
    return(NO_ERROR);
}
#endif

/*
	Hue()

	Applica la rotazione cromatica all'immagine a seconda del numero di gradi specificato dal parametro.

	La logica del filtro Hue (tonalit'a) si basa sul modello di colore HSL/HSV, che rappresenta i colori 
	in modo piu' intuitivo rispetto a RGB.

	"Hue" e' l'angolo sul cerchio cromatico che rappresenta il "tipo" di colore puro, cambiandolo e' come
	ruotare il cerchio cromatico sotto l'immagine, si esprime in gradi:

		0° = Rosso
		60° = Giallo
		120° = Verde
		180° = Ciano
		240° = Blu
		300° = Magenta
		360° = Rosso (torna al punto iniziale)

	e si calcola: hue nuovo = (hue originale + gradi rotazione) % 360.

	Notare che sui colori acromatici (grigio, bianco e nero) la rotazione cromatica non ha effetto.
*/
UINT CImage::Hue(void)
{
	int bpp = GetBPP();

	// non applica a un immagine che e'/sembra grayscale, restituisce errore
	if(IsTrueGrayscale() || IsVisualGrayscaleStatistical(VISUAL_GRAYSCALE_PARAMS))
	{
		SetLastErrorDescriptionEx("%s(): image is grayscale, filter will not be applied",__func__);
		return(GDI_ERROR);
	}

	// ricava il valore del parametro    
    int nFactor = 0;
    double nValue = 0;
    if(GetFilterParams("Hue",nValue))
        nFactor = (int)nValue;
	else
	{
		SetLastErrorDescriptionEx("%s(): ERROR_INVALID_PARAMETER",__func__);
		return(ERROR_INVALID_PARAMETER);
	}
    
    // scala -180 a +180 gradi
	// 0	-> nessun cambio
	// +180	-> rotazione completa (+180)
	// -180	-> rotazione completa (all'indietro)
	// +90	-> rotazione di un quarto (es: rosso -> giallo)
	// -90	-> rotazione inversa di un quarto
    if(nFactor!=0)
    {
        int width = GetWidth();
        int height = GetHeight();
        
        // converte gradi in frazione di cerchio (0-1)
        double hueShift = nFactor / 360.0;
        
        for(int y = 0; y < height; y++)
        {
            for(int x = 0; x < width; x++)
            {
                COLORREF c = GetPixel(x, y);
                double H, S, L;
                
                RGBtoHSL(c, &H, &S, &L);
                
                // applica shift Hue (H e' gia' in range 0-1)
                H += hueShift;
                
                // normalizza se fuori range 0-1 (H e' ciclico)
                if(H < 0.0) H += 1.0;
                if(H >= 1.0) H -= 1.0;
                
                SetPixel(x, y, HLStoRGB(H, L, S));
            }
        }
    }
    
    return(NO_ERROR);
}

/*
	JitterHorizontal()
*/
UINT CImage::JitterHorizontal(void)
{
    if(GetBPP() < 24)
	{
		SetLastErrorDescriptionEx("%s(): at least 24 BPP required",__func__);
		return(ERROR_WRONG_BPP_FORMAT);
	}

    // ricava il valore del parametro    
    int nFactor = 0;
    double nValue = 0;
    if(GetFilterParams("JitterHorizontal", nValue))
        nFactor = (int)nValue;
    else
        return(ERROR_INVALID_PARAMETER);

    if(nFactor==0)
        return(NO_ERROR);
    
    BYTE* pixels = (BYTE*)GetPixels();
    int width = GetWidth();
    int height = GetHeight();
    int bpp = GetBPP() / 8;  // Byte per pixel: 3 per 24-bit, 4 per 32-bit
    int stride = GetBytesWidth();
    
    // verifica
    if(bpp!=3 && bpp!=4)
	{
		SetLastErrorDescriptionEx("%s(): unsupported/unknown format",__func__);
        return ERROR_UNSUPPORTED_FORMAT;
    }
    
    for(int y = 0; y < height; y++)
	{
        int shift = fast_rand_range(2*nFactor + 1) - nFactor;
        
        if(shift!=0)
		{
            BYTE* row = pixels + y * stride;

#if 1 // originale
            if(shift > 0)
			{
                // shift a destra
                for(int x = width - 1; x >= shift; x--)
				{
                    // copia tutto il pixel (3 o 4 byte)
                    for(int b = 0; b < bpp; b++)
                        row[x*bpp + b] = row[(x - shift)*bpp + b];
                }
                // riempie bordo sinistro con nero (e alpha=255 se 32-bit)
                for(int x = 0; x < shift; x++)
				{
                    row[x*bpp] = row[x*bpp + 1] = row[x*bpp + 2] = 0;
                    if(bpp==4)
                        row[x*bpp + 3] = 255; // alpha pieno
                }
            }
			else
			{
                // shift a sinistra
                shift = -shift;
                for(int x = 0; x < width - shift; x++)
				{
                    // copia tutto il pixel
                    for(int b = 0; b < bpp; b++)
                        row[x*bpp + b] = row[(x + shift)*bpp + b];
                }
                // riempie bordo destro
                for(int x = width - shift; x < width; x++)
				{
                    row[x*bpp] = row[x*bpp + 1] = row[x*bpp + 2] = 0;
                    if(bpp==4)
                        row[x*bpp + 3] = 255; // Alpha pieno
                }
            }
#else // +efficente
if(shift > 0)
{
    // shift a destra
    memmove(row + shift*bpp, row, (width - shift) * bpp);
    // riempie bordo sinistro
    memset(row, 0, shift * bpp);
 
   if(bpp==4)
        for(int x = 0; x < shift; x++)
            row[x*bpp + 3] = 255; // Alpha
/*
// Per 32-bit, invece di settare alpha=255, copia l'alpha originale
if(bpp == 4) {
    for(int x = 0; x < shift; x++) {
        row[x*bpp + 3] = row[(shift)*bpp + 3]; // Copia alpha dal primo pixel visibile
    }
}
*/

}
else
{
    shift = -shift;
    // shift a sinistra
    memmove(row, row + shift*bpp, (width - shift) * bpp);
    // riempie bordo destro
    memset(row + (width - shift)*bpp, 0, shift * bpp);

    if(bpp==4)
        for(int x = width - shift; x < width; x++)
            row[x*bpp + 3] = 255; // Alpha
/*
// per 32-bit, invece di settare alpha=255, copia l'alpha originale
if(bpp==4)
    for(int x = 0; x < shift; x++)
        row[x*bpp + 3] = row[(shift)*bpp + 3]; // Copia alpha dal primo pixel visibile
*/

}
#endif

        }
    }
    
    return(NO_ERROR);
}

/*
	JitterSinusoidal()
*/
UINT CImage::JitterSinusoidal(void)
{
    if(GetBPP() < 24)
	{
		SetLastErrorDescriptionEx("%s(): at least 24 BPP required",__func__);
		return(ERROR_WRONG_BPP_FORMAT);
	}

    // ricava il parametro
    int nFactor = 0;
    double nValue = 0;
    if(!GetFilterParams("JitterSinusoidal", nValue))
	{
		SetLastErrorDescriptionEx("%s(): ERROR_INVALID_PARAMETER",__func__);
		return(ERROR_INVALID_PARAMETER);
	}
    
    nFactor = (int)nValue;
    
    if(nFactor == 0)
        return(NO_ERROR);
    
    // parametro per scegliere tipo di effetto
    BOOL randomPerLine = FALSE; // FALSE = sinusoidale, TRUE = random come JitterHorizontal
    
    BYTE* pPixels = (BYTE*)GetPixels();
    UINT width = GetWidth();
    UINT height = GetHeight();
    UINT bpp = GetBPP();
    UINT stride = GetBytesWidth();
    
    // calcola byte per pixel
    int bytesPerPixel = bpp / 8;
    if(bytesPerPixel < 3 || bytesPerPixel > 4) {
        return ERROR_UNSUPPORTED_FORMAT;
    }
    
    // buffer temporaneo per una riga
    std::vector<BYTE> tempRow(width * bytesPerPixel);
    
    for(UINT y = 0; y < height; y++)
	{
        BYTE* row = pPixels + y * stride;
        
        // calcola shift per questa riga
        int shift = 0;
        if(randomPerLine)
		{
            // effetto random (come JitterHorizontal)
//			shift = (rand() % (2 * nFactor + 1)) - nFactor;
            shift = fast_rand_range(2 * nFactor + 1) - nFactor;
        }
		else
		{
#if 0 /* originale */
            // effetto sinusoidale puro
            // aumenta il numero di cicli per un effetto più visibile
            float cycles = 4.0f; // 4 cicli completi sull'altezza dell'immagine
            float phase = (float)y / height * 2.0f * 3.14159265f * cycles;
            shift = (int)(sin(phase) * nFactor);
#else
			// usa una funzione d'onda piu' complessa
			float amplitude = (float)nFactor;
			float frequency = 4.0f; // cicli per altezza immagine
			float phase = (float)y / height * 2.0f * (float)M_PI * frequency;

			// aggiunge offset per evitare shift negativi
			shift = (int)((sin(phase) + 1.0f) * 0.5f * amplitude);
			// ora shift e' sempre >= 0
#endif

        }
        
        // se shift e' 0, salta (mantiene l'effetto sinusoidale puro)
        if(shift==0)
            continue;
        
        // salva la riga originale
        memcpy(tempRow.data(),row,width * bytesPerPixel);
        
        // gestisce shift positivo (destra) e negativo (sinistra)
        if(shift > 0)
		{
            // shift a destra
            int pixelsToShift = min(shift, (int)width);
            
            if(pixelsToShift > 0)
			{
                // sposta i pixel
                memmove(row + pixelsToShift * bytesPerPixel, 
                        row, 
                        (width - pixelsToShift) * bytesPerPixel);
                
                // riempie il bordo sinistro
                for(int x = 0; x < pixelsToShift; x++) {
                    // nero per RGB
                    row[x * bytesPerPixel + 0] = 0;     // R/B
                    row[x * bytesPerPixel + 1] = 0;     // G
                    row[x * bytesPerPixel + 2] = 0;     // B/R
                    
                    // gestisce alpha se presente
                    if(bytesPerPixel==4)
					{
                        // mantieni alpha originale o imposta a 255?
                        // row[x * bytesPerPixel + 3] = 255; // alpha opaco
                        // oppure copia alpha dal primo pixel visibile:
                        if((int)width > pixelsToShift)
                            row[x * bytesPerPixel + 3] = row[pixelsToShift * bytesPerPixel + 3];
                        else
                            row[x * bytesPerPixel + 3] = 255;
                    }
                }
            }
        }
		else
		{
            // shift a sinistra
            shift = -shift;
            int pixelsToShift = min(shift, (int)width);
            
            if(pixelsToShift > 0)
			{
                // sposta i pixel
                memmove(row, 
                        row + pixelsToShift * bytesPerPixel, 
                        (width - pixelsToShift) * bytesPerPixel);
                
                // riempie il bordo destro
                int startFill = width - pixelsToShift;
                for(int x = startFill; x < (int)width; x++)
				{
                    // nero per RGB
                    row[x * bytesPerPixel + 0] = 0;
                    row[x * bytesPerPixel + 1] = 0;
                    row[x * bytesPerPixel + 2] = 0;
                    
                    // gestisci alpha se presente
                    if(bytesPerPixel==4)
					{
                        // row[x * bytesPerPixel + 3] = 255; // alpha opaco
                        // oppure copia alpha dall'ultimo pixel visibile:
                        if(startFill > 0)
                            row[x * bytesPerPixel + 3] = row[(startFill - 1) * bytesPerPixel + 3];
                        else
                            row[x * bytesPerPixel + 3] = 255;
                    }
                }
            }
        }
    }
    
    return(NO_ERROR);
}

/*
    Median()

    Applica un filtro mediano per ridurre il rumore preservando i bordi.
    Il filtro sostituisce ogni pixel con il valore mediano della finestra NxN circostante.
    
    Supporta solo immagini 24-bit (RGB) e 32-bit (ARGB).
    Per immagini 32-bit, il canale Alpha viene preservato intatto.
	
	Intervallo parametri: 0.0 - 1.0
	0.0-0.2: Pulizia immagine (rimozione rumore)
	0.2-0.5: Effetto artistico leggero
	0.5-0.8: Effetto artistico forte
	0.8-1.0: Effetto estremo/stilizzazione
*/
UINT CImage::Median(void)
{
    // 4. Verifica formato immagine
    UINT bpp = GetBPP();
    if (bpp < 24)
	{
		SetLastErrorDescriptionEx("%s(): at least 24 BPP required",__func__);
		return(ERROR_WRONG_BPP_FORMAT);
	}
    
    // 1. Ottieni parametro intensity (0.0 - 1.0)
    double intensity = 0.0;
    if (!GetFilterParams("Median", intensity))
	{
		SetLastErrorDescriptionEx("%s(): ERROR_INVALID_PARAMETER",__func__);
		return(ERROR_INVALID_PARAMETER);
	}
    
    // 2. Validazione range (0.0 - 1.0)
    if (intensity < 0.0 || intensity > 1.0)
	{
		SetLastErrorDescriptionEx("%s(): ERROR_INVALID_PARAMETER",__func__);
		return(ERROR_INVALID_PARAMETER);
	}
    
    // 3. Se intensity è 0, nessuna operazione
    if (intensity < 0.001)
        return(NO_ERROR);
    
    // 5. Ottieni dati immagine
    void* pPixels = GetPixels();
    UINT width = GetWidth();
    UINT height = GetHeight();
    UINT stride = GetBytesWidth();
    
    if (!pPixels || width == 0 || height == 0)
        return(GDI_ERROR);
    
    // 6. Determina ordine scanline
    int dibOrder = GetDIBOrder();
    bool topDown = (dibOrder == 1);
    
    // 7. Crea copia dei dati originali
    UINT bufferSize = stride * height;
    std::vector<BYTE> originalBuffer(bufferSize);
    memcpy(originalBuffer.data(), pPixels, bufferSize);
    
    // 8. CALCOLO KERNEL CON FUNZIONE CORRETTA
    // -----------------------------------------------------------------
    // intensity è già 0.0-1.0
    // Usiamo funzione personalizzata basata sui tuoi test
    // -----------------------------------------------------------------
    
    float percent = (float)intensity;  // 0.0-1.0
    
    // FUNZIONE BASATA SU TEST EMPIRICI:
    
    int kernelSize;
    
    if (percent <= 0.0f) {
        kernelSize = 1;
    }
    else if (percent <= 0.1f) {
        // Interpolazione lineare 0->3, 0.1->5
        kernelSize = 3 + (int)(2.0f * (percent / 0.1f));
    }
    else if (percent <= 0.3f) {
        // Interpolazione lineare 0.1->5, 0.3->7
        kernelSize = 5 + (int)(2.0f * ((percent - 0.1f) / 0.2f));
    }
    else if (percent <= 0.6f) {
        // Interpolazione lineare 0.3->7, 0.6->9
        kernelSize = 7 + (int)(2.0f * ((percent - 0.3f) / 0.3f));
    }
    else if (percent <= 0.85f) {
        // Interpolazione lineare 0.6->9, 0.85->11
        kernelSize = 9 + (int)(2.0f * ((percent - 0.6f) / 0.25f));
    }
    else {
        // 0.85->11, 1.0->15 (crescita più rapida alla fine)
        kernelSize = 11 + (int)(4.0f * ((percent - 0.85f) / 0.15f));
    }
    
    // Assicura kernel dispari
    if (kernelSize % 2 == 0)
        kernelSize--;
    
    // Clamping finale
    if (kernelSize < 3) kernelSize = 3;
    if (kernelSize > 31) kernelSize = 31;
    
    int halfKernel = kernelSize / 2;
    
    // 9. DEBUG
    #ifdef _DEBUG
    char debugMsg[128];
    sprintf(debugMsg, "Median: intensity=%.3f, kernel=%dx%d", 
            intensity, kernelSize, kernelSize);
    OutputDebugStringA(debugMsg);
    #endif
    
    // 10. Parametri pixel
    int bytesPerPixel = bpp / 8;
    bool hasAlpha = (bytesPerPixel == 4);
    
    // 11. Applica filtro
    for (int channel = 0; channel < 3; channel++)
    {
        ApplyMedianChannel(
            (BYTE*)pPixels, originalBuffer.data(),
            width, height, stride, bytesPerPixel,
            channel, halfKernel, kernelSize,
            topDown, hasAlpha
        );
    }
    
    return(NO_ERROR);
}

/*
    ApplyMedianChannel()
    
    Applica filtro mediano a un singolo canale usando algoritmo sliding histogram.
    Gestisce automaticamente i bordi con clamping.
*/
void CImage::ApplyMedianChannel(
    BYTE* dst, const BYTE* src,
    UINT width, UINT height, UINT stride,
    int bytesPerPixel, int channel,
    int halfKernel, int kernelSize,
    bool topDown, bool hasAlpha)
{
    // Calcola mediana target (50% dei pixel nella finestra)
    int medianTarget = (kernelSize * kernelSize) / 2;
    
    // Per ogni riga dell'immagine
    for (UINT y = 0; y < height; y++)
    {
        // Puntatore alla riga di destinazione
        BYTE* dstLine = dst + y * stride;
        
        // Inizializza istogramma per la prima colonna di questa riga
        int histogram[256] = {0};
        InitHistogramForColumn(histogram, src, width, height, stride,
                               bytesPerPixel, channel, 0, y,
                               halfKernel, topDown, hasAlpha);
        
        // Per ogni colonna in questa riga
        for (UINT x = 0; x < width; x++)
        {
            // 1. Trova mediana dall'istogramma corrente
            int medianValue = FindMedianFromHistogram(histogram, medianTarget);
            
            // 2. Imposta il valore mediano nel pixel di destinazione
            int pixelIndex = x * bytesPerPixel + channel;
            dstLine[pixelIndex] = (BYTE)medianValue;
            
            // 3. Prepara per prossima colonna: rimuovi colonna sinistra, aggiungi colonna destra
            if (x + 1 < width)
            {
                UpdateHistogramForShift(
                    histogram, src, width, height, stride,
                    bytesPerPixel, channel, x, y,
                    halfKernel, topDown, hasAlpha
                );
            }
        }
    }
}

/*
    InitHistogramForColumn()
    
    Inizializza l'istogramma per la prima colonna di una riga.
    Conta tutti i pixel nella finestra centrata su (x, y).
*/
void CImage::InitHistogramForColumn(
    int histogram[256], const BYTE* src,
    UINT width, UINT height, UINT stride,
    int bytesPerPixel, int channel,
    UINT startX, UINT startY,
    int halfKernel, bool topDown, bool hasAlpha)
{
    // Resetta istogramma
    memset(histogram, 0, 256 * sizeof(int));
    
    // Calcola coordinate immagine considerando ordine scanline
    int baseY = topDown ? startY : (height - 1 - startY);
    
    // Per ogni riga nel kernel
    for (int ky = -halfKernel; ky <= halfKernel; ky++)
    {
        int srcY = baseY + ky;
        
        // Clamping verticale
        if (srcY < 0) srcY = 0;
        if (srcY >= (int)height) srcY = height - 1;
        
        // Converti a coordinate buffer se bottom-up
        if (!topDown)
            srcY = height - 1 - srcY;
            
        const BYTE* srcLine = src + srcY * stride;
        
        // Per ogni colonna nel kernel per questa colonna iniziale
        for (int kx = -halfKernel; kx <= halfKernel; kx++)
        {
            int srcX = startX + kx;
            
            // Clamping orizzontale
            if (srcX < 0) srcX = 0;
            if (srcX >= (int)width) srcX = width - 1;
            
            int pixelIndex = srcX * bytesPerPixel + channel;
            BYTE pixelValue = srcLine[pixelIndex];
            
            histogram[pixelValue]++;
        }
    }
}

/*
    UpdateHistogramForShift()
    
    Aggiorna l'istogramma quando la finestra si sposta di una colonna a destra.
    Rimuove la colonna uscente (sinistra) e aggiunge la colonna entrante (destra).
*/
void CImage::UpdateHistogramForShift(
    int histogram[256], const BYTE* src,
    UINT width, UINT height, UINT stride,
    int bytesPerPixel, int channel,
    UINT currentX, UINT currentY,
    int halfKernel, bool topDown, bool hasAlpha)
{
    // Coordinate considerando ordine scanline
    int baseY = topDown ? currentY : (height - 1 - currentY);
    
    // Colonna che esce (sinistra) e colonna che entra (destra)
    int leftCol = currentX - halfKernel;
    int rightCol = currentX + halfKernel + 1;
    
    // Applica clamping alle colonne
    if (leftCol < 0) leftCol = 0;
    if (rightCol >= (int)width) rightCol = width - 1;
    
    // Per ogni riga nel kernel
    for (int ky = -halfKernel; ky <= halfKernel; ky++)
    {
        int srcY = baseY + ky;
        
        // Clamping verticale
        if (srcY < 0) srcY = 0;
        if (srcY >= (int)height) srcY = height - 1;
        
        // Converti a coordinate buffer se bottom-up
        if (!topDown)
            srcY = height - 1 - srcY;
            
        const BYTE* srcLine = src + srcY * stride;
        
        // Rimuovi pixel dalla colonna sinistra (se valida)
        if (leftCol >= 0 && leftCol < (int)width)
        {
            int leftPixelIndex = leftCol * bytesPerPixel + channel;
            BYTE leftValue = srcLine[leftPixelIndex];
            histogram[leftValue]--;
        }
        
        // Aggiungi pixel dalla colonna destra (se valida)
        if (rightCol >= 0 && rightCol < (int)width)
        {
            int rightPixelIndex = rightCol * bytesPerPixel + channel;
            BYTE rightValue = srcLine[rightPixelIndex];
            histogram[rightValue]++;
        }
    }
}

/*
    FindMedianFromHistogram()
    
    Trova il valore mediano da un istogramma accumulato.
    Cerca il valore per cui la somma cumulativa raggiunge medianTarget.
*/
int CImage::FindMedianFromHistogram(const int histogram[256], int medianTarget)
{
    int cumulative = 0;
    
    for (int i = 0; i < 256; i++)
    {
        cumulative += histogram[i];
        
        if (cumulative > medianTarget)
        {
            return i;
        }
    }
    
    // Caso limite: restituisci ultimo valore
    return 255;
}

/*
	Mirror()
*/
/* LOCAL */UINT CImage::Mirror(UINT nDirection/* 0 = horiz, 1 = vert */)
{
	UINT nRet = GDI_ERROR;
	BOOL bLocked = FALSE;
	
	HDC hDC = NULL;
	BITMAPINFO *pBitmapInfoSrc = NULL;
	BITMAPINFO *pBitmapInfo = NULL;
	HBITMAP hBitmap = NULL;
	HBITMAP hOldBitmap = NULL;
	HPALETTE hPal = NULL;
	HPALETTE hOldPal = NULL;
	UINT nWidth;
	UINT nHeight;

	if(LockData())
	{
		bLocked = TRUE;

		hDC = ::CreateCompatibleDC(NULL);
		if(!hDC)
		{
			SetLastErrorDescriptionEx("%s(): unable to create the DC",__func__);
			goto done;
		}

		pBitmapInfoSrc = GetBMI();
		unsigned char *pDataSrc = (unsigned char*)GetPixels();
		if(!pBitmapInfoSrc || !pDataSrc)
		{
			SetLastErrorDescriptionEx("%s(): invalid image data",__func__);
			goto done;
		}

		// note:
		// - usa CImage::GetNumcolor e non quella della derivata perche' in alcuni casi, come paintLib, questa
		//   restituisce 16 mil., mentre per img con 24 o 32 bpp deve essere 0
		// - per calcolare la dimensione di pBitmapInfo, usa GetMaxPaletteColors() e non GetNumColors() per pura
		//   strategia difensiva, ossia alloca sempre lo spazio per 256 colori a prescindere dal tipo di img nel
		//   caso qualche conversione o chiamta GDI vada a scrivere pensando di avere tale spazio a disposizione
		pBitmapInfo = (BITMAPINFO*)new char[sizeof(BITMAPINFOHEADER) + (GetMaxPaletteColors() * sizeof(RGBQUAD))];
		if(!pBitmapInfo)
			goto done;

		memset(pBitmapInfo,'\0',sizeof(BITMAPINFOHEADER) + (GetMaxPaletteColors() * sizeof(RGBQUAD)));
		memcpy(pBitmapInfo,pBitmapInfoSrc,sizeof(BITMAPINFOHEADER) + (CImage::GetNumColors() * sizeof(RGBQUAD)));

		void* pVoid;		
		hBitmap = ::CreateDIBSection(hDC,(LPBITMAPINFO)pBitmapInfo,DIB_RGB_COLORS,&pVoid,0,0);
		if(!hBitmap)
		{
			SetLastErrorDescriptionEx("%s(): unable to create the DIB section",__func__);
			goto done;
		}

		hOldBitmap = (HBITMAP)::SelectObject(hDC,hBitmap);

		hPal = CreateDIBPalette((BITMAPINFO*)pBitmapInfoSrc);
   		if(hPal)
		{
			hOldPal = ::SelectPalette(hDC,hPal,FALSE);
			::RealizePalette(hDC);
		}
		
		nWidth = GetWidth();
		nHeight = GetHeight();
		
		if(nDirection==0)
		{
			nRet = ::StretchDIBits(	hDC,
									0,
									0,
									nWidth,
									nHeight,
									0,
									nHeight-1,
									nWidth,
									(-1)*nHeight,
									(LPSTR)GetPixels(),
									(BITMAPINFO*)pBitmapInfoSrc,
									DIB_RGB_COLORS,
									SRCCOPY);
		}
		else if(nDirection==1)
		{
			nRet = ::StretchDIBits(	hDC,
									0,
									0,
									nWidth,
									nHeight,
									nWidth-1,
									0,
									(-1)*nWidth,
									nHeight,
									(LPSTR)GetPixels(),
									(BITMAPINFO*)pBitmapInfoSrc,
									DIB_RGB_COLORS,
									SRCCOPY);
		}
		else
		{
			SetLastErrorDescriptionEx("%s(): wrong direction",__func__);
			nRet = GDI_ERROR;
		}

		::SelectObject(hDC,hOldBitmap);
		
		if(nRet==GDI_ERROR)
			goto done;
		else
			nRet = NO_ERROR;

		bLocked = !UnlockData();

		if(Create(pBitmapInfo,0) && LockData())
		{
			nRet = ::GetDIBits(hDC,hBitmap,0,nHeight,GetPixels(),GetBMI(),DIB_RGB_COLORS)!=0 ? NO_ERROR : GDI_ERROR;
			bLocked = !UnlockData(TRUE);
		}
			
		if(hOldPal)
			::SelectPalette(hDC,hOldPal,TRUE);			
	}

done:

	if(hDC)
		::DeleteDC(hDC);
	if(pBitmapInfo)
		delete [] pBitmapInfo;
	if(hBitmap)
		 ::DeleteObject(hBitmap);
	if(hPal)
		 ::DeleteObject(hPal);

	if(bLocked)
		UnlockData();
	
	return(nRet);
}

/*
	Negate()

	Solo x immagini bianco e nero (1-bit), NON per scala di grigi (8-bit) e resto.
*/
UINT CImage::Negate(void)
{
    UINT bpp = GetBPP();
    UINT width = GetWidth();
    UINT height = GetHeight();
    
    // Caso 1: Usa GetPixel/SetPixel (lento ma funziona per qualsiasi formato)
    if(bpp >= 24) {
        // Metodo semplice per 24/32-bit
        for(UINT y = 0; y < height; y++) {
            for(UINT x = 0; x < width; x++) {
                COLORREF pixel = GetPixel(x, y);
                
                // Inverte RGB
                int r = 255 - GetRValue(pixel);
                int g = 255 - GetGValue(pixel);
                int b = 255 - GetBValue(pixel);
                
                // Per 32-bit, mantieni alpha originale
                if(bpp == 32) {
                    // Se GetPixel restituisce COLORREF senza alpha,
                    // dobbiamo gestirlo diversamente
                    SetPixel(x, y, RGB(r, g, b));
                } else {
                    SetPixel(x, y, RGB(r, g, b));
                }
            }
        }
        return NO_ERROR;
    }
    
    // Caso 2: 1-bit (già implementato nel tuo codice)
    else if(bpp == 1) {
        if(!LockData())
            return GDI_ERROR;
            
        BYTE* pData = (BYTE*)GetPixels();
        if(!pData) {
            UnlockData(FALSE);
            return GDI_ERROR;
        }
        
        UINT bytesWidth = GetBytesWidth();
        UINT totalBytes = height * bytesWidth;
        
        for(UINT i = 0; i < totalBytes; i++) {
            pData[i] = ~pData[i];
        }
        
        UnlockData(TRUE);
        return NO_ERROR;
    }
    
    // Caso 3: 4-bit o 8-bit con palette (indexed/grayscale)
    else if(bpp == 4 || bpp == 8) {
        // Due opzioni:
        // A) Invertire la palette (più efficiente)
        // B) Invertire i valori dei pixel
        
        // Prova con la palette se disponibile
        LPBITMAPINFO bmi = GetBMI();
        if(bmi && bmi->bmiHeader.biBitCount == bpp) {
            // Inverte i colori nella palette
            int numColors = (bpp == 4) ? 16 : 256;
            RGBQUAD* palette = bmi->bmiColors;
            
            for(int i = 0; i < numColors; i++) {
                palette[i].rgbRed = 255 - palette[i].rgbRed;
                palette[i].rgbGreen = 255 - palette[i].rgbGreen;
                palette[i].rgbBlue = 255 - palette[i].rgbBlue;
                // Alpha rimane invariato se presente
            }
            
            // Aggiorna l'immagine con la nuova palette
            if(SetPalette(0, numColors, palette)) {
                return NO_ERROR;
            }
        }
        
        // Fallback: usa GetPixel/SetPixel anche per 8-bit
        // (le derivate che supportano GetPixel/SetPixel per 8-bit funzioneranno)
        for(UINT y = 0; y < height; y++) {
            for(UINT x = 0; x < width; x++) {
                COLORREF pixel = GetPixel(x, y);
                int r = 255 - GetRValue(pixel);
                int g = 255 - GetGValue(pixel);
                int b = 255 - GetBValue(pixel);
                SetPixel(x, y, RGB(r, g, b));
            }
        }
        return NO_ERROR;
    }
    
	SetLastErrorDescriptionEx("%s(): unsupported BPP value",__func__);
	return(ERROR_WRONG_BPP_FORMAT);
}

/*
	Noise()

	Produce un rumore digitale puro, dove ogni canale varia indipendentemente, diverso dall'effetto 
	"pellicola sgranata" di Grain().
	In sintesi, Noise() produce una interferenza digitale pura, mentre Grain() produce un effetto 
	analogico artistico.

	Il parametro per la intensita' (0-100) regola la intensita' del rumore, mentre quello per il tipo
	(0-2) deterimna che tipo di rumore viene prodotto:
	
	0 = uniforme (pixel "colorati" sparsi, effetto TV cattiva ricezione)
	1 = gaussiano (piu' simile a rumore reale di sensore)
	2 = salt & pepper (puntini neri/bianchi sparsi, tipo vecchie fotocopie)
*/
UINT CImage::Noise(void)
{
	// ricava i valori dei parametri
	// range: 0-2, 0-100
	// default: 0;40
    char szValues[32] = {0};
    double nValue = 0;
    if(!GetFilterParams("Noise",nValue,szValues,sizeof(szValues)))
	{
		SetLastErrorDescriptionEx("%s(): ERROR_INVALID_PARAMETER",__func__);
		return(ERROR_INVALID_PARAMETER);
	}

	int noiseType = 0;
	int noiseIntensity = 0;

	const char* delimiter = ";";
    char* token = strtok(szValues,delimiter);

    for(int i=0; token!=NULL; i++)
	{
		switch(i) {
			case 0:	noiseType = atoi(token);      break;
			case 1:	noiseIntensity = atoi(token); break;
		}
		token = strtok(NULL,delimiter);
    }

	if((noiseType < 0 || noiseType > 2) || (noiseIntensity < 0 || noiseIntensity > 100))
		return(ERROR_INVALID_PARAMETER);

    int width = GetWidth();
    int height = GetHeight();
    
    // converte intensita' in ampiezza rumore
    int noiseAmplitude = (noiseIntensity * 255) / 100;
    
//	srand((unsigned int)time(NULL));
    
    for(int y = 0; y < height; y++)
    {
        for(int x = 0; x < width; x++)
        {
            COLORREF c = GetPixel(x, y);
            int r = GetRValue(c);
            int g = GetGValue(c);
            int b = GetBValue(c);
            
            int noiseR, noiseG, noiseB;
            
            switch(noiseType)
            {
                case 0: // rumore uniforme (digitale puro)
                default:
                    //noiseR = (rand() % (noiseAmplitude * 2)) - noiseAmplitude;
					noiseR = fast_rand_symmetric(noiseAmplitude);
                    //noiseG = (rand() % (noiseAmplitude * 2)) - noiseAmplitude;
					noiseG = fast_rand_symmetric(noiseAmplitude);
                    //noiseB = (rand() % (noiseAmplitude * 2)) - noiseAmplitude;
					noiseB = fast_rand_symmetric(noiseAmplitude);
                    break;
                    
                case 1: // rumore gaussiano (piu' naturale)
#if 1
					// versione ottimizzata con fast_rand_range
					noiseR = (fast_rand_range(noiseAmplitude) + 
							  fast_rand_range(noiseAmplitude) +
							  fast_rand_range(noiseAmplitude) + 
							  fast_rand_range(noiseAmplitude)) / 2 - noiseAmplitude;

					noiseG = (fast_rand_range(noiseAmplitude) + 
							  fast_rand_range(noiseAmplitude) +
							  fast_rand_range(noiseAmplitude) + 
							  fast_rand_range(noiseAmplitude)) / 2 - noiseAmplitude;

					noiseB = (fast_rand_range(noiseAmplitude) + 
							  fast_rand_range(noiseAmplitude) +
							  fast_rand_range(noiseAmplitude) + 
							  fast_rand_range(noiseAmplitude)) / 2 - noiseAmplitude;
#else
					// approssimazione con somma di 4 rand()
                    noiseR = ((rand() % noiseAmplitude) + 
                              (rand() % noiseAmplitude) +
                              (rand() % noiseAmplitude) + 
                              (rand() % noiseAmplitude)) / 2 - noiseAmplitude;
                    noiseG = ((rand() % noiseAmplitude) + 
                              (rand() % noiseAmplitude) +
                              (rand() % noiseAmplitude) + 
                              (rand() % noiseAmplitude)) / 2 - noiseAmplitude;
                    noiseB = ((rand() % noiseAmplitude) + 
                              (rand() % noiseAmplitude) +
                              (rand() % noiseAmplitude) + 
                              (rand() % noiseAmplitude)) / 2 - noiseAmplitude;
#endif
                    break;
                    
                case 2: // Salt & Pepper (impulsivo)
#if 1
					// "Pepper" (nero)
					if(fast_rand_chance(noiseIntensity))
						r = g = b = 0;
					// "Salt" (bianco)
					else if(fast_rand_chance(noiseIntensity))
						r = g = b = 255;
                    // else: nessun cambiamento
                    noiseR = noiseG = noiseB = 0;
                    break;
#else
                    int randVal = rand() % 1000;
                    // "Pepper" (nero)
					if(randVal < noiseIntensity)
                        r = g = b = 0;
					// "Salt" (bianco)
                    else if(randVal > 1000 - noiseIntensity)
                        r = g = b = 255;
                    // else: nessun cambiamento
                    noiseR = noiseG = noiseB = 0;
                    break;
#endif
            }
            
            if(noiseType!=2) // non per Salt & Pepper
            {
                r += noiseR;
                g += noiseG;
                b += noiseB;
            }
            
            // clamping
            r = (r < 0) ? 0 : (r > 255) ? 255 : r;
            g = (g < 0) ? 0 : (g > 255) ? 255 : g;
            b = (b < 0) ? 0 : (b > 255) ? 255 : b;
            
            SetPixel(x, y, RGB(r, g, b));
        }
    }
    
    return(NO_ERROR);
}

/*
    Pixelate()

    Applica un effetto pixel art, dividendo l'immagine in blocchi quadrati e sostituendo ogni 
	blocco con il colore mediano dei suoi pixel. Il mediano mantiene colori puri e contrasto, 
	ideale per effetti artistici.
    
	parametro: blockSize (2-32):
	2  = effetto molto fine (pixel art ad alta risoluzione)
	8  = effetto medio (visibile ma riconoscibile)
	16 = effetto forte (stilizzazione evidente)
	32 = effetto estremo (immagine molto astratta)
    
	Ovviamente l'intensita' dell'effetto dipende, oltre che dal parametro, dalla risoluzione
	dell immagine (maggiore risuluzione = minore effetto e viceversa).
*/
UINT CImage::Pixelate(void)
{
    // 1. Ottieni parametro blockSize (2-32)
    double nValue = 0;
    if (!GetFilterParams("Pixelate", nValue))
	{
		SetLastErrorDescriptionEx("%s(): ERROR_INVALID_PARAMETER",__func__);
		return(ERROR_INVALID_PARAMETER);
	}
    
    int blockSize = (int)nValue;
    
    // 2. Validazione range
    if (blockSize < 2 || blockSize > 32)
        return(ERROR_INVALID_PARAMETER);
    
    // 3. Se blockSize è 1, nessuna operazione (ma minimo è 2)
    if (blockSize == 1)
        return(NO_ERROR);
    
    // 4. Verifica formato immagine
    UINT bpp = GetBPP();
    if (bpp < 24)
        return(GDI_ERROR);  // Solo 24-bit (RGB) e 32-bit (ARGB)
    
    // 5. Ottieni dati immagine
    void* pPixels = GetPixels();
    UINT width = GetWidth();
    UINT height = GetHeight();
    UINT stride = GetBytesWidth();
    
    if (!pPixels || width == 0 || height == 0)
        return(GDI_ERROR);
    
    // 6. Determina ordine scanline
    int dibOrder = GetDIBOrder();
    bool topDown = (dibOrder == 1);
    
    // 7. Crea copia dei dati originali per lettura
    UINT bufferSize = stride * height;
    std::vector<BYTE> originalBuffer(bufferSize);
    memcpy(originalBuffer.data(), pPixels, bufferSize);
    
    // 8. Parametri pixel
    int bytesPerPixel = bpp / 8;
    bool hasAlpha = (bytesPerPixel == 4);
    
    // 9. DEBUG
    #ifdef _DEBUG
    char debugMsg[128];
    sprintf(debugMsg, "Pixelate: blockSize=%d, image=%dx%d", 
            blockSize, width, height);
    OutputDebugStringA(debugMsg);
    #endif
    
    // 10. Calcola numero di blocchi (arrotondato per eccesso)
    int numBlocksX = (width + blockSize - 1) / blockSize;
    int numBlocksY = (height + blockSize - 1) / blockSize;
    
    // 11. Per ogni blocco nell'immagine
    for (int blockY = 0; blockY < numBlocksY; blockY++)
    {
        for (int blockX = 0; blockX < numBlocksX; blockX++)
        {
            // Calcola coordinate e dimensioni del blocco (gestendo bordi)
            int startX = blockX * blockSize;
            int startY = blockY * blockSize;
            
            int blockWidth = blockSize;
            int blockHeight = blockSize;
            
            // Regola dimensioni per blocchi parziali ai bordi
            if (startX + blockWidth > (int)width)
                blockWidth = width - startX;
            
            if (startY + blockHeight > (int)height)
                blockHeight = height - startY;
            
            // Salta blocchi troppo piccoli (dovrebbero essere rari)
            if (blockWidth < 1 || blockHeight < 1)
                continue;
            
            // 12. Calcola colore mediano di questo blocco
            COLORREF medianColor = CalculateBlockMedian(
                originalBuffer.data(), width, height, stride,
                bytesPerPixel, startX, startY, 
                blockWidth, blockHeight
            );
            
            // 13. Riempi il blocco con il colore mediano
            FillBlock(
                (BYTE*)pPixels, width, height, stride,
                bytesPerPixel, startX, startY,
                blockWidth, blockHeight, medianColor
            );
        }
    }
    
    // 14. Per immagini 32-bit, ripristina canale alpha dall'originale
    if (hasAlpha)
    {
        // Copia il canale alpha dall'originale (non modificato dal pixelate)
        for (UINT y = 0; y < height; y++)
        {
            BYTE* dstLine = (BYTE*)pPixels + y * stride;
            const BYTE* srcLine = originalBuffer.data() + y * stride;
            
            for (UINT x = 0; x < width; x++)
            {
                int idx = x * bytesPerPixel + 3;  // Posizione alpha (byte 3 in ARGB)
                dstLine[idx] = srcLine[idx];
            }
        }
    }
    
    return(NO_ERROR);
}

/*
    CalculateBlockMedian()
    
    Calcola il colore mediano di un blocco rettangolare.
    Usa istogrammi separati per R, G, B per efficienza.
*/
COLORREF CImage::CalculateBlockMedian(
    const BYTE* src, UINT width, UINT height, UINT stride,
    int bytesPerPixel, int startX, int startY,
    int blockWidth, int blockHeight)
{
    // Inizializza istogrammi per R, G, B
    int histR[256] = {0};
    int histG[256] = {0};
    int histB[256] = {0};
    
    int totalPixels = blockWidth * blockHeight;
    int medianTarget = totalPixels / 2;
    
    // Popola istogrammi con i pixel del blocco
    for (int y = 0; y < blockHeight; y++)
    {
        int srcY = startY + y;
        if (srcY >= (int)height) break;
        
        const BYTE* srcLine = src + srcY * stride;
        
        for (int x = 0; x < blockWidth; x++)
        {
            int srcX = startX + x;
            if (srcX >= (int)width) break;
            
            int pixelIdx = srcX * bytesPerPixel;
            
            histB[srcLine[pixelIdx + 0]]++;     // Blue
            histG[srcLine[pixelIdx + 1]]++;     // Green
            histR[srcLine[pixelIdx + 2]]++;     // Red
        }
    }
    
    // Trova mediane per ciascun canale
    int medianR = FindMedianFromHistogram(histR, medianTarget);
    int medianG = FindMedianFromHistogram(histG, medianTarget);
    int medianB = FindMedianFromHistogram(histB, medianTarget);
    
    return RGB(medianR, medianG, medianB);
}

/*
    FillBlock()
    
    Riempi un blocco rettangolare con un colore uniforme.
*/
void CImage::FillBlock(
    BYTE* dst, UINT width, UINT height, UINT stride,
    int bytesPerPixel, int startX, int startY,
    int blockWidth, int blockHeight, COLORREF color)
{
    BYTE r = GetRValue(color);
    BYTE g = GetGValue(color);
    BYTE b = GetBValue(color);
    
    for (int y = 0; y < blockHeight; y++)
    {
        int dstY = startY + y;
        if (dstY >= (int)height) break;
        
        BYTE* dstLine = dst + dstY * stride;
        
        for (int x = 0; x < blockWidth; x++)
        {
            int dstX = startX + x;
            if (dstX >= (int)width) break;
            
            int pixelIdx = dstX * bytesPerPixel;
            
            dstLine[pixelIdx + 0] = b;  // Blue
            dstLine[pixelIdx + 1] = g;  // Green
            dstLine[pixelIdx + 2] = r;  // Red
            // Alpha rimane invariato (gestito separatamente)
        }
    }
}

/*
	PixelSort()
*/
UINT CImage::PixelSort(void)
{
	// ricava i valori dei parametri
    char szValues[32] = {0};
    double nValue = 0;
    if(!GetFilterParams("PixelSort",nValue,szValues,sizeof(szValues)))
	{
		SetLastErrorDescriptionEx("%s(): ERROR_INVALID_PARAMETER",__func__);
		return(ERROR_INVALID_PARAMETER);
	}

	int direction = 0;
	int criterion = 0;
	int threshold = 0;
	int maxLength = 0;

    const char* delimiter = ";";
    char* token = strtok(szValues,delimiter);

    for(int i=0; token!=NULL; i++)
	{
		switch(i) {
			case 0:	direction = atoi(token); break;
			case 1:	criterion = atoi(token); break;
			case 2:	threshold = atoi(token); break;
			case 3:	maxLength = atoi(token); break;
		}
		token = strtok(NULL,delimiter);
    }

	if(	(direction < 0 || direction > 3)	||
		(criterion < 0 || criterion > 3)	||
		(threshold < 0 || threshold > 255)	||
		(maxLength < 0 || maxLength > 255)	)
		return(ERROR_INVALID_PARAMETER);

	PixelSort(direction,criterion,threshold,maxLength);

	return(NO_ERROR);

}

/*
	PixelSort()

	La logica dell'algoritmo e' ordinare i pixel secondo criteri visivi, come luminosita', 
	tonalita', etc., creando "linee fluide" o "bande" artistiche(?) nell'immagine.

	direction: 0=orizzontale, 1=verticale, 2=radiale, 3=diagonale
	criterion: 0=luminosità, 1=tonalità (angolo sul cerchio cromatico), 2=saturazione (intensità del colore), 3=valore (massimo tra R,G,B)
	threshold: Soglia minima (0-255) per selezionare quali pixel ordinare
	maxLength: Lunghezza massima segmento (0=nessun limite)

	PixelSort(0, 0, 180, 0);  // sottile - mhm
	PixelSort(0, 1, 128, 0);  // artistico - una merda
	PixelSort(1, 0, 80, 0);   // drammatico - forte
	PixelSort(0, 0, 150, 20); // texture - quasi inesistente

*/
UINT CImage::PixelSort(int direction, int criterion,int threshold, int maxLength)
{
    // 2. Ottieni dati immagine
    void* pPixels = GetPixels();
    UINT width = GetWidth();
    UINT height = GetHeight();
    UINT bpp = GetBPP();
    UINT stride = GetBytesWidth();
    
    if (!pPixels || width < 2 || height < 2 || bpp < 24) {
        return 0;
    }
    
    int bytesPerPixel = bpp / 8;
    
    // 3. Crea copia dei pixel per ordinamento
    std::vector<BYTE> pixelBuffer(stride * height);
    memcpy(pixelBuffer.data(), pPixels, stride * height);
    
    // 4. Applica pixel sorting in base alla direzione
    switch (direction) {
        case 0: // Orizzontale (da sinistra a destra)
            SortHorizontal(pixelBuffer.data(), width, height, 
                          stride, bytesPerPixel, criterion, 
                          threshold, maxLength);
            break;
            
        case 1: // Verticale (dall'alto in basso)
            SortVertical(pixelBuffer.data(), width, height, 
                        stride, bytesPerPixel, criterion,
                        threshold, maxLength);
            break;
            
        case 2: // Radiale (dal centro verso l'esterno)
            SortRadial(pixelBuffer.data(), width, height,
                      stride, bytesPerPixel, criterion,
                      threshold, maxLength);
            break;
            
        case 3: // Angolato (diagonale 45°)
            SortDiagonal(pixelBuffer.data(), width, height,
                        stride, bytesPerPixel, criterion,
                        threshold, maxLength);
            break;
    }
    
    // 5. Copia risultato finale
    memcpy(pPixels, pixelBuffer.data(), stride * height);
    
    return 1;
}

// Helper: Ordinamento orizzontale
void CImage::SortHorizontal(BYTE* pixels, UINT width, UINT height,
                           UINT stride, int bpp, int criterion,
                           int threshold, int maxLength)
{
    for (UINT y = 0; y < height; y++) {
        BYTE* row = pixels + y * stride;
        
        // Trova segmenti da ordinare
        int segmentStart = -1;
        
        for (UINT x = 0; x < width; x++) {
            BYTE* pixel = row + x * bpp;
            int value = GetPixelValue(pixel, criterion);
            
            // Criterio: inizia segmento se valore > threshold
            bool shouldSort = (value > threshold);
            
            if (shouldSort && segmentStart == -1) {
                segmentStart = x; // Inizia nuovo segmento
            } 
            else if (!shouldSort && segmentStart != -1) {
                // Fine segmento, ordina
                int segmentEnd = x - 1;
                SortSegment(row, segmentStart, segmentEnd, 
                           bpp, criterion, maxLength);
                segmentStart = -1;
            }
        }
        
        // Ordina ultimo segmento se necessario
        if (segmentStart != -1) {
            SortSegment(row, segmentStart, width - 1, 
                       bpp, criterion, maxLength);
        }
    }
}

// Helper: Ordinamento verticale
void CImage::SortVertical(BYTE* pixels, UINT width, UINT height,
                         UINT stride, int bpp, int criterion,
                         int threshold, int maxLength)
{
    // Simile a orizzontale ma per colonne
    // Necessita buffer temporaneo per colonna
    std::vector<BYTE> columnBuffer(height * bpp);
    
    for (UINT x = 0; x < width; x++) {
        // Estrai colonna
        for (UINT y = 0; y < height; y++) {
            BYTE* src = pixels + y * stride + x * bpp;
            BYTE* dst = columnBuffer.data() + y * bpp;
            memcpy(dst, src, bpp);
        }
        
        // Trova e ordina segmenti nella colonna
        int segmentStart = -1;
        
        for (UINT y = 0; y < height; y++) {
            BYTE* pixel = columnBuffer.data() + y * bpp;
            int value = GetPixelValue(pixel, criterion);
            
            bool shouldSort = (value > threshold);
            
            if (shouldSort && segmentStart == -1) {
                segmentStart = y;
            }
            else if (!shouldSort && segmentStart != -1) {
                int segmentEnd = y - 1;
                SortSegment(columnBuffer.data(), segmentStart, segmentEnd,
                           bpp, criterion, maxLength);
                segmentStart = -1;
            }
        }
        
        if (segmentStart != -1) {
            SortSegment(columnBuffer.data(), segmentStart, height - 1,
                       bpp, criterion, maxLength);
        }
        
        // Ripristina colonna ordinata
        for (UINT y = 0; y < height; y++) {
            BYTE* src = columnBuffer.data() + y * bpp;
            BYTE* dst = pixels + y * stride + x * bpp;
            memcpy(dst, src, bpp);
        }
    }
}

// Helper: Ordina un segmento di pixel
void CImage::SortSegment(BYTE* row, int start, int end,
                        int bpp, int criterion, int maxLength)
{
    int segmentLength = end - start + 1;
    
    // Limita lunghezza se specificato
    if (maxLength > 0 && segmentLength > maxLength) {
        return; // Non ordinare segmenti troppo lunghi
    }
    
    // Raccogli pixel del segmento
    std::vector<std::pair<int, BYTE*>> pixelsWithValues;
    pixelsWithValues.reserve(segmentLength);
    
    for (int i = start; i <= end; i++) {
        BYTE* pixel = row + i * bpp;
        int value = GetPixelValue(pixel, criterion);
        pixelsWithValues.push_back(std::make_pair(value, pixel));
    }
    
    // Ordina per valore
    std::sort(pixelsWithValues.begin(), pixelsWithValues.end(),
              [](const std::pair<int, BYTE*>& a, 
                 const std::pair<int, BYTE*>& b) {
                  return a.first < b.first;
              });
    
    // Buffer per pixel ordinati
    std::vector<BYTE> sortedPixels(segmentLength * bpp);
    
    // Copia pixel ordinati
    for (int i = 0; i < segmentLength; i++) {
        memcpy(sortedPixels.data() + i * bpp, 
               pixelsWithValues[i].second, bpp);
    }
    
    // Ripristina nell'array originale
    for (int i = 0; i < segmentLength; i++) {
        memcpy(row + (start + i) * bpp,
               sortedPixels.data() + i * bpp, bpp);
    }
}

// Helper: Calcola valore del pixel per criterio
int CImage::GetPixelValue(BYTE* pixel, int criterion)
{
    switch (criterion) {
        case 0: // Luminosità (0-255)
            return (pixel[0] * 30 + pixel[1] * 59 + pixel[2] * 11) / 100;
            
        case 1: // Tonalità (Hue) approssimata
		{
            // Approssimazione hue da RGB
            int maxVal = max(pixel[0], max(pixel[1], pixel[2]));
            int minVal = min(pixel[0], min(pixel[1], pixel[2]));
            if (maxVal == minVal) return 0;
            
            float hue = 0;
            if (maxVal == pixel[0]) {
                hue = (float)(pixel[1] - pixel[2]) / (maxVal - minVal);
            } else if (maxVal == pixel[1]) {
                hue = 2.0f + (float)(pixel[2] - pixel[0]) / (maxVal - minVal);
            } else {
                hue = 4.0f + (float)(pixel[0] - pixel[1]) / (maxVal - minVal);
            }
            
            hue *= 60;
            if (hue < 0) hue += 360;
            return (int)(hue * 255 / 360);
		}
        case 2: // Saturazione (0-255)
		{
            int maxV = max(pixel[0], max(pixel[1], pixel[2]));
            int minV = min(pixel[0], min(pixel[1], pixel[2]));
            if (maxV == 0) return 0;
            return ((maxV - minV) * 255) / maxV;
		}
            
        case 3: // Valore (brightness) - max componente
            return max(pixel[0], max(pixel[1], pixel[2]));
            
        default:
            return (pixel[0] + pixel[1] + pixel[2]) / 3;
    }
}

/*
    Posterize()
*/
UINT CImage::Posterize(void)
{
	// ricava il valore del parametro
    int nFactor = 0;
    double nValue = 0;
    if(GetFilterParams("Posterize",nValue))
        nFactor = (int)nValue;
	else
	{
		SetLastErrorDescriptionEx("%s(): ERROR_INVALID_PARAMETER",__func__);
		return(ERROR_INVALID_PARAMETER);
	}
    
    // se livelli = 256, nessun effetto (numero ottimale x livelli: 8)
    if(nFactor==256)
        return(NO_ERROR);
    
    int width = GetWidth();
    int height = GetHeight();
    
    // tabella di lookup per velocizzare (come nell'algoritmo ottimizzato)
    BYTE posterizeTable[256];
    for(int i = 0; i < 256; i++)
    {
        // formula corretta per posterize:
        // 1 quantizza a nLevels-1 livelli (0..nLevels-1)
        // 2 ridistribuisce su 0..255
        int quantized = (i * (nFactor - 1) + 127) / 255;  // +127 per arrotondamento
        posterizeTable[i] = (BYTE)(quantized * 255 / (nFactor - 1));
    }
    
    // processa ogni pixel (stesso pattern di Brightness(), Contrast(), etc.)
    for(int y = 0; y < height; y++)
    {
        for(int x = 0; x < width; x++)
        {
            COLORREF c = GetPixel(x, y);
            
            // applica la posterizzazione a ogni canale
            int r = posterizeTable[GetRValue(c)];
            int g = posterizeTable[GetGValue(c)];
            int b = posterizeTable[GetBValue(c)];
            
            // usa RGB() in ordine corretto (R,G,B)
            c = RGB(r, g, b);
            SetPixel(x, y, c);
        }
    }

	UpdateHeaderInfo();    

    return(NO_ERROR);
}

/*
	Saturation()
*/
UINT CImage::Saturation(void)
{
	// ricava il valore del parametro
    int nFactor = 0;
    double nValue = 0;
    if(GetFilterParams("Saturation",nValue))
	    nFactor = (int)nValue;
	else
	{
		SetLastErrorDescriptionEx("%s(): ERROR_INVALID_PARAMETER",__func__);
		return(ERROR_INVALID_PARAMETER);
	}

    // -100 = completamente desaturato (scala grigi)
    //    0 = nessun cambio
    // +100 = saturazione raddoppiata (max 1.0)
    int width = GetWidth();
    int height = GetHeight();
    
    for(int y = 0; y < height; y++)
    {
        for(int x = 0; x < width; x++)
        {
            COLORREF c = GetPixel(x, y);
            double H, S, L;
            RGBtoHSL(c, &H, &S, &L);
            
            // converte nFactor (-100..100) a moltiplicatore
            // -100 -> moltiplicatore 0.0
            //    0 -> moltiplicatore 1.0  
            // +100 -> moltiplicatore 2.0
            double factor = 1.0 + (nFactor / 100.0);
            
            S = S * factor;
            
            // clamping (0.0 - 1.0)
            if(S < 0.0) S = 0.0;
            if(S > 1.0) S = 1.0;
            
            SetPixel(x, y, HLStoRGB(H, L, S));
        }
    }

	return(NO_ERROR);
}

/*
    Sharpen()

    Implementazione generica (Unsharp Mask).
*/
UINT CImage::Sharpen(void)
{
	// ricava il valore del parametro
    int nFactor = 0;
    double nValue = 0;
    if(GetFilterParams("Sharpen",nValue))
		nFactor = (int)nValue;
	else
	{
		SetLastErrorDescriptionEx("%s(): ERROR_INVALID_PARAMETER",__func__);
		return(ERROR_INVALID_PARAMETER);
	}
    
    // per intensita' 0, nessun effetto
    if(nFactor==0)
        return(NO_ERROR);
    
    int width = GetWidth();
    int height = GetHeight();
    
    if(width < 3 || height < 3)
        return(GDI_ERROR); // immagine troppo piccola
    
    // fattore intensita' (0.0 - 2.0)
    float factor = nFactor / 50.0f; // 50 = standard, 100 = doppio
    
    // crea buffer per immagine sfocata (blurred)
    std::vector<COLORREF> blurred(width * height);
    
    // prima passata: calcola immagine sfocata (blur gaussiano 3x3)
    for(int y = 0; y < height; y++)
    {
        for(int x = 0; x < width; x++)
        {
            // per i bordi, usa pixel originale (semplificazione)
            if(y == 0 || y == height-1 || x == 0 || x == width-1)
            {
                blurred[y * width + x] = GetPixel(x, y);
                continue;
            }
            
            // kernel blur gaussiano 3x3
            float b = 0, g = 0, r = 0;
            float weights[9] = {1, 2, 1, 2, 4, 2, 1, 2, 1};
            int idx = 0;
            
            for(int dy = -1; dy <= 1; dy++)
            {
                for(int dx = -1; dx <= 1; dx++)
                {
                    COLORREF pixel = GetPixel(x + dx, y + dy);
                    float weight = weights[idx++] / 16.0f;
                    
                    b += GetBValue(pixel) * weight;
                    g += GetGValue(pixel) * weight;
                    r += GetRValue(pixel) * weight;
                }
            }
            
            blurred[y * width + x] = RGB(
				(BYTE)min(max((int)(r + 0.5f), 0), 255),
				(BYTE)min(max((int)(g + 0.5f), 0), 255),
				(BYTE)min(max((int)(b + 0.5f), 0), 255)
            );
        }
    }
    
    // seconda passata: applica unsharp mask
    for(int y = 1; y < height - 1; y++)
    {
        for(int x = 1; x < width - 1; x++)
        {
            COLORREF orig = GetPixel(x, y);
            COLORREF blur = blurred[y * width + x];
            
            // formula: sharpened = original + (original - blurred) * factor
            int newR = GetRValue(orig) + (int)((GetRValue(orig) - GetRValue(blur)) * factor);
            int newG = GetGValue(orig) + (int)((GetGValue(orig) - GetGValue(blur)) * factor);
            int newB = GetBValue(orig) + (int)((GetBValue(orig) - GetBValue(blur)) * factor);
            
            // clamping
            newR = min(max(newR, 0), 255);
            newG = min(max(newG, 0), 255);
            newB = min(max(newB, 0), 255);
            
            SetPixel(x, y, RGB(newR, newG, newB));
        }
    }
    
    return(NO_ERROR);
}

/*
	HuetoRGB()
*/
/* STATIC LOCAL */double CImage::HuetoRGB(double m1,double m2,double h)
{
	if(h < 0)
		h += 1.0;
	if(h > 1)
		h -= 1.0;
	if((6.0f * h) < 1)
		return(m1 + (m2 - m1) * h * 6.0f);
	if((2.0f * h) < 1)
		return(m2);
	if((3.0f * h) < 2.0f)
		return(m1 + (m2 - m1) * ((2.0f / 3.0f) - h) * 6.0f);

	return(m1);
}

/*
	HLStoRGB()
*/
/* STATIC LOCAL */COLORREF CImage::HLStoRGB(const double& H,const double& L,const double& S)
{
	double r,g,b;
	double m1,m2;

	if(S==0)
	{
		r = g = b = L;
	} 
	else 
	{
		if(L <= 0.5f)
			m2 = L * (1.0f + S);
		else
			m2 = L + S - L * S;
		
		m1 = 2.0f * L - m2;
		
		r = HuetoRGB(m1,m2,H + 1.0f / 3.0f);
		g = HuetoRGB(m1,m2,H);
		b = HuetoRGB(m1,m2,H - 1.0f / 3.0f);
	}

	return(RGB((BYTE)(r*255),(BYTE)(g*255),(BYTE)(b*255)));		// RGB
//	return(RGB((BYTE)(b*255),(BYTE)(g*255),(BYTE)(r*255)));		// BGR
}

/*
	RGBtoHSL()
*/
/* STATIC LOCAL */void CImage::RGBtoHSL(COLORREF rgb,double* H,double* S,double* L)
{
	double delta;
	double r = (double)GetRValue(rgb) / 255;
	double g = (double)GetGValue(rgb) / 255;
	double b = (double)GetBValue(rgb) / 255;
	double cmax = max(r,max(g,b));
	double cmin = min(r,min(g,b));
	*L = (cmax + cmin) / 2.0f;
	
	if(cmax==cmin) 
	{
		*S = 0;
		*H = 0;
	} 
	else 
	{
		if(*L < 0.5f) 
			*S = (cmax - cmin) / (cmax + cmin);
		else
			*S = (cmax - cmin) / (2.0f - cmax - cmin);
		
		delta = cmax - cmin;
		
		if(r==cmax)
			*H = (g - b) / delta;
		else if(g==cmax)
			*H = 2.0f + (b - r) / delta;
		else
			*H = 4.0f + (r - g) / delta;
		
		*H /= 6.0f;
		
		if(*H < 0.0f)
			*H += 1;
	}
}

/*
	1. True Grayscale (8 bpp) -> si, e' bianco e nero
		1 canale (luminosita')
		256 livelli di grigio (0=nero, 255=bianco)
		questo e' il "vero" grayscale

	2. Grayscale in RGB (24/32 bpp) -> Appare bianco e nero ma tecnicamente non lo e'
		3 canali (R, G, B)
		ma R=G=B per ogni pixel
		cccupa 3-4x piu' memoria
		Comune quando si converte da colori a B/N senza ridurre i bpp

	3. Palette-based (8 bpp) -> puo' essere B/N o colori
		Usa una palette (CLUT)
		Se la palette ha solo grigi -> e' B/N
		Se la palette ha colori -> e' colori

	Quindi: 8 bpp -> sicuramente grayscale strutturale, ma immagini RGB con R=G=B sono 
	visivamente B/N anche se a 24/32 bpp.

	8 BPP (256 colori)
	PUO' essere a colori - se la palette contiene colori (es. GIF a 256 colori)

	PUO' essere in bianco e nero - se la palette contiene solo grigi (R=G=B)

	Esempio concreto: Una GIF con una foto a colori ridotta a 256 colori e' 8 BPP ed a 
	colori.

	24/32 BPP (True Color)
	Generalmente a colori - ogni canale R, G, B puo' variare indipendentemente

	PUO' essere visivamente in bianco e nero - se tutti i pixel hanno R=G=B

	Esempio: Una foto RGB convertita in bianco e nero ma mantenuta come formato RGB 
	(24 BPP) e' 24 BPP ma visivamente B/N.

	Ricapitolando:

	BPP	Puo' essere a colori?	Puo' essere B/N?
	1	No (solo 2 colori)		Sì (bianco/nero)
	4	Sì (16 colori)			Sì (16 grigi)
	8	Sì (256 colori)			Sì (256 grigi)
	24	Sì (16.7M colori)		Sì (se R=G=B)
	32	Sì (+alpha)				Sì (se R=G=B)

	Quindi:

	8 BPP NON garantisce che sia B/N -> dipende dalla palette
	24/32 BPP NON garantisce che sia a colori -> potrebbero avere R=G=B

	Per questo servono due funzioni diverse:
	IsTrueGrayscale() -> Verifica se strutturalmente è in scala di grigi (palette di soli grigi)
	IsVisualGrayscale() -> Verifica se visivamente appare in bianco e nero (R=G=B anche su 24/32 bpp)

	Esempio pratico:
	Una foto B/N salvata come JPEG (24 bpp) -> IsTrueGrayscale() = FALSE, IsVisualGrayscale() = TRUE
	Una GIF a 256 colori (8 bpp) -> IsTrueGrayscale() = FALSE, IsVisualGrayscale() = dipende dall'immagine

*/

/*
	IsTrueGrayscale()

	Verifica SOLO verifica l'aspetto strutturale.
*/
/* LOCAL */BOOL CImage::IsTrueGrayscale(void)
{
    int bpp = GetBPP();
    
    // solo immagini con palette possono essere "true grayscale"
    if(bpp==24 ||bpp==32)
        return(FALSE);
    
	LPBITMAPINFO pBitmapInfo = GetBMI();

    // ottiene la palette
    RGBQUAD* palette = pBitmapInfo->bmiColors; //GetPalette();
    if(!palette)
        return(FALSE);
    
    int paletteSize = GetNumColors();
    if(paletteSize <= 0)
        paletteSize = 1 << bpp;
    
    // campiona punti strategici nella palette
    if(palette[0].rgbRed!=palette[0].rgbGreen || palette[0].rgbRed!=palette[0].rgbBlue)
        return(FALSE);
    
    // ultimo colore  
    if(paletteSize > 1)
    {
        RGBQUAD last = palette[paletteSize - 1];
        if(last.rgbRed!=last.rgbGreen || last.rgbRed!=last.rgbBlue)
	        return(FALSE);
    }
    
    // colore centrale
    if(paletteSize > 2)
    {
        RGBQUAD mid = palette[paletteSize / 2];
        if(mid.rgbRed!=mid.rgbGreen || mid.rgbRed!=mid.rgbBlue)
	        return(FALSE);
    }

    // campioni random (solo per palette grandi)
    if(paletteSize > 8)
    {
        for(int i = 0; i < 3; i++)
        {
            int idx = fast_rand_range(paletteSize);
            if(palette[idx].rgbRed!=palette[idx].rgbGreen || palette[idx].rgbRed!=palette[idx].rgbBlue)
		        return(FALSE);
        }
    }
    
    return(TRUE); // i campionamenti sono tutti R=G=B
}

/*
	IsVisualGrayscale()

	Verifica SOLO l'aspetto visivo
*/
/* LOCAL */BOOL CImage::IsVisualGrayscale(void)
{
    if(GetBPP() < 24)
		return(TRUE); // 1,4,8 bpp = certamente grayscale
    
    int w = GetWidth();
    int h = GetHeight();
    
    // punti strategici
    int center_x = w / 2;
    int center_y = h / 2;
    int rand_x = fast_rand_range(w);
    int rand_y = fast_rand_range(h);

	// se coordinate fuori range, clamping
	if(rand_x >= w)
		rand_x = w - 1;
	if(rand_y >= h)
		rand_y = h - 1;
    
    // centro
    COLORREF c1 = GetPixel(center_x,center_y);
    // angolo alto sinistro
    COLORREF c2 = GetPixel(0,0);
    // angolo basso destro
    COLORREF c3 = GetPixel(w - 1,h - 1);
    // punto casuale
    COLORREF c4 = GetPixel(rand_x,rand_y);
    
    // check esatto (R == G == B)
    if(	(GetRValue(c1)!=GetGValue(c1) || GetRValue(c1)!=GetBValue(c1)) ||
		(GetRValue(c2)!=GetGValue(c2) || GetRValue(c2)!=GetBValue(c2)) ||
		(GetRValue(c3)!=GetGValue(c3) || GetRValue(c3)!=GetBValue(c3)) ||
		(GetRValue(c4)!=GetGValue(c4) || GetRValue(c4)!=GetBValue(c4)) )
		return(FALSE);
    
    return(TRUE);
}

/*
	IsVisualGrayscaleStatistical()

	- numero di pixel su cui effettuare il check
	- tolleranza
	- percentuale da soddisfare

	Tolleranza:
	la differenza massima consentita tra i canali R, G, B affinche' un pixel sia considerato "grayscale"
	matematicamente: un pixel con colore RGB(r, g, b) e' considerato grayscale se:
		abs(r - g) <= tolerance
		AND
		abs(r - b) <= tolerance
		AND
		abs(g - b) <= tolerance
	Esempio:
		con tolleranza 3:
			RGB(100, 100, 100)	-> differenza 0 -> GRAY
			RGB(100, 102, 101)	-> Max differenza 2 -> GRAY
			RGB(100, 100, 95)	-> differenza 5 -> COLOR (bluastro)
			RGB(100, 95, 100)	-> differenza 5 -> COLOR (verdastro)
		con tolleranza 10:
			RGB(100, 100, 100)	-> GRAY
			RGB(100, 108, 102)	-> max diff 8 -> GRAY
			RGB(250, 255, 245)	-> max diff 10 -> GRAY (bianco "sporco")
			RGB(100, 85, 100)	-> max diff 15 -> COLOR
	occhio pero' perche i JPEG hanno artefatti di compressione, per cui:
			bianco originale: RGB(255, 255, 255)
			dopo compressione JPEG: RGB(252, 254, 250) (differenze fino a 4-5)
			con tolleranza = 3: COLOR (falso positivo)
			con tolleranza = 10: GRAY (corretto)
	Valori tipici consigliati:
		Tollerenza	Uso tipico				Pro										Contro
		0-3			Immagini PNG/lossless	Rileva piccole variazioni di colore		Troppi falsi positivi con JPEG
		5-8			Generico				Bilanciato								Puo' mancare colori molto tenui
		10-15		JPEG/compresso			Tollerante ad artefatti					Considera colori tenui come grayscale
		>15			Solo B/N evidente		Molto tollerante						Molti falsi negativi
	Configurazione "ottimale":
		IsVisualGrayscaleStatistical(
			100,    // maxPixelsToCheck - buon campionamento
			10,     // tolerance - perfetto per JPEG
			99.5f   // grayThresholdPercent - stringente (solo 0.5% pixel colorati consentiti)
		);
		ossia:	"Se almeno il 99.5% dei 100 pixel campionati hanno canali R,G,B che differiscono al massimo di 10 unita', 
				allora l'immagine e' visualmente grayscale."
	Note:
		L'occhio umano non nota differenze < 5-10 in valore RGB
		Una differenza di 10 su 255 e' circa 4% -> quasi impercettibile
		Quindi tolleranza = 10 corrisponde bene alla percezione visiva

	Percentuale:
	le soglia per la quale se i pixel campionati sono grayscale (entro la tolleranza), allora l'immagine 
	e' considerata visualmente grayscale:
		grayThresholdPercent = 95.0f ->	"Se almeno 95 su 100 dei pixel campionati sono grayscale (con - o + 10 di differenza, 
										osia tolleranza, R/G/B), allora l'immagine e' grayscale."
		scenario A): 97 pixel grayscale, 3 colorati -> TRUE (97% >= 95%)
		scenario B): 94 pixel grayscale, 6 colorati -> FALSE (94% < 95%)
	per cui la configurazione "ottimale" di cui sopra va letta come:
		"Su 100 pixel campionati, se almeno 99.5% (quasi tutti) hanno canali R/G/B che differiscono al massimo di 10 unita', 
		allora l'immagine appare all'occhio umano come in scala di grigi."
*/
/*
// per uso generale:
BOOL isGray = image.IsVisualGrayscaleStatistical(200, 5, 97.0f);
// 200 pixel totali, tolleranza 5, soglia 97%

// per controllo pi'u stringente (meno falsi positivi):
BOOL isGrayStrict = image.IsVisualGrayscaleStatistical(500, 3, 99.0f);
// 500 pixel, tolleranza 3, soglia 99%

// per controllo veloce (ma meno accurato):
BOOL isGrayFast = image.IsVisualGrayscaleStatistical(50, 8, 90.0f);
// 50 pixel, tolleranza 8, soglia 90%
*/
/* LOCAL */BOOL CImage::IsVisualGrayscaleStatistical(int maxPixelsToCheck /*= 100*/,int tolerance /*= 3*/,float grayThresholdPercent /*= 95.0f*/)
{
    int w = GetWidth();
    int h = GetHeight();
    
    if (w <= 0 || h <= 0) 
        return true;
    
    // Funzione per verificare se un colore è grayscale
    auto IsGrayWithTolerance = [tolerance](COLORREF c) -> bool {
        int r = GetRValue(c);
        int g = GetGValue(c);
        int b = GetBValue(c);
        return (abs(r - g) <= tolerance) && 
               (abs(r - b) <= tolerance) &&
               (abs(g - b) <= tolerance);
    };
    
    int totalPixelsChecked = 0;
    int grayPixelsFound = 0;
    
    // 1. PUNTI STRATEGICI FISSI (importanti per pattern regolari)
    // Angoli
    COLORREF corners[4];
    corners[0] = GetPixel(0, 0);                     // Alto sinistro
    corners[1] = GetPixel(w - 1, 0);                 // Alto destro
    corners[2] = GetPixel(0, h - 1);                 // Basso sinistro
    corners[3] = GetPixel(w - 1, h - 1);             // Basso destro
    
    // Centro e quarti
    COLORREF centers[5];
    centers[0] = GetPixel(w / 2, h / 2);             // Centro
    centers[1] = GetPixel(w / 4, h / 4);             // Quarto alto sinistro
    centers[2] = GetPixel(3 * w / 4, h / 4);         // Quarto alto destro
    centers[3] = GetPixel(w / 4, 3 * h / 4);         // Quarto basso sinistro
    centers[4] = GetPixel(3 * w / 4, 3 * h / 4);     // Quarto basso destro
    
    // Conta punti strategici
    for (int i = 0; i < 4; i++) {
        totalPixelsChecked++;
        if (IsGrayWithTolerance(corners[i])) grayPixelsFound++;
    }
    
    for (int i = 0; i < 5; i++) {
        totalPixelsChecked++;
        if (IsGrayWithTolerance(centers[i])) grayPixelsFound++;
    }
    
    // 2. PUNTI RANDOM (per catturare aree sparse)
    int randomPixelsToCheck = maxPixelsToCheck - totalPixelsChecked;
    if (randomPixelsToCheck < 10) randomPixelsToCheck = 10; // Minimo 10 punti random
    
    for (int i = 0; i < randomPixelsToCheck; i++) {
        int x = fast_rand_range(w);
        int y = fast_rand_range(h);
//        int x = xorshift32() % w;
//		int y = xorshift32() % h;
        
        COLORREF c = GetPixel(x, y);
//printf("%ld %d,%d\n",c,x,y);
        totalPixelsChecked++;
        if (IsGrayWithTolerance(c)) grayPixelsFound++;
        
        // Ottimizzazione: se troviamo abbastanza colori presto, possiamo fermarci
        float currentGrayPercent = (float)grayPixelsFound * 100.0f / totalPixelsChecked;
        float colorPercent = 100.0f - currentGrayPercent;

// controlla quando e come uscire dal ciclo, effetto Moebius

		// VECCHIO (troppo aggressivo):
        // Se abbiamo già trovato abbastanza colori per dire che NON è grayscale
//        if (colorPercent > (100.0f - grayThresholdPercent)) {
//            return FALSE; // Non è grayscale
//        }

		// NUOVO (più conservativo - solo dopo molti pixel):
//		if (totalPixelsChecked > 20 && colorPercent > (100.0f - grayThresholdPercent)) {
//			// Solo dopo 20 pixel controllati
//			return FALSE;
//		}

		// OPPURE (ancora meglio - soglia più alta per early exit):
		if (colorPercent > 20.0f) { // Se >20% dei pixel campionati sono colorati
			return FALSE;           // allora sicuramente non è grayscale
		}        

    }
    
    // 3. CALCOLO PERCENTUALE FINALE
    float grayPercent = (float)grayPixelsFound * 100.0f / totalPixelsChecked;

    return (grayPercent >= grayThresholdPercent);
}

/*
	NotImplemented()
*/
UINT CImage::NotImplemented(LPCSTR lpcszMethod)
{
#ifdef _DEBUG
	char buffer[256] = {0};
	snprintf(buffer,sizeof(buffer),"The method %s() is not implemented.",lpcszMethod);
	::MessageBox(NULL,buffer,"CImage()",MB_OK|MB_ICONWARNING|MB_TASKMODAL|MB_SETFOREGROUND|MB_TOPMOST);
#endif
	return(GDI_ERROR);
}

/*------------------------------------------------------------------------------------  TEST  ------------------------------------------------------------------------------------------*/

#define TEST_EQRGB 1

UINT CImage::Test(void)
{
#ifdef TEST_EQRGB
	EqualizeRGBIntensity(0.8); //EqualizeRGB();
#endif

	return(0);
}
