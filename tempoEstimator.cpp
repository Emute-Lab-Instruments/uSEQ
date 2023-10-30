#include "tempoEstimator.h"

double tempoEstimator::averageBPM(int currentValue, int millis) {
  if (lastValue==0) {
    if (currentValue == 1) {
      unsigned long delta = millis - lastTrig;
      lastTrig = millis;
      lastAverage = avgMillis.process(delta);
      if (lastAverage != 0) {
        avgBPM = 60000.0/lastAverage;
      }
    }
  }
  lastValue = currentValue;
  return avgBPM;
}
