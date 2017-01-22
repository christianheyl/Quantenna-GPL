/*
 * (C) Copyright 2012 Quantenna Communications Inc.
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */


#ifndef __BOARD_RUBY_PM_H
#define __BOARD_RUBY_PM_H

#include <linux/module.h>
#include <linux/workqueue.h>
#include <linux/pm_qos_params.h>

#include <common/ruby_pm.h>


int pm_queue_work(struct delayed_work *dwork, unsigned long delay);
int pm_cancel_work(struct delayed_work *dwork);
int pm_flush_work(struct delayed_work *dwork);

#endif // #ifndef __BOARD_RUBY_PM_H

