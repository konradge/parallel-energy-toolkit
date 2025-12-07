#include <iostream>
#include <vector>
#include <algorithm>
#include <thread>
#include <random>
#include <algorithm>
#include <execution>
#include <future>
#include <fcntl.h>     // open, O_RDONLY
#include <unistd.h>    // read, close
#include <sys/types.h> // ssize_t (optional)
#include <sys/stat.h>  // (optional)
#include <cerrno>      // errno (optional)
#include <cstdio>      // perror (optional)
#include <chrono>      // for std::chrono
#include <malloc.h>
#include "msr_reader.h"
#include <sched.h>
#include <assert.h>

template <typename P, typename R>
using Func = std::function<R(P)>;

int pin_to_thread()
{
    auto core_id = sched_getcpu();
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);
    int rc = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);

    if (rc != 0)
    {
        std::cerr << "Error calling pthread_setaffinity_np: " << rc << "\n";
    }
    return core_id;
}

class Args
{
public:
    long start;
    long end;
};

typedef std::chrono::time_point<std::chrono::high_resolution_clock> time_point;
typedef std::chrono::duration<double> duration;

// typedef long long InputT;
// typedef double OutputT;
// typedef double ReduceT;

template <typename T>
class ThreadResult
{
public:
    T result;
    time_point start_time;
    time_point end_time;
    double start_energy;
    double end_energy;

    int core_id;

    duration time() const
    {
        return end_time - start_time;
    }

    double energy() const
    {
        return end_energy - start_energy;
    }
};

template <typename InputT, typename PartialResultT>
ThreadResult<PartialResultT> calculate(PartialResultT calc(InputT), InputT input)
{
    ThreadResult<PartialResultT> result;

    result.core_id = pin_to_thread();

    result.start_time = std::chrono::high_resolution_clock::now();
    result.start_energy = read_intel_msr(result.core_id);

    // calculate
    PartialResultT result_value = calc(input);

    result.end_time = std::chrono::high_resolution_clock::now();
    result.end_energy = read_intel_msr(result.core_id);
    result.result = result_value;

    assert(result.core_id == sched_getcpu() && "Thread did run on multiple cores!");

    return result;
}

template <typename InputT, typename PartialResultT, typename ResultT>
ResultT parallel(int thread_count, Func<long, InputT> assign, PartialResultT parallel_calculate(InputT), ResultT reduce(ResultT total, PartialResultT partial_result), ResultT initial)
{
    std::vector<std::future<ThreadResult<PartialResultT>>> threads(thread_count);

    std::vector<ThreadResult<PartialResultT>> results(thread_count);

    auto start_time = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < thread_count; i++)
    {
        auto input = assign(i);
        threads[i] = std::async(calculate<InputT, PartialResultT>, parallel_calculate, input);
    }

    for (int i = 0; i < thread_count; i++)
    {
        results[i] = threads[i].get();
    }
    auto end_time = std::chrono::high_resolution_clock::now();
    duration elapsed = end_time - start_time;

    ResultT reduce_result = initial;
    duration max_elapsed_children = duration::zero();
    double energy_children = 0.0;

    std::unordered_map<int, std::vector<ThreadResult<PartialResultT>>> result_by_core;
    for (const auto &result : results)
    {
        if (result_by_core.find(result.core_id) == result_by_core.end())
        {
            result_by_core[result.core_id] = std::vector<ThreadResult<PartialResultT>>();
        }
        result_by_core[result.core_id].push_back(result);
    }

    for (const auto &[core_id, core_results] : result_by_core)
    {
        duration time = duration::zero();
        double energy = 0.0;
        for (size_t i = 0; i < core_results.size(); i++)
        {
            time.operator+=(core_results[i].time());
            energy += core_results[i].energy();
            reduce_result = reduce(reduce_result, core_results[i].result);
        }
        std::cout << "Core " << core_id << " results: \n";
        std::cout << "Total time: " << time.count() << " s";
        std::cout << ", Total energy: " << energy << " J\n";
        for (const auto &result : core_results)
        {
            std::cout << "Start time: " << std::chrono::duration_cast<std::chrono::milliseconds>(result.start_time.time_since_epoch()).count() << " ms, "
                      << "End time: " << std::chrono::duration_cast<std::chrono::milliseconds>(result.end_time.time_since_epoch()).count() << " ms, "
                      << "Duration: " << result.time().count() << " s, "
                      << "Energy: " << result.energy() << " J, "
                      << "Result: " << result.result << "\n";
        }
        std::cout << std::endl
                  << std::endl;
    }

    return reduce_result;
}
