/**
 * @file child_reader.h
 * Facility for reading from child processes
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 * 
 * @author Philippe Elie <phil_el@wanadoo.fr>
 * @author John Levon <moz@compsoc.man.ac.uk>
 */

#ifndef CHILD_READER_H
#define CHILD_READER_H

#include <sys/types.h>
#include "op_types.h"

#include <vector>
#include <string>

/**
 * a class to read stdout / stderr from a child process.
 *
 * two interfaces are provided. read line by line: getline() or read all data 
 * in one : get_data(). In all case get_data() must be called once to flush the
 * stderr child output
 */
/*
 * FIXME: code review is needed:
 *  - check the getline()/get_data()/block_read() interface.
 *  the expected behavior is:
 *  caller can call getline until nothing is available from the stdout of the
 * child. in this case child stderr is acumulated in buf2 and can be read
 * through get_data(). get_data() is blocking until the child close stderr /
 * stdout (even if the child die by a signal ?). The following corner case must
 * work but I'm unsure if the code reflect this behavior: the last line of the
 * child stdout have not necessarilly a LF terminator. the child can output any
 * size of data in stderr.
 */
class child_reader {
public:
	/** fork a process. use error() to get error code. Do not try to
	 * use other public member interface if error() return non-zero */
	child_reader(std::string const & cmd,
		std::vector<std::string> const & args);

	/** wait for the termination of the child process if this have not
	 * already occur. In this case return code of the child process is not 
	 * available. */
	~child_reader();

	/** fill result from on line of stdout of the child process.
	 * must be used as:
	 * child_reader reader(...);
	 * while (reader.getline(line)) .... */
	bool getline(std::string & result);

	/** fill out / err with the stdout / stderr of the child process.
	 * You can call this after calling one or more time getline(...). This
	 * call is blocking until the child die and so on all subsequent
	 * call will fail */
	bool get_data(std::ostream & out, std::ostream & err);

	/** rather to rely on dtor to wait for the termination of the child you
	 * can use terminate_process() to get the return code of the child
	 * process */
	int terminate_process();

	/** return the status of the first error encoutered
	 * < 0 : the child process can not be forked
	 * > 0 : the child process have return a non zero value */
	int error() const { return first_error; }
 
private:
	// ctor helper: create the child process.
	void exec_command(std::string const & cmd,
			  std::vector<std::string> const & args);
	// return false when eof condition is reached on fd1. fd2 can have
	// already input in the pipe buffer or in buf2.
	bool block_read();

	fd_t fd1;
	fd_t fd2;
	ssize_t pos1;
	ssize_t end1;
	ssize_t pos2;
	ssize_t end2;
	pid_t pid;
	int first_error;
	// child stderr is handled especially, we need to retain data even
	// if caller read only stdout of the child.
	char * buf2;
	ssize_t sz_buf2;
	char * buf1;
	bool is_terminated;
};

#endif // CHILD_READER_H
