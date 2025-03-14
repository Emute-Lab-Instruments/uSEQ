#ifndef USEQ_SIGNALS_H_
#define USEQ_SIGNALS_H_

/**
 * This file contains signal processing functions for uSEQ.
 * It handles generation, processing, and routing of signals 
 * for the various output types.
 */

// Filter implementation from uSEQ.cpp
class maxiFilter
{
private:
    double z = 0;
    double output = 0;

public:
    maxiFilter() {}
    double lopass(double input, double cutoff);
};

// External instances 
extern maxiFilter cvInFilter[2];

#endif // USEQ_SIGNALS_H_