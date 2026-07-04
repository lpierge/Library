/*$
	CSplashWindow.cpp
	Visualizza uno splash screen a dissolvenza con l'immagine specificata.
	Luca Piergentili, Marzo '26

	Vedi le note in CSplashWindow.h
*/
#include "env.h"
#include "pragma.h"
#include "macro.h"
#include "window.h"

#if defined(_AFX) || defined(_AFXDLL)

// include l'header <atlimage.h> SOLO qui (NON nel .h), evitando problemi di ambiguita' con i nomi dei simboli ATL
#include <atlimage.h> 
#include "CSplashWindow.h"

// definisce la struttura reale che contiene la CImage di Microsoft
struct CSplashWindowImpl
{
    ATL::CImage mfcImage;
};

BEGIN_MESSAGE_MAP(CSplashWindow,CWnd)
	ON_WM_TIMER()
	ON_WM_PAINT()
    ON_WM_LBUTTONDOWN()
    ON_WM_KEYDOWN()
    ON_WM_ERASEBKGND()
END_MESSAGE_MAP()

/*
	CSplashWindow()
*/
CSplashWindow::CSplashWindow() : m_dwVisibleMs(3000),m_dwFadeMs(2000),m_nCurrentAlpha(255)
{
	// alloca l'implementazione nascosta
	m_pImpl = new CSplashWindowImpl();
}

/*
	~CSplashWindow()
*/
CSplashWindow::~CSplashWindow()
{
	// deve rilasciare l'implementazione
	if(m_pImpl)
	{
		if(!m_pImpl->mfcImage.IsNull())
			m_pImpl->mfcImage.Destroy();
		delete m_pImpl;
	}
}

/*
	LoadFromFile()

	Carica l'immagine dal file.
	Nella versione ATL omette il caricamento da risorse, come invece fa nella versione SDK.
*/
BOOL CSplashWindow::LoadFromFile(LPCSTR lpszPath)
{
	// se non esiste l'implementazione, errore
	// se gia' caricata, la scarica prima di ricaricare il file
	if(!m_pImpl)
		return(FALSE);
	if(!m_pImpl->mfcImage.IsNull())
		m_pImpl->mfcImage.Destroy();

	// la CImage di MFC carica nativamente PNG, JPG, BMP usando GDI+ sottobanco
	HRESULT hr = m_pImpl->mfcImage.Load(lpszPath);
    
	if(SUCCEEDED(hr))
	{
		// barbatrucco x MFC: se l'immagine ha un canale alpha (32-bit), bisogna forzare
		// MFC a interpretare i pixel come pre-moltiplicati (BGRA), altrimenti GDI+ e MFC
		// entrano in conflitto nel thread ed invertono i canali
		if(m_pImpl->mfcImage.GetBPP()==32)
		{
			for(int y=0; y < m_pImpl->mfcImage.GetHeight(); y++)
			{
				for(int x=0; x < m_pImpl->mfcImage.GetWidth(); x++)
				{
					// forza il formato corretto pixel per pixel nel buffer interno di MFC
					BYTE* pPixel = (BYTE*)m_pImpl->mfcImage.GetPixelAddress(x,y);

					// pPixel[0] = blu, pPixel[1] = verde, pPixel[2] = rosso, pPixel[3] = alpha
					// GDI+ scrive in RGBA, ma la CImage di MFC su Windows x86 vuole BGRA
					// fa lo swap preventivo per blindare il risultato:
					BYTE temp = pPixel[0];
					pPixel[0] = pPixel[2];
					pPixel[2] = temp;
				}
			}
		}

		return(TRUE);
	}

	return(FALSE);
}

/*
	SetTimings()
*/
void CSplashWindow::SetTimings(DWORD dwVisibleMs,DWORD dwFadeMs)
{
	m_dwVisibleMs = dwVisibleMs;
	m_dwFadeMs = dwFadeMs;
}

/*
	CreateAndShow()
*/
BOOL CSplashWindow::CreateAndShow(void)
{
	if(!m_pImpl || m_pImpl->mfcImage.IsNull())
		return(FALSE);

	int nW = m_pImpl->mfcImage.GetWidth();
	int nH = m_pImpl->mfcImage.GetHeight();

	int screenW = ::GetSystemMetrics(SM_CXSCREEN);
	int screenH = ::GetSystemMetrics(SM_CYSCREEN);
	int x = (screenW - nW) / 2;
	int y = (screenH - nH) / 2;

	BOOL bCreated = CreateEx(WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW,AfxRegisterWndClass(0),"CalimeroFlyer",WS_POPUP | WS_VISIBLE,x,y,nW,nH,NULL,NULL);

	if(bCreated)
	{
		SetLayeredWindowAttributes(0,255,LWA_ALPHA);
		m_dwStartTime = ::GetTickCount();
		ShowWindow(SW_SHOW);
		UpdateWindow();
		SetTimer(1,30,NULL);
	}

	return(bCreated);
}

/*
	OnTimer()
*/
void CSplashWindow::OnTimer(UINT_PTR nIDEvent)
{
	DWORD dwElapsed = ::GetTickCount() - m_dwStartTime;

	if(dwElapsed < m_dwVisibleMs)
		return;

	DWORD dwFadeElapsed = dwElapsed - m_dwVisibleMs;
	if(dwFadeElapsed >= m_dwFadeMs)
	{
		KillTimer(1);
		DestroyWindow();
		return;
	}

	double ratio = (double)dwFadeElapsed / m_dwFadeMs;
	m_nCurrentAlpha = (BYTE)(255 * (1.0 - (ratio * ratio)));

	SetLayeredWindowAttributes(0,m_nCurrentAlpha,LWA_ALPHA);
}

