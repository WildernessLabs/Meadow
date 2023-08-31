#include <stdexcept>


class NotImplementedException : public std::runtime_error
{
public:
    NotImplementedException() : std::runtime_error("NotImplementedException") { }
};

