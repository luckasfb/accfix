/* Copyright Statement:
 *
 * This software/firmware and related documentation ("MediaTek Software") are
 * protected under relevant copyright laws. The information contained herein
 * is confidential and proprietary to MediaTek Inc. and/or its licensors.
 * Without the prior written permission of MediaTek inc. and/or its licensors,
 * any reproduction, modification, use or disclosure of MediaTek Software,
 * and information contained herein, in whole or in part, shall be strictly prohibited.
 *
 * MediaTek Inc. (C) 2010. All rights reserved.
 *
 * BY OPENING THIS FILE, RECEIVER HEREBY UNEQUIVOCALLY ACKNOWLEDGES AND AGREES
 * THAT THE SOFTWARE/FIRMWARE AND ITS DOCUMENTATIONS ("MEDIATEK SOFTWARE")
 * RECEIVED FROM MEDIATEK AND/OR ITS REPRESENTATIVES ARE PROVIDED TO RECEIVER ON
 * AN "AS-IS" BASIS ONLY. MEDIATEK EXPRESSLY DISCLAIMS ANY AND ALL WARRANTIES,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE OR NONINFRINGEMENT.
 * NEITHER DOES MEDIATEK PROVIDE ANY WARRANTY WHATSOEVER WITH RESPECT TO THE
 * SOFTWARE OF ANY THIRD PARTY WHICH MAY BE USED BY, INCORPORATED IN, OR
 * SUPPLIED WITH THE MEDIATEK SOFTWARE, AND RECEIVER AGREES TO LOOK ONLY TO SUCH
 * THIRD PARTY FOR ANY WARRANTY CLAIM RELATING THERETO. RECEIVER EXPRESSLY ACKNOWLEDGES
 * THAT IT IS RECEIVER'S SOLE RESPONSIBILITY TO OBTAIN FROM ANY THIRD PARTY ALL PROPER LICENSES
 * CONTAINED IN MEDIATEK SOFTWARE. MEDIATEK SHALL ALSO NOT BE RESPONSIBLE FOR ANY MEDIATEK
 * SOFTWARE RELEASES MADE TO RECEIVER'S SPECIFICATION OR TO CONFORM TO A PARTICULAR
 * STANDARD OR OPEN FORUM. RECEIVER'S SOLE AND EXCLUSIVE REMEDY AND MEDIATEK'S ENTIRE AND
 * CUMULATIVE LIABILITY WITH RESPECT TO THE MEDIATEK SOFTWARE RELEASED HEREUNDER WILL BE,
 * AT MEDIATEK'S OPTION, TO REVISE OR REPLACE THE MEDIATEK SOFTWARE AT ISSUE,
 * OR REFUND ANY SOFTWARE LICENSE FEES OR SERVICE CHARGE PAID BY RECEIVER TO
 * MEDIATEK FOR SUCH MEDIATEK SOFTWARE AT ISSUE.
 *
 * The following software/firmware and/or related documentation ("MediaTek Software")
 * have been modified by MediaTek Inc. All revisions are subject to any receiver's
 * applicable license agreements with MediaTek Inc.
 */

#include "accfix.h"
#include "mach/mt_boot.h"
#include <cust_eint.h>
#include <mach/mt_gpio.h>
#include <../../../../../../kernel/include/linux/stat.h>
#include <../../../../../../kernel/include/linux/syscalls.h>

#define ACCFIX_EINT
#define SW_WORK_AROUND_ACCDET_REMOTE_BUTTON_ISSUE

//#define ACCDET_MULTI_KEY_FEATURE 

#ifdef ACCDET_MULTI_KEY_FEATURE

//see /data/TCL_E928/mediatek/platform/mt6575/uboot/mt6575_auxadc.c for source code
//extern int IMM_GetOneChannelValue(int dwChannel, int data[4], int* rawdata);

#include <../../../uboot/inc/asm/arch/mt6575_auxadc_hw.h>

static int adc_auto_set =0;

int IMM_GetOneChannelValue(int dwChannel, int data[4], int* rawdata)
{
   unsigned int channel[14] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
   int idle_count =0;
   int data_ready_count=0;
 //  unsigned int i = 0, data = 0;
 //  unsigned int poweron, poweroff;

   // Polling until bit STA = 0
   while ((*(volatile u16 *)AUXADC_CON3) & 0x01)
   {
       ACCFIX_DEBUG("[accfix]: wait for module idle\n");
       mdelay(100);
           idle_count++;
           if(idle_count>30)
           {
              //wait for idle time out
              ACCFIX_DEBUG("[accfix]: wait for aux/adc idle time out\n");
              return -1;
           }
   }

   while ((*(volatile u16 *)(AUXADC_DAT0 + dwChannel * 0x04)) & (1<<12))
   {
       ACCFIX_DEBUG("[accfix]: wait for channel[%d] data ready\n",dwChannel);
       mdelay(100);
           data_ready_count++;
           if(data_ready_count>30)
           {
              //wait for idle time out
              ACCFIX_DEBUG("[accfix]: wait for channel[%d] data ready time out\n",dwChannel);
              return -2;
           }
   }

   //read data

   if(0==adc_auto_set)
   {
      //clear bit
          *(volatile u16 *)AUXADC_CON1 = *(volatile u16 *)AUXADC_CON1 & (~(1 << dwChannel));
          mdelay(20);
          //set bit
          *(volatile u16 *)AUXADC_CON1 = *(volatile u16 *)AUXADC_CON1 | (1 << dwChannel);
   }
   mdelay(20);
   //read data

   channel[dwChannel] = (*(volatile u16 *)(AUXADC_DAT0 + dwChannel * 0x04)) & 0x0FFF;
   if(NULL != rawdata)
   {
      *rawdata = channel[dwChannel];
   }

   data[0] = (channel[dwChannel] * 250 / 4096 / 100);
   data[1] = ((channel[dwChannel] * 250 / 4096) % 100);

   mdelay(20);
   if(0 == adc_auto_set)
   {
           //clear bit
           *(volatile u16 *)AUXADC_CON1 = *(volatile u16 *)AUXADC_CON1 & (~(1 << dwChannel));
   }

   return 0;
}

 
#define KEY_SAMPLE_PERIOD        (60)            //ms
#define MULTIKEY_ADC_CHANNEL	 (1)

#define NO_KEY			 (0x0)
#define UP_KEY			 (0x01)
#define MD_KEY		  	 (0x02)
#define DW_KEY			 (0x04)

#define SHORT_PRESS		 (0x0)
#define LONG_PRESS		 (0x10)
#define SHORT_UP                 ((UP_KEY) | SHORT_PRESS)
#define SHORT_MD             	 ((MD_KEY) | SHORT_PRESS)
#define SHORT_DW                 ((DW_KEY) | SHORT_PRESS)
#define LONG_UP                  ((UP_KEY) | LONG_PRESS)
#define LONG_MD                  ((MD_KEY) | LONG_PRESS)
#define LONG_DW                  ((DW_KEY) | LONG_PRESS)


/*

    MD         UP          DW
|---------|-----------|----------|
0V       0.07V       0.24V      0.9

*/

#define DW_KEY_THR		 (24) //0.24v
#define UP_KEY_THR               (7) //0.07v
#define MD_KEY_THR		 (0)

static int key_check(int a, int b)
{
	ACCFIX_DEBUG("adc_data: %d.%d v\n",a,b);
	/* when the voltage is bigger than 1.0V */
	if( a > 0)
		return NO_KEY;

	/* 0.24V ~ */
	if( b >= DW_KEY_THR) 
	{
		return DW_KEY;
	} 
	else if ((b < DW_KEY_THR)&& (b >= UP_KEY_THR))
	{
		return UP_KEY;
	}
	else if ((b < UP_KEY_THR) && (b >= MD_KEY_THR))
	{
		return MD_KEY;
	}
	return NO_KEY;
}

