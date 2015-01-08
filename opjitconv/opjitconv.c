/**
 * @file opjitconv.c
 * Convert a jit dump file to an ELF file
 *
 * @remark Copyright 2007 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Jens Wilke
 * @Modifications Maynard Johnson
 * @Modifications Daniel Hansel
 * @Modifications Gisle Dankel
 *
 * Copyright IBM Corporation 2007
 *
 */

#include "opjitconv.h"
#include "op_file.h"
#include "op_libiberty.h"

#include <getopt.h>
#include <dirent.h>
#include <fnmatch.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <assert.h>
#include <pwd.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>
#include <wait.h>
#include <sys/file.h>

/*
 * list head.  The linked list is used during parsing (parse_all) to
 * hold all jitentry elements. After parsing, the program works on the
 * array structures (entries_symbols_ascending, entries_address_ascending)
 * and the linked list is not used any more.
 */
struct jitentry * jitentry_list = NULL;
struct jitentry_debug_line * jitentry_debug_line_list = NULL;

/* Global variable for asymbols so we can free the storage later. */
asymbol ** syms;

/* jit dump header information */
enum bfd_architecture dump_bfd_arch;
int dump_bfd_mach;
char const * dump_bfd_target_name;

/* user information for special user 'oprofile' */
struct passwd * pw_oprofile;

char sys_cmd_buffer[PATH_MAX + 1];

/* the bfd handle of the ELF file we write */
bfd * cur_bfd;

/* count of jitentries in the list */
u32 entry_count;
/* maximul space in the entry arrays, needed to add entries */
u32 max_entry_count;
/* array pointing to all jit entries, sorted by symbol names */
struct jitentry ** entries_symbols_ascending;
/* array pointing to all jit entries sorted by address */
struct jitentry ** entries_address_ascending;

/* debug flag, print some information */
int debug;
/* indicates opjitconv invoked by non-root user via operf */
int non_root;
/* indicates we should delete jitdump files owned by the user */
int delete_jitdumps;
/* Session directory where sample data is stored */
char * session_dir;

static struct option long_options [] = {
                                        { "session-dir", required_argument, NULL, 's'},
                                        { "debug", no_argument, NULL, 'd'},
                                        { "delete-jitdumps", no_argument, NULL, 'j'},
                                        { "non-root", no_argument, NULL, 'n'},
                                        { "help", no_argument, NULL, 'h'},
                                        { NULL, 9, NULL, 0}
};
const char * short_options = "s:djnh";

LIST_HEAD(jitdump_deletion_candidates);

/*
 *  Front-end processing from this point to end of the source.
 *    From main(), the general flow is as follows:
 *      1. Find all anonymous samples directories
 *      2. Find all JIT dump files
 *      3. For each JIT dump file:
 *        3.1 Find matching anon samples dir (from list retrieved in step 1)
 *        3.2 mmap the JIT dump file
 *        3.3 Call op_jit_convert to create ELF file if necessary
 */

/* Callback function used for get_matching_pathnames() call to obtain
 * matching path names.
 */
static void get_pathname(char const * pathname, void * name_list)
{
	struct list_head * names = (struct list_head *) name_list;
	struct pathname * pn = xmalloc(sizeof(struct pathname));
	pn->name = xstrdup(pathname);
	list_add(&pn->neighbor, names);
}

static void delete_pathname(struct pathname * pname)
{
	free(pname->name);
	list_del(&pname->neighbor);
	free(pname);
}


static void delete_path_names_list(struct list_head * list)
{
	struct list_head * pos1, * pos2;
	list_for_each_safe(pos1, pos2, list) {
		struct pathname * pname = list_entry(pos1, struct pathname,
						     neighbor);
		delete_pathname(pname);
	}
}

static int mmap_jitdump(char const * dumpfile,
	struct op_jitdump_info * file_info)
{
	int rc = OP_JIT_CONV_OK;
	int dumpfd;

