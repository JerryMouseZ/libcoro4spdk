file(GLOB libsrc *.cpp)
add_library(libcoro4spdk STATIC ${libsrc})

if(NOT DEFINED SPDK_LINK_LIBRARIES)
  include("${CMAKE_CURRENT_LIST_DIR}/../../../cmake/SpdkDeps.cmake")
endif()

target_link_options(libcoro4spdk PUBLIC ${SPDK_LINK_OPTIONS})
target_link_libraries(libcoro4spdk PUBLIC ${SPDK_LINK_LIBRARIES})
target_include_directories(libcoro4spdk PUBLIC ${SPDK_INCLUDE_DIR})
target_include_directories(libcoro4spdk PUBLIC include)
