/*
 * Quantenna Board Provision configuraion file upload/update/show support
 */
#include <common.h>
#include <command.h>
#include <environment.h>
#include <malloc.h>
#include <net.h>
#include <stdbool.h>

#include "ruby.h"
#include "spi_flash.h"
#include "board_cfg.h"
#include "ruby_config.h"
#include "ruby_board_cfg.h"
#include "ruby_board_db.h"
#include "shared_defs_common.h"

#define FAIL			(-1)
#define SUCCESS			(0)
#define CMD_BUFSIZE		256
#define TMP_BUFSIZE		256
#define QTN_HW_BOARD_CFG_BIN		"qtn_hw_board_config.bin"
#define QTN_HW_BOARD_DB_BIN		"qtn_hw_board_db.bin"
#ifdef TOPAZ_EP_MINI_UBOOT
#undef RUN
#define RUN(fmt,args...)
#endif

#define QTN_FILES_DEBUG 0

#if QTN_FILES_DEBUG
#define DBG_PRINTF(fmt, args...) printf(fmt ,##args)
#else
#define DBG_PRINTF(fmt, args...)
#endif

char board_config_name[32];
board_cfg_t board_hw_config;
uint32_t flash_size = 0;

static const char * const type2name[] = BOARD_CFG_FIELD_NAMES;

static board_cfg_t	g_default_board_cfg = {
	.bc_board_id	= QTN_RUBY_UNIVERSAL_BOARD_ID,
	.bc_name	= "default board",
	.bc_ddr_type	= DEFAULT_DDR_CFG,
	.bc_ddr_speed	= DEFAULT_DDR_SPEED,
	.bc_ddr_size	= DDR_AUTO,
	.bc_emac0	= EMAC_NOT_IN_USE,
	.bc_emac1	= EMAC_NOT_IN_USE,
	.bc_phy0_addr = EMAC_PHY_ADDR_SCAN,
	.bc_phy1_addr = EMAC_PHY_ADDR_SCAN,
	.bc_spi1	= SPI1_IN_USE,
	.bc_wifi_hw	= QTN_RUBY_WIFI_NONE,
	.bc_uart1	= UART1_NOT_IN_USE,
	.bc_rgmii_timing = CONFIG_ARCH_RGMII_DEFAULT,
	};

#ifndef TOPAZ_EP_MINI_UBOOT
static uint32_t qtn_board_get_end(void)
{
	char tmpBuf[TMP_BUFSIZE];
	uint32_t addr;
	int i;
	if ((i = getenv_r("config_data_end", tmpBuf, sizeof(tmpBuf))) <= 0) {
		/* the new bootcfg data will be first entry in system */
		addr = BOOT_CFG_DEF_START;
	} else {
		tmpBuf[i]='\0';
		addr = simple_strtoul(tmpBuf, NULL, 16);
	}
	return addr;
}
#endif /* TOPAZ_EP_MINI_UBOOT */

static long qtn_board_get_env_val(char *env)
{
	char tmpBuf[TMP_BUFSIZE];
	long val = 0;
	int i;
	if ((i = getenv_r(env, tmpBuf, sizeof(tmpBuf))) <= 0) {
		val = -1;
	} else {
		tmpBuf[i]='\0';
		val = simple_strtol(tmpBuf, NULL, 0);
	}

	return val;
}

#ifndef TOPAZ_EP_MINI_UBOOT
static uint32_t qtn_board_get_load_addr(void)
{
	char tmpBuf[TMP_BUFSIZE];
	uint32_t addr;
	int i;
	if ((i = getenv_r("loadaddr", tmpBuf, sizeof(tmpBuf))) <= 0) {
		/* the new bootcfg data will be first entry in system */
		addr = CONFIG_SYS_LOAD_ADDR;
	} else {
		tmpBuf[i]='\0';
		addr = simple_strtoul(tmpBuf, NULL, 16);
	}
	return addr;
}
#endif /* TOPAZ_EP_MINI_UBOOT */

