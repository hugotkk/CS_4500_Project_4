/*
 * Weighted Round Robin Scheduling Class (mapped to the SCHED_WRR
 * policy)
 */

#include <linux/slab.h>

#define TIME_SLICE		DEF_TIMESLICE
#define GROUP_TIME_SLICE	TIME_SLICE *  6 // This can be changed 



static struct sched_wrr_supraentity* find_wrr_user(struct list_head *user_list, int userid){

	struct sched_wrr_supraentity *user, *next;
	struct sched_wrr_supraentity *ret = NULL;
	list_for_each_entry_safe(user, next, user_list, run_list_users){
		if(user->user_id == userid){
			ret = user;
			break;
		}
	}
	return ret;
}


 
static void enqueue_task_wrr(struct rq *rq, struct task_struct *p, int wakeup){

    struct sched_wrr_supraentity *wrr_supraentity = find_wrr_user(&(rq->wrr.ready_tasks), p->wrr_group);
    if(!wrr_supraentity){
		wrr_supraentity = kmalloc( sizeof(struct sched_wrr_supraentity), GFP_ATOMIC);
		if(!wrr_supraentity){
			printk(KERN_EMERG "Can't find the user in the wrr queue, enqueue failed \n");	
			//we should abort the process or something			
			return;
		}
		INIT_LIST_HEAD(&wrr_supraentity->run_list_users);
		INIT_LIST_HEAD(&wrr_supraentity->run_list);
		wrr_supraentity->user_id = p->wrr_group;
		wrr_supraentity->current_time_slice = GROUP_TIME_SLICE;
		wrr_supraentity->nr_processes = 0;
		rq->wrr.wrr_nr_users++;
		list_add_tail(&(wrr_supraentity->run_list_users), &(rq->wrr.ready_tasks)); //we introduce the supraentity to the rq	
    }
	
    list_add_tail(&(p->wrr.run_list), &(wrr_supraentity->run_list)); //we introduce the task on the supraentity list
    rq->wrr.wrr_nr_running++;
	wrr_supraentity->nr_processes++;

}

static void dequeue_task_wrr(struct rq *rq, struct task_struct *p, int sleep){

	struct sched_wrr_supraentity *wrr_supraentity = find_wrr_user(&(rq->wrr.ready_tasks), p->wrr_group);
	if(!wrr_supraentity){
		printk(KERN_EMERG "Can't find the user in the wrr queue, dequeue failed \n");
	return;
	}
	list_del_init(&(p->wrr.run_list)); // we delete the current task from the ready tasks
	wrr_supraentity->nr_processes--;
	if(wrr_supraentity->nr_processes == 0){
		rq->wrr.wrr_nr_users--;
		list_del_init(&(wrr_supraentity->run_list_users));
		kfree(wrr_supraentity);
	}
	
    rq->wrr.wrr_nr_running--;
}

static void requeue_wrr_entity(struct wrr_rq *wrr_rq, struct sched_wrr_supraentity *wrr_se, int head){
	if(wrr_rq->wrr_nr_users <= 1)return;
	list_move_tail(&(wrr_se->run_list_users), &(wrr_rq->ready_tasks)); //listhead of the entity gets reintroduced to the ready tasks list.
}

static void requeue_task_wrr(struct rq *rq, struct task_struct *p, int head){
	struct sched_wrr_supraentity *wrr_supraentity = find_wrr_user(&(rq->wrr.ready_tasks), p->wrr_group);
	if(!wrr_supraentity){
		printk(KERN_EMERG "Can't find the user in the wrr queue, requeue failed \n");
		return;
	}
   if(wrr_supraentity->nr_processes <= 1)return;  
   list_move_tail(&(p->wrr.run_list), &(wrr_supraentity->run_list)); //listhead of the entity gets reintroduced to the ready tasks list.
}

static void yield_task_wrr(struct rq *rq){
}
static void check_preempt_curr_wrr(struct rq *rq, struct task_struct *p, int flags){
}

