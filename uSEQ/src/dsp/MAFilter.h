#ifndef MAFILTER
#define MAFILTER

#include <vector>
#include <numeric>
#include <functional>
#include <algorithm>
#include <math.h>

class MovingAverageFilter
{
public:
    MovingAverageFilter(std::size_t filterSize) {
        init(filterSize);
    }

    MovingAverageFilter() {
        init(3);
    }   

    void init(std::size_t fSize) {
        filterSize_ = fSize;
        circularBuffer_ = std::vector<double>(filterSize_, 0.0);
        sum_ = 0.0;
    } 

    double process(double inputValue)
    {
        // Subtract the oldest value from the sum
        sum_ -= circularBuffer_[currentIndex_];

        // Add the new value to the sum
        sum_ += inputValue;

        // Store the new value in the circular buffer
        circularBuffer_[currentIndex_] = inputValue;

        // Move to the next index in the circular buffer
        currentIndex_++;
        if (currentIndex_ == filterSize_) {
          currentIndex_ = 0;
        }

        // Calculate and return the moving average
        return sum_ / filterSize_;
    }

    double std() {
        double sum = std::accumulate(std::begin(circularBuffer_), std::end(circularBuffer_), 0.0);
        double m =  sum / circularBuffer_.size();

        double accum = 0.0;
        std::for_each (std::begin(circularBuffer_), std::end(circularBuffer_), [&](const double d) {
            accum += (d - m) * (d - m);
        });

        return sqrt(accum / (circularBuffer_.size()-1));
    }

private:
    std::size_t filterSize_;
    std::vector<double> circularBuffer_;
    std::size_t currentIndex_ = 0;
    double sum_;
};

#endif