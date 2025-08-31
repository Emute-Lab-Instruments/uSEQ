#ifndef MOCKTEMPOESTIMATOR_H_
#define MOCKTEMPOESTIMATOR_H_

#include "../ITempoEstimator.h"
#include <vector>
#include <cmath>

class MockTempoEstimator : public ITempoEstimator {
public:
    MockTempoEstimator(double fixedBPM = 120.0) 
        : fixed_bpm_(fixedBPM), last_micros_(0) {}
    
    ~MockTempoEstimator() override = default;
    
    double averageBPM(double micros) override {
        if (last_micros_ > 0) {
            double delta = micros - last_micros_;
            // Calculate BPM from time between beats (in microseconds)
            double bpm = 60000000.0 / delta;
            beat_intervals_.push_back(bpm);
            
            // Keep only last N samples for moving average
            if (beat_intervals_.size() > window_size_) {
                beat_intervals_.erase(beat_intervals_.begin());
            }
            
            // Calculate average
            if (!beat_intervals_.empty()) {
                double sum = 0;
                for (double val : beat_intervals_) {
                    sum += val;
                }
                avg_bpm_ = sum / beat_intervals_.size();
            }
        }
        last_micros_ = micros;
        return avg_bpm_;
    }
    
    double std() override {
        if (beat_intervals_.size() < 2) return 0.0;
        
        double mean = avg_bpm_;
        double sum_sq = 0;
        for (double val : beat_intervals_) {
            sum_sq += (val - mean) * (val - mean);
        }
        return sqrt(sum_sq / beat_intervals_.size());
    }
    
    double getAvgBPM() const override {
        return avg_bpm_;
    }
    
    // Test helper methods
    void setFixedBPM(double bpm) { 
        fixed_bpm_ = bpm; 
        avg_bpm_ = bpm;
        beat_intervals_.clear();
        beat_intervals_.push_back(bpm);
    }
    
    void reset() {
        beat_intervals_.clear();
        last_micros_ = 0;
        avg_bpm_ = fixed_bpm_;
    }
    
private:
    double fixed_bpm_;
    double avg_bpm_ = 120.0;
    double last_micros_;
    std::vector<double> beat_intervals_;
    size_t window_size_ = 5;
};

#endif // MOCKTEMPOESTIMATOR_H_