/*
 * rtc_sync.c: module that syncs system time with rtc time
 * and vice versa with 10 seconds interval
 *
 * Copyright (C) 2018 Sierra Wireless.
 *
 * This file is licensed under the terms of the GNU General Public License
 * version 2.  This program is licensed "as is" without any warranty of any
 * kind, whether express or implied.
 */

/* IMPORTANT: the RTC only stores whole seconds. It is arbitrary
 * whether it stores the most close value or the value with partial
 * seconds truncated. However, it is important that we use it to store
 * the truncated value. This is because otherwise it is necessary,
 * in an rtc sync function, to read both xtime.tv_sec and
 * xtime.tv_nsec. On some processors (i.e. ARM), an atomic read
 * of >32bits is not possible. So storing the most close value would
 * slow down the sync API. So here we have the truncated value and
 * the best guess is to add 0.5s.
 */

#include <linux/module.h>
#include <linux/rtc.h>
#include <linux/timer.h>
#include <linux/time.h>
#include <linux/sched.h>
#include <linux/kthread.h>

static struct task_struct *time_check_thread;

/*
 * this function syncs system time with RTC when startup
 * if there is valid time store in RTC static int rtc_hctosys(void)
 */
static int rtc_hctosys(void)
{
	int err;
	struct rtc_time tm;
	struct rtc_device *rtc = rtc_class_open("rtc1");
	if (rtc == NULL) {
		printk("%s: unable to open rtc device (rtc1) for rtc_hctosys\n",
			__FILE__);
		return -ENODEV;
	}

	err = rtc_read_time(rtc, &tm);
	if (err == 0) {
		err = rtc_valid_tm(&tm);
		if (err == 0) {
			struct timespec tv;

			tv.tv_nsec = NSEC_PER_SEC >> 1;

			rtc_tm_to_time(&tm, &tv.tv_sec);

			do_settimeofday(&tv);

			dev_info(rtc->dev.parent,
				"setting system clock to "
				"%d-%02d-%02d %02d:%02d:%02d UTC (%u)\n",
				tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
				tm.tm_hour, tm.tm_min, tm.tm_sec,
				(unsigned int) tv.tv_sec);
		} else { 
			dev_err(rtc->dev.parent,
				"hctosys: invalid date/time\n");
		}
	} else {
		dev_err(rtc->dev.parent,
			"hctosys: unable to read the hardware clock\n");
	}

	rtc_class_close(rtc);

	return 0;
}

/*systohc sets RTC with system time*/
static int systohc(struct timespec now)
{
	struct rtc_device *rtc;
	struct rtc_time tm;
	int res = -ENODEV;
	printk("Setting time to %lld\n", timespec_to_ns(&now));

	if (now.tv_nsec < (NSEC_PER_SEC >> 1))
		rtc_time_to_tm(now.tv_sec, &tm);
	else
		rtc_time_to_tm(now.tv_sec + 1, &tm);

	rtc = rtc_class_open("rtc1");

	if (rtc == NULL) {
		printk("%s: unable to open rtc device (rtc1) for systohc\n",
			__FILE__);
		return res;
	}

	/* rtc_hctosys exclusively uses UTC, so we call set_time here,
	* not set_mmss. */
	if (rtc->ops && (rtc->ops->set_time || rtc->ops->set_mmss))
		res = rtc_set_time(rtc, &tm);
	rtc_class_close(rtc);

	return res;
}

/*
 * time_check chekcs if the system time has been modified during
 * the last checking period by comparing the expected time after
 * checking period with a time allowance static int time_check(void *data)
 */
static int time_check(void *data)
{
	while (!kthread_should_stop()) {
		const long long expected_delta_ns = 10LL * NSEC_PER_SEC;
		long timeout = 10L * HZ;
		struct timespec before;
		struct timespec after;
		long long delta_ns;

		ktime_get_real_ts(&before);
		while (timeout > 0) {
			/* schedule_timeout requires the task state to be set
			 * to either TASK_INTERRUPTIBLE or TASK_UNINTERRUPTIBLE,
			 * otherwise the kernel will not put the task on the sleep list.
			 * If you have any critical processing you need done in rtc
			 * then just change this to TASK_UNINTERRUPTIBLE - i.e no signals.
			 */
			set_current_state(TASK_INTERRUPTIBLE);
			timeout = schedule_timeout(timeout);
		}
		ktime_get_real_ts(&after);

		delta_ns = timespec_to_ns(&after) - timespec_to_ns(&before);
		if (abs64(delta_ns - expected_delta_ns) > 1 * NSEC_PER_SEC)
			systohc(after);
	}
	return 0;
}

static int __init rtc_sync_init(void)
{
	rtc_hctosys();
	time_check_thread = kthread_run(time_check, NULL, "check_time");
	return 0;
}

static void __exit rtc_sync_exit(void)
{
	if (time_check_thread) {
		kthread_stop(time_check_thread);
	}
}

module_init(rtc_sync_init);
module_exit(rtc_sync_exit);

MODULE_DESCRIPTION("Syncs system time with rtc time when waked up from ULPM. Checks if system time has been changed during a given period, if changed, saves system time to rtc.");
MODULE_LICENSE("GPL v2");