static int multi_key_detection(void)
{
        int current_status = 0;
	int index = 0;
	int count = long_press_time / ( KEY_SAMPLE_PERIOD + 40 ); //ADC delay
	count = count / 2; // make it 3 times faster !!! DAP
	int m_key = 0;
	int cur_key = 0;
	int adc_data[4] = {0, 0, 0, 0};
	int adc_raw;
	int channel = MULTIKEY_ADC_CHANNEL;

	IMM_GetOneChannelValue(channel, adc_data, &adc_raw);
	m_key = cur_key = key_check(adc_data[0], adc_data[1]);

	while(index++ < count)
	{

		/* Check if the current state has been changed */
		current_status = INREG32(ACCDET_MEMORIZED_IN) & 0x3;
		if(current_status != 0)
		{
			return (m_key | SHORT_PRESS);
		}

		/* Check if the voltage has been changed (press one key and another) */
		IMM_GetOneChannelValue(channel, adc_data, &adc_raw);
		cur_key = key_check(adc_data[0], adc_data[1]);
		if(m_key != cur_key)
			return (m_key | SHORT_PRESS);
		else
			m_key = cur_key;
		
		msleep(KEY_SAMPLE_PERIOD);
	}
	
	return (m_key | LONG_PRESS);
}

#endif


/*----------------------------------------------------------------------
static variable defination
----------------------------------------------------------------------*/
#define REGISTER_VALUE(x)   (x - 1)

static struct switch_dev accfix_data;
static struct input_dev *kpd_accfix_dev;
static struct cdev *accfix_cdev;
static struct class *accfix_class = NULL;
static struct device *accfix_nor_device = NULL;

static dev_t accfix_devno;

static int pre_status = 0;
static int pre_state_swctrl = 0;
static int accfix_status = PLUG_OUT;
static int cable_type = 0;
static s64 long_press_time_ns = 0 ;

static int g_accfix_first = 1;
static bool IRQ_CLR_FLAG = FALSE;
static volatile int call_status =0;
static volatile int button_status = 0;
static int tv_out_debounce = 0;

struct wake_lock accfix_suspend_lock; 

static struct work_struct accfix_work;
static struct workqueue_struct * accfix_workqueue = NULL;
static int g_accfix_working_in_suspend =0;

#ifdef ACCFIX_EINT
//static int g_accfix_working_in_suspend =0;

static struct work_struct accfix_eint_work;
static struct workqueue_struct * accfix_eint_workqueue = NULL;

static inline void __init accfix_init(void);

extern void mt65xx_eint_unmask(unsigned int line);
extern void mt65xx_eint_mask(unsigned int line);
//extern void mt65xx_eint_set_polarity(kal_uint8 eintno, kal_bool ACT_Polarity);
extern void mt65xx_eint_set_hw_debounce(kal_uint8 eintno, kal_uint32 ms);
extern kal_uint32 mt65xx_eint_set_sens(kal_uint8 eintno, kal_bool sens);
extern void mt65xx_eint_registration(kal_uint8 eintno, kal_bool Dbounce_En,
                                     kal_bool ACT_Polarity, void (EINT_FUNC_PTR)(void),
                                     kal_bool auto_umask);
#endif




static volatile int double_check_flag = 0;
static bool tv_headset_icon = false;
static int button_press_debounce = 0x400;

#define DEBUG_THREAD 1
static struct platform_driver accfix_driver;



/****************************************************************/
/***export function, for tv out driver                                                                     **/
/****************************************************************/
void switch_asw_to_tv(bool tv_enable)
{
	ACCFIX_DEBUG("[accfix]switch analog switch to tv is %d\n",tv_enable);
	if(tv_enable)
	{
		SETREG32(ACCDET_STATE_SWCTRL,TV_DET_BIT);
		//hwSPKClassABAnalogSwitchSelect(ACCDET_TV_CHA);
	}
	else
	{
		CLRREG32(ACCDET_STATE_SWCTRL,TV_DET_BIT);
		//hwSPKClassABAnalogSwitchSelect(ACCDET_MIC_CHA);
	}
}

void switch_NTSC_to_PAL(int mode)
{
    return;
}
//EXPORT_SYMBOL(switch_NTSC_to_PAL);


void accfix_detect_2(void)
{
	int ret = 0 ;
    
	ACCFIX_DEBUG("[accfix]accfix_detect\n");
    
	accfix_status = PLUG_OUT;
    ret = queue_work(accfix_workqueue, &accfix_work);	
    if(!ret)
    {
  		ACCFIX_DEBUG("[accfix]accfix_detect:accfix_work return:%d!\n", ret);  		
    }

	return;
}
EXPORT_SYMBOL(accfix_detect_2);

static void enable_tv_detect(bool tv_enable);
static void enable_tv_allwhite_signal(bool tv_enable);
void accfix_state_reset_2(void)
{
    
	ACCFIX_DEBUG("[accfix]accfix_state_reset\n");
    
	accfix_status = PLUG_OUT;
        cable_type = NO_DEVICE;
        enable_tv_allwhite_signal(false);
	enable_tv_detect(false);

	return;
}
EXPORT_SYMBOL(accfix_state_reset_2);

/****************************************************************/
/*******static function defination                                                                          **/
/****************************************************************/

#ifdef ACCFIX_EINT

void inline disable_accfix(void)
{
   // disable ACCDET unit
   ACCFIX_DEBUG("accfix: disable_accfix\n");
   pre_state_swctrl = INREG32(ACCDET_STATE_SWCTRL);
   OUTREG32(ACCDET_CTRL, ACCDET_DISABLE);
   OUTREG32(ACCDET_STATE_SWCTRL, 0);
   //unmask EINT
   mt65xx_eint_unmask(CUST_EINT_ACCDET_NUM);  
}

void accfix_eint_func(void)
{
    int ret=0;
    ACCFIX_DEBUG("[accfix] accfix_eint_func\n");
    ret = queue_work(accfix_eint_workqueue, &accfix_eint_work);	
    if(!ret)
    {
  	   ACCFIX_DEBUG("[accfix]accfix_eint_func:accfix_work return:%d!\n", ret);  		
    }
}


void accfix_eint_work_callback(struct work_struct *work)
{
    ACCFIX_DEBUG("[acfix]accfix_eint_work_callback fuc delay %d then enable accfix\n",ACCDET_DELAY_ENABLE_TIME);
	msleep(ACCDET_DELAY_ENABLE_TIME);
    //reset AccDet
    accfix_init();
	//enable ACCDET unit
	OUTREG32(ACCDET_STATE_SWCTRL, ACCDET_SWCTRL_EN);
  	OUTREG32(ACCDET_CTRL, ACCDET_ENABLE); 
    //when unmask EINT
}

static inline int accfix_setup_eint(void)
{
	
	/*configure to GPIO function, external interrupt*/
    ACCFIX_DEBUG("[accfix]accfix_setup_eint\n");
	
	mt_set_gpio_mode(GPIO_ACCDET_EINT_PIN, GPIO_ACCDET_EINT_PIN_M_EINT);
    mt_set_gpio_dir(GPIO_ACCDET_EINT_PIN, GPIO_DIR_IN);
	mt_set_gpio_pull_enable(GPIO_ACCDET_EINT_PIN, GPIO_PULL_ENABLE);
	#ifdef ACCFIX_EINT_HIGH_ACTIVE
	mt_set_gpio_pull_select(GPIO_ACCDET_EINT_PIN, GPIO_PULL_DOWN);
	#else
	mt_set_gpio_pull_select(GPIO_ACCDET_EINT_PIN, GPIO_PULL_UP);
	#endif
	
	
	/**
    mt65xx_eint_set_sens(CUST_EINT_ACCDET_NUM, CUST_EINT_ACCDET_SENSITIVE);
	#ifdef ACCFIX_EINT_HIGH_ACTIVE
	mt65xx_eint_set_polarity(CUST_EINT_ACCDET_NUM, 1);
	ACCFIX_DEBUG("[accfix]accfix_setup_eint active high \n");
	#else
	mt65xx_eint_set_polarity(CUST_EINT_ACCDET_NUM, 0);
	ACCFIX_DEBUG("[accfix]accfix_setup_eint active low\n");
	#endif
	**/
	mt65xx_eint_set_hw_debounce(CUST_EINT_ACCDET_NUM, CUST_EINT_ACCDET_DEBOUNCE_CN);
	mt65xx_eint_registration(CUST_EINT_ACCDET_NUM, CUST_EINT_ACCDET_DEBOUNCE_EN, CUST_EINT_ACCDET_POLARITY, accfix_eint_func, 0);
	mt65xx_eint_unmask(CUST_EINT_ACCDET_NUM);  

	
    return 0;
	
}


