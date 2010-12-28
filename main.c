#include <pspkernel.h>
#include <pspdebug.h>
#include <pspumd.h>
#include <pspctrl.h>
#include <pspthreadman_kernel.h>
#include <string.h>

PSP_MODULE_INFO("MonsterHunterSaveConverter", PSP_MODULE_KERNEL, 0, 1);
PSP_NO_CREATE_MAIN_THREAD();

struct game_info_t {
    int offset;
    char module_name[28];
    int size;
    char file_name[50];
    int file_exists;
};

int control_module_threads(char* module_name, int paused) {
    SceUID module_id;
    SceModule* module_info = sceKernelFindModuleByName(module_name);
    if (module_info)
        module_id = module_info->modid;
    else
        return 0;
    int thread_count;
    if (sceKernelGetThreadmanIdList(SCE_KERNEL_TMID_Thread, NULL, 0, &thread_count) < 0)
        return 0;
    if (thread_count < 1)
        return 0;
    SceUID thread_id[thread_count];
    if (sceKernelGetThreadmanIdList(SCE_KERNEL_TMID_Thread, thread_id, thread_count, NULL) < 0)
        return 0;
    int i;
    SceKernelThreadInfo thread_info;
    thread_info.size = sizeof(SceKernelThreadInfo);
    for (i = 0; i < thread_count; i++) {
        if (sceKernelReferThreadStatus(thread_id[i], &thread_info))
            continue;
        module_info = sceKernelFindModuleByAddress((unsigned int) thread_info.entry);
        if (module_info && module_info->modid == module_id) {
            if (paused)
                sceKernelResumeThread(thread_id[i]);
            else
                sceKernelSuspendThread(thread_id[i]);
        }
    }
    return 1;
}

int check_game(struct game_info_t* game_info) {
    sceUmdCheckMedium();
    sceUmdActivate(1, "disc0:");
    sceUmdWaitDriveStat(UMD_WAITFORINIT);
    SceUID umd_data = sceIoOpen("disc0:/UMD_DATA.BIN", PSP_O_RDONLY, 0777);
    if (umd_data < 0)
        return 0;
    char game_id[11];
    if (sceIoRead(umd_data, game_id, 10) < 10) {
        sceIoClose(umd_data);
        return 0;
    }
    sceIoClose(umd_data);
    game_id[10] = 0;
    if (strcmp("ULJM-05500", game_id) == 0)
        game_info->offset = 0x1195e30;
    else if (strcmp("ULES-01213", game_id) == 0)
        game_info->offset = 0x119a0f0;
    else if (strcmp("ULUS-10391", game_id) == 0)
        game_info->offset = 0x119a230;
    else
        return 0;
    SceUID eboot = sceIoOpen("disc0:/PSP_GAME/SYSDIR/EBOOT.BIN", PSP_O_RDONLY, 0777);
    if (eboot < 0)
        return 0;
    sceIoLseek32(eboot, 10, PSP_SEEK_SET);
    if (sceIoRead(eboot, game_info->module_name, 28) < 28) {
        sceIoClose(eboot);
        return 0;
    }
    sceIoClose(eboot);
    if (strcmp("MonsterHunterPortable2ndG", game_info->module_name) == 0) {
        game_info->size = 0x6a880;
        strcat(game_info->file_name, "mhp2g.bin");
    } else {
        return 0;
    }
    SceIoStat stat;
    if (sceIoGetstat(game_info->file_name, &stat) < 0)
        game_info->file_exists = 0;
    else
        game_info->file_exists = 1;
    return 1;
}

