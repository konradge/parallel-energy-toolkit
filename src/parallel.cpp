#include "parallel.hpp"

#include <algorithm>
#include <unordered_map>

#include "measurement.hpp"

namespace ParallelToolkit {
namespace detail {
double calculate_total_energy(const std::vector<TimeEnergyMeasurement*>& results,
                              size_t& distinct_used_machine_threads) {
  double total_energy = 0;
  /**
   * For measuring the consumed energy the results from each thread need to be
   * grouped by machine-thread, since they may have been scheduled at the same
   * time and * thus overlap. Then, at every point in time, only one of the
   * measured energies is taken for overlapping jobs.
   */

  // Group thread results by core they were calculated on
  std::unordered_map<int, std::vector<TimeEnergyMeasurement>> result_by_core;
  for (const auto& result : results) {
    if (result_by_core.find(result->thread_id) == result_by_core.end()) {
      result_by_core[result->thread_id] = std::vector<TimeEnergyMeasurement>();
    }
    result_by_core[result->thread_id].push_back(*result);
  }
  distinct_used_machine_threads = result_by_core.size();

  for (const auto& [core_id, core_results] : result_by_core) {
    // sort core_results by start_time
    std::vector<TimeEnergyMeasurement> sorted_core_results = core_results;
    std::sort(
        sorted_core_results.begin(), sorted_core_results.end(),
        [](const TimeEnergyMeasurement& a, const TimeEnergyMeasurement& b) {
          return a.start_time < b.start_time;
        });
    double core_energy = 0.0;

    TimeEnergyMeasurement previous_result = sorted_core_results[0];
    double last_start_energy = previous_result.start_energy;
    double last_end_energy = previous_result.end_energy;
    for (size_t i = 1; i < core_results.size(); i++) {
      auto current_result = sorted_core_results[i];
      if (previous_result.end_time < current_result.start_time) {
        /**
         * No overlap -> take the energy consumption of i:
         *  previous_res     current_res
         * <-----i-1----->
         *                  <-----i----->
         */
        core_energy += last_end_energy - last_start_energy;
        last_start_energy = current_result.start_energy;
        last_end_energy = current_result.end_energy;
      } else {
        /**
         * Overlap, one of the following cases:
         *
         * <------i-1------>
         *            <------i------->
         * -> start_energy = start[i-1], end_energy = end[i]
         * or:
         * <------i-1----------->
         *     <---i--->
         * -> start_energy = start[i-1], end_energy = end[i-1]
         */
        last_end_energy = std::max(last_end_energy, current_result.end_energy);
      }
    }
    core_energy += last_end_energy - last_start_energy;

    total_energy += core_energy;
  }

  return total_energy;
}
}  // namespace detail
}  // namespace ParallelToolkit