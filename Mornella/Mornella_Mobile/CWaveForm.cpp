/**
*	CWaveform
*	I/O class to manage wave in/out api
**/
#include "CWaveform.h"

///////////////////////////////////////////////////////////
// Waveform common class
///////////////////////////////////////////////////////////
CWaveform::CWaveform()
{
}

CWaveform::~CWaveform()
{
}

void CALLBACK CWaveform::waveInProc(HWAVEIN hwi, UINT uMsg, DWORD dwInstance, WAVEHDR *dwParam1, DWORD dwParam2)
{
	CWaveformIn *instance = reinterpret_cast<CWaveformIn *>(dwInstance);

	instance->waveInProc(uMsg, dwParam1, dwParam2);
}

void CALLBACK CWaveform::waveOutProc(HWAVEOUT hwo, UINT uMsg, DWORD dwInstance, WAVEHDR *dwParam1, DWORD dwParam2)
{
	CWaveformOut *instance = reinterpret_cast<CWaveformOut *>(dwInstance);

	instance->waveOutProc(uMsg, dwParam1, dwParam2);
}


CWaveformIn* CWaveform::InOpen(UINT uDeviceID, LPWAVEFORMATEX pwfx, DWORD dwCallback, DWORD dwInstance, DWORD fdwOpen)
{
	HWAVEIN hWaveIn = NULL;

	if (waveInOpen(&hWaveIn, uDeviceID, pwfx, dwCallback, dwInstance, fdwOpen) == MMSYSERR_NOERROR)
	{
		CWaveformIn *waveFormIn = new CWaveformIn(hWaveIn);

		return waveFormIn;
	}

	return NULL;
}

CWaveformIn*  CWaveform::InOpen(UINT uDeviceID, LPWAVEFORMATEX pwfx, DWORD fdwOpen)
{
	HWAVEIN hWaveIn = NULL;
	CWaveformIn *waveFormIn = new CWaveformIn(NULL);	// alloc object

	if (waveInOpen(&hWaveIn, uDeviceID, pwfx, (DWORD_PTR)&CWaveform::waveInProc, (DWORD_PTR) waveFormIn, fdwOpen) == MMSYSERR_NOERROR)
	{
		waveFormIn->setHandle(hWaveIn);
		return waveFormIn;
	}

	delete waveFormIn;	// destroy unmanaged object
	return NULL;
}

CWaveformOut* CWaveform::OutOpen(UINT uDeviceID, LPWAVEFORMATEX pwfx, DWORD dwCallback, DWORD dwInstance, DWORD fdwOpen)
{
	HWAVEOUT hWaveOut = NULL;

	if (waveOutOpen(&hWaveOut, uDeviceID, pwfx, dwCallback, dwInstance, fdwOpen) == MMSYSERR_NOERROR)
	{
		CWaveformOut *waveFormOut = new CWaveformOut(hWaveOut);

		return waveFormOut;
	}

	return NULL;
}

CWaveformOut* CWaveform::OutOpen(UINT uDeviceID, LPWAVEFORMATEX pwfx, DWORD fdwOpen)
{
	HWAVEOUT hWaveOut = NULL;
	CWaveformOut *waveFormOut = new CWaveformOut(NULL);

	if (waveOutOpen(&hWaveOut, uDeviceID, pwfx, (DWORD_PTR)&CWaveform::waveOutProc, (DWORD_PTR) waveFormOut, fdwOpen) == MMSYSERR_NOERROR)
	{
		waveFormOut->setHandle(hWaveOut);

		return waveFormOut;
	}

	delete waveFormOut;
	return NULL;

}


void CWaveform::PrepareWaveFormat(LPWAVEFORMATEX pWaveFormat, WORD wFormatTag, WORD nChannels, DWORD nSamplesPerRec, WORD wBitsPerSample)
{
	pWaveFormat->cbSize = 0;
	pWaveFormat->wFormatTag = wFormatTag;
	pWaveFormat->nChannels = nChannels;
	pWaveFormat->nSamplesPerSec = nSamplesPerRec;
	pWaveFormat->wBitsPerSample = wBitsPerSample;
	pWaveFormat->nBlockAlign = (wBitsPerSample/8)* nChannels;
	pWaveFormat->nAvgBytesPerSec = nSamplesPerRec*pWaveFormat->nBlockAlign;
}


