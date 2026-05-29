add_library(paths paths.cpp)

target_include_directories(paths PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_definitions(paths PRIVATE
    APP_ID="${APP_ID}"
    INSTALL_DATADIR="${CMAKE_INSTALL_DATADIR}"
    DEV_CONFIG_PATH="${CMAKE_SOURCE_DIR}/utils/config/config.yaml"
    DEV_MODELS_DIR="${CMAKE_SOURCE_DIR}/models"
)