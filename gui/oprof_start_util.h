#ifndef OPROF_START_UTIL_H
#define OPROF_START_UTIL_H

#include <cmath>
#include <string>
#include <iostream>
#include <vector> 
 
struct daemon_status {
	daemon_status();
	bool running;
	std::string runtime;
	unsigned int nr_interrupts;
};
 
inline double ratio(double x1, double x2)
{
	return fabs(((x1 - x2) / x2)) * 100;
}
 
std::string const get_user_filename(std::string const & filename); 
bool check_and_create_config_dir();
std::string const format(std::string const & orig, uint const maxlen);
int do_exec_command(std::string const & cmd, std::vector<std::string> args = std::vector<std::string>());
std::string const do_open_file_or_dir(std::string const & base_dir, bool dir_only);
unsigned long get_cpu_speed(); 
 
#endif // OPROF_START_UTIL_H 
