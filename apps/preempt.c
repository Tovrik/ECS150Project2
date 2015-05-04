#include "VirtualMachine.h"

#ifndef NULL
#define NULL    ((void *)0)
#endif

void VMThread(void *param){
    volatile int Index, Val = 0;
    VMPrint("VMThread Alive\nVMThread Starting\n");
    for(Index = 0; Index < 1000000000; Index++){
        Val++;
    }
    VMPrint("VMThread Done\n");
}

void VMMain(int argc, char *argv[]){
    TVMThreadID VMThreadID;
    TVMThreadState VMState;
    volatile int Index, Val = 0;
    VMPrint("VMMain creating thread.\n");
    VMThreadCreate(VMThread, NULL, 0x100000, VM_THREAD_PRIORITY_NORMAL, &VMThreadID);
    VMPrint("VMMain getting thread state: ");
    VMThreadState(VMThreadID, &VMState);
    switch(VMState){
        case VM_THREAD_STATE_DEAD:       VMPrint("DEAD\n");
                                        break;
        case VM_THREAD_STATE_RUNNING:    VMPrint("RUNNING\n");
                                        break;
        case VM_THREAD_STATE_READY:      VMPrint("READY\n");
                                        break;
        case VM_THREAD_STATE_WAITING:    VMPrint("WAITING\n");
                                        break;
        default:                        break;
    }
    VMPrint("VMMain activating thread.\n");
    VMThreadActivate(VMThreadID);
    VMPrint("VMMain Starting\n");
    for(Index = 0; Index < 1000000000; Index++){
        Val++;
    }
    VMPrint("VMMain Done\nWaiting\n");
    do{
        VMThreadState(VMThreadID, &VMState);
    }while(VM_THREAD_STATE_DEAD != VMState);
    VMPrint("Goodbye\n");
}

