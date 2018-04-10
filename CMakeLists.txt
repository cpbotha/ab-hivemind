cmake_minimum_required(VERSION 3.9)

# macOS example:
set(ARDUINO_PORT /dev/cu.usbmodem146121)
# Linux example:
#set(ARDUINO_PORT /dev/ttyACM0)

project(ab_hivemind)

set(CMAKE_CXX_STANDARD 11)

add_executable(ab_hivemind ab-hivemind-main.cpp)
set_target_properties(ab_hivemind PROPERTIES LINKER_LANGUAGE CXX)

include_directories(
        ${ARDUINO_SKETCHBOOK_PATH}/libraries/FastLED/
        ${ARDUINO_SKETCHBOOK_PATH}/libraries/XBee-Arduino_library/
)

# use the following two targets for building and uploading sketches from clion.
# select "verify" for just building, select "upload" for building and uploading.
# make sure you have selected the correct board and port in the arduino IDE before building this target!
# instead, you can also use --board and --port arguments here.
# - for the m0, --board arduino:samd:mzero_bl
# - for the uno, --board arduino:avr:uno
# see https://github.com/arduino/Arduino/blob/master/build/shared/manpage.adoc for arduino cmd options
add_custom_target(upload ALL ${ARDUINO_CMD} --upload --board ${ARDUINO_BOARD} --port ${ARDUINO_PORT} --preserve-temp-files --verbose blink.ino WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
add_custom_target(verify ALL ${ARDUINO_CMD} --verify --board ${ARDUINO_BOARD} --preserve-temp-files --verbose blink.ino WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
