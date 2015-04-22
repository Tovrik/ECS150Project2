#include <stdio.h>
#include <stdlib.h>
#include "VirtualMachine.h"
#include "Machine.h"

// using namespace std;



extern "C" {
TVMMainEntry VMLoadModule(const char *module);


TVMStatus VMFileClose(int filedescriptor) {
    if (close(filedescriptor) == 0) {
        return VM_STATUS_SUCCESS;
    }
    else {
        return VM_STATUS_FAILURE;
    }

}

// void MachineFileOpen(const char *filename, int flags, int mode, TMachineFileCallback callback, void *calldata);


// TVMStatus VMFileOpen(const char *filename, int flags, int mode, int *filedescriptor) {
//     if (filename == NULL || filedescriptor == NULL) {
//         return VM_STATUS_ERROR_INVALID_PARAMETER;
//     }
//     else {
//         MachineFileOpen(filename, flags, mode, ___, ___);
//         *filedescriptor = open(filename, flags, mode);
//         if (*filedescriptor != -1) {
//             return VM_STATUS_SUCCESS;
//         }
//         else {
//             return VM_STATUS_FAILURE;
//         }
//     }
// }


TVMStatus VMFileWrite(int filedescriptor, void *data, int *length) {
    //VMFileOpen(, &filedescriptor);
    if (data == NULL || length == NULL) {
        return VM_STATUS_ERROR_INVALID_PARAMETER;
    }
    else {
        if (write(filedescriptor, (char *)data, *length) != -1) {
            return VM_STATUS_SUCCESS;
        }
        else {
            return VM_STATUS_FAILURE;
        }
    }
}

// VMStart
// -Use typedef to define a function pointer and then create an instance/variable of that function pointer type
// -Then set that variable equal to the address returned from VMLoadModule
// -Then you'll want to make sure the returned address from VMLoadModule wasn't an error value
// -Next, call the function pointed to by that function pointer - holding the address of VMMain
// -Then you'll want to check which functions are called in the hello.c
// -You'll notice that it in hello.c it calls VMPrint
// -So get VMPrint to work using some system calls and error handling
TVMStatus VMStart(int tickms, int machinetickms, int argc, char *argv[]) {
    typedef void (*TVMMainEntry)(int argc, char* argv[]);
    TVMMainEntry VMMain;
    VMMain = VMLoadModule(argv[0]);
    if (VMMain != NULL) {
        VMMain(argc, argv);
        return VM_STATUS_SUCCESS;
    }
    else {
        return VM_STATUS_FAILURE;
    }
}
} // end extern C
