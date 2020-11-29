#include <rtthread.h>
#include <rtdevice.h>
#include "board.h"
#include "user_def.h"

#define DBG_TAG "door_control"
#define DBG_LVL DBG_LOG
#include <rtdbg.h>


/* ����������� */
#define LED_RED_PIN     18
#define LED_GREEN_PIN   23
#define LED_BLUE_PIN    30
#define BEEP_PIN     	21
#define NET_LED_PIN     26
#define DOOR_STATE_PIN  29



/* �ź��� */
struct rt_semaphore door_open_sem;
struct rt_semaphore beep_sem;


/* ������ƿ� */
struct rt_mailbox mb_led;
/* ���ڷ��ʼ����ڴ�� */
static char mb_led_pool[64];


/* ������ƿ� */
struct rt_mailbox mb_net;
/* ���ڷ��ʼ����ڴ�� */
static char mb_net_pool[64];


/* ��Ϣ���п��ƿ� */
struct rt_messagequeue mq_send_onenet;
/* ��Ϣ�������õ��ķ�����Ϣ���ڴ�� */
rt_uint8_t msg_send_onenet_pool[128];



#define PWM_DEV_NAME        "pwm"  /* PWM�豸���� */
#define PWM_DEV_CHANNEL     2       /* PWMͨ�� */
#define PWM_DEV_PERIOD     20000000      
struct rt_device_pwm *pwm_dev1;      /* PWM�豸��� */


static rt_uint8_t door_state_last;


void SetAngle(rt_uint32_t angle)
{
	rt_uint32_t  pulse;
	pulse = (50*(1+angle/45.0))*10000;
	rt_pwm_set(pwm_dev1, PWM_DEV_CHANNEL, PWM_DEV_PERIOD, pulse);
	rt_pwm_enable(pwm_dev1, PWM_DEV_CHANNEL);
}


/* ����״̬�жϻص����� */
void door_state_pin_ind(void *args)
{
	static struct onenet_msg mqbuf = {0};
	static rt_tick_t last_tick = 0;
	
	if(rt_tick_get() - last_tick > DOOR_STATE_UP_WAIT)
	{
		last_tick = rt_tick_get();
		
		/* ������Ϣ����Ϣ������ */
		mqbuf.type = DOOR_STATE_MSG;
		mqbuf.id = rt_pin_read(DOOR_STATE_PIN);
		rt_mq_send(&mq_send_onenet, &mqbuf, sizeof(mqbuf));
		door_state_last = rt_pin_read(DOOR_STATE_PIN);
	}
}


/* �����߳� */
static void door_open_entry(void)
{
	
    /* �����豸 */
    pwm_dev1 = (struct rt_device_pwm *)rt_device_find(PWM_DEV_NAME);
    SetAngle(50);
	rt_thread_mdelay(1000);
	rt_pwm_disable(pwm_dev1, PWM_DEV_CHANNEL);
	
	while(1)
	{
		rt_sem_control(&door_open_sem, RT_IPC_CMD_RESET, RT_NULL);
        rt_sem_take(&door_open_sem, RT_WAITING_FOREVER);
		
		rt_mb_send(&mb_led, LED_GREEN);
		rt_sem_release(&beep_sem);
		
		
		LOG_D("steering engine open!\n");
		SetAngle(125);
		rt_thread_mdelay(1000);
		
		for(int i = 0; i < 50 && !rt_pin_read(DOOR_STATE_PIN); i++)
		{
			rt_thread_mdelay(100);
		}
		
		LOG_D("steering engine reset!\n");
		SetAngle(50);
		rt_thread_mdelay(1000);
		rt_pwm_disable(pwm_dev1, PWM_DEV_CHANNEL);
		
		rt_mb_send(&mb_led, LED_BLUE);
	}
}

