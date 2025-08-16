#include <rtthread.h>
#include <rtdevice.h>
#include "lwip/apps/lwiperf.h"
#include "lwip/ip_addr.h"


static const char *report_type_str[] = {
    "TCP_DONE_SERVER (RX)",        /* LWIPERF_TCP_DONE_SERVER_RX,*/
    "TCP_DONE_SERVER (TX)",        /* LWIPERF_TCP_DONE_SERVER_TX,*/
    "TCP_DONE_CLIENT (TX)",        /* LWIPERF_TCP_DONE_CLIENT_TX,*/
    "TCP_DONE_CLIENT (RX)",        /* LWIPERF_TCP_DONE_CLIENT_RX,*/
    "TCP_ABORTED_LOCAL",           /* LWIPERF_TCP_ABORTED_LOCAL, */
    "TCP_ABORTED_LOCAL_DATAERROR", /* LWIPERF_TCP_ABORTED_LOCAL_DATAERROR, */
    "TCP_ABORTED_LOCAL_TXERROR",   /* LWIPERF_TCP_ABORTED_LOCAL_TXERROR, */
    "TCP_ABORTED_REMOTE",          /* LWIPERF_TCP_ABORTED_REMOTE, */
    "UDP_DONE_SERVER (RX)",        /* LWIPERF_UDP_DONE_SERVER_RX, */
    "UDP_DONE_SERVER (TX)",        /* LWIPERF_UDP_DONE_SERVER_TX, */
    "UDP_DONE_CLIENT (TX)",        /* LWIPERF_UDP_DONE_CLIENT_TX, */
    "UDP_DONE_CLIENT (RX)",        /* LWIPERF_UDP_DONE_CLIENT_RX, */
    "UDP_ABORTED_LOCAL",           /* LWIPERF_UDP_ABORTED_LOCAL, */
    "UDP_ABORTED_LOCAL_DATAERROR", /* LWIPERF_UDP_ABORTED_LOCAL_DATAERROR, */
    "UDP_ABORTED_LOCAL_TXERROR",   /* LWIPERF_UDP_ABORTED_LOCAL_TXERROR, */
    "UDP_ABORTED_REMOTE",          /* LWIPERF_UDP_ABORTED_REMOTE, */
};


static void lwiperf_report(void *arg,
                           enum lwiperf_report_type report_type,
                           const ip_addr_t *local_addr,
                           uint16_t local_port,
                           const ip_addr_t *remote_addr,
                           uint16_t remote_port,
                           uint32_t bytes_transferred,
                           uint32_t ms_duration,
                           uint32_t bandwidth_kbitpsec)
{
    rt_kprintf("-------------------------------------------------\r\n");
    if (report_type < (sizeof(report_type_str) / sizeof(report_type_str[0])))
    {
        rt_kprintf(" %s \r\n", report_type_str[report_type]);
        if (local_addr && remote_addr)
        {
            rt_kprintf(" Local address : %u.%u.%u.%u ", ((uint8_t *)local_addr)[0], ((uint8_t *)local_addr)[1],
                   ((uint8_t *)local_addr)[2], ((uint8_t *)local_addr)[3]);
            rt_kprintf(" Port %d \r\n", local_port);
            rt_kprintf(" Remote address : %u.%u.%u.%u ", ((uint8_t *)remote_addr)[0], ((uint8_t *)remote_addr)[1],
                   ((uint8_t *)remote_addr)[2], ((uint8_t *)remote_addr)[3]);
            rt_kprintf(" Port %u \r\n", remote_port);
            rt_kprintf(" Bytes Transferred %u \r\n", bytes_transferred);
            rt_kprintf(" Duration (ms) %u \r\n", ms_duration);
            rt_kprintf(" Bandwidth (kbitpsec) %u \r\n", bandwidth_kbitpsec);
        }
    }
    else
    {
        rt_kprintf(" IPERF Report error\r\n");
    }
    rt_kprintf("\r\n");
}


int test_eth_init()
{
    lwiperf_start_tcp_server_default(lwiperf_report, 0);  
   
	return 0;
}

INIT_APP_EXPORT(test_eth_init);


