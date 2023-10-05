cmake_minimum_required(VERSION 3.24)
project(virtual)

# --- Fetch FTXUI --------------------------------------------------------------
include(FetchContent)

set(FETCHCONTENT_UPDATES_DISCONNECTED TRUE)
FetchContent_Declare(ftxui
        GIT_REPOSITORY https://github.com/ArthurSonzogni/ftxui
        GIT_TAG v5.0.0
        )

FetchContent_GetProperties(ftxui)
if(NOT ftxui_POPULATED)
    FetchContent_Populate(ftxui)
    add_subdirectory(${ftxui_SOURCE_DIR} ${ftxui_BINARY_DIR} EXCLUDE_FROM_ALL)
endif()




# ------------------------------------------------------------------------------


set(CMAKE_CXX_STANDARD 17)
add_executable(virtual itoa.h itoa.cpp main.cpp String.cpp String.h Common.cpp Common.h  ../../uSEQ.ino ../../piopwm.h ../../uSEQ.ino ../../uSEQ.ino)
target_include_directories(virtual PRIVATE .)
target_include_directories(virtual PRIVATE /usr/include/libserial)

target_link_libraries(virtual
        PRIVATE ftxui::screen
        PRIVATE ftxui::dom
        PRIVATE ftxui::component # Not needed for this example.
        PRIVATE serial
        )
