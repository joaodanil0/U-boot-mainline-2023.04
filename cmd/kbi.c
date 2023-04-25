#include <common.h>
#include <bootretry.h>
#include <cli.h>
#include <command.h>
#include <dm.h>
#include <edid.h>
#include <env.h>
#include <amlogic/cpu_id.h>
#include <errno.h>
#include <i2c.h>
#include <malloc.h>
#include <asm/byteorder.h>
#include <linux/compiler.h>
#include <asm/arch/io.h>
#include <asm/arch/secure_apb.h>
#include <asm/u-boot.h>
// #include <amlogic/saradc.h>
#include <common.h>
#include <command.h>
#include <dm/uclass.h>
#include <asm/io.h>
#include <asm/arch/bl31_apis.h>

#define CHIP_ADDR              0x18
#define CHIP_ADDR_CHAR         "0x18"
#define I2C_SPEED              100000

#define REG_PASSWD_VENDOR       0x00
#define REG_MAC                 0x06
#define REG_USID                0x0c
#define REG_VERSION             0x12

#define REG_BOOT_MODE           0x20
#define REG_BOOT_EN_WOL         0x21
#define REG_BOOT_EN_RTC         0x22
#define REG_BOOT_EN_IR          0x24
#define REG_BOOT_EN_DCIN        0x25
#define REG_BOOT_EN_KEY         0x26
#define REG_BOOT_EN_GPIO        0x23
#define REG_LED_SYSTEM_ON_MODE  0x28
#define REG_LED_SYSTEM_OFF_MODE 0x29
#define REG_ADC                 0x2a
#define REG_MAC_SWITCH          0x2d
#define REG_IR_CODE1            0x2f
#if defined(CONFIG_KHADAS_VIM3) || defined(CONFIG_KHADAS_VIM3L)
#define REG_PORT_MODE           0x33
#endif
#define REG_IR_CODE2            0x34
#define REG_PASSWD_CUSTOM       0x40

#define REG_POWER_OFF           0x80
#define REG_PASSWD_CHECK_STATE  0x81
#define REG_PASSWD_CHECK_VENDOR 0x82
#define REG_PASSWD_CHECK_CUSTOM 0x83
#define REG_POWER_STATE    0x86

#define TST_STATUS         0x90

#define BOOT_EN_WOL         0
#define BOOT_EN_RTC         1
#define BOOT_EN_IR          2
#define BOOT_EN_DCIN        3
#define BOOT_EN_KEY         4
#define BOOT_EN_GPIO        5

#define LED_OFF_MODE        0
#define LED_ON_MODE         1
#define LED_BREATHE_MODE    2
#define LED_HEARTBEAT_MODE  3

#define LED_SYSTEM_OFF      0
#define LED_SYSTEM_ON       1

#define BOOT_MODE_SPI       0
#define BOOT_MODE_EMMC      1

#define FORCERESET_WOL      0
#define FORCERESET_GPIO     1

#define VERSION_LENGHT        2
#define USID_LENGHT           6
#define MAC_LENGHT            6
#define ADC_LENGHT            2
#define PASSWD_CUSTOM_LENGHT  6
#define PASSWD_VENDOR_LENGHT  6

#define HW_VERSION_ADC_VALUE_TOLERANCE   0x28
#define HW_VERSION_ADC_VAL_VIM1_V12      0x204
#define HW_VERSION_ADC_VAL_VIM1_V14      0x28a
#define HW_VERSION_ADC_VAL_VIM2_V12      0x200
#define HW_VERSION_ADC_VAL_VIM2_V14      0x28A
#define HW_VERSION_ADC_VAL_VIM3_V11      0x200
#define HW_VERSION_ADC_VAL_VIM3_V12      0x288
#define HW_VERSION_UNKNOW                0x00
#define HW_VERSION_VIM1_V12              0x12
#define HW_VERSION_VIM1_V14              0x14
#define HW_VERSION_VIM2_V12              0x22
#define HW_VERSION_VIM2_V14              0x24
#define HW_VERSION_VIM3_V11              0x31
#define HW_VERSION_VIM3_V12              0x32


static char* LED_MODE_STR[] = { "off", "on", "breathe", "heartbeat"};
static struct udevice *mcu_i2c_cur_bus;

static int mcu_i2c_set_bus_num(unsigned int busnum)
{
	struct udevice *bus;
	int ret;

	ret = uclass_get_device_by_seq(UCLASS_I2C, busnum, &bus);
	if (ret) {
		debug("%s: No bus %d\n", __func__, busnum);
		return ret;
	}

	mcu_i2c_cur_bus = bus;

	return 0;
}

