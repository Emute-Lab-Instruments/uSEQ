#include "tempoEstimator.h"

double tempoEstimator::averageBPM(int currentValue, int micros) {
  if (lastValue==0) {
    if (currentValue == 1) {
      unsigned long delta = micros - lastTrig;
      lastTrig = micros;
      lastAverage = avgBeat.process(delta);
      if (lastAverage != 0) {
        avgBPM = 60000000.0/lastAverage;
      }
    }
  }
  lastValue = currentValue;
  return avgBPM;
}
