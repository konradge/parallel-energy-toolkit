#include "measurement.hpp"

#include <pthread.h>
#include <sched.h>

#include <cassert>
#include <iostream>
#include <sys/sysinfo.h>

int pin_to_thread(size_t thread_id) {
  auto machine_thread_count = get_nprocs();
  auto core_id = thread_id % machine_thread_count;
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(core_id, &cpuset);
  int rc = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);

  if (rc != 0) {
    std::cerr << "Error calling pthread_setaffinity_np: " << rc << "\n";
  } else if(core_id != sched_getcpu()) {
    std::cerr << "Warning: Thread not running on the expected core " << core_id
              << " but on core " << sched_getcpu() << "\n";
  }
  return core_id;
}

void TimeMeasurement::start() {
  this->start_time = std::chrono::high_resolution_clock::now();
}

void TimeEnergyMeasurement::start(int thread_id) {
  TimeMeasurement::start();
  this->core_id = pin_to_thread(thread_id);
  this->start_energy = read_intel_msr(this->core_id);
}

void TimeMeasurement::stop() {
  this->end_time = std::chrono::high_resolution_clock::now();
  stopped = true;
}

void TimeEnergyMeasurement::stop() {
  TimeMeasurement::stop();

  this->end_energy = read_intel_msr(this->core_id);

  assert(this->core_id == sched_getcpu() &&
         "Thread did run on multiple cores!");
}