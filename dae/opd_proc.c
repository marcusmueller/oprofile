/* $Id: opd_proc.c,v 1.8 2000/08/01 21:10:20 moz Exp $ */

#include "oprofiled.h"

#define OPD_DEFAULT_IMAGES 32
#define OPD_IMAGE_INC 16
/* per-process */ 
#define OPD_DEFAULT_MAPS 16
#define OPD_MAP_INC 8 
 
/* kernel image entries are offset by this much */
#define OPD_KERNEL_OFFSET 1000000

extern unsigned long opd_stats[];
extern fd_t mapdevfd;
extern char *vmlinux; 
extern char *smpdir;
extern u8 ctr0_type_val;
extern u8 ctr1_type_val;
extern int ctr0_um;
extern int ctr1_um;
 
/* LRU list of processes */ 
static struct opd_proc *opd_procs;
 
struct opd_footer footer = { OPD_MAGIC, OPD_VERSION, 0, 0, 0, 0, 0, };
 
/* image structure */
static struct opd_image *opd_images;
static unsigned int nr_images=0;
static unsigned int max_nr_images=OPD_DEFAULT_IMAGES;
 
/* kernel and module support */ 
static u32 kernel_start;
static u32 kernel_end;
static char got_system_map=0;
static struct opd_module opd_modules[OPD_MAX_MODULES]; 
static unsigned int nr_modules=0; 
 
static struct opd_proc *opd_add_proc(u16 pid);
static void opd_grow_images(void);
static void opd_open_image(struct opd_image *image);
static struct opd_image *opd_find_image(const char *name);
static struct opd_image *opd_add_image(const char *name, int kernel);
static struct opd_image *opd_get_image(const char *name, int kernel);
static void opd_init_maps(struct opd_proc *proc);
static void opd_grow_maps(struct opd_proc *proc); 
static void opd_kill_maps(struct opd_proc *proc);
static void opd_put_mapping(struct opd_proc *proc, struct opd_image *image, u32 start, u32 offset, u32 end);
static struct opd_proc *opd_get_proc(u16 pid);
static void opd_delete_proc(struct opd_proc *proc); 
static int opd_is_in_map(struct opd_map *map, u32 eip);
 
/* every so many minutes, clean up old procs, msync mmaps, and
   report stats */ 
void opd_alarm(int val)
{
	struct opd_proc *proc;
	struct opd_proc *next;
	unsigned int i; 

	for (i=0; i < nr_images; i++) {
		if (opd_images[i].fd!=-1)
			msync(opd_images[i].start, opd_images[i].len, MS_ASYNC);
	}
 
	proc = opd_procs;

	while (proc) {
		next=proc->next;
		if (proc->dead)
			opd_delete_proc(proc); 
		proc=next; 
	}
 
	printf("%s stats:\n",opd_get_time());
	printf("Nr. kernel samples with no process context: %lu\n",opd_stats[OPD_KERNEL_NP]);
	printf("Nr. samples lost due to no process information: %lu\n",opd_stats[OPD_LOST_PROCESS]);
	printf("Nr. kernel samples with a process context: %lu\n",opd_stats[OPD_KERNEL_P]);
	printf("Nr. process samples in user-space: %lu\n",opd_stats[OPD_PROCESS]);
	printf("Nr. samples lost due to no map information: %lu\n",opd_stats[OPD_LOST_MAP_PROCESS]);
	printf("Average depth of search of proc queue: %f\n",
		(double)opd_stats[OPD_PROC_QUEUE_DEPTH]/(double)opd_stats[OPD_PROC_QUEUE_ACCESS]);
	printf("Average depth of iteration through mapping array: %f\n",
		(double)opd_stats[OPD_MAP_ARRAY_DEPTH]/(double)opd_stats[OPD_MAP_ARRAY_DEPTH]);
	printf("Nr. sample dumps %lu\n",opd_stats[OPD_DUMP_COUNT]); 
 
	for (i=0;i<OPD_MAX_STATS;i++)
		opd_stats[i]=0;
 
	alarm(60*20);
}
 
/**
 * opd_eip_is_kernel - is the sample from kernel/module space
 * @eip: EIP value
 *
 * Returns %TRUE if @eip is in the address space starting at
 * %KERNEL_VMA_OFFSET, %FALSE otherwise.
 */
