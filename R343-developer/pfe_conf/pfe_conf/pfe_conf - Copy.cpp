// pfe_conf.cpp : Defines the entry point for the console application.
//

#include <stdio.h>
#include <tchar.h>
#include <windows.h>
#include "nvapi.h"

static HANDLE g_file = NULL;
// -------------------------------------------------------------------------- 
//  ulog
//		A log callback function
//
static void log(const char* msg)
{
	printf("%s\n", msg);
	if(g_file == NULL)
	{

		char szFilename[512];
		SYSTEMTIME tm;
		::GetLocalTime(&tm);
		// Log in the executing folder
		_snprintf_s(szFilename, 256, 
				"./log-pfe-conf-%d_%02d_%02d_%02d_%02d_%02d.txt", 
				tm.wYear, tm.wMonth, tm.wDay, tm.wHour, tm.wMinute, tm.wSecond);

		g_file = CreateFileA(szFilename,
	                       	GENERIC_WRITE,
	                       	FILE_SHARE_READ,
	                       	NULL,
	                       	CREATE_ALWAYS,
	                       	FILE_ATTRIBUTE_NORMAL,
	                       	NULL);
	}

	char timeBuffer[64];
	SYSTEMTIME tm;
	::GetLocalTime(&tm);
	_snprintf_s(timeBuffer, 64, 
				"(%d-%02d-%02d-%02d:%02d:%02d.%03d) ", 
					tm.wYear, tm.wMonth, tm.wDay, 
					tm.wHour, tm.wMinute, tm.wSecond,
					tm.wMilliseconds);

	DWORD len = strlen(timeBuffer);
	WriteFile(g_file, timeBuffer, len, &len, NULL);
	len = strlen(msg);
	WriteFile(g_file, msg, len, &len, NULL);
	WriteFile(g_file, "\r\n", 2, &len, NULL);
}

// -------------------------------------------------------------------------- 
//  ulog
//		A log function that does the printf and logs to a local file
//
static void ulog(const char* fmt, ...)
{
	char buffer[4096];
	va_list args;
  	va_start( args, fmt );
  	vsnprintf_s(buffer, 4096, 4096, fmt, args);
  	va_end( args );

  	log(buffer);
}

void pfe_pause()
{
	ulog("Press any key to exit...");
	getchar();
}

// This function is used to do the NvAPI_DISP_GetDisplayConfig to get the current state of the system
NvAPI_Status AllocateAndGetDisplayConfig(NvU32* pathInfoCount, NV_DISPLAYCONFIG_PATH_INFO** pPathInfo)
{
    NvAPI_Status ret;

    // Retrieve the display path information
    NvU32 pathCount							= 0;
    NV_DISPLAYCONFIG_PATH_INFO *pathInfo	= NULL;

    ret = NvAPI_DISP_GetDisplayConfig(&pathCount, NULL);
    if (ret != NVAPI_OK)    return ret;

    pathInfo = (NV_DISPLAYCONFIG_PATH_INFO*) malloc(pathCount * sizeof(NV_DISPLAYCONFIG_PATH_INFO));
    if (!pathInfo)
    {
        return NVAPI_OUT_OF_MEMORY;
    }

    memset(pathInfo, 0, pathCount * sizeof(NV_DISPLAYCONFIG_PATH_INFO));
    for (NvU32 i = 0; i < pathCount; i++)
    {
        pathInfo[i].version = NV_DISPLAYCONFIG_PATH_INFO_VER;
    }

    // Retrieve the targetInfo counts
    ret = NvAPI_DISP_GetDisplayConfig(&pathCount, pathInfo);
    if (ret != NVAPI_OK)
    {
        return ret;
    }

    for (NvU32 i = 0; i < pathCount; i++)
    {
        // Allocate the source mode info
        pathInfo[i].sourceModeInfo = (NV_DISPLAYCONFIG_SOURCE_MODE_INFO*) malloc(sizeof(NV_DISPLAYCONFIG_SOURCE_MODE_INFO));
        if (pathInfo[i].sourceModeInfo == NULL)
        {
            return NVAPI_OUT_OF_MEMORY;
        }
        memset(pathInfo[i].sourceModeInfo, 0, sizeof(NV_DISPLAYCONFIG_SOURCE_MODE_INFO));

        // Allocate the target array
        pathInfo[i].targetInfo = (NV_DISPLAYCONFIG_PATH_TARGET_INFO*) malloc(pathInfo[i].targetInfoCount * sizeof(NV_DISPLAYCONFIG_PATH_TARGET_INFO));
        if (pathInfo[i].targetInfo == NULL)
        {
            return NVAPI_OUT_OF_MEMORY;
        }
        // Allocate the target details
        memset(pathInfo[i].targetInfo, 0, pathInfo[i].targetInfoCount * sizeof(NV_DISPLAYCONFIG_PATH_TARGET_INFO));
        for (NvU32 j = 0 ; j < pathInfo[i].targetInfoCount ; j++)
        {
            pathInfo[i].targetInfo[j].details = (NV_DISPLAYCONFIG_PATH_ADVANCED_TARGET_INFO*) malloc(sizeof(NV_DISPLAYCONFIG_PATH_ADVANCED_TARGET_INFO));    
            memset(pathInfo[i].targetInfo[j].details, 0, sizeof(NV_DISPLAYCONFIG_PATH_ADVANCED_TARGET_INFO));
            pathInfo[i].targetInfo[j].details->version = NV_DISPLAYCONFIG_PATH_ADVANCED_TARGET_INFO_VER;
        }
    }

    // Retrieve the full path info
    ret = NvAPI_DISP_GetDisplayConfig(&pathCount, pathInfo);
    if (ret != NVAPI_OK)    
    {
        return ret;
    }

    *pathInfoCount = pathCount;
    *pPathInfo = pathInfo;
    return NVAPI_OK;
}

