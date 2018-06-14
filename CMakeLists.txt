cmake_minimum_required(VERSION 3.6)
project(lsp)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

file(GLOB SOURCES "include/*.h" "include/*.hpp" "src/*.cpp")

add_executable(lsp ${SOURCES})

target_include_directories(lsp PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_include_directories(lsp PRIVATE ${CMAKE_SOURCE_DIR}/src)

find_package(Teem)
target_include_directories(lsp PRIVATE ${Teem_INCLUDE_DIRS})

find_package(LibXml2)
target_include_directories(lsp PRIVATE ${LIBXML2_INCLUDE_DIR})

find_package(Boost COMPONENTS system filesystem REQUIRED)
if (${Boost_FOUND})
    include_directories(lsp PRIVATE ${Boost_INCLUDE_DIRS})
    target_link_libraries(lsp ${Boost_LIBRARIES})
endif()

target_link_libraries(lsp ${Teem_LIBRARIES} ${LIBXML2_LIBRARIES})
