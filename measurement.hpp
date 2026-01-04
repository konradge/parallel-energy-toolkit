#include <chrono>
#include <iostream>

#include "msr_reader.h"

typedef std::chrono::time_point<std::chrono::high_resolution_clock> time_point;
typedef std::chrono::duration<double> duration;

class TimeMeasurement {
 public:
  void start();
  void stop();

  double time() const {
    if (!stopped) {
      std::cerr << "Warning: Measurement not stopped before reading time!"
                << std::endl;
    }
    return (end_time - start_time).count();
  };

  time_point start_time;
  time_point end_time;

 protected:
  bool stopped = false;
};

class TimeEnergyMeasurement : public TimeMeasurement {
 public:
  void start();
  void stop();
  double energy() const {
    if (!stopped) {
      std::cerr << "Warning: Measurement not stopped before reading time!"
                << std::endl;
    }
    return end_energy - start_energy;
  }

  double start_energy;
  double end_energy;
  int core_id;
};