#endif


static void enable_tv_detect(bool tv_enable)
{

    // disable ACCDET unit before switch the detect way
    OUTREG32(ACCDET_CTRL, ACCDET_DISABLE);
    OUTREG32(ACCDET_STATE_SWCTRL, 0);
    
    if(tv_enable)
    {	
    }
    else
	{
		// switch analog switch from tv out to mic
		OUTREG32(ACCDET_STATE_SWCTRL, ACCDET_SWCTRL_EN);

		// init the accdet MIC detect unit
	  	SETREG32(ACCDET_RSTB, MIC_INIT_BIT);
		CLRREG32(ACCDET_RSTB, MIC_INIT_BIT);  
	}
    
	ACCFIX_DEBUG("[accfix]enable_tv_detect:ACCDET_STATE_SWCTRL =%x\n",
        INREG32(ACCDET_STATE_SWCTRL));

    // enable ACCDET unit
	OUTREG32(ACCDET_CTRL, ACCDET_ENABLE); 
	
}

//enable allwhite signal or color bar from TV-out
static void enable_tv_allwhite_signal(bool tv_enable)
{
}

//enable TV to out tv signal
static TVOUT_STATUS enable_tv(bool tv_enable)
{
    TVOUT_STATUS ret = TVOUT_STATUS_OK;
    return ret;
}


//detect if remote button is short pressed or long pressed
static bool is_long_press(void)	{
	int current_status = 0;
	int index = 0;
	while(index++ < 40) //>800 millis = longpress
	{ 
		current_status = INREG32(ACCDET_MEMORIZED_IN) & 0x3;
		if(current_status != 0)
		{
			return false;
		}
		msleep(20);
	}
	return true;
}



//clear ACCDET IRQ in accdet register
static inline void clear_accfix_interrupt(void)
{
	//SETREG32(ACCDET_IRQ_STS, (IRQ_CLR_BIT|IRQ_CLR_SC_BIT));
	//it is safe by using polling to adjust when to clear IRQ_CLR_BIT
	SETREG32(ACCDET_IRQ_STS, (IRQ_CLR_BIT));
}


///*MT6575 E1*:resolve the hardware issue:
//fast plug out headset after plug in with hook switch pressed, 
//accdet detect hook switch instead of plug_out state.
static inline void double_check_workaround(void)
{
    int mem_in;
       
    mem_in = INREG32(ACCDET_MEMORIZED_IN);
    if(mem_in == 0)
    {
    	OUTREG32(ACCDET_RSV_CON3, ACCDET_DEFVAL_SEL);
		ACCFIX_DEBUG("double_check_workaround: ACCDET_RSV_CON3=0x%x \n", INREG32(ACCDET_RSV_CON3));
		
		enable_tv_allwhite_signal(false);
		enable_tv_detect(false);

		OUTREG32(ACCDET_RSV_CON3, 0);

		accfix_status = HOOK_SWITCH;
        cable_type = HEADSET_NO_MIC;
       
    }
    else if(mem_in == 3)
    { 
    	OUTREG32(ACCDET_RSV_CON3, ACCDET_DEFVAL_SEL | 0x55);
		ACCFIX_DEBUG("double_check_workaround: ACCDET_RSV_CON3=0x%x \n", INREG32(ACCDET_RSV_CON3));
		
		enable_tv_allwhite_signal(false);
		enable_tv_detect(false);
		
		OUTREG32(ACCDET_RSV_CON3, 0x55);

		accfix_status = MIC_BIAS;	  
		cable_type = HEADSET_MIC;

 		OUTREG32(ACCDET_DEBOUNCE0, cust_headset_settings.debounce0);
    }

    //reduce the detect time of press remote button when work around for
    //tv-out slowly plug in issue, so not resume debounce0 time at this time.
    //OUTREG32(ACCDET_DEBOUNCE0, cust_headset_settings.debounce0);
    OUTREG32(ACCDET_DEBOUNCE3, cust_headset_settings.debounce3);
}

#ifdef SW_WORK_AROUND_ACCDET_REMOTE_BUTTON_ISSUE


#define    ACC_ANSWER_CALL   1
#define    ACC_END_CALL      2
#define    ACC_PLAY_MUSIC    3
#define    ACC_PAUSE_MUSIC   4
//longxuewei add
#define     ACC_HEADSET_FLAG   5  //
//longxuewei end

static atomic_t send_event_flag = ATOMIC_INIT(0);

static DECLARE_WAIT_QUEUE_HEAD(send_event_wq);


static int accfix_key_event=0;

static int sendKeyEvent(void *unuse)
{
    while(1)
    {
        ACCFIX_DEBUG( " accfix:sendKeyEvent wait\n");
        //wait for signal
        wait_event_interruptible(send_event_wq, (atomic_read(&send_event_flag) != 0));

        ACCFIX_DEBUG( " accfix:going to send event %d\n", accfix_key_event);
/* 
        Workaround to avoid holding the call when pluging out.
        Added a customized value to in/decrease the delay time.
        The longer delay, the more time key event would take.
*/
#ifdef KEY_EVENT_ISSUE_DELAY
        if(KEY_EVENT_ISSUE_DELAY >= 0)
                msleep(KEY_EVENT_ISSUE_DELAY);
        else
                msleep(500);
#else
        msleep(500);
#endif
        if(PLUG_OUT !=accfix_status)
        {
            //send key event
            if(ACC_ANSWER_CALL == accfix_key_event)
            {
                ACCFIX_DEBUG("[accfix] answer call!\n");
                input_report_key(kpd_accfix_dev, KEY_CALL, 1);
                input_report_key(kpd_accfix_dev, KEY_CALL, 0);
                input_sync(kpd_accfix_dev);
            }
            if(ACC_END_CALL == accfix_key_event)
            {
                ACCFIX_DEBUG("[accfix] end call!\n");
                input_report_key(kpd_accfix_dev, KEY_ENDCALL, 1);
                input_report_key(kpd_accfix_dev, KEY_ENDCALL, 0);
                input_sync(kpd_accfix_dev);
            }
//longxuewei add      
	    if( ACC_HEADSET_FLAG== accfix_key_event)
            {
                ACCFIX_DEBUG("[accfix] HEADSET TEST BUTTON !\n");
                input_report_key(kpd_accfix_dev, KEY_F24, 1);
                input_report_key(kpd_accfix_dev, KEY_F24, 0);
                input_sync(kpd_accfix_dev);
            }
//longxuewei end
        }

        atomic_set(&send_event_flag, 0);
    }
    return 0;
}

static ssize_t notify_sendKeyEvent(int event)
{
	 
    accfix_key_event = event;
    atomic_set(&send_event_flag, 1);
    wake_up(&send_event_wq);
    ACCFIX_DEBUG( " accfix:notify_sendKeyEvent !\n");
    return 0;
}


#endif

bool isDown = false;