	dumpfd = open(dumpfile, O_RDONLY);
	if (dumpfd < 0) {
		if (errno == ENOENT)
			rc = OP_JIT_CONV_NO_DUMPFILE;
		else
			rc = OP_JIT_CONV_FAIL;
		goto out;
	}
	rc = fstat(dumpfd, &file_info->dmp_file_stat);
	if (rc < 0) {
		perror("opjitconv:fstat on dumpfile");
		rc = OP_JIT_CONV_FAIL;
		goto out;
	}
	file_info->dmp_file = mmap(0, file_info->dmp_file_stat.st_size,
				   PROT_READ, MAP_PRIVATE, dumpfd, 0);
	if (file_info->dmp_file == MAP_FAILED) {
		perror("opjitconv:mmap\n");
		rc = OP_JIT_CONV_FAIL;
	}
out:
	if (dumpfd != -1)
		close(dumpfd);
	return rc;
}

static char const * find_anon_dir_match(struct list_head * anon_dirs,
					char const * proc_id)
{
	struct list_head * pos;
	/* Current PID_MAX_LIMIT (as defined in include/linux/threads.h) is
	 *         4 x 4 x 1024 * 1024 (for 64-bit kernels)
	 * So need to have space for 7 chars for proc_id.
	 */
	char match_filter[12];
	snprintf(match_filter, 12, "*/%s.*", proc_id);
	list_for_each(pos, anon_dirs) {
		struct pathname * anon_dir =
			list_entry(pos, struct pathname, neighbor);
		if (!fnmatch(match_filter, anon_dir->name, 0))
			return anon_dir->name;
	}
	return NULL;
}

int change_owner(char * path)
{
	int rc = OP_JIT_CONV_OK;
	int fd;
	
	if (non_root)
		return rc;
	fd = open(path, 0);
	if (fd < 0) {
		printf("opjitconv: File cannot be opened for changing ownership.\n");
		rc = OP_JIT_CONV_FAIL;
		goto out;
	}
	if (fchown(fd, pw_oprofile->pw_uid, pw_oprofile->pw_gid) != 0) {
		printf("opjitconv: Changing ownership failed (%s).\n", strerror(errno));
		close(fd);
		rc = OP_JIT_CONV_FAIL;
		goto out;
	}
	close(fd);

out:
	return rc;
}

/* Copies the given file to the temporary working directory and sets ownership
 * to 'oprofile:oprofile'.
 */
int copy_dumpfile(char const * dumpfile, char * tmp_dumpfile)
{
#define OP_JITCONV_USECS_TO_WAIT 1000
	int file_locked = 0;
	unsigned int usecs_waited = 0;
	int rc = OP_JIT_CONV_OK;
	int fd = open(dumpfile, O_RDONLY);
	if (fd < 0) {
		perror("opjitconv failed to open JIT dumpfile");
		return OP_JIT_CONV_FAIL;
	}
again:
	// Need OS-level file locking here since opagent may still be writing to the file.
	rc = flock(fd, LOCK_EX | LOCK_NB);
	if (rc) {
		if (usecs_waited < OP_JITCONV_USECS_TO_WAIT) {
			usleep(100);
			usecs_waited += 100;
			goto again;
		} else {
			printf("opjitconv: Unable to obtain lock on %s.\n", dumpfile);
			rc = OP_JIT_CONV_FAIL;
			goto out;
		}
	}
	file_locked = 1;
	sprintf(sys_cmd_buffer, "/bin/cp -p %s %s", dumpfile, tmp_dumpfile);
	if (system(sys_cmd_buffer) != 0) {
		printf("opjitconv: Calling system() to copy files failed.\n");
		rc = OP_JIT_CONV_FAIL;
		goto out;
	}

	if (change_owner(tmp_dumpfile) != 0) {
		printf("opjitconv: Changing ownership of temporary dump file failed.\n");
		rc = OP_JIT_CONV_FAIL;
		goto out;
	}
	
out:
#undef OP_JITCONV_USECS_TO_WAIT
	close(fd);
	if (file_locked)
		flock(fd, LOCK_UN);

	return rc;
}

/* Copies the created ELF file located in the temporary working directory to the
 * final destination (i.e. given ELF file name) and sets ownership to the
 * current user.
 */
