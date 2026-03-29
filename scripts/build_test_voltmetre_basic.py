Import("env")

env.BuildSources(
    "$BUILD_DIR/test_src",
    "$PROJECT_DIR/test/e2e",
    "+<test_voltmetre_basic.cpp>",
)
