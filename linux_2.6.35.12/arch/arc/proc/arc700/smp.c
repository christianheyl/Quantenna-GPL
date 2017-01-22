/*
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * RajeshwarR: Dec 11, 2007
 *   -- Added support for Inter Processor Interrupts
 *
 * Vineetg: Nov 1st, 2007
 *    -- Initial Write (Borrowed heavily from ARM)
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/spinlock.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/profile.h>
#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/cpu.h>
#include <linux/smp.h>
#include <linux/irq.h>

#include <asm/atomic.h>
#include <asm/processor.h>
#include <asm/percpu.h>
#include <asm/idu.h>
#include <asm/mmu_context.h>
#include <linux/delay.h>

extern int _int_vec_base_lds;
extern struct task_struct *_current_task[NR_CPUS];

extern void wakeup_secondary(void);
extern void first_lines_of_secondary(void);
void smp_ipi_init(void);
extern void board_setup_timer(void);
extern void setup_processor(void);

/*
 * bitmask of present and online CPUs.
 * The present bitmask indicates that the CPU is physically present.
 * The online bitmask indicates that the CPU is up and running.
 */
cpumask_t cpu_possible_map;
EXPORT_SYMBOL(cpu_possible_map);
cpumask_t cpu_online_map;
EXPORT_SYMBOL(cpu_online_map);

secondary_boot_t secondary_boot_data;

/* Called from start_kernel */
void __init smp_prepare_boot_cpu(void)
{

}

/*
 * Initialise the CPU possible map early - this describes the CPUs
 * which may be present or become present in the system.
 */
void __init smp_init_cpus(void)
{
    unsigned int i;

    for (i = 0; i < NR_CPUS; i++)
        cpu_set(i, cpu_possible_map);
}

/* called from init ( ) =>  process 1 */
void __init smp_prepare_cpus(unsigned int max_cpus)
{
    int i;

    /*
     * Initialise the present map, which describes the set of CPUs
     * actually populated at the present time.
     */
    for (i = 0; i < max_cpus; i++)
        cpu_set(i, cpu_present_map);
}

void __init smp_cpus_done(unsigned int max_cpus)
{

}

asmlinkage void __cpuinit start_kernel_secondary(void)
{
    struct mm_struct *mm = &init_mm;
    unsigned int cpu = smp_processor_id();

    /* Do CPU init
        1. Detect CPU Type and its config
        2. TLB Init
        3. Setup Vector Tbl Base Address
        4. Maskoff all IRQs
        IMP!!! Dont do any fancy stuff inside this call as we have
        not yet setup mm contexts etc yet
     */
    setup_processor();

    _current_task[cpu] = current;

    atomic_inc(&mm->mm_users);
    atomic_inc(&mm->mm_count);
    current->active_mm = mm;
    cpu_set(cpu, mm->cpu_vm_mask);

    // TODO-vineetg: need to implement this call
    //enter_lazy_tlb(mm, current);

    cpu_set(cpu, cpu_online_map);

    /* vineetg Nov 19th 2007:
        For this printk to work in ISS, a bridge.dll instance in the
        uart address space is needed so that uart access by non-owner
        CPU are shunted to the other CPU which owns the UART.
     */

    printk(KERN_INFO "## CPU%u LIVE ##: Executing Code...\n", cpu);

    board_setup_timer();

    smp_ipi_init();

    /* Enable the interrupts on the local cpu */
    local_irq_enable();

    /* Note that preemption needs to be disabled before entering
       cpu_idle. When it sees need_resched it enables it momentarily
       But most of then time, idle is running, preemption is disabled
     */
    preempt_disable();
    cpu_idle();
}