int copy_elffile(char * elf_file, char * tmp_elffile)
{
	int rc = OP_JIT_CONV_OK;
	int fd;

	sprintf(sys_cmd_buffer, "/bin/cp -p %s %s", tmp_elffile, elf_file);
	if (system(sys_cmd_buffer) != 0) {
		printf("opjitconv: Calling system() to copy files failed.\n");
		rc = OP_JIT_CONV_FAIL;
		goto out;
	}

	fd = open(elf_file, 0);
	if (fd < 0) {
		printf("opjitconv: File cannot be opened for changing ownership.\n");
		rc = OP_JIT_CONV_FAIL;
		goto out;
	}
	if (fchown(fd, getuid(), getgid()) != 0) {
		printf("opjitconv: Changing ownership failed (%s).\n", strerror(errno));
		close(fd);
		rc = OP_JIT_CONV_FAIL;
		goto out;
	}
	close(fd);
	
out:
	return rc;
}

/* Look for an anonymous samples directory that matches the process ID
 * given by the passed JIT dmp_pathname.  If none is found, it's an error
 * since by agreement, all JIT dump files should be removed every time
 * the user does --reset.  If we do find the matching samples directory,
 * we create an ELF file (<proc_id>.jo) and place it in that directory.
 */
static int process_jit_dumpfile(char const * dmp_pathname,
				struct list_head * anon_sample_dirs,
				unsigned long long start_time,
				unsigned long long end_time,
				char * tmp_conv_dir)
{
	int result_dir_length, proc_id_length;
	int rc = OP_JIT_CONV_OK;
	int jofd;
	struct stat file_stat;
	time_t dumpfile_modtime;
	struct op_jitdump_info dmp_info;
	char * elf_file = NULL;
	char * proc_id = NULL;
	char const * anon_dir;
	char const * dumpfilename = rindex(dmp_pathname, '/');
	/* temporary copy of dump file created for conversion step */
	char * tmp_dumpfile;
	/* temporary ELF file created during conversion step */
	char * tmp_elffile;
	
	verbprintf(debug, "Processing dumpfile %s\n", dmp_pathname);
	
	/* Check if the dump file is a symbolic link.
	 * We should not trust symbolic links because we only produce normal dump
	 * files (no links).
	 */
	if (lstat(dmp_pathname, &file_stat) == -1) {
		printf("opjitconv: lstat for dumpfile failed (%s).\n", strerror(errno));
		rc = OP_JIT_CONV_FAIL;
		goto out;
	}
	if (S_ISLNK(file_stat.st_mode)) {
		printf("opjitconv: dumpfile path is corrupt (symbolic links not allowed).\n");
		rc = OP_JIT_CONV_FAIL;
		goto out;
	}
	
	if (dumpfilename) {
		size_t tmp_conv_dir_length = strlen(tmp_conv_dir);
		char const * dot_dump = rindex(++dumpfilename, '.');
		if (!dot_dump)
			goto chk_proc_id;
		proc_id_length = dot_dump - dumpfilename;
		proc_id = xmalloc(proc_id_length + 1);
		memcpy(proc_id, dumpfilename, proc_id_length);
		proc_id[proc_id_length] = '\0';
		verbprintf(debug, "Found JIT dumpfile for process %s\n",
			   proc_id);

		tmp_dumpfile = xmalloc(tmp_conv_dir_length + 1 + strlen(dumpfilename) + 1);
		strncpy(tmp_dumpfile, tmp_conv_dir, tmp_conv_dir_length);
		tmp_dumpfile[tmp_conv_dir_length] = '\0';
		strcat(tmp_dumpfile, "/");
		strcat(tmp_dumpfile, dumpfilename);
	}
chk_proc_id:
	if (!proc_id) {
		printf("opjitconv: dumpfile path is corrupt.\n");
		rc = OP_JIT_CONV_FAIL;
		goto out;
	}
	if (!(anon_dir = find_anon_dir_match(anon_sample_dirs, proc_id))) {
		/* When profiling with operf, opjitconv will remove old jitdump
		 * files (see _cleanup_jitdumps() for details).  But this cleanup
		 * strategy makes it quite likely that opjitconv will sometimes find
		 * jitdump files that are not owned by the current user or are in use
		 * by other operf users, thus, the current profile data would not have
		 * matching anon samples for such jitdump files.
		 */
		verbprintf(debug, "Informational message: No matching anon samples for %s\n",
		           dmp_pathname);
		rc = OP_JIT_CONV_NO_MATCHING_ANON_SAMPLES;
		goto free_res1;
	}
	
	if (copy_dumpfile(dmp_pathname, tmp_dumpfile) != OP_JIT_CONV_OK)
		goto free_res1;
	
	if ((rc = mmap_jitdump(tmp_dumpfile, &dmp_info)) == OP_JIT_CONV_OK) {
		char * anon_path_seg = rindex(anon_dir, '/');
		if (!anon_path_seg) {
			printf("opjitconv: Bad path for anon sample: %s\n",
			       anon_dir);
			rc = OP_JIT_CONV_FAIL;
			goto free_res2;
		}
		result_dir_length = ++anon_path_seg - anon_dir;
		/* create final ELF file name */
		elf_file = xmalloc(result_dir_length +
				   strlen(proc_id) + strlen(".jo") + 1);
		strncpy(elf_file, anon_dir, result_dir_length);
		elf_file[result_dir_length] = '\0';
		strcat(elf_file, proc_id);
		strcat(elf_file, ".jo");
		/* create temporary ELF file name */
		tmp_elffile = xmalloc(strlen(tmp_conv_dir) + 1 +
				   strlen(proc_id) + strlen(".jo") + 1);
		strncpy(tmp_elffile, tmp_conv_dir, strlen(tmp_conv_dir));
		tmp_elffile[strlen(tmp_conv_dir)] = '\0';
		strcat(tmp_elffile, "/");
		strcat(tmp_elffile, proc_id);
		strcat(tmp_elffile, ".jo");

		// Check if final ELF file exists already
		jofd = open(elf_file, O_RDONLY);
		if (jofd < 0)
			goto create_elf;
		rc = fstat(jofd, &file_stat);
		close(jofd);
		if (rc < 0) {
			perror("opjitconv:fstat on .jo file");
			rc = OP_JIT_CONV_FAIL;
			goto free_res3;
		}
		if (dmp_info.dmp_file_stat.st_mtime >
		    dmp_info.dmp_file_stat.st_ctime)
			dumpfile_modtime = dmp_info.dmp_file_stat.st_mtime;
		else
			dumpfile_modtime = dmp_info.dmp_file_stat.st_ctime;

		/* Final ELF file already exists, so if dumpfile has not been
		 * modified since the ELF file's mod time, we don't need to
		 * do ELF creation again.
		 */
		if (!(file_stat.st_ctime < dumpfile_modtime ||
		    file_stat.st_mtime < dumpfile_modtime)) {
			rc = OP_JIT_CONV_ALREADY_DONE;
			goto free_res3; 
		}

	create_elf:
		verbprintf(debug, "Converting %s to %s\n", dmp_pathname,
			   elf_file);
		/* Set eGID of the special user 'oprofile'. */
		if (!non_root && setegid(pw_oprofile->pw_gid) != 0) {
			perror("opjitconv: setegid to special user failed");
			rc = OP_JIT_CONV_FAIL;
			goto free_res3;
		}
		/* Set eUID of the special user 'oprofile'. */
		if (!non_root && seteuid(pw_oprofile->pw_uid) != 0) {
			perror("opjitconv: seteuid to special user failed");
			rc = OP_JIT_CONV_FAIL;
			goto free_res3;
		}
		/* Convert the dump file as the special user 'oprofile'. */
		rc = op_jit_convert(&dmp_info, tmp_elffile, start_time, end_time);
		if (rc < 0)
			goto free_res3;

		/* Set eUID back to the original user. */
		if (!non_root && seteuid(getuid()) != 0) {
			perror("opjitconv: seteuid to original user failed");
			rc = OP_JIT_CONV_FAIL;
			goto free_res3;
		}
		/* Set eGID back to the original user. */
		if (!non_root && setegid(getgid()) != 0) {
			perror("opjitconv: setegid to original user failed");
			rc = OP_JIT_CONV_FAIL;
			goto free_res3;
		}
		rc = copy_elffile(elf_file, tmp_elffile);
	free_res3:
		free(elf_file);
		free(tmp_elffile);
	free_res2:
		munmap(dmp_info.dmp_file, dmp_info.dmp_file_stat.st_size);
	}
free_res1:
	free(proc_id);
	free(tmp_dumpfile);
out:
	return rc;
}

