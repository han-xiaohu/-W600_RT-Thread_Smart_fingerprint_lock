#include <rtthread.h>
#include <rtdevice.h>
#include "board.h"
#include "user_def.h"

#define DBG_TAG "finger_control"
#define DBG_LVL DBG_LOG
#include <rtdbg.h>


/* ����������� */
#define DETECT_PIN     14
#define POWERON_PIN    28


/* �ź��� */
static struct rt_semaphore detect_sem;
static struct rt_semaphore rx_sem;

extern struct rt_semaphore door_open_sem;
extern struct rt_mailbox mb_led;
extern struct rt_messagequeue mq_send_onenet;


#define SAMPLE_UART_NAME  		"uart1"
#define DATA_CMD_END      		0xF5       /* ����λ */
#define ONE_DATA_MAXLEN   		20         /* ���������ݵ���󳤶� */
static rt_device_t serial;		


/* ģ���������� */
#define DATA_START			0xf5	//���ݰ���ʼ
#define DATA_END			0xf5	//���ݰ�����
#define CMD_SEARCH  		0x0c	//1:N�ȶ�
#define BUF_N 	8

rt_uint8_t rx_buffer[ONE_DATA_MAXLEN];   //���շ�����Ϣ
rt_uint8_t tx_buffer[ONE_DATA_MAXLEN];   //���������������

rt_uint8_t data_rx_end;       //���շ�����Ϣ������־
rt_uint8_t rx_flag = 1;  			 //���ձ�־λ


//�ȴ����ݰ��������
rt_uint8_t FPC1020_WaitData(void)
{
	rt_uint8_t i=0;
	for(i=30; i>0; i--)//�ȴ�ָ��оƬ����
	{
		rt_thread_mdelay(50);
		if(data_rx_end)
		{
			break;
		}
	}

	if(i==0)return RT_FALSE;//ָ��оƬû�з���
	else return RT_TRUE;
}


//����У��ֵ
rt_uint8_t CmdGenCHK(rt_uint8_t wLen,rt_uint8_t *ptr)
{
	rt_uint8_t i,temp = 0;
	
	for(i = 0; i < wLen; i++)
	{
		temp ^= *(ptr + i);
	}
	return temp;
}

//���Ϳ���ָ��оƬָ��
void FPC1020_SendPackage(rt_uint8_t wLen ,rt_uint8_t *ptr)
{
	unsigned int i=0,len=0;
	rx_flag=1;
	tx_buffer[0] = DATA_START; //ָ���
	for(i = 0; i < wLen; i++) // data in packet 
	{
		tx_buffer[1+i] = *(ptr+i);
	} 

	tx_buffer[wLen + 1] = CmdGenCHK(wLen, ptr); //Generate checkout data
	tx_buffer[wLen + 2] = DATA_END;
	len = wLen + 3;

	data_rx_end = 0;

	rt_device_write(serial, 0, tx_buffer, len);
	
}

//����������
rt_uint8_t FPC1020_CheckPackage(rt_uint8_t cmd,rt_uint8_t *q1,rt_uint8_t *q2,rt_uint8_t *q3)
{
	rt_uint8_t flag = RT_FALSE;
	rx_flag=1;
	if(!FPC1020_WaitData())
	{
		return flag; //�ȴ����շ�����Ϣ
	}
	if(data_rx_end) 
	{
		data_rx_end = 0;//�����ݰ����ձ�־
	}
	else 
	{
		return flag;
	}

	if(rx_buffer[0] != DATA_START) return flag;
	if(rx_buffer[1] != cmd) return flag;
	
	if(cmd == CMD_SEARCH)
	{
		if((1 == rx_buffer[4])||(2 == rx_buffer[4])||(3 == rx_buffer[4]))
		{
			flag = RT_TRUE;
		}
	}
	
	if(flag == RT_TRUE)
	{
		if(q1 != RT_NULL)
			*q1 = rx_buffer[2];
		if(q2 != RT_NULL)
			*q2 = rx_buffer[3];
		if(q3 != RT_NULL)
			*q3 = rx_buffer[4];
	}
	
	
	return flag;
}

//�������
rt_uint8_t FPC1020_CheckStart(void)
{
	rt_uint8_t flag = RT_TRUE;
	rx_flag=1;
	if(FPC1020_WaitData()) 
	{
		if(data_rx_end)
		{
			rx_flag=1;
			data_rx_end = 0; //�����ݰ����ձ�־
		}
		if(rx_buffer[0] == DATA_START)
		{
			return RT_TRUE;
		}
	}
	else
	{
		return RT_FALSE; 
	}

	return flag;
}

//����ָ�� 1:N
void FP_Search(void)
{
  rt_uint8_t buf[BUF_N];
  
  *buf = CMD_SEARCH;         
  *(buf+1) = 0x00;
  *(buf+2) = 0x00;
  *(buf+3) = 0x00;
  *(buf+4) = 0x00;

  FPC1020_SendPackage(5, buf);
}


//����ָ�� 1:N
rt_uint8_t FPC1020_Search(rt_uint8_t *q1, rt_uint8_t *q2, rt_uint8_t *q3)
{

	FP_Search();
	return FPC1020_CheckPackage(CMD_SEARCH, q1, q2, q3);
}



