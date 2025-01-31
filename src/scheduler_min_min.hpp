#include "common.hpp"


class MIN_MIN_Scheduler {
    private:
        simgrid_execs_t dag;
    public:
        MIN_MIN_Scheduler(std::string dot_file_path);
        
        bool has_next();
        std::tuple<std::string, unsigned int, unsigned long> next();
};