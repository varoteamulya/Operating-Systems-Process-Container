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
  struct task_struct *pthread;
  struct list_head list;
};

struct container_list
{
   __u64 cid;
   struct thread_list head;
   struct list_head list;
};

//static struct task_struct *current;
extern struct container_list containerHead;
int addThread(struct container_list *, bool);
struct container_list *isConatinerPresent(__u64);
void createContainer(__u64, bool);
int simple_thread_first(void);
void thread_cleanup(void) ;

static DEFINE_MUTEX(lock);

//Method to check if the container is present
struct container_list *isConatinerPresent(__u64 id)
{
   printk("iscontainerpresent\n ");
   struct container_list *temp;
   struct list_head *pos,*p;
   //Traversing the list
   list_for_each_safe(pos,p,&containerHead.list)
    {
      temp = list_entry(pos, struct container_list, list);
      if(temp!=NULL && temp->cid == id)
	{
	 return temp;
	}
    }
    return NULL;
}

struct thread_list *isThreadPresent(struct container_list *cont, pid_t pid)
{
   printk("isThreadPresent\n ");
   struct thread_list *tThreadTemp;
   struct list_head *p,*q;
   list_for_each_safe(p, q,&((cont->head).list))
   {
      tThreadTemp = list_entry(p, struct thread_list, list);
      if(tThreadTemp!=NULL && tThreadTemp->pthread->pid == pid)
      {
        printk("thread pid matched: %uld\n", pid);
        return tThreadTemp;
      }
   }
   return NULL;
}

int addThread(struct container_list *container, bool firstThread){
     printk("add thread to container ");
     struct thread_list *tTmp;
     tTmp = (struct thread_list *)kmalloc(sizeof(struct thread_list), GFP_KERNEL);
     mutex_lock(&lock);
     tTmp->pthread = current;
     list_add(&(tTmp->list), &((container->head).list));
     mutex_unlock(&lock);

    return 0;
}

void createContainer(__u64 kcid, bool isFirstThread)
{
     printk("creating the container ");
     struct container_list *tmp;
     //Creating a new container
     tmp = (struct container_list *)kmalloc(sizeof(struct container_list), GFP_KERNEL);
     tmp->cid = kcid;
     INIT_LIST_HEAD(&tmp->head.list);
     //Adding the container to the list
     mutex_lock(&lock);
     list_add(&(tmp->list),&(containerHead.list));
     mutex_unlock(&lock);
     struct thread_list *intermediateThread;
     intermediateThread = isThreadPresent(tmp, current->pid);
     if(intermediateThread == NULL)
     {
         addThread(tmp, isFirstThread);
     }
}

void deleteThread(struct container_list *cont)
{
     printk("deleting the thread ");
     mutex_lock(&lock);
     list_del_init(&((cont->head).list));
     mutex_unlock(&lock);
     //kfree(cont->head);
}

void deleteContainer(__u64 dcid)
{
     printk("deleting the container ");
     struct container_list *dtmp;
     struct list_head *pos,*p;
     list_for_each_safe(pos,p,&containerHead.list)
     {
        printk("Looping container to delete them\n");
        dtmp = list_entry(pos,struct container_list, list);
        if(dtmp!=NULL && dtmp->cid ==dcid)
        {
           mutex_lock(&lock);
           list_del_init(pos);
           mutex_unlock(&lock);
           kfree(dtmp);
        }
     }
}

struct thread_list *nextThreadInLoop(struct task_struct *myThread, __u64 cId)
{
     printk("inside nextThreadLoop\n");
     struct container_list *cnTemp;
     struct list_head *npo, *nq, *npo2,*nq2;
     struct thread_list *tnTemp;
     list_for_each_safe(npo, nq, &containerHead.list)
     {
        cnTemp = list_entry(npo, struct container_list,list);
        if(cnTemp!=NULL && cnTemp->cid == cId)
        {
printk("IsContainer matching loop\n");
             list_for_each_safe(npo2, nq2, &((cnTemp->head).list))
             {
                 printk("Found the thread head \n");
                 tnTemp = list_entry(npo2,struct thread_list, list );
                 if(tnTemp!=NULL && tnTemp->pthread->pid == myThread->pid)
                 {
                    printk("Thread pid matched\n");
                     if(list_is_last(npo2, &((cnTemp->head).list)))
                     {
                        printk("first return\n");
                        return list_first_entry(&((cnTemp->head).list), struct thread_list, list);
                     }
                     printk("Second return\n");
	           return list_entry(npo2->next, struct thread_list, list);
                 }
             }
       }

     }
      return NULL;
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
    struct list_head *pos,*p;
    struct container_list *dcTemp;
    copy_from_user(&kdcmd, (void __user*)user_cmd, sizeof(struct processor_container_cmd));
    printk("container in which thread is being deleted is: %llu \n", kdcmd.cid);

