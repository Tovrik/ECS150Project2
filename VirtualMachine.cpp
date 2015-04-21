#include <stdio.h>
#include <stdlib.h>
#include "VirtualMachine.h"
#include "Machine.h"

// using namespace std;

// VMStart
// -Use typedef to define a function pointer and then create an instance/variable of that function pointer type
// -Then set that variable equal to the address returned from VMLoadModule
// -Then you'll want to make sure the returned address from VMLoadModule wasn't an error value
// -Next, call the function pointed to by that function pointer - holding the address of VMMain
// -Then you'll want to check which functions are called in the hello.c
// -You'll notice that it in hello.c it calls VMPrint
// -So get VMPrint to work using some system calls and error handling

extern "C" {TVMMainEntry VMLoadModule(const char *module);}


TVMStatus VMStart(int tickms, int machinetickms, int argc, char *argv[]) {
    typedef void (*TVMMainEntry)(int argc, char* argv[]);
    TVMMainEntry MyMain;
    MyMain = VMLoadModule();
    MyMain(argc, argv);
};