static uint8_t *qtn_get_file_env_ptr(char *filename, uint16_t *size)
{
	int retval;
	uint32_t offset;
	char tmp[64], *ch;

	retval = getenv_r(filename, tmp, sizeof(tmp));
	if (retval < 0) {
		printf("File %s doesn't exist\n", filename);
		return NULL;
	}

	if(strstr(tmp, "cfg") == NULL) {
		printf("wrong file description. correct format is 'cfg 0xXXX 0xYYY'.\n");
		printf("use printenv for debugging\n");
		return NULL;
	}

	/* skip the string "cfg " */
	ch = tmp;
	while (*ch != '0') ch++;

	offset = simple_strtoul(ch, NULL, 16);

	/* skip offset to find size */
	if (size) {
		while (*ch != ' ') ch++;
		while (*ch != '0') ch++;
		*size = simple_strtoul(ch, NULL, 16);
	}
	return env_get_file_body(offset);
}

static int qtn_get_emac_set(void)
{
	char *emac_set_str = NULL;
	char *emac_index = NULL;
	uint32_t emac_set = 0;
	int retval = 0;

	emac_index = getenv("emac_index");
	emac_set_str = getenv("emac_set");

	if (emac_index != NULL && emac_set_str != NULL) {
		if (simple_strtoul(emac_index, NULL, 0) == 0) {

			emac_set = simple_strtoul(emac_set_str, NULL, 0);
			g_default_board_cfg.bc_emac0 = (int)emac_set;
		}else if (simple_strtoul(emac_index, NULL, 0) == 1) {

			emac_set = simple_strtoul(emac_set_str, NULL, 0);
			g_default_board_cfg.bc_emac1 = (int)emac_set;
		}
	} else {
		printf("Please setup emac setting correctly for tftp load file\n");
		retval = FAIL;
	}

	return retval;
}

static int qtn_get_ddr_set(void)
{
	char ddr_set_str[TMP_BUFSIZE];
	char *ddr_set_type = NULL;
	char *ddr_set_speed = NULL;
	char *ddr_set_size = NULL;
	int retval = 0;
	int i;

	i = getenv_r("ddr_set", ddr_set_str, sizeof(ddr_set_str));

	if (i > 0) {
		ddr_set_str[i] = '\0';
		ddr_set_type = strtok(ddr_set_str, ",");
		ddr_set_speed = strtok(NULL, ",");
		ddr_set_size = strtok(NULL, ",");

		if (ddr_set_type == NULL || ddr_set_speed == NULL || ddr_set_size == NULL) {
			goto INVALID_DDR_SET;
		}
		g_default_board_cfg.bc_ddr_type = (int)simple_strtoul(ddr_set_type, NULL, 0);
		g_default_board_cfg.bc_ddr_speed = (int)simple_strtoul(ddr_set_speed, NULL, 0);
		g_default_board_cfg.bc_ddr_size = (int)simple_strtoul(ddr_set_size, NULL, 0);

		return retval;
	}

INVALID_DDR_SET:
	printf("Please setup ddr setting correctly for tftp load file\n");
	retval = FAIL;

	return retval;
}

