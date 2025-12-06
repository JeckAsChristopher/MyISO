#include "utils/progress_bar.hpp"
#include "utils/colors.hpp"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <cmath>

ProgressBar::ProgressBar(size_t totalSize, const std::string& taskLabel)
    : total(totalSize), current(0), barWidth(50), label(taskLabel) {
    startTime = std::chrono::steady_clock::now();
}

void ProgressBar::update(size_t currentSize) {
    current = currentSize;
    
    double progress = total > 0 ? static_cast<double>(current) / total : 0.0;
    int pos = static_cast<int>(barWidth * progress);
    
    auto now = std::chrono::steady_clock::now();
    double elapsed = std::chrono::duration<double>(now - startTime).count();
    double speed = elapsed > 0 ? current / elapsed : 0;
    double remaining = (total - current) / (speed > 0 ? speed : 1);
    
    std::cout << "\r" << Colors::cyan(label) << ": [";
    
    for (int i = 0; i < barWidth; ++i) {
        if (i < pos) std::cout << Colors::green("=");
        else if (i == pos) std::cout << Colors::green(">");
        else std::cout << " ";
    }
    
    std::cout << "] " << std::fixed << std::setprecision(1) << (progress * 100.0) << "% ";
    std::cout << formatSize(current) << "/" << formatSize(total) << " ";
    std::cout << Colors::yellow("ETA: " + formatTime(remaining)) << " ";
    std::cout << Colors::blue("(" + formatSize(speed) + "/s)");
    std::cout.flush();
}

void ProgressBar::finish() {
    current = total;
    update(total);
    std::cout << std::endl;
    
    auto now = std::chrono::steady_clock::now();
    double elapsed = std::chrono::duration<double>(now - startTime).count();
    std::cout << Colors::green("Completed in " + formatTime(elapsed)) << std::endl;
}

std::string ProgressBar::formatTime(double seconds) {
    if (std::isnan(seconds) || std::isinf(seconds) || seconds < 0) {
        return "--:--";
    }
    
    int mins = static_cast<int>(seconds) / 60;
    int secs = static_cast<int>(seconds) % 60;
    
    std::ostringstream oss;
    oss << std::setfill('0') << std::setw(2) << mins << ":" 
        << std::setfill('0') << std::setw(2) << secs;
    return oss.str();
}

std::string ProgressBar::formatSize(size_t bytes) {
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    int unit = 0;
    double size = static_cast<double>(bytes);
    
    while (size >= 1024.0 && unit < 4) {
        size /= 1024.0;
        unit++;
    }
    
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << size << " " << units[unit];
    return oss.str();
}
