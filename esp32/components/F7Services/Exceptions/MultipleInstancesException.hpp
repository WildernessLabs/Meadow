#include <stdexcept>


class MultipleInstancesException : public std::runtime_error
{
public:
    MultipleInstancesException() : std::runtime_error("MultipleInstancesException") { }
};
