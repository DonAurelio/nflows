#include "mapper_base.hpp"

Mapper_Bare_Metal::Mapper_Base(common_t *common, scheduler_t &scheduler) : common(common), scheduler(scheduler)
{
    /* CREATE DUMMY HOST (CORE) */
    this->dummy_net_zone = simgrid::s4u::create_full_zone("zone0");
    this->dummy_host = dummy_net_zone->create_host("host0", "1Gf")->seal();
    this->dummy_net_zone->seal();
}

void Mapper_Base::start()
{
    simgrid_exec_t *selected_exec;
    int selected_core_id;
    unsigned long estimated_completion_time;

    while (this->scheduler.has_next())
    {

        std::tie(selected_exec, selected_core_id, estimated_completion_time) = this->scheduler.next();

        if (!selected_exec)
        {
            XBT_INFO("There are not ready tasks, waiting 5 seconds.");
            // printf("There are not ready tasks, waiting 5 seconds.\n");
            sleep(5);
            continue;
        }

        if (selected_core_id == -1)
        {
            XBT_INFO("There are not available cores, waiting 5 seconds.");
            // printf("There are not available cores, waiting 5 seconds.\n");
            sleep(5);
            continue;
        }

        // Initialize thread data.
        // data is free'd by the thread at the end of the execution.
        thread_data_t *data = (thread_data_t *)malloc(sizeof(thread_data_t));
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
