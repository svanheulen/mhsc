/*

Copyright (c) 2010-2011, Seth VanHeulen
All rights reserved.

This code is released under the BSD license.

*/

#include <pspkernel.h>
#include <pspctrl.h>
#include <string.h>

PSP_MODULE_INFO("MonsterHunterSaveConverter", PSP_MODULE_KERNEL, 0, 3);
PSP_NO_CREATE_MAIN_THREAD();

typedef struct {
    // pointer to the character data
    char* character;
    // length of the character data in memory
    unsigned int size;
    // module id of the main module of the game
    SceUID module_id;
    // name of the main module of the game
    char game_name[28];
    // name of the file to save/load character data to/from
    char file_name[50];
    // indicates if the file given in file_name exists
    int file_exists;
} game_info_t;

int control_module_threads(SceUID module_id, int pause) {
    // get an array of all threads
    int thread_count;
    if (sceKernelGetThreadmanIdList(SCE_KERNEL_TMID_Thread, NULL, 0, &thread_count) < 0)
        return 0;
    if (thread_count < 1)
        return 0;
    SceUID thread_id[thread_count];
    if (sceKernelGetThreadmanIdList(SCE_KERNEL_TMID_Thread, thread_id, thread_count, NULL) < 0)
        return 0;
    int i;
    SceModule* module;
    SceKernelThreadInfo thread_info;
    thread_info.size = sizeof(SceKernelThreadInfo);
    for (i = 0; i < thread_count; i++) {
        // determine if a thread is from the game
        if (sceKernelReferThreadStatus(thread_id[i], &thread_info))
            continue;
        module = sceKernelFindModuleByAddress((unsigned int) thread_info.entry);
        if (module && module->modid == module_id) {
            // pause or resume the thread
            if (pause)
                sceKernelSuspendThread(thread_id[i]);
            else
                sceKernelResumeThread(thread_id[i]);
        }
    }
    return 1;
}

int check_game(game_info_t* game_info) {
    // get the game's module info
    SceModule* module = NULL;
    while (module == NULL) {
        sceKernelDelayThread(1000000);
        module = sceKernelFindModuleByAddress(0x08804000);
    }
    // set the module id and game name
    game_info->module_id = module->modid;
    strcpy(game_info->game_name, module->modname);
    // get the game's global pointer
    // NOTE: the SceModule structure is incorrect, text_addr is actually the gp_value
    unsigned int module_gp = module->text_addr;
    // add the offset to the global pointer, add the offset to the character pointer,
    // set the size and file name based on the game name
    if (strcmp("MonsterHunterPSP", game_info->game_name) == 0) {
        module_gp -= 0x6e60;
        game_info->character = (char *) 648;
        game_info->size = 0x46a0;
        strcat(game_info->file_name, "mhp.bin");
    } else if (strcmp("MonsterHunterPortable2nd", game_info->game_name) == 0) {
        module_gp -= 0x7e5c;
        game_info->character = (char *) 1060;
        game_info->size = 0x13ef4;
        strcat(game_info->file_name, "mhp2.bin");
    } else if (strcmp("MonsterHunterPortable2ndG", game_info->game_name) == 0) {
        module_gp -= 0x7648;
        game_info->character = (char *) 1184;
        game_info->size = 0x6a938;
        strcat(game_info->file_name, "mhp2g.bin");
    } else if (strcmp("MonsterHunterPortable3rd", game_info->game_name) == 0) {
        module_gp += 0x88fc0;
        game_info->character = (char *) 2140;
        game_info->size = 0x5f378;
        strcat(game_info->file_name, "mhp3.bin");
    } else {
        return 0;
    }
    // wait for the global pointer to be filled
    while (*((unsigned int *) module_gp) == 0)
        sceKernelDelayThread(1000000);
    // set the character pointer
    game_info->character += *((unsigned int *) module_gp);
    // check if a save file already exists
    SceIoStat stat;
    if (sceIoGetstat(game_info->file_name, &stat) < 0)
        game_info->file_exists = 0;
    else
        game_info->file_exists = 1;
    return 1;
}