inline static int opd_eip_is_kernel(u32 eip)
{
	return (eip >= KERNEL_VMA_OFFSET);
}

/**
 * opd_read_system_map - parse System.map file
 * @filename: file name of System.map
 *
 * Parse the kernel's System.map file. If the filename is
 * passed as "", a warning is produced and the function returns.
 *
 * If the file is parsed correctly, the global variables
 * kernel_start and kernel_end are set to the correct values for the
 * text section of the mainline kernel, and the global got_system_map
 * is set to %TRUE.
 *
 * Note that kernel modules will have EIP values above the value of
 * kernel_end.
 */
void opd_read_system_map(const char *filename)
{
	FILE *fp;
	char *line;
	char *cp; 

	if (streq("",filename)) {
		printf("oprofiled: no System.map specified, cannot resolve kernel modules.\n"); 
		return; 
	}
 
	fp = opd_open_file(filename,"r");

        do {
                line = opd_get_line(fp);
                if (streq(line,"")) {
                        opd_free(line);
                        break;
                } else {
			if (strlen(line)<11) {
				opd_free(line); 
				continue; 
			}
			cp = line+11;
			if (streq("_text",cp))
				sscanf(line,"%x",&kernel_start);
			else if (streq("_end",cp)) 
				sscanf(line,"%x",&kernel_end);
                        opd_free(line);
                }
        } while (1);
 
	if (kernel_start && kernel_end)
		got_system_map=TRUE;

	opd_close_file(fp);
}

/**
 * opd_open_image - open an image sample file
 * @image: image to open file for
 *
 * Open and initialise an image sample file for
 * the image @image and set up memory mappings for
 * it. image->kernel and image->name must have meaningful
 * values.
 */
static void opd_open_image(struct opd_image *image)
{
	char *mangled;
	char *c;
	char *c2;

	mangled = opd_malloc(strlen(smpdir)+2+strlen(image->name));
	strcpy(mangled,smpdir);
	strcat(mangled,"/");
	c = mangled + strlen(smpdir) + 1; 
	c2 = image->name;
	do {
		if (*c2=='/')
			*c++ = '#';
		else
			*c++ = *c2;
	} while (*++c2);
	*c = '\0'; 

	/* for each byte in original, two u32 counters */
	image->len = opd_get_fsize(image->name)*sizeof(u32)*2;
	
	/* give space for "negative" entries. This is because we
	 * don't know about kernel/module sections other than .text so
	 * a sample could be before our nominal start of image */
	if (image->kernel)
		image->len += OPD_KERNEL_OFFSET;
 
	printf("Trying to open %s.\n",mangled);
	image->fd = open(mangled, O_CREAT|O_EXCL|O_RDWR,0644);
	if (image->fd==-1) {
		fprintf(stderr,"oprofiled: open of image sample file \"%s\" failed: %s", mangled,strerror(errno));
		goto out; 
	}

	if (lseek(image->fd, image->len, SEEK_SET)==-1) {
		fprintf(stderr, "oprofiled: seek failed for \"%s\". %s",mangled,strerror(errno));
		goto err; 
	}

	footer.is_kernel = image->kernel;
	 
	if ((write(image->fd, &footer, sizeof(struct opd_footer)))<(signed)sizeof(struct opd_footer)) {
		perror("oprofiled: wrote less than expected opd_footer. ");
		goto err;
	}

	image->start = mmap(0, image->len, PROT_READ|PROT_WRITE, MAP_SHARED, image->fd, 0);

	if (image->start==(void *)-1) {
		perror("oprofiled: mmap() failed. ");
		goto err;
	}
out:
	opd_free(mangled);
	return;
err:
	close(image->fd);
	image->fd=-1;
	goto out; 
}
 
/**
 * opd_get_count - retrieve counter value
 * @count: raw counter value
 *
 * Returns the counter value.
 */
inline static u16 opd_get_count(const u16 count)
{
	return (count & OP_COUNT_MASK);
}

/**
 * opd_get_counter - retrieve counter type
 * @count: raw counter value
 *
 * Returns postive for counter 1, zero for counter 0.
 */ 
