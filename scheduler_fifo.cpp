#include "scheduler_fifo.hpp"

XBT_LOG_NEW_DEFAULT_CATEGORY(fifo_scheduler, "Messages specific to this module.");

FIFO_Scheduler::FIFO_Scheduler(const common_t *common, simgrid_execs_t &dag) : Base_Scheduler(common, dag)
{
}

FIFO_Scheduler::~FIFO_Scheduler()
{
}

std::tuple<int, double> FIFO_Scheduler::get_best_core_id(const simgrid_exec_t *exec)
{
    int best_core_id = -1;
    int best_numa_id = -1;
    double earliest_finish_time_us = 0.0;
    std::vector<int> avail_core_ids = common_get_avail_core_ids(this->common);

    if (avail_core_ids.empty()) return {best_core_id, earliest_finish_time_us};

    // ASSUMPTION:
    // If data item pages are spread across multiple NUMA domains, we assume
    // the data is evenly distributed, i.e., all data pages have the same size.

    /* Count the amount of data (bytes) to be read per memory domain. */
    std::unordered_map<int, double> numa_id_to_payload;
    for (const auto &[comm_name, time_range_payload] : 
        common_filter_name_to_time_range_payload(this->common, exec->get_name(), COMM_WRITE_OFFSETS, COMM_DST))
    {
        std::vector<int> numa_ids = this->common->comm_name_to_numa_ids_w.at(comm_name);
        for (int numa_id : numa_ids)
            numa_id_to_payload[numa_id] += std::get<2>(time_range_payload) / numa_ids.size();
    }

    // Prioritize cores associated with NUMA nodes that have the most data to read.
    std::sort(avail_core_ids.begin(), avail_core_ids.end(),
              [&](int a, int b) {
                int a_numa_id = get_hwloc_numa_id_by_core_id(this->common, a);
                int b_numa_id = get_hwloc_numa_id_by_core_id(this->common, b);

                double a_score = (numa_id_to_payload.find(a_numa_id) != numa_id_to_payload.end()) ? numa_id_to_payload.at(a_numa_id) : 0.0;
                double b_score = (numa_id_to_payload.find(b_numa_id) != numa_id_to_payload.end()) ? numa_id_to_payload.at(b_numa_id) : 0.0; 

                return a_score > b_score; 
            });

    for (int avail_core_id : avail_core_ids) {
        XBT_DEBUG("avail_core_id: %d, avail_core_until: %f", avail_core_id,
            common_get_core_id_avail_unitl(this->common, avail_core_id));
    }

    if (this->common->mapper_type == SIMULATION)
    {
        // Get the current simulation time.
        double current_simulation_time = std::numeric_limits<double>::max();
        for (int avail_core_id : avail_core_ids)
            current_simulation_time = std::min(current_simulation_time,
                common_get_core_id_avail_unitl(this->common, avail_core_id));

        XBT_DEBUG("current_simulation_time: %f", current_simulation_time);

        // Find the first available core
        best_core_id = *std::find_if(avail_core_ids.begin(), avail_core_ids.end(), [&](int core_id) {
            return this->common->core_avail_until[core_id] <= current_simulation_time;
        });

    } else {
        best_core_id = avail_core_ids.front();
    }

    best_numa_id = get_hwloc_numa_id_by_core_id(this->common, best_core_id);

    XBT_DEBUG("best_core_id: %d, best_numa_id: %d", best_core_id, best_numa_id);

    double earliest_start_time_us = common_earliest_start_time(this->common, exec->get_name(), best_core_id);

    double estimated_read_time_us = 0.0;

    for (const auto &[comm_name, time_range_payload] : 
        common_filter_name_to_time_range_payload(this->common, exec->get_name(), COMM_WRITE_OFFSETS, COMM_DST))
    {
        double read_payload_bytes = (double) std::get<2>(time_range_payload);

        // ASSUMPTION:
        // For the read time estimation, we assume that the entire data item is stored in a single memory domain, the first one.
        int read_src_numa_id = this->common->comm_name_to_numa_ids_w.at(comm_name).front();

        estimated_read_time_us = std::max(
            estimated_read_time_us, common_communication_time(this->common, read_src_numa_id, best_numa_id, read_payload_bytes));
    }

    double flops = exec->get_remaining();
    double processor_speed_flops_per_second = (double) get_hwloc_core_performance_by_id(this->common, best_core_id);
    double estimated_compute_time_us = common_compute_time(this->common, flops, processor_speed_flops_per_second);

    double estimated_write_time_us = 0.0;

    for (const auto &succ : exec->get_successors())
    {
        const simgrid_comm_t *comm = dynamic_cast<simgrid_comm_t *>(succ.get());
        double write_payload_bytes = comm->get_remaining();

        // Skipt all task_i->end communications.
        auto [exec_name, succ_exec_name] = common_split(comm->get_cname(), "->");
        if (succ_exec_name == std::string("end")) continue;

        // Determine the NUMA node that will handle the write operation.
        // ASSUMPTION:
        // Since it is uncertain which NUMA node will handle the write operations for this task,
        // writes will follow the first-touch policy, i.e., data will be saved in 
        // the numa node that share locality with the core_id.

        estimated_write_time_us = std::max(
            estimated_write_time_us, common_communication_time(
                this->common, best_numa_id, best_numa_id, write_payload_bytes));
    }

    earliest_finish_time_us =
        earliest_start_time_us + estimated_read_time_us + estimated_compute_time_us + estimated_write_time_us;

    return {best_core_id, earliest_finish_time_us};
}

