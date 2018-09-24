//////////////////////////////////////////////////////////////////////
//                      North Carolina State University
//
//
//
//                             Copyright 2016
//
////////////////////////////////////////////////////////////////////////
//
// This program is free software; you can redistribute it and/or modify it
// under the terms and conditions of the GNU General Public License,
// version 2, as published by the Free Software Foundation.
//
// This program is distributed in the hope it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
// more details.
//
// You should have received a copy of the GNU General Public License along with
// this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
//
////////////////////////////////////////////////////////////////////////
//
//   Author:  Hung-Wei Tseng, Yu-Chia Liu
//
//   Description:
//     Core of Kernel Module for Processor Container
//
////////////////////////////////////////////////////////////////////////

#include "processor_container.h"

#include <asm/uaccess.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/poll.h>
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/list.h>

struct thread_list
{
  struct list_head list;
};

struct container_list
{
   __u64 cid;
   struct thread_list *head;
   struct list_head list;
};

static struct task_struct *current;
extern struct list_head containerHead;
int addThread(struct container_list *, bool);
struct container_list *isConatinerPresent(__u64);
void createContainer(__u64, bool);
int simple_thread_first(void);
void thread_cleanup(void) ;

//Method to check if the container is present
struct container_list *isConatinerPresent(__u64 id)
{
   printk("iscontainerpresent ");
   struct container_list *temp;
   struct list_head *pos;
   //Traversing the list
   list_for_each(pos,&containerHead)
    {
      temp = list_entry(pos, struct container_list, list);
      if(temp!=NULL && temp->cid == id)
	{
	 return temp;
	}
    }
    return NULL;
}

int addThread(struct container_list *container, bool firstThread){
     printk("add thread to container ");
     struct thread_list *tTmp;
     tTmp = (struct thread_list *)kmalloc(sizeof(struct thread_list), GFP_KERNEL);
     list_add(&(tTmp->list), &(container->head));

     current = kthread_create(simple_thread_first,NULL, "simple_thread");
      //if(current)
      //{
       // printk(KERN_INFO "inside kthread create for current ");
        //wake_up_process(current);
      //}
      //else
        //return 0;
    return 0;
}

int simple_thread_first(void)
{
   while(!kthread_should_stop())
   {
      set_current_state(TASK_RUNNING);
   }
 return 0;
}

void thread_cleanup(void) {
 int ret;
 ret = kthread_stop(current);
 if(!ret)
  printk(KERN_INFO "Thread stopped");

}

void createContainer(__u64 kcid, bool isFirstThread)
{
     printk("creating the container ");
     struct container_list *tmp;
     //Creating a new container
     tmp = (struct container_list *)kmalloc(sizeof(struct container_list), GFP_KERNEL);
     tmp->cid = kcid;
     //Adding the container to the list
     list_add(&(tmp->list),&(containerHead));
     addThread(&tmp, isFirstThread);
}

void deleteThread(struct container_list *cont)
{
     printk("deleting the thread ");
     list_del(cont->head);
     //kfree(cont->head);
}

void deleteContainer(__u64 dcid)
{
     printk("deleting the container ");
     struct container_list *dtmp;
     struct list_head *po;
     list_for_each(po, &containerHead)
     {
        dtmp = list_entry(po,struct container_list, list);
        if(dtmp!=NULL && dtmp->cid ==dcid)
        {
         list_del(po);
         kfree(dtmp);
        }
     }
}

/**
 * Delete the task in the container.
 *
 * external functions needed:
 * mutex_lock(), mutex_unlock(), wake_up_process(),
 */
int processor_container_delete(struct processor_container_cmd __user *user_cmd)
{
    printk("Inside pcontainer delete\n");
    struct processor_container_cmd kdcmd;
    struct list_head *p;
    struct container_list *dcTemp;
    copy_from_user(&kdcmd, (void __user*)user_cmd, sizeof(struct processor_container_cmd));
    printk("container in which thread is being deleted is %llu", kdcmd.cid);
    list_for_each(p,&containerHead)
    {
       dcTemp = list_entry(p,struct container_list, list);
       if(dcTemp!=NULL && dcTemp->cid == kdcmd.cid )
       {
           deleteThread(&dcTemp);
       }

       if(list_empty(&(dcTemp->head)))
       {
           deleteContainer(kdcmd.cid);
       }
    }
    return 0;
}

/**
 * Create a task in the corresponding container.
 * external functions needed:
 * copy_from_user(), mutex_lock(), mutex_unlock(), set_current_state(), schedule()
 *
 * external variables needed:
 * struct task_struct* current
 */
int processor_container_create(struct processor_container_cmd __user *user_cmd)
{
    printk("Inside pcontainer create\n");
    struct processor_container_cmd kcmd;
    struct container_list *intermediate;
    //Reading the inputs from the user space
    copy_from_user(&kcmd, (void __user*)user_cmd, sizeof(struct processor_container_cmd));
    printk("Container id received from user space is %llu", kcmd.cid);
    //Checking if the container is present
    intermediate = isConatinerPresent(kcmd.cid);
    if(intermediate == NULL)
     {
       printk("entering here %llu", kcmd.cid);
       //Creating the container if it is not present
       createContainer(kcmd.cid, true);
     }
    else{
       addThread(&intermediate, false);
    }

    if(list_empty(&(intermediate->head)))
    {
       printk("Thread list is empty for %llu", kcmd.cid);
    }
    return 0;
}

/**
 * switch to the next task within the same container
 * 
 * external functions needed:
 * mutex_lock(), mutex_unlock(), wake_up_process(), set_current_state(), schedule()
 */
int processor_container_switch(struct processor_container_cmd __user *user_cmd)
{
    return 0;
}

/**
 * control function that receive the command in user space and pass arguments to
 * corresponding functions.
 */
int processor_container_ioctl(struct file *filp, unsigned int cmd,
                              unsigned long arg)
{
    switch (cmd)
    {
    case PCONTAINER_IOCTL_CSWITCH:
        return processor_container_switch((void __user *)arg);
    case PCONTAINER_IOCTL_CREATE:
        return processor_container_create((void __user *)arg);
    case PCONTAINER_IOCTL_DELETE:
        return processor_container_delete((void __user *)arg);
    default:
        return -ENOTTY;
    }
}
