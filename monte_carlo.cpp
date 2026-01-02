#include <iostream>
#include <vector>
#include <random>
#include <future>
#include <numeric>
#include <chrono>
#include <thread>
#include <iomanip>
#include <format>

#include "parallel.hpp"

int thread_count;

// function that is executed in parallel
double parallel_calculate(long long num_iterations) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<double> dis(0.0, 1.0);

    long long hits = 0;

    for (long long i = 0; i < num_iterations; ++i) {
        double x = dis(gen);
        double y = dis(gen);

        if (x * x + y * y <= 1.0) {
            hits++;
        }
    }
    return 4.0 * hits / num_iterations;
}

// function to combine results
void reduce(double& total, double partial_result) {
    total += partial_result / thread_count;
}

int main(int argc, char* argv[]) {
    ParallelProgram<long long, double> program;
    
    thread_count = std::stoi(argv[1]);
    long long precision = argc > 2 ? std::stoll(argv[2]) : 100000000;

    for(size_t i = 0; i < thread_count; i++) {
        program.register_function(parallel_calculate, precision / thread_count);
    }
    double result = 0;
    program.benchmark(reduce, 50);
    // program.get_result(reduce, result);
    // std::cout << "Test function output: " << std::setprecision(15) << result << std::endl;
}