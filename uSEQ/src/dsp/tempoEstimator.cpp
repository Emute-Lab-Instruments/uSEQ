#include "tempoEstimator.h"

double tempoEstimator::averageBPM(double ts_micros) {
  double delta = ts_micros - lastVal;
  lastVal = ts_micros;
  double avg = avgBeat.process(delta);
  if (avg != 0) {
    avgBPM = 60000000.0/avg;
  }
  return avgBPM;
}
