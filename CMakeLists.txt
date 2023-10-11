cmake_minimum_required(VERSION 3.10)

project(LL_Objects)

# Set a default value for MAX_SDK_BASE_PATH and allow it to be overridden by the user
set(MAX_SDK_BASE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../max-sdk-base" CACHE PATH "Path to max-sdk-base")


# Optionally, you can set global compiler or build settings here

# Add subdirectories for each of the Max MSP objects
add_subdirectory(ll_2dslider)
add_subdirectory(ll_fastforward)
add_subdirectory(ll_mcwaveform)
add_subdirectory(ll_number)
