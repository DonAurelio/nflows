#include "mapper_bare_metal.hpp"

XBT_LOG_NEW_DEFAULT_CATEGORY(mapper_bare_metal, "Messages specific to this module.");


Mapper_Bare_Metal::Mapper_Bare_Metal(common_t *common, MIN_MIN_Scheduler& scheduler) : common(common), scheduler(scheduler)
{
    /* CREATE DUMMY HOST (CORE) */
    this->dummy_net_zone = simgrid::s4u::create_full_zone("zone0");
    this->dummy_host = dummy_net_zone->create_host("host0", "1Gf")->seal();
    this->dummy_net_zone->seal();
}

Mapper_Bare_Metal::~Mapper_Bare_Metal()
{

}

void Mapper_Bare_Metal::start()
{
    simgrid_exec_t* selected_exec;
    unsigned int selected_core_id;
    unsigned long estimated_completion_time;

    while(this->scheduler.has_next()){
        std::tie(selected_exec, selected_core_id, estimated_completion_time) = this->scheduler.next();

        if (!selected_exec)
        {
            XBT_INFO("There are not ready tasks, waiting 5 seconds.");
            sleep(5);
            continue;
        }

        if (selected_core_id == -1)
        {
            XBT_INFO("There are not available cores, waiting 5 seconds.");
            sleep(5);
            continue;
        }

        // Initialize thread data.
        // data is free'd by the thread at the end of the execution.
        thread_data_t *data = (thread_data_t*) malloc(sizeof(thread_data_t));
        data->exec = selected_exec;
        data->assigned_core_id = selected_core_id;
        data->common = this->common;
        data->thread_function = thread_function;

        // Set as assigned.
        selected_exec->set_host(this->dummy_host);
        bind_exec_to_thread(data);
    }

    common_wait_active_threads(this->common);

    // Workaround to properly finalize SimGrid resources.
    simgrid::s4u::Engine *e = simgrid::s4u::Engine::get_instance();
    e->run();
}

/**
 * @brief Emulate the activities involved in executing a workflow task.
 *
 * 1. Read the required data from memory by accessing a previously created address.
 *    Reads are performed sequentially, but the time recorded assumes they were performed in parallel.
 *
 * 2. Perform the FMA (Fused Multiply-Add) operation to emulate computation.
 *
 * 3. Write the required data to memory, creating a pointer to be accessed by successor tasks.
 *    Writes are performed sequentially, but the time recorded assumes they were performed in parallel.
 *
 * 4. Record the read, write, and execution times.
 *
 * 5. Save thread locality information, including the last NUMA node, core ID, and context switches.
 *
 * @param arg Structure used to collect thread execution data.
 * @return void*
 *
 * @note This method does not support SimGrid control dependencies (0, -1).
 */
void *thread_function(void *arg)
{
    thread_data_t *data = (thread_data_t *)arg;

    XBT_INFO("Process ID: %d, Thread ID: %d, Task ID: %s, Core ID: %d => message: started.", 
        getpid(), gettid(), data->exec->get_cname(), get_hwloc_core_id_by_pu_id(data->common, sched_getcpu()));

    XBT_INFO("Process ID: %d, Thread ID: %d, Task ID: %s, Core ID: %d => message: %s.",
        getpid(), gettid(), data->exec->get_cname(), get_hwloc_core_id_by_pu_id(data->common, sched_getcpu()), 
        get_hwloc_thread_mem_policy(data->common).c_str());

    XBT_INFO("Process ID: %d, Thread ID: %d, Task ID: %s, Core ID: %d => message: finished.", 
        getpid(), gettid(), data->exec->get_cname(), get_hwloc_core_id_by_pu_id(data->common, sched_getcpu()));

    /* CLEAN UP */

    // Mark successors as completed.
    for (const auto& succ_ptr : data->exec->get_successors())
        (succ_ptr.get())->complete(simgrid::s4u::Activity::State::FINISHED);

    // In the previous version, this worked, but in the current version, exec is a null pointer.
    // As a result, the task itself is not marked as completed, but it was assigned,
    // and its dependencies were marked as completed.
    // data->exec->complete(simgrid::s4u::Activity::State::FINISHED);

    // Decrement the active thread counter and signal if no more threads
    common_decrement_active_threads_counter(data->common);

    // Mark the selected hwloc_core_id as available.
    common_set_core_id_as_avail(data->common, data->assigned_core_id);

    // this pointer was created in the thread caller 'assign_exec'
    free(data);

    return NULL;
}
