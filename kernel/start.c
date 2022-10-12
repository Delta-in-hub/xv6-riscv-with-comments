#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "defs.h"

void main();
void timerinit();

// entry.S needs one stack per CPU.
//!  In the standard RISC-V calling convention, the stack pointer is ** always kept 16-byte aligned **.
__attribute__ ((aligned (16))) char stack0[4096 * NCPU];

// a scratch area per CPU for machine-mode timer interrupts.
uint64 timer_scratch[NCPU][5];

// assembly code in kernelvec.S for machine-mode timer interrupt.
extern void timervec();

// entry.S jumps here in machine mode on stack0.
void
start()
{
  // set M Previous Privilege mode to Supervisor, for mret.
  unsigned long x = r_mstatus();
  x &= ~MSTATUS_MPP_MASK; // "xPP holds the previous privilege mode (x=M,S or U). The xPP fields can only hold privilege modes up to x, so MPP is two bits wide, SPP is one bit wide, and UPP is implicitly zero."
  x |= MSTATUS_MPP_S; // set mpp to supervisor mode(0b01)
  w_mstatus(x);

  // set M Exception Program Counter to main, for mret.
  // requires gcc -mcmodel=medany
  w_mepc((uint64)main);

  // disable paging for now.
  w_satp(0); // 想要禁用页表地址翻译，需要将mode字段赋值0. 全部赋0更方便.

  // delegate all interrupts and exceptions to supervisor mode.
  // 默认情况下，任何特权级别的所有陷阱都在机器模式下处理
  // 为了提高性能，可以在 medeleg 和 mideleg 对单个位的写入来指定特定的异常和中断应该由 RISC-V 那种特权模式来处理。（通常就是 M 模式委托给 S 模式）
  w_medeleg(0xffff);
  w_mideleg(0xffff);

  w_sie(r_sie() | SIE_SEIE | SIE_STIE | SIE_SSIE);

  // configure Physical Memory Protection to give supervisor mode
  // access to all of physical memory.
  w_pmpaddr0(0x3fffffffffffffull); // 13 * 4 + 2 = 54(count 1 start from low bit)
  //0xf = 0b1111 , L = 1 , A = 3 , X = 1 , W = 1 , R = 1
  // PMP entry is locked,readable,writable,execable.
  // NAPOT: base=0x0 , range = 2^(3+54)
  w_pmpcfg0(0xf);

  // ask for clock interrupts.
  timerinit();

  // keep each CPU's hartid in its tp register, for cpuid().
  int id = r_mhartid();
  w_tp(id); // tp [thread pointer]

  // switch to supervisor mode and jump to main().
  asm volatile("mret");
}

// arrange to receive timer interrupts.
// they will arrive in machine mode at
// at timervec in kernelvec.S,
// which turns them into software interrupts for
// devintr() in trap.c.
void
timerinit()
{
  // each CPU has a separate source of timer interrupts.
  int id = r_mhartid();

  // ask the CLINT for a timer interrupt.
  // RISCV_ACLINT_DEFAULT_TIMEBASE_FREQ = 10000000
  // https://github.com/qemu/qemu/blob/e46e2628e9fcce39e7ae28ac8c24bcc643ac48eb/include/hw/intc/riscv_aclint.h#L78
  int interval = 1000000; // cycles; about 1/10th second in qemu.
  // A timer interrupt becomes pending whenever `mtime` contains a value greater than or equal to `mtimecmp`
  *(uint64*)CLINT_MTIMECMP(id) = *(uint64*)CLINT_MTIME + interval;

  // prepare information in scratch[] for timervec.
  // scratch[0..2] : space for timervec to save registers.
  // scratch[3] : address of CLINT MTIMECMP register.
  // scratch[4] : desired interval (in cycles) between timer interrupts.
  uint64 *scratch = &timer_scratch[id][0];
  scratch[3] = CLINT_MTIMECMP(id);
  scratch[4] = interval;
  w_mscratch((uint64)scratch);

  // set the machine-mode trap handler.
  w_mtvec((uint64)timervec);

  // enable machine-mode interrupts.
  w_mstatus(r_mstatus() | MSTATUS_MIE);

  // enable machine-mode timer interrupts.
  w_mie(r_mie() | MIE_MTIE);
}
