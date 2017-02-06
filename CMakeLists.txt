cmake_minimum_required(VERSION 2.8.11)

if(WIN32)
SET(CSI_INCLUDE_PATH
  ${CMAKE_SOURCE_DIR}/../include
  ${CMAKE_SOURCE_DIR}/../zlib
  ${CMAKE_SOURCE_DIR}/../zlib/build
  ${CMAKE_SOURCE_DIR}/../lz4/lib
  ${CMAKE_SOURCE_DIR}/../openssl/include
  ${CMAKE_SOURCE_DIR}/../boost
  ${CMAKE_SOURCE_DIR}/../csi-kafka
  ${CMAKE_SOURCE_DIR}/../csi-async
  ${CMAKE_SOURCE_DIR}/../csi-hcl-asio
  ${CMAKE_SOURCE_DIR}/../curl/include
)

SET(CSI_LIBRARY_PATH        
  ${CMAKE_SOURCE_DIR}/../openssl
  ${CMAKE_SOURCE_DIR}/../zlib/build
  ${CMAKE_SOURCE_DIR}/../boost/stage/lib/$(Platform)/lib
  ${CMAKE_SOURCE_DIR}/../lib/$(Platform)
  ${CMAKE_SOURCE_DIR}/../curl/lib/$(Platform)
  ${CMAKE_SOURCE_DIR}/../csi-kafka/lib/$(Platform)
)
SET(LIBRARY_OUTPUT_PATH     ${CMAKE_SOURCE_DIR}/lib/$(Platform))
SET(EXECUTABLE_OUTPUT_PATH  ${CMAKE_SOURCE_DIR}/bin/$(Platform))

add_definitions(-D_FILE_OFFSET_BITS=64 -D_REENTRANT -DEXTERNAL_LOCKS -DMULTITHREAD)
add_definitions(-DBOOST_ASIO_HAS_STD_CHRONO)
add_definitions(-DMARCH_x86_64) 
add_definitions(-D_WIN32_WINNT=0x0602) # win8.1 to get nthll 64 bit
add_definitions(-DWINVER=0x0601)
add_definitions(-DCF_WINDOWS)
add_definitions(/MP)
add_definitions(/bigobj)
add_definitions(-D_UNICODE)
add_definitions(-DUNICODE)
add_definitions(-D_CRT_SECURE_NO_WARNINGS)
add_definitions(-D_SCL_SECURE_NO_WARNINGS)
add_definitions(-DSECURITY_WIN32)
add_definitions(/wd4996) #'strdup': The POSIX name for this item is deprecated
add_definitions(/wd4197) # 'cf_atomic32' : top-level volatile in cast is ignored
add_definitions(/wd4200) #nonstandard extension used : zero-sized array in struct/union 
add_definitions(-DBOOST_ALL_STATIC_LINK)
add_definitions(-DCURL_STATICLIB)

SET(ZLIB_LIBS debug zlibstaticd optimized zlibstatic)
SET(KAFKA_LIBS csi-kafka)
#SET(SSL_LIBS libssl libcrypto)
SET(EXT_LIBS ${KAFKA_LIBS} libcurl ${SSL_LIBS} ${ZLIB_LIBS} crypt32 Ws2_32 legacy_stdio_definitions)

elseif(APPLE)
SET(CSI_INCLUDE_PATH 
  ${CMAKE_SOURCE_DIR}/../csi-kafka
  ${CMAKE_SOURCE_DIR}/../csi-async
  ${CMAKE_SOURCE_DIR}/../csi-hcl-asio
)
SET(CSI_LIBRARY_PATH 
  ${CMAKE_SOURCE_DIR}/../csi-kafka/lib
)

add_definitions(-DBOOST_LOG_DYN_LINK)
add_definitions(-std=c++11)

set(BOOST_LIBS boost_log_setup boost_log-mt boost_date_time boost_timer boost_thread-mt boost_system boost_program_options boost_filesystem boost_regex boost_chrono boost_iostreams pthread c)
SET(KAFKA_LIBS csi-kafka)
SET(EXT_LIBS ${KAFKA_LIBS} ${BOOST_LIBS} curl crypto ssl)

elseif(ALPINE_LINUX)
SET(CSI_INCLUDE_PATH        
  ${CMAKE_SOURCE_DIR}/../csi-kafka
  ${CMAKE_SOURCE_DIR}/../csi-async
  ${CMAKE_SOURCE_DIR}/../csi-hcl-asio
)

SET(CSI_LIBRARY_PATH
  ${CMAKE_SOURCE_DIR}/../csi-kafka/lib
)

set(LIBRARY_OUTPUT_PATH     ${CMAKE_SOURCE_DIR}/lib)
SET(EXECUTABLE_OUTPUT_PATH  ${CMAKE_SOURCE_DIR}/bin)

add_definitions(-D_FILE_OFFSET_BITS=64 -D_REENTRANT -DEXTERNAL_LOCKS -DMULTITHREAD)
add_definitions(-fPIC)
add_definitions(-std=c++11)
add_definitions(-DBOOST_LOG_DYN_LINK)
add_definitions(-DBOOST_ALL_DYN_LINK)
add_definitions(-Wno-deprecated)

#boost_thread
set(BOOST_LIBS boost_log_setup-mt boost_log-mt boost_date_time-mt boost_timer-mt boost_thread-mt boost_system-mt boost_program_options-mt boost_filesystem-mt boost_regex-mt boost_chrono-mt boost_iostreams-mt pthread rt c)
SET(KAFKA_LIBS csi-kafka)
SET(EXT_LIBS ${KAFKA_LIBS} ${BOOST_LIBS} curl crypto ssl)

else()
#LINUX
SET(CSI_INCLUDE_PATH        
  ${CMAKE_SOURCE_DIR}/../csi-kafka
  ${CMAKE_SOURCE_DIR}/../csi-async
  ${CMAKE_SOURCE_DIR}/../csi-hcl-asio
  ${CMAKE_SOURCE_DIR}/../boost
)

SET(CSI_LIBRARY_PATH
  ${CMAKE_SOURCE_DIR}/../csi-kafka/lib
  ${CMAKE_SOURCE_DIR}/../boost/stage/lib
)

set(LIBRARY_OUTPUT_PATH     ${CMAKE_SOURCE_DIR}/lib)
SET(EXECUTABLE_OUTPUT_PATH  ${CMAKE_SOURCE_DIR}/bin)

add_definitions(-D_FILE_OFFSET_BITS=64 -D_REENTRANT -DEXTERNAL_LOCKS -DMULTITHREAD)
add_definitions(-fPIC)
add_definitions(-std=c++11)
add_definitions(-DBOOST_LOG_DYN_LINK)
add_definitions(-Wno-deprecated)

set(BOOST_LIBS boost_log_setup boost_log boost_date_time boost_timer boost_thread boost_system boost_program_options boost_filesystem boost_regex boost_chrono boost_iostreams pthread rt c)
SET(KAFKA_LIBS csi-kafka)
SET(EXT_LIBS ${KAFKA_LIBS} ${BOOST_LIBS} curl crypto ssl)
endif() 

include_directories(${CSI_INCLUDE_PATH} ${CMAKE_SOURCE_DIR})
link_directories(${CSI_LIBRARY_PATH})


#dirty fix to complile non supported static lib on windows...
if(WIN32)
file(GLOB liblz4_files
  ${CMAKE_CURRENT_SOURCE_DIR}/../lz4/lib/*.c
  ${CMAKE_CURRENT_SOURCE_DIR}/../lz4/lib/*.h
  )
add_library(lz4 STATIC ${liblz4_files})
endif()

add_subdirectory(src)

