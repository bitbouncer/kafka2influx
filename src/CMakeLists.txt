project(kafka2influx)

SET(SRCS
kafka2influx.cpp
)

add_executable(kafka2influx ${SRCS})
target_link_libraries(kafka2influx ${EXT_LIBS})