//ACCDET state machine switch
static inline void check_cable_type(void)
{
    int current_status = 0;
    
    current_status = INREG32(ACCDET_MEMORIZED_IN) & 0x3; //A=bit1; B=bit0
    ACCFIX_DEBUG("[accfix]check_cable_type:[%s]current AB = %d\n", 
		accfix_status_string[accfix_status], current_status);
	    	
    button_status = 0;
    pre_status = accfix_status;
    if(accfix_status == PLUG_OUT)
        double_check_flag = 0;

    //ACCFIX_DEBUG("[accfix]check_cable_type: clock = 0x%x\n", INREG32(PERI_GLOBALCON_PDN0));
	//ACCFIX_DEBUG("[accfix]check_cable_type: PLL clock = 0x%x\n", INREG32(0xF0007020));
    ACCFIX_DEBUG("[accfix]check_cable_type: IRQ_STS = 0x%x call_status=%d\n", INREG32(ACCDET_IRQ_STS), call_status);
    IRQ_CLR_FLAG = FALSE;
    switch(accfix_status)
    {
        case PLUG_OUT:
            if(current_status == 0)
            {
				cable_type = HEADSET_NO_MIC;
				accfix_status = HOOK_SWITCH;
            }
            else if(current_status == 1)
            {
	         	accfix_status = MIC_BIAS;		
	         	cable_type = HEADSET_MIC;

				//ALPS00038030:reduce the time of remote button pressed during incoming call
                //solution: reduce hook switch debounce time to 0x400
                OUTREG32(ACCDET_DEBOUNCE0, button_press_debounce);
            }
            else if(current_status == 3)
            {
                ACCFIX_DEBUG("[accfix]PLUG_OUT state not change!\n");
				accfix_status = PLUG_OUT;		
	            cable_type = NO_DEVICE;
				#ifdef ACCFIX_EINT
		        disable_accfix();
		        #endif
            }
            else
            {
                ACCFIX_DEBUG("[accfix]PLUG_OUT can't change to this state!\n"); 
            }
            break;
            
        case MIC_BIAS:
	    //ALPS00038030:reduce the time of remote button pressed during incoming call
            //solution: resume hook switch debounce time
            OUTREG32(ACCDET_DEBOUNCE0, cust_headset_settings.debounce0);
			
            if(current_status == 0)
            {
                while(INREG32(ACCDET_IRQ_STS) & IRQ_STATUS_BIT)
	        {
		          ACCFIX_DEBUG("[accfix]check_cable_type: MIC BIAS clear IRQ on-going1....\n");	
			  msleep(10);
	        }
		CLRREG32(ACCDET_IRQ_STS, IRQ_CLR_BIT);
                IRQ_CLR_FLAG = TRUE;
                
		accfix_status = HOOK_SWITCH;
		button_status = 1;

//longxuewei add
		if((call_status == 0) && (INREG32(ACCDET_MEMORIZED_IN) & 0x3)==0)
		{
		  //ACCFIX_DEBUG("[accfix] key DOWN\n");
		  //input_report_key(kpd_accfix_dev, 226 , 1);
		  //input_sync(kpd_accfix_dev);
		  
#ifdef ACCDET_MULTI_KEY_FEATURE
		{
			int multi_key = NO_KEY;
			multi_key = multi_key_detection();
			switch (multi_key) {
			case SHORT_UP:
				ACCFIX_DEBUG("[accfix] Short press up (0x%x)\n", multi_key);
			  ACCFIX_DEBUG("[accfix] 114 key\n");
			  input_report_key(kpd_accfix_dev, 114 , 1); //KEY_H   KEY_VOLUMEUP
			  input_report_key(kpd_accfix_dev, 114 , 0);
			  input_sync(kpd_accfix_dev);
				break;
			case SHORT_MD:
				ACCFIX_DEBUG("[accfix] Short press middle (0x%x)\n", multi_key);
			  ACCFIX_DEBUG("[accfix] 226 key\n");
			  input_report_key(kpd_accfix_dev, 226 , 1); //KEY_H   KEY_VOLUMEUP
			  input_report_key(kpd_accfix_dev, 226 , 0);
			  input_sync(kpd_accfix_dev);
				break;
			case SHORT_DW:
				ACCFIX_DEBUG("[accfix] Short press down (0x%x)\n", multi_key);
			  ACCFIX_DEBUG("[accfix] 115 key\n");
			  input_report_key(kpd_accfix_dev, 115 , 1); //KEY_H   KEY_VOLUMEUP
			  input_report_key(kpd_accfix_dev, 115 , 0);
			  input_sync(kpd_accfix_dev);
				break;
			case LONG_UP:
				ACCFIX_DEBUG("[accfix] Long press up (0x%x)\n", multi_key);
			  ACCFIX_DEBUG("[accfix] 165 key\n");
			  input_report_key(kpd_accfix_dev, 165 , 1); //KEY_H   KEY_VOLUMEUP
			  input_report_key(kpd_accfix_dev, 165 , 0);
			  input_sync(kpd_accfix_dev);
				break;
			case LONG_MD:
				ACCFIX_DEBUG("[accfix] Long press middle (0x%x)\n", multi_key);
			  ACCFIX_DEBUG("[accfix] 163 key\n");
			  input_report_key(kpd_accfix_dev, 163 , 1); //KEY_H   KEY_VOLUMEUP
			  input_report_key(kpd_accfix_dev, 163 , 0);
			  input_sync(kpd_accfix_dev);
				break;
			case LONG_DW:
				ACCFIX_DEBUG("[accfix] Long press down (0x%x)\n", multi_key);
			  ACCFIX_DEBUG("[accfix] 163 key\n");
			  input_report_key(kpd_accfix_dev, 163 , 1); //KEY_H   KEY_VOLUMEUP
			  input_report_key(kpd_accfix_dev, 163 , 0);
			  input_sync(kpd_accfix_dev);
				break;
			default:
				ACCFIX_DEBUG("[accfix] unkown key (0x%x)\n", multi_key);
				break;
			}
		}
#else

		  ACCFIX_DEBUG("[accfix] key DOWN\n");
		  input_report_key(kpd_accfix_dev, 226 , 1); //KEY_H   KEY_VOLUMEUP
		  isDown = true;
		  if(is_long_press()) //was working
		  {
			input_sync(kpd_accfix_dev);
		  } else {
			ACCFIX_DEBUG("[accfix] key UP\n");
			input_report_key(kpd_accfix_dev, 226 , 0);
			isDown = false;
			input_sync(kpd_accfix_dev);
		  }  
#endif
		} else {
		        ACCFIX_DEBUG("[accfix] cable_type ACHTUNG KEY IS NOT PRESSED ! TODO: install shortpress if key is missed\n");
		}
//longxuewei add end

//cd /data/TCL_E928/kernel
//export CROSS_COMPILE=/data/android-toolchain-eabi/bin/arm-linux-androideabi-
// make  modules M=../mediatek/platform/mt6575/kernel/drivers/accdet

		if((call_status != 0) && button_status)
		{	
#ifdef SW_WORK_AROUND_ACCDET_REMOTE_BUTTON_ISSUE

	        if(is_long_press())
	     	{
	        	ACCFIX_DEBUG("[accfix]send long press remote button event %d \n",ACC_END_CALL);
			    notify_sendKeyEvent(ACC_END_CALL);
                } else {
		        ACCFIX_DEBUG("[accfix]send short press remote button event %d\n",ACC_ANSWER_CALL);
		        notify_sendKeyEvent(ACC_ANSWER_CALL);
	        }

#else
		    if(is_long_press())
		    {
	        	ACCFIX_DEBUG("[accfix]long press remote button to end call!\n");
			input_report_key(kpd_accfix_dev, KEY_ENDCALL, 1);
			input_report_key(kpd_accfix_dev, KEY_ENDCALL, 0);
			input_sync(kpd_accfix_dev);
  		    }
            else
	   	    {
		        ACCFIX_DEBUG("[accfix]short press remote button to accept call!\n");
			    input_report_key(kpd_accfix_dev, KEY_CALL, 1);
			    input_report_key(kpd_accfix_dev, KEY_CALL, 0);
                input_sync(kpd_accfix_dev);
	  	    }
			
#endif
		}
            }
            else if(current_status == 1)
            {
                accfix_status = MIC_BIAS;		
	            cable_type = HEADSET_MIC;
                ACCFIX_DEBUG("[accfix]MIC_BIAS state not change!\n");
            }
            else if(current_status == 3)
            {
		       accfix_status = PLUG_OUT;		
	           cable_type = NO_DEVICE;
			   #ifdef ACCFIX_EINT
		       disable_accfix();
		       #endif
            }
            else
            {
               ACCFIX_DEBUG("[accfix]MIC_BIAS can't change to this state!\n"); 
            }
            break;

        case DOUBLE_CHECK:
            if(current_status == 0)
            { 
                ///*MT6575 E1*: resolve the hardware issue:
                double_check_workaround();
            }
            else if(current_status == 2)
            {
                //ALPS00053818:when plug in iphone headset half, accdet detects tv-out plug in,
		//and then plug in headset completely, finally plug out it and can't detect plug out
		//Reason: tv signal(32mA) is abnormal by plug in tv cable in tv-out state.
		//Solution:increase debounce2(tv debounce) to a long delay to prevent accdet
		//stay in tv-out state again because of tv signal(32 mA) isn't stable.
		OUTREG32(ACCDET_DEBOUNCE2, 0xffff);

	        accfix_status = TV_OUT;
                cable_type = HEADSET_NO_MIC;
                
	        OUTREG32(ACCDET_CTRL, ACCDET_DISABLE);
	        enable_tv_allwhite_signal(false);
                enable_tv(true);

		if(double_check_flag == 1)			   	
		{					
		      tv_headset_icon = true;			   	
		}
	
            }
            else if(current_status == 3)
            {
	    	///*MT6575 E1*: resolve the hardware issue:
	    	//fast plug out headset after plug in with hook switch pressed, 
                //accdet detect mic bias instead of plug_out state.
                double_check_workaround();
            }
            else
            {
                ACCFIX_DEBUG("[accfix]DOUBLE_CHECK can't change to this state!\n"); 
            }
            break;

        case HOOK_SWITCH:
            if(current_status == 0)
            {
                cable_type = HEADSET_NO_MIC;
	        accfix_status = HOOK_SWITCH;
                ACCFIX_DEBUG("[accfix]HOOK_SWITCH state not change!\n");
	    }
            else if(current_status == 1)
            {
		if (isDown){
		  ACCFIX_DEBUG("[accfix] key UP\n");
		  input_report_key(kpd_accfix_dev, 226 , 0);
		  input_sync(kpd_accfix_dev);
		}  

		accfix_status = MIC_BIAS;		
	        cable_type = HEADSET_MIC;

		//ALPS00038030:reduce the time of remote button pressed during incoming call
                //solution: reduce hook switch debounce time to 0x400
                OUTREG32(ACCDET_DEBOUNCE0, button_press_debounce);

            }
            else if(current_status == 3)
            {
		       accfix_status = PLUG_OUT;		
		       cable_type = NO_DEVICE;
			   #ifdef ACCFIX_EINT
		       disable_accfix();
		       #endif
            }
            else
            {
                ACCFIX_DEBUG("[accfix]HOOK_SWITCH can't change to this state!\n"); 
            }
            break;

        case TV_OUT:	
  	    OUTREG32(ACCDET_DEBOUNCE0, cust_headset_settings.debounce0);
	    OUTREG32(ACCDET_DEBOUNCE3, cust_headset_settings.debounce3);
	    //ALPS00053818: resume debounce2 when jump out from tv-out state.
	    OUTREG32(ACCDET_DEBOUNCE2, tv_out_debounce);
            
            if(current_status == 0)
            {
		OUTREG32(ACCDET_RSV_CON3, ACCDET_DEFVAL_SEL);
		ACCFIX_DEBUG("[accfix]TV-out state: ACCDET_RSV_CON3=0x%x \n\r", INREG32(ACCDET_RSV_CON3));
		
                //work around method	      
		accfix_status = STAND_BY;
		ACCFIX_DEBUG("[accfix]TV out is disabled with tv cable plug in!\n");

		enable_tv(false);
                enable_tv_detect(false);

		OUTREG32(ACCDET_RSV_CON3, 0);
           }
            else if(current_status == 3)
            {
	    		accfix_status = PLUG_OUT;		
				cable_type = NO_DEVICE;
                enable_tv(false);
	        	enable_tv_detect(false);
				#ifdef ACCFIX_EINT
			    disable_accfix();//should  disable accdet here because enable_tv(false) will enalbe accdet
			    #endif
           }
		   else if(current_status == 2)
		   {
		        //ALSP00092273
		        OUTREG32(ACCDET_DEBOUNCE0, 0x4);
		        OUTREG32(ACCDET_DEBOUNCE3, 0x4);
		        
		   }
            else
            {
                ACCFIX_DEBUG("[accfix]TV_OUT can't change to this state!\n"); 
            }
            break;

        case STAND_BY:
            if(current_status == 3)
            {
				accfix_status = PLUG_OUT;		
				cable_type = NO_DEVICE;
				#ifdef ACCFIX_EINT
			    disable_accfix();
			    #endif
            }
            else
            {
                ACCFIX_DEBUG("[accfix]STAND_BY can't change to this state!\n"); 
            }
            break;

        default:
            ACCFIX_DEBUG("[accfix]check_cable_type: accfix current status error!\n");
            break;
            
    }

    if(!IRQ_CLR_FLAG)
    {
        while(INREG32(ACCDET_IRQ_STS) & IRQ_STATUS_BIT)
	{
           ACCFIX_DEBUG("[accfix]check_cable_type: Clear interrupt on-going2....\n");
		   msleep(10);
	}
	CLRREG32(ACCDET_IRQ_STS, IRQ_CLR_BIT);
	IRQ_CLR_FLAG = TRUE;
        ACCFIX_DEBUG("[accfix]check_cable_type:Clear interrupt:Done[0x%x]!\n", INREG32(ACCDET_IRQ_STS));	
    }
    else
    {
        IRQ_CLR_FLAG = FALSE;
    }
 
    if(accfix_status == TV_OUT)
    {
	OUTREG32(ACCDET_CTRL, ACCDET_ENABLE);
    }

    ACCFIX_DEBUG("[accfix]cable type:[%s], status switch:[%s]->[%s]\n",
        accfix_report_string[cable_type], accfix_status_string[pre_status], 
        accfix_status_string[accfix_status]);
}



