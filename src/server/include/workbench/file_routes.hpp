#pragma once
#ifdef _MSC_VER
#  pragma warning(push)
#  pragma warning(disable: 4127 4244 4456 4702 4996)
#endif
#include "httplib.h"
#ifdef _MSC_VER
#  pragma warning(pop)
#endif
#include "workbench/file_store.hpp"
namespace workbench {
void register_file_routes(httplib::Server& svr, FileStore& store);
}