inline static u16 opd_get_counter(const u16 count)
{
	return (count & OP_COUNTER);
}
  
struct opd_fentry {
	u32 count0;
	u32 count1;
};
 
/**
 * opd_put_image_sample - write sample to file
 * @image: image for sample
 * @offset: (file) offset to write to
 * @count: raw counter value
 *
 * Add to the count stored at position @offset in the
 * image file. Overflow pins the count at the maximum
 * value.
 *
 * If the image is a kernel or module image, the position
 * is further offset by %OPD_KERNEL_OFFSET.
 *
 * @count is the raw value passed from the kernel.
 */
inline static void opd_put_image_sample(struct opd_image *image, u32 offset, u16 count)
{
	struct opd_fentry *fentry;

	if (image->fd<1) {
		printf("Trying to write to non-opened image %s\n",image->name);
		return; 
	} 
 
	fentry = image->start + offset;

	if (image->kernel)
		fentry += OPD_KERNEL_OFFSET;

	if (opd_get_counter(count)) {
		if (fentry->count1 + count < fentry->count1)
			fentry->count1 = (u32)-1;
		else
			fentry->count1 += count; 
	} else {
		if (fentry->count0 + count < fentry->count0)
			fentry->count0 = (u32)-1;
		else
			fentry->count0 += count;
	}
}

 
/**
 * opd_init_images - initialise image structure
 *
 * Initialise the global image structure, reserving the
 * first entry for the kernel.
 */
void opd_init_images(void)
{
	/* 0 is reserved for the kernel image */
	opd_images = opd_calloc0(sizeof(struct opd_image), OPD_DEFAULT_IMAGES);
	opd_images[0].name = opd_malloc(strlen(vmlinux)+1);
	strncpy(opd_images[0].name,vmlinux,strlen(vmlinux)+1);
	 
	opd_images[0].kernel = 1; 
	opd_open_image(&opd_images[0]);
	nr_images=1;
}

/**
 * opd_grow_images - grow the image structure
 *
 * Grow the global image structure by %OPD_IMAGE_INC entries.
 */ 
static void opd_grow_images(void)
{
	opd_images = opd_realloc(opd_images, sizeof(struct opd_image)*(max_nr_images+OPD_IMAGE_INC));
	max_nr_images += OPD_IMAGE_INC;
}

/**
 * opd_find_image - find an image
 * @name: file name of image to find
 *
 * Returns the image * for the file specified by @name, or 0.
 */
static struct opd_image *opd_find_image(const char *name)
{
	unsigned int i;

	/* FIXME: use hash table */
	for (i=1;i<nr_images;i++) {
		if (streq(opd_images[i].name, name))
			return &opd_images[i];
	}

	return NULL;
}
 
/**
 * opd_add_image - add an image to the image structure
 * @name: name of the image to add
 * @kernel: is the image a kernel/module image 
 *
 * Add an image to the image structure with name @name.
 *
 * @name is copied i.e. should be GC'd separately from the
 * image structure if appropriate.
 */
static struct opd_image *opd_add_image(const char *name, int kernel)
{
	opd_images[nr_images].name = opd_malloc(strlen(name)+1);
	strncpy(opd_images[nr_images].name,name,strlen(name)+1);
 
	opd_images[nr_images].kernel = kernel; 
	opd_open_image(&opd_images[nr_images]);
	nr_images++;

	if (nr_images==max_nr_images)
		opd_grow_images();

	return &opd_images[nr_images-1]; 
}
 
/**
 * opd_get_image - get an image from the image structure
 * @name: name of the image to get
 * @kernel: is the image a kernel/module image 
 *
 * Get the image specified by the file name @name from the
 * image structure. If it is not present, the image is
 * added to the structure. In either case, a pointer to
 * the image structure is returned. 
 */
static struct opd_image *opd_get_image(const char *name, int kernel)
{
	struct opd_image *image; 
	if (!(image=opd_find_image(name)))
		image=opd_add_image(name,kernel);

	return image;
}


/**
 * opd_new_proc - create a new process structure
 * @prev: previous list entry
 * @next: next list entry
 *
 * Allocate and initialise a process structure and insert
 * it into the the list point specified by @prev and @next.
 */
