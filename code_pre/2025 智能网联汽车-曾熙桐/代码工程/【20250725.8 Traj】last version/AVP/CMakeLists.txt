add_executable(AVP "")

include_directories("${INCLUDE_DIR}")

IF (WIN32)
    add_definitions(-D_WIN32_WINNT=0x0501)
ENDIF()

add_definitions(-DNOMINMAX)
add_definitions(-DUNICODE)
add_definitions(-D_UNICODE)

# 设置头文件路径
include_directories("${INCLUDE_DIR}")
include_directories("${INCLUDE_DIR}/util")
include_directories("${INCLUDE_DIR}/HDMap")
include_directories("${CMAKE_CURRENT_LIST_DIR}/src")
include_directories("${CMAKE_CURRENT_LIST_DIR}/../util")
include_directories("${CMAKE_CURRENT_LIST_DIR}/../util/debug")
include_directories("${CMAKE_CURRENT_LIST_DIR}/../util/hdmap")
include_directories(${SIMONE_API_SAMPLE}/include/eigen/3.2.3/include/eigen3)

IF (MSVC)
    string(REPLACE "/MDd" "/MD" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
ENDIF()

file(GLOB_RECURSE srcs *.h *.hh *i *.hpp *.c *.cc *.cpp)
file(GLOB_RECURSE util_srcs "${CMAKE_CURRENT_LIST_DIR}/../util/*.c" "${CMAKE_CURRENT_LIST_DIR}/../util/*.cpp" "${CMAKE_CURRENT_LIST_DIR}/../util/*.h" "${CMAKE_CURRENT_LIST_DIR}/../util/*.hpp")
file(GLOB_RECURSE json_files "${CMAKE_CURRENT_LIST_DIR}/../TrajectoryControl/*.json" "${CMAKE_CURRENT_LIST_DIR}/../TrajectoryControl/readme.txt")
file(GLOB_RECURSE yaml_files "${CMAKE_CURRENT_LIST_DIR}/../TrajectoryControl/m_strategy/*.yaml")
file(GLOB_RECURSE strategies "${CMAKE_CURRENT_LIST_DIR}/../TrajectoryControl/m_strategy/*.stg")

target_sources(AVP PRIVATE ${srcs})
target_sources(AVP PRIVATE ${util_srcs})
target_sources(AVP PRIVATE ${json_files})
target_sources(AVP PRIVATE ${yaml_files})
target_sources(AVP PRIVATE ${strategies})

source_group(TREE ${CMAKE_CURRENT_LIST_DIR} FILES ${srcs})
source_group(TREE "${CMAKE_CURRENT_LIST_DIR}/../util" PREFIX "util" FILES ${util_srcs})
source_group(TREE "${CMAKE_CURRENT_LIST_DIR}/../TrajectoryControl/m_strategy" FILES ${yaml_files})
source_group(TREE "${CMAKE_CURRENT_LIST_DIR}/../TrajectoryControl/m_strategy" PREFIX "m_strategy" FILES ${strategies})

SetDefaultTargetProperties(AVP)

IF (WIN32)
    target_link_libraries(AVP
        ${LIB_DIR}/SimOneAPI.lib
        ${LIB_DIR}/SSD.lib
        ${LIB_DIR}/HDMapModule.lib 
        ${LIB_DIR}/ProtobufModule.lib
        # 可以在这里添加其他静态库
    )

    add_custom_command(TARGET AVP
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different  ${LIB_DIR}/SimOneAPI.dll ${BINARY_OUT_PATH}/SimOneAPI.dll
        COMMAND ${CMAKE_COMMAND} -E copy_if_different  ${LIB_DIR}/SSD.dll ${BINARY_OUT_PATH}/SSD.dll
        COMMAND ${CMAKE_COMMAND} -E copy_if_different  ${LIB_DIR}/HDMapModule.dll ${BINARY_OUT_PATH}/HDMapModule.dll
    )
    
ELSEIF (UNIX)
    target_link_libraries(AVP debug ${LIB_DIR}/libSimOneAPI.so)
    target_link_libraries(AVP optimized ${LIB_DIR}/libSimOneAPI.so -lpthread)
    target_link_libraries(AVP debug /usr/lib/x86_64-linux-gnu/libcurl.so.4)
    target_link_libraries(AVP optimized /usr/lib/x86_64-linux-gnu/libcurl.so.4)
    target_link_libraries(AVP debug ${LIB_DIR}/libSSD.so)
    target_link_libraries(AVP optimized ${LIB_DIR}/libSSD.so -lpthread)
    target_link_libraries(AVP debug ${LIB_DIR}/libHDMapModule.so)
    target_link_libraries(AVP optimized ${LIB_DIR}/libHDMapModule.so)

    add_custom_command(TARGET AVP
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different  ${CMAKE_BINARY_DIR}/../../bin/AVP ${BINARY_OUT_PATH}/AVP
        COMMAND ${CMAKE_COMMAND} -E copy_if_different  ${LIB_DIR}/libSimOneAPI.so ${BINARY_OUT_PATH}/libSimOneAPI.so
        COMMAND ${CMAKE_COMMAND} -E copy_if_different  ${LIB_DIR}/libSSD.so ${BINARY_OUT_PATH}/libSSD.so
        COMMAND ${CMAKE_COMMAND} -E copy_if_different  ${LIB_DIR}/libHDMapModule.so ${BINARY_OUT_PATH}/libHDMapModule.so
    )
ENDIF()