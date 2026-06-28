# MaNGOS unit-test harness (doctest).
#
# Enabled with -DBUILD_TESTS=ON. Off by default so normal/CI server builds are
# unaffected. doctest is a single, vendored header (tests/doctest/doctest.h);
# nothing extra is fetched at configure time.
#
# The harness links the `game` static library so tests can exercise real
# engine code. `game` is PUBLIC-linked, so its include directories and
# transitive deps (shared + dep/*) propagate automatically; we mirror mangosd's
# remaining explicit links (Threads/DL/OpenSSL).

file(GLOB MANGOS_TEST_SOURCES *.cpp)

add_executable(mangos_tests ${MANGOS_TEST_SOURCES})

target_include_directories(mangos_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(mangos_tests
    PRIVATE
        game
        Threads::Threads
        DL::DL
        ${OPENSSL_LIBRARIES}
)

set_target_properties(mangos_tests PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_test(NAME mangos_tests COMMAND mangos_tests)
