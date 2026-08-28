#pragma once
#include <stdint.h>
#include <stdio.h>

extern unsigned char PGS_global_os_ver;

enum pgs_address_status {
	PGS_ADDRESS_STATUS_OK = 0,
	PGS_ADDRESS_STATUS_OPEN_FAILED,
	PGS_ADDRESS_STATUS_INVALID_MACHO,
	PGS_ADDRESS_STATUS_READ_FAILED,
	PGS_ADDRESS_STATUS_UNSUPPORTED_UUID,
	PGS_ADDRESS_STATUS_FIRST_NOT_FOUND,
	PGS_ADDRESS_STATUS_FIRST_AMBIGUOUS,
	PGS_ADDRESS_STATUS_ABILITY_NOT_FOUND,
	PGS_ADDRESS_STATUS_ABILITY_AMBIGUOUS,
	PGS_ADDRESS_STATUS_REMOTE_VOLUME_NOT_FOUND,
	PGS_ADDRESS_STATUS_REMOTE_VOLUME_AMBIGUOUS,
};

struct pgs_address_diagnostics {
	enum pgs_address_status status;
	uint8_t bluetoothd_uuid[16];
	uint32_t product_id_offset;
	uint64_t first_hook_addr;
	uint64_t ability_func_addr;
	uint64_t support_remote_volume_change_addr;
	uint32_t first_match_count;
	uint32_t ability_match_count;
	uint32_t remote_volume_match_count;
};

int PGS_findAddressesDetailed(struct pgs_address_diagnostics *diagnostics);
const char *PGS_addressStatusDescription(enum pgs_address_status status);

struct address_map_entry {
	unsigned char version_major;
	unsigned char version_minor;
	unsigned char version_patch;
	unsigned int product_id_offset;
	uint64_t first_hook_addr;
	uint64_t ability_func_addr;
	uint64_t support_remote_volume_change_addr;
	uint64_t recv_logging_handler_addr;
};

struct product_id_map_entry_custom {
	uint16_t original;
	uint16_t target;
};

struct product_id_map_entry {
	uint16_t original;
	uint16_t target;
	uint8_t minimum_ios;
	uint8_t maximum_ios;
};

static const struct product_id_map_entry product_id_map_preset[] = {
	{8212, 8206, 0, 15},
	{8217, 8207, 0, 255},
	{8219, 8206, 0, 15}, // AirPods 4 (issue #73) -> AirPods Pro
	{8228, 8206, 0, 15},
	{8231, 8206, 0, 15}, // AirPods Pro 3 -> AirPods Pro
	{8232, 8206, 0, 15}, // AirPods Pro 3 -> AirPods Pro
	{8228, 8212, 16, 255}, // iOS 16 supports AirPods Pro 2 Lightning
	{8219, 8212, 16, 255}, // AirPods 4 -> AirPods Pro 2 Lightning
	{8231, 8212, 16, 255}, // AirPods Pro 3 -> AirPods Pro 2 Lightning
	{8232, 8212, 16, 255}, // AirPods Pro 3 -> AirPods Pro 2 Lightning
	{8221, 8203, 0, 16},
	{8211, 8207, 0, 255},
	{8214, 8209, 0, 255},
	{0, 0, 0, 0}
};

struct podsgrant_settings {
	uint8_t is_tweak_enabled;
	uint8_t is_managed_structure;
	struct product_id_map_entry_custom *product_id_mapping;
	struct address_map_entry *address_mapping;
	uint8_t product_id_mapping_cnt;
	uint8_t address_mapping_cnt;
};

#define NSSTR(a) @a

#define PGS_SETTINGS_FILE "/var/mobile/Library/Preferences/com.lns.pogr.bin"

uint16_t PGS_patchProductId(struct podsgrant_settings *conf, uint16_t original);

int PGS_saveSettings(struct podsgrant_settings *configuration);
struct podsgrant_settings *PGS_readSettings_to(struct podsgrant_settings *configuration, int read_full_anyway);
struct podsgrant_settings *PGS_readSettings(int read_full_anyway);
void PGS_freeSettings(struct podsgrant_settings *conf);

int PGS_findAddresses(uint64_t *addresses,uint32_t *pid_offset);
