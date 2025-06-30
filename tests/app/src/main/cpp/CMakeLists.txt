cmake_minimum_required(VERSION 3.18)
project(CommsStackAppNative CXX)

# IMPORTANT: Adjust this path to the root directory of your C++ comms_stack project.
# This example assumes the C++ project is a sibling directory named 'comms_stack_cpp_project'
# relative to the root of the Android application project.
# If your Android app is 'MyAndroidProject' and C++ stack is 'MyCppStack', and they are siblings:
# Path would be ../../MyCppStack
# If C++ stack is inside MyAndroidProject/app/src/main/cpp/external/MyCppStack:
# Path would be external/MyCppStack
add_subdirectory(
    ../../../../comms_stack_cpp_project # <<< ADJUST THIS RELATIVE PATH
    ${CMAKE_CURRENT_BINARY_DIR}/comms_stack_build 
)