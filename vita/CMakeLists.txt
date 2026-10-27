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
set(VITA_VERSION  "01.00")

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
set(GAME_SOURCES
  game/g_ai.c game/g_chase.c game/g_cmds.c game/g_combat.c game/g_func.c game/g_items.c game/g_main.c
  game/g_misc.c game/g_monster.c game/g_phys.c game/g_save.c game/g_spawn.c game/g_svcmds.c game/g_target.c
  game/g_trigger.c game/g_turret.c game/g_utils.c game/g_weapon.c game/m_actor.c game/m_berserk.c
  game/m_boss2.c game/m_boss3.c game/m_boss31.c game/m_boss32.c game/m_brain.c game/m_chick.c game/m_flash.c
  game/m_flipper.c game/m_float.c game/m_flyer.c game/m_gladiator.c game/m_gunner.c game/m_hover.c
  game/m_infantry.c game/m_insane.c game/m_medic.c game/m_move.c game/m_mutant.c game/m_parasite.c
  game/m_soldier.c game/m_supertank.c game/m_tank.c game/p_client.c game/p_hud.c game/p_trail.c game/p_view.c
  game/p_weapon.c game/q_shared.c
)
set(PLATFORM_SOURCES
  vita/sys_vita.c vita/in_vita.c vita/vid_vita.c vita/vk_imp.c vita/snd_vita.c vita/net_vita.c vita/cd_vita.c vita/music_vita.c
  linux/glob.c linux/vid_menu.c
)

set(SOURCES)
foreach(f ${CLIENT_SOURCES} ${COMMON_SOURCES} ${SERVER_SOURCES} ${REF_SOURCES} ${GAME_SOURCES} ${PLATFORM_SOURCES})
  list(APPEND SOURCES ${Q2}/${f})
endforeach()

foreach(f ${GAME_SOURCES})
  set_source_files_properties(${Q2}/${f} PROPERTIES COMPILE_DEFINITIONS "maxclients=game_maxclients;dedicated=game_dedicated")
endforeach()

add_executable(vkquake2 ${SOURCES})
target_include_directories(vkquake2 PRIVATE ${Q2})
target_compile_definitions(vkquake2 PRIVATE
  REF_HARD_LINKED GAME_HARD_LINKED C_ONLY stricmp=strcasecmp
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
  VkVita::vkvita SceCtrl_stub SceAudio_stub ScePower_stub SceNet_stub SceNetCtl_stub SceSysmodule_stub m stdc++
)

vita_create_self(vkquake2.self vkquake2 UNSAFE)
vita_create_vpk(vkquake2.vpk ${VITA_TITLEID} vkquake2.self
  VERSION ${VITA_VERSION}
  NAME ${VITA_APP_NAME}
  FILE ${CMAKE_CURRENT_SOURCE_DIR}/sce_sys/icon0.png sce_sys/icon0.png
  FILE ${CMAKE_CURRENT_SOURCE_DIR}/sce_sys/livearea/contents/bg.png sce_sys/livearea/contents/bg.png
  FILE ${CMAKE_CURRENT_SOURCE_DIR}/sce_sys/livearea/contents/startup.png sce_sys/livearea/contents/startup.png
  FILE ${CMAKE_CURRENT_SOURCE_DIR}/sce_sys/livearea/contents/template.xml sce_sys/livearea/contents/template.xml
)