static int mcu_i2c_get_cur_bus(struct udevice **busp)
{
	if (!mcu_i2c_cur_bus) {
		if (mcu_i2c_set_bus_num(MCU_I2C_BUS_NUM)) {
			printf("Default I2C bus %d not found\n",
					MCU_I2C_BUS_NUM);

			return -ENODEV;
		}
	}

	if (!mcu_i2c_cur_bus) {
		puts("No I2C bus selected\n");
		return -ENODEV;
	}
	*busp = mcu_i2c_cur_bus;

	return 0;
}

static int mcu_i2c_get_cur_bus_chip(uint chip_addr, struct udevice **devp)
{
	struct udevice *bus;
	int ret;

	ret = mcu_i2c_get_cur_bus(&bus);
	if (ret)
	return ret;

	return i2c_get_chip(bus, chip_addr, 1, devp);
}


static int mcu_i2c_probe(u32 bus)
{
	return mcu_i2c_set_bus_num(bus);
}

int khadas_i2c_read(u8 addr, u8 reg, u8 *val)
{
	int ret;
	struct udevice *dev;

	ret = mcu_i2c_get_cur_bus_chip(addr, &dev);

	if (!ret)
		ret = dm_i2c_read(dev, reg, val, 1);
	if (ret != 0)
		printf("khadas_i2c_read error!!!\n");

	return ret;
}

int khadas_i2c_write(u8 addr, u8 reg, u8 val)
{
	int ret;
	struct udevice *dev;

	ret = mcu_i2c_get_cur_bus_chip(addr, &dev);
	if (!ret)
		ret = dm_i2c_write(dev, reg, (u8 *)&val, 1);
	if (ret != 0)
		printf("khadas_i2c_write error!!!\n");

	return ret;
}

static int kbi_i2c_read(u8 reg)
{
	int ret;
	char val[64];
	uchar linebuf[1];

	ret = khadas_i2c_read(CHIP_ADDR, reg, linebuf);
	if (ret)
		printf("Error reading the chip: %d\n",ret);
	else {
		sprintf(val, "%d", linebuf[0]);
		ret = simple_strtoul(val, NULL, 10);

	}

	return ret;

}

static void  kbi_i2c_read_block(uint start_reg, int count, int val[])
{
	int ret;
	int nbytes;
	uint reg;

	nbytes = count;
	reg = start_reg;
	do {
		u8 value;
		ret = khadas_i2c_read(CHIP_ADDR, reg, &value);
		if (ret)
			printf("Error reading block the chip: %d\n",ret);
		else
			val[count-nbytes] =  value;

		reg++;
		nbytes--;

	} while (nbytes > 0);

}

static unsigned char chartonum(char c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'A' && c <= 'F')
		return (c - 'A') + 10;
	if (c >= 'a' && c <= 'f')
		return (c - 'a') + 10;

	return 0;
}

static int get_forcereset_wol(bool is_print)
{
	int enable;

	enable = kbi_i2c_read(REG_BOOT_EN_WOL);
	if (is_print)
	printf("wol forcereset: %s\n", enable&0x02 ? "enable":"disable");
	env_set("wol_forcereset", enable&0x02 ? "1" : "0");

	return enable;

}

static int get_wol(bool is_print)
{
	int enable;

	enable = kbi_i2c_read(REG_BOOT_EN_WOL);
	if (is_print)
	printf("boot wol: %s\n", enable&0x01 ? "enable":"disable");
	env_set("wol_enable", enable&0x01 ?"1" : "0");

	return enable;
}

