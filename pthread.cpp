#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <vector>
#include <bits/basic_string.h>
#include <cstring>
#include <cmath>
#include <chrono>
#include "msr_reader.h"

struct Result {
    int core_id;
    int total;
    long energy_consumption;
    long time_taken;
};

struct Input {
    int thread_id;
    int* id;
    int age;
    Result result;
};

// Function to find the current core ID from the CPU set
int get_current_core_id(cpu_set_t *cpuset) {
    for (int i = 0; i < CPU_SETSIZE; i++) {
        if (CPU_ISSET(i, cpuset)) {
            return i;
        }
    }
    return -1; // Should not happen in a correctly configured system
}

void *calculate(void *args) {
    pthread_t self = pthread_self();
    cpu_set_t initial_cpuset;
    int core_id = -1;

    // --- 1. Get the current affinity/core assignment ---
    // Note: getaffinity gives the set of cores the thread is *allowed* to run on.
    // In practice, when called immediately, the thread is often still on its
    // initially assigned core. We assume the current core is *one of* the allowed cores.
    int rc = pthread_getaffinity_np(self, sizeof(cpu_set_t), &initial_cpuset);
    if (rc != 0) {
        fprintf(stderr, "Error getting initial affinity: %s\n", strerror(rc));
        return NULL;
    }

    // Since a thread might be allowed on multiple cores initially,
    // we iterate through the set to find *an* allowed core and bind to it.
    core_id = sched_getcpu();

    if (core_id == -1) {
        fprintf(stderr, "Could not determine an allowed core for binding.\n");
        return NULL;
    }

    // --- 2. Create the new, single-core affinity set ---
    cpu_set_t single_core_cpuset;
    CPU_ZERO(&single_core_cpuset);
    CPU_SET(core_id, &single_core_cpuset);

    // --- 3. Enforce the new affinity (bind to the specific core) ---
    rc = pthread_setaffinity_np(self, sizeof(cpu_set_t), &single_core_cpuset);
    if (rc != 0) {
        fprintf(stderr, "Error enforcing affinity to core %d: %s\n", core_id, strerror(rc));
        return NULL;
    }

    // printf("Thread %lu successfully bound to core %d for its whole computation.\n", (unsigned long)self, core_id);

    auto start_time = std::chrono::high_resolution_clock::now();
    double start_energy = read_intel_msr(core_id);

    auto input = (struct Input*)args;
    // fprintf(stdout, "Thread %d starting computation on core %d\n", *input->id, core_id);
    auto core_id_start = sched_getcpu();
    long count = 0;
    for (long i = 0; i < 10000000; i++) {
        count += sin(i) * cos(i) / (tan(i) + 1);
    }
    input->result.total = *input->id + input->age;
    auto core_id_end = sched_getcpu();
    // printf("Thread %d finished computation on core %d\n", *input->id, core_id_end);

        auto end_time = std::chrono::high_resolution_clock::now();
    double end_energy = read_intel_msr(core_id);

    input->result.time_taken = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
    input->result.energy_consumption = end_energy - start_energy;
    input->result.total = count;
    input->result.core_id = core_id;

    pthread_exit((void*)nullptr);
    return nullptr;
}

int main() {
    std::vector<Input> inputs(8);
    std::vector<pthread_t> threads(8);
    for(int i = 0; i < 8; i++) {
        auto input = new Input();
        inputs[i].id = new int(i);
        inputs[i].age = i + 20;
        pthread_create(&threads[i], NULL, calculate, (void *)&inputs[i]);
    } 
    for(int i = 0; i < 8; i++) {
        pthread_join(threads[i], nullptr);
        auto res = inputs[i].result;
        printf("Core %d \t Energy: %ld, \t Time: %ld\n", res.core_id, res.energy_consumption, res.time_taken);
    }
    return 0;
}