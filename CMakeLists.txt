cmake_minimum_required(VERSION 3.16)
project(CommsStackProject CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)


if(ANDROID)
    message(STATUS "Configuring CommsStackProject for Android")
    message(STATUS "Android ABI: ${ANDROID_ABI}")
    message(STATUS "Android Platform: ${ANDROID_PLATFORM}")

    # --- Paths to Prebuilt Android Dependencies (ASSUMED LOCATIONS) ---
    set(ANDROID_PREBUILT_LIBS_DIR "${CMAKE_SOURCE_DIR}/third_party/android_libs/${ANDROID_ABI}")
    set(ANDROID_PREBUILT_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/third_party/android_includes")

    # Protobuf for Android
    set(Protobuf_INCLUDE_DIR "${ANDROID_PREBUILT_INCLUDE_DIR}/protobuf")
    set(Protobuf_LIBRARY "${ANDROID_PREBUILT_LIBS_DIR}/libprotobuf.so")
    set(Protobuf_FOUND TRUE)
    set(Protobuf_LIBRARIES ${Protobuf_LIBRARY})
    set(Protobuf_INCLUDE_DIRS ${Protobuf_INCLUDE_DIR})
    include_directories(${Protobuf_INCLUDE_DIR})
    message(STATUS "Android Protobuf Include Dir: ${Protobuf_INCLUDE_DIR}")
    message(STATUS "Android Protobuf Library: ${Protobuf_LIBRARY}")

    # Boost for Android
    set(Boost_INCLUDE_DIR "${ANDROID_PREBUILT_INCLUDE_DIR}/boost")
    set(Boost_LIBRARY_DIR "${ANDROID_PREBUILT_LIBS_DIR}")
    set(Boost_FOUND TRUE)
    include_directories(${Boost_INCLUDE_DIR})
    message(STATUS "Android Boost Include Dir: ${Boost_INCLUDE_DIR}")
    message(STATUS "Android Boost Library Dir: ${Boost_LIBRARY_DIR}")

    # vsomeip for Android
    set(vsomeip_INCLUDE_DIR "${ANDROID_PREBUILT_INCLUDE_DIR}/vsomeip")
    set(vsomeip_LIBRARY_DIR "${ANDROID_PREBUILT_LIBS_DIR}")
    set(vsomeip3_FOUND TRUE)
    include_directories(${vsomeip_INCLUDE_DIR})
    message(STATUS "Android vsomeip Include Dir: ${vsomeip_INCLUDE_DIR}")
    message(STATUS "Android vsomeip Library Dir: ${vsomeip_LIBRARY_DIR}")

else()
    message(STATUS "Configuring CommsStackProject for Host")
    # --- Find Dependencies for Host (existing logic) ---
    find_package(Protobuf REQUIRED)
    message(STATUS "Found Protobuf: ${Protobuf_LIBRARIES}")

    find_package(Boost 1.66.0 REQUIRED COMPONENTS system thread log_setup log date_time chrono)
    if(Boost_FOUND)
        message(STATUS "Found Boost: ${Boost_INCLUDE_DIRS}")
        include_directories(${Boost_INCLUDE_DIRS})
    else()
        message(FATAL_ERROR "Boost not found.")
    endif()

    find_package(vsomeip3 REQUIRED)
    if(vsomeip3_FOUND)
        message(STATUS "Found vsomeip3: ${vsomeip3_LIBRARIES}")
    else()
        find_package(vsomeip REQUIRED)
        if(vsomeip_FOUND)
            message(STATUS "Found vsomeip: ${vsomeip_LIBRARIES}")
        else()
            message(FATAL_ERROR "vsomeip3 or vsomeip not found.")
        endif()
    endif()
endif()

# --- Subdirectories ---
add_subdirectory(comms_stack)

if(ANDROID)
    # JNI Library
    add_library(comms_stack_jni SHARED
        comms_stack_jni/jni_bridge.cpp
    )
    target_link_libraries(comms_stack_jni PRIVATE
        comms_stack_lib # Link against our core C++ library
        log             # For __android_log_print via iostream or direct JNI logging
    )
    message(STATUS "CommsStack JNI library configured for Android.")
endif()

# --- Test Executables (Host Only) ---
if(NOT ANDROID)
    set(TEST_APPS_DIR tests)

    add_executable(publisher_test ${TEST_APPS_DIR}/publisher_test.cpp)
    target_link_libraries(publisher_test PRIVATE comms_stack_lib)

    add_executable(subscriber_test ${TEST_APPS_DIR}/subscriber_test.cpp)
    target_link_libraries(subscriber_test PRIVATE comms_stack_lib)

    add_executable(rpc_server_test ${TEST_APPS_DIR}/rpc_server_test.cpp)
    target_link_libraries(rpc_server_test PRIVATE comms_stack_lib)

    add_executable(rpc_client_test ${TEST_APPS_DIR}/rpc_client_test.cpp)
    target_link_libraries(rpc_client_test PRIVATE comms_stack_lib)

    message(STATUS "Host test applications configured.")
endif()