/* If non-NULL value is returned, caller is responsible for freeing memory.*/
static char * get_procid_from_dirname(char * dirname)
{
	char * ret = NULL;
	if (dirname) {
		char * proc_id;
		int proc_id_length;
		char * fname = rindex(dirname, '/');
		char const * dot = index(++fname, '.');
		if (!dot)
			goto out;
		proc_id_length = dot - fname;
		proc_id = xmalloc(proc_id_length + 1);
		memcpy(proc_id, fname, proc_id_length);
		proc_id[proc_id_length] = '\0';
		ret = proc_id;
	}
out:
	return ret;
}
static void filter_anon_samples_list(struct list_head * anon_dirs)
{
	struct procid {
		struct procid * next;
		char * pid;
	};
	struct procid * pid_list = NULL;
	struct procid * id, * nxt;
	struct list_head * pos1, * pos2;
	list_for_each_safe(pos1, pos2, anon_dirs) {
		struct pathname * pname = list_entry(pos1, struct pathname,
						     neighbor);
		char * proc_id = get_procid_from_dirname(pname->name);
		if (proc_id) {
			int found = 0;
			for (id = pid_list; id != NULL; id = id->next) {
				if (!strcmp(id->pid, proc_id)) {
					/* Already have an entry for this 
					 * process ID, so delete this entry
					 * from anon_dirs.
					 */
					free(pname->name);
					list_del(&pname->neighbor);
					free(pname);
					found = 1;
				}
			}
			if (!found) {
				struct procid * this_proc = 
					xmalloc(sizeof(struct procid));
				this_proc->pid = proc_id;
				this_proc->next = pid_list;
				pid_list = this_proc;
			}
		} else {
			printf("Unexpected result in processing anon sample"
			       " directory\n");
		}
	}
	for (id = pid_list; id; id = nxt) {
		free(id->pid);
		nxt = id->next;
		free(id);
	}
}


