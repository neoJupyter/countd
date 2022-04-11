#include "Json.h"

namespace count {
    using namespace kls;
    using namespace kls::coroutine;

    JsonClient::JsonClient(std::unique_ptr<kls::phttp::Endpoint> endpoint) :
            m_endpoint(phttp::ClientEndpoint::create(std::move(endpoint))) {}

    ValueAsync<temp_json> JsonClient::exec(std::string_view m, std::string_view p, const temp_json &request) {
        temp::string dump = request.dump();
        auto request_phttp = phttp::Request{
                .line = phttp::RequestLine(m, p, temp::resource()),
                .headers = phttp::Headers(temp::resource()),
                .body = json_to_block(request)
        };
        auto&&[line, _, body] = co_await m_endpoint->exec(std::move(request_phttp));
        const auto range = static_span_cast<char>(body.content());
        if (line.code() == 200) co_return temp_json::parse(std::string_view(range.begin(), range.end()));
        throw JsonRemoteException(m, p, dump, line.code(), line.message(), {range.begin(), range.end()});
    }

    ValueAsync<> JsonClient::close() { return m_endpoint->close(); }

    static constexpr auto exception_lines = std::array<std::string_view, 7>{
            "Exception throw from remote endpoint:",
            "\nWith Method: ",
            "\nWith Path: ",
            "\nWith Request:\n",
            "\nGot Code: ",
            "\nGot Message: "
            "\nGet Response:\n",
    };

    constexpr std::array<size_t, 7> get_exception_lines_size() {
        std::array<size_t, 7> ret{0};
        ret[0] = exception_lines[0].size();
        for (int i = 1; i < 7; ++i) ret[i] = exception_lines[i].size() + ret[i - 1];
        return ret;
    }

    static constexpr auto exception_lines_size = get_exception_lines_size();

    std::string_view append_get_second_unsafe(std::string &op, std::string_view first, std::string_view second) {
        const auto begin = (op.append(first), op.end());
        const auto end = (op.append(second), op.end());
        return {begin, end};
    }

    JsonRemoteException::JsonRemoteException(
            std::string_view method, std::string_view path, std::string_view request,
            int code, std::string_view message, std::string_view response
    ) {
        auto code_str = std::to_string(code);
        std::array<size_t, 7> args_size{
                0, method.size(), path.size(), request.size(),
                code_str.size(), message.size(), response.size()
        };
        for (int i = 1; i < 7; ++i) args_size[i] += args_size[i - 1];
        auto result = std::string();
        result.reserve(exception_lines_size[6] + args_size[6] + 16);
        result.append(exception_lines[0]);
        m_method = append_get_second_unsafe(result, exception_lines[1], method);
        m_path = append_get_second_unsafe(result, exception_lines[2], path);
        m_request = append_get_second_unsafe(result, exception_lines[3], request);
        append_get_second_unsafe(result, exception_lines[4], code_str);
        m_message = append_get_second_unsafe(result, exception_lines[5], message);
        m_response = append_get_second_unsafe(result, exception_lines[6], response);
        m_what_data = std::move(result);
        m_code = code;
    }

    const char *JsonRemoteException::what() const noexcept { return m_what_data.c_str(); }
}