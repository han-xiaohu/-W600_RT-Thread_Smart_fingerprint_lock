/*
 * �����嵥������һ�� PWM �豸ʹ������
 * ���̵����� pwm_led_sample ��������ն�
 * ������ø�ʽ��pwm_led_sample
 * �����ܣ�ͨ�� PWM �豸���� LED �Ƶ����ȣ����Կ���LED��ͣ���ɰ��䵽����Ȼ���ִ����䵽����
*/

#include <rtthread.h>
#include <rtdevice.h>

#define PWM_DEV_NAME        "pwm"  /* PWM�豸���� */
#define PWM_DEV_CHANNEL     1       /* PWMͨ�� */

struct rt_device_pwm *pwm_dev;      /* PWM�豸��� */

static int pwm_led_sample(int argc, char *argv[])
{
 

    while (1)
    {
        rt_thread_mdelay(500);
//        if (dir)
//        {
//            pulse += 5000;      /* ��0ֵ��ʼÿ������5000ns */
//        }
//        else
//        {
//            pulse -= 5000;      /* �����ֵ��ʼÿ�μ���5000ns */
//        }
//        if (pulse >= period)
//        {
//            dir = 0;
//        }
//        if (0 == pulse)
//        {
//            dir = 1;
//        }

//        /* ����PWM���ں������� */
//        rt_pwm_set(pwm_dev, PWM_DEV_CHANNEL, period, pulse);
    }
}
/* ������ msh �����б��� */
MSH_CMD_EXPORT(pwm_led_sample, pwm sample);