#pragma once

#include <fstream>
#include <future>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>
#include <numeric>
#include <iomanip>

#include "measurement.hpp"

namespace ParallelToolkit {

/**
 * --------------- Type Definitions ---------------
 */

  template <typename T>
using ThreadFunc = T (*)(size_t thread_id, size_t total_threads);

template <typename PartialT, typename ResultT = PartialT>
using ReduceFunc = void (*)(size_t thread_count, PartialT partial, ResultT& accumulator);

/**
 * --------------- Public Interface ---------------
 */

 /**
 * Runs a benchmark with the given function, outputting the results to a CSV file.
 *
 * @param thread_func Function to be executed in parallel.
 * @param reduce_func Function to combine results from each core.
 * @param sample_size Number of benchmark iterations for each thread count.
 * @param max_threads Maximum number of threads to use (executes 2..max).
 * @param output_path CSV file path for results.
 */
template <typename PartialT, typename ResultT = PartialT>
void benchmark(ThreadFunc<PartialT> thread_func,
               ReduceFunc<PartialT, ResultT> reduce_func,
               size_t sample_size, 
               size_t max_threads,
               const std::string& output_path);

/**
 * Runs the given function in parallel using the specified number of threads.
 *
 * @return The combined result from all threads.
 */
               template <typename PartialT, typename ResultT = PartialT>
ResultT run(ThreadFunc<PartialT> thread_func,
            ReduceFunc<PartialT, ResultT> reduce_func,
            size_t thread_count);

/**
 * --------------- Internal Logic ---------------
 */

namespace detail {

struct BenchmarkResult {
    double runtime;
    double parallel_energy;
    double combine_energy;
    size_t distinct_used_machine_threads;
};

template <typename PartialT, typename ResultT = PartialT>
BenchmarkResult run_single_benchmark(ThreadFunc<PartialT> thread_func,
                                     ReduceFunc<PartialT, ResultT> reduce_func,
                                     size_t thread_count);

double calculate_total_energy(const std::vector<TimeEnergyMeasurement*>& measurements,
                             size_t& distinct_used_machine_threads);

} // namespace detail

/**
 * --------------- Implementations ---------------
 */

template <typename PartialT, typename ResultT>
void benchmark(ThreadFunc<PartialT> thread_func,
               ReduceFunc<PartialT, ResultT> reduce_func,
               size_t sample_size, 
               size_t max_threads,
               const std::string& output_path) {
    
    std::cout << "Warming up on " << max_threads << " threads...\n";
    for (int i = 0; i < 10; ++i) {
        std::cout << "\r(1/2) Warmup: Iteration " << std::setfill(' ') << std::setw(2) << (i + 1) << " / 10" << std::flush;
        run(thread_func, reduce_func, max_threads);
    }
    std::cout << "\n";

    std::ofstream csv(output_path);
    csv << "thread_count,distinct_threads,runtime_ns,energy\n";

    for (size_t thread_count = 2; thread_count <= max_threads; ++thread_count) {
        for (size_t sample_iteration = 0; sample_iteration < sample_size; ++sample_iteration) {
            std::cout << "\r(2/2) Benchmark: Thread " << thread_count << "/" << max_threads 
                      << " | Sample " << (sample_iteration + 1) << "/" << sample_size << std::flush;

            const auto res = detail::run_single_benchmark(thread_func, reduce_func, thread_count);

            csv << thread_count << "," << res.distinct_used_machine_threads << "," 
                << res.runtime << "," << (res.parallel_energy + res.combine_energy) << "\n";
        }
    }
    std::cout << "\nDone. Writing file to " << output_path << "\n";
}

template <typename PartialT, typename ResultT>
ResultT run(ThreadFunc<PartialT> thread_func,
            ReduceFunc<PartialT, ResultT> reduce_func,
            size_t thread_count) {
    
    std::vector<std::future<PartialT>> futures;
    futures.reserve(thread_count);

    for (size_t i = 0; i < thread_count; ++i) {
        futures.push_back(std::async(std::launch::async, thread_func, i, thread_count));
    }

    ResultT accumulator{};
    for (auto& f : futures) {
        reduce_func(thread_count, f.get(), accumulator);
    }
    return accumulator;
}

template <typename T>
struct ThreadMeasurementWithData : public TimeEnergyMeasurement {
    T value;
};

/**
 * --------------- detail:: Implementations ---------------
 */

template <typename PartialT, typename ResultT>
detail::BenchmarkResult detail::run_single_benchmark(
    ThreadFunc<PartialT> thread_func,
    ReduceFunc<PartialT, ResultT> reduce_func,
    size_t thread_count) {

    using MeasuredResult = ThreadMeasurementWithData<PartialT>;
    
    std::vector<std::future<MeasuredResult>> futures;
    futures.reserve(thread_count);

    TimeMeasurement parent_time;
    parent_time.start();

    // 1. Launch with explicit async policy to prevent deferred execution
    for (size_t i = 0; i < thread_count; ++i) {
        futures.push_back(std::async(std::launch::async, [=]() {
            MeasuredResult m;
            m.start(i);
            m.value = thread_func(i, thread_count);
            m.stop();
            return m;
        }));
    }

    // 2. Collect Results
    std::vector<MeasuredResult> results;
    results.reserve(thread_count);
    for (auto& f : futures) {
        results.push_back(f.get());
    }

    // 3. Reduction Phase (Measured for energy)
    ResultT final_val{};
    TimeEnergyMeasurement reduction_energy;
    
    reduction_energy.start(0);
    for (const auto& res : results) {
        reduce_func(thread_count, res.value, final_val);
    }
    reduction_energy.stop();
    parent_time.stop();

    // 4. Energy calculation needs pointers to the base class
    std::vector<TimeEnergyMeasurement*> time_energy_measurement_ptrs;
    time_energy_measurement_ptrs.reserve(results.size());
    for (auto& r : results) time_energy_measurement_ptrs.push_back(&r);

    BenchmarkResult out;
    out.runtime = parent_time.time();
    out.combine_energy = reduction_energy.energy();
    out.parallel_energy = calculate_total_energy(time_energy_measurement_ptrs, out.distinct_used_machine_threads);

    return out;
}

} // namespace ParallelToolkit