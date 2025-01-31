#include <sys/time.h>
#include <fstream>
#include <iostream>
#include <ostream>
#include <sstream>
#include <string>
#include <vector>
#include <ctime> // For timestamp


uint64_t get_time_us()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000000 + tv.tv_usec;
}

std::string get_timestamped_filename(const std::string &base_name)
{
    std::time_t now = std::time(nullptr);
    char buf[100];
    if (std::strftime(buf, sizeof(buf), "%Y%m%d_%H%M%S", std::localtime(&now)))
    {
        return base_name + "_" + buf + ".yml";
    }
    return base_name + ".yml";
}

std::string join(const std::vector<int>& vec, const std::string& delimiter)
{
    std::ostringstream oss;

    for (size_t i = 0; i < vec.size(); ++i) {
        oss << vec[i];
        if (i != vec.size() - 1) { // Avoid adding a delimiter after the last element
            oss << delimiter;
        }
    }

    return oss.str();
}

std::pair<std::string, std::string> split(const std::string &input, std::string delimiter = "->")
{
    size_t pos = input.find(delimiter);
    if (pos == std::string::npos)
    {
        // If "->" is not found, return empty strings or handle as needed
        return {"", ""};
    }

    std::string first = input.substr(0, pos);
    std::string second = input.substr(pos + delimiter.length());
    return {first, second};
}