// judge cable type and implement the most job
void accfix_work_callback(struct work_struct *work)
{
    check_cable_type();

    if(cable_type != DOUBLE_CHECK_TV)
    {
	   switch_set_state((struct switch_dev *)&accfix_data, cable_type);
	   ACCFIX_DEBUG( "[accfix] accfix_work_callback set state %d\n", cable_type);
    }
}


static irqreturn_t accfix_irq_handler(int irq,void *dev_id)
{
    int ret = 0 ;
    
    ACCFIX_DEBUG("[accfix]accfix_irq_handler: %d\n", irq); //decrease top-half ISR cost time
    //disable_irq_nosync(MT6575_ACCDET_IRQ_ID);
    clear_accfix_interrupt();
    
    //zzz call direclty
    //check_cable_type();
    
    ret = queue_work(accfix_workqueue, &accfix_work);	
    if(!ret)
    {
	ACCFIX_DEBUG("[accfix]accfix_irq_handler:accfix_work return:%d!\n", ret);  		
    }
    
    return IRQ_HANDLED;
}

//ACCDET hardware initial
static inline void __init accfix_init(void)
{ 
    ACCFIX_DEBUG("[accfix]accfix hardware init\n");
    //kal_uint8 val;

    //resolve MT6573 ACCDET hardware issue: ear bias supply can make Vref drop obviously 
    //during plug in/out headset then cause modem exception or kernel panic
    //solution: set bit3 of PMIC_RESERVE_CON2 to force MIC voltage to 0 when MIC is drop to negative voltage
    //SETREG32(PMIC_RESERVE_CON2, MIC_FORCE_LOW);

    enable_clock(MT65XX_PDN_PERI_ACCDET,"ACCFIX");
	
    //reset the accdet unit
	OUTREG32(ACCDET_RSTB, RSTB_BIT); 
	OUTREG32(ACCDET_RSTB, RSTB_FINISH_BIT); 

    //init  pwm frequency and duty
    OUTREG32(ACCDET_PWM_WIDTH, REGISTER_VALUE(cust_headset_settings.pwm_width));
    OUTREG32(ACCDET_PWM_THRESH, REGISTER_VALUE(cust_headset_settings.pwm_thresh));

    OUTREG32(ACCDET_EN_DELAY_NUM,
		(cust_headset_settings.fall_delay << 15 | cust_headset_settings.rise_delay));

    // init the debounce time
    OUTREG32(ACCDET_DEBOUNCE0, cust_headset_settings.debounce0);
    OUTREG32(ACCDET_DEBOUNCE1, cust_headset_settings.debounce1);
    OUTREG32(ACCDET_DEBOUNCE3, cust_headset_settings.debounce3);	
	
		
    
    #ifdef ACCFIX_EINT
    // disable ACCDET unit
	pre_state_swctrl = INREG32(ACCDET_STATE_SWCTRL);
    OUTREG32(ACCDET_CTRL, ACCDET_DISABLE);
    OUTREG32(ACCDET_STATE_SWCTRL, 0);
	#else
	
    // enable ACCDET unit
    OUTREG32(ACCDET_STATE_SWCTRL, ACCDET_SWCTRL_EN);
    OUTREG32(ACCDET_CTRL, ACCDET_ENABLE); 
	#endif
    

}