static int board_hw_config_apply(uint16_t type,
		int data, uint8_t datalen,
		const char *str, uint8_t strlen)
{
	int need_save = 0;
	int lna_gain = 0;
	int antenna_number = 0;
	int antenna_gain = 0;
	int lna_gain_5db = 0;
	int tx_power_cal = 0;

	if (type == BOARD_CFG_NAME) {
		memcpy(board_config_name, str, min(strlen, sizeof(board_config_name) - 1));
		board_hw_config.bc_name = board_config_name;
	} else if (type < BOARD_CFG_STRUCT_NUM_FIELDS) {
		int *p = (int *) &board_hw_config;
		p[type] = data;
	} else {
		switch (type) {
		case BOARD_CFG_FLASH_SIZE:
			flash_size = data;
			if (flash_size <= 1024)
				flash_size <<= 20;
			break;
		case BOARD_CFG_EXT_LNA_GAIN:
			lna_gain = data;
			if (qtn_board_get_env_val("ext_lna_gain") != lna_gain) {
				RUN("setenv ext_lna_gain %d", lna_gain);
				need_save = 1;
			}
			break;
		case BOARD_CFG_TX_ANTENNA_NUM:
			antenna_number = data;
			if (qtn_board_get_env_val("antenna_num") != antenna_number) {
				RUN("setenv antenna_num %d", antenna_number);
				need_save = 1;
			}
			break;
		case BOARD_CFG_TX_ANTENNA_GAIN:
			antenna_gain = data;
			if (qtn_board_get_env_val("antenna_gain") != antenna_gain) {
				RUN("setenv antenna_gain %d", antenna_gain);
				need_save = 1;
			}
			break;
		case BOARD_CFG_EXT_LNA_BYPASS_GAIN:
			lna_gain_5db = data;
			if ((int8_t)(qtn_board_get_env_val("ext_lna_bypass_gain") - 256 ) != (int8_t)(lna_gain_5db - 256)) {
				RUN("setenv ext_lna_bypass_gain %d", (int8_t)(lna_gain_5db - 256));
				need_save = 1;
			}
			break;
		case BOARD_CFG_CALSTATE_VPD:
			tx_power_cal = data;
			if (qtn_board_get_env_val("tx_power_cal") != tx_power_cal) {
				RUN("setenv tx_power_cal %d", tx_power_cal);
				need_save = 1;
			}
			break;
		default:
			break;
		}
	}

	return need_save;
}

#ifndef TOPAZ_EP_MINI_UBOOT
static int board_hw_config_display(uint16_t type,
		int data, uint8_t datalen,
		const char *str, uint8_t strlen)
{
	if (type >= ARRAY_SIZE(type2name)) {
		return 0;
	}

	printf("%d: %s:\n", type+1, type2name[type]);
	if (type == BOARD_CFG_NAME) {
		printf(" %s\n", str);
	} else if (datalen == 0) {
		int db_data = 0;
		if (board_parse_tag(type2name[type], str, &db_data) == 0) {
			printf(" %s, 0x%08x\n", str, db_data);
		} else {
			printf(" %s\n", str);
		}
	} else {
		printf(" %s, 0x%08x\n", str, data);
	}

	return 0;
}
#endif /* TOPAZ_EP_MINI_UBOOT */

static int board_hw_config_no_resolve(int bc_index)
{
	return bc_index == BOARD_CFG_ID ||
		bc_index == BOARD_CFG_NAME ||
		bc_index == BOARD_CFG_EMAC0 ||
		bc_index == BOARD_CFG_EMAC1 ||
		bc_index == BOARD_CFG_RGMII_TIMING ||
		bc_index >= ARRAY_SIZE(type2name);
}

static int board_hw_config_iterate(int (*apply)(uint16_t, int, uint8_t, const char *, uint8_t),
		const uint8_t * const board_config,
		uint16_t board_config_size)
{
	int ret = 0;
	const uint8_t *cfg_p = board_config;

	while (1) {
		uint16_t config_index;
		uint16_t bc_index;
		uint8_t data_len = 0;
		uint8_t str_len = 0;
		char str[64] = {0};
		int data = 0;
		int uboot_data = 0;
		uint16_t bytes_read;

		/*
		 * Config index from the file is
		 * board config index + 1
		 */
		memcpy(&config_index, cfg_p, sizeof(config_index));
		cfg_p += sizeof(config_index);
		if (config_index > ARRAY_SIZE(type2name)) {
			printf("%s: invalid config_index: %u\n", __FUNCTION__, config_index);
			break;
		}

		str_len = *cfg_p;
		cfg_p += sizeof(str_len);
		if (str_len > sizeof(str) - 1) {
			printf("%s: invalid strlen: %u\n", __FUNCTION__, str_len);
			break;
		}
		memcpy(str, cfg_p, str_len);
		cfg_p += str_len;

		data_len = *cfg_p;
		cfg_p += sizeof(data_len);
		if (data_len > 4) {
			printf("%s: invalid data len: %u\n", __FUNCTION__, data_len);
			break;
		}
		/* file is always little endian */
		memcpy(&data, cfg_p, data_len);
		cfg_p += data_len;

		bc_index = config_index - 1;
		/* lookup string in the compiled in database */
		if (board_hw_config_no_resolve(bc_index)) {
			/* Don't attempt to resolve */
		} else if (data_len > 0 &&
				!board_parse_tag(type2name[bc_index], str, &uboot_data) &&
				data != uboot_data) {
			printf("%s: %s value from config %d doesn't match expected value %d\n",
					__FUNCTION__, type2name[bc_index], data, uboot_data);
		}

		if (0 && bc_index < ARRAY_SIZE(type2name)) {
			printf("%s: cfg %u %s <- 0x%08lx %s\n",
					__FUNCTION__,
					bc_index, type2name[bc_index],
					(unsigned long) data, str);
		}

		if (bc_index < ARRAY_SIZE(type2name))
			ret |= apply(bc_index, data, data_len, str, str_len);

		bytes_read = cfg_p - ((const uint8_t *) board_config);
		if (bytes_read >= board_config_size) {
			break;
		}
	}

	return ret;
}