static void set_wol(bool is_shutdown, int enable)
{
	char cmd[64];
	int mode;

	if ((enable&0x01) != 0) {

	int mac_addr[MAC_LENGHT] = {0};
	if (is_shutdown) {
		run_command("phyreg w 31 0", 0);
		run_command("phyreg w 0 0", 0);
	} else {
		run_command("phyreg w 31 0", 0);
		run_command("phyreg w 0 0x1040", 0);
	}

	mode = kbi_i2c_read(REG_MAC_SWITCH);
	if (mode == 1) {
		kbi_i2c_read_block(REG_MAC, MAC_LENGHT, mac_addr);
	} else {
		run_command("efuse mac", 0);
		char *s = env_get("eth_mac");
		if ((s != NULL) && (strcmp(s, "00:00:00:00:00:00") != 0)) {
			printf("getmac = %s\n", s);
			int i = 0;
			for (i = 0; i < 6 && s[0] != '\0' && s[1] != '\0'; i++) {
				mac_addr[i] = chartonum(s[0]) << 4 | chartonum(s[1]);
				s +=3;
			}
		} else {
			//		kbi_i2c_read_block(REG_MAC, MAC_LENGHT, mac_addr);
					int t = 0;
					for(t = 0; t < 6; t++){
						mac_addr[t] = 0x00;
					}
			}
	}

	run_command("phyreg w 31 0xd8c", 0);
	sprintf(cmd, "phyreg w 16 0x%02x%02x", mac_addr[1], mac_addr[0]);
	run_command(cmd, 0);
	sprintf(cmd, "phyreg w 17 0x%02x%02x", mac_addr[3], mac_addr[2]);
	run_command(cmd, 0);
	sprintf(cmd, "phyreg w 18 0x%02x%02x", mac_addr[5], mac_addr[4]);
	run_command(cmd, 0);
	run_command("phyreg w 31 0", 0);

	run_command("phyreg w 31 0xd8a", 0);
	run_command("phyreg w 16 0x1000", 0);
	run_command("phyreg w 17 0x9fff", 0);
	run_command("phyreg w 31 0", 0);

	run_command("phyreg w 31 0xd8a", 0);
	run_command("phyreg w 19 0x8002", 0);
	run_command("phyreg w 31 0", 0);

	run_command("phyreg w 31 0xd40", 0);
	run_command("phyreg w 22 0x20", 0);
	run_command("phyreg w 31 0", 0);
  } else {
	run_command("phyreg w 31 0xd8a", 0);
	run_command("phyreg w 16 0", 0);
	run_command("phyreg w 17 0x7fff", 0);
	run_command("phyreg w 19 0", 0);
	run_command("phyreg w 31 0", 0);

	run_command("phyreg w 31 0xd40", 0);
	run_command("phyreg w 22 0", 0);
	run_command("phyreg w 31 0", 0);
  }

	sprintf(cmd, "i2c mw %x %x.1 %d 1", CHIP_ADDR, REG_BOOT_EN_WOL, enable);
	run_command(cmd, 0);
//	printf("%s: %d\n", __func__, enable);
}

static void get_version(void)
{
	int version[VERSION_LENGHT] = {};
	int i;

	kbi_i2c_read_block(REG_VERSION, VERSION_LENGHT, version);
	printf("version: ");
	for (i=0; i< VERSION_LENGHT; i++) {
		printf("%x",version[i]);
	}
	printf("\n");
}

static void get_mac(void)
{
	run_command("efuse mac", 0);
	char *temp = env_get("eth_mac");
	if (temp != NULL) {
		printf("mac=%s\n", temp);
	}

	return;
}

// static const char *hw_version_str(int hw_ver)
// {
// 	switch (hw_ver) {
// 		case HW_VERSION_VIM4_V12:
// 			return "VIM4.V12";
// 		case HW_VERSION_VIM1S_V10:
// 			return "VIM1S.V10";
// 		default:
// 			return "Unknow";
// 	}
// }

// static int get_hw_version(void)
// {
// 	unsigned val = 0;
// 	int hw_ver = 0;
// 	int ret;
// 	struct udevice *dev;
// 	int current_channel = 1;
// 	unsigned int current_mode = ADC_MODE_AVERAGE;

// 	ret = uclass_get_device_by_name(UCLASS_ADC, "adc", &dev);
// 	if(ret)
// 		return ret;
// 	ret = adc_set_mode(dev, current_channel, current_mode);
// 	if(ret) {
// 			pr_err("current platform does not support mode\n");
// 			return ret;
// 	}
// 	udelay(100);
// 	ret = adc_channel_single_shot_mode("adc", current_mode,
//                        current_channel, &val);
// 	if(ret)
// 			return ret;
// 	printf("SARADC channel(%d) is %d.\n", current_channel, val);