std::tuple<simgrid_exec_t *, int, double> FIFO_Scheduler::next()
{
    int selected_core_id = -1;  
    double estimated_finish_time = 0.0;  
    simgrid_exec_t *selected_exec = nullptr;  

    simgrid_execs_t ready_execs = common_get_ready_tasks(this->dag);

    if (ready_execs.empty()) return {selected_exec, selected_core_id, estimated_finish_time};

    auto name_to_data_locality_score = this->get_data_locality_scores(ready_execs);

    std::sort(ready_execs.begin(), ready_execs.end(), [&](simgrid_exec_t *a, simgrid_exec_t *b) {
        return name_to_data_locality_score[a->get_name()] >
               name_to_data_locality_score[b->get_name()]; // Higher score first
    });
    
    // Append only new ready tasks to the queue
    for (const auto &exec : ready_execs) {
        if (std::find(this->queue.begin(), this->queue.end(), exec) == this->queue.end()) {
            this->queue.push_back(exec);
        }
    }

    selected_exec = this->queue.front();
    this->queue.pop_front();
    
    // Select the best core for execution
    std::tie(selected_core_id, estimated_finish_time) = this->get_best_core_id(selected_exec);
    
    XBT_DEBUG("selected_task: %s, selected_core_id: %d, estimated_finish_time: %f",
              selected_exec->get_cname(), selected_core_id, estimated_finish_time);

    return std::make_tuple(selected_exec, selected_core_id, estimated_finish_time);
}

double FIFO_Scheduler::compute_data_locality_score(simgrid_exec_t *exec)
{
    double exec_data_locality_score_bytes = 0.0;

    // Get all writtings (data items) where this exec is the destionation.
    name_to_time_range_payload_t comm_name_to_time_range_payload =
        common_filter_name_to_time_range_payload(this->common, exec->get_name(), COMM_WRITE_OFFSETS, COMM_DST);

    for (const auto &[comm_name, time_range_payload] : comm_name_to_time_range_payload)
    {
        exec_data_locality_score_bytes += std::get<2>(time_range_payload);
    }

    return exec_data_locality_score_bytes;
}

std::unordered_map<std::string, double> FIFO_Scheduler::get_data_locality_scores(simgrid_execs_t &execs)
{
    std::unordered_map<std::string, double> name_to_data_locality_scores;
    for (simgrid_exec_t *exec : execs)
    {
        name_to_data_locality_scores[exec->get_name()] = this->compute_data_locality_score(exec);
    }

    return name_to_data_locality_scores;
}
