/* opd_proc.c */
/* John Levon (moz@compsoc.man.ac.uk) */
/* May 2000 */

#include "oprofiled.h"

#define OPD_DEFAULT_IMAGES 32
#define OPD_IMAGE_INC 16
/* per-process */ 
#define OPD_DEFAULT_MAPS 16
#define OPD_MAP_INC 8 
 
extern FILE *imgfp;
extern unsigned long opd_stats[];  
 
/* LRU list of processes */ 
struct opd_proc *opd_procs; 
 
/* image filenames */ 
char **opd_images;
static int nr_images=0;
static int max_nr_images=OPD_DEFAULT_IMAGES; 
 
/* kernel and module support */ 
static u32 kernel_start;
static u32 kernel_end;
static char got_system_map=0;
static struct opd_module opd_modules[OPD_MAX_MODULES]; 
static int nr_modules=0; 
 
static struct opd_proc *opd_add_proc(u16 pid, u32 eip);
static char *opd_proc_exe(u16 pid); 
static void opd_get_maps(struct opd_proc *proc);
static int opd_add_map(struct opd_proc *proc, const char *line);
static void opd_grow_images(void);
static int opd_find_image(const char *name);
static void opd_output_image(const char *name);
static void opd_add_image(const char *name, u16 *num);
static void opd_get_image(const char *name, u16 *num);
static void opd_init_maps(struct opd_proc *proc);
static void opd_grow_maps(struct opd_proc *proc); 
static void opd_kill_maps(struct opd_proc *proc);
static struct opd_proc *opd_get_proc(u16 pid, u32 eip);
static void opd_delete_proc(struct opd_proc *proc); 
static int opd_is_in_map(struct opd_map *map, u32 eip);
 
/* every so many minutes, clean up old procs and
   report stats */ 
