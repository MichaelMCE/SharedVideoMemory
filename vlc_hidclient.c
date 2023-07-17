
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU LIBRARY GENERAL PUBLIC LICENSE
//  as published by the Free Software Foundation; either version 2
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU LIBRARY GENERAL PUBLIC LICENSE for details.



/*

Intended as a client for the VLC 2.0.x shared video out plugin libsvmem_plugin.dll

Start VLC before this:
VLC_CaptureServer.bat

or manually via:
vlc.exe --vout=svmem --svmem-width=854 --svmem-height=480 --svmem-chroma=RV16 "videofile.mp4"

*/

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <conio.h>
#include <inttypes.h>

#include "../libTeensyRawHid/libTeensyRawHid.h"
#include "plugin/svmem.h"

// display demensions 
static int DWIDTH = 0;
static int DHEIGHT = 0;

typedef struct {
	int width;
	int height;
	int bpp;
	int yOffset;
	void *frame;
	uint32_t frameAllocSize;
	HANDLE hMapFile;
	uint8_t *hMem;
}imagemap_t;


static teensyRawHidcxt_t ctx;
static rawhid_header_t desc;
static imagemap_t img;




static int openSharedMemory (imagemap_t *img, const char *name)
{
	img->hMapFile = OpenFileMapping(FILE_MAP_ALL_ACCESS, TRUE, VLC_SMEMNAME);
	if (img->hMapFile != NULL){
		img->hMem = (uint8_t*)MapViewOfFile(img->hMapFile, FILE_MAP_ALL_ACCESS, 0,0,0);
		if (img->hMem != NULL){
			//printf("MapViewOfFile(): Ok - 0x%p\n", (uintptr_t*)img->hMem);
			return 1;
		}else{
			//printf("MapViewOfFile() failed with error %d\n", (int)GetLastError());
			CloseHandle(img->hMapFile);
		}
	}else{
		//printf("OpenFileMapping() failed with error %d\n", (int)GetLastError());
	}
	return 0;
}

static void closeSharedMemory (imagemap_t *img)
{
	UnmapViewOfFile(img->hMem);
	CloseHandle(img->hMapFile);
}

static int openDisplayWait (const int timeMs)
{
	const int delay = 20;
	const int loops = timeMs/delay;
	
	for (int i = 0; i < loops; i++){
		Sleep(delay);
			
		if (libTeensyRawHid_OpenDisplay(&ctx))
			return 1;
	}
	return 0;
}

int display_init ()
{
	if (!libTeensyRawHid_OpenDisplay(&ctx)){
		if (!openDisplayWait(500))
			return 0;
	}

	libTeensyRawHid_GetConfig(&ctx, &desc);
		
	ctx.width = desc.u.cfg.width;
	ctx.height = desc.u.cfg.height;
	ctx.pitch = desc.u.cfg.pitch;
	ctx.rgbClamp = desc.u.cfg.rgbMax;
	
	DWIDTH = ctx.width;
	DHEIGHT = ctx.height;

	if (!DWIDTH || !DHEIGHT){
		libTeensyRawHid_Close(&ctx);
		return 0;
	}

	//printf("Display Width:%i Height:%i\n%s\n", DWIDTH, DHEIGHT, desc.u.cfg.string);
	printf("Found device: %s\n", desc.u.cfg.string);
	return 1;
}

static int updateDisplay (uint16_t *pixels, const int yOffset, const int stripHeight)
{
	int ret = 0;
	const int twrites = img.height / stripHeight;
	for (int i = 0; i < twrites; i++){
		int y = i * stripHeight;
		ret += libTeensyRawHid_WriteArea(&ctx, &pixels[y * img.width], 0, y+yOffset, img.width-1, yOffset+y+stripHeight-1);
	}

	const int remaining = img.height % stripHeight;
	if (remaining && ret){
		for (int i = twrites; i < twrites+1; i++){
			int y = i * stripHeight;
			libTeensyRawHid_WriteArea(&ctx, &pixels[y * img.width], 0, y+yOffset, img.width-1, yOffset+y+remaining-1);
		}
	}
	return (ret != 0);
}

