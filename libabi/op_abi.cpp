#include "op_abi.h"
#include "abi.h"
#include <fstream>

using namespace std;

int op_write_abi_to_file(char const * abi_file)
{
	Abi curr;
	ofstream file(abi_file);
	if (!file) 
		return 0;
	file << curr;
	return 1;
}

int op_abi_compatible_p(char const * abi_file)
{
	Abi curr, other;
	ifstream file(abi_file);
	if (!file)
		return 0;
	file >> other;
	if (curr == other)
		return 1;
	else
		return 0;
}