static void _add_jitdumps_to_deletion_list(void * all_jitdumps, char const * jitdump_dir )
{
	struct list_head * jd_fnames = (struct list_head *) all_jitdumps;
	struct list_head * pos1, *pos2;
	size_t dir_len = strlen(jitdump_dir);

	list_for_each_safe(pos1, pos2, jd_fnames) {
		struct pathname * dmpfile =
				list_entry(pos1, struct pathname, neighbor);
		struct stat mystat;
		char dmpfile_pathname[dir_len + 20];
		int fd;
		memset(dmpfile_pathname, '\0', dir_len + 20);
		strcpy(dmpfile_pathname, jitdump_dir);
		strcat(dmpfile_pathname,dmpfile->name);
		fd = open(dmpfile_pathname, O_RDONLY);
		if (fd < 0) {
			// Non-fatal error, so just display debug message and continue
			verbprintf(debug, "opjitconv: cannot open jitdump file %s\n",
			           dmpfile_pathname);
			continue;
		}
		if (fstat(fd, &mystat) < 0) {
			// Non-fatal error, so just display debug message and continue
			verbprintf(debug, "opjitconv: cannot fstat jitdump file");
			close(fd);
			continue;
		}
		close(fd);
		if (!non_root || geteuid() == mystat.st_uid) {
			struct jitdump_deletion_candidate * jdc =
					xmalloc(sizeof(struct jitdump_deletion_candidate));
			jdc->name = xstrdup(dmpfile->name);
			list_add(&jdc->neighbor, &jitdump_deletion_candidates);
		}
	}
}

