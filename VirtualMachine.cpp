#include <stdio.h>
#include <stdlib.h>
#include "VirtualMachine.h"
#include "Machine.h"
#include <vector>
#include <queue>

using namespace std;

extern "C" {
///////////////////////// TCB Class Definition ///////////////////////////
enum state_t { dead, ready, waiting, running };
enum priority_t { low, medium, high };

class TCB
{
public:
    TCB();
    ~TCB();

    int id;
    state_t thread_state;
    priority_t thread_priority;
    // mutex_info, stack_size, stack_base, entry_point, entry_params, SMachineContext
    int ticks_remaining;
};

///////////////////////// Globals ///////////////////////////
volatile int timer;
vector<TCB*> thread_vector;
queue<TCB*> low_priority_queue;
queue<TCB*> medium_priority_queue;
queue<TCB*> high_priority_queue;
TCB* current_thread;

///////////////////////// Function Prototypes ///////////////////////////
TVMMainEntry VMLoadModule(const char *module);

///////////////////////// Utilities ///////////////////////////
void scheduler_action(queue<TCB*> & Q) {
    current_thread->thread_state = ready;
    current_thread = Q.front();
    Q.pop();
    current_thread->thread_state = running;
}

void scheduler() {
    if (!high_priority_queue.empty()) {
        scheduler_action(high_priority_queue);
    }
    else if (!medium_priority_queue.empty()) {
        scheduler_action(medium_priority_queue);
    }
    else if (!low_priority_queue.empty()) {
        scheduler_action(low_priority_queue);
    }
}

void update_thread_ticks () {
    for (uint i = 0; i < thread_vector.size(); ++i) {
        if (thread_vector[i]->ticks_remaining > 0) {
            thread_vector[i]->ticks_remaining--;
        } 
        if (thread_vector[i]->ticks_remaining ==  0) {
            thread_vector[i]->thread_state = ready;
            switch (thread_vector[i]->thread_priority) {
                case high:
                    high_priority_queue.push(thread_vector[i]);
                    break;
                case medium:
                    medium_priority_queue.push(thread_vector[i]);
                    break;
                case low:
                    low_priority_queue.push(thread_vector[i]);
                    break;
            }
            // sets it to -1
            thread_vector[i]->ticks_remaining--;
        }
    }
}

///////////////////////// Callbacks ///////////////////////////
// for ref: typedef void (*TMachineAlarmCallback)(void *calldata);
void timerDecrement(void *calldata) {
    timer -= 1;
}

///////////////////////// VM Functions ///////////////////////////
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
        MachineRequestAlarm(tickms*1000, timerDecrement, NULL); // NULL b/c passing data through global vars
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