    struct container_list *tmp = isConatinerPresent(kdcmd.cid);
    struct thread_list *t_head = &(tmp->head);
    struct thread_list *ttpos = isThreadPresent(tmp, current->pid);
    struct thread_list *nextThreadInContainerLoop = NULL;
    nextThreadInContainerLoop =  nextThreadInLoop(current, kdcmd.cid);
    printk("poiter to th is \n");
    mutex_lock(&lock);
    wake_up_process(nextThreadInContainerLoop->pthread);
      printk("I am deleting the thread here\n");
      list_del(&ttpos->list);
      kfree(ttpos);
    mutex_unlock(&lock);
    printk(" Deleted the therad now ");
    if(list_empty(&t_head->list))
    {
        printk("Container ids are matchingfor delete: %llu\n", kdcmd.cid);
        mutex_lock(&lock);
        list_del(&tmp->list);
        kfree(tmp);
        mutex_unlock(&lock);
        printk("I am out of del");
    }
  schedule();
  printk("Problem is it ?\n");
//    list_for_each_safe(pos,p,&containerHead.list)
  //  {
    //   dcTemp = list_entry(p,struct container_list, list);
      // printk("checking if container id is matching\n");

       //if(dcTemp!=NULL && dcTemp->cid == kdcmd.cid )
       //{
         //  printk("Container ids are matching: %llu\n", kdcmd.cid);
          // deleteThread(dcTemp);
          // printk("I am out of del");
          // if(dcTemp->head !=NULL){
           //wake_up_process(&((dcTemp->head).pthread));}
          // printk("Can I wake you up now ?");
       //}
      // if(dcTemp!=NULL && dcTemp->head ==NULL)
       //{
         //  printk("Deleted all threads in the container\n ");
           //deleteContainer(kdcmd.cid);
       //}
//       else if(dcTemp!= NULL && &((dcTemp->head).pthread)!=NULL)
  //     {
    //       printk("waking up the sleeping process %llu \n", dcTemp->cid);
      //     wake_up_process(&((dcTemp->head).pthread));
       //}
     //}
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
    printk("Container id received from user space is %llu :", kcmd.cid);
    //Checking if the container is present
    intermediate = isConatinerPresent(kcmd.cid);
    if(intermediate == NULL)
     {
       printk("entering here %llu", kcmd.cid);
       //Creating the container if it is not present
       createContainer(kcmd.cid, true);
       set_current_state(TASK_RUNNING);
       schedule();
     }
    else{
       printk("Container is present: %llu\n ", kcmd.cid);
       addThread(intermediate, false);
       set_current_state(TASK_INTERRUPTIBLE);
       schedule();
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
    struct processor_container_cmd kccmd;
    printk(" Entering context switch\n");
    copy_from_user(&kccmd, (void __user*)user_cmd, sizeof(struct processor_container_cmd));

    struct container_list *contextContainer = isConatinerPresent(kccmd.cid);
    struct thread_list *t_head = &(contextContainer->head);
    struct thread_list *thread_switch = &(contextContainer->head);
   // mutex_lock(&lock);
    printk("Container id here in context switch is %llu\n", kccmd.cid);

    if(contextContainer != NULL)
    {
        printk("Cont Id inside contSwitch %llu", kccmd.cid);
        //struct thread_list *contextThread = isThreadPresent(contextContainer,current->pid);
        struct thread_list *nextThread = NULL;

        mutex_lock(&lock);
        nextThread =  nextThreadInLoop(current,kccmd.cid);
        list_rotate_left(&t_head->list);
        wake_up_process(nextThread->pthread);
        set_current_state(TASK_INTERRUPTIBLE);
        mutex_unlock(&lock);
        schedule();
        
        printk("End of task rotation loop\n");
     }
    printk("Done dana done\n");
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