static int op_process_jit_dumpfiles(char const * session_dir,
	unsigned long long start_time, unsigned long long end_time)
{
	struct list_head * pos1, * pos2;
	int rc = OP_JIT_CONV_OK;
	char jitdumpfile[PATH_MAX + 1];
	char oprofile_tmp_template[PATH_MAX + 1];
	char const * jitdump_dir = "/tmp/.oprofile/jitdump/";

	LIST_HEAD(jd_fnames);
	char const * anon_dir_filter = "*/{dep}/{anon:anon}/[0-9]*.*";
	LIST_HEAD(anon_dnames);
	char const * samples_subdir = "/samples/current";
	int samples_dir_len = strlen(session_dir) + strlen(samples_subdir);
	char * samples_dir;
	/* temporary working directory for dump file conversion step */
	char * tmp_conv_dir = NULL;

	if (non_root)
		sprintf(oprofile_tmp_template, "%s/tmp", session_dir);
	else
		strcpy(oprofile_tmp_template, "/tmp/oprofile.XXXXXX");

	/* Create a temporary working directory used for the conversion step.
	 */
	if (non_root) {
		sprintf(sys_cmd_buffer, "/bin/rm -rf %s", oprofile_tmp_template);
		if (system(sys_cmd_buffer) != 0) {
			printf("opjitconv: Removing temporary working directory %s failed.\n",
			       oprofile_tmp_template);
			rc = OP_JIT_CONV_TMPDIR_NOT_REMOVED;
		} else {
			if (!mkdir(oprofile_tmp_template, S_IRWXU | S_IRWXG ))
				tmp_conv_dir = oprofile_tmp_template;
		}
	} else {
		tmp_conv_dir = mkdtemp(oprofile_tmp_template);
	}

	if (tmp_conv_dir == NULL) {
		printf("opjitconv: Temporary working directory %s cannot be created.\n",
		       oprofile_tmp_template);
		perror("Exiting due to error");
		rc = OP_JIT_CONV_FAIL;
		goto out;
	}


	errno = 0;
	if ((rc = get_matching_pathnames(&jd_fnames, get_pathname,
		jitdump_dir, "*.dump", NO_RECURSION)) < 0
			|| list_empty(&jd_fnames)) {
		if (errno) {
			if (errno != ENOENT) {
				char msg[PATH_MAX];
				strcpy(msg, "opjitconv: fatal error trying to find JIT dump files in ");
				strcat(msg, jitdump_dir);
				perror(msg);
				rc = OP_JIT_CONV_FAIL;
			} else {
				verbprintf(debug, "opjitconv: Non-fatal error trying to find JIT dump files in %s: %s\n",
				           jitdump_dir, strerror(errno));
				rc = OP_JIT_CONV_NO_DUMPFILE;
			}
		}
		goto rm_tmp;
	}

	if (delete_jitdumps)
		_add_jitdumps_to_deletion_list(&jd_fnames, jitdump_dir);

	/* Get user information (i.e. UID and GID) for special user 'oprofile'.
	 */
	if (non_root) {
		pw_oprofile = NULL;
	} else {
		pw_oprofile = getpwnam("oprofile");
		if (pw_oprofile == NULL) {
			printf("opjitconv: User information for special user oprofile cannot be found.\n");
			rc = OP_JIT_CONV_FAIL;
			goto rm_tmp;
		}
	}

	/* Change ownership of the temporary working directory to prevent other users
	 * to attack conversion process.
	 */
	if (change_owner(tmp_conv_dir) != 0) {
		printf("opjitconv: Changing ownership of temporary directory failed.\n");
		rc = OP_JIT_CONV_FAIL;
		goto rm_tmp;
	}
	
	samples_dir = xmalloc(samples_dir_len + 1);
	sprintf(samples_dir, "%s%s", session_dir, samples_subdir);
	if (get_matching_pathnames(&anon_dnames, get_pathname,
				    samples_dir, anon_dir_filter,
				    MATCH_DIR_ONLY_RECURSION) < 0
	    || list_empty(&anon_dnames)) {
		rc = OP_JIT_CONV_NO_ANON_SAMPLES;
		goto rm_tmp;
	}
	/* When using get_matching_pathnames to find anon samples,
	 * the list that's returned may contain multiple entries for
	 * one or more processes; e.g.,
	 *    6868.0x100000.0x103000
	 *    6868.0xdfe77000.0xdec40000
	 *    7012.0x100000.0x103000
	 *    7012.0xdfe77000.0xdec40000
	 *
	 * So we must filter the list so there's only one entry per
	 * process.
	 */
	filter_anon_samples_list(&anon_dnames);

	/* get_matching_pathnames returns only filename segment when
	 * NO_RECURSION is passed, so below, we add back the JIT
	 * dump directory path to the name.
	 */
	list_for_each_safe(pos1, pos2, &jd_fnames) {
		struct pathname * dmpfile =
			list_entry(pos1, struct pathname, neighbor);
		strncpy(jitdumpfile, jitdump_dir, PATH_MAX);
		strncat(jitdumpfile, dmpfile->name, PATH_MAX);
		rc = process_jit_dumpfile(jitdumpfile, &anon_dnames,
					  start_time, end_time, tmp_conv_dir);
		if (rc == OP_JIT_CONV_FAIL) {
			verbprintf(debug, "JIT convert error %d\n", rc);
			goto rm_tmp;
		}
		delete_pathname(dmpfile);
	}
	delete_path_names_list(&anon_dnames);
	
rm_tmp:
	/* Delete temporary working directory with all its files
	 * (i.e. dump and ELF file).
	 */
	sprintf(sys_cmd_buffer, "/bin/rm -rf '%s'", tmp_conv_dir);
	if (system(sys_cmd_buffer) != 0) {
		printf("opjitconv: Removing temporary working directory failed.\n");
		rc = OP_JIT_CONV_TMPDIR_NOT_REMOVED;
	}
	
out:
	return rc;
}

