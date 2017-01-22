#include <stdio.h>
#include <string.h>
#include "mac_addr.h"

static const char* mac_addr_file_loc = "wifi_mac_addrs";
static const char* inventory_mac_addr_file_loc = "mac_inventory.bin";

static void build_mac_database(void);

struct mac_addr_block default_blk;

int main()
{
	FILE *mac_inv;
	FILE *mac_addrs;
	struct mac_addr_block blk;

	mac_inv = fopen(inventory_mac_addr_file_loc, "r");
	if(mac_inv == NULL)
	{
		build_mac_database();
		mac_inv = fopen(inventory_mac_addr_file_loc, "w+");
		if(mac_inv)
		{
			fwrite(&default_blk, sizeof(struct mac_addr_block), 1, mac_inv); 
			fclose(mac_inv);
		}
		else
			return; //File opening error
	}

	mac_addrs = fopen(mac_addr_file_loc, "r");
	if(mac_addrs == NULL)
	{
		mac_inv = fopen(inventory_mac_addr_file_loc, "r+");
		if(mac_inv == NULL)
			return; //File opening error
		mac_addrs = fopen(mac_addr_file_loc, "w+");
		if(mac_addrs)
		{
			rewind(mac_inv);
			fread(&blk, sizeof(struct mac_addr_block), 1, mac_inv); 
			
			if((blk.mac0_addr[0]+4) > 126)
			{
				blk.mac0_addr[0] = 2;
				blk.mac0_addr[1]++;

				blk.mac1_addr[0] = blk.mac0_addr[0] + 1;
				blk.mac2_addr[0] = blk.mac0_addr[0] + 2;
				blk.mac3_addr[0] = blk.mac0_addr[0] + 3;

				blk.mac1_addr[1] = blk.mac0_addr[1];
				blk.mac2_addr[1] = blk.mac0_addr[1];
				blk.mac3_addr[1] = blk.mac0_addr[1];
			}
			else
			{
				blk.mac0_addr[0] +=4;
				blk.mac1_addr[0] = blk.mac0_addr[0] + 1;
				blk.mac2_addr[0] = blk.mac0_addr[0] + 2;
				blk.mac3_addr[0] = blk.mac0_addr[0] + 3;
			}

			fprintf(mac_addrs, "%.2x:%.2x:%.2x:%.2x:%.2x:%.2x\n",
			blk.mac0_addr[5],blk.mac0_addr[4],
			blk.mac0_addr[3],blk.mac0_addr[2],
			blk.mac0_addr[1],blk.mac0_addr[0]);
			fflush(mac_addrs);

			fprintf(mac_addrs, "%.2x:%.2x:%.2x:%.2x:%.2x:%.2x\n",
			blk.mac1_addr[5],blk.mac1_addr[4],
			blk.mac1_addr[3],blk.mac1_addr[2],
			blk.mac1_addr[1],blk.mac1_addr[0]);
			fflush(mac_addrs);

			fprintf(mac_addrs, "%.2x:%.2x:%.2x:%.2x:%.2x:%.2x\n",
			blk.mac2_addr[5],blk.mac2_addr[4],
			blk.mac2_addr[3],blk.mac2_addr[2],
			blk.mac2_addr[1],blk.mac2_addr[0]);
			fflush(mac_addrs);

			fprintf(mac_addrs, "%.2x:%.2x:%.2x:%.2x:%.2x:%.2x",
			blk.mac3_addr[5],blk.mac3_addr[4],
			blk.mac3_addr[3],blk.mac3_addr[2],
			blk.mac3_addr[1],blk.mac3_addr[0]);
			fflush(mac_addrs);

			fclose(mac_addrs);
	
			/* Update the database as well */
			rewind(mac_inv);
			fwrite(&blk, sizeof(struct mac_addr_block), 1, mac_inv); 
		}
		else
			printf("Couldn't open mac_addr file\n");

		fclose(mac_inv);
	}

}

static void build_mac_database(void)
{
	unsigned int low_addr;
	unsigned int high_addr;

	low_addr = 0x16171902;
	high_addr = 0x1415;

	memcpy(default_blk.mac0_addr, &low_addr, 4);
	memcpy((default_blk.mac0_addr+4), &high_addr, 2);

	low_addr = 0x16171903;
	high_addr = 0x1415;

	memcpy(default_blk.mac1_addr, &low_addr, 4);
	memcpy(default_blk.mac1_addr+4, &high_addr, 2);

	low_addr = 0x16171904;
	high_addr = 0x1415;

	memcpy(default_blk.mac2_addr, &low_addr, 4);
	memcpy(default_blk.mac2_addr+4, &high_addr, 2);

	low_addr = 0x16171905;
	high_addr = 0x1415;

	memcpy(default_blk.mac3_addr, &low_addr, 4);
	memcpy(default_blk.mac3_addr+4, &high_addr, 2);
}