int save(game_info_t* game_info) {
    // open or create the save file
    SceUID file = sceIoOpen(game_info->file_name, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
    if (file < 0)
        return 0;
    // read character data from memory and write it into the save file
    if (sceIoWrite(file, game_info->character, game_info->size) < game_info->size) {
        sceIoClose(file);
        return 0;
    }
    sceIoClose(file);
    // mark the save file as existing
    game_info->file_exists = 1;
    return 1;
}

int load(game_info_t* game_info) {
    // open an existing save file
    SceUID file = sceIoOpen(game_info->file_name, PSP_O_RDONLY, 0777);
    if (file < 0)
        return 0;
    // read character data from the save file and write it to memory
    if (sceIoRead(file, game_info->character, game_info->size) < game_info->size) {
        sceIoClose(file);
        return 0;
    }
    sceIoClose(file);
    return 1;
}

int delete(game_info_t* game_info) {
    // delete an existing save file
    if (sceIoRemove(game_info->file_name) < 0)
        return 0;
    // mark the save file as not existing
    game_info->file_exists = 0;
    return 1;
}

void display_message(game_info_t* game_info, const char* message, unsigned int color) {
    // print some info and controls to the screen
    pspDebugScreenSetTextColor(0xffffff);
    pspDebugScreenSetXY(0, 0);
    pspDebugScreenKprintf("game: ");
    pspDebugScreenKprintf(game_info->game_name);
    pspDebugScreenSetXY(0, 1);
    pspDebugScreenKprintf("save file: ");
    pspDebugScreenKprintf(game_info->file_name);
    pspDebugScreenSetXY(0, 3);
    pspDebugScreenKprintf("o = exit, square = save");
    // print load and delete controls only if the save file exists
    if (game_info->file_exists)
        pspDebugScreenKprintf(", x = load, triangle = delete");
    else
        pspDebugScreenKprintf("                             ");
    pspDebugScreenSetXY(0, 5);
    pspDebugScreenSetTextColor(color);
    pspDebugScreenKprintf(message);
}

int main_thread(SceSize argc, void* argp) {
    game_info_t game_info;
    // get the path that the plugin was loaded from
    int path_length = strrchr((char*) argp, '/') - (char*) argp + 1;
    strncpy(game_info.file_name, argp, path_length);
    game_info.file_name[path_length] = 0;
    // check that the loaded game is supported
    if (!check_game(&game_info)) {
        sceKernelExitDeleteThread(0);
        return 0;
    }
    // setup the controller
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_DIGITAL);
    SceCtrlData pad;
    // wait for input
    int paused = 0;
    while (1) {
        sceCtrlPeekBufferPositive(&pad, 1);
        if (pad.Buttons != 0) {
            if (paused) {
                // exiting to xmb while the game is paused will cause the psp to hang
                // so resuming when the home button is pressed works around that
                if (pad.Buttons & (PSP_CTRL_CIRCLE | PSP_CTRL_HOME)) {
                    // resume the game
                    if (control_module_threads(game_info.module_id, 0))
                        paused = 0;
                    else
                        display_message(&game_info, "resume failed.  ", 0xff);
                }
                if (pad.Buttons & PSP_CTRL_SQUARE) {
                    // save character
                    display_message(&game_info, "saving...       ", 0xff0000);
                    if (save(&game_info))
                        display_message(&game_info, "save complete.  ", 0xff00);
                    else
                        display_message(&game_info, "save failed.    ", 0xff);
                }
                if (pad.Buttons & PSP_CTRL_CROSS && game_info.file_exists) {
                    // load character
                    display_message(&game_info, "loading...      ", 0xff0000);
                    if (load(&game_info))
                        display_message(&game_info, "load complete.  ", 0xff00);
                    else
                        display_message(&game_info, "load failed.    ", 0xff);
                }
                if (pad.Buttons & PSP_CTRL_TRIANGLE && game_info.file_exists) {
                    // delete save file
                    display_message(&game_info, "deleting...     ", 0xff0000);
                    if (delete(&game_info))
                        display_message(&game_info, "delete complete.", 0xff00);
                    else
                        display_message(&game_info, "delete failed.  ", 0xff);
                }
            } else {
                if (pad.Buttons & PSP_CTRL_NOTE) {
                    // pause the game
                    if (control_module_threads(game_info.module_id, 1)) {
                        paused = 1;
                        pspDebugScreenInit();
                        display_message(&game_info, "", 0);
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
    // create plugin main thread
    SceUID thid = sceKernelCreateThread("mhsc_main", main_thread, 0x30, 0x10000, 0, NULL);
    if (thid >= 0)
        sceKernelStartThread(thid, argc, argp);
    return 0;
}
