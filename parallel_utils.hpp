#pragma once

#include <pthread.h>
#include <sched.h>

#include <cassert>
#include <chrono>
#include <iostream>

#include "msr_reader.h"
#include "thread_result.hpp"

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

template <typename InputT, typename PartialResultT>
ThreadResult<PartialResultT> calculate(PartialResultT calc(InputT),
                                       InputT input) {
  ThreadResult<PartialResultT> result;

  result.core_id = pin_to_thread();

  result.start_time = std::chrono::high_resolution_clock::now();
  result.start_energy = read_intel_msr(result.core_id);

  // calculate
  PartialResultT result_value = calc(input);

  result.end_time = std::chrono::high_resolution_clock::now();
  result.end_energy = read_intel_msr(result.core_id);
  result.result = result_value;

  assert(result.core_id == sched_getcpu() &&
         "Thread did run on multiple cores!");

  return result;
}