#include "Service.h"
#include <iostream>
#include <kls/io/Block.h>
#include <kls/coroutine/Operation.h>

using namespace kls::coroutine;

namespace njp::count {
    class AsyncRAII {
    public:
        template<class T>
        requires requires(T item) {
            { item.get() };
        }
        std::decay_t<T> add(T &&item) {
            m_finals.push_back([ptr = item]() -> ValueAsync<> {
                try { co_await ptr->close(); } catch (...) {}
            });
            return std::forward<T>(item);
        }

        ValueAsync<> close() {
            while (!m_finals.empty()) {
                co_await m_finals.back()();
                m_finals.pop_back();
            }
        }
    private:
        std::vector<std::function<ValueAsync<>()>> m_finals{};
    };

    ValueAsync<json> read_config_unsafe(io::Block &file) {
        constexpr size_t SEGMENT = 4096;
        std::string string_json(SEGMENT, '\0');
        auto offset = 0;
        for (;;) {
            auto buffer = Span<char>(string_json).trim_front(offset).keep_front(SEGMENT);
            auto read = (co_await file.read(buffer, offset)).get_result();
            if (read != SEGMENT) break; else string_json.resize(string_json.size() + SEGMENT, '\0');
        }
        co_return json::parse(string_json);
    }

    ValueAsync<json> read_config(const char *arg) {
        auto file = co_await io::Block::open(arg, io::Block::F_READ);
        co_return co_await uses(file, [](io::Block &f) { return read_config_unsafe(f); });
    }

    io::Peer make_peer(const temp::string &address, int port) {
        if (address.contains(':'))
            return io::Peer{io::Address::CreateIPv6(address).value(), port};
        else
            return io::Peer{io::Address::CreateIPv4(address).value(), port};
    }

    std::shared_ptr<IServer> create_server(const json &cfg) {
        return IServer::create(make_peer(cfg["address"], cfg["port"]), cfg["backlog"]);
    }

    ValueAsync<std::shared_ptr<IStorage>> create_storage(const json &cfg) {
        if (cfg["type"] == "remote") {
            return connect_remote_storage(make_peer(cfg["address"], cfg["port"]));
        }
        throw std::runtime_error("unknown storage connector type");
    }

    ValueAsync<> recover_journal(const json &cfg, IStorage &storage) {
        temp::unordered_map<std::string_view, int64_t> map{};
        for (auto&&[k, v]: co_await IJournal::replay(cfg["path"])) map[k] = v;
        co_await storage.store(std::move(map));
    }

    auto create_journal(const json &cfg) { return IJournal::create(cfg["path"]); }

    ValueAsync<> start_service(const char *arg) {
        AsyncRAII raii{};
        co_await uses(raii, [arg](AsyncRAII &raii) -> ValueAsync<> {
            auto cfg = co_await read_config(arg);
            auto server = raii.add(create_server(cfg["server"]));
            auto storage = raii.add(co_await create_storage(cfg["storage"]));
            co_await recover_journal(cfg["journal"], *storage);
            auto journal = raii.add(create_journal(cfg["journal"]));
            auto cache = raii.add(ICache::create(storage, journal));
            cfg.clear();
            co_await run_service(*server, *cache);
        });
    }
}

int main(int argc, char **argv) {
    if (argc != 2) {
        std::cerr << "Argument setup incorrect" << std::endl;
        std::cerr << "Usage: [path] <path-to-config-file>" << std::endl;
        return -1;
    }
    return run_blocking([path = argv[1]]() -> ValueAsync<int> {
        try { co_return (co_await njp::count::start_service(path), 0); }
        catch (std::exception &e) { co_return (std::cerr << e.what() << std::endl, -1); }
        catch (...) { co_return -1; }
    });
}