int board_hw_config_read(void)
{
	int need_save = 0;
	uint16_t board_config_size = 0;
	const uint8_t * const board_config = qtn_get_file_env_ptr(QTN_HW_BOARD_CFG_BIN, &board_config_size);

	if (board_config == NULL) {

		if (qtn_get_emac_set() < 0) {
			return FAIL;
		}

		if (qtn_get_ddr_set() < 0) {
			return FAIL;
		}

		memcpy(&board_hw_config, &g_default_board_cfg, sizeof(board_cfg_t));
		return SUCCESS;
	}

	board_hw_config.bc_board_id = QTN_RUBY_UNIVERSAL_BOARD_ID;
	need_save = board_hw_config_iterate(&board_hw_config_apply,
			board_config, board_config_size);
	if (need_save == 1) {
		saveenv();
	}

	return 0;
}

#ifndef TOPAZ_EP_MINI_UBOOT
int do_qtn_show_board_hw_config(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	uint16_t board_config_size;
	const uint8_t * const board_config = qtn_get_file_env_ptr(QTN_HW_BOARD_CFG_BIN, &board_config_size);

	if (board_config == NULL) {
		return 0;
	}

	printf("##############current board Hardware configuration########################\n");

	board_hw_config_iterate(&board_hw_config_display,
			board_config, board_config_size);

	return 0;
}

int do_qtn_show_board_hw_db(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	uint16_t board_database_size;
	const uint8_t * const board_database = qtn_get_file_env_ptr(QTN_HW_BOARD_DB_BIN, &board_database_size);
	const uint8_t *cfg_p = board_database;

	uint16_t tlv_cnt;
	int i;

	if (board_database == NULL) {
		return 0;
	}

	memcpy(&tlv_cnt, cfg_p, sizeof(tlv_cnt));
	cfg_p += sizeof(tlv_cnt);

	printf("##############current board Hardware database########################\n");
	printf("Total %d configuration types\n", tlv_cnt-1);
	for (i = 0; i < tlv_cnt; i++) {
		int j;
		uint16_t type;
		uint16_t v_cnt;
		memcpy(&type, cfg_p, sizeof(type));
		cfg_p += sizeof(type);
		memcpy(&v_cnt, cfg_p, sizeof(v_cnt));
		cfg_p += sizeof(v_cnt);

		if (type > ARRAY_SIZE(type2name)) {
			printf("%u: %s, %u options\n", type, "unknown", v_cnt);
		}else if (type > 0)
			printf("%u: %s, %u options\n", type, type2name[type-1], v_cnt);

		for (j = 0; j < v_cnt; j++) {
			const char *str_sep = j == (v_cnt - 1) ? "" : ";";
			uint8_t datalen;
			uint8_t strlen;
			char str[64] = {0};
			int data = 0;

			strlen = *cfg_p;
			cfg_p += sizeof(strlen);
			if (strlen > sizeof(str) - 1) {
				printf("%s: invalid strlen: %u\n", __FUNCTION__, strlen);
				break;
			}
			memcpy(str, cfg_p, strlen);
			cfg_p += strlen;

			datalen = *cfg_p;
			cfg_p += sizeof(datalen);
			if (datalen > 4) {
				printf("%s: invalid datalen: %u\n", __FUNCTION__, datalen);
				break;
			}
			memcpy(&data, cfg_p, datalen);	// little endian
			cfg_p += datalen;

			if (type > 0) {
				if (datalen == 0) {
					printf(" %s%s\n", str, str_sep);
				} else {
					printf(" %s, 0x%08x%s\n", str, data, str_sep);
				}
			}
		}
	}

	return 0;
}

