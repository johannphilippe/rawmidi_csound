#include<iostream>
#include<csound.hpp>

int main()
{
    Csound cs;
    int res = cs.CompileCsd("./test.csd");
    std::cout << "$$$ res for compile is : " << res << std::endl;
    res  = cs.Start();
    std::cout << "$$$ res for start is : " << res << std::endl;
    cs.Perform();
    cs.Reset();

    return 0;
}