static struct opd_proc * opd_new_proc(struct opd_proc *prev, struct opd_proc *next)
{
	struct opd_proc *proc;

	proc = opd_malloc(sizeof(struct opd_proc));
        proc->maps=NULL;
	proc->pid=0;
	proc->nr_maps=0;
	proc->max_nr_maps=0;
	proc->dead=0;
	proc->prev=prev;
	proc->next=next;
	return proc; 
}
 
/**
 * opd_delete_proc - delete a process
 * @proc: process to delete
 *
 * Remove the process @proc from the process list and free
 * the associated structures.
 */
static void opd_delete_proc(struct opd_proc *proc)
{
	printf("Deleting pid %u\n",proc->pid);

	if (!proc->prev)
		opd_procs=proc->next;
	else
		proc->prev->next=proc->next;

	if (proc->next)
		proc->next->prev=proc->prev;
	
	opd_free(proc->maps);
	opd_free(proc);
}

/**
 * opd_init_maps - initialise map structure for a process
 * @proc: process to work on
 *
 * Initialise the mapping info structure for process @proc.
 * The zeroth map is set to values for the kernel.
 */
static void opd_init_maps(struct opd_proc *proc)
{
	/* first map is the kernel */ 
	proc->maps = opd_calloc0(sizeof(struct opd_map), OPD_DEFAULT_MAPS);
	proc->maps[0].start = KERNEL_VMA_OFFSET; 
	proc->maps[0].offset = 0;
	proc->maps[0].end = 0xffffffff;
	proc->maps[0].image = opd_get_image(vmlinux,1);
	proc->max_nr_maps = OPD_DEFAULT_MAPS; 
	proc->nr_maps = 1;
}

/**
 * opd_add_proc - add a process
 * @pid: process id
 *
 * Create a new process structure and add it
 * to the process list. The process structure
 * is filled in as appropriate.
 *
 */
static struct opd_proc *opd_add_proc(u16 pid)
{
	struct opd_proc *proc;

	proc=opd_new_proc(NULL,opd_procs); 
	if (opd_procs)
		opd_procs->prev=proc;
 
	opd_procs=proc;

	opd_init_maps(proc);
	proc->pid=pid;

	return proc;
}

/**
 * opd_grow_maps - grow map structure for a process
 * @proc: process to work on
 *
 * Grow the map structure for a process by %OPD_MAP_INC
 * entries.
 */
static void opd_grow_maps(struct opd_proc *proc)
{
	proc->maps = opd_realloc(proc->maps, sizeof(struct opd_map)*(proc->max_nr_maps+OPD_MAP_INC));
	proc->max_nr_maps += OPD_MAP_INC;
}

/**
 * opd_kill_maps - delete mapping information for a process
 * @proc: process to work on
 *
 * Frees structures holding mapping information and resets
 * the values.
 */
static void opd_kill_maps(struct opd_proc *proc)
{
	if (proc->maps) 
		opd_free(proc->maps);
	proc->maps=NULL; 
	proc->nr_maps=0; 
	proc->max_nr_maps=0; 
	opd_init_maps(proc);
}
 
/**
 * opd_do_proc_lru - rework process list
 * @proc: process to move
 *
 * Perform LRU on the process list by resetting
 * the process's age and moving it to the head
 * of the process list.
 */
static void opd_do_proc_lru(struct opd_proc *proc)
{
	if (proc->prev) {
		proc->prev->next = proc->next;
		if (proc->next)
			proc->next->prev = proc->prev; 
		opd_procs->prev = proc;
		proc->prev=NULL;
		proc->next=opd_procs;
		opd_procs=proc; 
	}
}
 
/**
 * opd_get_proc - get process from process list
 * @pid: pid to search for
 *
 * A process with pid @pid is searched on the process list,
 * maintaining LRU. If it is not found, %NULL is returned.
 *
 * The process structure is returned.
 */
static struct opd_proc *opd_get_proc(u16 pid)
{
	struct opd_proc *proc;
 
	proc = opd_procs;

	opd_stats[OPD_PROC_QUEUE_ACCESS]++; 
	while (proc) { 
		opd_stats[OPD_PROC_QUEUE_DEPTH]++; 
		if (pid==proc->pid) {
			opd_do_proc_lru(proc); 
			return proc;
		}
		proc=proc->next;
	}

