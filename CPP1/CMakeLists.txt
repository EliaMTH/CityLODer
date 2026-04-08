cmake_minimum_required(VERSION 3.16)

project(triangulate_city LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)

## cinolib
set(cinolib_DIR ${PROJECT_SOURCE_DIR}/cinolib)
set(CINOLIB_USES_TRIANGLE ON)
set(CINOLIB_USES_OPENGL_GLFW_IMGUI OFF)
find_package(cinolib REQUIRED)

## general
add_executable(${PROJECT_NAME} main.cpp)

include_directories(${PROJECT_SOURCE_DIR}/src)

target_link_libraries (${PROJECT_NAME} PUBLIC cinolib)

add_compile_definitions(HOME_PATH="${PROJECT_SOURCE_DIR}")

include(GNUInstallDirs)
install(TARGETS ${PROJECT_NAME}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