UINT CWaveform::OutGetNumDevs()
{
	return waveOutGetNumDevs();
}


BOOL CWaveform::InGetDevCaps(UINT uDeviceID, LPWAVEINCAPS pwic, UINT cbwic)
{
	if (waveInGetDevCaps(uDeviceID, pwic, cbwic) == MMSYSERR_NOERROR)
		return TRUE;

	return FALSE;
}

BOOL CWaveform::InGetErrorText(MMRESULT mmError, LPTSTR pszText, UINT cchText)
{
	if (waveInGetErrorText(mmError, pszText, cchText) == MMSYSERR_NOERROR)
		return TRUE;

	return FALSE;
}

UINT CWaveform::InGetNumDevs()
{
	return waveInGetNumDevs();
}

BOOL CWaveform::OutGetDevCaps(UINT uDeviceID, LPWAVEOUTCAPS pwoc, UINT cbwoc)
{
	if ( waveOutGetDevCaps(uDeviceID, pwoc, cbwoc) == MMSYSERR_NOERROR)
		return TRUE;

	return FALSE;
}

BOOL CWaveform::OutGetErrorText(UINT mmError, LPTSTR pszText, UINT cchText)
{
	if ( waveOutGetErrorText(mmError, pszText, cchText) == MMSYSERR_NOERROR)
		return TRUE;

	return FALSE;
}


///////////////////////////////////////////////////////////
// Wave In class
///////////////////////////////////////////////////////////
CWaveformIn::CWaveformIn(HWAVEIN hwi)
:	m_hwi(hwi)
{
}

CWaveformIn::~CWaveformIn()
{
}

void CWaveformIn::setHandle(HWAVEIN hwi)
{
	m_hwi = hwi;
}

BOOL CWaveformIn::AddBuffer(LPWAVEHDR pwh, UINT cbwh)
{
	if (waveInAddBuffer(m_hwi, pwh, cbwh) == MMSYSERR_NOERROR)
		return TRUE;

	return FALSE;
}

// Close method
BOOL CWaveformIn::Close()
{
	if (m_hwi != NULL)
		if (waveInClose(m_hwi) == MMSYSERR_NOERROR)
		{
			m_hwi = NULL;
			return TRUE;
		}

		return FALSE;
}

BOOL CWaveformIn::GetID(PUINT puDeviceID)
{
	if (waveInGetID(m_hwi, puDeviceID) == MMSYSERR_NOERROR)
		return TRUE;

	return FALSE;
}


BOOL CWaveformIn::GetPosition(LPMMTIME pmmt, UINT cbmmt)
{
	if (waveInGetPosition(m_hwi, pmmt, cbmmt) == MMSYSERR_NOERROR)
		return TRUE;

	return FALSE;
}


BOOL CWaveformIn::Message(UINT uMsg, DWORD dw1, DWORD dw2)
{
	if (waveInMessage(m_hwi, uMsg, dw1, dw2) == MMSYSERR_NOERROR)
		return TRUE;

	return FALSE;
}


BOOL CWaveformIn::PrepareHeader(LPWAVEHDR pwh, UINT cbwh)
{
	if (waveInPrepareHeader(m_hwi, pwh, cbwh) == MMSYSERR_NOERROR)
		return TRUE;

	return FALSE;
}


BOOL CWaveformIn::Reset()
{
	if (waveInReset(m_hwi) == MMSYSERR_NOERROR)
		return TRUE;

	return FALSE;
}

BOOL CWaveformIn::Start()
{
	if (waveInStart(m_hwi) == MMSYSERR_NOERROR)
		return TRUE;

	return FALSE;
}

BOOL CWaveformIn::Stop()
{
	if (waveInStop(m_hwi) == MMSYSERR_NOERROR)
		return TRUE;

	return FALSE;
}

BOOL CWaveformIn::UnprepareHeader(LPWAVEHDR pwh, UINT cbwh)
{
	if (waveInUnprepareHeader(m_hwi, pwh, cbwh) == MMSYSERR_NOERROR)
		return TRUE;

	return FALSE;
}

// callback event
void CALLBACK CWaveformIn::waveInProc(UINT uMsg, WAVEHDR *dwParam1, DWORD dwParam2)
{
	return;
}

///////////////////////////////////////////////////////////
// Wave Out class
///////////////////////////////////////////////////////////
CWaveformOut::CWaveformOut(HWAVEOUT hwo)
:	m_hwo(hwo)
{
}