static long accfix_unlocked_ioctl(struct file *file, unsigned int cmd,unsigned long arg)
{
//	bool ret = true;
		
    switch(cmd)
    {
        case ACCFIX_INIT :
		ACCFIX_DEBUG("[accfix]accfix_ioctl : init");
     		break;
            
	case SET_CALL_STATE :
		call_status = (int)arg;
		ACCFIX_DEBUG("[accfix]accfix_ioctl : CALL_STATE=%d \n", call_status);
		break;

	case GET_BUTTON_STATUS :
	    ACCFIX_DEBUG("[accfix]accfix_ioctl : Button_Status=%d (state:%d)\n", button_status, accfix_data.state);	
	    return button_status;
            
	default:
	    ACCFIX_DEBUG("[accfix]accfix_ioctl : default\n");
            break;
  }
    return 0;
}

static int accfix_open(struct inode *inode, struct file *file)
{ 
	ACCFIX_DEBUG("[accfix]accfix_open : init");
   	return 0;
}

static ssize_t accfix_write(struct file *filp, const char *buff, size_t len, loff_t * off)
{ 
	ACCFIX_DEBUG("[accfix]accfix_write : count = %d",len);

	if (len==2) {
	    call_status = 1;
	}   else {
	    call_status = 0;
	}  
   	return -EINVAL;;
}

static int accfix_release(struct inode *inode, struct file *file)
{
    ACCFIX_DEBUG("[accfix]accfix_release ");
    return 0;
}
static struct file_operations accfix_fops = {
	.owner		= THIS_MODULE,
	.write		= accfix_write,
	.unlocked_ioctl		= accfix_unlocked_ioctl,
	.open		= accfix_open,
	.release	= accfix_release,	
};

#if DEBUG_THREAD

int g_start_debug_thread =0;
static struct task_struct *thread = NULL;
int g_dump_register=0;

static int dump_register(void)
{

   int i=0;
   for (i=0; i<= 120; i+=4)
   {
     ACCFIX_DEBUG(" ACCDET_BASE + %x=%x\n",i,INREG32(ACCDET_BASE + i));
   }

 OUTREG32(0xf0007420,0x0111);
 // wirte c0007424 x0d01
 OUTREG32(0xf0007424,0x0d01);
 
   ACCFIX_DEBUG(" 0xc1016d0c =%x\n",INREG32(0xf1016d0c));
   ACCFIX_DEBUG(" 0xc1016d10 =%x\n",INREG32(0xf1016d10));
   ACCFIX_DEBUG(" 0xc209e070 =%x\n",INREG32(0xf209e070));
   ACCFIX_DEBUG(" 0xc0007160 =%x\n",INREG32(0xf0007160));
   ACCFIX_DEBUG(" 0xc00071a8 =%x\n",INREG32(0xf00071a8));
   ACCFIX_DEBUG(" 0xc0007440 =%x\n",INREG32(0xf0007440));

   return 0;
}


extern int mt6575_i2c_polling_writeread(int port, unsigned char addr, unsigned char *buffer, int write_len, int read_len);

static int dump_pmic_register(void)
{ 
	int ret = 0;
	unsigned char data[2];
	
	//u8 data = 0x5f;
	data[0] = 0x5f;
	//ret = mt6575_i2c_polling_writeread(2,0xc2,data,1,1);
	if(ret > 0)
	{
       ACCFIX_DEBUG("dump_pmic_register i2c error");
	}
    ACCFIX_DEBUG("dump_pmic_register 0x5f= %x",data[0]);

    data[0] = 0xc8;
	//ret = mt6575_i2c_polling_writeread(2,0xc0,data,1,1);
	if(ret > 0)
	{
       ACCFIX_DEBUG("dump_pmic_register i2c error");
	}
    ACCFIX_DEBUG("dump_pmic_register 0xc8= %x",data[0]);

	return 0;  
}
static int dbug_thread(void *unused) 
{
   while(g_start_debug_thread)
   	{
      ACCFIX_DEBUG("dbug_thread INREG32(ACCDET_BASE + 0x0008)=%x\n",INREG32(ACCDET_BASE + 0x0008));
	  ACCFIX_DEBUG("[accfix]dbug_thread:sample_in:%x!\n", INREG32(ACCDET_SAMPLE_IN));	
	  ACCFIX_DEBUG("[accfix]dbug_thread:curr_in:%x!\n", INREG32(ACCDET_CURR_IN));
	  ACCFIX_DEBUG("[accfix]dbug_thread:mem_in:%x!\n", INREG32(ACCDET_MEMORIZED_IN));
	  ACCFIX_DEBUG("[accfix]dbug_thread:FSM:%x!\n", INREG32(ACCDET_BASE + 0x0050));
      ACCFIX_DEBUG("[accfix]dbug_thread:IRQ:%x!\n", INREG32(ACCDET_IRQ_STS));
      if(g_dump_register)
	  {
	    dump_register();
		dump_pmic_register(); 
      }

	  msleep(500);

   	}
   return 0;
}
//static ssize_t store_trace_value(struct device_driver *ddri, const char *buf, size_t count)


static ssize_t store_accfix_start_debug_thread(struct device_driver *ddri, const char *buf, size_t count)
{
	
	unsigned int start_flag;
	int error;

	if (sscanf(buf, "%u", &start_flag) != 1) {
		ACCFIX_DEBUG("accfix: Invalid values\n");
		return -EINVAL;
	}

	ACCFIX_DEBUG("[accfix] start flag =%d \n",start_flag);

	g_start_debug_thread = start_flag;

    if(1 == start_flag)
    {
	   thread = kthread_run(dbug_thread, 0, "ACCFIX");
       if (IS_ERR(thread)) 
	   { 
          error = PTR_ERR(thread);
          ACCFIX_DEBUG( " failed to create kernel thread: %d\n", error);
       }
    }

	return count;
}

static ssize_t store_accfix_set_headset_mode(struct device_driver *ddri, const char *buf, size_t count)
{

    unsigned int value;
	//int error;

	if (sscanf(buf, "%u", &value) != 1) {
		ACCFIX_DEBUG("accfix: Invalid values\n");
		return -EINVAL;
	}

	ACCFIX_DEBUG("[accfix]store_accfix_set_headset_mode value =%d \n",value);

	return count;
}

static ssize_t store_accfix_dump_register(struct device_driver *ddri, const char *buf, size_t count)
{
    unsigned int value;
//	int error;

	if (sscanf(buf, "%u", &value) != 1) 
	{
		ACCFIX_DEBUG("accfix: Invalid values\n");
		return -EINVAL;
	}

	g_dump_register = value;

	ACCFIX_DEBUG("[accfix]store_accfix_dump_register value =%d \n",value);

	return count;
}




/*----------------------------------------------------------------------------*/
static DRIVER_ATTR(dump_register,      S_IWUSR | S_IRUGO, NULL,         store_accfix_dump_register);

static DRIVER_ATTR(set_headset_mode,      S_IWUSR | S_IRUGO, NULL,         store_accfix_set_headset_mode);

static DRIVER_ATTR(start_debug,      S_IWUSR | S_IRUGO, NULL,         store_accfix_start_debug_thread);

/*----------------------------------------------------------------------------*/
static struct driver_attribute *accfix_attr_list[] = {
	&driver_attr_start_debug,        
	&driver_attr_set_headset_mode,
	&driver_attr_dump_register,
};

static int accfix_create_attr(struct device_driver *driver) 
{
	int idx, err = 0;
	int num = (int)(sizeof(accfix_attr_list)/sizeof(accfix_attr_list[0]));
	if (driver == NULL)
	{
		return -EINVAL;
	}

	for(idx = 0; idx < num; idx++)
	{
		if((err = driver_create_file(driver, accfix_attr_list[idx])))
		{            
			ACCFIX_DEBUG("driver_create_file (%s) = %d\n", accfix_attr_list[idx]->attr.name, err);
			break;
		}
	}    
	return err;
}
/*----------------------------------------------------------------------------*/

#endif

