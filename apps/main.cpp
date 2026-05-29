#include <filesystem>
#include <iostream>

#include "config.hpp"
#include "paths.hpp"
#include "pipeline.hpp"

int main() {
    AppConfig cfg = load_config(get_config_path());

    cfg.model.path =
        (std::filesystem::path(get_models_dir()) / cfg.model.path).string();

    Pipeline pipeline(cfg);
    pipeline.run();

    return 0;
}
