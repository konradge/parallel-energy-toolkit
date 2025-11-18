#include <iostream>
#include <vector>
#include <algorithm>
#include <thread>
#include <random>
#include <algorithm>
#include <execution>
#include <future>
#include <fcntl.h>      // open, O_RDONLY
#include <unistd.h>     // read, close
#include <sys/types.h>  // ssize_t (optional)
#include <sys/stat.h>   // (optional)
#include <cerrno>       // errno (optional)
#include <cstdio>       // perror (optional)
#include <chrono>       // for std::chrono
#include <malloc.h>
#include "msr_reader.h"

class ThreadResult {
    public:
    long long result;
    std::chrono::duration<double> time;
    double energy;
};

ThreadResult calculate(int start, int end) {
    ThreadResult result;
    long long sum = 0;

    // auto thread_id = std::this_thread::get_id();
    auto core_id = sched_getcpu();

    auto start_time = std::chrono::high_resolution_clock::now();
    double start_energy = read_intel_msr(core_id);
    
    // Expensive parallelized computation
    for (int i = start; i < end; ++i) {
        result.result += sqrt(i) * 10 + sin(i + 1) * cos(i - 3) * tan(i / 2.0);
    }

    auto final_core_id = sched_getcpu();

    std::cout << "Thread on core " << core_id << " finished on core " << final_core_id << std::endl;

    auto end_time = std::chrono::high_resolution_clock::now();
    double end_energy = read_intel_msr(core_id);

    result.time = end_time - start_time;
    result.energy = end_energy - start_energy;
    result.result = sum;
    return result;
}

int main(int argc, char* argv[]) {
    long count = std::stol(argv[1]);

    int thread_count = std::stoi(argv[2]);

    long first_end = count / 4;
    long second_end = count / 2;
    long third_end = 3 * count / 4;

    // auto future = std::async(calculate, 0, first_end);

    std::vector<std::future<ThreadResult>> threads(thread_count);

    std::vector<ThreadResult> results(thread_count);


    auto start_time = std::chrono::high_resolution_clock::now();
    for(int i = 0; i < thread_count; i++) {
        threads[i] = std::async(calculate, i * (count / thread_count), (i + 1) * (count / thread_count));
    }

    for(int i = 0; i < thread_count; i++) {
        results[i] = threads[i].get();
    }
    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end_time - start_time;

    long long total_sum = 0;
    std::chrono::duration<double> max_elapsed_children = std::chrono::duration<double>::zero();
    double energy_children = 0.0;
    for (const auto& result : results) {
        total_sum += result.result;
        max_elapsed_children = std::max(max_elapsed_children, result.time);
        energy_children += result.energy;
    }

    return 0;
}
