# build service
set(SOURCE_FILES NonBlockingTest.cpp)

add_executable(runNetworkTests ${SOURCE_FILES} ${BACKWARD_ENABLE})
target_link_libraries(runNetworkTests Network gtest gtest_main)

add_backward(runNetworkTests)
add_test(runNetworkTests runNetworkTests)
