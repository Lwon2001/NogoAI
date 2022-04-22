cmake_minimum_required(VERSION 3.21)
project(NoGo)

set(CMAKE_CXX_STANDARD 17)

add_executable(NoGo main.cpp)

add_executable(main_debug main_debug.cpp jsoncpp)
add_executable(main_debug2 main_debug2.cpp jsoncpp)
add_executable(main06 main06.cpp jsoncpp)
add_executable(example example.cpp)
add_executable(MCTS_UCT MCTS_UCT.cpp)
add_executable(main_expandall main_expandall.cpp)
add_executable(Combine Combine.cpp)
add_executable(Combine_Plus Combine_Plus.cpp)
add_executable(sss sss.cpp)