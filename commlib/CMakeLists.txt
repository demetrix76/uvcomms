set(TARGET_NAME commlib)

set(SOURCES 
    commlib.cpp
    commlib.h
    overloaded.h
)

if(CMAKE_SYSTEM_NAME MATCHES Darwin)
    list(APPEND SOURCES commlib_mac.h commlib_mac.mm)
endif()

source_group(TREE ${CMAKE_SOURCE_DIR} FILES ${SOURCES})

add_library(${TARGET_NAME} STATIC
    ${SOURCES}
)

target_link_libraries(${TARGET_NAME} PUBLIC uvw::uvw)

target_include_directories(${TARGET_NAME} PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
)