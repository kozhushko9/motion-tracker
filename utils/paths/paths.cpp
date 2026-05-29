#include "paths.hpp"

#include <filesystem>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;

fs::path get_executable_dir() {
    return fs::canonical("/proc/self/exe").parent_path();
}

fs::path get_install_data_dir() {
    const fs::path exe_dir = get_executable_dir();

    return exe_dir.parent_path() / INSTALL_DATADIR / APP_ID;
}

std::string get_config_path() {
    const fs::path install_path = get_install_data_dir() / "config.yaml";

    if (fs::exists(install_path)) {
        return install_path.string();
    }

    const fs::path dev_path = DEV_CONFIG_PATH;

    if (fs::exists(dev_path)) {
        return dev_path.string();
    }

    throw std::runtime_error(
        "Config file not found. Checked installed path: " +
        install_path.string() +
        " and development path: " +
        dev_path.string()
    );
}

std::string get_models_dir() {
    const fs::path install_path = get_install_data_dir() / "models";

    if (fs::exists(install_path)) {
        return install_path.string();
    }

    const fs::path dev_path = DEV_MODELS_DIR;

    if (fs::exists(dev_path)) {
        return dev_path.string();
    }

    throw std::runtime_error(
        "Models directory not found. Checked installed path: " +
        install_path.string() +
        " and development path: " +
        dev_path.string()
    );
}