// 	if ((val >= HW_VERSION_ADC_VAL_VIM4_V12 - HW_VERSION_ADC_VALUE_TOLERANCE) && (val <= HW_VERSION_ADC_VAL_VIM4_V12 + HW_VERSION_ADC_VALUE_TOLERANCE)) {
// 			hw_ver = HW_VERSION_VIM4_V12;
// 	} else if ((val >= HW_VERSION_ADC_VAL_VIM1S_V10 - HW_VERSION_ADC_VALUE_TOLERANCE) && (val <= HW_VERSION_ADC_VAL_VIM1S_V10 + HW_VERSION_ADC_VALUE_TOLERANCE)) {
// 			hw_ver = HW_VERSION_VIM1S_V10;
// 	} else {
// 				hw_ver = HW_VERSION_UNKNOW;
// 	}
// 	printf("saradc: 0x%x, hw_ver: 0x%x (%s)\n", val, hw_ver, hw_version_str(hw_ver));
// 	env_set("hwver", hw_version_str(hw_ver));
// 	current_channel = -1;

// 	return 0;
// }

static void get_usid(int is_print)
{
	run_command("efuse usid", 0);
	char *temp = env_get("usid");
	if (temp != NULL) {\
		printf("usid=%s\n", temp);
	}

	return;
}

static void get_power_state(void)
{
	int val;

	val = kbi_i2c_read(REG_POWER_STATE);
	if (val == 0) {
		printf("normal power on\n");
		env_set("power_state","0");
	} else if (val == 1) {
		printf("abort power off\n");
		env_set("power_state","1");
	} else if (val == 2) {
		printf("normal power off\n");
		env_set("power_state","2");
	} else {
		printf("state err\n");
		env_set("power_state","f");
	}
}

static void set_bootmode(int mode)
{
	char cmd[64];

	sprintf(cmd, "i2c mw %x %x.1 %d 1",CHIP_ADDR, REG_BOOT_MODE, mode);
	run_command(cmd, 0);

}

static void get_bootmode(void)
{
	int mode;

	mode = kbi_i2c_read(REG_BOOT_MODE);
	if (mode == BOOT_MODE_EMMC) {
		printf("bootmode: emmc\n");
	} else if (mode == BOOT_MODE_SPI) {
		printf("bootmode: spi\n");
	} else {
		printf("bootmode err: %d\n",mode);
	}
}

static void get_rtc(void)
{
	int enable;

	enable = kbi_i2c_read(REG_BOOT_EN_RTC);
	printf("boot rtc: %s\n", enable==1 ? "enable" : "disable" );
}

static void set_rtc(int enable)
{
	char cmd[64];

	sprintf(cmd, "i2c mw %x %x.1 %d 1",CHIP_ADDR, REG_BOOT_EN_RTC, enable);
	run_command(cmd, 0);

}

static void get_dcin(void)
{
	int enable;

	enable = kbi_i2c_read(REG_BOOT_EN_DCIN);
	printf("boot dcin: %s\n", enable==1 ? "enable" : "disable" );
}

static void set_dcin(int enable)
{
	char cmd[64];

	sprintf(cmd, "i2c mw %x %x.1 %d 1",CHIP_ADDR, REG_BOOT_EN_DCIN, enable);
	run_command(cmd, 0);
}

static void get_switch_mac(void)
{
	int mode;

	mode = kbi_i2c_read(REG_MAC_SWITCH);
	printf("switch mac from %d\n", mode);
	env_set("switch_mac", mode==1 ? "1" : "0");
}

static void set_switch_mac(int mode)
{
	char cmd[64];

	sprintf(cmd, "i2c mw %x %x %d 1",CHIP_ADDR, REG_MAC_SWITCH, mode);
	printf("set_switch_mac :%d\n", mode);
	run_command(cmd, 0);
	env_set("switch_mac", mode==1 ? "1" : "0");
}

static void get_key(void)
{
	int enable;

	enable = kbi_i2c_read(REG_BOOT_EN_KEY);
	printf("boot key: %s\n", enable==1 ? "enable" : "disable" );
}
static void set_key(int enable)
{
	char cmd[64];

	sprintf(cmd, "i2c mw %x %x.1 %d 1",CHIP_ADDR, REG_BOOT_EN_KEY, enable);
	run_command(cmd, 0);
}

static int get_forcereset_gpio(bool is_print)
{
	int enable;

	enable = kbi_i2c_read(REG_BOOT_EN_GPIO);
	if (is_print)
	printf("gpio forcereset: %s\n", enable&0x02 ? "enable" : "disable" );

	return enable;
}

static int get_gpio(bool is_print)
{
	int enable;

	enable = kbi_i2c_read(REG_BOOT_EN_GPIO);
	if (is_print)
	printf("boot gpio: %s\n", enable&0x01 ? "enable" : "disable" );

	return enable;
}

static void set_gpio(int enable)
{
	char cmd[64];

	sprintf(cmd, "i2c mw %x %x.1 %d 1",CHIP_ADDR, REG_BOOT_EN_GPIO, enable);
	run_command(cmd, 0);
}