/* ��������жϻص����� */
void detect_pin_ind(void *args)
{
	if(rt_pin_read(DETECT_PIN)){
		LOG_D("finger detected!\n");
		rt_pin_write(POWERON_PIN, PIN_HIGH);
		rt_sem_release(&detect_sem);
	}
}


/* �������ݻص����� */
static rt_err_t uart_rx_ind(rt_device_t dev, rt_size_t size)
{
    /* ���ڽ��յ����ݺ�����жϣ����ô˻ص�������Ȼ���ͽ����ź��� */
    if (size > 0)
    {
        rt_sem_release(&rx_sem);
    }
    return RT_EOK;
}

static char uart_sample_get_char(void)
{
    char ch;

    while (rt_device_read(serial, 0, &ch, 1) == 0)
    {
        rt_sem_control(&rx_sem, RT_IPC_CMD_RESET, RT_NULL);
        rt_sem_take(&rx_sem, RT_WAITING_FOREVER);
    }
    return ch;
}

/* ���ݽ����߳� */
static void data_parsing(void)
{
    rt_uint8_t ch;
    static rt_uint8_t i = 0;

    while (1)
    {
        ch = uart_sample_get_char();
        if(rx_flag == 1 && ch == DATA_CMD_END)
        {
			i=0;
			rx_flag=0;
			data_rx_end = 0;
        }
		if(!rx_flag)
		{
			rx_buffer[i++] = ch;
		}

		if(i == 8 && !rx_flag) 
		{
			data_rx_end = 0xFF;
			i=0;
		}
    }
}


/* ָ��ʶ���߳� */
static void finger_identification_entry(void)
{
	rt_uint8_t uid[2] = {0};
	struct onenet_msg mqbuf = {0};
	rt_tick_t last_tick = 0;
	while(1)
	{
		rt_sem_control(&detect_sem, RT_IPC_CMD_RESET, RT_NULL);
        rt_sem_take(&detect_sem, RT_WAITING_FOREVER);
		
		if(FPC1020_WaitData())
		{
			rt_mb_send(&mb_led, LED_RED);
			LOG_D("SCANING...");
			if(FPC1020_Search(&uid[0], &uid[1], RT_NULL))
			{
				rt_sem_release(&door_open_sem);
				LOG_D("SUCCESS!\n");
				
				//����Ƶ����������
				if(rt_tick_get() - last_tick > FINGER_UP_WAIT)
				{
					last_tick = rt_tick_get();
					/* ������Ϣ����Ϣ������ */
					mqbuf.type = FP_MSG;
					mqbuf.id = (rt_uint32_t)((uid[0] << 8) | uid[1]);
					rt_mq_send(&mq_send_onenet, &mqbuf, sizeof(mqbuf));
				}
			}
			else
			{
				rt_mb_send(&mb_led, LED_BLUE);
				LOG_D("FAIL\n");
			}
		}
		rt_pin_write(POWERON_PIN, PIN_LOW);
	}
}



rt_err_t fingerprint_control_init(void)
{
    rt_err_t ret = RT_EOK;

    /* ����ϵͳ�еĴ����豸 */
    serial = rt_device_find(SAMPLE_UART_NAME);
    if (!serial)
    {
        LOG_D("find %s failed!\n", SAMPLE_UART_NAME);
        return RT_ERROR;
    }

    /* ��ʼ���ź��� */
    rt_sem_init(&rx_sem, "rx_sem", 0, RT_IPC_FLAG_FIFO);
	rt_sem_init(&detect_sem, "detect_sem", 0, RT_IPC_FLAG_FIFO);

	
    /* ���жϽ��ռ���ѯ����ģʽ�򿪴����豸 */
    rt_device_open(serial, RT_DEVICE_FLAG_INT_RX);
    /* ���ý��ջص����� */
    rt_device_set_rx_indicate(serial, uart_rx_ind);
	

	/* ��ָ�������Ϊ����ģʽ */
    rt_pin_mode(DETECT_PIN, PIN_MODE_INPUT_PULLDOWN);
    /* ���жϣ��½���ģʽ���ص�������Ϊbeep_on */
    rt_pin_attach_irq(DETECT_PIN, PIN_IRQ_MODE_RISING, detect_pin_ind, RT_NULL);
    /* ʹ���ж� */
    rt_pin_irq_enable(DETECT_PIN, PIN_IRQ_ENABLE);
	
	
	/* ָ��ģ���Դ�������ģʽ */
	rt_pin_mode(POWERON_PIN, PIN_MODE_OUTPUT);
	rt_pin_write(POWERON_PIN, PIN_LOW);
	
	
    /* ���� serial �߳� */
    rt_thread_t thread = rt_thread_create("finger_parsing", (void (*)(void *parameter))data_parsing, RT_NULL, 512, 5, 10); //stack_size 256
    /* �����ɹ��������߳� */
    if (thread != RT_NULL)
    {
        rt_thread_startup(thread);
    }
    else
    {
        ret = RT_ERROR;
    }
	
	thread = rt_thread_create("finger_identification", (void (*)(void *parameter))finger_identification_entry, RT_NULL, 1024, 20, 10); //stack_size 512
	/* �����ɹ��������߳� */
    if (thread != RT_NULL)
    {
        rt_thread_startup(thread);
    }
    else
    {
        ret = RT_ERROR;
    }

    return ret;
}

