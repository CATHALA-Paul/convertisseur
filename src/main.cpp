/*
 * Copyright (c) 2021 LAAS-CNRS
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU Lesser General Public License as published by
 *   the Free Software Foundation, either version 2.1 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public License
 *   along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGLPV2.1
 */

/**
 * @brief   This file it the main entry point of the
 *          OwnTech Power API. Please check the README.md
 *          file at the root of this project for basic
 *          information on how to use the Power API,
 *          or refer the the wiki for detailed information.
 *          Wiki: https://gitlab.laas.fr/owntech/power-api/core/-/wikis/home
 *
 * @author  Clément Foucher <clement.foucher@laas.fr>
 */

//-------------OWNTECH DRIVERS-------------------
#include "HardwareConfiguration.h"
#include "DataAcquisition.h"
#include "Scheduling.h"
#include "opalib_control_pid.h"

//------------ZEPHYR DRIVERS----------------------
#include "zephyr.h"
#include "device.h"
#include "drivers/uart.h"
#include "shell/shell.h"
#include <version.h>
#include <logging/log.h>
#include <stdlib.h>
#include <ctype.h>
#include "console/console.h"

//--------------SETUP FUNCTIONS DECLARATION-------------------
void setup_hardware(); //setups the hardware peripherals of the system
void setup_software(); //setups the scheduling of the software and the control method

//-------------LOOP FUNCTIONS DECLARATION----------------------
void loop_communication_task(); //code to be executed in the slow communication task
void loop_application_task();   //code to be executed in the fast application task
void loop_control_task();       //code to be executed in real-time at 20kHz

enum serial_interface_menu_mode //LIST OF POSSIBLE MODES FOR THE OWNTECH CONVERTER
{
    IDLEMODE = 0,
    POWERMODE = 0
};

uint8_t mode = IDLEMODE;
float32_t duty =0.0f;
static bool pwm_enable = false; //[bool] state of the PWM (ctrl task)
static uint32_t control_task_period = 50; //[us] period of the control task


static float32_t V1_low_value; //store value of V1_low (app task)
static float32_t V2_low_value; //store value of V2_low (app task)
static float32_t Vhigh_value; //store value of Vhigh (app task)

static float32_t i1_low_value; //store value of i1_low (app task)
static float32_t i2_low_value; //store value of i2_low (app task)
static float32_t ihigh_value; //store value of ihigh (app task)

static float32_t meas_data; //temp storage meas value (ctrl task)


static int set_bypass(const struct shell *sh, shell_bypass_cb_t bypass)
{
	static bool in_use;

	if (bypass && in_use) {
		shell_error(sh, "Sample supports setting bypass on single instance.");

		return -EBUSY;
	}

	in_use = !in_use;
	if (in_use) {
		shell_print(sh, "Bypass started, press ctrl-x ctrl-q to escape");
		in_use = true;
	}

	shell_set_bypass(sh, bypass);

	return 0;
}

#define CHAR_1 0x18
#define CHAR_2 0x11

static void bypass_cb(const struct shell *sh, uint8_t *data, size_t len)
{
	static uint8_t tail;
	bool escape = false;

	/* Check if escape criteria is met. */
	if (tail == CHAR_1 && data[0] == CHAR_2) {
		escape = true;
	} else {
		for (int i = 0; i < (int)(len - 1); i++) {
			if (data[i] == CHAR_1 && data[i + 1] == CHAR_2) {
				escape = true;
				break;
			}
		}
	}

	if (escape) {
		shell_print(sh, "Exit bypass");
		set_bypass(sh, NULL);
		tail = 0;
		return;
	}

	/* Store last byte for escape sequence detection */
	tail = data[len - 1];

	/* Do the data processing. */
	for (int i = 0; i < (int)len; i++) {
		shell_fprintf(sh, SHELL_INFO, "%02x ", data[i]);
	}
	shell_fprintf(sh, SHELL_INFO, "| ");

	for (int i = 0; i < (int)len; i++) {
		shell_fprintf(sh, SHELL_INFO, "%c", data[i]);
	}
	shell_fprintf(sh, SHELL_INFO, "\n");

}

static int cmd_power_mode(const struct shell *sh, size_t argc, char **argv)
{
    mode = POWERMODE;
    shell_print(sh, "Power mode activated", NULL);
    printk("tototutoutout");

    return 0;
}

