// AllocatorTests.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <unordered_map>
#include <chrono>
#include <fstream>
#include <algorithm>


#include "../Engine/DummyAllocator.h"
#include "../Engine/Core/Memory/LinearAllocator.h"
#include "../Engine/Core/Memory/StackAllocator.h"
#include "../Engine/Core/Memory/PoolAllocator.h"
#include "../Engine/ConfigReader.h"
#include "AllocatorTests.h"

//void testAllocator()
//{
//    DummyAllocator allocator;
//    void* mem = allocator.allocate(64);
//    allocator.deallocate(mem);
//
//}
//
//void testLinearAllocator()
//{
//    // Example usage of LinearAllocator
//    LinearAllocator linearAllocator(1024); // 1 KB pool
//    void* mem1 = linearAllocator.allocate(128);
//    void* mem2 = linearAllocator.allocate(256);
//    linearAllocator.reset(); // Reset the allocator to reuse memory
//    void* mem3 = linearAllocator.allocate(512); // Allocate again after reset
//
//    std::cout << "LinearAllocator allocated memory blocks at: " 
//		<< mem1 << ", " << mem2 << ", " << mem3 << std::endl;
//
//	linearAllocator.reset(); // Clean up
//}
//
//void testStackAllocator()
//{
//    // Example usage of StackAllocator
//    StackAllocator stackAllocator(1024); // 1 KB pool
//    void* mem1 = stackAllocator.allocate(64);
//    StackMarker marker = stackAllocator.getMarker();
//    void* mem2 = stackAllocator.allocate(64);
//    stackAllocator.freeToMarker(marker); // Free back to marker
//    void* mem3 = stackAllocator.allocate(64); // Allocate again after freeing to marker
//    std::cout << "StackAllocator allocated memory blocks at: " 
//        << mem1 << ", " << mem2 << ", " << mem3 << std::endl;
//    stackAllocator.reset(); // Clean up
//}
//
//void testPoolAllocator()
//{
//
//     PoolAllocator poolAllocator(32, 10); // 10 blocks of 32 bytes each
//     void* mem1 = poolAllocator.allocate(32);
//     void* mem2 = poolAllocator.allocate(32);
//     poolAllocator.deallocate(mem1);
//     void* mem3 = poolAllocator.allocate(32); // Should reuse the freed block
//     std::cout << "PoolAllocator allocated memory blocks at: " 
//         << mem1 << ", " << mem2 << ", " << mem3 << std::endl;
//}


template<typename Func>
long long benchmark(Func&& f) {
    auto start = std::chrono::high_resolution_clock::now();
    f();
    auto end = std::chrono::high_resolution_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
}

Allocator* createAllocator(const std::unordered_map<std::string, std::string>& config) {
    std::string type = config.at("allocator");
    if (type == "Linear") {
        size_t blockSize = std::stoull(config.at("block_size"));
        return new LinearAllocator(blockSize);
    }
    else if (type == "Stack") {
        size_t blockSize = std::stoull(config.at("block_size"));
        return new StackAllocator(blockSize);
    }
    else if (type == "Pool") {
        size_t blockSize = std::stoull(config.at("block_size"));
        size_t numBlocks = std::stoull(config.at("num_blocks"));
        return new PoolAllocator(blockSize, numBlocks);
    }
    else {
        return new DummyAllocator();
    }
}

void saveBenchmarkCSV(const std::string& filename,
    const std::vector<std::tuple<std::string, size_t, size_t, long long>>& results) {
    std::ofstream file(filename);
    file << "Allocator,BlockSize,AllocCount,Time(ms)\n";
    for (auto& r : results) {
        file << std::get<0>(r) << ","   // allocator name
            << std::get<1>(r) << ","   // block size
            << std::get<2>(r) << ","   // alloc count
            << std::get<3>(r) << "\n"; // time
    }
}

#pragma region Benchmarking and Auto-Selection