// This function is used to display the current GPU configuration and the connected displays
void ShowCurrentDisplayConfig(void)
{
	NvAPI_Status ret						= NVAPI_OK;
	NV_DISPLAYCONFIG_PATH_INFO *pathInfo	= NULL;
	NvU32 pathCount							= 0;
	NV_DISPLAYCONFIG_PATH_INFO *pathInfo1	= NULL;
	NvU32 nDisplayIds						= 0;
    NvU32 physicalGpuCount					= 0;
    NV_GPU_DISPLAYIDS* pDisplayIds			= NULL;
    NvPhysicalGpuHandle hPhysicalGpu[NVAPI_MAX_PHYSICAL_GPUS];

	for (NvU32 PhysicalGpuIndex = 0; PhysicalGpuIndex < NVAPI_MAX_PHYSICAL_GPUS; PhysicalGpuIndex++)
    {
        hPhysicalGpu[PhysicalGpuIndex]=0;
    }

	ulog("The currently running display configuration is as follows:\n");
	for(NvU32 count = 0; count < 60; count++)	ulog("#");
	ulog("GPU index\tGPU ID\t\tDisplayIDs of displays\n");
	
    
        
    // Enumerate the physical GPU handle
    ret = NvAPI_EnumPhysicalGPUs(hPhysicalGpu, &physicalGpuCount);
	if(ret != NVAPI_OK)
	{
		ulog("Cannot enumerate GPUs in the system...\n");
		getchar();
		exit(1);
	}
    // get the display ids of connected displays
	NvU32 DisplayGpuIndex					= 0;

	for(NvU32 GpuIndex = 0; GpuIndex < physicalGpuCount; GpuIndex++)
	{
		ulog("getting gpu index %d/%d...\n", GpuIndex, physicalGpuCount);
		ret = NvAPI_GPU_GetConnectedDisplayIds(hPhysicalGpu[GpuIndex], pDisplayIds, &nDisplayIds, 0);
		ulog("getting %d displays ids...\n", nDisplayIds);
		if((ret == NVAPI_OK) && nDisplayIds)
		{
			DisplayGpuIndex					= GpuIndex;
			pDisplayIds						= (NV_GPU_DISPLAYIDS*)malloc(nDisplayIds * sizeof(NV_GPU_DISPLAYIDS));
			if (pDisplayIds)
			{
				memset(pDisplayIds, 0, nDisplayIds * sizeof(NV_GPU_DISPLAYIDS));
				pDisplayIds[GpuIndex].version		= NV_GPU_DISPLAYIDS_VER;
				ret = NvAPI_GPU_GetConnectedDisplayIds(hPhysicalGpu[DisplayGpuIndex], pDisplayIds, &nDisplayIds, 0);
				for(NvU32 DisplayIdIndex = 0; DisplayIdIndex < nDisplayIds; DisplayIdIndex++)
				{
					ulog("%2d\t\t0x%x\t0x%x\t%d", GpuIndex, hPhysicalGpu[DisplayGpuIndex], pDisplayIds[DisplayIdIndex].displayId, pDisplayIds[DisplayIdIndex].connectorType);
					if(!pDisplayIds[DisplayIdIndex].displayId)ulog("(NONE)");
				}
			}
		}
		else
			ulog("%2d\t\t0x%x\n", GpuIndex, hPhysicalGpu[GpuIndex]);
	}

	ret = AllocateAndGetDisplayConfig(&pathCount, &pathInfo);
    if (ret != NVAPI_OK)
	{
		ulog("AllocateAndGetDisplayConfig failed!\n");
		getchar();
		exit(1);
	}
	else
	{
		ulog("Got %d display configs.\n", pathCount);
	}

	if( pathCount == 1 )
    {
        if( pathInfo[0].targetInfoCount == 1 ) // if pathCount = 1 and targetInfoCount =1 it is Single Mode
            ulog("Single MODE\n");
        else if( pathInfo[0].targetInfoCount > 1) // if pathCount >= 1 and targetInfoCount >1 it is Clone Mode
            ulog("Monitors in Clone MODE\n");
    }
    else
    {
        for (NvU32 PathIndex = 0; PathIndex < pathCount; PathIndex++)
        {   
            if(pathInfo[PathIndex].targetInfoCount == 1)
            {
                ulog("Monitor with Display Id 0x%x is in Extended MODE\n",pathInfo[PathIndex].targetInfo->displayId); 
                // if pathCount > 1 and targetInfoCount =1 it is Extended Mode


                NV_DISPLAYCONFIG_SOURCE_MODE_INFO* sourceModeInfo = pathInfo[PathIndex].sourceModeInfo;

                ulog("resolution(%dx%d in depth %d)\t"
                		"color format(%d)\t"
                		"position(%d, %d)\t"
                		"orientation(%d)\t"
                		"is primary(%d)\t"
                		"SLIFocus(%d)\n\n",
                		sourceModeInfo->resolution.width, sourceModeInfo->resolution.height, sourceModeInfo->resolution.colorDepth,
                		sourceModeInfo->colorFormat,
                		sourceModeInfo->position.x, sourceModeInfo->position.y,
                		sourceModeInfo->spanningOrientation,
                		sourceModeInfo->bGDIPrimary,
                		sourceModeInfo->bSLIFocus);

                NV_MONITOR_CAPABILITIES monitorCaps;
                if((ret = NvAPI_DISP_GetMonitorCapabilities(/*pathInfo[PathIndex].targetInfo->displayId*/0, &monitorCaps)) == NVAPI_OK)
                {
                	ulog("monitor version(%d), size(%d), infoType(%d), connectorType(%d)\n", monitorCaps.version, (NvU32)monitorCaps.size, monitorCaps.infoType, monitorCaps.connectorType);
                }
                else
                {
                	ulog("Failed to get monitor capabilities err = %d\n", ret);
                }
            }
            else if( pathInfo[PathIndex].targetInfoCount > 1)
            {
                for (NvU32 TargetIndex = 0; TargetIndex < pathInfo[PathIndex].targetInfoCount; TargetIndex++)
                {
					// if pathCount >= 1 and targetInfoCount > 1 it is Clone Mode
					ulog("Monitors with Display Id 0x%x are in Clone MODE\n",pathInfo[PathIndex].targetInfo[TargetIndex].displayId);
                }
            }
        }
    }
}