	return NULL;
}
 
/**
 * opd_is_in_map - check whether an EIP is within a mapping
 * @map: map to check
 * @eip: EIP value
 *
 * Return %TRUE if the EIP value @eip is within the boundaries
 * of the map @map, %FALSE otherwise.
 */
static int opd_is_in_map(struct opd_map *map, u32 eip)
{
	return (eip >= map->start && eip < map->end);
}

/**
 * opd_clear_module_info - clear kernel module information
 *
 * Clear and free all kernel module information and reset
 * values.
 */
void opd_clear_module_info(void)
{
	int i;

	for (i=0; i < OPD_MAX_MODULES; i++) {
		if (opd_modules[i].name)
			opd_free(opd_modules[i].name);
		opd_modules[i].start=0;
		opd_modules[i].end=0;
	}
} 
 
/**
 * opd_get_module - get module structure
 * @name: name of module image
 *
 * Find the module structure for module image @name.
 * If it could not be found, add the module to
 * the global module structure.
 *
 * If an existing module is found, @name is free()d.
 * Otherwise it must be freed when the module structure
 * is removed (i.e. in opd_clear_module_info()).
 */
static struct opd_module *opd_get_module(char *name)
{
	int i;

	for (i=0; i < OPD_MAX_MODULES; i++) {
		if (opd_modules[i].image && streq(name,opd_modules[i].image->name)) {
			/* free this copy */ 
			opd_free(name);
			return &opd_modules[i];
		} 
	}

	opd_modules[nr_modules].image = NULL;
	opd_modules[nr_modules].start = 0;
	opd_modules[nr_modules].end = 0;
	nr_modules++;
	if (nr_modules==OPD_MAX_MODULES) {
		fprintf(stderr, "Exceeded %u kernel modules !\n",OPD_MAX_MODULES); 
		exit(1);
	}
 
	return &opd_modules[nr_modules-1];
}
 
/**
 * opd_get_module_info - parse mapping information for kernel modules
 *
 * Parse the file /proc/ksyms to read in mapping information for
 * all kernel modules. The modutils package adds special symbols
 * to this file which allows determination of the module image
 * and mapping addresses of the form :
 *
 * __insmod_modulename_Oobjectfile_Mmtime_Vversion
 * __insmod_modulename_Ssectionname_Llength
 *
 * Currently the image file "objectfile" is stored, and details of
 * ".text" sections.
 */
static void opd_get_module_info(void)
{
	char *line;
	char *cp, *cp2, *cp3; 
	FILE *fp;
	struct opd_module *mod; 
	char *modname;
	char *filename; 

	nr_modules=0;
 
	fp = opd_try_open_file("/proc/ksyms","r");

	if (!fp) {	
		printf("oprofiled: /proc/ksyms not readable, can't process module samples.\n");
		return;
	}

	do {
		line = opd_get_line(fp);
		/* FIXME: bug present in 2.4.0-test3 - verify */ 
		if (streq("",line) && !feof(fp)) {
			opd_free(line);
			continue;
		}

		if (strlen(line)<9) {
			printf("oprofiled: corrupt /proc/ksyms line \"%s\"\n",line);
			goto failure;
		}

		if (strncmp("__insmod_",line+9,9)) {
			opd_free(line); 
			continue;
		} 
 
		cp = line + 18;
		cp2 = cp;
		while ((*cp2) && !streqn("_S",cp2+1,2) && !streqn("_O",cp2+1,2))
			cp2++;

		if (!*cp2) {
			printf("oprofiled: corrupt /proc/ksyms line \"%s\"\n",line);
			goto failure; 
		}
	
		cp2++;
		/* freed by opd_clear_module_info() or opd_get_module() */
		modname = opd_malloc((size_t)(cp2-cp+1));
		strncpy(modname,cp,(size_t)(cp2-cp));
		modname[cp2-cp]='\0';

		mod = opd_get_module(modname);
 
		switch (*(++cp2)) {
			case 'O':
				/* get filename */
				cp2++;
				cp3 = cp2;

				while ((*cp3) && !streqn("_M",cp3+1,2))
					cp3++;

				if (!*cp3) {
					opd_free(line);
					continue;
				} 
				
				cp3++;
				filename = opd_malloc((size_t)(cp3-cp2+1));
				strncpy(filename,cp2,(size_t)(cp3-cp2));

				mod->image = opd_get_image(filename,1);
				opd_free(filename);
				break;

			case 'S':
				/* get extent of .text section */
				cp2++;
				if (strncmp(".text_L",cp2,7)) {
					opd_free(line); 
					continue;
				} 

				cp2+=7;
				sscanf(line,"%x",&mod->start);
				sscanf(cp2,"%u",&mod->end);
				mod->end += mod->start;
				break;
		}
 
		opd_free(line);
	} while (1);
 
failure:
	opd_free(line);
	opd_close_file(fp);
}
 