static void get_boot_enable(int type)
{
	if (type == BOOT_EN_WOL)
		get_wol(true);
	else if (type == BOOT_EN_RTC)
		get_rtc();
	else if (type == BOOT_EN_DCIN)
		get_dcin();
	else if (type == BOOT_EN_KEY)
		get_key();
	else
		get_gpio(true);
}

static void set_boot_enable(int type, int enable)
{
	int state = 0;

	if (type == BOOT_EN_WOL)
	{
		state = get_wol(false);
		set_wol(false, enable|(state&0x02));
	}
	else if (type == BOOT_EN_RTC)
		set_rtc(enable);
	else if (type == BOOT_EN_DCIN)
		set_dcin(enable);
	else if (type == BOOT_EN_KEY)
		set_key(enable);
	else {
		state = get_gpio(false);
		set_gpio(enable|(state&0x02));
	}
}

static void get_forcereset_enable(int type)
{
	if (type == FORCERESET_GPIO)
		get_forcereset_gpio(true);
	else if (type == FORCERESET_WOL)
		get_forcereset_wol(true);
	else
		printf("get forcereset err=%d\n", type);
}

static int set_forcereset_enable(int type, int enable)
{
	int state = 0;

	if (type == FORCERESET_GPIO)
	{
		state = get_forcereset_gpio(false);
		set_gpio((state&0x01)|(enable<<1));
	}
	else if (type == FORCERESET_WOL)
	{
		state = get_forcereset_wol(false);
		set_wol(false, (state&0x01)|(enable<<1));
	} else {
		printf("set forcereset err=%d\n", type);
		return CMD_RET_USAGE;
	}

	return 0;
}

static void get_blue_led_mode(int type)
{
	int mode;

	if (type == LED_SYSTEM_OFF) {
		mode = kbi_i2c_read(REG_LED_SYSTEM_OFF_MODE);
		if ((mode >= 0) && (mode <=3) )
		printf("led mode: %s  [systemoff]\n",LED_MODE_STR[mode]);
		else
		printf("read led mode err\n");
	}
	else {
		mode = kbi_i2c_read(REG_LED_SYSTEM_ON_MODE);
		if ((mode >= LED_OFF_MODE) && (mode <= LED_HEARTBEAT_MODE))
		printf("led mode: %s  [systemon]\n",LED_MODE_STR[mode]);
		else
		printf("read led mode err\n");
	}
}

static int set_blue_led_mode(int type, int mode)
{
	char cmd[64];

	if (type == LED_SYSTEM_OFF) {
		sprintf(cmd, "i2c mw %x %x.1 %d 1",CHIP_ADDR, REG_LED_SYSTEM_OFF_MODE, mode);
	} else if (type == LED_SYSTEM_ON) {
		sprintf(cmd, "i2c mw %x %x.1 %d 1",CHIP_ADDR, REG_LED_SYSTEM_ON_MODE, mode);
	} else {
		return CMD_RET_USAGE;
	}

	run_command(cmd, 0);

	return 0;
}

static int do_kbi_init(struct cmd_tbl * cmdtp, int flag, int argc, char * const argv[])
{

	return 0;
}

static int do_kbi_resetflag(struct cmd_tbl * cmdtp, int flag, int argc, char * const argv[])
{
	if (argc < 2)
		return CMD_RET_USAGE;

	if (strcmp(argv[1], "1") == 0) {
		run_command("i2c mw 0x18 0x87.1 1 1", 0);
	} else if (strcmp(argv[1], "0") == 0) {
		run_command("i2c mw 0x18 0x87.1 0 1", 0);
	} else {
		return CMD_RET_USAGE;
	}

	return 0;
}

static int do_kbi_version(struct cmd_tbl * cmdtp, int flag, int argc, char * const argv[])
{
	get_version();

	return 0;

}

static int do_kbi_usid(struct cmd_tbl * cmdtp, int flag, int argc, char * const argv[])
{
	if (argc == 2) {
		if (strcmp(argv[1], "noprint") == 0) {
			get_usid(0);
			return 0;
		}
	}
	get_usid(1);

	return 0;
}

static int do_kbi_powerstate(struct cmd_tbl * cmdtp, int flag, int argc, char * const argv[])
{
	get_power_state();

	return 0;

}

static int do_kbi_ethmac(struct cmd_tbl * cmdtp, int flag, int argc, char * const argv[])
{
	get_mac();

	return 0;
}

