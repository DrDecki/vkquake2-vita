cmake_minimum_required(VERSION 3.16)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  if(DEFINED ENV{VITASDK})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VITASDK}/share/vita.toolchain.cmake" CACHE PATH "toolchain file")
  else()
    message(FATAL_ERROR "Please define VITASDK to point to your SDK path!")
  endif()
endif()

project(vkquake2_vita C CXX)
include("${VITASDK}/share/vita.cmake" REQUIRED)

set(VITA_APP_NAME "vkQuake2")
set(VITA_TITLEID  "VKQUAKE02")
set(VITA_VERSION  "01.10")

set(VKVITA_DIR "" CACHE PATH "VkVita source tree (empty: use the installed VkVita)")
if(VKVITA_DIR)
  set(VKV_BUILD_SAMPLES OFF CACHE BOOL "" FORCE)
  add_subdirectory(${VKVITA_DIR} vkvita)
else()
  find_package(VkVita 0.1 REQUIRED)
endif()

set(Q2 ${CMAKE_CURRENT_SOURCE_DIR}/..)

set(CLIENT_SOURCES
  client/cl_cin.c client/cl_ents.c client/cl_fx.c client/cl_input.c client/cl_inv.c client/cl_main.c
  client/cl_newfx.c client/cl_parse.c client/cl_pred.c client/cl_scrn.c client/cl_tent.c client/cl_view.c
  client/console.c client/keys.c client/menu.c client/qmenu.c client/snd_dma.c client/snd_mem.c
  client/snd_mix.c
)
set(COMMON_SOURCES
  qcommon/cmd.c qcommon/cmodel.c qcommon/common.c qcommon/crc.c qcommon/cvar.c qcommon/files.c
  qcommon/md4.c qcommon/net_chan.c qcommon/pmove.c
)
set(SERVER_SOURCES
  server/sv_ccmds.c server/sv_ents.c server/sv_game.c server/sv_init.c server/sv_main.c server/sv_send.c
  server/sv_user.c server/sv_world.c
)
set(REF_SOURCES
  ref_vk/vk_buffer.c ref_vk/vk_cmd.c ref_vk/vk_common.c ref_vk/vk_device.c ref_vk/vk_draw.c ref_vk/vk_image.c
  ref_vk/vk_light.c ref_vk/vk_mesh.c ref_vk/vk_model.c ref_vk/vk_pipeline.c ref_vk/vk_rmain.c ref_vk/vk_rmisc.c
  ref_vk/vk_rsurf.c ref_vk/vk_shaders.c ref_vk/vk_swapchain.c ref_vk/vk_validation.c ref_vk/vk_warp.c
  ref_vk/vk_mem_alloc.cpp
)
set(GAME_SHARED_SOURCES game/q_shared.c game/m_flash.c)
set(PLATFORM_SOURCES
  vita/sys_vita.c vita/in_vita.c vita/vid_vita.c vita/vk_imp.c vita/snd_vita.c vita/net_vita.c vita/cd_vita.c vita/music_vita.c
  linux/glob.c linux/vid_menu.c
)

set(SOURCES)
foreach(f ${CLIENT_SOURCES} ${COMMON_SOURCES} ${SERVER_SOURCES} ${REF_SOURCES} ${GAME_SHARED_SOURCES} ${PLATFORM_SOURCES})
  list(APPEND SOURCES ${Q2}/${f})
endforeach()

