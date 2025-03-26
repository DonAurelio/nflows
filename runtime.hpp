#pragma once

#include <iostream>
#include <fstream>
#include <vector>
#include <memory>
#include <exception> // For std::exception
#include <cstdlib>   // For EXIT_FAILURE
#include <nlohmann/json.hpp>
#include <xbt/log.h>

#include "common.hpp"
#include "mapper_bare_metal.hpp"
#include "mapper_simulation.hpp"
#include "scheduler_base.hpp"
#include "scheduler_fifo.hpp"
#include "scheduler_heft.hpp"
#include "scheduler_min_min.hpp"

void runtime_start(mapper_t **mapper);
void runtime_stop(common_t **common);
void runtime_initialize(common_t **common, simgrid_execs_t **dag, scheduler_t **scheduler, mapper_t **mapper, const std::string &config_path);
void runtime_finalize(common_t **common, simgrid_execs_t **dag, scheduler_t **scheduler, mapper_t **mapper);

void common_initialize(common_t *common, const simgrid_execs_t *dag, const nlohmann::json &data);
