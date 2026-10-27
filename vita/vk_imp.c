/*
Copyright (C) 1997-2001 Id Software, Inc.
Copyright (C) 2018-2019 Krzysztof Kondrak
Copyright (C) 2026 DrDecki

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include <vkvita/vkvita.h>

#include "../ref_vk/vk_local.h"
#include "vk_vita.h"
#include "vita.h"

vkwstate_t vkw_state;

int Vkimp_SetMode(int *pwidth, int *pheight, int mode, qboolean fullscreen)
{
	int width, height;

	ri.Con_Printf(PRINT_ALL, "Initializing Vulkan display\n");
	if (!ri.Vid_GetModeInfo(&width, &height, mode))
	{
		ri.Con_Printf(PRINT_ALL, " invalid mode\n");
		return rserr_invalid_mode;
	}
	ri.Con_Printf(PRINT_ALL, "...%d x %d\n", width, height);

	*pwidth = width;
	*pheight = height;
	ri.Vid_NewWindow(width, height);
	return rserr_ok;
}

void Vkimp_GetInstanceExtensions(char **extensions, uint32_t *extCount)
{
	uint32_t count = 0, i;
	VkExtensionProperties *props;

	VK_VERIFY(vkEnumerateInstanceExtensionProperties(NULL, &count, NULL));
	props = malloc(sizeof(*props) * (count ? count : 1));
	VK_VERIFY(vkEnumerateInstanceExtensionProperties(NULL, &count, props));
	for (i = 0; i < count; i++)
		vk_config.vk_khr_get_physical_device_properties2_available |=
			strcmp(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME, props[i].extensionName) == 0;
	free(props);

	if (extensions)
	{
		extensions[0] = VK_KHR_SURFACE_EXTENSION_NAME;
		extensions[1] = VK_EXT_HEADLESS_SURFACE_EXTENSION_NAME;
		if (vk_config.vk_khr_get_physical_device_properties2_available)
			extensions[2] = VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME;
	}
	if (extCount)
		*extCount = vk_config.vk_khr_get_physical_device_properties2_available ? 3 : 2;
}

VkResult Vkimp_CreateSurface(void)
{
	return vkvCreateDisplaySurface(vk_instance, &vk_surface);
}

VkSurfaceCapabilitiesKHR Vkimp_SetupFullScreenExclusive(void)
{
	VkSurfaceCapabilitiesKHR caps;
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vk_device.physical, vk_surface, &caps);
	return caps;
}

void Vkimp_Shutdown(void)
{
	if (vkw_state.log_fp)
	{
		fclose(vkw_state.log_fp);
		vkw_state.log_fp = NULL;
	}
}

int Vkimp_Init(void *hinstance, void *wndproc)
{
	return true;
}

void Vkimp_BeginFrame(float camera_separation)
{
}

void Vkimp_EndFrame(void)
{
}

void Vkimp_AppActivate(qboolean active)
{
}
