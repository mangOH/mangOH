# No backwards compatibility testing has been performed, but cmake complains if a minimum version
# isn't specified.  3.5 was chosen because it's the version that was included with Ubuntu 16.04
# LTS.
cmake_minimum_required (VERSION 3.5)
project (libBluezDBus)

find_package(PkgConfig REQUIRED)

pkg_check_modules(GLIB2 REQUIRED glib-2.0 gio-2.0 gio-unix-2.0)
include_directories(${GLIB2_INCLUDE_DIRS})
link_directories(${GLIB2_LIBRARY_DIRS})

include_directories("${PROJECT_BINARY_DIR}")

find_program(GDBUSCODEGEN NAMES gdbus-codegen)
if (NOT GDBUSCODEGEN)
  message(SEND_ERROR "Couldn't find gdbus-codegen")
endif (NOT GDBUSCODEGEN)

macro(codegen_bluez interface)
    add_custom_command(
        OUTPUT ${interface}.c ${interface}.h
        COMMAND gdbus-codegen  --generate-c-code=${interface} --interface-prefix org.bluez. --c-namespace Bluez ${CMAKE_CURRENT_SOURCE_DIR}/${interface}.xml
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${interface}.xml
    )
endmacro(codegen_bluez)

set(
    bluez_interfaces
    org.bluez.Adapter1
    org.bluez.Device1
    org.bluez.GattCharacteristic1
    org.bluez.GattDescriptor1
    org.bluez.GattManager1
    org.bluez.GattService1
    org.bluez.LEAdvertisement1
    org.bluez.LEAdvertisingManager1
    )

foreach(intf ${bluez_interfaces})
    codegen_bluez(${intf})
    list(APPEND gen_code ${intf}.c ${intf}.h)
endforeach(intf)


add_library(
    BluezDBus SHARED
    ${gen_code}
)
