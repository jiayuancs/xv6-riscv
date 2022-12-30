#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "defs.h"

void main();
void timerinit();

// entry.S needs one stack per CPU.
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
  // 将mstatus寄存器中的MPP字段置为1，即Supervisor模式
  // 调用mret时，硬件会自动将当前特权级恢复为MPP字段代表的特权级
  unsigned long x = r_mstatus();
  x &= ~MSTATUS_MPP_MASK;
  x |= MSTATUS_MPP_S;
  w_mstatus(x);

  // set M Exception Program Counter to main, for mret.
  // requires gcc -mcmodel=medany
  // 将main函数入口地址写入mepc寄存器
  // 调用mret时，硬件自动将mepc中的值写入PC
  w_mepc((uint64)main);

  // disable paging for now.
  // satp(Supervisor Address Translation and Protection Register)
  w_satp(0);

  // delegate all interrupts and exceptions to supervisor mode.
  // RISC-V的异常委托机制
  // 默认情况下，任何特权级别上的所有trap都是在机器模式下处理的
  // 为了提高性能，将所有异常和中断都交由supervisor模式处理
  w_medeleg(0xffff);
  w_mideleg(0xffff);
  // supervisor模式的中断使能
  w_sie(r_sie() | SIE_SEIE | SIE_STIE | SIE_SSIE);

  // configure Physical Memory Protection to give supervisor mode
  // access to all of physical memory.
  // xv6基于Sv39 RISC-V
  // Sv39只使用虚拟地址的低39位，即应用程序的虚拟地址空间最大为512GiB
  // Sv39的物理地址为56位，即物理内存最大为2^56B(0x3fffffffffffffull)
  // 访问限制：如果访问的地址大于等于pmpaddr[i]，但小于pmpaddr[i+1]，
  // 则pmpaddr[i+1]的配置寄存器pmpcfg[i+1]将决定该访问是否可以继续，如果不能将会引发访问异常。
  // 这里将pmpaddr0置为最大物理内存容量0x3fffffffffffffull，
  // 于是所有的访问都会使用配置寄存器pmpcfg0，该寄存器低4位置1，表示允许读、写、执行
  w_pmpaddr0(0x3fffffffffffffull);
  w_pmpcfg0(0xf);

  // ask for clock interrupts.
  // RISC-V要求定时器中断只能在机器模式下进行。
  timerinit();

  // keep each CPU's hartid in its tp register, for cpuid().
  // tp(Thread pointer) 线程指针寄存器
  int id = r_mhartid();
  w_tp(id);

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
  // 设置mtimecmp和mtime寄存器，每过一个cycle, mtime都会加1
  // 当mtime大于等于mtimecmp时产生一个timer中断
  int interval = 1000000; // cycles; about 1/10th second in qemu.
  *(uint64*)CLINT_MTIMECMP(id) = *(uint64*)CLINT_MTIME + interval;

  // prepare information in scratch[] for timervec.
  // scratch[0..2] : space for timervec to save registers.
  // scratch[3] : address of CLINT MTIMECMP register.
  // scratch[4] : desired interval (in cycles) between timer interrupts.
  uint64 *scratch = &timer_scratch[id][0];
  scratch[3] = CLINT_MTIMECMP(id); // mtimecmp寄存器的地址
  scratch[4] = interval;
  w_mscratch((uint64)scratch);

  // set the machine-mode trap handler.
  // 设置机器模式的中断向量，中断发生时，跳转到timevec执行(mtvec.MODE字段为0, 查询中断)
  w_mtvec((uint64)timervec);

  // enable machine-mode interrupts.
  w_mstatus(r_mstatus() | MSTATUS_MIE);

  // enable machine-mode timer interrupts.
  w_mie(r_mie() | MIE_MTIE);
}