extern void mt_irq_set_sens(unsigned int irq, unsigned int sens);
extern void mt_irq_set_polarity(unsigned int irq, unsigned int polarity);

static int accfix_probe(struct platform_device *xxx)	
{
	int ret = 0;
#ifdef SW_WORK_AROUND_ACCDET_REMOTE_BUTTON_ISSUE
     struct task_struct *keyEvent_thread = NULL;
	 int error=0;
#endif
	ACCFIX_DEBUG("[accfix]accfix_probe begin!\n");

	//------------------------------------------------------------------
	// 							below register accdet as switch class
	//------------------------------------------------------------------	
		
	//------------------------------------------------------------------
	// 							Create normal device for auido use
	//------------------------------------------------------------------
	ret = alloc_chrdev_region(&accfix_devno, 0, 1, ACCFIX_DEVNAME);
	if (ret)
	{
		ACCFIX_DEBUG("[accfix]alloc_chrdev_region: Get Major number error!\n");			
		return -1;
	} 
		
	accfix_cdev = cdev_alloc();
    accfix_cdev->owner = THIS_MODULE;
    accfix_cdev->ops = &accfix_fops;
    ret = cdev_add(accfix_cdev, accfix_devno, 1);
	if(ret)
	{
		ACCFIX_DEBUG("[accfix]accfix error: cdev_add\n");
		return -1;
	}
	
	
	
	ACCFIX_DEBUG("[accfix]try create classd\n");
	accfix_class = class_create(THIS_MODULE, ACCFIX_DEVNAME);
	if (accfix_class == NULL){
		ACCFIX_DEBUG("[accfix]class_create failed\n");
		return -1; 
	}  else {
		ACCFIX_DEBUG("[accfix]class_create success\n");
	}  
  	
	//ACCFIX_DEBUG("[accfix]try destroy device\n");
	//device_destroy(accfix_class, accfix_devno);  
	//ACCFIX_DEBUG("[accfix]destroy device success\n");
	
    // if we want auto creat device node, we must call this

	accfix_nor_device = device_create(accfix_class, NULL, accfix_devno, NULL, ACCFIX_DEVNAME);  
	if (accfix_nor_device == NULL){
		ACCFIX_DEBUG("[accfix]device_create failed\n");
		return -1; 
	} else {
		ACCFIX_DEBUG("[accfix]device_create success\n");
	}  

	//sys_chmod("/dev/accfix", 777);

	/*ACCFIX_DEBUG("[accfix]try device_rename \n");
    	ret= device_rename(accfix_nor_device, "accdet");  
	if(ret)
	{
	    ACCFIX_DEBUG("[accfix]device_rename returned:%d!\n", ret);
	    return -1;
	} else {
	    ACCFIX_DEBUG("[accfix]device_rename success\n");
	}  

	//zzzzz
	ACCFIX_DEBUG("[accfix] DEBUG EXIT\n");
	return -1;
	*/
	
	
	//switch dev
	accfix_data.name = "h2w_test"; //zzz
	accfix_data.index = 0;
	accfix_data.state = NO_DEVICE;
	
	
	//zzz
	//ACCFIX_DEBUG("[accfix]try switch_dev_unregister returned:%d!\n", ret);
	//switch_dev_unregister(&accfix_data);
	
	ret = switch_dev_register(&accfix_data);
	if(ret)
	{
		ACCFIX_DEBUG("[accfix]switch_dev_register returned:%d!\n", ret);
		return -1; //switch doesnt matter - just an icon in taskbar
	}

//------------------------------------------------------------------make: *** [_module_../med
	// 							Create input device 
	//------------------------------------------------------------------
	kpd_accfix_dev = input_allocate_device();
	if (!kpd_accfix_dev) 
	{
		ACCFIX_DEBUG("[accfix]kpd_accfix_dev : fail!\n");
		return -ENOMEM;
	}
	__set_bit(EV_KEY, kpd_accfix_dev->evbit);
	__set_bit(KEY_CALL, kpd_accfix_dev->keybit);
	__set_bit(KEY_ENDCALL, kpd_accfix_dev->keybit);
    
//longxuewei add
	__set_bit(KEY_F24, kpd_accfix_dev->keybit);
	__set_bit(114, kpd_accfix_dev->keybit);
	__set_bit(115, kpd_accfix_dev->keybit);
	__set_bit(163, kpd_accfix_dev->keybit);
	__set_bit(165, kpd_accfix_dev->keybit);
	__set_bit(226, kpd_accfix_dev->keybit);
//longxuewei add end    
	kpd_accfix_dev->id.bustype = BUS_HOST;
	kpd_accfix_dev->name = "ACCFIX";
	if(input_register_device(kpd_accfix_dev))
	{
		ACCFIX_DEBUG("[accfix]kpd_accfix_dev register : fail!\n");
	}else
	{
		ACCFIX_DEBUG("[accfix]kpd_accfix_dev register : success!!\n");
	} 
	//------------------------------------------------------------------
	// 							Create workqueue 
	//------------------------------------------------------------------	
	accfix_workqueue = create_singlethread_workqueue("accfix");
	INIT_WORK(&accfix_work, accfix_work_callback);

	#ifdef ACCFIX_EINT

    accfix_eint_workqueue = create_singlethread_workqueue("accfix_eint");
	INIT_WORK(&accfix_eint_work, accfix_eint_work_callback);
	accfix_setup_eint();
    #endif	

/* #if defined(SUPPORT_CALLINGLIGHT)
	hrtimer_init(&accfix_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	accfix_timer.function = calling_light_decate;
#endif */
    //------------------------------------------------------------------
	//							wake lock
	//------------------------------------------------------------------
	wake_lock_init(&accfix_suspend_lock, WAKE_LOCK_SUSPEND, "accfix wakelock");
#ifdef SW_WORK_AROUND_ACCDET_REMOTE_BUTTON_ISSUE
     init_waitqueue_head(&send_event_wq);
     //start send key event thread
	 keyEvent_thread = kthread_run(sendKeyEvent, 0, "keyEvent_send");
     if (IS_ERR(keyEvent_thread)) 
	 { 
        error = PTR_ERR(keyEvent_thread);
        ACCFIX_DEBUG( " failed to create kernel thread: %d\n", error);
     }
#endif
	
	#if DEBUG_THREAD

	if((ret = accfix_create_attr(&accfix_driver.driver)))
	{
		ACCFIX_DEBUG("create attribute err = %d\n", ret);
		
	}

	#endif

	/* For early porting before audio driver add */
	//temp_func();
	ACCFIX_DEBUG("[accfix]accfix_probe : ACCFIX_INIT\n");  
	if (g_accfix_first == 1) 
	{	
		long_press_time_ns = (s64)long_press_time * NSEC_PER_MSEC;
				
		//Accdet Hardware Init
		accfix_init();
                
		//mt6575_irq_set_sens(MT6575_ACCDET_IRQ_ID, MT65xx_EDGE_SENSITIVE);
		mt_irq_set_sens(MT_ACCDET_IRQ_ID, MT65xx_LEVEL_SENSITIVE);
		mt_irq_set_polarity(MT_ACCDET_IRQ_ID, MT65xx_POLARITY_LOW);
		//register accdet interrupt
		
		ret =  request_irq(MT_ACCDET_IRQ_ID, accfix_irq_handler, 0, "ACCFIX", NULL);
		if(ret)
		{
			ACCFIX_DEBUG("[accfix]accfix register interrupt error\n");
			ACCFIX_DEBUG("[accfix]try free IRQ\n");
			free_irq(MT_ACCDET_IRQ_ID,NULL);
			ACCFIX_DEBUG("[accfix]IRQ freed success\n");
			ret =  request_irq(MT_ACCDET_IRQ_ID, accfix_irq_handler, 0, "ACCFIX", NULL);
			if(ret){
			  ACCFIX_DEBUG("[accfix]accfix register interrupt error twice\n");
			}  
		}
                
		queue_work(accfix_workqueue, &accfix_work); //schedule a work for the first detection					
		g_accfix_first = 0;
	}

        ACCFIX_DEBUG("[accfix]accfix_probe done!\n");
	return 0;
}

