cmake_minimum_required(VERSION 2.8)

project(atc3dgtracker)

set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)

find_package(LibUSB)

include_directories(include)


# build shared library
add_library(atclib SHARED src/atclib.cpp)
target_link_libraries(atclib ${LIBUSB_LIBRARY})
set_target_properties(atclib
	PROPERTIES
	VERSION 0.0.1
	SOVERSION 0.0.1
)


# build example
ADD_EXECUTABLE(atcTest examples/atcTest.cpp)
TARGET_LINK_LIBRARIES(atcTest atclib)
SET_TARGET_PROPERTIES(atcTest PROPERTIES OUTPUT_NAME atcTest)



install(TARGETS atcTest RUNTIME DESTINATION /usr/bin)
install(
	TARGETS atclib
	DESTINATION /usr/lib
	PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
)

install(
	FILES include/atclib.h
	DESTINATION include
	PERMISSIONS OWNER_READ GROUP_READ WORLD_READ
)

install(
	FILES lib/udev/rules.d/99-libusb.rules
	DESTINATION lib/udev/rules.d
	PERMISSIONS OWNER_READ GROUP_READ WORLD_READ
)

# reload udev rules
install(
	CODE "execute_process(COMMAND udevadm control --reload-rules)"
)
install(
	CODE "execute_process(COMMAND udevadm trigger)"
)
