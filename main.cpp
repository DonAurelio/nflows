#include "runtime.hpp"
#include <xbt/log.h>

XBT_LOG_NEW_DEFAULT_CATEGORY(n_flow, "Messages specific to this module.");

int main(int argc, char *argv[])
{
    // Initialize XBT logging system
    xbt_log_init(&argc, argv);

    common_t *common = nullptr;
    simgrid_execs_t *dag = nullptr;
    scheduler_t *scheduler = nullptr;
    mapper_t *mapper = nullptr;

    try {
        runtime_initialize(&common, &dag, &scheduler, &mapper, argv[1]);
        runtime_start(&mapper);
        runtime_stop(&common);
        runtime_finalize(&common, &dag, &scheduler, &mapper);
    }
    catch (const std::out_of_range &e) {
        XBT_ERROR("Error (out of range): %s", e.what());
        runtime_stop(&common);
        runtime_finalize(&common, &dag, &scheduler, &mapper);
        std::exit(EXIT_FAILURE);
    } 
    catch (const std::bad_alloc &e) {
        XBT_ERROR("Error (memory allocation failed): %s", e.what());
        runtime_stop(&common);
        runtime_finalize(&common, &dag, &scheduler, &mapper);
        std::exit(EXIT_FAILURE);
    }
    catch (const nlohmann::detail::type_error &e) {
        runtime_stop(&common);
        runtime_finalize(&common, &dag, &scheduler, &mapper);
        std::exit(EXIT_FAILURE);
    }
    catch (const std::runtime_error &e) {
        XBT_ERROR("Runtime Error: %s", e.what());
        runtime_stop(&common);
        runtime_finalize(&common, &dag, &scheduler, &mapper);
        std::exit(EXIT_FAILURE);
    }
    catch (const std::exception &e) {
        XBT_ERROR("Standard Exception: %s", e.what());
        runtime_stop(&common);
        runtime_finalize(&common, &dag, &scheduler, &mapper);
        std::exit(EXIT_FAILURE);
    }
    catch (...) {
        XBT_ERROR("Unknown Error: An unexpected exception occurred.");
        runtime_stop(&common);
        runtime_finalize(&common, &dag, &scheduler, &mapper);
        std::exit(EXIT_FAILURE);
    }

    return 0;
}