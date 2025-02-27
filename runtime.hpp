#pragma once

#include <iostream>
#include <fstream>
#include <vector>
#include <memory>
#include <nlohmann/json.hpp>

#include "common.hpp"
#include "mapper_bare_metal.hpp"
#include "mapper_simulation.hpp"
#include "scheduler_base.hpp"
#include "scheduler_fifo.hpp"
#include "scheduler_heft.hpp"
#include "scheduler_min_min.hpp"


void runtime_start(mapper_t **mapper);

void runtime_write(common_t **common);

// Main runtime initialization function
void runtime_initialize(common_t **common, simgrid_execs_t **dag, scheduler_t **scheduler, mapper_t **mapper,
    const std::string &config_path);

// Function to free allocated memory and release resources
void runtime_finalize(common_t **common, simgrid_execs_t **dag, scheduler_t **scheduler, mapper_t **mapper);

// Initializes the common data structure
void initialize_common(common_t *common, const nlohmann::json &data);

// Parses core availability mask and determines core count
std::vector<bool> parse_core_avail_mask(uint64_t mask, size_t &core_count);

// Creates a scheduler instance based on the configuration
scheduler_t* create_scheduler(const std::string &type, common_t *cmn, simgrid_execs_t &dag);

// Creates a mapper instance based on the configuration
mapper_t* create_mapper(const std::string &type, common_t *cmn, scheduler_t &scheduler);