/* ����led���߳� */
static void net_led_entry(void)
{
	rt_uint32_t led_state = NET_ERR;
	rt_uint32_t led_on_time = 200;
	rt_uint32_t led_off_time = 800;
	
	rt_pin_mode(NET_LED_PIN, PIN_MODE_OUTPUT);
	
	while(1)
	{
		if(rt_mb_recv(&mb_net, (rt_ubase_t *)&led_state, 0) == RT_EOK)
		{	
			LOG_D("led recv:%d",led_state);
			if(led_state == NET_SET)
			{
				led_off_time = 10;
				led_on_time = 1000;
			}
			else if(led_state == NET_ERR)
			{
				led_on_time = 100;
				led_off_time = 1000;
			}
			else if(led_state == NET_OK)
			{
				led_on_time = 100;
				led_off_time = 10000;
			}

		}
			
		rt_thread_mdelay(led_off_time);
		rt_pin_write(NET_LED_PIN, PIN_LOW);
		rt_thread_mdelay(led_on_time);
		rt_pin_write(NET_LED_PIN, PIN_HIGH);

	}
}


/* ��led���߳� */
static void door_led_entry(void)
{
	rt_uint32_t led_state;
	
	rt_pin_mode(LED_RED_PIN, PIN_MODE_OUTPUT);
	rt_pin_mode(LED_GREEN_PIN, PIN_MODE_OUTPUT);
	rt_pin_mode(LED_BLUE_PIN, PIN_MODE_OUTPUT);
	
	rt_pin_write(LED_RED_PIN, PIN_HIGH);
	rt_pin_write(LED_GREEN_PIN, PIN_HIGH);
	rt_pin_write(LED_BLUE_PIN, PIN_LOW);
	
	while(1)
	{
		rt_mb_recv(&mb_led, (rt_ubase_t *)&led_state, RT_WAITING_FOREVER);
		rt_pin_write(LED_RED_PIN, PIN_HIGH);
		rt_pin_write(LED_GREEN_PIN, PIN_HIGH);
		rt_pin_write(LED_BLUE_PIN, PIN_HIGH);
		switch(led_state)
		{
			case LED_RED:
				rt_pin_write(LED_RED_PIN, PIN_LOW);
				LOG_D("LED_RED!\n");
				break;
			case LED_GREEN:
				rt_pin_write(LED_GREEN_PIN, PIN_LOW);
				LOG_D("LED_GREEN!\n");
				break;
			case LED_BLUE:
				rt_pin_write(LED_BLUE_PIN, PIN_LOW);
				LOG_D("LED_BLUE!\n");
				break;
			default:
				break;
		}
	}
}


/* �������߳� */
static void door_beep_entry(void)
{
	rt_pin_mode(BEEP_PIN, PIN_MODE_OUTPUT);
	rt_pin_write(BEEP_PIN, PIN_LOW);
	while(1)
	{
		rt_sem_control(&beep_sem, RT_IPC_CMD_RESET, RT_NULL);
		rt_sem_take(&beep_sem, RT_WAITING_FOREVER);

		LOG_D("beep beep beep!\n");

//		rt_pin_write(BEEP_PIN, PIN_HIGH);
//		rt_thread_mdelay(80);
//		rt_pin_write(BEEP_PIN, PIN_LOW);
//		rt_thread_mdelay(80);

//		rt_pin_write(BEEP_PIN, PIN_HIGH);
//		rt_thread_mdelay(80);
//		rt_pin_write(BEEP_PIN, PIN_LOW);
//		rt_thread_mdelay(80);

//		rt_pin_write(BEEP_PIN, PIN_HIGH);
//		rt_thread_mdelay(80);
//		rt_pin_write(BEEP_PIN, PIN_LOW);
//		rt_thread_mdelay(80);
	}
}


/* �������߳� */
static void door_state_confirm_entry(void)
{
	struct onenet_msg mqbuf = {0};
	while(1)
	{
		
		if(door_state_last != rt_pin_read(DOOR_STATE_PIN))
		{
			/* ������Ϣ����Ϣ������ */
			mqbuf.type = DOOR_STATE_MSG;
			mqbuf.id = rt_pin_read(DOOR_STATE_PIN);
			rt_mq_send(&mq_send_onenet, &mqbuf, sizeof(mqbuf));
			door_state_last = rt_pin_read(DOOR_STATE_PIN);	
		}
		
		rt_thread_mdelay(10000);
	}
}

