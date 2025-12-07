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

int thread_count = 4;

double parallel_calculate(long long num_iterations) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<double> dis(0.0, 1.0);

    long long hits = 0;

    for (long long i = 0; i < num_iterations; ++i) {
        double x = dis(gen);
        double y = dis(gen);

        // Check if point is within the unit circle (x^2 + y^2 <= 1)
        if (x * x + y * y <= 1.0) {
            hits++;
        }
    }
    return 4.0 * hits / num_iterations;
}

double reduce(double total, double partial_result) {
    std::cout << std::setprecision(15) << partial_result << std::endl;
    return total + partial_result / thread_count;
}

int main() {
    Func<long, long long> map = [](long process_id) {
        return 2000000 / thread_count;
    };

    double result = 0;
    result = parallel(thread_count, map, parallel_calculate, reduce, result);
    std::cout << "Test function output: " << std::setprecision(15) << result << std::endl;
}