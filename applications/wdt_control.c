/*
 * �����嵥������һ���������Ź��豸ʹ������
 * ���̵����� wdt_sample ��������ն�
 * ������ø�ʽ��wdt_sample wdt
 * ������ͣ�����ڶ���������Ҫʹ�õĿ��Ź��豸���ƣ�Ϊ����ʹ������Ĭ�ϵĿ��Ź��豸��
 * �����ܣ�����ͨ���豸���Ʋ��ҿ��Ź��豸��Ȼ���ʼ���豸�����ÿ��Ź��豸���ʱ�䡣
 *           Ȼ�����ÿ����̻߳ص��������ڻص��������ι����
*/

#include <rtthread.h>
#include <rtdevice.h>

#define WDT_DEVICE_NAME    "wdg"    /* ���Ź��豸���� */

static rt_device_t wdg_dev;         /* ���Ź��豸��� */

static void idle_hook(void)
{
    /* �ڿ����̵߳Ļص�������ι�� */
    rt_device_control(wdg_dev, RT_DEVICE_CTRL_WDT_KEEPALIVE, NULL);
}

rt_err_t wdt_init(void)
{
    rt_err_t ret = RT_EOK;
    rt_uint32_t timeout = 3;        /* ���ʱ�䣬��λ���� */

    /* �����豸���Ʋ��ҿ��Ź��豸����ȡ�豸��� */
    wdg_dev = rt_device_find(WDT_DEVICE_NAME);
    if (!wdg_dev)
    {
        rt_kprintf("find %s failed!\n", WDT_DEVICE_NAME);
        return RT_ERROR;
    }

    /* ���ÿ��Ź����ʱ�� */
    ret = rt_device_control(wdg_dev, RT_DEVICE_CTRL_WDT_SET_TIMEOUT, &timeout);
    if (ret != RT_EOK)
    {
        rt_kprintf("set %s timeout failed!\n", WDT_DEVICE_NAME);
        return RT_ERROR;
    }
    /* �������Ź� */
    ret = rt_device_control(wdg_dev, RT_DEVICE_CTRL_WDT_START, RT_NULL);
    if (ret != RT_EOK)
    {
        rt_kprintf("start %s failed!\n", WDT_DEVICE_NAME);
        return -RT_ERROR;
    }
    /* ���ÿ����̻߳ص����� */
    rt_thread_idle_sethook(idle_hook);

    return ret;
}

void wdg_reset(void)
{
	rt_thread_idle_delhook(idle_hook);
}