rt_err_t door_control_init(void)
{
	rt_err_t ret = RT_EOK;
	rt_thread_t thread = RT_NULL;
	
	/* ��ʼ���ź��� */
    rt_sem_init(&door_open_sem, "open_sem", 0, RT_IPC_FLAG_FIFO);
	rt_sem_init(&beep_sem, "beep_sem", 0, RT_IPC_FLAG_FIFO);
	
	
	/* ��ʼ��һ�� mailbox */
    ret = rt_mb_init(&mb_led, "mb_led", &mb_led_pool[0], sizeof(mb_led) / 4, RT_IPC_FLAG_FIFO);   
    if (ret != RT_EOK)
    {
		ret = RT_ERROR;
        goto _error;
    }
	
	/* ��ʼ��һ�� mailbox */
    ret = rt_mb_init(&mb_net, "mb_net", &mb_net_pool[0], sizeof(mb_net_pool) / 4, RT_IPC_FLAG_FIFO);   
    if (ret != RT_EOK)
    {
		ret = RT_ERROR;
        goto _error;
    }
	
	/* ��ʼ����Ϣ���� */
    ret = rt_mq_init(&mq_send_onenet, "mqt", &msg_send_onenet_pool[0], sizeof(struct onenet_msg), sizeof(msg_send_onenet_pool), RT_IPC_FLAG_FIFO);
    if (ret != RT_EOK)
    {
		ret = RT_ERROR;
        goto _error;
    }
	
	
	/* ����״̬����Ϊ����ģʽ */
    rt_pin_mode(DOOR_STATE_PIN, PIN_MODE_INPUT_PULLDOWN);
    /* ���жϣ��½���ģʽ���ص�������Ϊdoor_state_pin_ind */
    rt_pin_attach_irq(DOOR_STATE_PIN, PIN_IRQ_MODE_RISING_FALLING, door_state_pin_ind, RT_NULL);
    /* ʹ���ж� */
    rt_pin_irq_enable(DOOR_STATE_PIN, PIN_IRQ_ENABLE);
	
	
	 /* ���������߳� */
    thread = rt_thread_create("door_open", (void (*)(void *parameter))door_open_entry, RT_NULL, 1024, 10, 10);  //stack_size 512
    if (thread != RT_NULL)
    {
        rt_thread_startup(thread);
    }
    else
    {
        ret = RT_ERROR;
		goto _error;
    }
	
	/* ����door led���߳� */
    thread = rt_thread_create("door_led", (void (*)(void *parameter))door_led_entry, RT_NULL, 512, 27, 10);  //stack_size 256
    if (thread != RT_NULL)
    {
        rt_thread_startup(thread);
    }
    else
    {
        ret = RT_ERROR;
		goto _error;
    }
	
	/* ����net led���߳� */
    thread = rt_thread_create("net_led", (void (*)(void *parameter))net_led_entry, RT_NULL, 512, 29, 10);  //stack_size 256
    if (thread != RT_NULL)
    {
        rt_thread_startup(thread);
    }
    else
    {
        ret = RT_ERROR;
		goto _error;
    }
	
	/* �����������߳� */
    thread = rt_thread_create("door_beep", (void (*)(void *parameter))door_beep_entry, RT_NULL, 512, 28, 10); //stack_size 256
    if (thread != RT_NULL)
    {
        rt_thread_startup(thread);
    }
    else
    {
        ret = RT_ERROR;
		goto _error;
    }
	
	/* ������״̬ȷ���߳� */
    thread = rt_thread_create("door_confirm", (void (*)(void *parameter))door_state_confirm_entry, RT_NULL, 512, 30, 10); //stack_size 256
    if (thread != RT_NULL)
    {
        rt_thread_startup(thread);
    }
    else
    {
        ret = RT_ERROR;
		goto _error;
    }
	
	
_error:
	return ret;
}

