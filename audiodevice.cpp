

#include "audiodevice.hpp"
#include <stdio.h>
#include <stdlib.h>
#include <cassert>
#include <algorithm>
#include "asio.h"
#include "asiosys.h"
#include "asiodrivers.h"
#include "mex.h"


#define ASIO_MAX_CH 64

bool loadAsioDriver(char *name);
AudioDevice*    audioDevice = nullptr;
AsioDriverList  asioDrivers;
ASIOBufferInfo  asioBuffer[2*ASIO_MAX_CH];
ASIOChannelInfo asioInfo[2*ASIO_MAX_CH];
ASIOCallbacks   asioCallbacks;
int32_t*        audioBuffer = 0;

AudioDevice::AudioDevice()
{
    assert(audioDevice==nullptr);        // There can be only one
    audioDevice = this;              
}

AudioDevice::~AudioDevice()
{
    Close();
}

int32_t AudioDevice::Devices()
{
    return asioDrivers.asioGetNumDev();
}

char Temp[256];
char* AudioDevice::Device(int32_t n)
{
    if (n<0 || n>=Devices()) return nullptr;
    asioDrivers.asioGetDriverName(n,Temp,256);
    return Temp;
}

void AudioDevice::bufferSwitch(long index, bool processNow)
{  
    if (!running) return;
    for (int c=0; c<m_in; c++)  memcpy(audioBuffer+c*m_swBufSize+(m_T%m_swBufSize),asioBuffer[c].buffers[index],m_hwBufSize*sizeof(int32_t));
    for (int c=0; c<m_out; c++) memcpy(asioBuffer[m_in+c].buffers[index],audioBuffer+(m_in+c)*m_swBufSize+((m_T+2*m_hwBufSize)%m_swBufSize),m_hwBufSize*sizeof(int32_t));
    for (int c=0; c<m_out; c++) memset(audioBuffer+(m_in+c)*m_swBufSize+((m_T+2*m_hwBufSize)%m_swBufSize),0,m_hwBufSize*sizeof(int32_t));
//    for (int c=0; c<m_out; c++) memcpy(asioBuffer[m_in+c].buffers[index],asioBuffer[c].buffers[index],m_hwBufSize*sizeof(int32_t));
    m_T += m_hwBufSize;
    ASIOOutputReady();
}

void bufferSwitch(long index, ASIOBool processNow)
{
    audioDevice->bufferSwitch(index, (bool)processNow);
}

long asioMessages(long selector, long value, void* message, double* opt)
{
    fprintf(stderr,"ASIO MESSAGE %ld %ld\n",selector,value);    // Handle ASIO message event
    return 0;
}
void sampleRateDidChange(ASIOSampleRate sRate) {}
void asioBuffersOverrun(int64_t index)  { audioDevice->asioBufferOverrun(index); }
void asioBuffersUnderrun(int64_t index) { audioDevice->asioBufferUnderrun(index); }

const int debug=1;
bool AudioDevice::Open(int32_t n, int32_t rx, int32_t tx, int32_t swBuf)
{
    fprintf(stderr,"Help");

    if (n<0 || n>=Devices() ) return false;
    if (rx>ASIO_MAX_CH || tx>ASIO_MAX_CH) return false;
    if (running) return false;
    
    ASIODriverInfo asioDriver;
	if (!loadAsioDriver(Device(n)) || ASIOInit(&asioDriver)) return false;

    long nIn, nOut, nMin, nMax, nPref, nStep;
	//ASIOSampleRate dFs;

    ASIOGetChannels(&nIn, &nOut);   
	//ASIOGetSampleRate(&dFs);
	ASIOGetBufferSize(&nMin, &nMax, &nPref, &nStep);

    m_in = min((int)nIn,(int)rx);
    m_out = min((int)nOut,(int)tx);
    m_rate = 48000; //(int)*(double *)&dFs;
    m_hwBufSize = nPref;
    m_swBufSize = ((swBuf-1)/m_hwBufSize+1)*m_hwBufSize;  // Make it integral size

	asioCallbacks.bufferSwitch = &::bufferSwitch;
    asioCallbacks.sampleRateDidChange = &sampleRateDidChange;
	asioCallbacks.asioMessage = &asioMessages;
	asioCallbacks.bufferSwitchTimeInfo = nullptr;

	for (int32_t i = 0; i < m_in + m_out; i++)
	{
		asioBuffer[i].isInput =    (i<m_in) ? ASIOTrue : ASIOFalse;
		asioBuffer[i].channelNum = (i<m_in) ? i : i - m_in;
		asioBuffer[i].buffers[0] = asioBuffer[i].buffers[1] = 0;
	}

	ASIOCreateBuffers(asioBuffer, m_in + m_out, m_hwBufSize, &asioCallbacks);
	for (int32_t i = 0; i < m_in + m_out; i++)
	{
		asioInfo[i].channel = asioBuffer[i].channelNum;
		asioInfo[i].isInput = asioBuffer[i].isInput;
		ASIOGetChannelInfo(&asioInfo[i]);
	}

	if (ASIOStart() != ASE_OK) { Close(); return false; };
    m_T=0;
    audioBuffer = (int32_t *)calloc((m_in+m_out)*m_swBufSize,sizeof(int32_t));
    running = true;
}