/*
	OnPaint()
*/
void CSplashWindow::OnPaint(void)
{
	CPaintDC dc(this); // context DC nativo di MFC
    
	// il metodo Draw di CImage (MFC) accetta direttamente l'HDC del PaintDC
	if(m_pImpl && !m_pImpl->mfcImage.IsNull())
		m_pImpl->mfcImage.Draw(dc.GetSafeHdc(),0,0);
}

#else

#include "CImage.h"
#include "CSplashWindow.h"

BEGIN_MESSAGE_MAP(CSplashWindow, CWnd)
    ON_WM_TIMER()
    ON_WM_PAINT()
    ON_WM_LBUTTONDOWN()
    ON_WM_KEYDOWN()
    ON_WM_ERASEBKGND()
END_MESSAGE_MAP()

/*
	CSplashWindow()
*/
CSplashWindow::CSplashWindow(CImage* pImage) : m_pImage(pImage),m_dwVisibleMs(3000),m_dwFadeMs(2000),m_nCurrentAlpha(255),m_bIsFading(FALSE)
{
}

/*
	~CSplashWindow()
*/
CSplashWindow::~CSplashWindow()
{
}

/*
	LoadFromFile()

	Carica l'immagine da file, usando l'oggetto CImage.
*/
BOOL CSplashWindow::LoadFromFile(LPCSTR lpszPath)
{
	DWORD dwError = 0L;
	return(m_pImage->Load(lpszPath,dwError));
}

/*
	LoadFromResource()

	Carica l'immagine dalle risorse, implementata solo qui nella versione SDK, non in quella ATL.
*/
BOOL CSplashWindow::LoadFromResource(UINT nResID)
{
	// carica il DIB da una risorsa BITMAP
	HINSTANCE hInst = AfxGetResourceHandle();
	HRSRC hRes = ::FindResource(hInst,MAKEINTRESOURCE(nResID),RT_BITMAP);
	if(!hRes)
		return(FALSE);

	HGLOBAL hGlobal = ::LoadResource(hInst,hRes);
	if(!hGlobal)
		return(FALSE);

	LPVOID pData = ::LockResource(hGlobal);
	if(!pData)
		return(FALSE);

	// copia i dati della risorsa in un handle globale per sicurezza
	DWORD dwSize = ::SizeofResource(hInst,hRes);
	HGLOBAL hDibCopy = ::GlobalAlloc(GHND,dwSize);
	if(hDibCopy)
	{
		LPVOID pCopy = ::GlobalLock(hDibCopy);
		memcpy(pCopy,pData,dwSize);
		::GlobalUnlock(hDibCopy);
		BOOL bResult = m_pImage->SetDIB((HDIB)hDibCopy,-1);
		::GlobalFree(hDibCopy);
		return(bResult);
	}

	return(FALSE);
}

/*
	SetTimings()
*/
void CSplashWindow::SetTimings(DWORD dwVisibleMs,DWORD dwFadeMs)
{
	m_dwVisibleMs = dwVisibleMs;
	m_dwFadeMs = dwFadeMs;
}

/*
	CreateAndShow()
*/
BOOL CSplashWindow::CreateAndShow(void)
{
	int nW = m_pImage->GetWidth();
	int nH = abs((long)m_pImage->GetHeight());

	// centra sullo schermo
	int screenW = ::GetSystemMetrics(SM_CXSCREEN);
	int screenH = ::GetSystemMetrics(SM_CYSCREEN);
	int x = (screenW - nW) / 2;
	int y = (screenH - nH) / 2;

	// crea finestra Layered e TopMost
	BOOL bCreated = CreateEx(	WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
								AfxRegisterWndClass(0),
								"CalimeroFlyer",
								WS_POPUP | WS_VISIBLE,
								x,y,nW,nH,NULL,NULL);

	if(bCreated)
	{
		SetLayeredWindowAttributes(0,255,LWA_ALPHA);
		m_dwStartTime = GetTickCount();
		ShowWindow(SW_SHOW);
		UpdateWindow();
		SetTimer(IDT_CHECK_FADE,30,NULL); // controllo fluido ogni 30ms
	}

	return bCreated;
}

/*
	OnTimer()
*/
void CSplashWindow::OnTimer(UINT_PTR nIDEvent)
{
	DWORD dwElapsed = ::GetTickCount() - m_dwStartTime;

	if(dwElapsed < m_dwVisibleMs)
		return; // sta' nell'intervallo di piena visibilita'

	// calcolo x dissolvenza
	DWORD dwFadeElapsed = dwElapsed - m_dwVisibleMs;
	if(dwFadeElapsed >= m_dwFadeMs)
	{
		KillTimer(IDT_CHECK_FADE);
		DestroyWindow();
		return;
	}

#if 0
	// proporzione lineare: alpha va da 255 a 0
	m_nCurrentAlpha = (BYTE)(255 - (255 * dwFadeElapsed / m_dwFadeMs));
#else
	// non lineare, usa un decadimento piu' suave
	double ratio = (double)dwFadeElapsed / m_dwFadeMs;
	m_nCurrentAlpha = (BYTE)(255 * (1.0 - (ratio * ratio))); // decadimento parabolico
#endif

	SetLayeredWindowAttributes(0,m_nCurrentAlpha,LWA_ALPHA);
}

/*
	OnPaint()
*/
void CSplashWindow::OnPaint(void)
{
	CPaintDC dc(this);
    
	// disegna l'immagine dell'oggetto m_pImage sul DC della finestra
	if(m_pImage)
		m_pImage->Draw(dc.GetSafeHdc(),0,0);
}

#endif
