#include "frontend/gui.hpp"
#include "frontend/loader.hpp"
#include "frontend/message.hpp"
#include "log.hpp"
#include "status.hpp"

#include <cstdlib>
#include <filesystem>
#include <string_view>

namespace fs = std::filesystem;

int main(int argc, char* argv[])
{
    fs::path work_path = fs::current_path();

    if (Status status = init_file_log(); !status.Ok()) {
        log_warn(status.Message());
    }

    if (Status status = frontend::gui::Init(work_path); !status.Ok()) {
        log_fatal(status.Message());
        return EXIT_FAILURE;
    }

    // Optional CLI arguments:
    // 1; path to rom
    // 2; path to bios

    bool start_game_immediately{};
    if (argc > 1) {
        Status status = frontend::LoadCoreAndGame(argv[1]);
        if (status.Ok()) {
            start_game_immediately = true;
        } else {
            message::Error(status.Message());
        }
    }
    if (argc > 2 && start_game_immediately) {
        Status status = frontend::GetCore()->LoadBios(argv[2]);
        if (!status.Ok()) {
            message::Error(status.Message());
        }
    }

    frontend::gui::Run(start_game_immediately);

    tear_down_log();

    return EXIT_SUCCESS;
}