void AudioDevice::Close()
{
    running = false;
    ASIOStop();
	ASIOExit();								// Stop the ASIO subsystem so no more ASIO callbacks
    if (audioBuffer) free(audioBuffer);
    audioBuffer = nullptr;
    m_T = 0;
	Sleep(200);								// Brief sleep 
}

// Write audio into the buffer at time T and length N
// Will only write the audio that would make sense give the current time and buffer
bool AudioDevice::Write(int64_t T, int32_t N, int32_t *data, int32_t Sstride, int32_t Cstride)
{
    if (!running) return false;
    if (T     > m_T + m_swBufSize) return false;                  // All audio is too far into the future
    if (T + N < m_T + 2*m_hwBufSize) return false;                // All audio is too far in the past

    int64_t t_start = max(T,     m_T + 2*m_hwBufSize);
    int64_t t_end   = min(T + N, m_T + m_swBufSize);

    if (t_start > T) data += Sstride * (t_start - T);
    N  = (int32_t)(t_end - t_start);
    if (N<=0) return false;

    t_start = t_start % m_swBufSize;
    if (t_start + N > m_swBufSize)
    {
        for (int i=0; i<m_out; i++) for (int j=t_start; j<m_swBufSize; j++) audioBuffer[(i+m_in)*m_swBufSize+j] = data[i*Cstride+(j-t_start)*Sstride];
        N    -= (m_swBufSize - t_start);
        data += (m_swBufSize - t_start)*Sstride;
        t_start = 0;
    }
    for (int i=0; i<m_out; i++) for (int j=t_start; j<t_start+N; j++) audioBuffer[(i+m_in)*m_swBufSize+j] = data[i*Cstride+(j-t_start)*Sstride];
    return true;
}

bool AudioDevice::Read(int64_t T, int32_t N, int32_t *data, int32_t Sstride, int32_t Cstride)
{
    if (!running) return false;
    if (T     > m_T               )  return false;                // All audio is too far into the future
    if (T + N < m_T - m_swBufSize )  return false;                // All audio is too far in the past

    int64_t t_start = max(T,     m_T - m_swBufSize);
    int64_t t_end   = min(T + N, m_T);

    if (t_start > T) data += Sstride * (t_start - T);
    N  = (int32_t)(t_end - t_start);
    if (N<=0) return false;

    t_start = t_start % m_swBufSize;
    if (t_start + N > m_swBufSize)
    {
        for (int i=0; i<m_in; i++) for (int j=t_start; j<m_swBufSize; j++) data[i*Cstride+(j-t_start)*Sstride] = audioBuffer[i*m_swBufSize+j];
        N    -= (m_swBufSize - t_start);
        data += (m_swBufSize - t_start)*Sstride;
        t_start = 0;
    }
    for (int i=0; i<m_in; i++)     for (int j=t_start; j<t_start+N; j++) data[i*Cstride+(j-t_start)*Sstride] = audioBuffer[i*m_swBufSize+j];
    return true;
}