int save(struct game_info_t* game_info) {
    SceUID file = sceIoOpen(game_info->file_name, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
    if (file < 0)
        return 0;
    char* mem_pointer = (char*) (0x8800000 + game_info->offset);
    if (sceIoWrite(file, mem_pointer, game_info->size) < game_info->size) {
        sceIoClose(file);
        return 0;
    }
    sceIoClose(file);
    game_info->file_exists = 1;
    return 1;
}

int load(struct game_info_t* game_info) {
    SceUID file = sceIoOpen(game_info->file_name, PSP_O_RDONLY, 0777);
    if (file < 0)
        return 0;
    char* mem_pointer = (char*) (0x8800000 + game_info->offset);
    if (sceIoRead(file, mem_pointer, game_info->size) < game_info->size) {
        sceIoClose(file);
        return 0;
    }
    sceIoClose(file);
    return 1;
}

int delete(struct game_info_t* game_info) {
    if (sceIoRemove(game_info->file_name) < 0)
        return 0;
    game_info->file_exists = 0;
    return 1;
}

void display_message(struct game_info_t* game_info, const char* message, unsigned int color) {
    pspDebugScreenSetTextColor(0x00ffffff);
    pspDebugScreenSetXY(0, 0);
    pspDebugScreenKprintf("game: ");
    pspDebugScreenKprintf(game_info->module_name);
    pspDebugScreenSetXY(0, 1);
    pspDebugScreenKprintf("save file: ");
    pspDebugScreenKprintf(game_info->file_name);
    pspDebugScreenSetXY(0, 3);
    pspDebugScreenKprintf("o = exit, square = save");
    if (game_info->file_exists)
        pspDebugScreenKprintf(", x = load, triangle = delete");
    else
        pspDebugScreenKprintf("                             ");
    pspDebugScreenSetXY(0, 5);
    pspDebugScreenSetTextColor(color);
    pspDebugScreenKprintf(message);
}

int main_thread(SceSize argc, void* argp) {
    struct game_info_t game_info;
    int path_length = strrchr((char*) argp, '/') - (char*) argp + 1;
    strncpy(game_info.file_name, argp, path_length);
    game_info.file_name[path_length] = 0;
    if (!check_game(&game_info)) {
        sceKernelExitDeleteThread(0);
        return 0;
    }
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_DIGITAL);
    SceCtrlData pad;
    int paused = 0;
    while (1) {
        sceCtrlPeekBufferPositive(&pad, 1);
        if (pad.Buttons != 0) {
            if (paused) {
                if (pad.Buttons & PSP_CTRL_CIRCLE) {
                    if (control_module_threads(game_info.module_name, 1))
                        paused = 0;
                    else
                        display_message(&game_info, "resume failed.  ", 0x000000ff);
                }
                if (pad.Buttons & PSP_CTRL_SQUARE) {
                    display_message(&game_info, "saving...       ", 0x00ff0000);
                    if (save(&game_info))
                        display_message(&game_info, "save complete.  ", 0x0000ff00);
                    else
                        display_message(&game_info, "save failed.    ", 0x000000ff);
                }
                if (pad.Buttons & PSP_CTRL_CROSS && game_info.file_exists) {
                    display_message(&game_info, "loading...      ", 0x00ff0000);
                    if (load(&game_info))
                        display_message(&game_info, "load complete.  ", 0x0000ff00);
                    else
                        display_message(&game_info, "load failed.    ", 0x000000ff);
                }
                if (pad.Buttons & PSP_CTRL_TRIANGLE && game_info.file_exists) {
                    display_message(&game_info, "deleting...     ", 0x00ff0000);
                    if (delete(&game_info))
                        display_message(&game_info, "delete complete.", 0x0000ff00);
                    else
                        display_message(&game_info, "delete failed.  ", 0x000000ff);
                }
            } else {
                if (pad.Buttons & PSP_CTRL_NOTE) {
                    if (control_module_threads(game_info.module_name, 0)) {
                        paused = 1;
                        pspDebugScreenInit();
                        display_message(&game_info, "", 0x00ffffff);
                    }
                }
            }
        }
        sceKernelDelayThread(100000);
    }
    sceKernelExitDeleteThread(0);
    return 0;
}

int module_start(SceSize argc, void* argp) {
    SceUID thid = sceKernelCreateThread("mhsc_main", main_thread, 0x30, 0x10000, 0, NULL);
    if (thid >= 0)
        sceKernelStartThread(thid, argc, argp);
    return 0;
}