// static int do_kbi_hwver(struct cmd_tbl * cmdtp, int flag, int argc, char * const argv[])
// {
// 	return get_hw_version();
// }

// static int do_kbi_forcebootsd(struct cmd_tbl * cmdtp, int flag, int argc, char * const argv[])
// {
// 	set_boot_first_timeout(SCPI_CMD_SDCARD_BOOT);

// 	run_command("reboot", 0);

// 	return 0;
// }

static int do_kbi_switchmac(struct cmd_tbl * cmdtp, int flag, int argc, char * const argv[])
{

	if (argc < 2)
		return CMD_RET_USAGE;

	if (strcmp(argv[1], "w") == 0) {
		if (argc < 3)
			return CMD_RET_USAGE;

		if (strcmp(argv[2], "0") == 0) {
			set_switch_mac(0);
		} else if (strcmp(argv[2], "1") == 0) {
			set_switch_mac(1);
		} else {
			return CMD_RET_USAGE;
		}
	} else if (strcmp(argv[1], "r") == 0) {
		get_switch_mac();
	} else {
		return CMD_RET_USAGE;
	}

	return 0;
}

static int do_kbi_led(struct cmd_tbl * cmdtp, int flag, int argc, char * const argv[])
{
	int ret = 0;

	if (argc < 3)
		return CMD_RET_USAGE;

	if (strcmp(argv[1], "systemoff") ==0) {
		if (strcmp(argv[2], "r") == 0) {
			get_blue_led_mode(LED_SYSTEM_OFF);
		} else if (strcmp(argv[2], "w") == 0) {
			if (argc < 4)
				return CMD_RET_USAGE;
			if (strcmp(argv[3], "breathe") == 0) {
				ret = set_blue_led_mode(LED_SYSTEM_OFF, LED_BREATHE_MODE);
			} else if (strcmp(argv[3], "heartbeat") == 0) {
				ret = set_blue_led_mode(LED_SYSTEM_OFF, LED_HEARTBEAT_MODE);
			} else if (strcmp(argv[3], "on") == 0) {
				ret = set_blue_led_mode(LED_SYSTEM_OFF, LED_ON_MODE);
			} else if (strcmp(argv[3], "off") == 0) {
				ret = set_blue_led_mode(LED_SYSTEM_OFF, LED_OFF_MODE);
			} else {
				ret =  CMD_RET_USAGE;
			}
		}
	} else if (strcmp(argv[1], "systemon") ==0) {

		if (strcmp(argv[2], "r") == 0) {
			get_blue_led_mode(LED_SYSTEM_ON);
		} else if (strcmp(argv[2], "w") == 0) {
			if (argc <4)
				return CMD_RET_USAGE;
			if (strcmp(argv[3], "breathe") == 0) {
				ret = set_blue_led_mode(LED_SYSTEM_ON, LED_BREATHE_MODE);
			} else if (strcmp(argv[3], "heartbeat") == 0) {
				ret = set_blue_led_mode(LED_SYSTEM_ON, LED_HEARTBEAT_MODE);
			} else if (strcmp(argv[3], "on") == 0) {
				ret = set_blue_led_mode(LED_SYSTEM_ON, LED_ON_MODE);
			} else if (strcmp(argv[3], "off") == 0) {
				ret = set_blue_led_mode(LED_SYSTEM_ON, LED_OFF_MODE);
			} else {
				ret =  CMD_RET_USAGE;
			}
		}

	} else {
		return CMD_RET_USAGE;
	}

	return ret;
}

static int do_kbi_forcereset(struct cmd_tbl * cmdtp, int flag, int argc, char * const argv[])
{
	int ret = 0;

	if (argc < 3)
		return CMD_RET_USAGE;

	if (strcmp(argv[1], "wol") ==0) {
		if (strcmp(argv[2], "r") == 0) {
			get_forcereset_enable(FORCERESET_WOL);
		} else if (strcmp(argv[2], "w") == 0) {
			if (argc < 4)
				return CMD_RET_USAGE;
			if (strcmp(argv[3], "1") == 0) {
				ret = set_forcereset_enable(FORCERESET_WOL, 1);
			} else if (strcmp(argv[3], "0") == 0) {
				ret = set_forcereset_enable(FORCERESET_WOL, 0);
			} else {
				ret =  CMD_RET_USAGE;
			}
		}
	} else if (strcmp(argv[1], "gpio") ==0) {

		if (strcmp(argv[2], "r") == 0) {
			get_forcereset_enable(FORCERESET_GPIO);
		} else if (strcmp(argv[2], "w") == 0) {
			if (argc <4)
				return CMD_RET_USAGE;
			if (strcmp(argv[3], "1") == 0) {
				ret = set_forcereset_enable(FORCERESET_GPIO, 1);
			} else if (strcmp(argv[3], "0") == 0) {
				ret = set_forcereset_enable(FORCERESET_GPIO, 0);
			} else {
				ret =  CMD_RET_USAGE;
			}
		}

	} else {
		return CMD_RET_USAGE;
	}

	return ret;
}

