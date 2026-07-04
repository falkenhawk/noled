cmake_minimum_required(VERSION 3.16)

set(CMAKE_SYSTEM_NAME "Generic")
set(CMAKE_C_COMPILER "arm-vita-eabi-gcc")
set(CMAKE_CXX_COMPILER "arm-vita-eabi-g++")

project(HENkaku)

if(NOT DEFINED ENV{VITASDK})
	message(FATAL_ERROR "Please define VITASDK to point to your SDK path")
endif()

set(VITASDK "$ENV{VITASDK}")
set(VITA_ELF_CREATE "${VITASDK}/bin/vita-elf-create")
set(VITA_MAKE_FSELF "${VITASDK}/bin/vita-make-fself")
set(VITA_MKSFOEX "${VITASDK}/bin/vita-mksfoex")
set(VITA_PACK_VPK "${VITASDK}/bin/vita-pack-vpk")

set(CMAKE_C_FLAGS "-Wl,-q -Wall -O3 -std=gnu99")
set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -std=c++11 -fno-rtti -fno-exceptions")

include_directories(
)

link_directories(
	${CMAKE_CURRENT_BINARY_DIR}
)

if (NOT RELEASE)
	add_definitions(-DENABLE_LOGGING)
endif()

add_executable(noled.elf
	main.c
)

target_link_libraries(noled.elf
	taihenForKernel_stub
	taihenModuleUtils_stub
	SceGpioForDriver_stub
	SceIofilemgrForDriver_stub
	SceKernelSuspendForDriver_stub
	SceLedForDriver_stub
	ScePowerForDriver_stub
	SceSysconForDriver_stub
	SceThreadmgrForDriver_stub
	)

set_target_properties(noled.elf
	PROPERTIES LINK_FLAGS "-nostdlib"
	COMPILE_FLAGS "-D__VITA_KERNEL__ -fno-builtin"
)

add_custom_target(noled.skprx ALL
	COMMAND vita-elf-create -e ${CMAKE_SOURCE_DIR}/noled.yml noled.elf noled.velf ${CMAKE_SOURCE_DIR}/taihen.json
	COMMAND vita-make-fself -c noled.velf noled.skprx
)
add_dependencies(noled.skprx noled.elf)

add_executable(noled_cmd_probe.elf EXCLUDE_FROM_ALL
	cmd_probe.c
)

target_link_libraries(noled_cmd_probe.elf
	SceIofilemgrForDriver_stub
	SceSysconForDriver_stub
	)

set_target_properties(noled_cmd_probe.elf
	PROPERTIES LINK_FLAGS "-nostdlib"
	COMPILE_FLAGS "-D__VITA_KERNEL__ -fno-builtin"
)

add_custom_target(noled_cmd_probe.skprx
	COMMAND vita-elf-create -e ${CMAKE_SOURCE_DIR}/cmd_probe.yml noled_cmd_probe.elf noled_cmd_probe.velf
	COMMAND vita-make-fself -c noled_cmd_probe.velf noled_cmd_probe.skprx
)
add_dependencies(noled_cmd_probe.skprx noled_cmd_probe.elf)

add_custom_target(send
	COMMAND curl -T noled.skprx ftp://$(PSVITAIP):1337/ux0:/data/tai/
	DEPENDS noled.skprx
)

set(DEBUG_SCREEN_DIR "${VITASDK}/share/gcc-arm-vita-eabi/samples/common")

add_executable(noled_loader.elf
	loader/main.c
	${DEBUG_SCREEN_DIR}/debugScreen.c
)

target_include_directories(noled_loader.elf PRIVATE
	${DEBUG_SCREEN_DIR}
)

target_link_libraries(noled_loader.elf
	taihen_stub
	SceCtrl_stub
	SceDisplay_stub
	SceIofilemgr_stub
	SceLibKernel_stub
	SceSysmem_stub
	ScePower_stub
	SceLibc_stub
)

add_custom_target(noled_loader.vpk ALL
	COMMAND ${VITA_ELF_CREATE} noled_loader.elf noled_loader.velf
	COMMAND ${VITA_MAKE_FSELF} -c noled_loader.velf eboot.bin
	COMMAND ${VITA_MKSFOEX} -s TITLE_ID=NOLED0001 -s APP_VER=01.00 "noLED Loader" noled_loader_param.sfo
	COMMAND ${VITA_PACK_VPK} -s noled_loader_param.sfo -b eboot.bin -a noled.skprx=noled.skprx noled_loader.vpk
	DEPENDS noled_loader.elf noled.skprx
)