static void _cleanup_jitdumps(void)
{
	struct list_head * pos1, *pos2;
	char const * jitdump_dir = "/tmp/.oprofile/jitdump/";
	size_t dir_len = strlen(jitdump_dir);
	char dmpfile_pathname[dir_len + 20];
	char proc_fd_dir[PATH_MAX];

	if (!delete_jitdumps)
		return;

	/* The delete_jitdumps flag tells us to try to delete the jitdump files we found
	 * that belong to this user.  Only operf should pass the --delete-jitdumps
	 * argument to opjitconv since legacy oprofile uses opcontrol to delete old
	 * jitdump files.
	 *
	 * The code below will only delete jitdump files that are not currently
	 * being used by another process.
	 */
	list_for_each_safe(pos1, pos2, &jitdump_deletion_candidates) {
		DIR * dir;
		struct dirent * dirent;
		int pid;
		size_t dmpfile_name_len;
		int do_not_delete = 0;
		struct jitdump_deletion_candidate * cand = list_entry(pos1,
		                                                      struct jitdump_deletion_candidate,
		                                                      neighbor);
		memset(dmpfile_pathname, '\0', dir_len + 20);
		memset(proc_fd_dir, '\0', PATH_MAX);

		if ((sscanf(cand->name, "%d", &pid)) != 1) {
			verbprintf(debug, "Cannot get process id from jitdump file %s\n",
			           cand->name);
			continue;
		}

		strcpy(dmpfile_pathname, jitdump_dir);
		strcat(dmpfile_pathname, cand->name);
		dmpfile_name_len = strlen(dmpfile_pathname);

		sprintf(proc_fd_dir, "/proc/%d/fd/", pid);
		if ((dir = opendir(proc_fd_dir))) {
			size_t proc_fd_dir_len = strlen(proc_fd_dir);
			while ((dirent = readdir(dir))) {
				if (dirent->d_type == DT_LNK) {
					char buf[1024];
					char fname[1024];
					memset(buf, '\0', 1024);
					memset(fname, '\0', 1024);
					memset(buf, '\0', 1024);
					strcpy(fname, proc_fd_dir);
					strncat(fname, dirent->d_name, 1023 - proc_fd_dir_len);
					if (readlink(fname, buf, 1023) > 0) {
						verbprintf(debug, "readlink found for %s\n", buf);
						if (strncmp(buf, dmpfile_pathname,
						            dmpfile_name_len) == 0) {
							do_not_delete = 1;
							break;
						}
					}
				}
			}
			closedir(dir);
		}
		if (!do_not_delete) {
			if (remove(dmpfile_pathname))
				verbprintf(debug, "Unable to delete %s: %s\n", dmpfile_pathname,
				           strerror(errno));
		}
	}
	list_for_each_safe(pos1, pos2, &jitdump_deletion_candidates) {
		struct jitdump_deletion_candidate * pname = list_entry(pos1,
		                                                       struct jitdump_deletion_candidate,
		                                                       neighbor);
		free(pname->name);
		list_del(&pname->neighbor);
		free(pname);
	}

}