static int do_kbi_wolreset(struct cmd_tbl * cmdtp, int flag, int argc, char * const argv[])
{
	run_command("phyreg w 31 0xd8a", 0);
	run_command("phyreg w 16 0", 0);
	run_command("phyreg w 17 0x7fff", 0);
	run_command("phyreg w 19 0", 0);
	run_command("phyreg w 31 0", 0);

	run_command("phyreg w 31 0xd40", 0);
	run_command("phyreg w 22 0", 0);
	run_command("phyreg w 31 0", 0);

	return 0;
}

static int do_kbi_poweroff(struct cmd_tbl * cmdtp, int flag, int argc, char * const argv[])
{
	char cmd[64];

	printf("%s\n",__func__);
	int enable = get_wol(false);
	if ((enable&0x03) != 0)
		set_wol(true, enable);
	sprintf(cmd, "i2c mw %x %x.1 %d 1", CHIP_ADDR, REG_POWER_OFF, 1);
	run_command(cmd, 0);

	return 0;
}

static int do_kbi_trigger(struct cmd_tbl * cmdtp, int flag, int argc, char * const argv[])
{
	if (argc < 3)
		return CMD_RET_USAGE;

	if (strcmp(argv[2], "r") == 0) {

		if (strcmp(argv[1], "wol") == 0)
			get_boot_enable(BOOT_EN_WOL);
		else if (strcmp(argv[1], "rtc") == 0)
			get_boot_enable(BOOT_EN_RTC);
		else if (strcmp(argv[1], "dcin") == 0)
			get_boot_enable(BOOT_EN_DCIN);
		else if (strcmp(argv[1], "key") == 0)
			get_boot_enable(BOOT_EN_KEY);
		else if (strcmp(argv[1], "gpio") == 0)
		    get_boot_enable(BOOT_EN_GPIO);
		else
			return CMD_RET_USAGE;
	} else if (strcmp(argv[2], "w") == 0) {
		if (argc < 4)
			return CMD_RET_USAGE;
		if ((strcmp(argv[3], "1") != 0) && (strcmp(argv[3], "0") != 0))
			return CMD_RET_USAGE;

		if (strcmp(argv[1], "wol") == 0) {

			if (strcmp(argv[3], "1") == 0)
				set_boot_enable(BOOT_EN_WOL, 1);
			else
				set_boot_enable(BOOT_EN_WOL, 0);

	    } else if (strcmp(argv[1], "rtc") == 0) {

			if (strcmp(argv[3], "1") == 0)
				set_boot_enable(BOOT_EN_RTC, 1);
			else
				set_boot_enable(BOOT_EN_RTC, 0);

		} else if (strcmp(argv[1], "dcin") == 0) {

			if (strcmp(argv[3], "1") == 0)
				set_boot_enable(BOOT_EN_DCIN, 1);
			else
				set_boot_enable(BOOT_EN_DCIN, 0);

		} else if (strcmp(argv[1], "key") == 0) {

			if (strcmp(argv[3], "1") == 0)
				set_boot_enable(BOOT_EN_KEY, 1);
			else
				set_boot_enable(BOOT_EN_KEY, 0);

		} else if (strcmp(argv[1], "gpio") == 0) {

			if (strcmp(argv[3], "1") == 0)
				set_boot_enable(BOOT_EN_GPIO, 1);
			else
				set_boot_enable(BOOT_EN_GPIO, 0);

		} else {
			return CMD_RET_USAGE;

		}
	} else {

		return CMD_RET_USAGE;
	}

	return 0;
}

