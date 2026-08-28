#include "general.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mach-o/loader.h>
#include <os/log.h>
#include <unistd.h>

// This fork only supports iOS 18. Sharing processes do not run the bluetoothd
// constructor that normally populates this value, so use the supported major
// version as the safe default in every injected process.
unsigned char PGS_global_os_ver=18;

int PGS_saveSettings(struct podsgrant_settings *configuration) {
	if(!configuration)
		return -1;
	const char *temporary_path=PGS_SETTINGS_FILE ".tmp";
	FILE *config_file=fopen(temporary_path, "wb");
	if(!config_file)
		return -1;
	int failed=0;
	failed|=fputc(configuration->is_tweak_enabled, config_file)==EOF;
	failed|=fputc(configuration->product_id_mapping_cnt, config_file)==EOF;
	failed|=fwrite(configuration->product_id_mapping, sizeof(struct product_id_map_entry_custom), configuration->product_id_mapping_cnt, config_file)!=configuration->product_id_mapping_cnt;
	failed|=fputc(configuration->address_mapping_cnt, config_file)==EOF;
	failed|=fwrite(configuration->address_mapping, sizeof(struct address_map_entry), configuration->address_mapping_cnt, config_file)!=configuration->address_mapping_cnt;
	failed|=fflush(config_file)!=0;
	failed|=fsync(fileno(config_file))!=0;
	failed|=fclose(config_file)!=0;
	if(failed || rename(temporary_path, PGS_SETTINGS_FILE)!=0) {
		remove(temporary_path);
		return -2;
	}
	return 0;
}

struct podsgrant_settings *PGS_readSettings_to(struct podsgrant_settings *configuration, int read_full_anyway) {
	if(!configuration)
		return NULL;
	configuration->is_managed_structure=0;
	configuration->is_tweak_enabled=0;
	configuration->product_id_mapping=NULL;
	configuration->address_mapping=NULL;
	configuration->product_id_mapping_cnt=0;
	configuration->address_mapping_cnt=0;
	FILE *config_file=fopen(PGS_SETTINGS_FILE, "rb");
	if(!config_file) {
		configuration->is_tweak_enabled=1;
		return configuration;
	}
	int enabled=fgetc(config_file);
	if(enabled==EOF)
		goto invalid_settings;
	configuration->is_tweak_enabled=(uint8_t)enabled;
	if(!configuration->is_tweak_enabled&&!read_full_anyway) {
		fclose(config_file);
		return configuration;
	}
	int product_count=fgetc(config_file);
	if(product_count==EOF)
		goto invalid_settings;
	uint8_t product_id_mapping_entries=(uint8_t)product_count;
	configuration->product_id_mapping_cnt=product_id_mapping_entries;
	if(product_id_mapping_entries) {
		configuration->product_id_mapping=calloc(product_id_mapping_entries, sizeof(struct product_id_map_entry_custom));
		if(!configuration->product_id_mapping || fread(configuration->product_id_mapping, sizeof(struct product_id_map_entry_custom), product_id_mapping_entries, config_file)!=product_id_mapping_entries)
			goto invalid_settings;
	}
	int address_count=fgetc(config_file);
	if(address_count==EOF)
		goto invalid_settings;
	uint8_t addr_mapping_entries=(uint8_t)address_count;
	configuration->address_mapping_cnt=addr_mapping_entries;
	if(addr_mapping_entries) {
		configuration->address_mapping=calloc(addr_mapping_entries, sizeof(struct address_map_entry));
		if(!configuration->address_mapping || fread(configuration->address_mapping, sizeof(struct address_map_entry), addr_mapping_entries, config_file)!=addr_mapping_entries)
			goto invalid_settings;
	}
	fclose(config_file);
	return configuration;

invalid_settings:
	fclose(config_file);
	free(configuration->product_id_mapping);
	free(configuration->address_mapping);
	configuration->product_id_mapping=NULL;
	configuration->address_mapping=NULL;
	configuration->product_id_mapping_cnt=0;
	configuration->address_mapping_cnt=0;
	configuration->is_tweak_enabled=0;
	remove(PGS_SETTINGS_FILE);
	return configuration;
}

