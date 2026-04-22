cmake_minimum_required(VERSION 3.21)

if(POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT
      "$<IF:$<CONFIG:Debug,RelWithDebInfo>,$<$<CXX_COMPILER_ID:MSVC>:EditAndContinue>,$<$<CXX_COMPILER_ID:MSVC>:ProgramDatabase>>")
endif()

project(tiny-hyper-router VERSION 0.1.0 LANGUAGES CXX)

option(THR_ENABLE_LIVE_TESTS "Enable live API-backed tests" OFF)
option(THR_ENABLE_SQLITE_STORAGE "Enable SQLite-backed storage adapter" OFF)

find_package(nlohmann_json CONFIG REQUIRED)
find_package(CURL REQUIRED)

if(THR_ENABLE_SQLITE_STORAGE)
  find_package(SQLite3 REQUIRED)
endif()

add_library(tiny_hyper_router
    src/core/runtime.cpp
    src/core/types.cpp
    src/http/client.cpp
    src/http/curl_http_client.cpp
    src/storage/in_memory_storage.cpp
    src/storage/json_file_storage.cpp
    src/storage/json_serialization.cpp
    src/providers/stub/provider.cpp
    src/providers/openrouter/items.cpp
    src/providers/openrouter/provider.cpp
)

if(THR_ENABLE_SQLITE_STORAGE)
  target_sources(tiny_hyper_router PRIVATE
      src/storage/sqlite_storage.cpp
  )
endif()

add_library(tiny_hyper_router::tiny_hyper_router ALIAS tiny_hyper_router)

target_compile_features(tiny_hyper_router PUBLIC cxx_std_20)

target_include_directories(tiny_hyper_router
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(tiny_hyper_router
    PUBLIC
        nlohmann_json::nlohmann_json
        CURL::libcurl
)

if(THR_ENABLE_SQLITE_STORAGE)
  target_link_libraries(tiny_hyper_router
      PUBLIC
          SQLite::SQLite3
  )

  target_compile_definitions(tiny_hyper_router
      PUBLIC
          THR_HAS_SQLITE_STORAGE=1
  )
endif()

if(MSVC)
  target_compile_options(tiny_hyper_router PRIVATE /W4 /permissive- /EHsc)
else()
  target_compile_options(tiny_hyper_router PRIVATE -Wall -Wextra -Wpedantic)
endif()

include(GNUInstallDirs)
install(TARGETS tiny_hyper_router
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

add_executable(tiny_hyper_router_basic
    examples/basic.cpp
)

target_link_libraries(tiny_hyper_router_basic
    PRIVATE
        tiny_hyper_router
)

enable_testing()

add_executable(tiny_hyper_router_basic_smoke
    tests/basic_smoke.cpp
)

target_link_libraries(tiny_hyper_router_basic_smoke
    PRIVATE
        tiny_hyper_router
)

add_test(
    NAME basic_smoke
    COMMAND tiny_hyper_router_basic_smoke
)

add_executable(tiny_hyper_router_json_file_storage_smoke
    tests/json_file_storage_smoke.cpp
)

target_link_libraries(tiny_hyper_router_json_file_storage_smoke
    PRIVATE
        tiny_hyper_router
)

add_test(
    NAME json_file_storage_smoke
    COMMAND tiny_hyper_router_json_file_storage_smoke
)

if(THR_ENABLE_SQLITE_STORAGE)
  add_executable(tiny_hyper_router_sqlite_storage_smoke
      tests/sqlite_storage_smoke.cpp
  )

  target_link_libraries(tiny_hyper_router_sqlite_storage_smoke
      PRIVATE
          tiny_hyper_router
  )

  add_test(
      NAME sqlite_storage_smoke
      COMMAND tiny_hyper_router_sqlite_storage_smoke
  )
endif()

add_executable(tiny_hyper_router_openrouter_mapping_smoke
    tests/openrouter_mapping_smoke.cpp
)

target_link_libraries(tiny_hyper_router_openrouter_mapping_smoke
    PRIVATE
        tiny_hyper_router
)

add_test(
    NAME openrouter_mapping_smoke
    COMMAND tiny_hyper_router_openrouter_mapping_smoke
)

add_executable(tiny_hyper_router_openrouter_live_manual
    tests/openrouter_live_manual.cpp
)

target_link_libraries(tiny_hyper_router_openrouter_live_manual
    PRIVATE
        tiny_hyper_router
)

if(THR_ENABLE_LIVE_TESTS)
  add_test(
      NAME openrouter_live_manual
      COMMAND tiny_hyper_router_openrouter_live_manual
  )
endif()
