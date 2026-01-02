#include <algorithm>
#include <future>
#include <string>
#include <unordered_map>
#include <vector>

#include "parallel_utils.hpp"
#include "thread_result.hpp"

template <typename ArgumentT, typename PartialResultT,
          typename ResultT = PartialResultT>
class Benchmark {
 public:
  double runtime;
  double energy;

  double max_core_runtime;
  double min_core_runtime;
  double max_core_energy;
  double min_core_energy;

  Benchmark()
      : runtime(0.0),
        energy(0.0),
        max_core_runtime(0.0),
        min_core_runtime(std::numeric_limits<double>::max()),
        max_core_energy(0.0),
        min_core_energy(std::numeric_limits<double>::max()) {}

  void run(std::vector<std::pair<PartialResultT (*)(ArgumentT), ArgumentT>>
               partial_funcs,
           void (*reduce)(ResultT&, PartialResultT));

  std::string csv() {
    return std::to_string(runtime) + ", " + std::to_string(energy) + ", " +
           std::to_string(min_core_runtime) + ", " +
           std::to_string(max_core_runtime) + ", " +
           std::to_string(min_core_energy) + ", " +
           std::to_string(max_core_energy) + "\n";
  }
};

template <typename ArgumentT, typename PartialResultT, typename ResultT>
void Benchmark<ArgumentT, PartialResultT, ResultT>::run(
    std::vector<std::pair<PartialResultT (*)(ArgumentT), ArgumentT>>
        partial_funcs,
    void (*reduce)(ResultT&, PartialResultT)) {
  // start threads
  std::vector<std::future<ThreadResult<PartialResultT>>> threads;
  threads.reserve(partial_funcs.size());
  auto start_time = std::chrono::high_resolution_clock::now();

  for (auto& [function, argument] : partial_funcs) {
    threads.push_back(
        std::async(calculate<ArgumentT, PartialResultT>, function, argument));
  }

  // read results (synchronously wait for threads)
  std::vector<ThreadResult<PartialResultT>> results(threads.size());
  ResultT res;
  for (size_t i = 0; i < threads.size(); i++) {
    results[i] = threads[i].get();

    // reduce to final result (dicard for benchmark)
    reduce(res, results[i].result);
  }

  std::cout << "Result: " << res << std::endl;

  auto end_time = std::chrono::high_resolution_clock::now();
  duration elapsed = end_time - start_time;
  // Since the parent thread takes the longest time, we use its time as the
  // total runtime
  this->runtime = elapsed.count();

  std::unordered_map<int, std::vector<ThreadResult<PartialResultT>>>
      result_by_core;
  for (const auto& result : results) {
    if (result_by_core.find(result.core_id) == result_by_core.end()) {
      result_by_core[result.core_id] =
          std::vector<ThreadResult<PartialResultT>>();
    }
    result_by_core[result.core_id].push_back(result);
  }

  for (const auto& [core_id, core_results] : result_by_core) {
    // sort core_results by start_time
    std::vector<ThreadResult<PartialResultT>> sorted_core_results =
        core_results;
    std::sort(sorted_core_results.begin(), sorted_core_results.end(),
              [](const ThreadResult<PartialResultT>& a,
                 const ThreadResult<PartialResultT>& b) {
                return a.start_time < b.start_time;
              });
    duration core_time = duration::zero();
    double core_energy = 0.0;
    ThreadResult<PartialResultT> last_result = sorted_core_results[0];
    double start_energy = last_result.start_energy;
    double end_energy = last_result.end_energy;
    auto start_time = last_result.start_time;
    auto end_time = last_result.end_time;
    bool last_added = false;
    for (size_t i = 1; i < core_results.size(); i++) {
      auto current_result = sorted_core_results[i];
      if (last_result.end_time > current_result.start_time) {
        // No overlap, take the energy at the end of the last result
        core_energy += last_result.end_energy - start_energy;
        start_energy = current_result.start_energy;
        core_time.operator+=(last_result.end_time - start_time);
        start_time = current_result.end_time;
        last_added = true;
      } else {
        // Overlap, possibly result i+1 completly within result i
        end_time = std::max(end_time, current_result.end_time);
        end_energy = std::max(end_energy, current_result.end_energy);
      }
    }
    if (!last_added) {
      core_energy += end_energy - start_energy;
      core_time += (end_time - start_time);
    }

    this->energy += core_energy;
    this->max_core_energy = std::max(this->max_core_energy, core_energy);
    this->min_core_energy = std::min(this->min_core_energy, core_energy);
    this->max_core_runtime =
        std::max(this->max_core_runtime, core_time.count());
    this->min_core_runtime =
        std::min(this->min_core_runtime, core_time.count());
  }
}