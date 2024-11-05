#include <iostream>
#include <exception>

int main()
{
    try
    {
        std::cout << "Nisse Proto 1\n";
    }
    catch(std::exception const& e)
    {
        std::cerr << "Exception: " << e.what() << "\n";
        throw;
    }
    catch(...)
    {
        std::cerr << "Exception: UNKNOWN\n";
        throw;
    }
}