void makeEdid(NV_EDID* p_nvEdid)
{
	NV_EDID loadEdidNv;
	memset(&loadEdidNv, 0, sizeof(NV_EDID));
	p_nvEdid->version = NV_EDID_VER;
	p_nvEdid->edidId = 0;
	p_nvEdid->offset = 0;
	p_nvEdid->sizeofEDID = 256;

	NvU8 EdidData[NV_EDID_DATA_SIZE]= { 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00,
										0x4c, 0x2d, 0x64, 0x08, 0x00, 0x00, 0x00, 0x00,
										0x13, 0x15, 0x01, 0x04, 0xa5, 0x35, 0x1e, 0x78,
										0x22, 0xef, 0xe1, 0xa3, 0x55, 0x55, 0x9f, 0x27,
										0x0c, 0x50, 0x54, 0xbf, 0xef, 0x80, 0x71, 0x4f,
										0x81, 0x00, 0x81, 0x40, 0x81, 0x80, 0x95, 0x00,
										0x95, 0x0f, 0xa9, 0x40, 0xb3, 0x00, 0x02, 0x3a, 
										0x80, 0x18, 0x71, 0x38, 0x2d, 0x40, 0x58, 0x2c,
										0x45, 0x00, 0x13, 0x2b, 0x21, 0x00, 0x00, 0x1e,
										0x01, 0x1d, 0x00, 0x72, 0x51, 0xd0, 0x1e, 0x20,
										0x6e, 0x28, 0x55, 0x00, 0x13, 0x2b, 0x21, 0x00,
										0x00, 0x1e, 0x00, 0x00, 0x00, 0xfd, 0x00, 0x38,
										0x4b, 0x1e, 0x51, 0x11, 0x00, 0x0a, 0x20, 0x20,
										0x20, 0x20, 0x20, 0x20, 0x00, 0x00, 0x00, 0xfc,
										0x00, 0x53, 0x4d, 0x53, 0x32, 0x34, 0x41, 0x36,
										0x35, 0x30, 0x0a, 0x20, 0x20, 0x20, 0x01, 0xdc,
										0x02, 0x03, 0x13, 0xc1, 0x46, 0x90, 0x1f, 0x04,
										0x13, 0x03, 0x12, 0x23, 0x09, 0x07, 0x07, 0x83,
										0x01, 0x00, 0x00, 0x01, 0x1d, 0x00, 0xbc, 0x52,
										0xd0, 0x1e, 0x20, 0xb8, 0x28, 0x55, 0x40, 0x13,
										0x2b, 0x21, 0x00, 0x00, 0x1e, 0x8c, 0x0a, 0xd0,
										0x90, 0x20, 0x40, 0x31, 0x20, 0x0c, 0x40, 0x55,
										0x00, 0x13, 0x2b, 0x21, 0x00, 0x00, 0x18, 0x8c,
										0x0a, 0xd0, 0x8a, 0x20, 0xe0, 0x2d, 0x10, 0x10,
										0x3e, 0x96, 0x00, 0x13, 0x2b, 0x21, 0x00, 0x00,
										0x18, 0x02, 0x3a, 0x80, 0xd0, 0x72, 0x38, 0x2d,
										0x40, 0x10, 0x2c, 0x45, 0x80, 0x13, 0x2b, 0x21,
										0x00, 0x00, 0x1e, 0x00, 0x00, 0x00, 0x00, 0x00,
										0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
										0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
										0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
										0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xb4};

	memcpy(&p_nvEdid->EDID_Data, &EdidData[0], sizeof(EdidData));
}

