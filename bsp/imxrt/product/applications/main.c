/*
 * Copyright (c) 2006-2022, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2019-05-06     tyustli      first version
 *
 */

#include <rtthread.h>

int main(void)
{   
    rt_kprintf("EMC test is running...\n");
    while (1)
    {      
        rt_thread_delay(1000);        
    }
}








