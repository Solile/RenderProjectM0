#pragma once

#include <algorithm>
#include <cstdint>

namespace m0::profile {

// Mean and max over the last N samples, for the live overlay. (Percentiles are the scripts' job, T5.)
template <uint32_t N>
class RollingStat {
public:
    void add(double v) {
        samples_[next_] = v;
        next_ = (next_ + 1) % N;
        count_ = std::min(count_ + 1, N);
    }

    double mean() const {
        double sum = 0.0;
        for (uint32_t i = 0; i < count_; ++i) sum += samples_[i];
        return count_ ? sum / count_ : 0.0;
    }

    double max() const {
        double m = 0.0;
        for (uint32_t i = 0; i < count_; ++i) m = std::max(m, samples_[i]);
        return m;
    }

    bool empty() const { return count_ == 0; }

private:
    double samples_[N] = {};
    uint32_t next_ = 0;
    uint32_t count_ = 0;
};

}  // namespace m0::profile