int qtn_upload_update_file_to_flash(char *file_name, int transfer_len)
{
	int retval = 0;
	uint32_t mem_addr = qtn_board_get_load_addr();
	uchar *data = NULL;

	if ( qtn_get_file_env_ptr(file_name, NULL) ) {
		printf("file %s already exist. use qtn_erase_file to delete it first\n", file_name);
		return FAIL;
	}

	if ((qtn_board_get_end() + transfer_len) > BOOT_CFG_DATA_SIZE) {
                printf("error - len out of range, transfer len = %d, end_of_present = 0x%08x, border = 0x%08x\n", transfer_len, qtn_board_get_end(), (int)BOOT_CFG_DATA_SIZE);
                return 0;
        }

	if (transfer_len > 0) {
                retval = RUN("setenv %s cfg 0x%08x 0x%08x", file_name, (qtn_board_get_end()), transfer_len);
                if (retval >= 0) {
                        retval = RUN("setenv config_data_end 0x%08x", (qtn_board_get_end() + transfer_len));
                        if (retval < 0) {
                                printf("setenv config_data_end error\n");
                                return 0;
                        }
                } else {
                        printf("setenv %s error\n", file_name);
                        return 0;
                }
        } else {
                printf("File size of %s error, please try again\n", file_name);
                return 0;
        }

	data = qtn_get_file_env_ptr(file_name, NULL);
	if (data != NULL) {
		memcpy(data, (void *) mem_addr, transfer_len);
                printf("0x%x bytes data copied from 0x%.8x to 0x%.8x\n", transfer_len, (size_t)mem_addr, (size_t)data);
	} else {
		printf("unable to write file\n");
		return 0;
	}

	env_crc_update();
	saveenv();
	return 0;
}

int do_qtn_upload_file(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	char *file_name = NULL;
	int retval = 0;

	if (argc < 2) {
		printf("Help:\n");
		printf("qtn_upload_file <file_name>\n");
		return 0;
	}
	file_name = argv[1];

	retval = RUN("tftpboot %s", file_name);
	if (retval < 0)
		return retval;

	return qtn_upload_update_file_to_flash(file_name, NetBootFileXferSize);
}

int do_qtn_upload_file_serial(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	char tmpBuf[TMP_BUFSIZE];
	char *file_name;
	int retval = 0;
	uint32_t file_size;

	if (argc < 2) {
		printf("Help:\n");
		printf("qtn_upload_file_serial <file_name>\n");
		return 0;
	}
	file_name = argv[1];

	retval = RUN("loadb");
	if (retval < 0)
		return retval;

	getenv_r("filesize", tmpBuf, sizeof(tmpBuf));
	file_size = simple_strtoul(tmpBuf, NULL, 16);

	return qtn_upload_update_file_to_flash(file_name, file_size);
}

