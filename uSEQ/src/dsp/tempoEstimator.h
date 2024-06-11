#ifndef TEMPOESTIMATOR_H
#define TEMPOESTIMATOR_H


#include "MAFilter.h"

class tempoEstimator {
  public:
    tempoEstimator() {
      avgBeat.init(5);
    }
    double std() {
      return avgBeat.std();
    }
    double averageBPM(double micros);
    double avgBPM=0;
  private:
    MovingAverageFilter avgBeat;
    double lastVal=0;
};

#endif