struct podsgrant_settings *PGS_readSettings(int read_full_anyway) {
	struct podsgrant_settings *configuration=calloc(1, sizeof(struct podsgrant_settings));
	if(!configuration)
		return NULL;
	if(!PGS_readSettings_to(configuration, read_full_anyway)) {
		free(configuration);
		return NULL;
	}
	configuration->is_managed_structure=1;
	return configuration;
}

void PGS_freeSettings(struct podsgrant_settings *conf) {
	if(!conf)
		return;
	if(conf->product_id_mapping)free(conf->product_id_mapping);
	if(conf->address_mapping)free(conf->address_mapping);
	if(conf->is_managed_structure)
		free(conf);
	return;
}

uint16_t PGS_patchProductId(struct podsgrant_settings *conf, uint16_t original) {
	if(conf) {
		for(uint8_t index=0;index<conf->product_id_mapping_cnt;index++) {
			struct product_id_map_entry_custom *entry=&conf->product_id_mapping[index];
			if(entry->original==original) {
				return entry->target;
			}
		}
	}
	for(const struct product_id_map_entry *entry=product_id_map_preset;;entry++) {
		if(!entry->original)
			break;
		if(PGS_global_os_ver>entry->maximum_ios||PGS_global_os_ver<entry->minimum_ios)
			continue;
		if(entry->original==original) {
			return entry->target;
		}
	}
	return 0;
}

static const uint32_t match_arr_1[]={
	// iOS 18.7.1: copy the four product fields from the device object.
	0xffffffff, 0xaa0003e8, // MOV X8, X0
	0xffffffff, 0x3950b000, // LDRB W0, [X0, #0x42c]
	0xffffffff, 0x7100041f, // CMP W0, #1
	0xff00001f, 0x54000001, // B.NE *
	0xffffffff, 0xb9443109, // LDR W9, [X8, #0x430]
	0xffffffff, 0xb9000029, // STR W9, [X1]
	0xffffffff, 0xb9443509, // LDR W9, [X8, #0x434]
	0xffffffff, 0xb9000049, // STR W9, [X2]
	0xffffffff, 0xb9443909, // LDR W9, [X8, #0x438]
	0xffffffff, 0xb9000069, // STR W9, [X3]
	0xffffffff, 0xb9443d08, // LDR W8, [X8, #0x43c]
	0xffffffff, 0xb9000088, // STR W8, [X4]
	0xffffffff, 0xd65f03c0, // RET
};

static const uint32_t match_arr_2[]={
	// LDR W*, [ X0, * ]
	0xffc003e0, 0xb9400000,
	// CMP W*, #0x4C
	0xfffffc1f, 0x7101301f,
	// B.NE *
	0xff00001f, 0x54000001,
	// LDR W*, [ X*, * ]
	0xffc00000, 0xb9400000,
	// MOV W*, #0xFFFFDFFE
	0xffffffe0, 0x12840020,
	// ADD W*, W*, W*
	0xff000000, 0x0b000000,
	// iOS 18.7.1 expands the product range through 0x1d.
	0xfffffc1f, 0x7100741f,
	// B.HI * distinguishes the actual ability function from an inlined copy.
	0xff00001f, 0x54000008,
};

static const uint32_t match_arr_3[]={
	// ADRP X*, *
	0x9f000000, 0x90000000,
	// LDR X*, [ X*, * ]
	0xffc00000, 0xf9400000,
	// ADRP X*, *
	0x9f000000, 0x90000000,
	// LDR X*, [ X*, * ]
	0xffc00000, 0xf9400000,
	// CMP W1, #0
	0xffffffff, 0x7100003f,
	// CSEL X*, X*, X*, NE
	0xffe0fc00, 0x9a801000,
	// LDR X2, [ X* ]
	0xfffffc1f, 0xf9400002,
	// ADRP X1, *
	0x9f00001f, 0x90000001,
	// ADD X1, X1, *
	0xff8003ff, 0x91000021,
	// B *
	0xfc000000, 0x14000000
};

static int match_instructions(uint32_t *out, size_t osz, const uint32_t *target, size_t tsz, FILE *bin) {
	fseek(bin,0,SEEK_SET);
	int total_cnt=0;
	size_t match_cnt=0;
	unsigned int val;
	while((fread(&val,1,4,bin))==4) {
		if((target[match_cnt*2]&val)==target[match_cnt*2+1]) {
			match_cnt++;
			if(match_cnt==tsz/8) {
				out[total_cnt]=ftell(bin)-tsz/2;
				total_cnt++;
				if((size_t)total_cnt==osz)
					break;
				match_cnt=0;
				continue;
			}
		}else{
			match_cnt=0;
		}
	}
	return total_cnt;
}

