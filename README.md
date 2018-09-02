# CSC 501 Project 1: Process Container

## Description


## Step
### Kernel Compilation
```shell
cd kernel_module
sudo make
sudo make install
cd ..
```

### User Space Library Compilation
```shell
cd library
sudo make
sudo make install
cd ..
```

### Benchmark Compilation
```shell
cd benchmark
make
cd ..
```

### Run
```shell
./test.sh
```

## Useful Kernel Functions/Variables
### Mutex
```c
mutex_init(struct mutex *lock);
mutex_lock(struct mutex *lock);
mutex_unlock(struct mutex *lock);
```

### Schedule
```c
// functions
wake_up_process(struct task_struct *p);
set_current_state(volatile long state);
schedule();

// variables
volatile long TASK_INTERRUPTIBLE;
volatile long TASK_RUNNING;
struct task_struct *current;
```

### Memory Allocation
```c
// functions
kmalloc(size_t size, gfp_t flags);
kcalloc(size_t n, size_t size, gfp_t flags);
kfree(const void * objp);

// variables
gfp_t GFP_KERNEL;
```

### Debug Message
```c
// functions
printk(const char *fmt, ...);
```