void opd_alarm(int val)
{
	struct opd_proc *proc;
	struct opd_proc *next;
	time_t now = time(NULL); 
	int i; 

	proc = opd_procs;

	while (proc) {
		next=proc->next;
		if (now - proc->age > OPD_REAP_TIME)
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
		(double)opd_stats[OPD_PROC_QUEUE_ACCESS]/(double)opd_stats[OPD_PROC_QUEUE_DEPTH]);
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
 * opd_init_images - initialise image structure
 *
 * Initialise the global image structure. 
 */ 
void opd_init_images(void)
{
	/* 0 is reserved for the no image file */ 
	opd_images = opd_calloc0(sizeof(char *), OPD_DEFAULT_IMAGES);
	opd_images[0]=NULL;
	nr_images=1;
}

/**
 * opd_grow_images - grow the image structure
 *
 * Grow the global image structure by %OPD_IMAGE_INC entries.
 */ 
static void opd_grow_images(void)
{
	opd_images = opd_realloc(opd_images, sizeof(char *)*(max_nr_images+OPD_IMAGE_INC));
	max_nr_images += OPD_IMAGE_INC;
}

/**
 * opd_find_image - find an image
 * @name: file name of image to find
 *
 * Returns the index into the global image structure
 * for the file specified by @name, or 0.
 */
static int opd_find_image(const char *name)
{
	int i;

	for (i=1;i<nr_images;i++) {
		if (streq(opd_images[i], name))
			return i;
	}

	return 0;
}
 
/**
 * opd_output_image - output image name
 * @name: file name of image
 *
 * Write the specified image name @name to the
 * images file, including the terminating '\0'.
 */
static void opd_output_image(const char *name)
{
	opd_write_file(imgfp, name, strlen(name)+1); 
	fflush(imgfp);
}
 
/**
 * opd_add_image - add an image to the image structure
 * @name: name of the image to add
 * @num: where to store the image number
 *
 * Copy the string @name into the global image structure,
 * and enter the index into the structure into the variable
 * pointed to by @num. @num must be a valid pointer.
 *
 * @name is copied i.e. should be GC'd separately from the
 * image structure if appropriate.
 */
static void opd_add_image(const char *name, u16 *num)
{
	opd_images[nr_images] = opd_malloc(strlen(name)+1);
	strncpy(opd_images[nr_images],name,strlen(name)+1);
	*num=nr_images;
 
	opd_output_image(name);
	nr_images++;

	if (nr_images==max_nr_images)
		opd_grow_images();
}
 
/**
 * opd_get_image - get an image from the image structure
 * @name: name of the image to get
 * @num: where to store the image number
 *
 * Get the image specified by the file name @name from the
 * image structure. If it is not present, the image is
 * added to the structure. In either case, the integer pointed 
 * to by @num is filled with the index into the structure
 * for the image.
 */
static void opd_get_image(const char *name, u16 *num)
{
	if (!(*num=opd_find_image(name)))
		opd_add_image(name,num);
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
	proc->exe_nr=0;
	proc->nr_maps=0;
	proc->max_nr_maps=0;
	proc->prev=prev; 
	proc->age=time(NULL); 
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
	printf("Deleting pid %u of age %lu\n",proc->pid,proc->age);

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
 * opd_add_proc - add a process
 * @pid: process id
 * @eip: EIP value of current sample 
 *
 * Create a new process structure and add it
 * to the process list. The member exe_nr
 * gets filled with the number of the
 * process image, or 0 on error. Additionally
 * available mapping information is read for the process.
 *
 * The value of @eip is used to determine whether
 * this is a kernel image (vmlinux or a module image).
 *
 * In this case the string name will have its first
 * character replaced by a 'k'. This should always be
 * '/', so it can be mapped back.
 */
static struct opd_proc *opd_add_proc(u16 pid, u32 eip)
{
	struct opd_proc *proc;
	char *exe;

	proc=opd_new_proc(NULL,opd_procs); 
	if (opd_procs)
		opd_procs->prev=proc;
 
	opd_procs=proc;

	proc->pid=pid;
	exe = opd_proc_exe(pid);
 
	if (exe) {
		if (opd_eip_is_kernel(eip))
			exe[0]='k'; 
		opd_get_image(exe,&proc->exe_nr);
	} else
		proc->exe_nr=0;

	opd_get_maps(proc);

	return proc;
}

/**
 * opd_proc_exe - read exe link for a process
 * @pid: process id
 *
 * Read the symbolic link /proc/<pid>/exe to
 * determine the process image.
 *
 * Returns %NULL on error.
 */
static char *opd_proc_exe(u16 pid)
{
	char *exe;
	char exelink[20]="/proc/";

	snprintf(exelink+6, 6, "%hu", pid);
	strcat(exelink,"/exe");
 
	exe = opd_read_link(exelink);
	
	return exe;
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
	proc->maps[0].image_nr = 0; 
	proc->max_nr_maps = OPD_DEFAULT_MAPS; 
	proc->nr_maps = 1;
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
 * opd_add_map - parse an ASCII map string for a process
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
static int opd_add_map(struct opd_proc *proc, const char *line)
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

	opd_get_image(cp,&map->image_nr); 

	if (!map->image_nr)
		return FALSE; 
 
	proc->nr_maps++;
	if (proc->nr_maps==proc->max_nr_maps)
		opd_grow_maps(proc);

	return TRUE;
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
	/* signal handler may have wiped our maps */
	if (proc->maps) 
		opd_free(proc->maps);
	proc->nr_maps=0; 
	proc->max_nr_maps=0; 
	opd_init_maps(proc);
} 
 
/**
 * opd_get_maps - read all maps for a process
 * @proc: process to work on
 *
 * Read the /proc/<pid>/maps file and add all
 * mapping information found to the process @proc.
 */
static void opd_get_maps(struct opd_proc *proc)
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
		/*  currently eof check is necessary due to 2.4.0-test3 */
		if (streq(line,"") && feof(fp)) {
			opd_free(line);
			break;
		} else {
			opd_add_map(proc,line);
			opd_free(line);
		}
	} while (1);			
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
	proc->age=time(NULL);
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
 * @eip: EIP value of current sample 
 *
 * A process with pid @pid is searched on the process list,
 * maintaining LRU. If it is not found, the process is 
 * added to the process list.
 *
 * The EIP value @eip is needed for the opd_add_proc()
 * hack.
 *
 * The process structure is returned.
 */
static struct opd_proc *opd_get_proc(u16 pid, u32 eip)
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

	return opd_add_proc(pid,eip);
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
static void opd_clear_module_info(void)
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
		if (opd_modules[i].name && streq(name,opd_modules[i].name)) {
			/* free this copy */ 
			opd_free(name); 
			return &opd_modules[i]; 
		} 
	}

	opd_modules[nr_modules].name = name;
	opd_modules[nr_modules].start = 0;
	opd_modules[nr_modules].end = 0;
	opd_modules[nr_modules].image_nr=0;
	nr_modules++;
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
 * ".text" sections. Really we should store all mappings as we're
 * not positive that only .text is used. FIXME
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

				/* add to list and output to file */
				opd_add_image(filename,&mod->image_nr);
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
 * @pid: process id or 0 for kernel thread
 * @eip: EIP value
 * @offset: pointer to offset return
 * @image_nr: pointer to image nr. return
 *
 * Process a sample in module address space. The sample @eip
 * is matched against module information. If the search was
 * successful, the values pointed to by @offset and @image_nr
 * are set appropriately. Note that @offset will be the offset
 * of the address from the start of the text section. 
 *
 * If the sample could not be located in a module, it is treated
 * as a kernel sample (offset from kernel_start, image nr. of 0)
 */
