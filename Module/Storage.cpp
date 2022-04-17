#include "Storage.h"
#include "njp/Network.h"
#include <kls/phttp/Transport.h>

using namespace njp;
using namespace kls;
using namespace njp::count;
using namespace kls::coroutine;

namespace {
    class RemoteStorage : public IStorage {
    public:
        explicit RemoteStorage(std::shared_ptr<IClient> e) : m_client(std::move(e)) {}

        ValueAsync<temp::vector<int64_t>> fetch(temp::vector<std::string_view> names) override {
            auto result = co_await m_client->exec("GET", "/kv/ints", to_json<temp_json>(names));
            co_return from_json<temp::vector<int64_t>>(result);
        }

        ValueAsync<> store(temp::unordered_map<std::string_view, int64_t> elem) override {
            co_await m_client->exec("PUT", "/kv/ints", to_json<temp_json>(elem));
        }

        ValueAsync<> close() override { return m_client->close(); }
    public:
        std::shared_ptr<IClient> m_client;
    };
}

ValueAsync<std::shared_ptr<IStorage>> njp::count::connect_remote_storage(io::Peer remote) {
    co_return std::make_shared<RemoteStorage>(co_await IClient::create(remote));
}
