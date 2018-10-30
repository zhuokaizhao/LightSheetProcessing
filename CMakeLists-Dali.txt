cmake_minimum_required(VERSION 3.6)
project(lsp)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fopenmp")

file(GLOB HEADERS "include/*.h" "include/*.hpp")
file(GLOB SOURCES "src/*.cpp")

# add teem library that is on dali
add_library(teem STATIC IMPORTED)
set_property(TARGET teem PROPERTY IMPORTED_LOCATION "/software/teem-trunk-el7-x86_64/lib/libteem.so")

add_executable(lsp ${SOURCES})

# teem include directory that is on dali
include_directories("/software/teem-trunk-el7-x86_64/include")
# libxml2 include directory that is on dali
include_directories("/software/libxml2-2.9-el7-x86_64/include/libxml2")
target_include_directories(lsp PRIVATE ${CMAKE_SOURCE_DIR}/include)

target_link_libraries(lsp teem xml2 boost_filesystem boost_system opencv_core opencv_videoio opencv_imgcodecs fftw3f png z)

install (TARGETS lsp DESTINATION bin)


# -------------------------------------------
# Old version for pre-installed libs
# Cannot work for midway's module system
# -------------------------------------------
#find_package(Teem REQUIRED)
#target_include_directories(lsp PRIVATE ${Teem_INCLUDE_DIRS})

#find_package(LibXml2 REQUIRED)
#target_include_directories(lsp PRIVATE ${LIBXML2_INCLUDE_DIR})

#set(Boost_NO_BOOST_CMAKE ON)
#find_package(Boost COMPONENTS system filesystem REQUIRED)
#if (${Boost_FOUND})
#    include_directories(lsp PRIVATE ${Boost_INCLUDE_DIRS})
#    target_link_libraries(lsp ${Boost_LIBRARIES})
#endif()

#target_link_libraries(lsp ${Teem_LIBRARIES} ${LIBXML2_LIBRARIES})

