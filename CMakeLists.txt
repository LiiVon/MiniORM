cmake_minimum_required(VERSION 3.16)
project(MiniORM LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_program(MINIORM_MYSQL_CONFIG mysql_config)
if (MINIORM_MYSQL_CONFIG)
    execute_process(
        COMMAND ${MINIORM_MYSQL_CONFIG} --cflags
        OUTPUT_VARIABLE MINIORM_MYSQL_CFLAGS
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    execute_process(
        COMMAND ${MINIORM_MYSQL_CONFIG} --libs
        OUTPUT_VARIABLE MINIORM_MYSQL_LIBS
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    separate_arguments(MINIORM_MYSQL_CFLAGS_LIST NATIVE_COMMAND "${MINIORM_MYSQL_CFLAGS}")
    separate_arguments(MINIORM_MYSQL_LIBS_LIST NATIVE_COMMAND "${MINIORM_MYSQL_LIBS}")
endif()

enable_testing()

add_library(miniorm STATIC)

target_sources(miniorm
    PRIVATE
        src/core/utils.cpp
        src/adapter/adapter.cpp
        src/connection/database_connection.cpp
        src/connection/connection_pool.cpp
        src/connection/connection_manager.cpp
        src/connection/transaction.cpp
        src/query/query_builder.cpp
        src/entity/reflection.cpp
        src/entity/entity_manager.cpp
)

target_include_directories(miniorm
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if (MINIORM_MYSQL_CONFIG)
    target_compile_definitions(miniorm PUBLIC MINIORM_HAS_MYSQL_CLIENT=1)
    target_compile_options(miniorm PRIVATE ${MINIORM_MYSQL_CFLAGS_LIST})
    target_link_libraries(miniorm PUBLIC ${MINIORM_MYSQL_LIBS_LIST})
else()
    message(WARNING "mysql_config not found; MySQL adapter will be unavailable")
endif()

function(add_miniorm_test target_name source_file)
    add_executable(${target_name} ${source_file})
    target_link_libraries(${target_name} PRIVATE miniorm)
    add_test(NAME ${target_name} COMMAND ${target_name})
endfunction()

add_miniorm_test(test_config test/test_config.cpp)
add_miniorm_test(test_traits test/test_traits.cpp)
add_miniorm_test(test_utils test/test_utils.cpp)
add_miniorm_test(test_adapter test/test_adapter.cpp)
add_miniorm_test(test_concepts test/test_concepts.cpp)
add_miniorm_test(test_query test/test_query.cpp)
add_miniorm_test(test_entity test/test_entity.cpp)
add_miniorm_test(test_integration test/test_integration.cpp)

# Example executable for manual demo
add_executable(miniorm_example example.cpp)
target_link_libraries(miniorm_example PRIVATE miniorm)
