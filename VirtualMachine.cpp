#include <stdio.h>
#include <stdlib.h>
#include "VirtualMachine.h"
#include "Machine.h"
#include <vector>
#include <deque>

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

    TVMThreadIDRef id;
    TVMThreadState thread_state;
    TVMThreadPriority thread_priority;
    TVMMemorySize stack_size;
    uint8_t *stack_base;
    TVMThreadEntry entry_point;
    void *entry_params;
    TVMTick ticks_remaining;
    SMachineContext machine_context; // for the context to switch to/from the thread
    int call_back_result;
// Possibly need something to hold file return type
// Possibly hold a pointer or ID of mutex waiting on
// Possibly hold a list of held mutexes
};

///////////////////////// Globals ///////////////////////////
#define VM_THREAD_PRIORITY_IDLE                  ((TVMThreadPriority)0x00)

vector<TCB*> thread_vector;
deque<TCB*> low_priority_queue;
deque<TCB*> normal_priority_queue;
deque<TCB*> high_priority_queue;
vector<TCB*> sleep_vector;
TCB*        idle_thread;
TCB*        current_thread;

volatile int timer;
TMachineSignalStateRef sigstate;

///////////////////////// Function Prototypes ///////////////////////////
TVMMainEntry VMLoadModule(const char *module);

///////////////////////// Utilities ///////////////////////////
void actual_removal(TCB* thread, deque<TCB*> &Q) {
    for (deque<TCB*>::iterator it=Q.begin(); it != Q.end(); ++it) {
        if (*it == thread) {
            Q.erase(it);
            break;
        }
    }
}

void determine_queue_and_push(TCB* thread) {
    if (thread->thread_priority == VM_THREAD_PRIORITY_LOW) {
        low_priority_queue.push_back(thread);
    }
    else if (thread->thread_priority == VM_THREAD_PRIORITY_NORMAL) {
        normal_priority_queue.push_back(thread);
    }
    else if (thread->thread_priority == VM_THREAD_PRIORITY_HIGH) {
        high_priority_queue.push_back(thread);
    }
}

void determine_queue_and_remove(TCB *thread) {
    if (thread->thread_priority == VM_THREAD_PRIORITY_LOW) {
        actual_removal(thread, low_priority_queue);
    }
    else if (thread->thread_priority == VM_THREAD_PRIORITY_NORMAL) {
        actual_removal(thread, normal_priority_queue);
    }
    else if (thread->thread_priority == VM_THREAD_PRIORITY_HIGH) {
        actual_removal(thread, high_priority_queue);
    }
}

void scheduler_action(deque<TCB*> &Q) {
    // set current thread to ready state if it was running
    if (current_thread->thread_state == VM_THREAD_STATE_RUNNING) {
        current_thread->thread_state = VM_THREAD_STATE_READY;
    }
    if (current_thread->thread_state == VM_THREAD_STATE_READY) {
        determine_queue_and_push(current_thread);
    }
    else if (current_thread->thread_state == VM_THREAD_STATE_WAITING) {
        sleep_vector.push_back(current_thread);
    }
    TCB* temp = current_thread;
    // set current to next and remove from Q
    current_thread = Q.front();
    Q.pop_front();
    // set current to running
    current_thread->thread_state = VM_THREAD_STATE_RUNNING;
    // MachineContextSwitch(mcntxold,mcntxnew), old = current, new = next = Q.front()
    // VMPrint("switching context\n");
    MachineContextSwitch(&(temp->machine_context), &(current_thread->machine_context));
}

void scheduler() {
    if (!high_priority_queue.empty()) {
        // VMPrint("high\n");
        scheduler_action(high_priority_queue);
    }
    else if (!normal_priority_queue.empty()) {
        // VMPrint("normal\n");
        scheduler_action(normal_priority_queue);
    }
    else if (!low_priority_queue.empty()) {
        // VMPrint("low\n");
        scheduler_action(low_priority_queue);
    }
    // schedule idle thread
    else {
        // VMPrint("idle\n");
        if (current_thread->thread_state == VM_THREAD_STATE_READY) {
            determine_queue_and_push(current_thread);
        }
        else if (current_thread->thread_state == VM_THREAD_STATE_WAITING) {
            sleep_vector.push_back(current_thread);
        }
        TCB* temp = current_thread;
        current_thread = idle_thread;
        current_thread->thread_state = VM_THREAD_STATE_RUNNING;
        thread_vector.push_back(current_thread);
        MachineContextSwitch(&(temp->machine_context), &(idle_thread->machine_context));
    }
}