bool qtn_envvar_get_next
(
		int* offset,            /* for first var just put 0 */
		char* name,             /* name typically ends by '=' in environment database. better just to copy it to outer buffer*/
		int name_buffer_length, /* to be safe about buffer overflows */
		char** value            /* as the value is zero-terminated it is safe not to copy it*/
)
{
	bool result = false;

	char* name_and_value = (char*)env_get_addr(*offset);

	char* value_part = strchr(name_and_value, '=');

	if (value_part) {
		int name_len = value_part - name_and_value;
		result = true; /* OK, we've found something like name=value\0 */
		*value = value_part + 1; /* skip the '=' itself */
		if (name_len > (name_buffer_length -1))	{
			name_len = name_buffer_length -1;
			printf("environment variable name length exceeds %d, truncated\n", name_buffer_length);
		}
		strncpy(name, name_and_value, name_len);
		name[name_len] = 0;
	}

	*offset += strlen(name_and_value) + 1; /* +1 to skip trailing zero */

	return result;
}

/*returns TRUE if the variable looks like file description*/
bool qtn_get_file_location(char* value, uint8_t** p_data, uint16_t* size)
{
#define FILE_ID_TRAIT "cfg "
#define	FILE_ID_TRAIT_LEN (sizeof(FILE_ID_TRAIT)-1)
	int result = false;
	if (0 == strncmp(value, FILE_ID_TRAIT, FILE_ID_TRAIT_LEN)) {
		char* current = value + FILE_ID_TRAIT_LEN;
		while (*current != '0') current++; /* skip blanks */
		if (p_data) {
			*p_data = env_get_file_body(simple_strtoul(current, &current, 16));
		}
		while (*current != '0') current++; /* skip blanks */
		if (size) {
			*size = (uint16_t)simple_strtoul(current, NULL, 16);
		}
		result = true;
	}
	return result;
}

void qtn_set_file_location(char* value, uint8_t* p_data, uint16_t size)
{
#define FILE_DESCRIPTOR_FORMAT_STR "cfg 0x%08x 0x%08x"
	/*it is safe to do modification in place because of fixed data format*/
	sprintf
	(
		value,
		FILE_DESCRIPTOR_FORMAT_STR,
		(uint32_t)(p_data - (uint8_t*)env_get_file_body(0)), // an offset
		(uint32_t)size
	);
}

int do_qtn_erase_file(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	char *file_name           = NULL;
	int offset                = 0;
	char  name[TMP_BUFSIZE];
	char* value               = NULL;
	uint8_t* erase_ptr        = NULL;
	uint16_t erase_size       = 0;
	int bytes_to_move         = 0;
	uint32_t current_data_end = qtn_board_get_end();

	if (argc < 2) {
		printf("Help:\n");
		printf("qtn_erase_file <file_name>\n");
		return FAIL;
	}
	file_name = argv[1];

	erase_ptr = qtn_get_file_env_ptr(file_name, &erase_size);
	DBG_PRINTF("erase_ptr = %p, size to be erased = %d\n", erase_ptr, erase_size);

	if ( NULL == erase_ptr ) {
		DBG_PRINTF("file does not exist\n");
		return FAIL;
	}

	while ( qtn_envvar_get_next( &offset, name, sizeof(name), &value) ) {
		uint8_t* dataptr = NULL;
		uint16_t size;
		DBG_PRINTF("checking variable %s\n", name);
		if (qtn_get_file_location(value, &dataptr, &size)) {
			DBG_PRINTF("file with size %d is at %p\n", size, dataptr);
			if ((erase_ptr + erase_size) <= dataptr) {
				qtn_set_file_location(value, dataptr - erase_size, size);
				DBG_PRINTF("shifted left from %p to %p\n", dataptr, dataptr - erase_size);
			} else {
				DBG_PRINTF("no need to move\n");
			}
		} else {
			DBG_PRINTF("not a file, skipped\n");
		}
	}
	RUN("setenv config_data_end 0x%08x", current_data_end - erase_size );
	RUN("setenv %s", file_name);

	/*file descriptors updated, now is the time to shift the actual content*/
	/*we are dealing with overlapped regions so memmove is our choice*/
	bytes_to_move = (uint8_t*)env_get_file_body(current_data_end) - ( erase_ptr + erase_size );
	DBG_PRINTF("%d byte(s) needs to be moved from %p to %p\n", bytes_to_move, erase_ptr + erase_size, erase_ptr);
	memmove(erase_ptr, erase_ptr + erase_size, bytes_to_move);
	DBG_PRINTF("done\n");

	env_crc_update();
	DBG_PRINTF("environment CRC updated\n");

	saveenv();
	DBG_PRINTF("environment saved\n");

	return SUCCESS;
}