void printDebugInfo()
{
    DWORD           dispIdx = 0;
    DWORD           devmodeIdx = 0;
    DWORD           deviceFlag = EDD_GET_DEVICE_INTERFACE_NAME;
    DISPLAY_DEVICEA  displayDevice;
    DEVMODEA         devmode;

    // initialize displayDevice
    ZeroMemory(&displayDevice, sizeof(displayDevice));
    displayDevice.cb = sizeof(displayDevice);

    // get all display devices
    while(EnumDisplayDevicesA(NULL, dispIdx, &displayDevice, deviceFlag))
    {
        // Print the device infor
        ulog("Display Device (%u), Name: (%s), Device String: (%s), state flag: (%u)", 
                                                dispIdx, 
                                                displayDevice.DeviceName,
                                                displayDevice.DeviceString,
                                                displayDevice.StateFlags);

        // Additional info
        if ( displayDevice.StateFlags & DISPLAY_DEVICE_ATTACHED_TO_DESKTOP )
            ulog("State: (DISPLAY_DEVICE_ATTACHED_TO_DESKTOP)");

        if ( displayDevice.StateFlags & DISPLAY_DEVICE_ACTIVE )
            ulog("State: (DISPLAY_DEVICE_ACTIVE)");

        if ( displayDevice.StateFlags & DISPLAY_DEVICE_MIRRORING_DRIVER )
            ulog("State: (DISPLAY_DEVICE_MIRRORING_DRIVER)");

        if ( displayDevice.StateFlags & DISPLAY_DEVICE_MODESPRUNED )
            ulog("State: (DISPLAY_DEVICE_MODESPRUNED)");

        if ( displayDevice.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE )
            ulog("State: (DISPLAY_DEVICE_PRIMARY_DEVICE)");

        if ( displayDevice.StateFlags & DISPLAY_DEVICE_REMOVABLE )
            ulog("State: (DISPLAY_DEVICE_REMOVABLE)");

        if ( displayDevice.StateFlags & DISPLAY_DEVICE_VGA_COMPATIBLE )
            ulog("State: (DISPLAY_DEVICE_VGA_COMPATIBLE)");

        // Check for primary display only as this is a single display test  
        if ((displayDevice.StateFlags & DISPLAY_DEVICE_ACTIVE)
            && (displayDevice.StateFlags & DISPLAY_DEVICE_ATTACHED_TO_DESKTOP))
        {
            ulog("Getting current devmode for %s...", 
                                                displayDevice.DeviceName);

            devmode.dmSize = sizeof(DEVMODEA);
            devmode.dmDriverExtra = 0;
            if(EnumDisplaySettingsA(
                    displayDevice.DeviceName,
                    ENUM_CURRENT_SETTINGS,
                    &devmode))
            {
                ulog("%s devmode: (%u)x(%u), bpp: (%u)",
                                            displayDevice.DeviceName,
                                            devmode.dmPelsWidth,
                                            devmode.dmPelsHeight,
                                            devmode.dmBitsPerPel);
            }
            else
            {
                ulog("Failed to get current devmode, err = %d", GetLastError());
            }
        }

        {
        	UINT32 mode_idx = 0;
        	DEVMODEA mode_info = {0};
        	while(EnumDisplaySettingsA(displayDevice.DeviceName, mode_idx, &mode_info))
        	{
        		ulog("%s devmode[%u]: (%u)x(%u), bpp: (%u)",
                                            displayDevice.DeviceName,
                                            mode_idx,
                                            mode_info.dmPelsWidth,
                                            mode_info.dmPelsHeight,
                                            mode_info.dmBitsPerPel);
        		mode_idx++;
        	}
        }

		ulog("//////////////////////////////////////////////////////");

        ZeroMemory(&displayDevice, sizeof(displayDevice));
        displayDevice.cb = sizeof(displayDevice);
        dispIdx++;
    } // end while for all display devices
}

