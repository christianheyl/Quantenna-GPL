/**
  Copyright (c) 2008 - 2013 Quantenna Communications Inc
  All Rights Reserved

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

 **/

#ifndef __QDRV_UC_PRINT_H
#define __QDRV_UC_PRINT_H

#include "qdrv_soc.h"

void uc_print_schedule_work(void);
int qdrv_uc_print_init(struct qdrv_cb *qcb);
int qdrv_uc_print_exit(struct qdrv_cb *qcb);

#endif // __QDRV_UC_PRINT_H

