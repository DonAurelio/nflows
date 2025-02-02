#include "common.hpp"


class MIN_MIN_Scheduler {
    private:
        const common_t *common;
        const simgrid_execs_t& dag;
    public:
        MIN_MIN_Scheduler(const simgrid_execs_t &dag, const common_t *common);
        
        bool has_next();
        std::tuple<std::string, unsigned int, unsigned long> next();
        std::tuple<unsigned int, unsigned long> get_best_core_id(const simgrid_exec_t *exec);
};