static void opd_handle_module_sample(u16 pid, u32 eip, u32 *offset, u16 *image_nr)
{
	int i;
	int pass=0;

retry:
	for (i=0; i < nr_modules; i++) {
		if (opd_modules[i].start && opd_modules[i].end &&
	            opd_modules[i].start <= eip &&
	            opd_modules[i].end > eip) {
			*offset = eip - opd_modules[i].start;
			*image_nr = opd_modules[i].image_nr;
			return;
		}
	}

	/* not locatable, log as kernel sample */
	if (pass) {
		*image_nr = 0;
		*offset = eip - kernel_start;
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
 * @pid: pid of process
 * @eip: EIP value of sample
 * @offset: pointer to offset return
 * @image_nr: pointer to image number return
 *
 * Handle a sample in kernel address space or in a module. The offset
 * of the sample from the start of the relevant text section will be
 * placed in @offset. @image_nr is unchanged for a kernel sample,
 * but contains the image number of the relevant binary image for
 * a module sample if found. This function requires the global variable
 * got_system_map to be %TRUE to handle module samples.
 */
static void opd_handle_kernel_sample(struct opd_proc *proc, u16 pid, u32 eip, u32 *offset, u16 *image_nr)
{
	(proc) ? (opd_stats[OPD_KERNEL_P]++) : (opd_stats[OPD_KERNEL_NP]++);

	if (got_system_map) {
		if (eip < kernel_end) {
			*offset = eip - kernel_start;
			return;
		}

		/* in a module */

		opd_handle_module_sample(pid,eip,offset,image_nr);
		return;
	}

	*offset = eip - KERNEL_VMA_OFFSET;
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
 * opd_get_offset - process a sample
 * @pid: process id
 * @eip: EIP value
 * @image_nr: image number return
 * @offset: offset return
 * @name: name value return
 *
 * Return %TRUE if the sample passed could be handled correctly,
 * %FALSE otherwise. @offset, @image_nr, and @name are filled
 * in appropriately on success. 
 * @offset is the file offset into the binary image in the case
 * of user-space images, but the offset from the .text section
 * for kernel-space and module-space samples, which means that
 * multi-executable-section modules are not supported yet. It's obviously
 * too slow to determine the file offset of the section and account for it.
 * Perhaps we should further mangle the images file to add the section name
 * for each module section ? FIXME
 *
 * How does this code deal with races concerning pid re-use,
 * exited processes, and changing maps ?
 *
 * pid re-use shouldn't be an issue as pid re-use period is highly
 * likely to be longer than the time process information lasts
 * (OPD_REAP_TIME).
 *
 * If a process has exited without oprofiled ever gathering information,
 * the samples are discarded.
 *
 * Other races can occur with the mapping information. If an EIP is
 * not within any of the currently recorded maps, it could be because
 * the recorded maps were gathered before additional mappings were made
 * by the process. So we re-read the mapping information to try to
 * find the correct map.
 *
 * By far our worst scenario is the following :
 *
 * process A fork()s to process B
 * One or more samples are recorded for process B.
 * The sample(s) are evicted from the hash table.
 * The eviction buffer fills and the daemon reads in the samples.
 * The map information is read.
 * Now process B exec()s a different binary.
 * Until an EIP is out of range, every sample will now
 * be incorrectly associated with the old mappings we have. I can't
 * see any simple way around this. Need to do some testing to see
 * the impact of this problem. There is an theoretically arbritarily
 * long wall clock time between the fork() and exec() but it's not
 * clear how often this will actually happen. Looks like the best solution
 * is a simple "drop mapping info" notifier by springboarding kernel
 * fork(). Maybe a mmap one as well ? FIXME FIXME
 *
 * Additionally a sys_request_module() springboard would fix the module
 * problems (deliver 0 pid to signal handler)
 */ 
int opd_get_offset(u16 pid, u32 eip, u16 *image_nr, u32 *offset,u16 *name)
{
	int i; 
	int c=2; 
	struct opd_proc *proc; 

	/* non-process context kernel sample */
	/* note that in all these, kernel modules are essentially treated
	   the same as a normal process image. We need to some processing
	   to differentiate a kernel sample from a module sample though */ 
	if (!pid) {
		*image_nr = 0;
		*name = 0;
		opd_handle_kernel_sample(NULL,pid,eip,offset,image_nr);
		return TRUE;
	}
		 
	/* FIXME: exe_nr's of zero fall through here - should delete based on eip ? */
	if (!(proc=opd_get_proc(pid,eip))) {
		/* no /proc info but non-zero pid, with samples in the
		   kernel space means this is kswapd or similar */
		if (opd_eip_is_kernel(eip)) {
			*image_nr = 0;
			*name = 0; 
			opd_handle_kernel_sample(NULL,pid,eip,offset,image_nr); 
			return TRUE;
		}
 
		*image_nr = 0;
		*offset = 0;
		*name = 0;
		opd_stats[OPD_LOST_PROCESS]++;
		return FALSE;
	}

	/* now we have a process with same pid and same exe (binary image) */
 
	while (c--) {
		/* look for which map and find offset */ 
		for (i=0;i<proc->nr_maps;i++) {
			if (opd_is_in_map(&proc->maps[i],eip)) {
				*image_nr = proc->maps[i].image_nr;
				*name = proc->exe_nr;
				if (opd_eip_is_kernel(eip))
					opd_handle_kernel_sample(proc,pid,eip,offset,image_nr); 
				else { 
					*offset = opd_map_offset(&proc->maps[i],eip);
					opd_stats[OPD_PROCESS]++;
				}
				return TRUE;
			}
		}

		/* hmm, looks like the map information has become out of
		   date during process startup, something like libdl, or
		   execve()/mmap() notification, so re-do and try again */
		if (!c)
			break;
 
		opd_kill_maps(proc); 
		opd_get_maps(proc); 
	}

	/* now we can't do anything - a mapping has appeared then
	   disappeared before we had a chance */
	*image_nr = 0;
	*offset = 0;
	*name = 0;
	opd_stats[OPD_LOST_MAP_PROCESS]++;
	return FALSE;
}
