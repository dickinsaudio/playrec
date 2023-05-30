#include <string.h>
#include "mex.h"
#include "audiodevice.hpp"
#include <chrono>  
#include <thread>  


static AudioDevice audio;

void mexExit(void)
{
    audio.Close();
}

void config(int nrhs, const mxArray *prhs[]);

void mexFunction(int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[]) 
{
    mexAtExit(mexExit);

    if (nrhs==0) {     plhs[0] = mxCreateDoubleScalar((double)audio.Now()); return; };

    if (nrhs==2 && mxIsChar(prhs[0]) && mxIsScalar(prhs[1]) && !_stricmp(mxArrayToString(prhs[0]),"device"))
    {
        plhs[0] = mxCreateString(audio.Device((int)mxGetScalar(prhs[1])-1));
        return;
	}

    if (nrhs>=1 && mxIsChar(prhs[0]) && !_stricmp(mxArrayToString(prhs[0]),"open"))
    {
        mexPrint("Attempting to open an ASIO device.\nNote that only 32 bit ASIO devices are presently supported - other formats may create digital noise.\n");
        config(nrhs, prhs);
        plhs[0]=mxCreateDoubleScalar((double)audio.Now());
        return;
    }

    if (nrhs>=1 && mxIsChar(prhs[0]) && !_stricmp(mxArrayToString(prhs[0]),"record"))
    {
        int64_t T;
        int     N;
        if (nrhs>=3) { T = (int)mxGetScalar(prhs[1]); N = (int)mxGetScalar(prhs[2]); };
        if (nrhs==2) { N = (int)mxGetScalar(prhs[1]); T = audio.Now() - N; };
        if (nrhs==1) { N = audio.swBufSize()/2; T = audio.Now() - N; };        
        mwSize dims[2] = { (mwSize)N, (mwSize)audio.in() };
        plhs[0] = mxCreateNumericArray(2, dims, mxINT32_CLASS, mxREAL);
        if ( T+N-audio.Now() < 10*audio.rate() ) while(audio.Now() < T+N) std::this_thread::sleep_for(std::chrono::microseconds(250));
        if (!audio.Read(T, N, (int32_t *)mxGetData(plhs[0]), 1, N)) mexWarnMsgIdAndTxt("playrec:warning","Capture was outside of buffer range");
        return;
    }

    if (nrhs>=2 && mxIsChar(prhs[0]) && !_stricmp(mxArrayToString(prhs[0]),"play"))
    {
        int64_t T = audio.Now() + 2*audio.hwBufSize();
        const mxArray *p = prhs[1];
        if (nrhs==3) { T = (int)mxGetScalar(prhs[1]); p = prhs[2]; };
        if (mxGetClassID(p)!=mxINT32_CLASS) mexErrMsgIdAndTxt("playrec:error","Only int32 precision supported for now");
        if (mxGetN(p)!=audio.out())         mexErrMsgIdAndTxt("playrec:error","Must be correct number of channels to output");
        int N = (int)mxGetM(p);
        if (!audio.Write(T, N, (int32_t *)mxGetData(p), 1, N)) mexWarnMsgIdAndTxt("playrec:warning","Playback was outside of buffer range (too late)"); 
        return;
    }


    if (nrhs==1 && mxIsChar(prhs[0]))
    {
        char *pStr = mxArrayToString(prhs[0]);
        if (!_stricmp(pStr,"devices"))   plhs[0] = mxCreateDoubleScalar(audio.Devices());  
        if (!_stricmp(pStr,"now"))       plhs[0] = mxCreateDoubleScalar((double)audio.Now());  
        if (!_stricmp(pStr,"in"))        plhs[0] = mxCreateDoubleScalar(audio.in());  
        if (!_stricmp(pStr,"out"))       plhs[0] = mxCreateDoubleScalar(audio.out()); 
        if (!_stricmp(pStr,"rate"))      plhs[0] = mxCreateDoubleScalar(audio.rate()); 
        if (!_stricmp(pStr,"hwbuffer"))  plhs[0] = mxCreateDoubleScalar(audio.hwBufSize());  
        if (!_stricmp(pStr,"swbuffer"))  plhs[0] = mxCreateDoubleScalar(audio.swBufSize());  
        if (plhs[0]==0) mexErrMsgIdAndTxt("playrec:error","Unknown Query - Use Devices | Now | In | Out | Rate | HWBuffer | SWBuffer");
        return;
    }
    mexErrMsgIdAndTxt("playrec:error","Unknown Query - Use Devices | Now | In | Out | Rate | HWBuffer | SWBuffer");
}


void config(int nrhs, const mxArray *prhs[])
{
    int device = 0, in = 2, out = 2, buffer = 96000;
    int narg = 1;

    audio.Close();

    if (audio.Devices()==0)  mexErrMsgIdAndTxt("playrec:error","No devices found");

    while (narg<nrhs)
    {
        if (mxIsChar(prhs[narg]) && nrhs>narg+1 && mxIsScalar(prhs[narg+1]))
        {
            char* s = mxArrayToString(prhs[narg]);
            int x=(int)mxGetScalar(prhs[narg+1]);
            if      (!_stricmp(s,"in"))     in  = x;
            else if (!_stricmp(s,"out"))    out = x;
            else if (!_stricmp(s,"device")) device = x;
            else if (!_stricmp(s,"buffer")) buffer = x;
            else
            {
                char sError[128];
                snprintf(sError,128,"Error bad configuration parameter pair '%s'=%d",s,x);
                mexErrMsgIdAndTxt("playrec:error",sError);
            }
            narg+=2;
        }
        else mexErrMsgIdAndTxt("playrec:error","Invalid parameter in config");
    }

    if (audio.Open(device-1, in, out, buffer) == false)  mexErrMsgIdAndTxt("playrec:error","Failed to open device");
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
}

