#pragma once
// fem_routes.hpp -- forward declaration for FEM, batch, and benchmark job route registration.
#ifdef _MSC_VER
#  pragma warning(push)
#  pragma warning(disable: 4127 4244 4456 4702 4996)
#endif
#include "httplib.h"
#ifdef _MSC_VER
#  pragma warning(pop)
#endif
#include "workbench/job_manager.hpp"
#include <functional>
#include <string>

namespace workbench {

void register_fem_routes(
    httplib::Server&              svr,
    JobManager&                   jm,
    const std::string&            results_dir,
    std::function<std::int64_t()> get_ltr_fn = {});

}  // namespace workbench
