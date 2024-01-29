/*
 * Copyright (c) 2022-2023 LAAS-CNRS
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
 * @date   2023
 * @author Clément Foucher <clement.foucher@laas.fr>
 */


// Current module
#include "scheduling_common.hpp"

// OwnTech Power API
#include "timer.h"
#include "hrtim.h"
#include "DataAPI.h"
#include "data_api_internal.h"

#ifdef CONFIG_OWNTECH_SCHEDULING_TIMING
#include "timing/timing.h"
#endif

/////
// Local variables and constants

// Timer device
static const struct device* timer6 = DEVICE_DT_GET(TIMER6_DEVICE);

// Task status
static task_status_t uninterruptibleTaskStatus = task_status_t::inexistent;

// Interrupt source
static scheduling_interrupt_source_t interrupt_source = source_uninitialized;

// For HRTIM interrupts
static task_function_t user_periodic_task = NULL;

// Data dispatch
static bool do_data_dispatch = false;
static uint32_t task_period = 0;

#ifdef CONFIG_OWNTECH_SCHEDULING_TIMING
/////
// For timing recording - Must be declared as volatile
// to be catched by debugguer
volatile uint32_t record_rise_data_dispatch[2048];
volatile uint32_t record_fall_data_dispatch[2048];
volatile uint32_t record_rise_control_task[2048];
volatile uint32_t record_fall_control_task[2048];
bool reset_i = 0;
bool reset_j = 0;
int i = 0;
int j = 0;
#endif
/////
// Private API

void user_task_proxy()
{
	if (user_periodic_task == NULL) return;

	if (do_data_dispatch == true)
	{
		#ifdef CONFIG_OWNTECH_SCHEDULING_TIMING
		// LL_GPIO_SetOutputPin(GPIOB, LL_GPIO_PIN_2);
		if (i <= 2047) {
			if (i == 0) {timing_start();}
		}
		else if (i >2047 && reset_i == 0){
			i = 0;
			reset_i = 1; //Dump the first array 
		}
		record_rise_data_dispatch[i] = (uint32_t)timing_counter_get();
		#endif

		data_dispatch_do_full_dispatch();

		#ifdef CONFIG_OWNTECH_SCHEDULING_TIMING
		record_fall_data_dispatch[i] = (uint32_t)timing_counter_get();
		#endif
	}

	#ifdef CONFIG_OWNTECH_SCHEDULING_TIMING
	if (j <= 2047) {
		j += 1;
	}		
	else if (j > 2047 && reset_j == 0){
		j = 0;
		reset_j = 1;
	}
	record_rise_control_task[j] = (uint32_t)timing_counter_get();
	#endif

	user_periodic_task();

	#ifdef CONFIG_OWNTECH_SCHEDULING_TIMING
	record_fall_control_task[j] = (uint32_t)timing_counter_get();
	#endif

}

/////
// Public API


void scheduling_set_uninterruptible_synchronous_task_interrupt_source(scheduling_interrupt_source_t int_source)
{
	interrupt_source = int_source;
}

int8_t scheduling_define_uninterruptible_synchronous_task(task_function_t periodic_task, uint32_t task_period_us)
{
	if ( (uninterruptibleTaskStatus != task_status_t::inexistent) && (uninterruptibleTaskStatus != task_status_t::suspended))
		return -1;

	if (periodic_task == NULL)
		return -1;

	if (interrupt_source == source_tim6)
	{
		if (device_is_ready(timer6) == false)
			return -1;

		task_period = task_period_us;
		user_periodic_task = periodic_task;

		// Everything OK, go on with timer configuration
		struct timer_config_t timer_cfg = {0};
		timer_cfg.timer_enable_irq   = 1;
		timer_cfg.timer_irq_callback = user_task_proxy;
		timer_cfg.timer_irq_t_usec   = task_period_us;

		timer_config(timer6, &timer_cfg);

		uninterruptibleTaskStatus = task_status_t::defined;

		return 0;
	}
	else if (interrupt_source == source_hrtim)
	{
		uint32_t hrtim_period_us = hrtim_period_Master_get_us();

		if (hrtim_period_us == 0)
			return -1;

		if (task_period_us % hrtim_period_us != 0)
			return -1;

		uint32_t repetition = task_period_us / hrtim_period_us;

		if (repetition == 0)
			return -1;

		task_period = task_period_us;
		user_periodic_task = periodic_task;
		hrtim_PeriodicEvent_configure(MSTR, repetition, user_task_proxy);

		uninterruptibleTaskStatus = task_status_t::defined;

		return 0;
	}

	return -1;
}

void scheduling_start_uninterruptible_synchronous_task(bool manage_data_acquisition)
{
	if ( (uninterruptibleTaskStatus != task_status_t::defined) && (uninterruptibleTaskStatus != task_status_t::suspended) )
		return;

	if (interrupt_source == scheduling_interrupt_source_t::source_uninitialized)
		return;

	if ( (manage_data_acquisition == true) && (data.started() == false) )
	{
		// If Data Acquisition has not been started yet,
		// then Scheduling will be in charge of data dispatch
		do_data_dispatch = true;

		// Configure Data Acquisition module
		data.setDispatchMethod(DispatchMethod_t::externally_triggered);

		uint32_t repetition;
		if (interrupt_source == scheduling_interrupt_source_t::source_hrtim)
		{
			repetition = hrtim_PeriodicEvent_GetRep(MSTR);
		}
		else // (interrupt_source == scheduling_interrupt_source_t::source_tim6)
		{
			uint32_t hrtim_period_us = hrtim_period_Master_get_us();
			if (hrtim_period_us == 0)
			{
				return;
			}

			repetition = task_period / hrtim_period_us;
		}
		data.setRepetitionsBetweenDispatches(repetition);

		// Then start it
		data.start();
	}

	if (interrupt_source == source_tim6)
	{
		if (device_is_ready(timer6) == false)
			return;

		timer_start(timer6);

		uninterruptibleTaskStatus = task_status_t::running;
	}
	else if (interrupt_source == source_hrtim)
	{
		if (user_periodic_task == NULL)
			return;

		hrtim_PeriodicEvent_en(MSTR);

		uninterruptibleTaskStatus = task_status_t::running;
	}
}

void scheduling_stop_uninterruptible_synchronous_task()
{
	if (uninterruptibleTaskStatus != task_status_t::running)
		return;

	if (interrupt_source == source_tim6)
	{
		if (device_is_ready(timer6) == false)
			return;

		timer_stop(timer6);

		uninterruptibleTaskStatus = task_status_t::suspended;
	}
	else if (interrupt_source == source_hrtim)
	{
		hrtim_PeriodicEvent_dis(MSTR);

		uninterruptibleTaskStatus = task_status_t::suspended;
	}
}
