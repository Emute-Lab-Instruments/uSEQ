#ifndef TEMPOESTIMATOR_H
#define TEMPOESTIMATOR_H


#include "MAFilter.hpp"

class tempoEstimator {
  public:
    double averageBPM(int currentValue, int millis);
    double avgBPM=0;
  private:
    unsigned long lastTrig=0;
    int lastValue=0;
    MovingAverageFilter avgMillis{MovingAverageFilter(9)};
    double lastAverage=0;
};

#endif