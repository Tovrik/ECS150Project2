#include <stdio.h>
#include <stdlib.h>
#include "VirtualMachine.h"
#include "Machine.h"

// using namespace std;

extern "C" {
volatile int timer;

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

// void MachineFileWrite(int fd, void *data, int length, TMachineFileCallback callback, void *calldata);

TVMStatus VMFileWrite(int filedescriptor, void *data, int *length) {
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

// for ref: typedef void (*TMachineAlarmCallback)(void *calldata);
void dec(void *calldata) {
    timer -= 1;
}

// VMStart
// -Use typedef to define a function pointer and then create an instance/variable of that function pointer type
// -Then set that variable equal to the address returned from VMLoadModule
// -Then you'll want to make sure the returned address from VMLoadModule wasn't an error value
// -Next, call the function pointed to by that function pointer - holding the address of VMMain
// -Then you'll want to check which functions are called in the hello.c
// -You'll notice that it in hello.c it calls VMPrint
// -So get VMPrint to work using some system calls and error handling

// So the general order you want to call things in VMStart would be something like:
// 1. VMLoadModule
// 2. MachineInitialize
// 3. MachineRequestAlarm
// 4. MachineEnableSignals
// 5. VMMain (whatever VMLoadModule returned)


TVMStatus VMStart(int tickms, int machinetickms, int argc, char *argv[]) { //The time in milliseconds of the virtual machine tick is specified by the tickms parameter, the machine responsiveness is specified by the machinetickms.
    typedef void (*TVMMainEntry)(int argc, char* argv[]);
    TVMMainEntry VMMain;
    VMMain = VMLoadModule(argv[0]);
    if (VMMain != NULL) {
        MachineInitialize(machinetickms); //The timeout parameter specifies the number of milliseconds the machine will sleep between checking for requests.
        MachineRequestAlarm(tickms*1000, dec, NULL); // NULL b/c passing data through global vars
        MachineEnableSignals();
        VMMain(argc, argv);
        return VM_STATUS_SUCCESS;
    }
    else {
        return VM_STATUS_FAILURE;
    }
}

// if tick == VM_TIMEOUT_INFINITE the current thread yields to the next ready thread. It basically 
// goes to the end of its ready queue. The processing quantum is the amount of time that each thread 
// (or process) gets for its time slice. You can assume it is one tick.
TVMStatus VMThreadSleep(TVMTick tick){
    if (tick == VM_TIMEOUT_INFINITE) {
        return VM_STATUS_ERROR_INVALID_PARAMETER;
    }
    else {
        timer = tick;
        //sleep(tick); // NOT SUPPOSED TO USE SLEEP
        while (timer != 0);
        return VM_STATUS_SUCCESS;
    }
}
} // end extern C
