#include <vitasdkkern.h>
#include <psp2/touch.h>
#include <taihen.h>

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

int  ksceTouchSetSamplingState(SceUInt32 port, SceTouchSamplingState pState);
int  ksceTouchPeek(SceUInt32 port, SceTouchData *pData, SceUInt32 nBufs);
bool ksceAppMgrIsExclusiveProcessRunning();

static SceUID touch_hook;
static tai_hook_ref_t touch_ref;

int ksceTouchPeekRegion_patched(SceUInt32 port, SceTouchData *pData, SceUInt32 nBufs, int region) {
    int ret = TAI_CONTINUE(int, touch_ref, port, pData, nBufs, region);
    
    // Only patch touch data if livearea or a system app is running
    if(ret >= 0 && !ksceAppMgrIsExclusiveProcessRunning()) {
        if(port == SCE_TOUCH_PORT_FRONT || port == SCE_TOUCH_PORT_BACK) {
            ksceTouchSetSamplingState(SCE_TOUCH_PORT_FRONT, SCE_TOUCH_SAMPLING_STATE_START);
            ksceTouchSetSamplingState(SCE_TOUCH_PORT_BACK, SCE_TOUCH_SAMPLING_STATE_START);
            
            SceTouchData fake_data[SCE_TOUCH_PORT_MAX_NUM];
            memset(fake_data, 0, sizeof(fake_data));
            
            for(int port = 0; port < SCE_TOUCH_PORT_MAX_NUM; port++)
                ksceTouchPeek(port, &fake_data[port], 1);
            
            for(int i = 0; i < nBufs; i++) {
                unsigned int num_reports = 0;
                
                if(port == SCE_TOUCH_PORT_FRONT) {
                    // Overwrite intercepted front touch readings with any back panel touch readings
                    if(fake_data[SCE_TOUCH_PORT_BACK].reportNum > 0) {
                        pData->report[0].id = fake_data[SCE_TOUCH_PORT_BACK].report[0].id;
                        pData->report[0].x  = fake_data[SCE_TOUCH_PORT_BACK].report[0].x;
                        pData->report[0].y  = fake_data[SCE_TOUCH_PORT_BACK].report[0].y * 1.28;
                        num_reports++;
                        
                        pData->report[1].id = fake_data[SCE_TOUCH_PORT_BACK].report[1].id;
                        pData->report[1].x  = fake_data[SCE_TOUCH_PORT_BACK].report[1].x;
                        pData->report[1].y  = fake_data[SCE_TOUCH_PORT_BACK].report[1].y * 1.28;
                        num_reports++;
                    }
                }
                
                if(num_reports > 0)
                    pData->reportNum = num_reports;
                
                pData++;
            }
        }
    }
    
    return ret;
}

void _start() __attribute__ ((weak, alias ("module_start")));
int module_start(SceSize argc, const void *args) {
    touch_hook = taiHookFunctionExportForKernel(KERNEL_PID, &touch_ref, "SceTouch", TAI_ANY_LIBRARY, 0x9B3F7207, ksceTouchPeekRegion_patched);
    
    return SCE_KERNEL_START_SUCCESS;
}

int module_stop(SceSize argc, const void *args) {
    taiHookReleaseForKernel(touch_hook, touch_ref);
    
    return SCE_KERNEL_STOP_SUCCESS;
}
