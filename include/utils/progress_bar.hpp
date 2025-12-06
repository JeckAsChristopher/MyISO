#ifndef PROGRESS_BAR_HPP
#define PROGRESS_BAR_HPP

#include <string>
#include <chrono>

class ProgressBar {
private:
    size_t total;
    size_t current;
    int barWidth;
    std::chrono::time_point<std::chrono::steady_clock> startTime;
    std::string label;
    
public:
    ProgressBar(size_t totalSize, const std::string& taskLabel = "Progress");
    void update(size_t currentSize);
    void finish();
    
private:
    std::string formatTime(double seconds);
    std::string formatSize(size_t bytes);
};

#endif // PROGRESS_BAR_HPP
