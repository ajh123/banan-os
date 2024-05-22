cmake_minimum_required(VERSION 3.26)

project(desktop CXX)

set(SOURCES
	drawing.hpp
	font.hpp
	gui.hpp
	main.cpp
)

add_executable(desktop ${SOURCES})
target_compile_options(desktop PUBLIC -O2 -g)
target_link_libraries(desktop PUBLIC libc ban)

add_custom_target(desktop-install
	COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/desktop ${BANAN_BIN}/
	DEPENDS desktop
)