/* At this point, Secondary Processor  is "HALT"ed:
        -It booted, but was halted in head.S
        -It was configured to halt-on-reset
       So need to wake it up.
   Essential requirements being where to run from (PC) and stack (SP)
*/
int __cpuinit __cpu_up(unsigned int cpu)
{
    struct task_struct *idle;
    unsigned long wait_till;

    idle = fork_idle(cpu);
    if (IS_ERR(idle)) {
        printk(KERN_ERR "CPU%u: fork() failed\n", cpu);
        return PTR_ERR(idle);
    }

    /* TODO-vineetg: 256 is arbit for now.
       copy_thread saves some things near top of kernel stack.
       Thus leaving some margin for actual stack to begin
       Need to revisit this later
    */
    secondary_boot_data.stack = task_stack_page(idle) + THREAD_SIZE - 256;

    secondary_boot_data.c_entry = first_lines_of_secondary;
    secondary_boot_data.cpu_id = cpu;

    printk(KERN_INFO "Trying to bring up CPU%u ...\n", cpu);

    wakeup_secondary();

    /* vineetg, Dec 11th 2007, Boot waits for 2nd to come-up
       Wait for 1 sec for 2nd CPU to comeup and then chk it's online bit
       jiffies is incremented every tick and 1 sec has "HZ" num of ticks
       jiffies + HZ => wait for 1 sec
     */

    // TODO-vineetg: workaround for 3.4 bug, replace the 3 with HZ later
    wait_till = jiffies + 3; //HZ;
    while ( time_before(jiffies, wait_till)) {
        if (cpu_online(cpu))
            break;
    }

    if ( !cpu_online(cpu)) {
        printk(KERN_INFO "Timeout: CPU%u FAILED to comeup !!!\n", cpu);
        return -1;
    }

    return 0;
}


/*
 * not supported here
 */
int __init setup_profiling_timer(unsigned int multiplier)
{
    return -EINVAL;
}



/*****************************************************************************/
/*              Inter Processor Interrupt Handling                           */
/*****************************************************************************/


/*
 * structures for inter-processor calls
 * A Collection of single bit ipi messages
 *
 */

//TODO_rajesh investigate timer, tlb and stop message types.
enum ipi_msg_type {
    IPI_RESCHEDULE,
    IPI_CALL_FUNC,
    IPI_CPU_STOP,
};


struct ipi_data {
    spinlock_t lock;
    unsigned long ipi_count;
    unsigned long bits;
};

static DEFINE_PER_CPU(struct ipi_data, ipi_data) = {
        .lock = SPIN_LOCK_UNLOCKED,
};

struct smp_call_struct {
    void (*func)(void *info);
    void *info;
    int wait;
    cpumask_t pending;
    cpumask_t unfinished;
};

static struct smp_call_struct* volatile smp_call_function_data;
static DEFINE_SPINLOCK(smp_call_function_lock);



static void send_ipi_message(cpumask_t callmap, enum ipi_msg_type msg)
{
    unsigned long flags;
    unsigned int cpu;

    local_irq_save(flags);

    for_each_cpu_mask(cpu, callmap) {
        struct ipi_data *ipi = &per_cpu(ipi_data, cpu);

        spin_lock(&ipi->lock);
        ipi->bits |= 1 << msg;
        spin_unlock(&ipi->lock);
    }

    /*
     * Call the platform specific cross-CPU call function.
     */

    for_each_cpu_mask(cpu, callmap)
        idu_irq_assert(cpu);

    local_irq_restore(flags);
}


void smp_send_reschedule(int cpu)
{
    send_ipi_message(cpumask_of_cpu(cpu), IPI_RESCHEDULE);
}


void smp_send_stop(void)
{
    cpumask_t mask = cpu_online_map;
    cpu_clear(smp_processor_id(), mask);
    send_ipi_message(mask, IPI_CPU_STOP);
}

/**
 * smp_call_function(): Run a function on all other CPUs.
 * @func: The function to run. This must be fast and non-blocking.
 * @info: An arbitrary pointer to pass to the function.
 * @nonatomic: currently unused.
 * @wait: If true, wait (atomically) until function has completed on other CPUs.
 *
 * Returns 0 on success, else a negative status code. Does not return until
 * remote CPUs are nearly ready to execute <<func>> or are or have executed.
 *
 * You must not call this function with disabled interrupts or from a
 * hardware interrupt handler or from a bottom half handler.
 */
