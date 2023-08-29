cmake_minimum_required(VERSION 3.26)
project(c_demo)

set (CMAKE_CXX_STANDARD 17)

include_directories(/usr/local/include)
include_directories(/opt/homebrew/include)

link_directories(/usr/local/lib)
link_directories(/opt/homebrew/lib)

add_executable(main main.cpp
                    handler.h
                    watcher.h
                    watchable.h
                    connectionwrapper.h
                    )
target_link_libraries(main uv amqpcpp pthread dl ssl)