void detachNonNvdiaDisplays()
{
	DWORD           dispIdx = 0;
    DWORD           devmodeIdx = 0;
    DWORD           deviceFlag = EDD_GET_DEVICE_INTERFACE_NAME;
    DISPLAY_DEVICEA  displayDevice;
    DEVMODEA         devmode;
    DWORD dwRet = DISP_CHANGE_FAILED;

    // initialize displayDevice
    ZeroMemory(&displayDevice, sizeof(displayDevice));
    displayDevice.cb = sizeof(displayDevice);

    // get all display devices
    while(EnumDisplayDevicesA(NULL, dispIdx, &displayDevice, deviceFlag))
    {
        // Print the device infor
        ulog("Display Device (%u), Name: (%s), Device String: (%s), state flag: (%u)", 
                                                dispIdx, 
                                                displayDevice.DeviceName,
                                                displayDevice.DeviceString,
                                                displayDevice.StateFlags);


        // Check for primary display only as this is a single display test  
        if ((displayDevice.StateFlags & DISPLAY_DEVICE_ACTIVE)
            && (displayDevice.StateFlags & DISPLAY_DEVICE_ATTACHED_TO_DESKTOP)
            && strstr(displayDevice.DeviceString, "NVIDIA") == NULL)
        {
            ulog("Getting current devmode for %s...", 
                                                displayDevice.DeviceName);

            devmode.dmSize = sizeof(DEVMODEA);
            devmode.dmDriverExtra = 0;
            if(EnumDisplaySettingsA(
                    displayDevice.DeviceName,
                    ENUM_CURRENT_SETTINGS,
                    &devmode))
            {
                ulog("%s devmode: (%u)x(%u), bpp: (%u)",
                                            displayDevice.DeviceName,
                                            devmode.dmPelsWidth,
                                            devmode.dmPelsHeight,
                                            devmode.dmBitsPerPel);

                devmode.dmPelsWidth = 0;
				devmode.dmPelsHeight = 0;
                DWORD dwFlag = CDS_UPDATEREGISTRY | CDS_NORESET;
    			dwRet = ChangeDisplaySettingsExA(displayDevice.DeviceName,
    													&devmode,
                                        				NULL,
                                        				dwFlag,
                                        				NULL);

    			if(dwRet != DISP_CHANGE_SUCCESSFUL)
    			{
    				ulog("Failed to detach this display, err = %d", GetLastError());
    				break;
    			}
            }
            else
            {
                ulog("Failed to get current devmode, err = %d", GetLastError());
            }
        }

        ZeroMemory(&displayDevice, sizeof(displayDevice));
        displayDevice.cb = sizeof(displayDevice);
        dispIdx++;
    } // end while for all display devices

    if(dwRet == DISP_CHANGE_SUCCESSFUL)
    {
    	dwRet = ChangeDisplaySettingsEx (NULL, NULL, NULL, NULL, NULL);
        if ( dwRet == DISP_CHANGE_SUCCESSFUL )
        {
            ulog("Applied display setting changes.");
        }
        else
        {
            ulog("Failed to apply display setting err = %d", GetLastError());
        }
    }
}