static int cmd_idle_mode(const struct shell *sh, size_t argc, char **argv)
{
    mode = IDLEMODE;
    shell_print(sh, "Idle mode activated", NULL);

    return 0;
}

static int cmd_duty_up(const struct shell *sh, size_t argc, char **argv)
{
    duty += 0.05;
    shell_print(sh, "Duty cycle increased at %f", duty);

    return 0;
}

static int cmd_duty_down(const struct shell *sh, size_t argc, char **argv)
{
    duty -= 0.05;
    shell_print(sh, "Duty cycle decreased at %f", duty);

    return 0;
}

static int cmd_board_info(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	shell_print(sh, "Zephyr version %s", KERNEL_VERSION_STRING);
    shell_print(sh, "OwnTech Core API version V1.0", NULL);
    shell_print(sh, "Hardware version : TWIST V1.1.2", NULL);

	return 0;
}

static int cmd_bypass(const struct shell *sh, size_t argc, char **argv)
{
	return set_bypass(sh, bypass_cb);
}

SHELL_CMD_REGISTER(power_mode, NULL, "Enable power", cmd_power_mode);
SHELL_CMD_REGISTER(idle_mode, NULL, "Disable power", cmd_idle_mode);
SHELL_CMD_REGISTER(logging_msg, NULL, "Retrieve logging messages and data", cmd_bypass);
SHELL_CMD_REGISTER(duty_up, NULL, "Increase duty cycle by 0.05", cmd_duty_up);
SHELL_CMD_REGISTER(duty_down, NULL, "Decrease duty cycle by 0.05", cmd_duty_down);
SHELL_CMD_REGISTER(board_info, NULL, "Show software and hardware version", cmd_board_info);


void setup_hardware()
{
    hwConfig.setBoardVersion(TWIST_v_1_1_2);
    hwConfig.configureAdcDefaultAllMeasurements();
    hwConfig.initInterleavedBuckMode();  
    // console_init();
    //setup your hardware here
}

void setup_software()
{
    scheduling.startApplicationTask(loop_application_task);
    scheduling.startControlTask(loop_control_task, control_task_period);
    //setup your software scheduling here
}

void loop_application_task()
{
    while(1){

        if(mode==IDLEMODE) {
            hwConfig.setLedOff();
        }
        else if(mode==POWERMODE) {
            hwConfig.setLedOn();
            printk("%f:", duty);
            printk("%f:", Vhigh_value);
            printk("%f:", V1_low_value);
            printk("%f:", V2_low_value);
            printk("%f:", ihigh_value);
            printk("%f:", i1_low_value);
            printk("%f\n", i2_low_value);
        }      
        k_msleep(100);    
    }
}

void loop_control_task()
{
    meas_data = dataAcquisition.getVHigh();
    if(meas_data!=-10000) Vhigh_value = meas_data;

    meas_data = dataAcquisition.getV1Low();
    if(meas_data!=-10000) V1_low_value = meas_data;

    meas_data = dataAcquisition.getV2Low();
    if(meas_data!=-10000) V2_low_value= meas_data;

    meas_data = dataAcquisition.getIHigh();
    if(meas_data!=-10000) ihigh_value = meas_data;

    meas_data = dataAcquisition.getI1Low();
    if(meas_data!=-10000) i1_low_value = meas_data;

    meas_data = dataAcquisition.getI2Low();
    if(meas_data!=-10000) i2_low_value = meas_data;

    if(mode==IDLEMODE) {
        pwm_enable = false;
        hwConfig.setLedOff();
    }else if(mode==POWERMODE) {
        if(!pwm_enable) {
            pwm_enable = true;
            hwConfig.setLedOn();
        }
    }
}

void main(void)
{
    setup_hardware();
    setup_software();

#if DT_NODE_HAS_COMPAT(DT_CHOSEN(zephyr_shell_uart), st_stm32_lpuart)
	const struct device *dev;
	uint32_t dtr = 0;

	dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_shell_uart));
	if (!device_is_ready(dev)) {
		return;
	}

	// while (!dtr) {
	// 	uart_line_ctrl_get(dev, UART_LINE_CTRL_DTR, &dtr);
	// 	k_sleep(K_MSEC(100));
	// }
#endif
}
