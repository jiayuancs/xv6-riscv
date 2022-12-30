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

        # la(load address)是伪指令，用于将符号的地址加载到寄存器中
        # la指令在非位置无关码（non-PIC）和位置无关码（PIC，position independance code）中，具有不同的解释方式
        # 此处对应的是non-PIC，所以 la sp, stack0 将被转换为如下2条指令：
        #       auipc sp, delta[31:12]+delta[11]
        #       addi sp, sp, delta[11:0]
        # 其中，delta = stack0 - pc，即stack0相对pc的地址
        # auipc(add upper immediate to pc)指令将20位立即数与12位0拼接，构成32位立即数，然后将立即数与pc相加，将结果保存到目标寄存器中
        la sp, stack0
        # li用于加载立即数(Load immediate)
        li a0, 1024*4   # 每个hart使用4KiB的栈

        # # csrr即读csr寄存器(控制状态寄存器)
        # mhartid寄存器存放的是执行当前代码的hart的ID
        # 计算当前hart的栈顶地址
        csrr a1, mhartid
        addi a1, a1, 1
        mul a0, a0, a1
        add sp, sp, a0  
        
        # jump to start() in start.c
        call start
spin:
        j spin