/**
 * opd_handle_module_sample - process a module sample
 * @eip: EIP value
 * @count: count value of sample 
 *
 * Process a sample in module address space. The sample @eip
 * is matched against module information. If the search was
 * successful, the sample is output to the relevant file.
 *
 * Note that for modules and the kernel, the offset will be
 * wrong in the file, as it is not a file offset, but the offset
 * from the text section. This is fixed up in pp.
 *
 * If the sample could not be located in a module, it is treated
 * as a kernel sample.
 */
static void opd_handle_module_sample(u32 eip, u16 count)
{
	unsigned int i;
	unsigned int pass=0;

retry:
	for (i=0; i < nr_modules; i++) {
		if (opd_modules[i].start && opd_modules[i].end &&
	            opd_modules[i].start <= eip &&
	            opd_modules[i].end > eip) {
			opd_put_image_sample(opd_modules[i].image, eip - opd_modules[i].start, count);
			return;
		}
	}

	/* not locatable, log as kernel sample */
	if (pass) {
		opd_put_image_sample(&opd_images[0], eip - kernel_start, count); 
		return;
	}

	pass++;
 
	/* not found in known modules, re-read our info */
	opd_clear_module_info();
	opd_get_module_info();
	goto retry;
}
 
/**
 * opd_handle_kernel_sample - process a kernel sample
 * @proc: process for which the sample is, or %NULL for kernel thread
 * @eip: EIP value of sample
 * @count: count value of sample 
 *
 * Handle a sample in kernel address space or in a module. The sample is
 * output to the relevant image file.
 *
 * This function requires the global variable
 * got_system_map to be %TRUE to handle module samples.
 */
static void opd_handle_kernel_sample(struct opd_proc *proc, u32 eip, u16 count)
{
	(proc) ? (opd_stats[OPD_KERNEL_P]++) : (opd_stats[OPD_KERNEL_NP]++);

	if (got_system_map) {
		if (eip < kernel_end) {
			opd_put_image_sample(&opd_images[0], eip - kernel_start, count); 
			return;
		}

		/* in a module */

		opd_handle_module_sample(eip,count);
		return;
	}

	opd_put_image_sample(&opd_images[0], eip - KERNEL_VMA_OFFSET, count);
	return;
}
 
/**
 * opd_map_offset - return offset of sample against map
 * @map: map to use
 * @eip: EIP value to use
 *
 * Returns the offset of the EIP value @eip into
 * the map @map, which is the same as the file offset
 * for the relevant binary image. 
 */
inline static u32 opd_map_offset(struct opd_map *map, u32 eip)
{
	return map->offset + eip - map->start;
}
 
/**
 * opd_put_sample - process a sample
 * @sample: sample to process
 *
 * Write out the sample to the appropriate sample file. This 
 * routine handles kernel and module samples as well as ordinary ones.
 */ 
