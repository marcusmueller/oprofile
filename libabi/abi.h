#include <string>
#include <map>

struct Abi_exception : std::exception
{
	std::string const desc;
	explicit Abi_exception(std::string const d);
};

class Abi 
{
	std::map<std::string,int> slots;
public:
	Abi();
	Abi(Abi const & other);
	int need(std::string const key) const throw (Abi_exception);
	bool operator==(Abi const & other) const;
	friend ostream & operator<<(std::ostream & o, Abi const & abi);
	friend istream & operator>>(std::istream & i, Abi & abi);
};

