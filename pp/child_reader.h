/* COPYRIGHT (C) 2001 Philippe Elie
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef CHILD_READER_H
#define CHILD_READER_H

#include <vector>
#include <string>

#include "../op_user.h"

/// a class to read stdout / stderr from a child process.
// code review is needed:
//  - check the getline()/get_block()/read_block() interface.
//  the expected behavior is:
//  caller can call getline until nothing is available from the stdout of the
// child. in this case child stderr is acumulated in buf2 and can be read
// through get_block(). get_block() is blocking until the child close stderr /
// stdout (even if the child die by a signal ?). The following corner case must
// work but I'm unsure if the code reflect this behavior: the last line of the
// child stdout have not necessarilly a LF terminator. the child can output any
// size of data in stderr.
class ChildReader {
public:
	/// fork a process. use error() to get error code. Do not try to
	/// use other public member interface if error() return non-zero
	ChildReader(std::string const & cmd,
		    std::vector<std::string> const & args);

	/// wait for the termination of the child process if this have not
	/// occur. In this cas return code of the child process is not
	/// available.
	~ChildReader();

	/// two interface are possible. read line by line or read in one block.
	/// getline only read from stdout of the child. get_block() must be
	/// called to flush the input from the stderr child.
	
	/// fill result from on line of stdout of the child process.
	/// must be used as:
	/// ChildReader reader(...);
	/// while (reader.getline(line) ....
	bool getline(std::string & result);

	/// fill out / err with the stdout / stderr of the child process.
	bool get_data(ostream & out, ostream & err);

	/// rather to rely on dtor to wait for the termination of the child you
	/// can use terminate_process() to get the return code of the child
	/// process
	int terminate_process();

	/// return the status of the first error encoutered
	/// < 0 : the child process can not be child
	/// > 0 : the child process have return a non zero value
	int error() const { return first_error; }
private:
	/// ctor helper: create the child process.
	void exec_command(std::string const & cmd,
			  std::vector<std::string> const & args);
	/// return false when eof condition is reached on fd1. fd2 can have
	/// already input in the pipe buffer or in buf2.
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
	// caller read only stdout of the child.
	char * buf2;
	ssize_t sz_buf2;
	char buf1[PIPE_BUF];
	bool is_terminated;
};

#endif /* ! CHILD_READER_H */
