/**
 * @file child_reader.cpp
 * Facility for reading from child processes
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie <phil_el@wanadoo.fr>
 * @author John Levon <moz@compsoc.man.ac.uk>
 */

#include <unistd.h>
#include <sys/wait.h>

#include <cerrno>
#include <sstream>
#include <iostream>

#include "child_reader.h"
#include "op_libiberty.h"

using std::string;
using std::vector;
using std::cerr;
using std::endl;
using std::ostream;
using std::max;

child_reader::child_reader(string const & cmd, vector<string> const & args)
	:
	fd1(-1),
	fd2(-1),
	pos1(0),
	end1(0),
	pos2(0),
	end2(0),
	pid(0),
	first_error(0),
	buf2(0),
	sz_buf2(0),
	buf1(new char [PIPE_BUF]),
	is_terminated(true)
{
	exec_command(cmd, args);
}

child_reader::~child_reader()
{
	terminate_process();
	delete [] buf1;
	if (buf2) {
		// allocated through C alloc
		free(buf2);
	}
}

void child_reader::exec_command(string const & cmd, vector<string> const & args)
{
	int pstdout[2];
	int pstderr[2];

	if (pipe(pstdout) == -1 || pipe(pstderr) == -1) {
		first_error = errno;
		cerr << "Couldn't create pipes !" << endl;
		return;
	}

	pid = fork();
	switch (pid) {
		case -1:
			first_error = errno;
			cerr << "Couldn't fork !" << endl;
			return;

		case 0: {
			char const ** argv = new char const * [args.size() + 2];
			size_t i;
			argv[0] = cmd.c_str();

			for (i = 1 ; i <= args.size() ; ++i)
				argv[i] = args[i - 1].c_str();

			argv[i] = 0;

			// child: we can cleanup a few fd
			close(pstdout[0]);
			dup2(pstdout[1], STDOUT_FILENO);
			close(pstdout[1]);
			close(pstderr[0]);
			dup2(pstderr[1], STDERR_FILENO);
			close(pstderr[1]);

			execvp(cmd.c_str(), (char * const *)argv);

			int ret_code = errno;

			// we can communicate with parent by writing to stderr
			// and by returning a non zero error code. Setting
			// first_error in the child is a non-sense

			// we are in the child process: so this error message
			// is redirect to the parent process
			cerr << "Couldn't exec \"" << cmd << "\" : "
			     << strerror(errno) << endl;
			exit(ret_code);
		}

		default:;
			// parent: we do not write on these fd.
			close(pstdout[1]);
			close(pstderr[1]);
	}

	fd1 = pstdout[0];
	fd2 = pstderr[0];

	is_terminated = false;

	return;
}

bool child_reader::block_read()
{
	fd_set read_fs;

	FD_ZERO(&read_fs);
	FD_SET(fd1, &read_fs);
	FD_SET(fd2, &read_fs);

	if (select(max(fd1, fd2) + 1, &read_fs, 0, 0, 0) >= 0) {
		if (FD_ISSET(fd1, &read_fs))
			end1 = read(fd1, buf1, PIPE_BUF);

		if (FD_ISSET(fd2, &read_fs)) {
			if (end2 >= sz_buf2) {
				sz_buf2 += PIPE_BUF;
				buf2 = (char *)xrealloc(buf2, sz_buf2);
			}

			ssize_t temp = read(fd2, buf2, sz_buf2 - end2);
			if (temp > 0)
				end2 += temp;
		}
	}

	bool ret = !(end1 == 0 && end2 == 0);

	if (end1 == -1)
		end1 = 0;
	if (end2 == -1)
		end2 = 0;

	return ret;
}

bool child_reader::getline(string & result)
{
	// some stl lacks string::clear()
	result.erase(result.begin(), result.end());

	bool ok = true;
	bool ret = true;
	do {
		if (pos1 >= end1) {
			pos1 = 0;
			ret = block_read();
		}

		// for efficiency try to copy as much as we can of data
		ssize_t temp_pos = pos1;
		while (temp_pos < end1 && ok) {
			char ch = buf1[temp_pos++];
			if (ch == '\n')
				ok = false;
		}

		// !ok ==> endl has been read so do not copy it.
		result.append(&buf1[pos1], (temp_pos - pos1) - !ok);

		pos1 = temp_pos;
	} while (ok && end1);

	// Is this correct ?
	return end1 != 0 || result.length() != 0;
}

bool child_reader::get_data(ostream & out, ostream & err)
{
	bool ret = true;
	while (ret) {
		ret = block_read();

		out.write(buf1, end1);
		err.write(buf2, end2);

		end1 = end2 = 0;
	}

	return first_error == 0;
}

int child_reader::terminate_process()
{
	if (!is_terminated) {
		int ret;
		waitpid(pid, &ret, 0);

		first_error = WEXITSTATUS(ret);
	}

	if (fd1 != -1)
		close(fd1);
	if (fd2 != -1)
		close(fd2);

	return first_error;
}