int do_qtn_rename_file(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	char* old_file_name      = NULL;
	char* new_file_name      = NULL;
	char tmpBuf[TMP_BUFSIZE] = {0};

	if (argc < 3) {
		printf("Help:\n");
		printf("qtn_rename_file <oldname> <newname>\n");
		return FAIL;
	}
	old_file_name = argv[1];
	new_file_name = argv[2];

	if ( getenv_r(old_file_name, tmpBuf, sizeof(tmpBuf)) <= 0) {
		printf("unable to get value from environment. use printenv for debugging\n");
		return FAIL;
	}
	if ( !qtn_get_file_location(tmpBuf, NULL, NULL) ) {
		printf("wrong file descriptor. unable to rename\n");
		return FAIL;
	}

	setenv(old_file_name, NULL);
	setenv(new_file_name, tmpBuf);

	env_crc_update();
	saveenv();

	return SUCCESS;
}

int do_qtn_dump_file(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	char *file_name      = NULL;
	uint8_t* ptr         = NULL;
	uint16_t size        = 0;

	if (argc < 2) {
		printf("Help:\n");
		printf("qtn_dump_file <file_name>\n");
		return FAIL;
	}
	file_name = argv[1];

	ptr = qtn_get_file_env_ptr(file_name, &size);

	if ( !ptr ) {
		printf("Unable to read file %s\n", file_name);
		return FAIL;
	}

	RUN("md.b 0x%x 0x%x", (uint32_t)ptr, (uint32_t)size);

	return SUCCESS;
}

int do_qtn_list_files(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	int offset                = 0;
	char  name[TMP_BUFSIZE];
	char* value               = NULL;

	while (qtn_envvar_get_next( &offset, name, sizeof(name), &value)) {
		uint8_t* dataptr = NULL;
		uint16_t size;
		if (qtn_get_file_location(value, &dataptr, &size)) {
			printf("file %s with size %d is at %p\n", name, size, dataptr);
		}
	}

	return SUCCESS;
}

U_BOOT_CMD(qtn_upload_file, CONFIG_SYS_MAXARGS, 0, do_qtn_upload_file,
		"Upload file from memory to flash via ethernet port",
		"Quantenna universal board config. Attempts to upload file into flash\n"
	  );

U_BOOT_CMD(qtn_upload_file_serial, CONFIG_SYS_MAXARGS, 0, do_qtn_upload_file_serial,
		"Upload file from memory to flash via serial port",
		"Quantenna universal board config. Attempts to upload file into flash\n"
	  );

U_BOOT_CMD(qtn_show_board_config, CONFIG_SYS_MAXARGS, 0, do_qtn_show_board_hw_config,
		"Show board hw configuration",
		"Quantenna universal board config.\n"
	  );

U_BOOT_CMD(qtn_show_board_database, CONFIG_SYS_MAXARGS, 0, do_qtn_show_board_hw_db,
		"Show board hw configuration database content",
		"Quantenna universal board config.\n"
	  );

U_BOOT_CMD(qtn_erase_file, 2, 0, do_qtn_erase_file,
		"Erases file from flash <filename>",
		"Quantenna universal board config. Erases file from flash\n"
	  );

U_BOOT_CMD(qtn_rename_file, 3, 0, do_qtn_rename_file,
		"Renames file from <oldname> to <newname>",
		"Quantenna universal board config. Renames file\n"
	  );

U_BOOT_CMD(qtn_dump_file, 2, 0, do_qtn_dump_file,
		"Dumps file from config data space <filename>",
		"Quantenna universal board config. Dumps file\n"
	  );

U_BOOT_CMD(qtn_list_files, 1, 0, do_qtn_list_files,
		"list files from config data space",
		"Quantenna universal board config.\n"
	  );

#endif /* TOPAZ_EP_MINI_UBOOT */