static int accfix_remove(struct platform_device *xxx)	
{
	ACCFIX_DEBUG("[accfix]accfix_remove begin!\n");
	if(g_accfix_first == 0)
	{
		free_irq(MT_ACCDET_IRQ_ID,NULL);
	}
    
	//cancel_delayed_work(&accfix_work);
	#ifdef ACCFIX_EINT
	destroy_workqueue(accfix_eint_workqueue);
	#endif
	destroy_workqueue(accfix_workqueue);
	switch_dev_unregister(&accfix_data);
	device_del(accfix_nor_device);
	class_destroy(accfix_class);
	cdev_del(accfix_cdev);
	unregister_chrdev_region(accfix_devno,1);	
	input_unregister_device(kpd_accfix_dev);
	ACCFIX_DEBUG("[accfix]accfix_remove Done!\n");
    
	return 0;
}

static int accfix_suspend(struct platform_device *dev, pm_message_t state)  // only one suspend mode
{
  
    int i=0;
    ACCFIX_DEBUG("[accfix]accfix_suspend\n");
    //close vbias
    //SETREG32(0xf0007500, (1<<7));
    //before close accdet clock we must clear IRQ done
    while(INREG32(ACCDET_IRQ_STS) & IRQ_STATUS_BIT)
	{
           ACCFIX_DEBUG("[accfix]check_cable_type: Clear interrupt on-going3....\n");
		   msleep(10);
	}
	while(INREG32(ACCDET_IRQ_STS))
	{
	  msleep(10);
	  CLRREG32(ACCDET_IRQ_STS, IRQ_CLR_BIT);
	  IRQ_CLR_FLAG = TRUE;
      ACCFIX_DEBUG("[accfix]check_cable_type:Clear interrupt:Done[0x%x]!\n", INREG32(ACCDET_IRQ_STS));	
	}
    while(i<50 && (INREG32(ACCDET_BASE + 0x0050)!=0x0))
	{
	  // wake lock
	  wake_lock(&accfix_suspend_lock);
	  msleep(10); //wait for accdet finish IRQ generation
	  g_accfix_working_in_suspend =1;
	  ACCFIX_DEBUG("suspend wake lock %d\n",i);
	  i++;
	}
	if(1 == g_accfix_working_in_suspend)
	{
	  wake_unlock(&accfix_suspend_lock);
	  g_accfix_working_in_suspend =0;
	  ACCFIX_DEBUG("suspend wake unlock\n");
	}

	ACCFIX_DEBUG("[accfix]suspend:sample_in:%x!\n curr_in:%x!\n mem_in:%x!\n FSM:%x!\n"
		, INREG32(ACCDET_SAMPLE_IN)
		,INREG32(ACCDET_CURR_IN)
		,INREG32(ACCDET_MEMORIZED_IN)
		,INREG32(ACCDET_BASE + 0x0050));

#ifdef ACCFIX_EINT

    if(INREG32(ACCDET_CTRL)&& call_status == 0)
    {
	   //record accdet status
	   ACCFIX_DEBUG("[accfix]accfix_working_in_suspend\n");
	   g_accfix_working_in_suspend = 1;
       pre_state_swctrl = INREG32(ACCDET_STATE_SWCTRL);
	   // disable ACCDET unit
       OUTREG32(ACCDET_CTRL, ACCDET_DISABLE);
       OUTREG32(ACCDET_STATE_SWCTRL, 0);
    }
#else
    // disable ACCDET unit
    if(call_status == 0)
    {
       pre_state_swctrl = INREG32(ACCDET_STATE_SWCTRL);
       OUTREG32(ACCDET_CTRL, ACCDET_DISABLE);
       OUTREG32(ACCDET_STATE_SWCTRL, 0);
       disable_clock(MT65XX_PDN_PERI_ACCDET,"ACCFIX");
    }
#endif	

    ACCFIX_DEBUG("[accfix]accfix_suspend: ACCDET_CTRL=[0x%x]\n", INREG32(ACCDET_CTRL));
    ACCFIX_DEBUG("[accfix]accfix_suspend: ACCDET_STATE_SWCTRL=[0x%x]->[0x%x]\n", 
        pre_state_swctrl,INREG32(ACCDET_STATE_SWCTRL));
    ACCFIX_DEBUG("[accfix]accfix_suspend ok\n");


	return 0;
}

static int accfix_resume(struct platform_device *dev) // wake up
{
    ACCFIX_DEBUG("[accfix]accfix_resume\n");
#ifdef ACCFIX_EINT

	if(1==g_accfix_working_in_suspend &&  0== call_status)
	{
       // enable ACCDET unit	
       OUTREG32(ACCDET_STATE_SWCTRL, pre_state_swctrl);
       OUTREG32(ACCDET_CTRL, ACCDET_ENABLE); 
       //clear g_accfix_working_in_suspend
	   g_accfix_working_in_suspend =0;
	   ACCFIX_DEBUG("[accfix]accfix_resume : recovery accfix register\n");
	   
	}
#else
	if(call_status == 0)
	{
       enable_clock(MT65XX_PDN_PERI_ACCDET,"ACCFIX");

       // enable ACCDET unit	
  
       OUTREG32(ACCDET_STATE_SWCTRL, pre_state_swctrl);
       OUTREG32(ACCDET_CTRL, ACCDET_ENABLE); 
	}
#endif
    ACCFIX_DEBUG("[accfix]accfix_resume: ACCDET_CTRL=[0x%x]\n", INREG32(ACCDET_CTRL));
    ACCFIX_DEBUG("[accfix]accfix_resume: ACCDET_STATE_SWCTRL=[0x%x]\n", 
        INREG32(ACCDET_STATE_SWCTRL));
	ACCFIX_DEBUG("[accfix]resum:sample_in:%x!\n curr_in:%x!\n mem_in:%x!\n FSM:%x!\n"
		, INREG32(ACCDET_SAMPLE_IN)
		,INREG32(ACCDET_CURR_IN)
		,INREG32(ACCDET_MEMORIZED_IN)
		,INREG32(ACCDET_BASE + 0x0050));
    ACCFIX_DEBUG("[accfix]accfix_resume ok\n");
    return 0;
}

#if 0
struct platform_device accfix_device = {
	.name	  ="Accdet_Driver_Hacked",		
	.id		  = -1,	
	.dev    ={
	.release = accfix_dumy_release,
	}
};
#endif

static struct platform_driver accfix_driver = {
	.probe		= accfix_probe,	
	.suspend	= accfix_suspend,
	.resume		= accfix_resume,
	.remove   = accfix_remove,
	.driver     = {
	.name       = "Accfix_Driver",
	},
};

static int __init accfix_mod_init(void)
{
	int ret = 0;

	ACCFIX_DEBUG("[accfix]accfix_mod_init begin 3!\n");

     //ACCFIX_DEBUG("[accfix]Try unregister old driver 1!\n");
     //platform_driver_unregister(&accfix_driver);
    
    ACCFIX_DEBUG("[accfix]Try register new driver 1!\n");
    ret = platform_driver_register(&accfix_driver);
	if (ret) {
		ACCFIX_DEBUG("[accfix]platform_driver_register error:(%d)\n", ret);
		return ret;
	}
	else
	{
		ACCFIX_DEBUG("[accfix]platform_driver_register done 1!\n");
	}

    
    ACCFIX_DEBUG("[accfix]try accfix_probe 3!\n");
    accfix_probe(NULL);

    ACCFIX_DEBUG("[accfix]accfix_mod_init done 1!\n");
    return 0;

}

static void __exit accfix_mod_exit(void)
{
	ACCFIX_DEBUG("[accfix]accfix_mod_exit\n");
	
	//accfix_remove(NULL);
	//ACCFIX_DEBUG("[accfix]accfix_mod_remove Done!\n");
	//platform_device_unregister(&accfix_device);
	
	platform_driver_unregister(&accfix_driver);
	ACCFIX_DEBUG("[accfix]accfix_mod_exit Done!\n");

}


module_init(accfix_mod_init);
module_exit(accfix_mod_exit);


module_param(debug_enable,int,0644);
module_param(call_status,int,0644);

MODULE_DESCRIPTION("Hacked MTK MT6575 ACCDET driver");
MODULE_AUTHOR("Anny <Anny.Hu@mediatek.com>");
MODULE_LICENSE("GPL");
MODULE_ALIAS("accfix");



