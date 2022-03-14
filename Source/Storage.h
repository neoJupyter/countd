#pragma once

#include <utility>
#include <string_view>
#include <kls/temp/STL.h>
#include <kls/coroutine/ValueAsync.h>

namespace count {
    using namespace kls;
    using namespace kls::coroutine;
    using namespace kls::essential;

    struct IStorage {
        virtual ValueAsync<temp::vector<int64_t>> fetch(temp::vector<std::string_view> names) = 0;
        virtual ValueAsync<void> store(temp::vector<std::pair<std::string_view, int64_t>> elements) = 0;
    };
}