int main1()
{
	NvAPI_Status ret = NVAPI_OK;
	ret = NvAPI_Initialize();
	if (ret != NVAPI_OK)
	{
		ulog("NvAPI_Initialize() failed = 0x%x", ret);
		pfe_pause();
		return 1; // Initialization failed
	}


	ulog("attached displays...\n");
	NvU32 num = 0;
	NvDisplayHandle display_handle;
	while (NvAPI_EnumNvidiaDisplayHandle(num, &display_handle) == NVAPI_OK)
	{
		NvAPI_ShortString display_name;
		if (NvAPI_GetAssociatedNvidiaDisplayName(display_handle, display_name) == NVAPI_OK)
		{
			ulog("display: %s\t", display_name);
		}
		else
		{
			ulog("Failed to get display name for attached display device %d", num);
		}

		NvU32 display_output_id;
		if (NvAPI_GetAssociatedDisplayOutputId(display_handle, &display_output_id) == NVAPI_OK)
		{
			ulog("output id: 0x%x\n", display_output_id);
		}
		else
		{
			ulog("Failed to get display name for attached display device %d", num);
		}

		num++;
	}


	ulog("unattached displays...\n");
	NvUnAttachedDisplayHandle unattached_display_handle;
	num = 0;
	while (NvAPI_EnumNvidiaUnAttachedDisplayHandle(num, &unattached_display_handle) == NVAPI_OK)
	{
		NvAPI_ShortString display_name;
		if(NvAPI_GetUnAttachedAssociatedDisplayName(unattached_display_handle, display_name) == NVAPI_OK)
		{
			ulog("%s\n", display_name);
		}
		else
		{
			ulog("Failed to get display name for unattached display device %d", num);
		}

		num++;
	}

	//ShowCurrentDisplayConfig();


	{
		NvAPI_Status nvStatus;
		NvPhysicalGpuHandle nvGPUHandles[NVAPI_MAX_PHYSICAL_GPUS];
		memset(nvGPUHandles, 0, sizeof(NvPhysicalGpuHandle));
		NvU32 nvGpuCount = 0;
		if ((NvAPI_EnumPhysicalGPUs(nvGPUHandles, &nvGpuCount) == NVAPI_OK) && nvGpuCount > 0) 
		{
			// we got some NVIDIA GPUs...
			NV_EDID anEdidNv;
			memset(&anEdidNv, 0, sizeof(NV_EDID));
			anEdidNv.version = NV_EDID_VER;

			for (size_t h = 0; h < nvGpuCount; h++) 
			{
				NvU32 displayCnt = 0;
				nvStatus = NvAPI_GPU_GetAllDisplayIds(nvGPUHandles[h], NULL, &displayCnt);
				if (nvStatus == NVAPI_OK)
				{
					ulog("%u displays for GPU %u", displayCnt, h);

					NV_GPU_DISPLAYIDS* pDisplayIds = NULL;

					pDisplayIds	= (NV_GPU_DISPLAYIDS*)malloc(displayCnt * sizeof(NV_GPU_DISPLAYIDS));
					memset(pDisplayIds, 0, displayCnt * sizeof(NV_GPU_DISPLAYIDS));

					for (unsigned int i = 0; i < displayCnt; ++i)
					{
						pDisplayIds[i].version = NV_GPU_DISPLAYIDS_VER;
					}

					nvStatus = NvAPI_GPU_GetAllDisplayIds(nvGPUHandles[h], pDisplayIds, &displayCnt);
					if(nvStatus == NVAPI_OK)
					{
						for (unsigned int i = 0; i < displayCnt; ++i)
						{
							ulog("GPU[%u] display[0x%x]: OSVisible[%s], connected[%s], active[%s]", 
									h, pDisplayIds[i].displayId, pDisplayIds[i].isOSVisible?"YES":"NO", pDisplayIds[i].isConnected?"YES":"NO", pDisplayIds[i].isActive?"YES":"NO");

							NV_EDID loadEdid;
							makeEdid(&loadEdid);
							nvStatus = NvAPI_GPU_GetEDID(nvGPUHandles[h], pDisplayIds[i].displayId, &anEdidNv);
							if (nvStatus != NVAPI_OK)
							{
								ulog("Failed to get EDID for display 0x%x GPU %u", pDisplayIds[i].displayId, h);

								if (pDisplayIds[i].isOSVisible && !pDisplayIds[i].isConnected && !pDisplayIds[i].isActive)
								{
									NV_EDID loadEdid;
									makeEdid(&loadEdid);
									nvStatus = NvAPI_GPU_SetEDID(nvGPUHandles[h], pDisplayIds[i].displayId, &loadEdid);
									if(nvStatus != NVAPI_OK)
									{
										ulog("Failed to set EDID for display 0x%x GPU %u", pDisplayIds[i].displayId, h);
									}
									else
									{

									}
								}
							}
							else
							{
								ulog("Got EDID (id = %u, version = 0x%x, size = %u, offset = %u) for display 0x%x (%sactive) GPU %u\n", 
									anEdidNv.edidId, anEdidNv.version, anEdidNv.sizeofEDID, anEdidNv.offset, pDisplayIds[i].displayId, pDisplayIds[i].isActive?"":"in", h);

								for(unsigned int j = 0; j < anEdidNv.sizeofEDID; j++)
								{
									printf("%.2x ", anEdidNv.EDID_Data[j]&0xff);
								}
							}
						}
					}

					free(pDisplayIds);
				}
				else
				{
					ulog("Failed to get number of displays for GPU %u", h);
					continue;
				}
			}
		}
	}

	bool hasAttachedDisplay = false;
	ulog("rechecking attached displays...\n");
	num = 0;
	while (NvAPI_EnumNvidiaDisplayHandle(num, &display_handle) == NVAPI_OK)
	{
		NvAPI_ShortString display_name;
		if (NvAPI_GetAssociatedNvidiaDisplayName(display_handle, display_name) == NVAPI_OK)
		{
			ulog("display: %s\t", display_name);
		}
		else
		{
			ulog("Failed to get display name for attached display device %d", num);
		}

		NvU32 display_output_id;
		if (NvAPI_GetAssociatedDisplayOutputId(display_handle, &display_output_id) == NVAPI_OK)
		{
			ulog("output id: 0x%x\n", display_output_id);
		}
		else
		{
			ulog("Failed to get display name for attached display device %d", num);
		}

		hasAttachedDisplay = true;

		num++;
	}

	if(hasAttachedDisplay)
	{
		ulog("NVIDIA display attached, detach other displays...\n");

		printDebugInfo();

		detachNonNvdiaDisplays();
	}


	ulog("Display Configuration Successful!\n");
	pfe_pause();
	return 0;
}

