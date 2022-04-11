#pragma once

#include <utility>
#include <string_view>
#include <kls/io/IP.h>
#include <kls/temp/STL.h>
#include <kls/coroutine/Async.h>

namespace count {
    using namespace kls;

    struct IStorage {
        virtual coroutine::ValueAsync<temp::vector<int64_t>> fetch(temp::vector<std::string_view> names) = 0;
        virtual coroutine::ValueAsync<> store(temp::unordered_map<std::string_view, int64_t> elements) = 0;
        virtual coroutine::ValueAsync<> close() = 0;
    };

    coroutine::ValueAsync<std::shared_ptr<IStorage>> connect_remote_storage(io::Peer remote);
}