static const uint8_t ios_18_7_1_bluetoothd_uuid[16] = {
	0x81, 0x31, 0x3d, 0x39, 0x55, 0x33, 0x3f, 0x6b,
	0xaf, 0xcc, 0xe8, 0x85, 0x47, 0x8e, 0x70, 0x86,
};

static int read_macho_uuid(FILE *bin, uint8_t uuid[16]) {
	struct mach_header_64 header;
	if(fseek(bin, 0, SEEK_SET) != 0 || fread(&header, sizeof(header), 1, bin) != 1)
		return 0;
	if(header.magic != MH_MAGIC_64 || header.cputype != CPU_TYPE_ARM64)
		return 0;
	for(uint32_t i=0;i<header.ncmds;i++) {
		long command_offset=ftell(bin);
		struct load_command command;
		if(command_offset < 0 || fread(&command, sizeof(command), 1, bin) != 1)
			return 0;
		if(command.cmdsize < sizeof(command))
			return 0;
		if(command.cmd == LC_UUID) {
			struct uuid_command uuid_command;
			if(fseek(bin, command_offset, SEEK_SET) != 0 || fread(&uuid_command, sizeof(uuid_command), 1, bin) != 1)
				return 0;
			memcpy(uuid, uuid_command.uuid, sizeof(uuid_command.uuid));
			return 1;
		}
		if(fseek(bin, command_offset + command.cmdsize, SEEK_SET) != 0)
			return 0;
	}
	return 0;
}

const char *PGS_addressStatusDescription(enum pgs_address_status status) {
	switch(status) {
		case PGS_ADDRESS_STATUS_OK:return "All iOS 18.7.1 addresses were verified";
		case PGS_ADDRESS_STATUS_OPEN_FAILED:return "Unable to open /usr/sbin/bluetoothd";
		case PGS_ADDRESS_STATUS_INVALID_MACHO:return "bluetoothd is not a valid ARM64 Mach-O";
		case PGS_ADDRESS_STATUS_READ_FAILED:return "Unable to read the required bluetoothd data";
		case PGS_ADDRESS_STATUS_UNSUPPORTED_UUID:return "This bluetoothd build is not supported";
		case PGS_ADDRESS_STATUS_FIRST_NOT_FOUND:return "Product-field function was not found";
		case PGS_ADDRESS_STATUS_FIRST_AMBIGUOUS:return "Product-field function signature was ambiguous";
		case PGS_ADDRESS_STATUS_ABILITY_NOT_FOUND:return "Ability function was not found";
		case PGS_ADDRESS_STATUS_ABILITY_AMBIGUOUS:return "Ability function signature was ambiguous";
		case PGS_ADDRESS_STATUS_REMOTE_VOLUME_NOT_FOUND:return "Remote-volume function was not found";
		case PGS_ADDRESS_STATUS_REMOTE_VOLUME_AMBIGUOUS:return "Remote-volume function signature was ambiguous";
	}
	return "Unknown address finder status";
}

static void log_address_diagnostics(const struct pgs_address_diagnostics *diagnostics) {
	if(diagnostics->status!=PGS_ADDRESS_STATUS_OK)
		os_log_error(OS_LOG_DEFAULT, "[PodsGrant] address discovery failed: %{public}s", PGS_addressStatusDescription(diagnostics->status));
}

