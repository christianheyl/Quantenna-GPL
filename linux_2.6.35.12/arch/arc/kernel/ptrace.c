/*
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  vineetg: Jan 2010
 *      -Enabling dynamic printk for fun
 *      -str to id conversion for ptrace requests and reg names
 *      -Consistently use offsets wrt "struct user" for all ptrace req
 *      -callee_reg accessed using accessor to implement alternate ways of
 *       fetcing it (say using dwarf unwinder)
 *      -Using generic_ptrace_(peek|poke)data
 *
 * Soam Vasani, Kanika Nema: Codito Technologies 2004
 */

#include <linux/kernel.h>
#include <linux/ptrace.h>
#include <linux/sched.h>
#include <linux/user.h>
#include <linux/mm.h>
#include <linux/elf.h>
#include <linux/regset.h>
#include <asm/unistd.h>
#include <asm/uaccess.h>

#define DBG(fmt, args...) pr_debug(fmt , ## args)

typedef struct {
    int num_entries;
    struct {
        int id;
        char *str;
    } entry [ ];
}
id_to_str_tbl_t;

char *get_str_from_id(int id, id_to_str_tbl_t *tbl)
{
    int i;
    for ( i=0; i < tbl->num_entries; i++) {
        if ( id == tbl->entry[i].id )
            return tbl->entry[i].str;
    }

    return "(null)";
}

static id_to_str_tbl_t  req_names = {
    //.num_entries = (sizeof(req_names) - sizeof(req_names.num_entries))/sizeof(req_names.entry[0]),
    .num_entries = 16,
    {
        {PTRACE_PEEKTEXT, "peek-text"},
        {PTRACE_PEEKDATA, "peek-data"},
        {PTRACE_PEEKUSR, "peek-user"},
        {PTRACE_POKETEXT, "poke-text"},
        {PTRACE_POKEDATA, "poke-data"},
        {PTRACE_POKEUSR, "poke-user"},
        {PTRACE_CONT, "continue...."},
        {PTRACE_KILL, "kill $#%#$$"},
        {PTRACE_ATTACH, "attach"},
        {PTRACE_DETACH, "detach"},
        {PTRACE_GETREGS, "getreg"},
        {PTRACE_SETREGS, "setreg"},
        {PTRACE_SETOPTIONS, "set-option"},
        {PTRACE_SETSIGINFO, "set-siginfo"},
        {PTRACE_GETSIGINFO, "get-siginfo"},
        {PTRACE_GETEVENTMSG, "get-event-msg"}
    }
};

static id_to_str_tbl_t  reg_names = {
    .num_entries = 42,
    {
        {0, "res1"},
        {4, "BTA"},
        {8, "LPS"},
        {12, "LPE"},
        {16, "LPC"},
        {20, "STATUS3"},
        {24, "PC"},
        {28, "BLINK"},
        {32, "FP"},
        {36, "GP"},
        {40, "r12"},
        {44, "r11"},
        {48, "r10"},
        {52, "r9"},
        {56, "r8"},
        {60, "r7"},
        {64, "r6"},
        {68, "r5"},
        {72, "r4"},
        {76, "r3"},
        {80, "r2"},
        {84, "r1"},
        {88, "r0"},
        {92, "ORIG r0 (sys-call only)"},
        {96, "ORIG r8 (event type)"},
        {100, "SP"},
        {104, "res2"},
        {108, "r25"},
        {112, "r24"},
        {116, "r23"},
        {120, "r22"},
        {124, "r21"},
        {128, "r20"},
        {132, "r19"},
        {136, "r18"},
        {140, "r17"},
        {144, "r16"},
        {148, "r15"},
        {152, "r14"},
        {156, "r13"},
        {160, "(EFA) Break Pt addr"},
        {164, "STOP PC"}
    }
};

static struct callee_regs *task_callee_regs(struct task_struct *tsk, struct callee_regs *calleereg)
{
    struct callee_regs *tmp = (struct callee_regs *)tsk->thread.callee_reg;
    return tmp;
}

void ptrace_disable(struct task_struct *child)
{
  /* DUMMY: dummy function to resolve undefined references */
}

