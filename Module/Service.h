#pragma once

#include "Cache.h"
#include "njp/Network.h"

namespace njp::count {
    coroutine::ValueAsync<> run_service(IServer &server, ICache &cache);
}