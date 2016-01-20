cmake_minimum_required(VERSION 2.8)
INCLUDE(${CMAKE_SOURCE_DIR}/../build-scripts/CMakeDependencies.txt)

if(WIN32)
add_definitions(-DVERSION="csi-internal")
set(EXT_LIBS csi-avro-utils ${CSI_KAFKA_LIBS}) 
endif()

if(__LINUX__)
set(EXT_LIBS csi-avro-utils ${CSI_KAFKA_LIBS})
endif()

add_subdirectory(src)