CWaveformOut::~CWaveformOut()
{
}

BOOL CWaveformOut::BreakLoop()
{
	if (waveOutBreakLoop(m_hwo) == MMSYSERR_NOERROR)
		return TRUE;

	return FALSE;
}

void CWaveformOut::setHandle(HWAVEOUT hwo)
{
	m_hwo = hwo;
}

BOOL CWaveformOut::Close()
{
	if (m_hwo != NULL)
		if (waveOutClose(m_hwo) == MMSYSERR_NOERROR)
		{
			m_hwo = NULL;
			return TRUE;
		}

		return FALSE;
}

BOOL CWaveformOut::GetID(LPUINT puDeviceId)
{
	if ( waveOutGetID(m_hwo, puDeviceId) == MMSYSERR_NOERROR)
		return TRUE;

	return FALSE;
}


BOOL CWaveformOut::GetPitch(LPDWORD pdwPitch)
{
	if ( waveOutGetPitch(m_hwo, pdwPitch) == MMSYSERR_NOERROR)
		return TRUE;

	return FALSE;
}

BOOL CWaveformOut::GetPlaybackRate(LPDWORD pdwRate)
{
	if ( waveOutGetPlaybackRate(m_hwo, pdwRate) == MMSYSERR_NOERROR)
		return TRUE;

	return FALSE;
}

BOOL CWaveformOut::GetPosition(LPMMTIME pmmt, UINT cbmmt)
{
	if ( waveOutGetPosition(m_hwo, pmmt, cbmmt) == MMSYSERR_NOERROR)
		return TRUE;

	return FALSE;
}

BOOL CWaveformOut::GetVolume(LPDWORD pdwVolume)
{
	if ( waveOutGetVolume(m_hwo, pdwVolume) == MMSYSERR_NOERROR)
		return TRUE;

	return FALSE;
}

BOOL CWaveformOut::Message(UINT uMsg, DWORD dw1, DWORD dw2)
{
	if ( waveOutMessage(m_hwo, uMsg, dw1, dw2) == MMSYSERR_NOERROR)
		return TRUE;

	return FALSE;
}

BOOL CWaveformOut::Pause()
{
	if ( waveOutPause(m_hwo) == MMSYSERR_NOERROR)
		return TRUE;

	return FALSE;
}

BOOL CWaveformOut::PrepareHeader(LPWAVEHDR pwh, UINT cbwh)
{
	if ( waveOutPrepareHeader(m_hwo, pwh, cbwh) == MMSYSERR_NOERROR)
		return TRUE;

	return FALSE;
}

BOOL CWaveformOut::Reset()
{
	if ( waveOutReset(m_hwo) == MMSYSERR_NOERROR)
		return TRUE;

	return FALSE;
}

BOOL CWaveformOut::Restart()
{
	if ( waveOutRestart(m_hwo) == MMSYSERR_NOERROR)
		return TRUE;

	return FALSE;
}

BOOL CWaveformOut::SetPitch(DWORD dwPitch)
{
	if ( waveOutSetPitch(m_hwo, dwPitch) == MMSYSERR_NOERROR)
		return TRUE;

	return FALSE;
}

BOOL CWaveformOut::SetPlaybackRate(DWORD dwRate)
{
	if ( waveOutSetPlaybackRate(m_hwo, dwRate) == MMSYSERR_NOERROR)
		return TRUE;

	return FALSE;
}

BOOL CWaveformOut::SetVolume(DWORD dwVolume)
{
	if ( waveOutSetVolume(m_hwo, dwVolume) == MMSYSERR_NOERROR)
		return TRUE;

	return FALSE;
}

BOOL CWaveformOut::UnprepareHeader(LPWAVEHDR pwh, UINT cbwh)
{
	if ( waveOutUnprepareHeader(m_hwo, pwh, cbwh) == MMSYSERR_NOERROR)
		return TRUE;

	return FALSE;
}

BOOL CWaveformOut::Write(LPWAVEHDR pwh, UINT cbwh)
{
	if ( waveOutWrite(m_hwo, pwh, cbwh) == MMSYSERR_NOERROR)
		return TRUE;

	return FALSE;
}

// callback event
void CALLBACK CWaveformOut::waveOutProc(UINT uMsg, WAVEHDR *dwParam1, DWORD dwParam2)
{
	return;
}
