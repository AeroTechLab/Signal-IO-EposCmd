cmake_minimum_required( VERSION 2.8.12 )
project( Signal-IO-EposCmd )

set( CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wall" )
set( CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -Wall" )

include( ${CMAKE_CURRENT_LIST_DIR}/interface/CMakeLists.txt )

add_library( EposCmdIO MODULE  ${CMAKE_CURRENT_LIST_DIR}/signal_io_epos.cpp )
set_target_properties( EposCmdIO PROPERTIES PREFIX "" )
target_link_libraries( EposCmdIO -lEposCmd )