unsigned long
getreg(unsigned int offset,struct task_struct *child)
{
    unsigned int *reg_file;
    struct pt_regs *ptregs = task_pt_regs(child);
    unsigned int data = 0;

    if(offset < offsetof(struct user_regs_struct, reserved2)) {
        reg_file = (unsigned int *)ptregs;
        data = reg_file[offset/4];
    }
    else if (offset <  offsetof(struct user_regs_struct, efa)) {
        struct callee_regs calleeregs;
        reg_file = (unsigned int *)task_callee_regs(child, &calleeregs);
        data = reg_file[(offset - offsetof(struct user_regs_struct, reserved2))/4];
    }
    else if(offset == offsetof(struct user_regs_struct,efa)) {
        data = child->thread.fault_address;
    }
    else if(offset == offsetof(struct user_regs_struct,stop_pc))
    {
        if(in_brkpt_trap(ptregs))
        {
            data = child->thread.fault_address;
            DBG("\t\tstop_pc (brk-pt)\n");
        }
        else
        {
            data = ptregs->ret;
            DBG("\t\tstop_pc (others)\n");
        }
    }

    DBG("\t\t%s:0x%2x (%s) = %x\n", __FUNCTION__,offset,
            get_str_from_id(offset, &reg_names), data);

    return data;
}


void
setreg(unsigned int offset, unsigned int data, struct task_struct *child)
{
    unsigned int *reg_file;
    struct pt_regs *ptregs = task_pt_regs(child);

    DBG("\t\t%s:0x%2x (%s) = 0x%x\n", __FUNCTION__,offset,
            get_str_from_id(offset, &reg_names),data);

    if(offset ==offsetof(struct user_regs_struct, reserved1) ||
        offset == offsetof(struct user_regs_struct, reserved2) ||
        offset ==  offsetof(struct user_regs_struct,efa)) {
        printk("Bogus ptrace setreg request\n");
        return;
    }

    if(offset == offsetof(struct user_regs_struct,stop_pc))
    {
        DBG("\t\tstop_pc (others)\n");
        ptregs->ret = data;
    }
    else if(offset < offsetof(struct user_regs_struct, reserved2)) {
        reg_file = (unsigned int *)ptregs;
        reg_file[offset/4] = data;
    }
    else if (offset < offsetof(struct user_regs_struct, efa)) {
        struct callee_regs calleeregs;
        reg_file = (unsigned int *)task_callee_regs(child, &calleeregs);
        offset -= offsetof(struct user_regs_struct, reserved2);
        reg_file[offset/4] = data;
    }
    else {
        printk("Bogus ptrace setreg request\n");
    }
}

long arch_ptrace(struct task_struct *child, long request, long addr, long data)
{
    int ret;
    int i;
    unsigned long tmp;

    if (! (request == PTRACE_PEEKTEXT || request == PTRACE_PEEKDATA ||
           request == PTRACE_PEEKUSR )) {
        DBG("REQ=%ld (%s), ADDR =0x%lx, DATA=0x%lx)\n",
            request, get_str_from_id(request, &req_names),addr, data);
    }

    switch (request) {

    /* Memory Read
     * From location @addr in @child into location @data
     */
    case PTRACE_PEEKTEXT:
    case PTRACE_PEEKDATA:
            ret = generic_ptrace_peekdata(child, addr, data);
            DBG("\tPeek-data @ 0x%lx = 0x%lx \n",addr, data);
            break;

    case PTRACE_POKETEXT: /* write the word at location addr. */
    case PTRACE_POKEDATA:
            ret = generic_ptrace_pokedata(child, addr, data);
            break;

    case PTRACE_SYSCALL:   /* Stop at ret of sys call and next sys call */
    case PTRACE_CONT:       /* restart simply or say after a signal. */
        ret = -EIO;
        if (!valid_signal(data))
            break;
        if (request == PTRACE_SYSCALL)
            set_tsk_thread_flag(child, TIF_SYSCALL_TRACE);
        else
            clear_tsk_thread_flag(child, TIF_SYSCALL_TRACE);

        child->exit_code = data;
        wake_up_process(child);
        ret = 0;
        break;

    case PTRACE_GETREGS: {
        for(i=0; i< sizeof(struct user_regs_struct)/4;i++)
        {
            /* getreg wants a byte offset */
            tmp = getreg(i << 2,child);
            ret = put_user(tmp,((unsigned long *)data) + i);
            if(ret < 0)
                goto out;
        }
        break;
    }

    case PTRACE_SETREGS: {
        for(i=0; i< sizeof(struct user_regs_struct)/4; i++)
        {
            unsigned long tmp;
            ret = get_user(tmp, (((unsigned long *)data) + i));
            if(ret < 0)
                goto out;
            setreg(i << 2, tmp, child);
        }

        ret = 0;
        break;
    }

        /*
         * make the child exit.  Best I can do is send it a sigkill.
         * perhaps it should be put in the status that it wants to
         * exit.
         */
    case PTRACE_KILL: {
        ret = 0;
        if (child->exit_state == EXIT_ZOMBIE)   /* already dead */
            break;
        child->exit_code = SIGKILL;
        wake_up_process(child);
        break;
    }

    case PTRACE_DETACH:
        /* detach a process that was attached. */
        ret = ptrace_detach(child, data);
        break;

    /* U-AREA Read (Registers, signal etc)
     * From offset @addr in @child's struct user into location @data
     */
    case PTRACE_PEEKUSR: {
        /* user regs */
        if(addr > (unsigned)offsetof(struct user,regs) &&
           addr < (unsigned)offsetof(struct user,regs) + sizeof(struct user_regs_struct))
        {
            DBG("\tPeek-usr\n");
            tmp = getreg(addr,child);
            ret = put_user(tmp, ((unsigned long *)data));
        }
        /* signal */
        else if(addr == (unsigned)offsetof(struct user, signal))
        {
            tmp = current->exit_code; /* set by syscall_trace */
            ret = put_user(tmp, ((unsigned long *)data));
        }

        /* nothing else is interesting yet*/
        else if(addr > 0 && addr < sizeof(struct user))
            return -EIO;
        /* out of bounds */
        else
            return -EIO;

        break;
    }

    case PTRACE_POKEUSR: {
        ret = 0;
        if(addr ==(int) offsetof(struct user_regs_struct,efa))
            return -EIO;

        if(addr > (unsigned)offsetof(struct user,regs) &&
           addr < (unsigned)offsetof(struct user,regs) + sizeof(struct user_regs_struct))
        {
            setreg(addr, data, child);
        }
        else
            return -EIO;

        break;
    }

        /*
         * Single step not supported.  ARC's single step bit won't
         * work for us.  It is in the DEBUG register, which the RTI
         * instruction does not restore.
         */
    case PTRACE_SINGLESTEP:

    default:
        ret = ptrace_request(child, request, addr, data);
        break;
    }

out:
    return ret;

}

