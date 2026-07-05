cmake_minimum_required(VERSION 3.16)

set(CMAKE_SYSTEM_NAME "Generic")
set(CMAKE_C_COMPILER "arm-vita-eabi-gcc")
set(CMAKE_CXX_COMPILER "arm-vita-eabi-g++")

project(noled)

if(NOT DEFINED ENV{VITASDK})
	message(FATAL_ERROR "Please define VITASDK to point to your SDK path")
endif()

set(VITASDK "$ENV{VITASDK}")
set(VITA_ELF_CREATE "${VITASDK}/bin/vita-elf-create")
set(VITA_MAKE_FSELF "${VITASDK}/bin/vita-make-fself")
set(VITA_MKSFOEX "${VITASDK}/bin/vita-mksfoex")
set(VITA_PACK_VPK "${VITASDK}/bin/vita-pack-vpk")

set(CMAKE_C_FLAGS "-Wl,-q -Wall -O3 -std=gnu99")

include_directories(
	${CMAKE_CURRENT_SOURCE_DIR}
)

link_directories(
	${CMAKE_CURRENT_BINARY_DIR}
)

# ---------------------------------------------------------------------------
# noled.skprx - the release plugin, the only default build output
# ---------------------------------------------------------------------------

add_executable(noled.elf
	main.c
)

target_link_libraries(noled.elf
	taihenForKernel_stub
	taihenModuleUtils_stub
	SceGpioForDriver_stub
	SceKernelSuspendForDriver_stub
	ScePowerForDriver_stub
	SceSysconForDriver_stub
	SceThreadmgrForDriver_stub
	)

set_target_properties(noled.elf
	PROPERTIES LINK_FLAGS "-nostdlib"
	COMPILE_FLAGS "-D__VITA_KERNEL__ -fno-builtin"
)

add_custom_target(noled.skprx ALL
	COMMAND ${VITA_ELF_CREATE} -e ${CMAKE_SOURCE_DIR}/noled.yml noled.elf noled.velf ${CMAKE_SOURCE_DIR}/taihen.json
	COMMAND ${VITA_MAKE_FSELF} -c noled.velf noled.skprx
)
add_dependencies(noled.skprx noled.elf)

add_custom_target(send
	COMMAND curl -T noled.skprx ftp://$(PSVITAIP):1337/ux0:/data/tai/
	DEPENDS noled.skprx
)

# ---------------------------------------------------------------------------
# dev/ - diagnostic plugin, probe module and loader app; built on demand:
#   cmake --build build --target noled_dev.skprx noled_loader.vpk
# see docs/runtime-testing.md
# ---------------------------------------------------------------------------

add_executable(noled_dev.elf EXCLUDE_FROM_ALL
	dev/main.c
)

target_link_libraries(noled_dev.elf
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

set_target_properties(noled_dev.elf
	PROPERTIES LINK_FLAGS "-nostdlib"
	COMPILE_FLAGS "-D__VITA_KERNEL__ -fno-builtin"
)

add_custom_target(noled_dev.skprx
	COMMAND ${VITA_ELF_CREATE} -e ${CMAKE_SOURCE_DIR}/dev/noled_dev.yml noled_dev.elf noled_dev.velf ${CMAKE_SOURCE_DIR}/taihen.json
	COMMAND ${VITA_MAKE_FSELF} -c noled_dev.velf noled_dev.skprx
)
add_dependencies(noled_dev.skprx noled_dev.elf)

add_executable(noled_cmd_probe.elf EXCLUDE_FROM_ALL
	dev/cmd_probe.c
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
	COMMAND ${VITA_ELF_CREATE} -e ${CMAKE_SOURCE_DIR}/dev/cmd_probe.yml noled_cmd_probe.elf noled_cmd_probe.velf
	COMMAND ${VITA_MAKE_FSELF} -c noled_cmd_probe.velf noled_cmd_probe.skprx
)
add_dependencies(noled_cmd_probe.skprx noled_cmd_probe.elf)

set(DEBUG_SCREEN_DIR "${VITASDK}/share/gcc-arm-vita-eabi/samples/common")

add_executable(noled_loader.elf EXCLUDE_FROM_ALL
	dev/loader/main.c
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

# the loader pairs with the dev plugin: it is packed into the vpk under the
# name the loader expects (app0:noled.skprx)
add_custom_target(noled_loader.vpk
	COMMAND ${VITA_ELF_CREATE} noled_loader.elf noled_loader.velf
	COMMAND ${VITA_MAKE_FSELF} -c noled_loader.velf eboot.bin
	COMMAND ${VITA_MKSFOEX} -s TITLE_ID=NOLED0001 -s APP_VER=01.00 "noLED Loader" noled_loader_param.sfo
	COMMAND ${VITA_PACK_VPK} -s noled_loader_param.sfo -b eboot.bin -a noled_dev.skprx=noled.skprx noled_loader.vpk
	DEPENDS noled_loader.elf
)
add_dependencies(noled_loader.vpk noled_dev.skprx)
