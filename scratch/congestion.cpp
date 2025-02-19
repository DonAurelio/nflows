#include <iostream>
#include <thread>
#include <vector>
#include <cstdlib>
#include <cstring>

const size_t TOTAL_MEMORY_MB = 47000;  // Half of 94GB
const size_t THREAD_COUNT = 32;        // Adjust as needed
const size_t MEMORY_PER_THREAD_MB = TOTAL_MEMORY_MB / THREAD_COUNT;
const size_t MEMORY_PER_THREAD = MEMORY_PER_THREAD_MB * 1024 * 1024; // Convert MB to bytes

void memory_write(char* buffer, size_t size) {
    // Fill memory with dummy data to simulate writes
    memset(buffer, 0xAB, size);
}

int main() {
    std::vector<std::thread> threads;
    std::vector<char*> memory_blocks(THREAD_COUNT);

    // Allocate memory per thread
    for (size_t i = 0; i < THREAD_COUNT; i++) {
        memory_blocks[i] = (char*)malloc(MEMORY_PER_THREAD);
        if (!memory_blocks[i]) {
            std::cerr << "Memory allocation failed for thread " << i << "\n";
            return -1;
        }
    }

    // Launch threads to write memory
    for (size_t i = 0; i < THREAD_COUNT; i++) {
        threads.emplace_back(memory_write, memory_blocks[i], MEMORY_PER_THREAD);
    }

    // Join threads
    for (auto& t : threads) {
        t.join();
    }

    // Free memory
    for (size_t i = 0; i < THREAD_COUNT; i++) {
        free(memory_blocks[i]);
    }

    std::cout << "Memory write test completed.\n";
    return 0;
}
