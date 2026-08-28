#include <mach-o/dyld.h>
#include <os/log.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <substrate.h>
#include <sys/sysctl.h>
#include "general.h"

static uint32_t product_id_offset;
static struct podsgrant_settings *settings;

static unsigned int (*originalProductInfo)(void *, void *, void *, void *, void *);
static unsigned int patchedProductInfo(void *device, void *vendorSource, void *vendorID, void *productID, void *version) {
	if(device) {
		uint16_t patched=PGS_patchProductId(settings, *(uint32_t *)((uint8_t *)device+product_id_offset));
		if(patched)
			*(uint32_t *)((uint8_t *)device+product_id_offset)=patched;
	}
	return originalProductInfo(device, vendorSource, vendorID, productID, version);
}

static unsigned int (*originalAbility)(void *, unsigned int);
static unsigned int patchedAbility(void *device, unsigned int abilityID) {
	if(device) {
		uint16_t patched=PGS_patchProductId(settings, *(uint32_t *)((uint8_t *)device+product_id_offset));
		if(patched)
			*(uint32_t *)((uint8_t *)device+product_id_offset)=patched;
		if(*(uint32_t *)((uint8_t *)device+product_id_offset)==8206 && (abilityID==12 || abilityID==26))
			return 1;
	}
	return originalAbility(device, abilityID);
}

static void *(*originalRemoteVolumeSupport)(void *, BOOL);
static void *patchedRemoteVolumeSupport(void *object, BOOL supported) {
	(void)supported;
	return originalRemoteVolumeSupport(object, YES);
}

__attribute__((destructor))
static void PodsGrantTeardown(void) {
	PGS_freeSettings(settings);
}

__attribute__((constructor))
static void PodsGrantInitialize(void) {
	char executablePath[512]={0};
	uint32_t pathLength=sizeof(executablePath);
	if(_NSGetExecutablePath(executablePath, &pathLength)!=0 || strcmp(executablePath, "/usr/sbin/bluetoothd")!=0)
		return;

	settings=PGS_readSettings(0);
	if(!settings || !settings->is_tweak_enabled)
		return;

	char osVersion[16]={0};
	size_t osVersionLength=sizeof(osVersion)-1;
	if(sysctlbyname("kern.osproductversion", osVersion, &osVersionLength, NULL, 0)!=0)
		return;
	osVersion[osVersionLength]=0;
	PGS_global_os_ver=(unsigned char)atoi(osVersion);
	if(PGS_global_os_ver!=18) {
		os_log_error(OS_LOG_DEFAULT, "[PodsGrant] unsupported iOS major version");
		return;
	}

	uint64_t addresses[3]={0};
	if(!PGS_findAddresses(addresses, &product_id_offset))
		return;

	uint64_t slide=0;
	int foundBluetoothdImage=0;
	for(uint32_t index=0;index<_dyld_image_count();index++) {
		const char *imageName=_dyld_get_image_name(index);
		if(imageName && strcmp(imageName, "/usr/sbin/bluetoothd")==0) {
			slide=(uint64_t)_dyld_get_image_vmaddr_slide(index);
			foundBluetoothdImage=1;
			break;
		}
	}
	if(!foundBluetoothdImage) {
		os_log_error(OS_LOG_DEFAULT, "[PodsGrant] bluetoothd image slide was not found");
		return;
	}

	MSHookFunction((void *)(slide+addresses[0]), (void *)&patchedProductInfo, (void **)&originalProductInfo);
	MSHookFunction((void *)(slide+addresses[1]), (void *)&patchedAbility, (void **)&originalAbility);
	if(addresses[2])
		MSHookFunction((void *)(slide+addresses[2]), (void *)&patchedRemoteVolumeSupport, (void **)&originalRemoteVolumeSupport);
}
