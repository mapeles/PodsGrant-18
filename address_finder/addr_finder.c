#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <mach-o/loader.h>

static const uint32_t match_arr_1[]={
	0xffffffff, 0xaa0003e8,
	0xffffffff, 0x3950b000,
	0xffffffff, 0x7100041f,
	0xff00001f, 0x54000001,
	0xffffffff, 0xb9443109,
	0xffffffff, 0xb9000029,
	0xffffffff, 0xb9443509,
	0xffffffff, 0xb9000049,
	0xffffffff, 0xb9443909,
	0xffffffff, 0xb9000069,
	0xffffffff, 0xb9443d08,
	0xffffffff, 0xb9000088,
	0xffffffff, 0xd65f03c0,
};

static uint32_t match_arr_2[]={
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
	// CMP W*, #0x1d
	0xfffffc1f, 0x7100741f,
	// B.HI *
	0xff00001f, 0x54000008,
};

uint32_t match_arr_3[]={
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

int match_instructions(uint32_t *out, size_t osz, const uint32_t *target, size_t tsz, FILE *bin) {
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
				//printf("Matched at %p.\n",(void*)(ftell(bin)-sizeof(match_arr)/2));
				match_cnt=0;
				continue;
			}
		}else{
			match_cnt=0;
		}
	}
	return total_cnt;
}

static int read_macho_uuid(FILE *bin, uint8_t uuid[16]) {
	struct mach_header_64 header;
	if(fseek(bin, 0, SEEK_SET) != 0 || fread(&header, sizeof(header), 1, bin) != 1)
		return 0;
	if(header.magic != MH_MAGIC_64 || header.cputype != CPU_TYPE_ARM64)
		return 0;
	for(uint32_t i=0;i<header.ncmds;i++) {
		long command_offset=ftell(bin);
		struct load_command command;
		if(command_offset < 0 || fread(&command, sizeof(command), 1, bin) != 1 || command.cmdsize < sizeof(command))
			return 0;
		if(command.cmd == LC_UUID) {
			struct uuid_command uuid_command;
			if(fseek(bin, command_offset, SEEK_SET) != 0 || fread(&uuid_command, sizeof(uuid_command), 1, bin) != 1)
				return 0;
			memcpy(uuid, uuid_command.uuid, 16);
			return 1;
		}
		if(fseek(bin, command_offset + command.cmdsize, SEEK_SET) != 0)
			return 0;
	}
	return 0;
}

static void print_uuid(const uint8_t uuid[16]) {
	for(int i=0;i<16;i++) {
		printf("%02X", uuid[i]);
		if(i==3||i==5||i==7||i==9)
			putchar('-');
	}
	putchar('\n');
}

static const uint8_t supported_uuid[16] = {
	0x81, 0x31, 0x3d, 0x39, 0x55, 0x33, 0x3f, 0x6b,
	0xaf, 0xcc, 0xe8, 0x85, 0x47, 0x8e, 0x70, 0x86,
};

int main(int argc, char *argv[]) {
	if(argc!=2) {
		printf("%s <bluetoothd>\n",argv[0]);
		return 1;
	}
	FILE *bin=fopen(argv[1], "rb");
	if(!bin) {
		printf("status=open_failed\n");
		return 1;
	}
	uint8_t uuid[16];
	if(!read_macho_uuid(bin, uuid)) {
		printf("status=invalid_macho\n");
		fclose(bin);
		return 1;
	}
	printf("uuid=");
	print_uuid(uuid);
	if(memcmp(uuid, supported_uuid, sizeof(supported_uuid)) != 0) {
		printf("status=unsupported_uuid\n");
		fclose(bin);
		return 3;
	}
	uint32_t results[16];
	int first_match_cnt=match_instructions(results,16,match_arr_1,sizeof(match_arr_1),bin);
	printf("first_match_count=%d\n", first_match_cnt);
	if(first_match_cnt!=1) {
		printf("first_status=%s\n", first_match_cnt ? "ambiguous" : "not_found");
	}
	for(int i=0;i<first_match_cnt;i++) {
		printf("first_candidate[%d]=0x%llx\n", i, 0x100000000ULL+results[i]);
	}
	if(first_match_cnt==1) {
		uint32_t product_id_ldr;
		if(fseek(bin,results[0]+8*4,SEEK_SET) != 0 || fread(&product_id_ldr,1,4,bin) != 4) {
			printf("status=read_failed\n");
			fclose(bin);
			return 1;
		}
		uint32_t offset=((product_id_ldr>>10)&((1<<12)-1))<<2;
		printf("product_id_offset=0x%x\n",offset);
	}
	int second_match_cnt=match_instructions(results,16,match_arr_2,sizeof(match_arr_2),bin);
	printf("ability_match_count=%d\n", second_match_cnt);
	if(second_match_cnt!=1) {
		printf("ability_status=%s\n", second_match_cnt ? "ambiguous" : "not_found");
	}
	for(int i=0;i<second_match_cnt;i++) {
		printf("ability_candidate[%d]=0x%llx\n", i, 0x100000000ULL+results[i]);
	}
	int third_match_cnt=match_instructions(results,16,match_arr_3,sizeof(match_arr_3),bin);
	int remote_match_count=0;
	for(int i=0;i<third_match_cnt;i++) {
		unsigned int adrp_and_add[2];
		if(fseek(bin,results[i]+7*4,SEEK_SET) != 0 || fread(adrp_and_add,1,8,bin) != 8)
			continue;
		uint64_t adrp=*adrp_and_add;
		uint64_t addr=(((adrp>>5)&((1<<19)-1))<<14)|(((adrp>>29)&3)<<12);
		addr+=(results[i]>>12)<<12;
		//printf("addr1=%p\n",adrp);
		addr+=(adrp_and_add[1]>>10)&0xfff;
		//addr-=0x100000000;
		char buf[45];
		if(fseek(bin,addr,SEEK_SET) != 0 || fread(buf,1,45,bin)!=45)
			continue;
		//printf("r=%p, addr=%p, buf=%s\n",results[i],addr,buf);
		if(memcmp(buf,"kBTAudioMsgPropertySupportRemoteVolumeChange",45)==0) {
			printf("remote_volume_candidate[%d]=0x%llx\n", remote_match_count, 0x100000000ULL+results[i]);
			remote_match_count++;
		}
	}
	printf("remote_volume_match_count=%d\n", remote_match_count);
	printf("status=%s\n", first_match_cnt==1 && second_match_cnt==1 && remote_match_count==1 ? "ok" : "failed");
	fclose(bin);
	return first_match_cnt==1 && second_match_cnt==1 && remote_match_count==1 ? 0 : 2;
}
