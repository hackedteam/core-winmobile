#include <windows.h>
#include <mmsystem.h>

#ifndef __WAVEFORM_H__
#define __WAVEFORM_H__

class CWaveformIn;
class CWaveformOut;

class CWaveform
{
public:
	CWaveform();
	~CWaveform();

	CWaveformIn*  InOpen(UINT uDeviceID, LPWAVEFORMATEX pwfx, DWORD dwCallback, DWORD dwInstance, DWORD fdwOpen);
	CWaveformIn*  InOpen(UINT uDeviceID, LPWAVEFORMATEX pwfx, DWORD fdwOpen);

	CWaveformOut* OutOpen(UINT uDeviceID, LPWAVEFORMATEX pwfx, DWORD dwCallback, DWORD dwInstance, DWORD fdwOpen);
	CWaveformOut* OutOpen(UINT uDeviceID, LPWAVEFORMATEX pwfx, DWORD fdwOpen);

	static BOOL InGetDevCaps(UINT uDeviceID, LPWAVEINCAPS pwic, UINT cbwic);
	static BOOL InGetErrorText(MMRESULT mmError, LPTSTR pszText, UINT cchText);
	static UINT InGetNumDevs();

	static BOOL OutGetDevCaps(UINT uDeviceID, LPWAVEOUTCAPS pwoc, UINT cbwoc);
	static BOOL OutGetErrorText(UINT mmError, LPTSTR pszText, UINT cchText);
	static UINT OutGetNumDevs();

	static void PrepareWaveFormat(LPWAVEFORMATEX pWaveFormat, WORD wFormatTag, WORD nChannels, DWORD nSamplesPerRec, WORD wBitsPerSample);

protected:
	static void CALLBACK waveInProc(HWAVEIN hwi, UINT uMsg, DWORD dwInstance, WAVEHDR *dwParam1, DWORD dwParam2);
	static void CALLBACK waveOutProc(HWAVEOUT hwo, UINT uMsg, DWORD dwInstance, WAVEHDR *dwParam1, DWORD dwParam2);

private:

};

/*	CWaveformIn class
*/
class CWaveformIn
{
	friend CWaveform;

protected:
	CWaveformIn(HWAVEIN hwi);
	void setHandle(HWAVEIN hwi);
	virtual void CALLBACK waveInProc(UINT uMsg, WAVEHDR *dwParam1, DWORD dwParam2);

public:
	inline HWAVEIN getHandle() { return this->m_hwi; };

	~CWaveformIn();

	BOOL AddBuffer(LPWAVEHDR pwh, UINT cbwh);
	BOOL Close();

	BOOL GetID(PUINT puDeviceID);

	BOOL GetPosition(LPMMTIME pmmt, UINT cbmmt);
	BOOL Message(UINT uMsg, DWORD dw1, DWORD dw2);

	BOOL PrepareHeader(LPWAVEHDR pwh, UINT cbwh);
	BOOL Reset();
	BOOL Start();
	BOOL Stop();
	BOOL UnprepareHeader(LPWAVEHDR pwh, UINT cbwh);

private:
	HWAVEIN	m_hwi;
};

class CWaveformOut
{
	friend CWaveform;

protected:
	CWaveformOut(HWAVEOUT hwo);
	void setHandle(HWAVEOUT hwo);

	virtual void CALLBACK waveOutProc(UINT uMsg, WAVEHDR *dwParam1, DWORD dwParam2);

public:
	~CWaveformOut();
	inline HWAVEOUT getHandle() { return this->m_hwo; };

	BOOL BreakLoop();
	BOOL Close();
	BOOL GetID(LPUINT puDeviceId);
	BOOL GetPitch(LPDWORD pdwPitch);
	BOOL GetPlaybackRate(LPDWORD pdwRate);
	BOOL GetPosition(LPMMTIME pmmt, UINT cbmmt);
	BOOL GetVolume(LPDWORD pdwVolume);
	BOOL Message(UINT uMsg, DWORD dw1, DWORD dw2);
	BOOL Pause();
	BOOL PrepareHeader(LPWAVEHDR pwh, UINT cbwh);
	BOOL Reset();
	BOOL Restart();
	BOOL SetPitch(DWORD dwPitch);
	BOOL SetPlaybackRate(DWORD dwRate);
	BOOL SetVolume(DWORD dwVolume);
	BOOL UnprepareHeader(LPWAVEHDR pwh, UINT cbwh);
	BOOL Write(LPWAVEHDR pwh, UINT cbwh);

private:
	HWAVEOUT m_hwo;
};

#endif
