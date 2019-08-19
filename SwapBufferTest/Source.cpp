#include <atomic>
#include <iostream>
#include <thread>
#include <new>
#include <Windows.h>
#include <assert.h>
unsigned previous_read = -1;

std::thread g_readThread;
volatile bool g_shutdownReadThread = false;
volatile bool g_shutdownWriteThread = false;
constexpr uint64_t g_BUFFER_SIZE = 16384ULL;
constexpr unsigned g_NUM_BUFFERS = 2;

alignas(8) volatile uint64_t* buffers = (volatile uint64_t*)_aligned_malloc(g_BUFFER_SIZE * sizeof(uint64_t) * g_NUM_BUFFERS, 8);
alignas(8) volatile uint64_t buffer_sizes[g_NUM_BUFFERS];

unsigned read_buffer_index = 0;
unsigned write_buffer_index = 1;

void Swap()
{
	read_buffer_index = write_buffer_index;
	write_buffer_index = (write_buffer_index + 1) & (g_NUM_BUFFERS-1);
}

bool Try_Push_Write(uint64_t value)
{
	if (buffer_sizes[write_buffer_index] == g_BUFFER_SIZE)
		return false;
	assert(buffer_sizes[write_buffer_index] < g_BUFFER_SIZE);
	buffers[write_buffer_index * g_BUFFER_SIZE + buffer_sizes[write_buffer_index]] = value;
	buffer_sizes[write_buffer_index]++;
	return true;
}

void Process_Reads()
{
	auto sz = buffer_sizes[read_buffer_index];
	if (!sz)
		return;

	for (unsigned i = 0; i < buffer_sizes[read_buffer_index]; i++)
	{
		unsigned this_read = buffers[read_buffer_index * g_BUFFER_SIZE + i];
		assert(this_read == previous_read + 1);
		std::cout << buffers[read_buffer_index * g_BUFFER_SIZE + i] << "\n";
		previous_read = this_read;
	}

	buffer_sizes[read_buffer_index] = 0;
}

int read_main()
{
	while (!g_shutdownReadThread)
	{
		Process_Reads();
	}
	return 0;
}

int main()
{
	uint64_t counter = 0;
	g_readThread = std::thread(read_main);

	while (counter < 16000000)
	{
		if (!Try_Push_Write(counter))
		{
			while (buffer_sizes[read_buffer_index] != 0)
			{
				Sleep(0);
			}
			// TODO // Run a job while we wait?

			//
			Swap();
			Try_Push_Write(counter);
			counter++;
			continue;
		};
		if (buffer_sizes[read_buffer_index] == 0)
			Swap();

		Sleep(0);
		counter++;
	}
	g_shutdownReadThread = true;
	g_readThread.join();
	return 0;
}