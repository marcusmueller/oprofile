SUBDIRS = libutil libop libutil++ libopt++ libop++ libdb libabi daemon dae \
 utils pp gui oprof_report doc

all-local: kernel_module
install-exec-local: install.kernel

if WITH_KERNEL_SUPPORT
kernel_module:
install.kernel:
else
kernel_module:
	make -C libutil
	make -C libop
	(cd $(KSRC) && $(MAKE) SUBDIRS=$(OPROFILE_DIR)/module/arch modules) || exit 1 ;
	(cd $(KSRC) && $(MAKE) SUBDIRS=$(OPROFILE_DIR)/module modules) || exit 1 ;

install.kernel: kernel_module
	-mkdir -p $(MODINSTALLDIR)
	cp module/oprofile-module.o $(MODINSTALLDIR)/oprofile.o
	if test $(KVERS) = "`uname -r`"; then depmod -a ; fi
endif

clean-local:
	rm -f module/*.o module/arch/*.o module/.*.o.cmd module/.*.o.flags module/arch/.*.o.cmd module/arch/.*.o.flags

EXTRA_DIST = ChangeLog-2001 autogen.sh include/sstream module/*.c module/*.h module/Makefile.in module/x86/*.h module/x86/*.c module/x86/*.S module/x86/Makefile.in module/ia64/*.h module/ia64/*.c module/ia64/*.S module/ia64/Makefile.in

dist-hook:
	rmdir $(distdir)/module/arch
