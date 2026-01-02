#pragma once
#include <chrono>

typedef std::chrono::time_point<std::chrono::high_resolution_clock> time_point;
typedef std::chrono::duration<double> duration;

template <typename T>
class ThreadResult
{
public:
    T result;
    time_point start_time;
    time_point end_time;
    double start_energy;
    double end_energy;

    int core_id;

    duration time() const
    {
        return end_time - start_time;
    }

    double energy() const
    {
        return end_energy - start_energy;
    }
};