///////////////////////// Callbacks ///////////////////////////
// for ref: typedef void (*TMachineAlarmCallback)(void *calldata);
void timerDecrement(void *calldata) {
    // timer -= 1;
    // decrements ticks for each sleeping thread
    for (int i = 0; i < sleep_vector.size(); i++) {
        // if (sleep_vector[i]->ticks_remaining > 0) {
            sleep_vector[i]->ticks_remaining--;
            // VMPrint("%d\n",sleep_vector[i]->ticks_remaining);
            if (sleep_vector[i]->ticks_remaining ==  0) {
                sleep_vector[i]->thread_state = VM_THREAD_STATE_READY;
                determine_queue_and_push(sleep_vector[i]);
                sleep_vector.erase(sleep_vector.begin() + i);
                scheduler();
            }
        // }
    }
}

void SkeletonEntry(void *param) {
    MachineEnableSignals();
    TCB* temp = (TCB*)param;
    temp->entry_point(temp->entry_params);
    VMThreadTerminate(*(temp->id)); // This will allow you to gain control back if the ActualThreadEntry returns
}

void idleEntry(void *param) {
    while(1);
}

void MachineCallback(void* param, int result) {
    TCB* temp = (TCB*) param;
    temp->thread_state = VM_THREAD_STATE_READY;
    determine_queue_and_push(temp);
    temp->call_back_result = result;
    scheduler();
}

///////////////////////// VMThread Functions ///////////////////////////
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
        // create main_thread
        TCB* main_thread = new TCB((unsigned int *)0, VM_THREAD_STATE_RUNNING, VM_THREAD_PRIORITY_NORMAL, 0, NULL, NULL, 0);
        thread_vector.push_back(main_thread);
        current_thread = main_thread;
        // create idle_thread
        idle_thread = new TCB((unsigned int *)1, VM_THREAD_STATE_DEAD, VM_THREAD_PRIORITY_IDLE, 0x100000, NULL, NULL, 0);
        idle_thread->thread_state = VM_THREAD_STATE_READY;
        MachineContextCreate(&(idle_thread->machine_context), idleEntry, NULL, idle_thread->stack_base, idle_thread->stack_size);
        // thread_vector.push_back(idle_thread);
        VMMain(argc, argv);
        return VM_STATUS_SUCCESS;
    }
    else {
        return VM_STATUS_FAILURE;
    }
}

TVMStatus VMThreadActivate(TVMThreadID thread) {
    MachineSuspendSignals(sigstate);
    TCB* actual_thread = thread_vector[thread];
    if (actual_thread->thread_state != VM_THREAD_STATE_DEAD) {
        MachineResumeSignals(sigstate);
        return VM_STATUS_ERROR_INVALID_STATE;
    }
    else {
        //void MachineContextCreate(SMachineContextRef mcntxref, void (*entry)(void *), void *param, void *stackaddr, size_t stacksize);
        MachineContextCreate(&(actual_thread->machine_context), SkeletonEntry, actual_thread, actual_thread->stack_base, actual_thread->stack_size);
        actual_thread->thread_state = VM_THREAD_STATE_READY;
        determine_queue_and_push(actual_thread);
        // scheduler();
        MachineResumeSignals(sigstate);
        return VM_STATUS_SUCCESS;
    }
}

TVMStatus VMThreadCreate(TVMThreadEntry entry, void *param, TVMMemorySize memsize, TVMThreadPriority prio, TVMThreadIDRef tid) {
    MachineSuspendSignals(sigstate);
    if (entry == NULL || tid == NULL) {
        MachineResumeSignals(sigstate);
        return VM_STATUS_ERROR_INVALID_PARAMETER;
    }

    else {
        TCB *new_thread = new TCB(tid, VM_THREAD_STATE_DEAD, prio, memsize, entry, param, 0);
        *(new_thread->id) = (TVMThreadID)thread_vector.size();
        thread_vector.push_back(new_thread);
        MachineResumeSignals(sigstate);
        return VM_STATUS_SUCCESS;
    }
}

TVMStatus VMThreadDelete(TVMThreadID thread) {
    MachineSuspendSignals(sigstate);
    if (thread_vector[thread]->thread_state == VM_THREAD_STATE_DEAD) {
        MachineResumeSignals(sigstate);
        return VM_STATUS_ERROR_INVALID_STATE;
    }
    else {
        delete thread_vector[thread];
        thread_vector[thread] = NULL;
        MachineResumeSignals(sigstate);
        return VM_STATUS_SUCCESS;
    }
}

TVMStatus VMThreadID(TVMThreadIDRef threadref) {
    MachineSuspendSignals(sigstate);
    if (threadref == NULL) {
        MachineResumeSignals(sigstate);
        return VM_STATUS_ERROR_INVALID_PARAMETER;
    }
    else {
        threadref = current_thread->id;
        MachineResumeSignals(sigstate);
        return VM_STATUS_SUCCESS;
    }
}

