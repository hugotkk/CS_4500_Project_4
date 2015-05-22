#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/pid.h>

asmlinkage int sys_get_wrr_weight(int pid){
	struct task_struct *task = pid_task(find_get_pid(pid), PIDTYPE_PID);
	if(task == NULL)return -1;
	return task->wrr.weight;
}