static void __print_usage(void)
{
	fprintf(stderr, "usage: opjitconv [--debug | --non-root | --delete-jitdumps ] --session-dir=<dir> <starttime> <endtime>\n");
}

static int _process_args(int argc, char * const argv[])
{
	int keep_trying = 1;
	int idx_of_non_options = 0;
	char * prev_env = getenv("POSIXLY_CORRECT");
	setenv("POSIXLY_CORRECT", "1", 0);
	while (keep_trying) {
		int option_idx = 0;
		int c = getopt_long(argc, argv, short_options, long_options, &option_idx);
		switch (c) {
		case -1:
			if (optind != argc) {
				idx_of_non_options = optind;
			}
			keep_trying = 0;
			break;
		case '?':
			printf("non-option detected at optind %d\n", optind);
			keep_trying = 0;
			idx_of_non_options = -1;
			break;
		case 's':
			session_dir = optarg;
			break;
		case 'd':
			debug = 1;
			break;
		case 'n':
			non_root = 1;
			break;
		case 'j':
			delete_jitdumps = 1;
			break;
		case 'h':
			break;
		default:
			break;
		}
	}

	if (prev_env == NULL)
		unsetenv("POSIXLY_CORRECT");

	return idx_of_non_options;
}

int main(int argc, char * const argv[])
{
	unsigned long long start_time, end_time;
	struct stat filestat;
	int non_options_idx, rc = 0;
	size_t sessdir_len = 0;

	debug = 0;
	non_root = 0;
	delete_jitdumps = 0;
	session_dir = NULL;
	non_options_idx = _process_args(argc, argv);
	// We need the session_dir and two non-option values passed -- starttime and endtime.
	if (!session_dir || (non_options_idx != argc - 2)) {
		__print_usage();
		fflush(stdout);
		rc = EXIT_FAILURE;
		goto out;
	}

	/*
	 * Check for a maximum of 4096 bytes (Linux path name length limit) minus 16 bytes
	 * (to be used later for appending samples sub directory) minus 1 (for terminator).
	 * Integer overflows according to the session dir parameter (user controlled)
	 * are not possible anymore.
	 */
	if ((sessdir_len = strlen(session_dir)) >= (PATH_MAX - 17)) {
		printf("opjitconv: Path name length limit exceeded for session directory\n");
		rc = EXIT_FAILURE;
		goto out;
	}

	if (stat(session_dir, &filestat)) {
		perror("stat operation on passed session-dir failed");
		rc = EXIT_FAILURE;
		goto out;
	}
	if (!S_ISDIR(filestat.st_mode)) {
		printf("Passed session-dir %s is not a directory\n", session_dir);
		rc = EXIT_FAILURE;
		goto out;
	}

	start_time = atol(argv[non_options_idx++]);
	end_time = atol(argv[non_options_idx]);

	if (start_time > end_time) {
		rc = EXIT_FAILURE;
		goto out;
	}
	verbprintf(debug, "start time/end time is %llu/%llu\n",
		   start_time, end_time);
	rc = op_process_jit_dumpfiles(session_dir, start_time, end_time);
	if (delete_jitdumps)
		_cleanup_jitdumps();

	if (rc > OP_JIT_CONV_OK) {
		verbprintf(debug, "opjitconv: Ending with rc = %d. This code"
			   " is usually OK, but can be useful for debugging"
			   " purposes.\n", rc);
		rc = OP_JIT_CONV_OK;
	}
	fflush(stdout);
	if (rc == OP_JIT_CONV_OK)
		rc = EXIT_SUCCESS;
	else
		rc = EXIT_FAILURE;
out:
	_exit(rc);
}
