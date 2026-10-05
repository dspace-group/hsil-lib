cmake_minimum_required(VERSION 3.21)

project(InstalledConsumer LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

enable_testing()
include(CTest)

# Find HsilCoSim package installed in prefix (provided via CMAKE_PREFIX_PATH during CMake configuration)
find_package(HsilCoSim CONFIG REQUIRED)

add_executable(installedConsumer main.c)

# Link against the imported target provided by HsilCoSim package
target_link_libraries(installedConsumer PRIVATE HsilCoSim::hsil_cosim)

# Copy runtime DLLs next to executable on Windows
if(WIN32)
    add_custom_command(TARGET installedConsumer POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E
            "$<IF:$<BOOL:$<TARGET_RUNTIME_DLLS:installedConsumer>>,copy_if_different,true>"
            "$<TARGET_RUNTIME_DLLS:installedConsumer>"
            "$<$<BOOL:$<TARGET_RUNTIME_DLLS:installedConsumer>>:$<TARGET_FILE_DIR:installedConsumer>>"
        COMMAND_EXPAND_LISTS
        VERBATIM
        COMMENT "Copying runtime DLLs next to installedConsumer"
    )
endif()

add_test(NAME InstalledConsumerTest COMMAND installedConsumer)