void opd_put_sample(const struct op_sample *sample)
{
	unsigned int i; 
	struct opd_proc *proc; 

	if (!sample->pid) {
		opd_handle_kernel_sample(NULL,sample->eip,sample->count);
		return;
	}
		 
	if (!(proc=opd_get_proc(sample->pid))) {
		/* no mapping info ? probably kswapd or similar */
		if (opd_eip_is_kernel(sample->eip)) {
			opd_handle_kernel_sample(NULL,sample->eip,sample->count);
			return;
		}
 
		opd_stats[OPD_LOST_PROCESS]++;
		return;
	}

	/* look for which map and find offset */ 
	/* FIXME: binary search ? */
	opd_stats[OPD_MAP_ARRAY_ACCESS]++; 
	for (i=0;i<proc->nr_maps;i++) {
		if (opd_is_in_map(&proc->maps[i],sample->eip)) {
			if (opd_eip_is_kernel(sample->eip))
				opd_handle_kernel_sample(proc,sample->eip,sample->count);
			else { 
				u32 offset = opd_map_offset(&proc->maps[i],sample->eip); 
				opd_put_image_sample(proc->maps[i].image,offset,sample->count);
				opd_stats[OPD_PROCESS]++;
			}
			return;
		}
		opd_stats[OPD_MAP_ARRAY_DEPTH]++; 
	}

	/* couldn't locate it */ 
	opd_stats[OPD_LOST_MAP_PROCESS]++;
	return;
}

/**
 * opd_put_mapping - add a mapping to a process
 * @proc: process to add map to
 * @image: mapped image
 * @start: start of mapping
 * @offset: file offset of mapping
 * @end: end of mapping
 *
 * Add the mapping specified to the process @proc.
 */
void opd_put_mapping(struct opd_proc *proc, struct opd_image *image, u32 start, u32 offset, u32 end)
{
	/* note we can have duplicate maps due to binary handler kludge. so what ? */
	proc->maps[proc->nr_maps].image = image;
	proc->maps[proc->nr_maps].start = start; 
	proc->maps[proc->nr_maps].offset = offset; 
	proc->maps[proc->nr_maps].end = end; 
	
	if (++proc->nr_maps==proc->max_nr_maps)
		opd_grow_maps(proc); 
}
 
/**
 * opd_handle_fork - deal with fork notification
 * @sample: sample structure from kernel
 *
 * Deal with a fork() notification by creating a new process
 * structure, and copying mapping information from the old process.
 *
 * sample->pid contains the process id of the old process.
 * sample->eip contains the process id of the new process.
 */
void opd_handle_fork(const struct op_sample *sample)
{
	struct opd_proc *old;
	struct opd_proc *proc;

	old = opd_get_proc(sample->pid);

	if (!old) {
		printf("Told that non-existent process %u just forked.\n",sample->pid);
		return;
	}
 
	/* eip is actually pid of new process */ 
	proc = opd_add_proc((u16)sample->eip);

	/* remove the kernel map and copy over */
 
	opd_free(proc->maps);
	proc->maps = opd_malloc(sizeof(struct opd_map)*old->max_nr_maps);
	memcpy(proc->maps,old->maps,sizeof(struct opd_map)*old->nr_maps);
	proc->nr_maps = old->nr_maps;
	proc->max_nr_maps = old->max_nr_maps;
}
 
/**
 * opd_handle_exit - deal with exit notification
 * @sample: sample structure from kernel
 *
 * Deal with an exit() notification by setting the flag "dead"
 * on a process. These will be later cleaned up by %SIGALRM 
 * handler.
 *
 * sample->pid contains the process id of the exited process.
 */
void opd_handle_exit(const struct op_sample *sample)
{
	struct opd_proc *proc;

	proc = opd_get_proc(sample->pid);
	if (proc)
		proc->dead = 1;
	else
		printf("unknown proc %u just exited.\n",sample->pid); 
}
 
struct op_mapping {
	u32 addr;
	u32 len;
	u32 offset;
	char path[0];
} __attribute__((__packed__,__aligned__(16)));
 
/**
 * opd_handle_mapping - deal with mapping notification
 * @sample: sample structure from kernel
 *
 * Deal with a notification that a process has mapped in
 * a new executable file. The mapping information is read
 * from the mapping device and added to the process structure.
 *
 * sample->pid contains the process id of the process.
 * sample->eip contains the number of bytes to read from
 * the mapping device.
 */
void opd_handle_mapping(const struct op_sample *sample)
{
	struct opd_proc *proc;
	struct op_mapping *mapping;
	ssize_t size; 

	proc = opd_get_proc(sample->pid);

	if (!proc) {
		printf("Told about mapping for non-existent process %u.\n",sample->pid);
		return;
	}

	/* eip is actually nr. of bytes to read from map device */
	size = (ssize_t)sample->eip; 

	mapping = opd_malloc(size+1);

	opd_read_device(mapdevfd,mapping,size,0);
	/* string might need terminating */
	*(((char *)&mapping)+size) = '\0';
	printf("Mapping from 0x%x, size 0x%x, offset 0x%x, of file $%s$\n",mapping->addr, mapping->len, mapping->offset,
		mapping->path);

	opd_put_mapping(proc,opd_get_image(mapping->path,0),mapping->addr, mapping->offset, mapping->addr+mapping->len);
	opd_free(mapping); 
}
 
