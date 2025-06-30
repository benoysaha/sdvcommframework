# comms_stack/CMakeLists.txt

# --- Library Definition ---
add_library(comms_stack_lib STATIC # Or SHARED
    src/communication_manager.cpp
    src/publisher.cpp
    src/subscriber.cpp
    src/rpc_client.cpp
    src/rpc_service.cpp
    src/my_sample_rpc_impl.cpp # Added RPC service implementation
    # Generated protobuf files will be added by protobuf_generate_cpp
)

# --- Protobuf Code Generation ---
# Define where generated files will go (within CMAKE_CURRENT_BINARY_DIR)
set(PROTOBUF_GEN_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated-sources/protobuf)
file(MAKE_DIRECTORY ${PROTOBUF_GEN_DIR}) # Ensure the directory exists

# Find all .proto files in the protos directory
file(GLOB PROTO_FILES "${CMAKE_CURRENT_SOURCE_DIR}/protos/*.proto")

if(PROTO_FILES)
    message(STATUS "Found proto files for comms_stack_lib: ${PROTO_FILES}")

    # This command generates C++ sources and headers.
    # PROTO_SRCS and PROTO_HDRS will be populated with the paths to generated files.
    protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${PROTO_FILES}
        IMPORT_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/protos" # Optional: if protos import others in same dir
        PROTOC_ARGS "--experimental_allow_proto3_optional" # Example of adding protoc args if needed
    )
    # For services (like SampleRpc), cc_generic_services=true in the .proto file
    # should make protoc generate the necessary service base classes without needing a gRPC plugin.

    message(STATUS "Generated Protobuf Sources: ${PROTO_SRCS}")
    message(STATUS "Generated Protobuf Headers: ${PROTO_HDRS}")

    # Add generated source files to the library target
    target_sources(comms_stack_lib PRIVATE ${PROTO_SRCS})

    # Add the directory containing generated headers to include paths
    # PUBLIC so that anything linking comms_stack_lib can also find these headers
    target_include_directories(comms_stack_lib PUBLIC ${PROTOBUF_GEN_DIR})
else()
    message(WARNING "No .proto files found in ${CMAKE_CURRENT_SOURCE_DIR}/protos/")
endif()


# --- Include Directories ---
target_include_directories(comms_stack_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include  # For our own library's headers
    ${PROTOBUF_GEN_DIR} # For generated protobuf headers (set earlier)
)
if(ANDROID)
    target_include_directories(comms_stack_lib PUBLIC
        ${Protobuf_INCLUDE_DIR} # From root CMakeLists.txt
        ${Boost_INCLUDE_DIR}    # From root CMakeLists.txt
        ${vsomeip_INCLUDE_DIR}  # From root CMakeLists.txt
    )
else() # Host
    target_include_directories(comms_stack_lib PUBLIC
        ${Protobuf_INCLUDE_DIRS} # Plural from find_package
        ${Boost_INCLUDE_DIRS}    # Plural from find_package
        # vsomeip includes usually handled by target_link_libraries with imported targets on host
    )
endif()


# --- Linking Dependencies ---
if(ANDROID)
    # Linking prebuilt .so files for Android
    target_link_libraries(comms_stack_lib PUBLIC
        ${Protobuf_LIBRARY} # e.g., .../libprotobuf.so
        # Boost libraries
        "${Boost_LIBRARY_DIR}/libboost_system.so"
        "${Boost_LIBRARY_DIR}/libboost_thread.so"
        "${Boost_LIBRARY_DIR}/libboost_log.so"
        # vsomeip libraries
        "${vsomeip_LIBRARY_DIR}/libvsomeip3.so"
        "${vsomeip_LIBRARY_DIR}/libvsomeip3-cfg.so"
        "${vsomeip_LIBRARY_DIR}/libvsomeip3-sd.so"
        # Android specific libs if needed
        log # For __android_log_print
    )
else() # Host linking (existing logic)
    target_link_libraries(comms_stack_lib PUBLIC
        ${Protobuf_LIBRARIES}
        Boost::system Boost::thread Boost::log_setup Boost::log Boost::date_time Boost::chrono
    )
    if(vsomeip3_FOUND)
        target_link_libraries(comms_stack_lib PUBLIC vsomeip3::vsomeip3 vsomeip3::vsomeip3-cfg vsomeip3::vsomeip3-sd)
    elseif(vsomeip_FOUND)
        target_link_libraries(comms_stack_lib PUBLIC vsomeip)
    else()
        message(FATAL_ERROR "Neither vsomeip3 nor vsomeip found for linking host build.")
    endif()
endif()

# --- Protobuf Code Generation (Example, will be detailed in next step) --- # This was from previous merge, already implemented above
# file(GLOB PROTO_FILES "${CMAKE_CURRENT_SOURCE_DIR}/protos/*.proto")
#
# if(PROTO_FILES)
#    message(STATUS "Found proto files: ${PROTO_FILES}")
#    get_filename_component(PROTOBUF_GEN_DIR ${CMAKE_BINARY_DIR}/generated-sources/protobuf ABSOLUTE)
#    file(MAKE_DIRECTORY ${PROTOBUF_GEN_DIR})
#
#    protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${PROTO_FILES})
#    # For cc_generic_services=true, you might need to specify a plugin or use grpc's generator
#    # For basic services (non-grpc), protoc itself should generate Service base classes.
#    # protobuf_generate(TARGET comms_stack_lib PROTOS ${PROTO_FILES} PLUGIN "protoc-gen-grpc=${GRPC_CPP_PLUGIN_EXECUTABLE}") # If gRPC
#
#    target_sources(comms_stack_lib PRIVATE ${PROTO_SRCS})
#    target_include_directories(comms_stack_lib PUBLIC ${PROTOBUF_GEN_DIR}) # Add generated headers path
#    message(STATUS "Proto SRCS: ${PROTO_SRCS}")
#    message(STATUS "Proto HDRS: ${PROTO_HDRS}")
# else()
#    message(WARNING "No .proto files found in ${CMAKE_CURRENT_SOURCE_DIR}/protos/")
# endif()

message(STATUS "comms_stack_lib configured.")
