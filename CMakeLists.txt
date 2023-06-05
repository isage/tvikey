cmake_minimum_required(VERSION 3.2)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  if(DEFINED ENV{VITASDK})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VITASDK}/share/vita.toolchain.cmake" CACHE PATH "toolchain file")
  else()
    message(FATAL_ERROR "Please define VITASDK to point to your SDK path!")
  endif()
endif()

project(tvikey)
include("${VITASDK}/share/vita.cmake" REQUIRED)

add_executable(${PROJECT_NAME}_kernel
  src/devices/process_bind.c
  src/devices/mouse.c
  src/devices/keyboard.c
  src/inputdevice.c
  src/util/ini.c
  src/main.c
)

target_link_libraries(${PROJECT_NAME}_kernel
  SceCtrlForDriver_stub
  SceDebugForDriver_stub
  SceIofilemgrForDriver_stub
  SceKernelSuspendForDriver_stub
  SceProcEventForDriver_stub
  SceSblAIMgrForDriver_stub
  SceSysclibForDriver_stub
  SceSysmemForDriver_stub
  SceSysrootForKernel_stub
  SceThreadmgrForDriver_stub
  SceUsbdForDriver_stub
  SceUsbServForDriver_stub
  taihenForKernel_stub
)

set_target_properties(${PROJECT_NAME}_kernel
  PROPERTIES LINK_FLAGS "-nostdlib"
)

vita_create_self(${PROJECT_NAME}.skprx ${PROJECT_NAME}_kernel
  CONFIG ${CMAKE_SOURCE_DIR}/${PROJECT_NAME}_kernel.yml
  UNSAFE
)
