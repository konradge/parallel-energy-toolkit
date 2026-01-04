#include "measurement.hpp"

#include <pthread.h>
#include <sched.h>

#include <cassert>
#include <iostream>

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

void TimeMeasurement::start() {
  this->start_time = std::chrono::high_resolution_clock::now();
}

void TimeEnergyMeasurement::start() {
  TimeMeasurement::start();
  this->core_id = pin_to_thread();
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