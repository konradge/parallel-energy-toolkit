#include <random>
#include <algorithm>

int* create_random_int_vector(int n, int min_val, int max_val) {
    int* vec = new int[n];

    std::random_device rd;

    std::mt19937 gen(rd());

    std::uniform_int_distribution<> distrib(min_val, max_val);

    std::generate(vec, vec + n, [&]() {
        return distrib(gen);
    });

    return vec;
}

/** ----- Reduce functions ------ */

void sum(size_t thread_count, int partial_result, int& result) {
  result += partial_result;
}

template<typename P>
void average(size_t thread_count, P partial_result, double& total) {
  total += partial_result / thread_count;
}