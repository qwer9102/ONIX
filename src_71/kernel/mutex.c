#include <onix/mutex.h>
#include <onix/interrupt.h>
#include <onix/assert.h>
#include <onix/task.h>

// 初始化互斥量
void mutex_init(mutex_t *mutex)   
{
    mutex->value = false;         //初始化为没有被人占用,一般的是初始化为资源量
    list_init(&mutex->waiters);
}

 // 尝试持有互斥量
void mutex_lock(mutex_t *mutex)
{
    bool intr = interrupt_disable();

    task_t *current = running_task();
    //这里true表示已经被人占用
    while(mutex->value == true)
    {
        task_block(current,&mutex->waiters,TASK_BLOCKED);
    }
    //无人持有,why???
    assert(mutex->value == false);

    mutex->value++;
    assert(mutex->value == true);

    set_interrupt_state(intr);
    
}

// 释放互斥量
void mutex_unlock(mutex_t *mutex)
{
    bool intr = interrupt_disable();
    
    assert(mutex->value == true);

    //取消持有
    mutex->value--;
    assert(mutex->value == false);

    //唤醒链表最后一个节点
    if(!list_empty(&mutex->waiters))
    {
        task_t *task = element_entry(task_t,node,mutex->waiters.tail.prev);
        assert(task->magic == ONIX_MAGIC);
        task_unblock(task);
        task_yield();   //避免进程饿死
    }

    set_interrupt_state(intr);
}


// 锁初始化
void lock_init(lock_t *lock)
{
    lock->holder = NULL;
    lock->repeat = 0;
    mutex_init(&lock->mutex);
}  

 // 加锁
void lock_acquire(lock_t *lock)
{
    task_t *current = running_task();
    //已经有人加过锁后释放:之前不是自己加锁情形
    if(lock->holder != current)
    {
        mutex_lock(&lock->mutex);
        lock->holder = current;
        assert(lock->repeat == 0);
        lock->repeat = 1;
    }
    //已经加锁的人就是自己
    else
    {
        lock->repeat++;
    }
} 

 // 解锁:只能加锁者解锁
void lock_release(lock_t *lock)
{
    task_t *current = running_task();
    assert(lock->holder == current);
    if(lock->repeat > 1)
    {
        lock->repeat--;
        return;
    }
    //repeat = 1情形
    assert(lock->repeat == 1);
    lock->holder = NULL;
    lock->repeat = 0;
    mutex_unlock(&lock->mutex);
}