asmlinkage void syscall_trace(void)
{
    if (!(current->ptrace & PT_PTRACED))
        return;
    if (!test_thread_flag(TIF_SYSCALL_TRACE))
        return;

    /* The 0x80 provides a way for the tracing parent to distinguish
       between a syscall entry/exit stop and SIGTRAP delivery */
    ptrace_notify(SIGTRAP | ((current->ptrace & PT_TRACESYSGOOD) ?
                             0x80 : 0));

    /*
     * this isn't the same as continuing with a signal, but it will do
     * for normal use.  strace only continues with a signal if the
     * stopping signal is not SIGTRAP.  -brl
     */
    if (current->exit_code) {
        send_sig(current->exit_code, current, 1);
        current->exit_code = 0;
    }
}

static int arc_regs_get(struct task_struct *target,
             const struct user_regset *regset,
             unsigned int pos, unsigned int count,
             void *kbuf, void __user *ubuf)
{
    DBG("arc_regs_get %p %d %d\n", target, pos, count);

    if (kbuf) {
        unsigned long *k = kbuf;
        while (count > 0) {
            *k++ = getreg(pos, target);
            count -= sizeof(*k);
            pos += sizeof(*k);
        }
    }
    else {
        unsigned long __user *u = ubuf;
        while (count > 0) {
            if (__put_user(getreg(pos, target), u++))
                return -EFAULT;
            count -= sizeof(*u);
            pos += sizeof(*u);
        }
    }

    return 0;
}

static int arc_regs_set(struct task_struct *target,
             const struct user_regset *regset,
             unsigned int pos, unsigned int count,
             const void *kbuf, const void __user *ubuf)
{
    int ret = 0;

    DBG("arc_regs_set %p %d %d\n", target, pos, count);

    if (kbuf) {
        const unsigned long *k = kbuf;
        while (count > 0) {
            setreg(pos, *k++, target);
            count -= sizeof(*k);
            pos += sizeof(*k);
        }
    }
    else {
        const unsigned long __user *u = ubuf;
        unsigned long word;
        while (count > 0) {
            ret = __get_user(word,u++);
            if (ret)
                return ret;
            setreg(pos, word, target);
            count -= sizeof(*u);
            pos += sizeof(*u);
        }
    }

    return 0;
}

static const struct user_regset arc_regsets[] = {
    [0] = {
        .core_note_type = NT_PRSTATUS,
        .n = ELF_NGREG,
        .size = sizeof(long),
        .align = sizeof(long),
        .get = arc_regs_get,
        .set = arc_regs_set
    }
};

static const struct user_regset_view user_arc_view = {
    .name = UTS_MACHINE,
    .e_machine = EM_ARCTANGENT,
    .regsets = arc_regsets,
    .n = ARRAY_SIZE(arc_regsets)
};

const struct user_regset_view *task_user_regset_view(struct task_struct *task)
{
    return &user_arc_view;
}

