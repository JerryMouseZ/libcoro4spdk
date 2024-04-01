include_directories(${GTEST_INCLUDE_DIRS})

# 定义一个函数用于添加测试
function(add_mytest testname)
    # 通过参数传递测试名称
    add_executable(${testname} ${testname}.cpp)
    # 链接 GTest 库和其他可能的库
    target_link_libraries(${testname} libcoro4spdk GTest::GTest GTest::Main)
endfunction()

# 获取当前 CMakeLists.txt 文件的目录
get_filename_component(PROJECT_DIR "${CMAKE_CURRENT_LIST_DIR}" ABSOLUTE)
# 使用相对路径获取所有的源文件
file(GLOB_RECURSE MY_SOURCE_FILES RELATIVE ${PROJECT_DIR} *.cpp)
foreach(SOURCE_FILE ${MY_SOURCE_FILES})
    # 去掉文件名后缀
    string(REPLACE ".cpp" "" SOURCE_FILE_NO_SUFFIX ${SOURCE_FILE})
    # 添加测试并传递测试名称
    add_mytest(${SOURCE_FILE_NO_SUFFIX})
endforeach()

