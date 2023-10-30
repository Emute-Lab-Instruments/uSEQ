#ifndef MAFILTER
#define MAFILTER

#include <vector>

class MovingAverageFilter
{
public:
    explicit MovingAverageFilter(std::size_t filterSize)
        : filterSize_(filterSize), circularBuffer_(filterSize, 0.0), sum_(0.0) {}

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

private:
    std::size_t filterSize_;
    std::vector<double> circularBuffer_;
    std::size_t currentIndex_ = 0;
    double sum_;
};

#endif