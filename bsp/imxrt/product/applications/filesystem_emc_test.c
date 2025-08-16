#include <rtthread.h>
#include <dfs_file.h>
#include <unistd.h>


#define BUF_SIZE 16*1024
#define FILE_MAX_SIZE 0x80000 //512K bytes
#define TEST_FILE_NAME "/test_fs"

static uint32_t w_file_err = 0;
static uint32_t r_file_err = 0;
static uint32_t o_file_err = 0;
static uint32_t malloc_err = 0;
static uint32_t data_err = 0;
static uint32_t round = 0;

void test_fs(void *para)
{    
    char filename[] = "/test_fs_00";
    uint32_t value = 0;
    uint32_t fs_idx = 0;
    uint32_t wr_file_size;
    uint8_t *buf = NULL;      

    buf = malloc(BUF_SIZE);
    if (NULL == buf) {
        rt_kprintf("malloc error!\n",TEST_FILE_NAME);
        malloc_err++;
        return;
    }

    while(1) {
        snprintf(filename,sizeof(filename),"/test_fs_%02u",fs_idx);
        
        int fd = open(filename,O_CREAT | O_RDWR);        
        if(fd < 0) {
            rt_kprintf("open file %s error!\n",filename);
            o_file_err++;
            return;
        }

        if (round&0x1) {
            memset(buf,0x5A,BUF_SIZE);
            value = 0x5A5A5A5A;
        } else {
            memset(buf,0xA5,BUF_SIZE);
            value = 0xA5A5A5A5;
        }
        for(wr_file_size = 0; wr_file_size < FILE_MAX_SIZE; wr_file_size+=BUF_SIZE) {
            if(write(fd,buf,BUF_SIZE) != BUF_SIZE) {
                w_file_err++;
            }
        }  
        close(fd);

        fd = open(filename,O_RDONLY);
        if (fd < 0) {
            rt_kprintf("open file %s error!\n",filename);
            o_file_err++;
            return;
        }
        for(wr_file_size = 0; wr_file_size < FILE_MAX_SIZE; wr_file_size+=BUF_SIZE) {
            memset(buf,0,BUF_SIZE);
            if(read(fd,buf,BUF_SIZE) != BUF_SIZE) {
                r_file_err++;
            }
            for(uint32_t i = 0; i < BUF_SIZE/4; i++) {
                if (value != *((uint32_t*)buf+i)) {
                    rt_kprintf("data verify failed!\n");
                    data_err++;
                }
            }
            
        }  
        close(fd);

        round++;
        fs_idx++;
        if (fs_idx >= 30) {
            fs_idx = 0;
        }
    }   
}

int test_fs_init()
{
    rt_thread_t tid;

    tid = rt_thread_create("test_fs",
                     test_fs,
                     NULL,
                     2048,
                     25,
                     5);
    if(tid) {
        rt_thread_startup(tid);
    } else {
        rt_kprintf("creak task test_fs fail!\n");
    }

    return 0;
}

INIT_APP_EXPORT(test_fs_init);

void show_fs_stat(int argc, char *argv[]) {
    rt_kprintf("w_file_err=%u\n",w_file_err);
    rt_kprintf("r_file_err=%u\n",r_file_err);
    rt_kprintf("o_file_err=%u\n",o_file_err);  
    rt_kprintf("malloc_err=%u\n",malloc_err); 
    rt_kprintf("data_err=%u\n",data_err); 
    rt_kprintf("round=%u\n",round); 
}

MSH_CMD_EXPORT(show_fs_stat,);



