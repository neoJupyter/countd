#include <kls/coroutine/Blocking.h>

using namespace kls::coroutine;

int main() {
    run_blocking([]() -> ValueAsync<void> {

    });
    return 0;
}