int main (int argc, char* argv[])
{
	
	if (!display_init()){
		printf("Display not found or connection in use\n");
		return 0;
	}

	HANDLE hUpdateEvent = NULL;
	HANDLE hDataLock = NULL;
	TSVMEM *svmem = NULL;
	int gotMapHandle = 0;

	printf("Waiting for frame server..\n");
	
	while(!kbhit()){
		gotMapHandle = openSharedMemory(&img, VLC_SMEMNAME);
		if (gotMapHandle){
			printf("Connected\n");
			break;
		}
		Sleep(500);
	};

	if (gotMapHandle){
		svmem = (TSVMEM*)img.hMem;
		
		// VLC will set a global event, VLC_SMEMEVENT, on each update
		hUpdateEvent = CreateEvent(NULL, 0, 0, VLC_SMEMEVENT);
		
		// data access is synchronized through a semaphore 
		hDataLock = CreateSemaphore(NULL, 0, 1, VLC_SMEMLOCK);

		// playback may need to be restarted for any change to these values to take effect
		// tell VLC the resolution we need
		svmem->hdr.rwidth = DWIDTH;
		svmem->hdr.rheight = DHEIGHT;

		img.yOffset = 0;
		img.width = -1;
		img.height = -1;
		img.bpp = -1;
		img.frameAllocSize = sizeof(uint32_t) * DWIDTH * DHEIGHT;
		img.frame = calloc(1, img.frameAllocSize);
		if (!img.frame) abort();


		
		while(!kbhit()){
			// wait for the frame ready signal from the plugin
			if (WaitForSingleObject(hUpdateEvent, 500) == WAIT_OBJECT_0){
				// lock the IPC. 
				// this also blocks VLC from updating the buffer
				if (WaitForSingleObject(hDataLock, 1000) == WAIT_OBJECT_0){
					
					if (svmem->hdr.count < 2){
						memset(img.frame, 0, sizeof(uint32_t)*DWIDTH*DHEIGHT);
						libTeensyRawHid_WriteImage(&ctx, img.frame);
					}
					
					if (img.bpp != svmem->hdr.bpp){
						img.bpp = svmem->hdr.bpp;
						img.width = svmem->hdr.width;
						img.height = svmem->hdr.height;
						img.yOffset = abs(DHEIGHT - svmem->hdr.height) / 2;
						
						memset(img.frame, 0, img.frameAllocSize);
						libTeensyRawHid_WriteImage(&ctx, img.frame);

					}else if (img.width != svmem->hdr.width || img.height != svmem->hdr.height){
						img.yOffset = abs(DHEIGHT - svmem->hdr.height) / 2;
						img.width = svmem->hdr.width;
						img.height = svmem->hdr.height;

						memset((void*)&svmem->pixels, 0, svmem->hdr.vsize);
						memset(img.frame, 0, img.frameAllocSize);
						libTeensyRawHid_WriteImage(&ctx, img.frame);
					}

					if (svmem->hdr.ssize && svmem->hdr.fsize)
						memcpy(img.frame, (void*)&svmem->pixels, svmem->hdr.fsize);
					
					ReleaseSemaphore(hDataLock, 1, NULL);

					int devStatus = updateDisplay((uint16_t*)img.frame, img.yOffset, desc.u.cfg.stripHeight);	// stripHeight should match that of the display
					if (!devStatus) break;
				}
			}else{
				Sleep(25);
			}
		}
		
		if (img.frame)
			free(img.frame);
		if (hUpdateEvent)
			CloseHandle(hUpdateEvent);
		if (hDataLock)
			CloseHandle(hDataLock);
		closeSharedMemory(&img);	
	}


	libTeensyRawHid_Close(&ctx);

	return 1;
}