void loadCustomDisplay(NV_CUSTOM_DISPLAY *cd)
{
	cd->version = NV_CUSTOM_DISPLAY_VER;
	cd->width = 999;
	cd->height = 999;
	cd->depth = 32;
	cd->colorFormat = NV_FORMAT_A8R8G8B8;
	cd->srcPartition.x = 0;
	cd->srcPartition.y = 0;
	cd->srcPartition.w = 1;
	cd->srcPartition.h = 1;
	cd->xRatio = 1;
	cd->yRatio = 1;
}

NvAPI_Status GetConnectedDisplays(NvU32 *displayIds, NvU32 *noDisplays)
{
	NvAPI_Status ret = NVAPI_OK;

	NvPhysicalGpuHandle nvGPUHandle[NVAPI_MAX_PHYSICAL_GPUS] = { 0 };
	NvU32 gpuCount = 0;
	NvU32 noDisplay = 0;

	// Get all the Physical GPU Handles
	ret = NvAPI_EnumPhysicalGPUs(nvGPUHandle, &gpuCount);
	if (ret != NVAPI_OK)
	{
		return ret;
	}

	for (NvU32 Count = 0; Count < gpuCount; Count++) // iterating per Physical GPU Handle call
	{
		NvU32 dispIdCount = 0;
		// First call to get the no. of displays connected by passing NULL
		if (NvAPI_GPU_GetConnectedDisplayIds(nvGPUHandle[Count], NULL, &dispIdCount, 0) != NVAPI_OK)
		{
			return NVAPI_ERROR;
		}

		if (dispIdCount > 0) // If no. of displays connected > 0 we can proceed to check if its active
		{
			// alocations for the display ids
			NV_GPU_DISPLAYIDS *dispIds = (NV_GPU_DISPLAYIDS *)malloc(sizeof(NV_GPU_DISPLAYIDS)*dispIdCount);

			for (NvU32 dispIndex = 0; dispIndex < dispIdCount; dispIndex++)
				dispIds[dispIndex].version = NV_GPU_DISPLAYIDS_VER; // adding the correct version information

			// second call to get the display ids
			if (NvAPI_GPU_GetConnectedDisplayIds(nvGPUHandle[Count], dispIds, &dispIdCount, 0) != NVAPI_OK)
			{
				return NVAPI_ERROR;
			}

			for (NvU32 dispIndex = 0; dispIndex < dispIdCount; dispIndex++)
			{
				displayIds[noDisplay] = dispIds[dispIndex].displayId;
				noDisplay++;
			}
		}
	}

	*noDisplays = noDisplay;

	return ret;
}

