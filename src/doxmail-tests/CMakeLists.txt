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

add_executable(test-eutil test-eutil.c)

target_compile_definitions(test-eutil PRIVATE
	-DG_LOG_DOMAIN=\"test-eutil\"
)

target_compile_options(test-eutil PRIVATE
	${EVOLUTION_DATA_SERVER_CFLAGS}
	${GNOME_PLATFORM_CFLAGS}
)

target_include_directories(test-eutil PRIVATE
	${CMAKE_BINARY_DIR}
	${CMAKE_BINARY_DIR}/src
	${CMAKE_SOURCE_DIR}/src
	${EVOLUTION_DATA_SERVER_INCLUDE_DIRS}
	${GNOME_PLATFORM_INCLUDE_DIRS}
)

target_link_libraries(test-eutil
	evolution-util
	${EVOLUTION_DATA_SERVER_LDFLAGS}
	${GNOME_PLATFORM_LDFLAGS}
)

add_test(NAME eutil COMMAND test-eutil)

add_executable(test-ui-files test-ui-files.c)

target_compile_definitions(test-ui-files PRIVATE
	-DG_LOG_DOMAIN=\"test-ui-files\"
	-DDOXMAIL_SOURCE_DIR=\"${CMAKE_SOURCE_DIR}\"
)

target_compile_options(test-ui-files PRIVATE
	${EVOLUTION_DATA_SERVER_CFLAGS}
	${GNOME_PLATFORM_CFLAGS}
)

target_include_directories(test-ui-files PRIVATE
	${CMAKE_BINARY_DIR}
	${CMAKE_BINARY_DIR}/src
	${CMAKE_SOURCE_DIR}/src
	${EVOLUTION_DATA_SERVER_INCLUDE_DIRS}
	${GNOME_PLATFORM_INCLUDE_DIRS}
)

target_link_libraries(test-ui-files
	evolution-util
	evolution-mail
	evolution-calendar
	econtacteditor
	${EVOLUTION_DATA_SERVER_LDFLAGS}
	${GNOME_PLATFORM_LDFLAGS}
)

add_test(NAME ui-files COMMAND test-ui-files)
