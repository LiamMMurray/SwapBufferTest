#include <Windows.h>
#include <assert.h>
#include <atomic>
#include <iostream>
#include <new>
#include <thread>
unsigned g_dbg_previousRead = -1;

// single producer single consumer 
struct InputBuffer
{
    private:
        static constexpr uint64_t m_BUFFER_SIZE = 16384ULL;
        static constexpr unsigned m_NUM_BUFFERS = 2;

        volatile uint64_t* m_buffers;
        volatile uint64_t  m_bufferSizes[m_NUM_BUFFERS];
        unsigned           m_readBufferIndex  = 0;
        unsigned           m_writeBufferIndex = 1;

        void Swap()
        {
                m_readBufferIndex  = m_writeBufferIndex;
                m_writeBufferIndex = (m_writeBufferIndex + 1) & (m_NUM_BUFFERS - 1);
        }

        bool TryPushWrite(uint64_t value)
        {
                if (m_bufferSizes[m_writeBufferIndex] == m_BUFFER_SIZE)
                        return false;
                assert(m_bufferSizes[m_writeBufferIndex] < m_BUFFER_SIZE);
                m_buffers[m_writeBufferIndex * m_BUFFER_SIZE + m_bufferSizes[m_writeBufferIndex]] = value;
                m_bufferSizes[m_writeBufferIndex]++;
                return true;
        }

    public:
        void ProcessReads()
        {
                auto sz = m_bufferSizes[m_readBufferIndex];
                if (!sz)
                        return;

                for (unsigned i = 0; i < m_bufferSizes[m_readBufferIndex]; i++)
                {
                        unsigned thisRead = m_buffers[m_readBufferIndex * m_BUFFER_SIZE + i];
                        assert(thisRead == g_dbg_previousRead + 1);
                        std::cout << m_buffers[m_readBufferIndex * m_BUFFER_SIZE + i] << "\n";
                        g_dbg_previousRead = thisRead;
                }

                m_bufferSizes[m_readBufferIndex] = 0;
        }

        void Write(uint64_t value)
        {
                if (!TryPushWrite(value))
                {
                        while (m_bufferSizes[m_readBufferIndex] != 0)
                        {
                                Sleep(0);
                        }
                        // TODO // Run a job while we wait?

                        //
                        Swap();
                        TryPushWrite(value);
                        return;
                };
                if (m_bufferSizes[m_readBufferIndex] == 0)
                        Swap();

                Sleep(0);
        }

        void Initialize()
        {
                m_buffers = (volatile uint64_t*)_aligned_malloc(m_BUFFER_SIZE * sizeof(uint64_t) * m_NUM_BUFFERS, 8);
        }

        void Shutdown()
        {
                _aligned_free(const_cast<uint64_t*>(m_buffers));
        }
};

std::thread   g_readThread;
volatile bool g_shutdownReadThread  = false;
bool          g_shutdownWriteThread = false;
InputBuffer   g_InputBuffer;

int readMain()
{
        while (!g_shutdownReadThread)
        {
                g_InputBuffer.ProcessReads();
        }
        return 0;
}

int main()
{
        g_InputBuffer.Initialize();
        g_readThread   = std::thread(readMain);
        unsigned count = 0;
        while (!g_shutdownWriteThread)
        {
                g_InputBuffer.Write(count);
                count++;
        }
        g_InputBuffer.Shutdown();
        return 0;
}