int smp_call_function (void (*func) (void *info), void *info, int nonatomic,
            int wait)
{
    struct smp_call_struct data;
    cpumask_t callmap = cpu_online_map;
    int ret = 0;

    data.func = func;
    data.info = info;
    data.wait = wait;

    cpu_clear(smp_processor_id(), callmap);
    if (cpus_empty(callmap))
        goto out;

    data.pending = callmap;
    if (wait)
        data.unfinished = callmap;

    /*
     * try to get the mutex on smp_call_function_data
     */
    spin_lock(&smp_call_function_lock);
    smp_call_function_data = &data;

    send_ipi_message(callmap, IPI_CALL_FUNC);

    /* Wait for response */
    while (!cpus_empty(data.pending))
        barrier();

    // TODO_rajesh Incase of arm, it times out,
    // should we have to timeout? timeout value?

    if (wait)
        while (!cpus_empty(data.unfinished))
            barrier();

    smp_call_function_data = NULL;
    spin_unlock(&smp_call_function_lock);

out:
    return ret;
}



/*
 * ipi_call_function - handle IPI from smp_call_function()
 *
 * We copy data out of the cross-call structure and then
 * let the caller know that we are here and done with their data
 *
 */
static void ipi_call_function (unsigned int cpu)
{
    struct smp_call_struct *data = smp_call_function_data;
    void (*func)(void *info) = data->func;
    void *info = data->info;
    int wait = data->wait;

    cpu_clear(cpu, data->pending);

    func(info);

    if (wait)
        cpu_clear (cpu, data->unfinished);
}

/*
 * ipi_cpu_sto - handle IPI from smp_send_stop()
 *
 */
static void ipi_cpu_stop(unsigned int cpu)
{
    __asm__("flag 1");
}

/*
 * Main handler for inter-processor interrupts
 *
 */

irqreturn_t do_IPI (int irq, void *dev_id, struct pt_regs *regs)
{
    unsigned int cpu = smp_processor_id();
    struct ipi_data *ipi = &per_cpu(ipi_data, cpu);

    ipi->ipi_count++;

    idu_irq_clear((IDU_INTERRUPT_0 + cpu));

    for(;;) {
        unsigned long msgs;

        spin_lock (&ipi->lock);
        msgs = ipi->bits;
        ipi->bits = 0;
        spin_unlock (&ipi->lock);

        if (!msgs)
            break;

        do {
            unsigned long nextmsg;
            nextmsg = msgs & -msgs;
            msgs &= ~nextmsg;


            nextmsg = ffz(~nextmsg);

            switch (nextmsg) {
                case IPI_RESCHEDULE:

                    /* Do nothing, on return from interrupt, resched flag
                       will be checked anyways
                     */
                    break;

                case IPI_CALL_FUNC:
                    ipi_call_function (cpu);
                    break;

                case IPI_CPU_STOP:
                    ipi_cpu_stop (cpu);
                    break;

                default:
                    printk(KERN_CRIT"CPU%u: Unknown IPI message 0x%lx\n",
                            cpu, nextmsg);
                    break;
            }
        } while (msgs);
    }

    return IRQ_HANDLED;
}


static struct irq_node ipi_intr[NR_CPUS];

void smp_ipi_init(void)
{
    // Owner of the Idu Interrupt determines who is SELF
    int cpu = smp_processor_id();

    // Check if CPU is configured for more than 16 interrupts
    // TODO_rajesh: what error should we report at this point
    if(NR_IRQS <=16 || get_hw_config_num_irq() <= 16)
        BUG();

    // Setup the interrupt in IDU
    idu_disable();

#ifdef CONFIG_ARCH_ARC800
    idu_irq_set_tgtcpu(cpu, /* IDU IRQ assoc with CPU */
                        (0x1 << cpu) /* target cpus mask, here single cpu*/
                        );

    idu_irq_set_mode(cpu,  /* IDU IRQ assoc with CPu */
                     IDU_IRQ_MOD_TCPU_ALLRECP,
                     IDU_IRQ_MODE_PULSE_TRIG);

#endif

    idu_enable();

    // Install the interrupts
    ipi_intr[cpu].handler = do_IPI;
    ipi_intr[cpu].flags = 0;
    ipi_intr[cpu].disable_depth = 0;
    ipi_intr[cpu].dev_id = NULL;
    ipi_intr[cpu].devname = "IPI Interrupt";
    ipi_intr[cpu].next = NULL;

    /* Setup ISR as well as enable IRQ on CPU */
    setup_arc_irq((IDU_INTERRUPT_0 + cpu), &ipi_intr[cpu]);
}