static int do_kbi_bootmode(struct cmd_tbl * cmdtp, int flag, int argc, char * const argv[])
{
	if (argc < 2)
		return CMD_RET_USAGE;
	if (strcmp(argv[1], "w") == 0) {
		if (argc < 3)
			return CMD_RET_USAGE;
		if (strcmp(argv[2], "emmc") == 0) {
			set_bootmode(BOOT_MODE_EMMC);
		} else if (strcmp(argv[2], "spi") == 0) {
			set_bootmode(BOOT_MODE_SPI);
		} else {
			return CMD_RET_USAGE;
		}
	} else if (strcmp(argv[1], "r") == 0) {

		get_bootmode();

	} else {
		return CMD_RET_USAGE;
	}

	return 0;
}

static struct cmd_tbl cmd_kbi_sub[] = {
	U_BOOT_CMD_MKENT(init, 1, 1, do_kbi_init, "", ""),
	U_BOOT_CMD_MKENT(resetflag, 2, 1, do_kbi_resetflag, "", ""),
	U_BOOT_CMD_MKENT(usid, 1, 1, do_kbi_usid, "", ""),
	U_BOOT_CMD_MKENT(version, 1, 1, do_kbi_version, "", ""),
	U_BOOT_CMD_MKENT(powerstate, 1, 1, do_kbi_powerstate, "", ""),
	U_BOOT_CMD_MKENT(ethmac, 1, 1, do_kbi_ethmac, "", ""),
	// U_BOOT_CMD_MKENT(hwver, 1, 1, do_kbi_hwver, "", ""),
	U_BOOT_CMD_MKENT(poweroff, 1, 1, do_kbi_poweroff, "", ""),
	U_BOOT_CMD_MKENT(switchmac, 3, 1, do_kbi_switchmac, "", ""),
	U_BOOT_CMD_MKENT(led, 4, 1, do_kbi_led, "", ""),
	U_BOOT_CMD_MKENT(trigger, 4, 1, do_kbi_trigger, "", ""),
	U_BOOT_CMD_MKENT(bootmode, 3, 1, do_kbi_bootmode, "", ""),
	U_BOOT_CMD_MKENT(wolreset, 1, 1, do_kbi_wolreset, "", ""),
	U_BOOT_CMD_MKENT(forcereset, 4, 1, do_kbi_forcereset, "", ""),
	// U_BOOT_CMD_MKENT(forcebootsd, 1, 1, do_kbi_forcebootsd, "", ""),
};

static int do_kbi(struct cmd_tbl * cmdtp, int flag, int argc, char * const argv[])
{
	struct cmd_tbl *c;
	mcu_i2c_probe(MCU_I2C_BUS_NUM);
	run_command("i2c dev 0", 0);
#if defined(CONFIG_KHADAS_VIM4)
		run_command("i2c dev 6", 0);
#elif defined(CONFIG_KHADAS_VIM1S)
		run_command("i2c dev 1", 0);
#endif

	if (argc < 2)
		return CMD_RET_USAGE;

	/* Strip off leading 'kbi' command argument */
	argc--;
	argv++;

	c = find_cmd_tbl(argv[0], &cmd_kbi_sub[0], ARRAY_SIZE(cmd_kbi_sub));

	if (c)
		return c->cmd(cmdtp, flag, argc, argv);
	else
		return CMD_RET_USAGE;

}

static char kbi_help_text[] =
		"[function] [mode] [write|read] <value>\n"
		"\n"
		"kbi version - read version information\n"
		"kbi usid - read usid information\n"
		"kbi powerstate - read power on state\n"
		"kbi poweroff - power off device\n"
		"kbi ethmac - read ethernet mac address\n"
		// "kbi hwver - read board hardware version\n"
		"\n"
		"kbi led [systemoff|systemon] w <off|on|breathe|heartbeat> - set blue led mode\n"
		"kbi led [systemoff|systemon] r - read blue led mode\n"
		"\n"
		"kbi forcereset [wol|gpio] w <0|1> - disable/enable force-reset\n"
		"kbi forcereset [wol|gpio] r - read state of force-reset\n"
		"[notice: the wol|gpio boot trigger must be enabled if you want to enable force-reset]\n"
		"\n"
		"kbi bootmode w <emmc|spi> - set bootmode to emmc or spi\n"
		"kbi bootmode r - read current bootmode\n"
		"\n"
		// "kbi forcebootsd\n"
		"kbi wolreset\n"
		"\n"
		"kbi trigger [wol|rtc|dcin|key|gpio] w <0|1> - disable/enable boot trigger\n"
		"kbi trigger [wol|rtc|dcin|key|gpio] r - read mode of a boot trigger";

U_BOOT_CMD(
		kbi, 6, 1, do_kbi,
		"Khadas Bootloader Instructions sub-system",
		kbi_help_text
);
