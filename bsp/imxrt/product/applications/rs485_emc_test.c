#include <rtthread.h>
#include <rtdevice.h>

//#define ENABLE_UART8_DMA_TX
#define RX_485_BUF_LEN 512
#define TEST_485_FRAME_LEN 256
#define RS485_UART "uart8"

static rt_sem_t dev485_rx_notice;
static rt_sem_t dev485_tx_notice;
static uint32_t rx_485_cnt = 0;
static uint32_t tx_485_cnt = 0;
static uint32_t tx_485_fail = 0;


static rt_err_t dev485_rx_ind(rt_device_t dev, rt_size_t size) 
{
	return rt_sem_release(dev485_rx_notice);	
}
static rt_err_t dev485_tx_ind(rt_device_t dev, void *buffer) 
{
	return rt_sem_release(dev485_tx_notice);	
}

void test_pingpong_rs485() {
    int ret;
    uint32_t offset = 0;
    rt_size_t recv_len;
    rt_device_t dev485;
    char *ch = malloc(RX_485_BUF_LEN);

    dev485 = rt_device_find(RS485_UART);
    
#ifdef ENABLE_UART8_DMA_TX
    ret = rt_device_open(dev485, RT_DEVICE_OFLAG_RDWR | RT_DEVICE_FLAG_DMA_RX | RT_DEVICE_FLAG_DMA_TX);
#else
    ret = rt_device_open(dev485, RT_DEVICE_OFLAG_RDWR | RT_DEVICE_FLAG_DMA_RX );
#endif  
    if (RT_EOK != ret) {
        rt_kprintf("oepn 485 serial error!\n");
    } else {
        dev485_rx_notice = rt_sem_create("rx_rs485", 0, RT_IPC_FLAG_FIFO);
        dev485_tx_notice = rt_sem_create("tx_rs485", 0, RT_IPC_FLAG_FIFO);
        rt_device_set_rx_indicate(dev485, dev485_rx_ind);
        rt_device_set_tx_complete(dev485, dev485_tx_ind);       
    }
    
    while (1)
    { 
        if(RT_EOK == rt_sem_take(dev485_rx_notice, RT_WAITING_FOREVER)) {
            do 
            {
                recv_len = rt_device_read(dev485, 0, ch+offset, RX_485_BUF_LEN-offset);                
                if(recv_len > 0) {     
                    //rt_kprintf("recv from 485 uart!\n");
                    rx_485_cnt += recv_len;    
                    offset += recv_len;                    
                    if(TEST_485_FRAME_LEN == offset) {
                        if(offset != rt_device_write(dev485, 0, ch, offset)) {
                            tx_485_fail++;
                        } else {
                            tx_485_cnt += offset;
                        }
                        offset = 0;
                    } else if (offset > TEST_485_FRAME_LEN) {
                        rt_kprintf("The peer sends too fast!\n");
                        offset = 0;
                    }  
                }                
            } while(recv_len > 0); 
        }
    }
    
}



int test_rs485_init()
{
     rt_thread_t tid; 
     
     tid = rt_thread_create("test_rs485",
                     test_pingpong_rs485,
                     NULL,
                     1024,
                     RT_THREAD_PRIORITY_MAX / 3 - 1,
                     5);
    if(tid) {
        rt_thread_startup(tid);
    } else {
        rt_kprintf("creak task test_485 fail!\n");
    }

    return 0;
}

INIT_APP_EXPORT(test_rs485_init);

void show_485_stat(int argc, char *argv[]) {
    rt_kprintf("rx_485_cnt=%u\n",rx_485_cnt);
    rt_kprintf("tx_485_cnt=%u\n",tx_485_cnt);
    rt_kprintf("tx_485_fail=%u\n",tx_485_fail);   
    
}

void reset_485_stat(int argc, char *argv[]) {
    rx_485_cnt = 0;
    tx_485_cnt = 0;
    tx_485_fail = 0;
}

MSH_CMD_EXPORT(show_485_stat,);
MSH_CMD_EXPORT(reset_485_stat,);