// if tick == VM_TIMEOUT_INFINITE the current thread yields to the next ready thread. It basically
// goes to the end of its ready queue. The processing quantum is the amount of time that each thread
// (or process) gets for its time slice. You can assume it is one tick.
TVMStatus VMThreadSleep(TVMTick tick){
    // MachineSuspendSignals(sigstate);
    if (tick == VM_TIMEOUT_INFINITE) {
        // MachineResumeSignals(sigstate);
        return VM_STATUS_ERROR_INVALID_PARAMETER;
    }
    else {
        current_thread->ticks_remaining = tick;
        current_thread->thread_state = VM_THREAD_STATE_WAITING;
        // determine_queue_and_push();
        scheduler();
        return VM_STATUS_SUCCESS;
    }
}

TVMStatus VMThreadState(TVMThreadID thread, TVMThreadStateRef stateref) {
    if(thread == VM_THREAD_ID_INVALID) {
        return VM_STATUS_ERROR_INVALID_ID;
    }
    if (stateref == NULL) {
        return VM_STATUS_ERROR_INVALID_PARAMETER;
    }
    else {
        *stateref = thread_vector[thread]->thread_state;
        return VM_STATUS_SUCCESS;
    }
}

TVMStatus VMThreadTerminate(TVMThreadID thread) {
    if (thread_vector[thread]->thread_state == VM_THREAD_STATE_DEAD) {
        return VM_STATUS_ERROR_INVALID_STATE;
    }
    else {
        thread_vector[thread]->thread_state = VM_THREAD_STATE_DEAD;
        determine_queue_and_remove(thread_vector[thread]);
        scheduler();
        return VM_STATUS_SUCCESS;
    }
}

///////////////////////// VMFile Functions ///////////////////////////
// TVMStatus VMFileClose(int filedescriptor) {
//     current_thread->thread_state = VM_THREAD_STATE_WAITING;
//     TVMThreadIDRef close_thread = current_thread->id;
//     MachineFileClose(filedescriptor, MachineCallback, current_thread);
//     scheduler();

//     if (thread_vector[*(close_thread)]->call_back_result != -1) {
//         return VM_STATUS_SUCCESS;
//     }
//     else {
//         return VM_STATUS_FAILURE;
//     }
// }


// // void MachineFileOpen(const char *filename, int flags, int mode, TMachineFileCallback callback, void *calldata);

// TVMStatus VMFileOpen(const char *filename, int flags, int mode, int *filedescriptor) {
//     if (filename == NULL || filedescriptor == NULL) {
//         return VM_STATUS_ERROR_INVALID_PARAMETER;
//     }
//     else {
//         VMPrint("begin\n");
//         current_thread->thread_state = VM_THREAD_STATE_WAITING;
//         TVMThreadIDRef open_thread = current_thread->id;
//         VMPrint("preopen\n");
//         MachineFileOpen(filename, flags, mode, MachineCallback, open_thread);
//         VMPrint("postopen\n");
//         scheduler();
//         *filedescriptor = thread_vector[*(open_thread)]->call_back_result;
//         if (*filedescriptor != -1) {
//             return VM_STATUS_SUCCESS;
//         }
//         else {
//             return VM_STATUS_FAILURE;
//         }
//     }
// }


// TVMStatus VMFileWrite(int filedescriptor, void *data, int *length) {
//     if (data == NULL || length == NULL) {
//         return VM_STATUS_ERROR_INVALID_PARAMETER;
//     }
//     current_thread->thread_state = VM_THREAD_STATE_WAITING;
//     TVMThreadIDRef write_thread = current_thread->id;
//     MachineFileWrite(filedescriptor, data, *length, MachineCallback, write_thread);
//     scheduler();
//     if(thread_vector[*(write_thread)]->call_back_result != -1) {
//         return VM_STATUS_SUCCESS;
//     }
//     else {
//         return VM_STATUS_FAILURE;
//     }
// }

// TVMStatus VMFileRead(int filedescriptor, void *data, int *length) {
//     if (data == NULL || length == NULL) {
//         return VM_STATUS_ERROR_INVALID_PARAMETER;
//     }
//     current_thread->thread_state = VM_THREAD_STATE_WAITING;
//     TVMThreadIDRef read_thread = current_thread->id;
//     MachineFileRead(filedescriptor, data, *length, MachineCallback, read_thread);
//     scheduler();
//     *length = thread_vector[*(read_thread)]->call_back_result;
//     if(*length > 0) {
//         return VM_STATUS_SUCCESS;
//     }
//     else {
//         return VM_STATUS_FAILURE;
//     }
// }

// TVMStatus VMFileSeek(int filedescriptor, int offset, int whence, int *newoffset) {
//     current_thread->thread_state = VM_THREAD_STATE_WAITING;
//     TVMThreadIDRef seek_thread = current_thread->id;
//     MachineFileSeek(filedescriptor, offset, whence, MachineCallback, seek_thread);
//     scheduler();
//     if(newoffset != NULL) {
//         *newoffset = thread_vector[*(seek_thread)]->call_back_result;
//         return VM_STATUS_SUCCESS;
//     }
//     else {
//         return VM_STATUS_FAILURE;
//     }

// }

} // end extern C
