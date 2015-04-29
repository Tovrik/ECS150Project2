#include <stdio.h>
#include <stdlib.h>
#include "VirtualMachine.h"
#include "Machine.h"
#include <vector>
#include <queue>

using namespace std;

extern "C" {
///////////////////////// TCB Class Definition ///////////////////////////
class TCB
{
public:
    TCB(TVMThreadIDRef id, TVMThreadState thread_state, TVMThreadPriority thread_priority, TVMMemorySize stack_size, TVMThreadEntry entry_point, void *entry_params, TVMTick ticks_remaining) :
        id(id),
        thread_state(thread_state),
        thread_priority(thread_priority),
        stack_size(stack_size),
        entry_point(entry_point),
        entry_params(entry_params),
        ticks_remaining(ticks_remaining) {
            stack_base = new uint8_t[stack_size];
        }
    ~TCB();

    TVMThreadIDRef id;
    TVMThreadState thread_state;
    TVMThreadPriority thread_priority;
    TVMMemorySize stack_size;
    uint8_t *stack_base;
    TVMThreadEntry entry_point;
    void *entry_params;
    TVMTick ticks_remaining;
    SMachineContextRef machine_context_ref; // for the context to switch to/from the thread

// Possibly need something to hold file return type
// Possibly hold a pointer or ID of mutex waiting on
// Possibly hold a list of held mutexes
};

///////////////////////// Globals ///////////////////////////
vector<TCB*> thread_vector;
queue<TCB*> low_priority_queue;
queue<TCB*> normal_priority_queue;
queue<TCB*> high_priority_queue;
TCB* current_thread;

volatile int timer;
TMachineSignalStateRef sigstate;

///////////////////////// Function Prototypes ///////////////////////////
TVMMainEntry VMLoadModule(const char *module);

///////////////////////// Utilities ///////////////////////////
void determine_queue_and_push(TCB* thread) {
    if (thread->thread_priority == VM_THREAD_PRIORITY_LOW) {
        low_priority_queue.push(thread);
    }
    else if (thread->thread_priority == VM_THREAD_PRIORITY_NORMAL) {
        normal_priority_queue.push(thread);
    }
    else if (thread->thread_priority == VM_THREAD_PRIORITY_HIGH) {
        high_priority_queue.push(thread);
    }
}

void scheduler_action(queue<TCB*> & Q) {
    // set current thread to ready state
    current_thread->thread_state = VM_THREAD_STATE_READY;
    // STILL NEED TO PUSH CURRENT THREAD TO END OF RESPECTIVE QUEUE
    // MachineContextSwitch(mcntxold,mcntxnew), old = current, new = next = Q.front()
    MachineContextSwitch(current_thread->machine_context_ref, Q.front()->machine_context_ref);
    // set current to next and remove from Q
    current_thread = Q.front();
    Q.pop();
    // set current to running
    current_thread->thread_state = VM_THREAD_STATE_RUNNING;
}

void scheduler() {
    if (!high_priority_queue.empty()) {
        scheduler_action(high_priority_queue);
    }
    else if (!normal_priority_queue.empty()) {
        scheduler_action(normal_priority_queue);
    }
    else if (!low_priority_queue.empty()) {
        scheduler_action(low_priority_queue);
    }
}

void update_thread_ticks () {
    for (int i = 0; i < thread_vector.size(); ++i) {
        if (thread_vector[i]->ticks_remaining > 0) {
            thread_vector[i]->ticks_remaining--;
        }
        if (thread_vector[i]->ticks_remaining ==  0) {
            thread_vector[i]->thread_state = VM_THREAD_STATE_READY;
            switch (thread_vector[i]->thread_priority) {
                case VM_THREAD_PRIORITY_HIGH :
                    high_priority_queue.push(thread_vector[i]);
                    break;
                case VM_THREAD_PRIORITY_NORMAL:
                    normal_priority_queue.push(thread_vector[i]);
                    break;
                case VM_THREAD_PRIORITY_LOW:
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

void SkeletonEntry(void *param){
    //get the entry function and param that you need to call
    this->entry_point(entry_params);
    VMTerminate(*(this->id)); // This will allow you to gain control back if the ActualThreadEntry returns
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

TVMStatus VMThreadActivate(TVMThreadID thread) {
    // add status checking
    try {
        thread_vector.at(thread);
    }
    catch (out_of_range err) {
        VM_STATUS_ERROR_INVALID_ID;
    }
    TCB* actual_thread = thread_vector[thread];
    if (actual_thread->thread_state == VM_THREAD_STATE_DEAD) {
        VM_STATUS_ERROR_INVALID_STATE;
    }
    else {
        MachineSuspendSignals(sigstate);
        MachineContextCreate(&actual_thread.machine_context_ref, SkeletonEntry, NULL, actual_thread->stack_base, actual_thread->stack_size);
        actual_thread->thread_state = VM_THREAD_STATE_READY;
        determine_queue_and_push(thread_vector[thread]);
        scheduler();
        MachineResumeSignals(sigstate);
    }
}

TVMStatus VMThreadCreate(TVMThreadEntry entry, void *param, TVMMemorySize memsize, TVMThreadPriority prio, TVMThreadIDRef tid) {
    if (entry == NULL || tid == NULL) {
        return VM_STATUS_ERROR_INVALID_PARAMETER;
    }
    else {
        MachineSuspendSignals(sigstate);
        TCB *new_thread = new TCB(tid, VM_THREAD_STATE_DEAD, prio, memsize, entry, param, 0);
        *(new_thread->id) = (TVMThreadID)thread_vector.size();
        thread_vector.push_back(new_thread);
        MachineResumeSignals(sigstate);
        return VM_STATUS_SUCCESS;
    }
}

TVMStatus VMThreadDelete(TVMThreadID thread) {

}

TVMStatus VMThreadID(TVMThreadIDRef threadref) {

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

TVMStatus VMThreadTerminate(TVMThreadID thread) {
    current_thread->thread_state = VM_THREAD_STATE_DEAD;

}

} // end extern C