# Each game module (baseq2 and the mission packs) is linked into one object
# with only GetGameAPI left global and renamed to GetGameAPI_<dir>, like a DLL.
# Sys_GetGameAPI picks one by the game cvar.
set(GAME_MODULES game xatrix rogue zaero ctf)
set(GAME_OBJECTS)
foreach(m ${GAME_MODULES})
  file(GLOB mod_sources ${Q2}/${m}/*.c)
  list(FILTER mod_sources EXCLUDE REGEX "rogue/m_move2[.]c$")
  add_library(game_${m} OBJECT ${mod_sources})
  target_compile_definitions(game_${m} PRIVATE C_ONLY stricmp=strcasecmp)
  target_compile_options(game_${m} PRIVATE -O2 -std=gnu11 -fcommon -fno-strict-aliasing -w)
  set(obj ${CMAKE_CURRENT_BINARY_DIR}/game_${m}.o)
  add_custom_command(OUTPUT ${obj}
    COMMAND ${CMAKE_LINKER} -r -d -o ${obj}.tmp $<TARGET_OBJECTS:game_${m}>
    COMMAND ${CMAKE_OBJCOPY} --keep-global-symbol=GetGameAPI ${obj}.tmp ${obj}.tmp2
    COMMAND ${CMAKE_OBJCOPY} --redefine-sym GetGameAPI=GetGameAPI_${m} ${obj}.tmp2 ${obj}
    DEPENDS game_${m} $<TARGET_OBJECTS:game_${m}>
    COMMAND_EXPAND_LISTS VERBATIM)
  list(APPEND GAME_OBJECTS ${obj})
endforeach()
set_source_files_properties(${GAME_OBJECTS} PROPERTIES EXTERNAL_OBJECT TRUE GENERATED TRUE)

add_executable(vkquake2 ${SOURCES} ${GAME_OBJECTS})
target_include_directories(vkquake2 PRIVATE ${Q2})
target_compile_definitions(vkquake2 PRIVATE
  REF_HARD_LINKED C_ONLY stricmp=strcasecmp
  VMA_VULKAN_VERSION=1000000 VMA_DEDICATED_ALLOCATION=0 VMA_BIND_MEMORY2=0 VMA_MEMORY_BUDGET=0
  VMA_BUFFER_DEVICE_ADDRESS=0 VMA_MEMORY_PRIORITY=0 VMA_KHR_MAINTENANCE4=0 VMA_KHR_MAINTENANCE5=0
  VMA_GET_PHYSICAL_DEVICE_PROPERTIES2=0 VMA_STATS_STRING_ENABLED=0
)
target_compile_options(vkquake2 PRIVATE
  -O2 -fno-strict-aliasing -Wno-format-truncation -Wno-unused-result -Wno-format-overflow
  $<$<COMPILE_LANGUAGE:C>:-std=gnu11>
  $<$<COMPILE_LANGUAGE:CXX>:-std=gnu++17>
)
target_link_libraries(vkquake2 PRIVATE
  VkVita::vkvita SceAppMgr_stub SceCtrl_stub SceAudio_stub ScePower_stub SceNet_stub SceNetCtl_stub SceSysmodule_stub m stdc++
)

vita_create_self(vkquake2.self vkquake2 UNSAFE)

file(GLOB VITA_SHADER_PACK ${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.gxp)
set(VITA_SHADER_FILES)
foreach(f ${VITA_SHADER_PACK})
  get_filename_component(n ${f} NAME)
  list(APPEND VITA_SHADER_FILES FILE ${f} shaders/${n})
endforeach()

vita_create_vpk(vkquake2.vpk ${VITA_TITLEID} vkquake2.self
  VERSION ${VITA_VERSION}
  NAME ${VITA_APP_NAME}
  FILE ${CMAKE_CURRENT_SOURCE_DIR}/sce_sys/icon0.png sce_sys/icon0.png
  FILE ${CMAKE_CURRENT_SOURCE_DIR}/sce_sys/livearea/contents/bg.png sce_sys/livearea/contents/bg.png
  FILE ${CMAKE_CURRENT_SOURCE_DIR}/sce_sys/livearea/contents/startup.png sce_sys/livearea/contents/startup.png
  FILE ${CMAKE_CURRENT_SOURCE_DIR}/sce_sys/livearea/contents/template.xml sce_sys/livearea/contents/template.xml
  ${VITA_SHADER_FILES}
)
