
#include <stdint.h>

class AudioDevice
{
public:
    AudioDevice();
    ~AudioDevice();

    int32_t Devices();
    char* Device(int32_t device);

    bool Open(int32_t Device, int32_t rx=2, int32_t tx=2, int32_t swBuf=96000);
    void Close();    

    int32_t hwBufSize() { return m_hwBufSize; };
    int32_t swBufSize() { return m_swBufSize; };
    int32_t  in()  { return m_in; };
    int32_t  out() { return m_out; };
    int32_t  rate() { return m_rate; };

    bool Write(int64_t T, int32_t N, int32_t *data, int32_t Sstride, int32_t Cstride);
    bool Read(int64_t T, int32_t N, int32_t *data, int32_t Sstride, int32_t Cstride);

    int64_t Now() { return m_T; };

    void   bufferSwitch(long index, bool processNow);
    void   asioBufferOverrun(int64_t index) { overrun++; };
    void   asioBufferUnderrun(int64_t index) { underrun++; };

private:
    int64_t m_T=0;
    int32_t m_swBufSize=0;
    int32_t m_hwBufSize=0;
    int32_t  m_in=0;
    int32_t  m_out=0;
    int32_t  m_rate=0;
    bool running=false;
    int32_t overrun=0;
    int32_t underrun=0;
};