int PGS_findAddressesDetailed(struct pgs_address_diagnostics *diagnostics) {
	memset(diagnostics, 0, sizeof(*diagnostics));
	FILE *bin=fopen("/usr/sbin/bluetoothd","rb");
	if(!bin) {
		diagnostics->status=PGS_ADDRESS_STATUS_OPEN_FAILED;
		log_address_diagnostics(diagnostics);
		return 0;
	}
	if(!read_macho_uuid(bin, diagnostics->bluetoothd_uuid)) {
		diagnostics->status=PGS_ADDRESS_STATUS_INVALID_MACHO;
		fclose(bin);
		log_address_diagnostics(diagnostics);
		return 0;
	}
	if(memcmp(diagnostics->bluetoothd_uuid, ios_18_7_1_bluetoothd_uuid, sizeof(ios_18_7_1_bluetoothd_uuid)) != 0) {
		diagnostics->status=PGS_ADDRESS_STATUS_UNSUPPORTED_UUID;
		fclose(bin);
		log_address_diagnostics(diagnostics);
		return 0;
	}
	uint32_t results[16];
	int first_match_cnt=match_instructions(results,16,match_arr_1,sizeof(match_arr_1),bin);
	diagnostics->first_match_count=first_match_cnt;
	if(first_match_cnt!=1) {
		diagnostics->status=first_match_cnt ? PGS_ADDRESS_STATUS_FIRST_AMBIGUOUS : PGS_ADDRESS_STATUS_FIRST_NOT_FOUND;
		fclose(bin);
		log_address_diagnostics(diagnostics);
		return 0;
	}
	diagnostics->first_hook_addr=(uint64_t)results[0]+0x100000000;
	uint32_t product_id_ldr;
	// The copied fields are vendor source, vendor ID, product ID, and version.
	// On iOS 18.7.1 the product ID is the third LDR at object offset 0x438.
	if(fseek(bin,results[0]+8*4,SEEK_SET) != 0 || fread(&product_id_ldr,1,4,bin) != 4) {
		diagnostics->status=PGS_ADDRESS_STATUS_READ_FAILED;
		fclose(bin);
		log_address_diagnostics(diagnostics);
		return 0;
	}
	diagnostics->product_id_offset=((product_id_ldr>>10)&((1<<12)-1))<<2;
	int second_match_cnt=match_instructions(results,16,match_arr_2,sizeof(match_arr_2),bin);
	diagnostics->ability_match_count=second_match_cnt;
	if(second_match_cnt!=1) {
		diagnostics->status=second_match_cnt ? PGS_ADDRESS_STATUS_ABILITY_AMBIGUOUS : PGS_ADDRESS_STATUS_ABILITY_NOT_FOUND;
		fclose(bin);
		log_address_diagnostics(diagnostics);
		return 0;
	}
	diagnostics->ability_func_addr=(uint64_t)results[0]+0x100000000;
	int third_match_cnt=match_instructions(results,16,match_arr_3,sizeof(match_arr_3),bin);
	uint32_t resolved_remote_count=0;
	for(int i=0;i<third_match_cnt;i++) {
		unsigned int adrp_and_add[2];
		if(fseek(bin,results[i]+7*4,SEEK_SET) != 0 || fread(adrp_and_add,1,8,bin) != 8)
			continue;
		uint64_t adrp=*adrp_and_add;
		uint64_t addr=(((adrp>>5)&((1<<19)-1))<<14)|(((adrp>>29)&3)<<12);
		addr+=(results[i]>>12)<<12;
		addr+=(adrp_and_add[1]>>10)&0xfff;
		char buf[45];
		if(fseek(bin,addr,SEEK_SET) == 0 && fread(buf,1,45,bin)==45 && memcmp(buf,"kBTAudioMsgPropertySupportRemoteVolumeChange",45)==0) {
			diagnostics->support_remote_volume_change_addr=(uint64_t)results[i]+0x100000000;
			resolved_remote_count++;
		}
	}
	diagnostics->remote_volume_match_count=resolved_remote_count;
	fclose(bin);
	if(resolved_remote_count!=1) {
		diagnostics->status=resolved_remote_count ? PGS_ADDRESS_STATUS_REMOTE_VOLUME_AMBIGUOUS : PGS_ADDRESS_STATUS_REMOTE_VOLUME_NOT_FOUND;
		log_address_diagnostics(diagnostics);
		return 0;
	}
	diagnostics->status=PGS_ADDRESS_STATUS_OK;
	log_address_diagnostics(diagnostics);
	return 1;
}

int PGS_findAddresses(uint64_t *addresses,uint32_t *product_id_offset) {
	struct pgs_address_diagnostics diagnostics;
	if(!PGS_findAddressesDetailed(&diagnostics))
		return 0;
	addresses[0]=diagnostics.first_hook_addr;
	addresses[1]=diagnostics.ability_func_addr;
	addresses[2]=diagnostics.support_remote_volume_change_addr;
	if(product_id_offset)
		*product_id_offset=diagnostics.product_id_offset;
	return 1;
}
