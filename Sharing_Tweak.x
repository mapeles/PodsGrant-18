#import <Foundation/Foundation.h>
#include <mach-o/dyld.h>
#include "general.h"

static struct podsgrant_settings *settings;

%group SharingHooks

%hook SFBLEScanner

- (id)pairingParsePayload:(NSData *)payload identifier:(id)identifier bleDevice:(id)bleDevice peerInfo:(id)peerInfo {
	if(payload.length<7)
		return %orig;
	uint16_t original=*(const uint16_t *)((const uint8_t *)payload.bytes+5);
	uint16_t patched=PGS_patchProductId(settings, original);
	if(!patched)
		return %orig;
	NSMutableData *newPayload=[payload mutableCopy];
	*(uint16_t *)((uint8_t *)newPayload.mutableBytes+5)=patched;
	return %orig(newPayload, identifier, bleDevice, peerInfo);
}

- (id)pairingParsePayload:(NSData *)payload identifier:(id)identifier bleDevice:(id)bleDevice {
	if(payload.length<7)
		return %orig;
	uint16_t original=*(const uint16_t *)((const uint8_t *)payload.bytes+5);
	uint16_t patched=PGS_patchProductId(settings, original);
	if(!patched)
		return %orig;
	NSMutableData *newPayload=[payload mutableCopy];
	*(uint16_t *)((uint8_t *)newPayload.mutableBytes+5)=patched;
	return %orig(newPayload, identifier, bleDevice);
}

%end

%end

%dtor {
	PGS_freeSettings(settings);
}

%ctor {
	char executablePath[512]={0};
	uint32_t pathLength=sizeof(executablePath);
	if(_NSGetExecutablePath(executablePath, &pathLength)!=0 || strcmp(executablePath, "/usr/sbin/bluetoothd")==0)
		return;
	settings=PGS_readSettings(0);
	if(!settings || !settings->is_tweak_enabled)
		return;
	%init(SharingHooks);
}
