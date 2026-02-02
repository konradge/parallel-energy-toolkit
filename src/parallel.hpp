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
using WorkerFunc = T (*)(size_t worker_id, size_t total_workers);

template <typename PartialT, typename ResultT = PartialT>
using ReduceFunc = void (*)(size_t total_workers, PartialT partial, ResultT& accumulator);

/**
 * --------------- Public Interface ---------------
 */

 /**
 * Runs a benchmark with the given function, outputting the results to a CSV file.
 *
 * @param worker_func Function to be executed in parallel.
 * @param reduce_func Function to combine results from each core.
 * @param sample_size Number of benchmark iterations for each thread count.
 * @param max_workers Maximum number of workers to use (executes 2..max).
 * @param output_path CSV file path for results.
 */
template <typename PartialT, typename ResultT = PartialT>
void benchmark(WorkerFunc<PartialT> worker_func,
               ReduceFunc<PartialT, ResultT> reduce_func,
               size_t sample_size, 
               size_t max_workers,
               const std::string& output_path);

/**
 * Runs the given function in parallel using the specified number of workers.
 *
 * @return The combined result from all workers.
 */
               template <typename PartialT, typename ResultT = PartialT>
ResultT run(WorkerFunc<PartialT> worker_func,
            ReduceFunc<PartialT, ResultT> reduce_func,
            size_t worker_count);

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
BenchmarkResult run_single_benchmark(WorkerFunc<PartialT> worker_func,
                                     ReduceFunc<PartialT, ResultT> reduce_func,
                                     size_t worker_count);

double calculate_total_energy(const std::vector<TimeEnergyMeasurement*>& measurements,
                             size_t& distinct_used_machine_threads);

} // namespace detail

/**
 * --------------- Implementations ---------------
 */

template <typename PartialT, typename ResultT>
void benchmark(WorkerFunc<PartialT> worker_func,
               ReduceFunc<PartialT, ResultT> reduce_func,
               size_t sample_size, 
               size_t max_workers,
               const std::string& output_path) {
    
    std::cout << "Warming up on " << max_workers << " workers...\n";
    for (int i = 0; i < 10; ++i) {
        std::cout << "\r(1/2) Warmup: Iteration " << std::setfill(' ') << std::setw(2) << (i + 1) << " / 10" << std::flush;
        run(worker_func, reduce_func, max_workers);
    }
    std::cout << "\n";

    std::ofstream csv(output_path);
    csv << "worker_count,distinct_threads,runtime_ns,energy\n";

    for (size_t worker_count = 2; worker_count <= max_workers; ++worker_count) {
        for (size_t sample_iteration = 0; sample_iteration < sample_size; ++sample_iteration) {
            std::cout << "\r(2/2) Benchmark: Workers: " << worker_count << "/" << max_workers 
                      << " | Sample: " << (sample_iteration + 1) << "/" << sample_size << std::flush;

            const auto res = detail::run_single_benchmark(worker_func, reduce_func, worker_count);

            csv << worker_count << "," << res.distinct_used_machine_threads << "," 
                << res.runtime << "," << (res.parallel_energy + res.combine_energy) << "\n";
        }
    }
    std::cout << "\nDone. Writing file to " << output_path << "\n";
}

template <typename PartialT, typename ResultT>
ResultT run(WorkerFunc<PartialT> worker_func,
            ReduceFunc<PartialT, ResultT> reduce_func,
            size_t worker_count) {
    
    std::vector<std::future<PartialT>> futures;
    futures.reserve(worker_count);

    for (size_t i = 0; i < worker_count; ++i) {
        futures.push_back(std::async(std::launch::async, worker_func, i, worker_count));
    }

    ResultT accumulator{};
    for (auto& f : futures) {
        reduce_func(worker_count, f.get(), accumulator);
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
    WorkerFunc<PartialT> worker_func,
    ReduceFunc<PartialT, ResultT> reduce_func,
    size_t worker_count) {

    using MeasuredResult = ThreadMeasurementWithData<PartialT>;
    
    std::vector<std::future<MeasuredResult>> futures;
    futures.reserve(worker_count);

    TimeMeasurement parent_time;
    parent_time.start();

    // 1. Launch with explicit async policy to prevent deferred execution
    for (size_t i = 0; i < worker_count; ++i) {
        futures.push_back(std::async(std::launch::async, [=]() {
            MeasuredResult m;
            m.start(i);
            m.value = worker_func(i, worker_count);
            m.stop();
            return m;
        }));
    }

    // 2. Collect Results
    std::vector<MeasuredResult> results;
    results.reserve(worker_count);
    for (auto& f : futures) {
        results.push_back(f.get());
    }

    // 3. Reduction Phase (Measured for energy)
    ResultT final_val{};
    TimeEnergyMeasurement reduction_energy;
    
    reduction_energy.start(0);
    for (const auto& res : results) {
        reduce_func(worker_count, res.value, final_val);
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