#include "frontend/gui.hpp"
#include "frontend/loader.hpp"
#include "frontend/message.hpp"
#include "log.hpp"
#include "status.hpp"

#include <cstdlib>
#include <string_view>

int main(int argc, char* argv[])
{
    if (Status status = init_file_log(); !status.ok()) {
        log_warn(status.message());
    }

    if (Status status = frontend::gui::Init(); !status.ok()) {
        log_fatal(status.message());
        return EXIT_FAILURE;
    }

    // Optional CLI arguments:
    // 1; path to rom
    // 2; path to bios

    bool start_game_immediately{};
    if (argc > 1) {
        Status status = frontend::load_core_and_game(argv[1]);
        if (status.ok()) {
            start_game_immediately = true;
        } else {
            message::error(status.message());
        }
    }
    if (argc > 2 && start_game_immediately) {
        Status status = frontend::get_core()->load_bios(argv[2]);
        if (!status.ok()) {
            message::error(status.message());
        }
    }

    frontend::gui::Run(start_game_immediately);

    return EXIT_SUCCESS;
}
