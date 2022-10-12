        # qemu -kernel loads the kernel at 0x80000000
        # and causes each hart (i.e. CPU) to jump there.
        # kernel.ld causes the following code to
        # be placed at 0x80000000.
.section .text
.global _entry
_entry:
        # set up a stack for C.
        # stack0 is declared in start.c,
        # with a 4096-byte stack per CPU.
        # sp = stack0 + (hartid * 4096)
        la sp, stack0 # load address of stack0 to sp register
        li a0, 1024*4 # per hart has 4kB space of stack.
        csrr a1, mhartid # get current hart id to a1
        addi a1, a1, 1 # the stack grows downward, so `sp` of hart0 is stack0 + 4096*(0+1)
        mul a0, a0, a1
        add sp, sp, a0 # sp of hart0 is stack0 + 4096*1 , of hart1 is stack0 + 4096*2
        # jump to start() in start.c
        call start # Entry C language world, say bye to assembly
spin:
        j spin