static void opd_get_ascii_maps(struct opd_proc *proc);
 
/**
 * opd_handle_drop_mappings - deal with notification of dropped mappings
 * @sample: sample structure from kernel
 *
 * Drop all mapping information for the process.
 *
 * sample->pid contains the process id of the process.
 */
void opd_handle_drop_mappings(const struct op_sample *sample)
{
	struct opd_proc *proc;

	proc = opd_get_proc(sample->pid);
	if (proc)
		opd_kill_maps(proc);
	else
		printf("Told to drop mappings for a non-existent process %u.\n",sample->pid);
}

/**
 * opd_add_ascii_map - parse an ASCII map string for a process
 * @proc: process to add map to
 * @line: 0-terminated ASCII string
 *
 * Attempt to parse the string @line for map information
 * and add the info to the process @proc. Returns %TRUE
 * on success, %FALSE otherwise.
 *
 * The parsing is based on Linux 2.4 format, which looks like this :
 * 
 * 4001e000-400fc000 r-xp 00000000 03:04 31011      /lib/libc-2.1.2.so
 */
static int opd_add_ascii_map(struct opd_proc *proc, const char *line)
{
	struct opd_map *map = &proc->maps[proc->nr_maps]; 
	const char *cp=line;

	/* skip to protection field */ 
	while (*cp && *cp!=' ')
		cp++;

	/* handle rwx */
	if (!*cp || (!*(++cp)) || (!*(++cp)) || (*(++cp)!='x'))
		return FALSE;

	/* get start and end from "40000000-4001f000" */ 
	if (sscanf(line,"%x-%x", &map->start, &map->end)!=2)
		return FALSE;

	/* "p " */ 
	cp+=2;

	/* read offset */ 
	if (sscanf(cp,"%x", &map->offset)!=1)
		return FALSE;
 
	while (*cp && *cp!='/')
		cp++;

	if (!*cp)
		return FALSE; 

	map->image = opd_get_image(cp,0); 

	if (!map->image)
		return FALSE; 
 
	if (++proc->nr_maps==proc->max_nr_maps)
		opd_grow_maps(proc);

	return TRUE;
}

/**
 * opd_get_ascii_maps - read all maps for a process
 * @proc: process to work on
 *
 * Read the /proc/<pid>/maps file and add all
 * mapping information found to the process @proc.
 */
static void opd_get_ascii_maps(struct opd_proc *proc)
{
	FILE *fp;
	char mapsfile[20]="/proc/";
	char *line; 

	opd_init_maps(proc);

	snprintf(mapsfile+6, 6, "%hu", proc->pid);
	strcat(mapsfile,"/maps");
 
	fp = opd_try_open_file(mapsfile,"r");
	if (!fp)
		return;
 
	do {
		line = opd_get_line(fp);
		/* FIXME: ? currently eof check is necessary due to 2.4.0-test3 */
		if (streq(line,"") && feof(fp)) {
			opd_free(line);
			break;
		} else {
			opd_add_ascii_map(proc,line);
			opd_free(line);
		}
	} while (1);
}

/**
 * opd_get_ascii_procs - read process and mapping information from /proc
 *
 * Read information on each process and its mappings from the /proc
 * filesystem.
 */
void opd_get_ascii_procs(void)
{
	DIR *dir;
	struct dirent *dirent;
	struct opd_proc *proc;
	u16 pid;
 
	if (!(dir = opendir("/proc"))) {
		perror("oprofiled: /proc directory could not be opened. ");
		exit(1);
	}

	while ((dirent = readdir(dir))) {
		if (sscanf(dirent->d_name, "%hu", &pid)==1) {
			proc = opd_add_proc(pid);
			opd_get_ascii_maps(proc);
		}
	}

	closedir(dir);
}
