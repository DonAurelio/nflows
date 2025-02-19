#pragma once

#include "common.hpp"
#include "mapper_bare_metal.hpp"
#include "scheduler_base.hpp"
#include "scheduler_fifo.hpp"
#include "scheduler_heft.hpp"
#include "scheduler_min_min.hpp"

void runtime_initialize(common_t **common, simgrid_execs_t **dag, scheduler_t **scheduler, mapper_t **mapper,
                        const std::string &config_path);
void runtime_finalize(common_t *common, simgrid_execs_t *dag, scheduler_t *scheduler, mapper_t *mapper);
void runtime_start(mapper_t *mapper);
void runtime_write(common_t *common);
