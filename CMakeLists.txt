cmake_minimum_required(VERSION 3.6)
project(lsp)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fopenmp")

file(GLOB HEADERS "include/*.h" "include/*.hpp")
file(GLOB SOURCES "src/*.cpp")

# add teem library that is on my mac
add_library(teem STATIC IMPORTED)
set_property(TARGET teem PROPERTY IMPORTED_LOCATION "/Users/zhuokaizhao/Desktop/UChicago/Research/Teem/teem-install/lib/libteem.a")

add_library(fftw3 STATIC IMPORTED)
set_property(TARGET fftw3 PROPERTY IMPORTED_LOCATION "/usr/local/Cellar/fftw/3.3.8/lib/libfftw3f.a")

add_library(libxml2 STATIC IMPORTED)
set_property(TARGET libxml2 PROPERTY IMPORTED_LOCATION "/usr/local/Cellar/libxml2/2.9.7/lib/libxml2.dylib")

add_library(png STATIC IMPORTED)
set_property(TARGET png PROPERTY IMPORTED_LOCATION "/usr/local/Cellar/libpng/1.6.35/lib/libpng.a")

add_library(z STATIC IMPORTED)
set_property(TARGET z PROPERTY IMPORTED_LOCATION "/usr/local/Cellar/zlib/1.2.11/lib/libz.a")

add_executable(lsp ${SOURCES})

target_link_libraries(lsp teem fftw3 libxml2 png z)

# teem include directory that is on my mac
include_directories("/Users/zhuokaizhao/Desktop/UChicago/Research/Teem/teem-install/include/")
#include_directories("/usr/local/opt/libxml2/include/libxml2/")
include_directories("/usr/local/Cellar/libxml2/2.9.7/include/libxml2")
include_directories("/usr/local/include")
include_directories("/usr/include")
include_directories("/usr/local/Cellar")
target_include_directories(lsp PRIVATE ${CMAKE_SOURCE_DIR}/include)

# find_package(LibXml2 REQUIRED)
# if (${LibXml2_FOUND})
# 	target_include_directories(lsp PRIVATE ${LIBXML2_INCLUDE_DIRS})
# 	target_link_libraries(lsp ${LibXml2_LIBS})
# endif()

# find_package(PNG REQUIRED)
# if (${PNG_FOUND})
# 	target_include_directories(lsp PRIVATE ${PNG_INCLUDE_DIRS})
# 	target_link_libraries(lsp ${PNG_LIBS})
# endif()

# find boost library
#set(Boost_NO_BOOST_CMAKE ON)
find_package(Boost COMPONENTS system filesystem REQUIRED)
if (${Boost_FOUND})
    include_directories(lsp PRIVATE ${Boost_INCLUDE_DIRS})
    target_link_libraries(lsp ${Boost_LIBRARIES})
endif()

# find opencv library
find_package(OpenCV COMPONENTS core videoio imgcodecs REQUIRED)
if (${OpenCV_FOUND})
	include_directories(lsp PRIVATE ${OpenCV_INCLUDE_DIRS})
	target_link_libraries(lsp ${OpenCV_LIBS})
endif()

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

