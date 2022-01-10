
cmake_minimum_required(VERSION 3.10.2)


project("opensslDemo")

include_directories(
        include
        include/ns_turn
        uclient
        stunclient
        include/hiredis
        client
        client++
        include/event
        include/event/extra
        include/event/event2
        common
)
add_library(ssl SHARED IMPORTED)
set_target_properties(ssl
        PROPERTIES IMPORTED_LOCATION
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/${CMAKE_ANDROID_ARCH_ABI}/libssl.so)

message("zbq-------------------${CMAKE_CURRENT_SOURCE_DIR}/lib/${ANDROID_ABI}/libssl.so")

add_library(crypto SHARED IMPORTED)
set_target_properties(crypto
        PROPERTIES
        IMPORTED_LOCATION
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/${ANDROID_ABI}/libcrypto.so)

#set(libs ${CMAKE_CURRENT_SOURCE_DIR}/lib/${ANDROID_ABI})
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -L${libs}")

aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/client SOURCE_CLIENT)
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/common SOURCE_COMMON)
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/event SOURCE_EVENT)
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/hiredis SOURCE_HIREDIS)
#aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/natdiscovery SOURCE_NAT)
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/stunclient SOURCE_STUN)
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/uclient SOURCE_UCLIENT)

add_library( # Sets the name of the library.
        native-lib
        SHARED
        native-lib.cpp
        coturn_manager.cpp
        ${SOURCE_CLIENT}
        ${SOURCE_COMMON}
        ${SOURCE_EVENT}
        ${SOURCE_HIREDIS}
        #        ${SOURCE_NAT}
        ${SOURCE_STUN}
        ${SOURCE_UCLIENT}
        )


find_library( # Sets the name of the path variable.
        log-lib
        log)


target_link_libraries( # Specifies the target library.
        native-lib
        crypto
        ssl
        ${log-lib})