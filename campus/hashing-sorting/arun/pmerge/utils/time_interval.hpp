#include <chrono>
#include <cstddef>
#include <iostream>
class TimeInterval {
public:
    std::chrono::nanoseconds Reset() {
        auto now = std::chrono::steady_clock::now();
        auto diff = now - previous_;
        previous_ = now;
        return diff;
    }

private:
    std::chrono::time_point<std::chrono::steady_clock> previous_{};
};

class WriteInterval {
public:
    WriteInterval() {
        interval_.Reset();
    }
    void WriteHappend(size_t bytes) {
        std::cout << std::format("{} writes done in {}", bytes, interval_.Reset())
                  << std::endl;
    }

private:
    TimeInterval interval_{};
};

static constexpr int kCurrentIntervalSlots = 1 << 17;
