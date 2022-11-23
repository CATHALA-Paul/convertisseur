/*
 * Copyright (c) 2022 LAAS-CNRS
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

#include "../src/analog_comm.h"

#include "AnalogCommunication.h"

/////
// Public object to interact with the class

AnalogCommunication analogCommunication;


/////
// Extern variable defined in this module

extern uint16_t broadcast_time;
extern uint16_t control_time;



void AnalogCommunication::init()
{
    analog_comm_init();
}

void AnalogCommunication::triggerAnalogComm()
{
    analog_comm_trigger();
}

float32_t AnalogCommunication::getAnalogCommValue()
{
    return analog_comm_get_value();
}

void AnalogCommunication::setAnalogCommValue(uint32_t analog_bus_value)
{
    analog_comm_set_value(analog_bus_value);
}

