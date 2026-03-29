Import("env")

env.BuildSources(
    "$BUILD_DIR/test_src",
    "$PROJECT_DIR/test/e2e",
    "+<test_sht30_rs485.cpp>",
)

env.BuildSources(
    "$BUILD_DIR/app_src",
    "$PROJECT_DIR/src",
    "+<ModbusSensors.cpp> +<Logger.cpp>",
)
