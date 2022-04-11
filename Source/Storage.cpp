#include "Json.h"
#include "Storage.h"
#include <kls/phttp/Transport.h>

namespace count {
    using namespace kls;
    using namespace kls::coroutine;

    class Storage : public IStorage {
    public:
        explicit Storage(std::unique_ptr<phttp::Endpoint> e) : m_client(std::move(e)) {}

        ValueAsync<temp::vector<int64_t>> fetch(temp::vector<std::string_view> names) override {
            auto result = co_await m_client.exec("GET", "/kv/ints", to_json<temp_json>(names));
            co_return from_json<temp::vector<int64_t>>(result);
        }

        ValueAsync<> store(temp::unordered_map<std::string_view, int64_t> elem) override {
            co_await m_client.exec("PUT", "/kv/ints", to_json<temp_json>(elem));
        }

        ValueAsync<> close() override { return m_client.close(); }
    public:
        JsonClient m_client;
    };

    ValueAsync<std::shared_ptr<IStorage>> connect_remote_storage(io::Peer remote) {
        co_return std::make_shared<Storage>(co_await phttp::connect_tcp(remote));
    }
}