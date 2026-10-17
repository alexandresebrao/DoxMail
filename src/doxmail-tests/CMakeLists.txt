# DoxMail: checks for the GTK 4 port, built with WITH_GTK4 only.
# Run them with: ctest --test-dir <build> or the binaries directly.

add_executable(test-canvas test-canvas.c)

target_compile_definitions(test-canvas PRIVATE
	-DG_LOG_DOMAIN=\"test-canvas\"
)

target_include_directories(test-canvas PRIVATE
	${CMAKE_BINARY_DIR}
	${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test-canvas
	gnomecanvas
)

enable_testing()
add_test(NAME canvas COMMAND test-canvas)
