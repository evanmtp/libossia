set(CMAKE_MODULE_PATH
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake"
  "${CMAKE_MODULE_PATH}"
)

if(OSSIA_SDK)
  list(APPEND CMAKE_PREFIX_PATH "${OSSIA_SDK}/portaudio/lib/cmake/portaudio")
endif()

if(APPLE AND EXISTS /usr/local/opt/qt5)
  list(APPEND CMAKE_PREFIX_PATH "/usr/local/opt/qt5")
endif()

cmake_minimum_required(VERSION 3.8 FATAL_ERROR)

project(OSSIA C CXX)

set(CMAKE_POSITION_INDEPENDENT_CODE 1)
find_package(Threads)
find_package(Qt5 5.3 COMPONENTS Core)

# Options
set(OSSIA_3RDPARTY_FOLDER "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty" CACHE "" INTERNAL)
include(OssiaOptions)

# Dependencies
include(OssiaDeps)

# Configuration
include(OssiaConfiguration)
cmake_policy(SET CMP0054 NEW)
cmake_policy(SET CMP0056 NEW)
cmake_policy(SET CMP0069 NEW)

# Main library
add_subdirectory(src)

# Examples & tests
if(OSSIA_EXAMPLES)
    add_subdirectory(examples)
endif()

if(OSSIA_TESTING)
  enable_testing()
  add_subdirectory(tests)
endif()

if(OSSIA_GENVAR)
  add_subdirectory(tools/gen_variant)
endif()