std::string autoSelectAllocator() {
    constexpr size_t NUM_ALLOCS = 50000;
    constexpr size_t BLOCK_SIZE = 32;

    std::unordered_map<std::string, long long> results;

    results["Linear"] = benchmark([&]() {
        LinearAllocator alloc(NUM_ALLOCS * BLOCK_SIZE);
        for (size_t i = 0; i < NUM_ALLOCS; ++i) alloc.allocate(BLOCK_SIZE);
        alloc.reset();
        });

    results["Stack"] = benchmark([&]() {
        StackAllocator alloc(NUM_ALLOCS * BLOCK_SIZE);
        for (size_t i = 0; i < NUM_ALLOCS; ++i) alloc.allocate(BLOCK_SIZE);
        alloc.reset();
        });

    results["Pool"] = benchmark([&]() {
        PoolAllocator alloc(BLOCK_SIZE, NUM_ALLOCS);
        for (size_t i = 0; i < NUM_ALLOCS; ++i) {
            void* p = alloc.allocate(BLOCK_SIZE);
            alloc.deallocate(p);
        }
        });

    results["Dummy"] = benchmark([&]() {
        DummyAllocator alloc;
        for (size_t i = 0; i < NUM_ALLOCS; ++i) {
            void* p = alloc.allocate(BLOCK_SIZE);
            alloc.deallocate(p);
        }
        });

    // Pick the best
    auto best = std::min_element(
        results.begin(), results.end(),
        [](auto& a, auto& b) { return a.second < b.second; }
    );

    std::cout << "Auto benchmark results:\n";
    for (auto& r : results) {
        std::cout << "  " << r.first << ": " << r.second << " ms\n";
    }
    std::cout << "Fastest allocator: " << best->first << "\n";

    return best->first; //  still returns a string
}


std::vector<std::tuple<std::string, size_t, size_t, long long>> runBenchmarks() {
    std::vector<std::tuple<std::string, size_t, size_t, long long>> results;

    const std::vector<size_t> blockSizes = { 32, 256, 1024 };
    const std::vector<size_t> allocCounts = { 10000, 100000, 1000000 };


    for (size_t block : blockSizes) {
        for (size_t count : allocCounts) {

            // Linear
            long long linearTime = benchmark([&]() {
                LinearAllocator alloc(count * block);
                for (size_t i = 0; i < count; ++i) alloc.allocate(block);
                alloc.reset();
                });
            results.emplace_back("Linear", block, count, linearTime);

            // Stack
            long long stackTime = benchmark([&]() {
                StackAllocator alloc(count * block);
                for (size_t i = 0; i < count; ++i) alloc.allocate(block);
                alloc.reset();
                });
            results.emplace_back("Stack", block, count, stackTime);

            // Pool
            long long poolTime = benchmark([&]() {
                PoolAllocator alloc(block, count);
                for (size_t i = 0; i < count; ++i) {
                    void* p = alloc.allocate(block);
                    alloc.deallocate(p);
                }
                });
            results.emplace_back("Pool", block, count, poolTime);

            // Dummy (malloc/free)
            long long dummyTime = benchmark([&]() {
                DummyAllocator alloc;
                for (size_t i = 0; i < count; ++i) {
                    void* p = alloc.allocate(block);
                    alloc.deallocate(p);
                }
                });
            results.emplace_back("Dummy", block, count, dummyTime);
        }
    }
    return results;
}

#pragma endregion

void saveConfig(const std::string& filename, const std::string& allocator) {
    std::ofstream file(filename);
    if (!file) return;

    file << "allocator = " << allocator << "\n";
    if (allocator == "Pool") {
        file << "block_size = 32\n";
        file << "num_blocks = 1000\n";
    }
    else {
        file << "block_size = 32768\n"; // 32 KB default
    }

	file << "show_profiler_overlay = true\n";
}

void runAllocatorTest(Allocator* allocator) {
    for (int i = 0; i < 5; ++i) {
        void* ptr = allocator->allocate(32);
        allocator->deallocate(ptr);
    }

    auto stats = allocator->getStats();
    std::cout << "Stats:\n"
        << "  Allocated: " << stats.totalAllocated << " bytes\n"
        << "  Peak:      " << stats.peakUsage << " bytes\n"
        << "  Count:     " << stats.allocationCount << "\n";
}


int main()
{
    /*std::cout << "Mini Engine: Week 1 Started \n" << std::endl;

    testLinearAllocator();

    testStackAllocator();

    testPoolAllocator();

    std::cout << "DummyAllocator successfully allocated and freed memory!" << std::endl;

    std::cout << "Press Enter To Quit.." << std::endl;*/

    InitConfig();

   // Allocator* allocator = createAllocator(config);

   // runAllocatorTest(allocator);



    //delete allocator;

    std::cin.get();
    return 0;
}

void InitConfig()
{
    std::string configFile = "engine.cfg";
    auto config = loadConfig(configFile);

    if (config.empty()) {
        std::cout << "No config found. Running auto-benchmark...\n";
        auto results = runBenchmarks();
        saveBenchmarkCSV("benchmarks.csv", results);
        std::string bestAllocator = autoSelectAllocator();
        saveConfig(configFile, bestAllocator);
        config = loadConfig(configFile);

    }
}

