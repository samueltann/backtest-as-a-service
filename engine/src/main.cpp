// backtest-engine CLI
//
//   backtest-engine --config <config.json> [--output <results.json>]
//   backtest-engine --list-strategies
//
// Exit code 0 on success. On failure, writes {"error": "..."} to stderr and
// exits 1 — the Spring backend surfaces that message to the user.

#include <fstream>
#include <iostream>
#include <string>

#include <nlohmann/json.hpp>

#include "bt/data_handler.hpp"
#include "bt/engine.hpp"
#include "bt/strategy.hpp"

namespace {

void fail(const std::string& message) {
    std::cerr << nlohmann::json{{"error", message}}.dump() << std::endl;
    std::exit(1);
}

}  // namespace

int main(int argc, char** argv) {
    std::string config_path;
    std::string output_path;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--list-strategies") {
            std::cout << bt::strategy_catalog().dump(2) << std::endl;
            return 0;
        }
        if (arg == "--config" && i + 1 < argc) config_path = argv[++i];
        else if (arg == "--output" && i + 1 < argc) output_path = argv[++i];
        else fail("unknown or incomplete argument: " + arg);
    }

    if (config_path.empty())
        fail("usage: backtest-engine --config <config.json> [--output <results.json>]");

    try {
        std::ifstream in(config_path);
        if (!in) throw bt::EngineError("cannot open config file: " + config_path);
        nlohmann::json config_json = nlohmann::json::parse(in);

        auto config = bt::Config::from_json(config_json);
        nlohmann::json results = bt::run_backtest(config);

        if (output_path.empty()) {
            std::cout << results.dump() << std::endl;
        } else {
            std::ofstream out(output_path);
            if (!out) throw bt::EngineError("cannot write output file: " + output_path);
            out << results.dump();
        }
        return 0;
    } catch (const std::exception& e) {
        fail(e.what());
    }
    return 1;
}