static struct task_struct *pick_next_task_wrr(struct rq *rq){
	struct sched_wrr_entity *my_container;
	struct sched_wrr_supraentity *my_supracontainer;
	struct list_head *head;
	struct task_struct *p = NULL;

    if(rq->wrr.wrr_nr_running <= 0)return NULL; // if there is no processes to run
	head = &(rq->wrr.ready_tasks);
	my_supracontainer = list_entry(head->next, struct sched_wrr_supraentity, run_list_users);
	head = &(my_supracontainer->run_list);	 
    my_container = list_entry(head->next, struct sched_wrr_entity, run_list);
    if(my_container)p = my_container->p;
    else return NULL;
    return p;
}

static void put_prev_task_wrr(struct rq *rq, struct task_struct *p){
}

#ifdef CONFIG_SMP
static int select_task_rq_wrr(struct task_struct *p, int sd_flag, int flags){
	return task_cpu(p);
}

static unsigned long
load_balance_wrr(struct rq *this_rq, int this_cpu, struct rq *busiest,
                unsigned long max_load_move,
                struct sched_domain *sd, enum cpu_idle_type idle,
                int *all_pinned, int *this_best_prio)
{
        /* don't touch WRR tasks */
        return 0;
}

static int
move_one_task_wrr(struct rq *this_rq, int this_cpu, struct rq *busiest,
                 struct sched_domain *sd, enum cpu_idle_type idle)
{
        return 0;
}


#endif
static void set_curr_task_wrr(struct rq *rq){
	struct task_struct *p = rq->curr;

	p->se.exec_start = rq->clock;
}

static void task_tick_wrr(struct rq *rq, struct task_struct *p, int queued){
	struct sched_wrr_supraentity *wrr_supraentity = find_wrr_user(&(rq->wrr.ready_tasks), p->wrr_group);
	if(!wrr_supraentity){
			printk(KERN_EMERG "Can't find the user in the wrr queue, tick failed \n");	
			//we should abort the process or something			
			return;
	}
	--p->wrr.time_slice;
	--wrr_supraentity->current_time_slice;

    if(p->wrr.time_slice > 0 &&  wrr_supraentity->current_time_slice > 0)return;
	if (wrr_supraentity->current_time_slice == 0){ //the group has expired its time 
		wrr_supraentity->current_time_slice = GROUP_TIME_SLICE;
		requeue_wrr_entity(&rq->wrr, wrr_supraentity, 0);
	}
	if(p->wrr.time_slice == 0){ //the process has expired its time
		p->wrr.time_slice = TIME_SLICE * p->wrr.weight;
		requeue_task_wrr(rq, p,0);
	}
    resched_task(p); //we flag that this process needs to be rescheduled
}

unsigned int get_rr_interval_wrr(struct task_struct *task){
        /*
 *          * Time slice is 0 for SCHED_FIFO tasks
 *                   */
        if (task->policy == SCHED_WRR)
                return DEF_TIMESLICE;
        else
                return 0;
}
/* added by Jia Rao: No preemption, so we leave this function empty */
static void prio_changed_wrr(struct rq *rq, struct task_struct *p,
                              int oldprio, int running){
}

static void switched_to_wrr(struct rq *rq, struct task_struct *p,
                           int running){
}

static const struct sched_class wrr_sched_class = {
	.next			= &idle_sched_class,
	.enqueue_task		= enqueue_task_wrr,
	.dequeue_task		= dequeue_task_wrr,
	.yield_task		= yield_task_wrr,

	.check_preempt_curr	= check_preempt_curr_wrr,

	.pick_next_task		= pick_next_task_wrr,
	.put_prev_task		= put_prev_task_wrr,

#ifdef CONFIG_SMP
	.select_task_rq		= select_task_rq_wrr,

	.load_balance		= load_balance_wrr,
	.move_one_task		= move_one_task_wrr,
#endif

	.set_curr_task          = set_curr_task_wrr,
	.task_tick		= task_tick_wrr,

	.get_rr_interval	= get_rr_interval_wrr,

//	.prio_changed		= prio_changed_wrr,
	.switched_to		= switched_to_wrr,
};

