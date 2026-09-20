add_executable(TrajectoryControl "")

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
include_directories("${CMAKE_CURRENT_LIST_DIR}/src/BuidMap")
include_directories("${CMAKE_CURRENT_LIST_DIR}/src/common")
include_directories("${CMAKE_CURRENT_LIST_DIR}/src/core")
include_directories("${CMAKE_CURRENT_LIST_DIR}/src/manual")
include_directories("${CMAKE_CURRENT_LIST_DIR}/src/tools")
include_directories("${CMAKE_CURRENT_LIST_DIR}/../util")
include_directories("${CMAKE_CURRENT_LIST_DIR}/../util/debug")
include_directories("${CMAKE_CURRENT_LIST_DIR}/../util/hdmap")

# 添加 OpenCV 配置路径
if($ENV{USERNAME} STREQUAL "crb")
    set(OpenCV_DIR "D:/Library/opencv/build")
else()
    set(OpenCV_DIR "D:/Program Files/Library/opencv/build")
endif()
set(OpenCV_BIN_DIR "${OpenCV_DIR}/x64/vc16/bin")
find_package(OpenCV REQUIRED)

# 打印 OpenCV 状态（可选）
message(STATUS "OpenCV version: ${OpenCV_VERSION}")
message(STATUS "OpenCV include dirs: ${OpenCV_INCLUDE_DIRS}")
message(STATUS "OpenCV libs: ${OpenCV_LIBS}")

# 加入 OpenCV 的头文件路径（确保 OpenCV_INCLUDE_DIRS 有效）
include_directories(${OpenCV_INCLUDE_DIRS})

IF (MSVC)
    string(REPLACE "/MDd" "/MD" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
ENDIF()

file(GLOB_RECURSE srcs *.h *.hh *i *.hpp *.c *.cc *.cpp)
file(GLOB_RECURSE util_srcs "${CMAKE_CURRENT_LIST_DIR}/../util/*.c" "${CMAKE_CURRENT_LIST_DIR}/../util/*.cpp" "${CMAKE_CURRENT_LIST_DIR}/../util/*.h" "${CMAKE_CURRENT_LIST_DIR}/../util/*.hpp")
file(GLOB_RECURSE json_files *.json *.txt)
file(GLOB_RECURSE yaml_files "${CMAKE_CURRENT_LIST_DIR}/../TrajectoryControl/m_strategy/*.yaml")
file(GLOB_RECURSE strategies "${CMAKE_CURRENT_LIST_DIR}/../TrajectoryControl/m_strategy/*.stg")

target_sources(TrajectoryControl PRIVATE ${srcs})
target_sources(TrajectoryControl PRIVATE ${util_srcs})
target_sources(TrajectoryControl PRIVATE ${json_files})
target_sources(TrajectoryControl PRIVATE ${yaml_files})
target_sources(TrajectoryControl PRIVATE ${strategies})

source_group(TREE ${CMAKE_CURRENT_LIST_DIR} FILES ${srcs})
source_group(TREE "${CMAKE_CURRENT_LIST_DIR}/../util" PREFIX "util" FILES ${util_srcs})
source_group(TREE ${CMAKE_CURRENT_LIST_DIR} FILES ${json_files})
source_group(TREE "${CMAKE_CURRENT_LIST_DIR}/../TrajectoryControl/m_strategy" FILES ${yaml_files})
source_group(TREE "${CMAKE_CURRENT_LIST_DIR}/../TrajectoryControl/m_strategy" PREFIX "m_strategy" FILES ${strategies})

SetDefaultTargetProperties(TrajectoryControl)

IF (WIN32)
    target_link_libraries(TrajectoryControl
    # 链接 SimOneAPI 的静态库
    ${LIB_DIR}/SimOneAPI.lib
    ${LIB_DIR}/SSD.lib
    ${LIB_DIR}/HDMapModule.lib
    ${LIB_DIR}/ProtobufModule.lib

    # 链接 OpenCV 库
    ${OpenCV_LIBS}

    # 后面还可以添加自己的其他静态库

    )

    add_custom_command(TARGET TrajectoryControl
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different  ${LIB_DIR}/SimOneAPI.dll ${BINARY_OUT_PATH}/SimOneAPI.dll
        COMMAND ${CMAKE_COMMAND} -E copy_if_different  ${LIB_DIR}/SSD.dll ${BINARY_OUT_PATH}/SSD.dll
        COMMAND ${CMAKE_COMMAND} -E copy_if_different  ${LIB_DIR}/HDMapModule.dll ${BINARY_OUT_PATH}/HDMapModule.dll

        # 拷贝 OpenCV DLL（可选，根据用到的模块手动指定）
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<IF:$<CONFIG:Debug>,${OpenCV_BIN_DIR}/opencv_world4110d.dll,${OpenCV_BIN_DIR}/opencv_world4110.dll>" ${BINARY_OUT_PATH}/
        )
ELSEIF (UNIX)
        target_link_libraries(TrajectoryControl debug ${LIB_DIR}/libSimOneAPI.so)
        target_link_libraries(TrajectoryControl optimized ${LIB_DIR}/libSimOneAPI.so -lpthread)
        target_link_libraries(TrajectoryControl debug ${LIB_DIR}/libSSD.so)
        target_link_libraries(TrajectoryControl optimized ${LIB_DIR}/libSSD.so -lpthread)
        target_link_libraries(TrajectoryControl debug ${LIB_DIR}/libHDMapModule.so)
        target_link_libraries(TrajectoryControl optimized ${LIB_DIR}/libHDMapModule.so)

        add_custom_command(TARGET TrajectoryControl
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different  ${LIB_DIR}/libSimOneAPI.so ${BINARY_OUT_PATH}/libSimOneAPI.so
        COMMAND ${CMAKE_COMMAND} -E copy_if_different  ${LIB_DIR}/libSSD.so ${BINARY_OUT_PATH}/libSSD.so
        COMMAND ${CMAKE_COMMAND} -E copy_if_different  ${LIB_DIR}/libHDMapModule.so ${BINARY_OUT_PATH}/libHDMapModule.so
        )

        # Linux 下使用 OpenCV，这里也加 ${OpenCV_LIBS}
        target_link_libraries(TrajectoryControl ${OpenCV_LIBS})
ENDIF()