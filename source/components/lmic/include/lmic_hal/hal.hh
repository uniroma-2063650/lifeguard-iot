/*******************************************************************************
 * Copyright (c) 2015-2016 Matthijs Kooijman
 * Copyright (c) 2016-2026 MCCI Corporation
 *
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v1.0
 * which accompanies this distribution, and is available at
 * http://www.eclipse.org/legal/epl-v10.html
 *
 * This the HAL to run LMIC on top of the Arduino environment.
 *******************************************************************************/
#ifndef _hal_hal_h_
#define _hal_hal_h_

#include "lmic/oslmic_types.h"
#include "configuration.hh"

namespace ESP_IDF_LMIC {

void lmic_hal_wake_task();
void lmic_hal_wake_task_from_isr();

}

#endif // _hal_hal_h_