NvAPI_Status ApplyCustomDisplay()
{
	NvAPI_Status ret = NVAPI_OK;
	NvU32 noDisplays = 0;

	NvDisplayHandle hNvDisplay[NVAPI_MAX_DISPLAYS] = { 0 };

	NvU32 displayIds[NVAPI_MAX_DISPLAYS] = { 0 };

	ret = GetConnectedDisplays(&displayIds[0], &noDisplays);
	if (ret != NVAPI_OK)
	{
		printf("\nCall to GetConnectedDisplays() failed");
		return ret;
	}

	printf("\nNumber of Displays in the system = %2d", noDisplays);

	NV_CUSTOM_DISPLAY cd[NVAPI_MAX_DISPLAYS] = { 0 };

	float rr = 60;

	//timing computation (to get timing that suits the changes made)
	NV_TIMING_FLAG flag = { 0 };

	NV_TIMING_INPUT timing = { 0 };

	timing.version = NV_TIMING_INPUT_VER;

	for (NvU32 count = 0; count < noDisplays; count++)
	{
		//Load the NV_CUSTOM_DISPLAY structure with data from XML file
		loadCustomDisplay(&cd[count]);

		timing.height = cd[count].height;
		timing.width = cd[count].width;
		timing.rr = rr;

		timing.flag = flag;
		timing.type = NV_TIMING_OVERRIDE_AUTO;

		ret = NvAPI_DISP_GetTiming(displayIds[0], &timing, &cd[count].timing);

		if (ret != NVAPI_OK)
		{
			printf("NvAPI_DISP_GetTiming() failed = %d\n", ret);		//failed to get custom display timing
			return ret;
		}
	}

	printf("\nCustom Timing to be tried: ");
	printf("%d X %d @ %0.2f hz", cd[0].width, cd[0].height, rr);

	printf("\nNvAPI_DISP_TryCustomDisplay()");

	ret = NvAPI_DISP_TryCustomDisplay(&displayIds[0], noDisplays, &cd[0]); // trying to set custom display
	if (ret != NVAPI_OK)
	{
		printf("NvAPI_DISP_TryCustomDisplay() failed = %d", ret);		//failed to set custom display
		return ret;
	}
	else
		printf(".....Success!\n");
	Sleep(5000);

	printf("NvAPI_DISP_SaveCustomDisplay()");

	ret = NvAPI_DISP_SaveCustomDisplay(&displayIds[0], noDisplays, true, true);
	if (ret != NVAPI_OK)
	{
		printf("NvAPI_DISP_SaveCustomDisplay() failed = %d", ret);		//failed to save custom display
		return ret;
	}
	else
		printf(".....Success!\n");

	Sleep(5000);

	printf("NvAPI_DISP_RevertCustomDisplayTrial()");

	// Revert the new custom display settings tried.
	ret = NvAPI_DISP_RevertCustomDisplayTrial(&displayIds[0], 1);
	if (ret != NVAPI_OK)
	{
		printf("NvAPI_DISP_RevertCustomDisplayTrial() failed = %d", ret);		//failed to revert custom display trail
		return ret;
	}
	else
	{
		printf(".....Success!");
		Sleep(5000);

	}

	return ret;	// Custom Display.
}

int main2()
{
	NvAPI_Status ret = NVAPI_OK;

	ret = NvAPI_Initialize();
	if (ret != NVAPI_OK)
	{
		printf("NvAPI_Initialize() failed = 0x%x", ret);
		return 1; // Initialization failed
	}
	for (NvU32 q = 0; q < 50; q++)printf("/");
	printf("\n");

	ret = ApplyCustomDisplay();
	if (ret != NVAPI_OK)
	{
		getchar();
		return 1; // Failed to apply custom display
	}
	printf("\n");

	for (NvU32 q = 0; q < 50; q++)printf("/");
	printf("\n");

	printf("\nCustom_Timing successful!\nPress any key to exit...\n");
	getchar();
	return 0;
}

int main()
{
	return main1();
}
