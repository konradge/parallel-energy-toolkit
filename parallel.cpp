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
#include <sched.h>
#include <assert.h>

int pin_to_thread() {
    auto core_id = sched_getcpu();
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);
    int rc = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);

    if (rc != 0) {
        std::cerr << "Error calling pthread_setaffinity_np: " << rc << "\n";
    }
    return core_id;
}

class Args {
    public:
    long start;
    long end;
};

typedef std::chrono::time_point<std::chrono::high_resolution_clock> time_point;
typedef std::chrono::duration<double> duration;

class ThreadResult {
    public:
    long long result;
    time_point start_time;
    time_point end_time;
    double start_energy;
    double end_energy;

    int core_id;

    duration time() const {
        return end_time - start_time;
    }

    double energy() const {
        return end_energy - start_energy;
    }
};

std::mutex iomutex;
ThreadResult calculate(int start, int end) {
    ThreadResult result;

    result.core_id = pin_to_thread();
    long long sum = 0;

    result.start_time = std::chrono::high_resolution_clock::now();
    result.start_energy = read_intel_msr(result.core_id);
    
    // Expensive parallelized computation
    for (int i = 0; i < end-start; i++) {
        result.result += sqrt(i) * 10 + sin(i + 1) * cos(i - 3) * tan(i / 2.0);
    }

    result.end_time = std::chrono::high_resolution_clock::now();
    result.end_energy = read_intel_msr(result.core_id);
    result.result = sum;

    auto final_core_id = sched_getcpu();

    assert(result.core_id == sched_getcpu() && "Thread did run on multiple cores!");

    iomutex.lock();
    // std::cout << "Thread on core " << result.core_id << " finished on core " << final_core_id << std::endl;
    iomutex.unlock();

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
    duration elapsed = end_time - start_time;

    long long total_sum = 0;
    duration max_elapsed_children = duration::zero();
    double energy_children = 0.0;

    std::unordered_map<int, std::vector<ThreadResult>> result_by_core;
    for (const auto& result : results) {
        if(result_by_core.find(result.core_id) == result_by_core.end()) {
            result_by_core[result.core_id] = std::vector<ThreadResult>();
        }
        result_by_core[result.core_id].push_back(result);
    }

    for(const auto& [core_id, core_results] : result_by_core) {
        // auto results = core_results;
        // std::sort(results.begin(), results.end(), [](const ThreadResult& a, const ThreadResult& b) {
        //     return a.start_time < b.start_time;
        // });
        duration time = duration::zero();
        double energy = 0.0;
        for(size_t i = 0; i < core_results.size(); i++) {
            time.operator+=(core_results[i].time());
            energy += core_results[i].energy();
        }
        std::cout << "Core " << core_id << " results: \n";
        std::cout << "Total time: " << time.count() << " s";
        std::cout << ", Total energy: " << energy << " J\n";
        for(const auto& result : core_results) {
            std::cout << "Start time: " << std::chrono::duration_cast<std::chrono::milliseconds>(result.start_time.time_since_epoch()).count() << " ms, "
                      << "End time: " << std::chrono::duration_cast<std::chrono::milliseconds>(result.end_time.time_since_epoch()).count() << " ms, "
                      << "Duration: " << result.time().count() << " s, "
                      << "Energy: " << result.energy() << " J, "
                      << "Result: " << result.result << "\n";
    }
    std::cout << std::endl << std::endl;
}

    return 0;
}
