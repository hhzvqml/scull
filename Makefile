# disable/enable debugging
#DEBUG = y
 
# 当DEBUG变量等于y时。两个比较变量用括号括起来，逗号分隔。ifeq和括号中间有一个空格。
ifeq ($(DEBUG), y)  
# += 追加变量值。如果该变量之前没有被定义过，+=就自动变成=，变量被定义成递归展开式的变量；如果之前已经定义过，就遵循之前的风格。
# = 递归展开式变量：在定义时，变量中对其它变量的引用不会被替换展开。变量在引用它的地方被替换展开时，变量中其它变量才被同时替换展开。
# -O 程序优化参数；
# -g 使生成的debuginfo包额外支持gnu和gdb调试程序，易于使用
# -DSCULL_DEBUG 即define SCULL_DEBUG   TODO
    DEBFLAGS += -O -g -DSCULL_DEBUG # -O is needed to expand inlines
else
    DEBFLAGS += -O2
endif
 
#  CFLAGS影响编译过程。应在遵循原设置的基础上添加新的设置，注意+=
CFLAGS += $(DEBFLAGS)
# -I 选项指出头文件位置。LDDINC是下面定义的一个变量。
CFLAGS += -I$(LDDINC)
 
# 如果KERNELRELEASE不等于空。ifneq判断参数是否不相等。
ifneq ($(KERNELRELEASE),)
# := 直接展开式变量：在定义时就展开变量中对其它变量或函数的引用，定以后就成了需要定义的文本串。不能实现对其后定义变量的引用。
scull-objs := main.o pipe.o access.o
obj-m := scull.o
# 否则（KERNELRELEASE是空）
else
# ?= 条件赋值：只有在此变量之前没有赋值的情况下才会被赋值。
# shell uname r 内核版本号
KERNELDIR ?= /lib/modules/$(shell uname -r)/build
# shell pwd 当前在文件系统中的路径
PWD := $(shell pwd)
 
modules:
    # $(MAKE) TODO
    # -C $(KERNELDIR) 在读取Makefile之前进入$(KERNELDIR)目录
    # M=$(PWD) LDDINC=$(PWD)/../include 传递2个变量给Makefile
    # modules 是$(KERNELDIR)中的Makefile的target   TODO
	$(MAKE) -C $(KERNELDIR) M=$(PWD) LDDINC=$(PWD)/../include modules
 
endif
 
# 清理
clean:
	rm -rf *.o *~ core .depend .*.cmd *.ko *.mod.c .tmp_versions
 
# TODO
# 产生依赖信息文件，如果存在的话则将其包含到Makefile中。
depend .depend dep:
	$(CC) $(CFLAGS) -M *.c > .depend
    
ifeq (.depend,$(wildcard .depend))
include .depend
endif
