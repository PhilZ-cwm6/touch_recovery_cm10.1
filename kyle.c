#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <limits.h>
#include <linux/input.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/reboot.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <sys/wait.h>
#include <sys/limits.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/statfs.h>

#include <signal.h>
#include <sys/wait.h>

#include "bootloader.h"
#include "common.h"
#include "cutils/properties.h"
#include "firmware.h"
#include "install.h"
#include "make_ext4fs.h"
#include "minui/minui.h"
#include "minzip/DirUtil.h"
#include "roots.h"
#include "recovery_ui.h"

#include "extendedcommands.h"
#include "nandroid.h"
#include "mounts.h"
#include "flashutils/flashutils.h"
#include "edify/expr.h"
#include <libgen.h>
#include "mtdutils/mtdutils.h"
#include "bmlutils/bmlutils.h"
#include "cutils/android_reboot.h"
#include "kyle.h"
#include "recovery.h"

// # *** Thanks to PhilZ for all of this! *** #
// He has been such a HUGE part of where this recovery has ended up
void ui_print_custom_logtail(const char* filename, int nb_lines) {
    char * backup_log;
    char tmp[PATH_MAX];
    FILE * f;
    int line=0;
    sprintf(tmp, "tail -n %d %s > /tmp/custom_tail.log", nb_lines, filename);
    __system(tmp);
    f = fopen("/tmp/custom_tail.log", "rb");
    if (f != NULL) {
        while (line < nb_lines) {
            backup_log = fgets(tmp, PATH_MAX, f);
            if (backup_log == NULL) break;
            ui_print("%s", tmp);
            line++;
        }
        fclose(f);
    }
}

  //start show flash kernel menu (flash/restore from default location)
void flash_kernel_default (const char* kernel_path) {
    static char* headers[] = { "Flash kernel image",
                                NULL
    };
    if (ensure_path_mounted(kernel_path) != 0) {
        LOGE ("Can't mount %s\n", kernel_path);
        return;
    }
    char tmp[PATH_MAX];
    sprintf(tmp, "%s/clockworkmod/.kernel_bak/", kernel_path);
    //without this check, we get 2 errors in log: "directory not found":
    if (access(tmp, F_OK) != -1) {
        //folder exists, but could be empty!
        char* kernel_file = choose_file_menu(tmp, ".img", headers);
        if (kernel_file == NULL) {
            //either no valid files found or we selected no files by pressing back menu
            if (no_files_found == 1) {
                //0 valid files to select
                ui_print("No *.img files in %s\n", tmp);
            }
            return;
        }
        static char* confirm_install = "Confirm flash kernel?";
        static char confirm[PATH_MAX];
        sprintf(confirm, "Yes - Flash %s", basename(kernel_file));
        if (confirm_selection(confirm_install, confirm)) {
            char tmp[PATH_MAX];
            sprintf(tmp, "kernel-restore.sh %s %s", kernel_file, kernel_path);
            __system(tmp);
            //prints log
            char logname[PATH_MAX];
            sprintf(logname, "%s/clockworkmod/.kernel_bak/log.txt", kernel_path);
            ui_print_custom_logtail(logname, 3);
        }
    } else {
        ui_print("%s not found.\n", tmp);
        return;
    }
}

  //start show efs partition backup/restore menu
void show_efs_menu() {
    static char* headers[] = { "EFS/Boot Backup & Restore",
                                "",
                                NULL
    };

    static char* list[] = { "Backup /boot to sdcard",
                     "Flash /boot from sdcard",
                     "Backup /efs to sdcard",
                     "Restore /efs from sdcard",
                     NULL,
                     NULL,
                     NULL,
                     NULL,
                     NULL
    };

    char *other_sd = NULL;
    if (volume_for_path("/emmc") != NULL) {
        other_sd = "/emmc";
        list[4] = "Backup /boot to Internal sdcard";
        list[5] = "Flash /boot from Internal sdcard";
        list[6] = "Backup /efs to Internal sdcard";
        list[7] = "Restore /efs from Internal sdcard";
    } else if (volume_for_path("/external_sd") != NULL) {
        other_sd = "/external_sd";
        list[4] = "Backup /boot to External sdcard";
        list[5] = "Flash /boot from External sdcard";
        list[6] = "Backup /efs to External sdcard";
        list[7] = "Restore /efs from External sdcard";
    }

    for (;;) {
        //header function so that "Toggle menu" doesn't reset to main menu on action selected
        int chosen_item = get_filtered_menu_selection(headers, list, 0, 0, sizeof(list) / sizeof(char*));
        if (chosen_item == GO_BACK)
            break;
        switch (chosen_item)
        {
            case 0:
                if (ensure_path_mounted("/sdcard") != 0) {
                    ui_print("Can't mount /sdcard\n");
                    break;
                }
                __system("kernel-backup.sh /sdcard");
                ui_print_custom_logtail("/sdcard/clockworkmod/.kernel_bak/log.txt", 3);
                break;
            case 1:
                flash_kernel_default("/sdcard");
                break;
            case 2:
                if (ensure_path_mounted("/sdcard") != 0) {
                    ui_print("Can't mount /sdcard\n");
                    break;
                }
                ensure_path_unmounted("/efs");
                __system("backup-efs.sh /sdcard");
                ui_print_custom_logtail("/sdcard/clockworkmod/.efsbackup/log.txt", 3);
                break;
            case 3:
                if (ensure_path_mounted("/sdcard") != 0) {
                    ui_print("Can't mount /sdcard\n");
                    break;
                }
                ensure_path_unmounted("/efs");
                if (access("/sdcard/clockworkmod/.efsbackup/efs.img", F_OK ) != -1) {
                    if (confirm_selection("Confirm?", "Yes - Restore /efs")) {
                        __system("restore-efs.sh /sdcard");
                        ui_print_custom_logtail("/sdcard/clockworkmod/.efsbackup/log.txt", 3);
                    }
                } else {
                    ui_print("No efs.img backup found in sdcard.\n");
                }
                break;
            case 4:
                {
                    if (ensure_path_mounted(other_sd) != 0) {
                        ui_print("Can't mount %s\n", other_sd);
                        break;
                    }
                    char tmp[PATH_MAX];
                    sprintf(tmp, "kernel-backup.sh %s", other_sd);
                    __system(tmp);
                    //prints log
                    char logname[PATH_MAX];
                    sprintf(logname, "%s/clockworkmod/.kernel_bak/log.txt", other_sd);
                    ui_print_custom_logtail(logname, 3);
                }
                break;
            case 5:
                flash_kernel_default(other_sd);
                break;
            case 6:
                {
                    if (ensure_path_mounted(other_sd) != 0) {
                        ui_print("Can't mount %s\n", other_sd);
                        break;
                    }
                    ensure_path_unmounted("/efs");
                    char tmp[PATH_MAX];
                    sprintf(tmp, "backup-efs.sh %s", other_sd);
                    __system(tmp);
                    //prints log
                    char logname[PATH_MAX];
                    sprintf(logname, "%s/clockworkmod/.efsbackup/log.txt", other_sd);
                    ui_print_custom_logtail(logname, 3);
                }
                break;
            case 7:
                {
                    if (ensure_path_mounted(other_sd) != 0) {
                        ui_print("Can't mount %s\n", other_sd);
                        break;
                    }
                    ensure_path_unmounted("/efs");
                    char filename[PATH_MAX];
                    sprintf(filename, "%s/clockworkmod/.efsbackup/efs.img", other_sd);
                    if (access(filename, F_OK ) != -1) {
                        if (confirm_selection("Confirm?", "Yes - Restore /efs")) {
                            char tmp[PATH_MAX];
                            sprintf(tmp, "restore-efs.sh %s", other_sd);
                            __system(tmp);
                            //prints log
                            char logname[PATH_MAX];
                            sprintf(logname, "%s/clockworkmod/.efsbackup/log.txt", other_sd);
                            ui_print_custom_logtail(logname, 3);
                        }
                    } else {
                        ui_print("No efs.img backup found in %s\n", other_sd);
                    }
                }
                break;
        }
    }
}

  //start show partition backup/restore menu
void show_misc_menu() {
    static char* headers[] = { "Misc/Boot Backup & Restore",
                                "",
                                NULL
    };

    static char* list[] = { "Backup /boot to sdcard",
                     "Flash /boot from sdcard",
                     "Backup /misc to sdcard",
                     "Restore /misc from sdcard",
                     NULL,
                     NULL,
                     NULL,
                     NULL,
                     NULL
    };

    char *other_sd = NULL;
    if (volume_for_path("/emmc") != NULL) {
        other_sd = "/emmc";
        list[4] = "Backup /boot to Internal sdcard";
        list[5] = "Flash /boot from Internal sdcard";
        list[6] = "Backup /misc to Internal sdcard";
        list[7] = "Restore /misc from Internal sdcard";
    } else if (volume_for_path("/external_sd") != NULL) {
        other_sd = "/external_sd";
        list[4] = "Backup /boot to External sdcard";
        list[5] = "Flash /boot from External sdcard";
        list[6] = "Backup /misc to External sdcard";
        list[7] = "Restore /misc from External sdcard";
    }

    for (;;) {
        //header function so that "Toggle menu" doesn't reset to main menu on action selected
        int chosen_item = get_filtered_menu_selection(headers, list, 0, 0, sizeof(list) / sizeof(char*));
        if (chosen_item == GO_BACK)
            break;
        switch (chosen_item)
        {
            case 0:
                if (ensure_path_mounted("/sdcard") != 0) {
                    ui_print("Can't mount /sdcard\n");
                    break;
                }
                __system("kernel-backup.sh /sdcard");
                ui_print_custom_logtail("/sdcard/clockworkmod/.kernel_bak/log.txt", 3);
                break;
            case 1:
                flash_kernel_default("/sdcard");
                break;
            case 2:
                if (ensure_path_mounted("/sdcard") != 0) {
                    ui_print("Can't mount /sdcard\n");
                    break;
                }
                ensure_path_unmounted("/misc");
                __system("backup-misc.sh /sdcard");
                ui_print_custom_logtail("/sdcard/clockworkmod/.miscbackup/log.txt", 3);
                break;
            case 3:
                if (ensure_path_mounted("/sdcard") != 0) {
                    ui_print("Can't mount /sdcard\n");
                    break;
                }
                ensure_path_unmounted("/misc");
                if (access("/sdcard/clockworkmod/.miscbackup/misc.img", F_OK ) != -1) {
                    if (confirm_selection("Confirm?", "Yes - Restore /misc")) {
                        __system("restore-misc.sh /sdcard");
                        ui_print_custom_logtail("/sdcard/clockworkmod/.miscbackup/log.txt", 3);
                    }
                } else {
                    ui_print("No misc.img backup found in sdcard.\n");
                }
                break;
            case 4:
                {
                    if (ensure_path_mounted(other_sd) != 0) {
                        ui_print("Can't mount %s\n", other_sd);
                        break;
                    }
                    char tmp[PATH_MAX];
                    sprintf(tmp, "kernel-backup.sh %s", other_sd);
                    __system(tmp);
                    //prints log
                    char logname[PATH_MAX];
                    sprintf(logname, "%s/clockworkmod/.kernel_bak/log.txt", other_sd);
                    ui_print_custom_logtail(logname, 3);
                }
                break;
            case 5:
                flash_kernel_default(other_sd);
                break;
            case 6:
                {
                    if (ensure_path_mounted(other_sd) != 0) {
                        ui_print("Can't mount %s\n", other_sd);
                        break;
                    }
                    ensure_path_unmounted("/misc");
                    char tmp[PATH_MAX];
                    sprintf(tmp, "backup-misc.sh %s", other_sd);
                    __system(tmp);
                    //prints log
                    char logname[PATH_MAX];
                    sprintf(logname, "%s/clockworkmod/.miscbackup/log.txt", other_sd);
                    ui_print_custom_logtail(logname, 3);
                }
                break;
            case 7:
                {
                    if (ensure_path_mounted(other_sd) != 0) {
                        ui_print("Can't mount %s\n", other_sd);
                        break;
                    }
                    ensure_path_unmounted("/misc");
                    char filename[PATH_MAX];
                    sprintf(filename, "%s/clockworkmod/.miscbackup/misc.img", other_sd);
                    if (access(filename, F_OK ) != -1) {
                        if (confirm_selection("Confirm?", "Yes - Restore /misc")) {
                            char tmp[PATH_MAX];
                            sprintf(tmp, "restore-misc.sh %s", other_sd);
                            __system(tmp);
                            //prints log
                            char logname[PATH_MAX];
                            sprintf(logname, "%s/clockworkmod/.miscbackup/log.txt", other_sd);
                            ui_print_custom_logtail(logname, 3);
                        }
                    } else {
                        ui_print("No misc.img backup found in %s\n", other_sd);
                    }
                }
                break;
        }
    }
}

int create_customzip(const char* custompath)
{
    char command[PATH_MAX];
    sprintf(command, "create_update_zip.sh %s", custompath);
    __system(command);
    return 0;
}

#define SCRIPT_COMMAND_SIZE 512

int run_custom_ors(const char* ors_script) {
	FILE *fp = fopen(ors_script, "r");
	int ret_val = 0, cindex, line_len, i, remove_nl;
	char script_line[SCRIPT_COMMAND_SIZE], command[SCRIPT_COMMAND_SIZE],
		 value[SCRIPT_COMMAND_SIZE], mount[SCRIPT_COMMAND_SIZE],
		 value1[SCRIPT_COMMAND_SIZE], value2[SCRIPT_COMMAND_SIZE];
	char *val_start, *tok;
	int ors_system = 0;
	int ors_data = 0;
	int ors_cache = 0;
	int ors_recovery = 0;
	int ors_boot = 0;
	int ors_andsec = 0;
	int ors_sdext = 0;

	if (fp != NULL) {
		while (fgets(script_line, SCRIPT_COMMAND_SIZE, fp) != NULL && ret_val == 0) {
			cindex = 0;
			line_len = strlen(script_line);
			//if (line_len > 2)
				//continue; // there's a blank line at the end of the file, we're done!
			ui_print("script line: '%s'\n", script_line);
			for (i=0; i<line_len; i++) {
				if ((int)script_line[i] == 32) {
					cindex = i;
					i = line_len;
				}
			}
			memset(command, 0, sizeof(command));
			memset(value, 0, sizeof(value));
			if ((int)script_line[line_len - 1] == 10)
					remove_nl = 2;
				else
					remove_nl = 1;
			if (cindex != 0) {
				strncpy(command, script_line, cindex);
				ui_print("command is: '%s' and ", command);
				val_start = script_line;
				val_start += cindex + 1;
				strncpy(value, val_start, line_len - cindex - remove_nl);
				ui_print("value is: '%s'\n", value);
			} else {
				strncpy(command, script_line, line_len - remove_nl + 1);
				ui_print("command is: '%s' and there is no value\n", command);
			}
			if (strcmp(command, "install") == 0) {
				// Install zip
				ui_print("Installing zip file '%s'\n", value);
				ret_val = install_zip(value);
				if (ret_val != INSTALL_SUCCESS) {
					LOGE("Error installing zip file '%s'\n", value);
					ret_val = 1;
				}
			} else if (strcmp(command, "wipe") == 0) {
				// Wipe
				if (strcmp(value, "cache") == 0 || strcmp(value, "/cache") == 0) {
					ui_print("-- Wiping Cache Partition...\n");
					erase_volume("/cache");
					ui_print("-- Cache Partition Wipe Complete!\n");
				} else if (strcmp(value, "dalvik") == 0 || strcmp(value, "dalvick") == 0 || strcmp(value, "dalvikcache") == 0 || strcmp(value, "dalvickcache") == 0) {
					ui_print("-- Wiping Dalvik Cache...\n");
					if (0 != ensure_path_mounted("/data")) {
						ret_val = 1;
						break;
					}
					ensure_path_mounted("/sd-ext");
					ensure_path_mounted("/cache");
					if (confirm_selection( "Confirm wipe?", "Yes - Wipe Dalvik Cache")) {
						__system("rm -r /data/dalvik-cache");
						__system("rm -r /cache/dalvik-cache");
						__system("rm -r /sd-ext/dalvik-cache");
						ui_print("Dalvik Cache wiped.\n");
					}
					ensure_path_unmounted("/data");

					ui_print("-- Dalvik Cache Wipe Complete!\n");
				} else if (strcmp(value, "data") == 0 || strcmp(value, "/data") == 0 || strcmp(value, "factory") == 0 || strcmp(value, "factoryreset") == 0) {
					ui_print("-- Wiping Data Partition...\n");
					wipe_data(0);
					ui_print("-- Data Partition Wipe Complete!\n");
				} else {
					LOGE("Error with wipe command value: '%s'\n", value);
					ret_val = 1;
				}
			} else if (strcmp(command, "backup") == 0) {
				// Backup
				char backup_path[PATH_MAX];

				tok = strtok(value, " ");
				strcpy(value1, tok);
				tok = strtok(NULL, " ");
				if (tok != NULL) {
					memset(value2, 0, sizeof(value2));
					strcpy(value2, tok);
					line_len = strlen(tok);
					if ((int)value2[line_len - 1] == 10 || (int)value2[line_len - 1] == 13) {
						if ((int)value2[line_len - 1] == 10 || (int)value2[line_len - 1] == 13)
							remove_nl = 2;
						else
							remove_nl = 1;
					} else
						remove_nl = 0;
					strncpy(value2, tok, line_len - remove_nl);
					ui_print("Backup folder set to '%s'\n", value2);
					sprintf(backup_path, "/sdcard/clockworkmod/backup/%s", value2);
				} else {
					time_t t = time(NULL);
					struct tm *tmp = localtime(&t);
					if (tmp == NULL)
					{
						struct timeval tp;
						gettimeofday(&tp, NULL);
						sprintf(backup_path, "/sdcard/clockworkmod/backup/%d", tp.tv_sec);
					}
					else
					{
						strftime(backup_path, sizeof(backup_path), "/sdcard/clockworkmod/backup/%F.%H.%M.%S", tmp);
					}
				}

				ui_print("Backup options are ignored in CWMR: '%s'\n", value1);
				nandroid_backup(backup_path);
				ui_print("Backup complete!\n");
			} else if (strcmp(command, "restore") == 0) {
				// Restore
				tok = strtok(value, " ");
				strcpy(value1, tok);
				ui_print("Restoring '%s'\n", value1);
				tok = strtok(NULL, " ");
				if (tok != NULL) {
					ors_system = 0;
					ors_data = 0;
					ors_cache = 0;
					ors_boot = 0;
					ors_sdext = 0;

					memset(value2, 0, sizeof(value2));
					strcpy(value2, tok);
					ui_print("Setting restore options:\n");
					line_len = strlen(value2);
					for (i=0; i<line_len; i++) {
						if (value2[i] == 'S' || value2[i] == 's') {
							ors_system = 1;
							ui_print("System\n");
						} else if (value2[i] == 'D' || value2[i] == 'd') {
							ors_data = 1;
							ui_print("Data\n");
						} else if (value2[i] == 'C' || value2[i] == 'c') {
							ors_cache = 1;
							ui_print("Cache\n");
						} else if (value2[i] == 'R' || value2[i] == 'r') {
							ui_print("Option for recovery ignored in CWMR\n");
						} else if (value2[i] == '1') {
							ui_print("%s\n", "Option for special1 ignored in CWMR");
						} else if (value2[i] == '2') {
							ui_print("%s\n", "Option for special1 ignored in CWMR");
						} else if (value2[i] == '3') {
							ui_print("%s\n", "Option for special1 ignored in CWMR");
						} else if (value2[i] == 'B' || value2[i] == 'b') {
							ors_boot = 1;
							ui_print("Boot\n");
						} else if (value2[i] == 'A' || value2[i] == 'a') {
							ui_print("Option for android secure ignored in CWMR\n");
						} else if (value2[i] == 'E' || value2[i] == 'e') {
							ors_sdext = 1;
							ui_print("SD-Ext\n");
						} else if (value2[i] == 'M' || value2[i] == 'm') {
							ui_print("MD5 check skip option ignored in CWMR\n");
						}
					}
				} else
					LOGI("No restore options set\n");
				nandroid_restore(value1, ors_boot, ors_system, ors_data, ors_cache, ors_sdext, 0);
				ui_print("Restore complete!\n");
			} else if (strcmp(command, "mount") == 0) {
				// Mount
				if (value[0] != '/') {
					strcpy(mount, "/");
					strcat(mount, value);
				} else
					strcpy(mount, value);
				ensure_path_mounted(mount);
				ui_print("Mounted '%s'\n", mount);
			} else if (strcmp(command, "unmount") == 0 || strcmp(command, "umount") == 0) {
				// Unmount
				if (value[0] != '/') {
					strcpy(mount, "/");
					strcat(mount, value);
				} else
					strcpy(mount, value);
				ensure_path_unmounted(mount);
				ui_print("Unmounted '%s'\n", mount);
			} else if (strcmp(command, "set") == 0) {
				// Set value
				tok = strtok(value, " ");
				strcpy(value1, tok);
				tok = strtok(NULL, " ");
				strcpy(value2, tok);
				ui_print("Setting function disabled in CWMR: '%s' to '%s'\n", value1, value2);
			} else if (strcmp(command, "mkdir") == 0) {
				// Make directory (recursive)
				ui_print("Recursive mkdir disabled in CWMR: '%s'\n", value);
			} else if (strcmp(command, "reboot") == 0) {
				// Reboot
			} else if (strcmp(command, "cmd") == 0) {
				if (cindex != 0) {
					__system(value);
				} else {
					LOGE("No value given for cmd\n");
				}
			} else {
				LOGE("Unrecognized script command: '%s'\n", command);
				ret_val = 1;
			}
		}
		fclose(fp);
		ui_print("Done processing script file\n");
	} else {
		LOGE("Error opening script file '%s'\n", ors_script);
		return 1;
	}
	return ret_val;
}

void choose_bootanimation_menu(const char *ba_path)
{
    if (ensure_path_mounted(ba_path) != 0) {
        LOGE("Can't mount %s\n", ba_path);
        return;
    }

    static char* headers[] = {  "Choose a bootanimation",
                                "",
                                NULL
    };

    char* ba_file = choose_file_menu(ba_path, ".zip", headers);
    if (ba_file == NULL)
        return;

    if (confirm_selection("Confirm change bootanimation?", "Yes - Change")) {
        char tmp[PATH_MAX];
	sprintf(tmp, "change_ba.sh %s", ba_file);
	ensure_path_mounted("/system");
	__system(tmp);
	ensure_path_unmounted("/system");
	ui_print("Bootanimation has been changed.\n");
    }
}

void show_bootanimation_menu() {
    static char* headers[] = {  "Bootanimation Menu",
                                "",
                                NULL
    };

    char* list[] = { "choose bootanimation from internal sdcard",
        "choose bootanimation from external sdcard",
        NULL
    };

    int chosen_item = get_menu_selection(headers, list, 0, 0);
    switch (chosen_item) {
        case 0:
                choose_bootanimation_menu("/sdcard/");
                break;
        case 1:
                choose_bootanimation_menu("/external_sd/");
                break;
    }
}

void show_custom_ors_menu(const char* ors_path)
{
    if (ensure_path_mounted(ors_path) != 0) {
        LOGE("Can't mount %s\n", ors_path);
        return;
    }

    static char* headers[] = {  "Choose a script to run",
                                "",
                                NULL
    };

    char tmp[PATH_MAX];
    sprintf(tmp, "%s/clockworkmod/ors/", ors_path);
    char* ors_file = choose_file_menu(tmp, ".ors", headers);
    if (ors_file == NULL)
        return;

    if (confirm_selection("Confirm run script?", "Yes - Run")) {
	run_custom_ors(ors_file);
    }
}

void choose_aromafm_menu(const char* aromafm_path)
{
    if (ensure_path_mounted(aromafm_path) != 0) {
        LOGE("Can't mount %s\n", aromafm_path);
        return;
    }

    static char* headers[] = { "Browse for aromafm.zip",
                                NULL
    };

    char* aroma_file = choose_file_menu(aromafm_path, "aromafm.zip", headers);
    if (aroma_file == NULL)
        return;
    static char* confirm_install = "Confirm Run Aroma?";
    static char confirm[PATH_MAX];
    sprintf(confirm, "Yes - Run %s", basename(aroma_file));
    if (confirm_selection(confirm_install, confirm)) {
        install_zip(aroma_file);
    }
}
  //Show custom aroma menu: manually browse sdcards for Aroma file manager
void custom_aroma_menu() {
    static char* headers[] = { "Browse for aromafm.zip",
                                "",
                                NULL
    };

    static char* list[] = { "Search sdcard",
                            NULL,
                            NULL
    };

    char *other_sd = NULL;
    if (volume_for_path("/emmc") != NULL) {
        other_sd = "/emmc/";
        list[1] = "Search Internal sdcard";
    } else if (volume_for_path("/external_sd") != NULL) {
        other_sd = "/external_sd/";
        list[1] = "Search External sdcard";
    }

    for (;;) {
        //header function so that "Toggle menu" doesn't reset to main menu on action selected
        int chosen_item = get_filtered_menu_selection(headers, list, 0, 0, sizeof(list) / sizeof(char*));
        if (chosen_item == GO_BACK)
            break;
        switch (chosen_item)
        {
            case 0:
                choose_aromafm_menu("/sdcard/");
                break;
            case 1:
                choose_aromafm_menu(other_sd);
                break;
        }
    }
}
  //launch aromafm.zip from default locations
static int default_aromafm (const char* aromafm_path) {
        if (ensure_path_mounted(aromafm_path) != 0) {
            //no sdcard at moint point
            return 0;
        }
        char aroma_file[PATH_MAX];
        sprintf(aroma_file, "%s/clockworkmod/.aromafm/aromafm.zip", aromafm_path);
        if (access(aroma_file, F_OK) != -1) {
            install_zip(aroma_file);
            return 1;
        }
        return 0;
}

void show_extras_menu()
{
    static char* headers[] = {  "Extras Menu",
                                "",
                                NULL
    };

    static char* list[] = { "change bootanimation",
                            "enable/disable one confirm",
                            "hide/show backup & restore progress",
			    "set android_secure internal/external",
			    "aroma file manager",
			    "create custom zip (BETA)",
			    "run custom openrecoveryscript",
			    "recovery info",
			    NULL,
                            NULL };

    if (volume_for_path("/misc") != NULL) {
        list[9] = "Misc/Boot Backup & Restore";
    }
    else if (volume_for_path("/efs") != NULL) {
        list[9] = "EFS/Boot Backup & Restore";
    }

    for (;;)
    {
        int chosen_item = get_filtered_menu_selection(headers, list, 0, 0, sizeof(list) / sizeof(char*));
        if (chosen_item == GO_BACK)
            break;
        switch (chosen_item)
        {
            case 0:
		show_bootanimation_menu();
		break;
	    case 1:
		ensure_path_mounted("/sdcard");
                if( access("/sdcard/clockworkmod/.one_confirm", F_OK ) != -1 ) {
                   __system("rm -rf /sdcard/clockworkmod/.one_confirm");
                   ui_print("one confirm disabled\n");
                } else {
                   __system("touch /sdcard/clockworkmod/.one_confirm");
                   ui_print("one confirm enabled\n");
                }
		break;
	    case 2:
                ensure_path_mounted("/sdcard");
                if( access("/sdcard/clockworkmod/.hidenandroidprogress", F_OK ) != -1 ) {
                   __system("rm -rf /sdcard/clockworkmod/.hidenandroidprogress");
                   ui_print("nandroid progress will be shown\n");
                } else {
                   __system("touch /sdcard/clockworkmod/.hidenandroidprogress");
                   ui_print("nandroid progress will be hidden\n");
                }
                break;
	    case 3:
                ensure_path_mounted("/sdcard");
                if( access("/sdcard/clockworkmod/.is_as_external", F_OK ) != -1 ) {
                   __system("rm -rf /sdcard/clockworkmod/.is_as_external");
                   ui_print("android_secure will be set to internal\n");
                } else {
                   __system("touch /sdcard/clockworkmod/.is_as_external");
                   ui_print("android_secure will be set to external\n");
                }
                break;
	    case 4:
                //look for clockworkmod/.aromafm/aromafm.zip in /external_sd, then /sdcard and finally /emmc
                if (volume_for_path("/external_sd") != NULL) {
                    if (default_aromafm("/external_sd")) {
                        break;
                    }
                }
                if (volume_for_path("/sdcard") != NULL) {
                    if (default_aromafm("/sdcard")) {
                        break;
                    }
                }
                if (volume_for_path("/emmc") != NULL) {
                    if (default_aromafm("/emmc")) {
                        break;
                    }
                }
                ui_print("No clockworkmod/.aromafm/aromafm.zip on sdcards\n");
                ui_print("Browsing custom locations\n");
                custom_aroma_menu();
                break;
	    case 5:
		ensure_path_mounted("/system");
		ensure_path_mounted("/sdcard");
                if (confirm_selection("Create a zip from system and boot?", "Yes - Create custom zip")) {
		ui_print("Creating custom zip...\n");
		ui_print("This may take a while. Be Patient.\n");
                    char custom_path[PATH_MAX];
                    time_t t = time(NULL);
                    struct tm *tmp = localtime(&t);
                    if (tmp == NULL)
                    {
                        struct timeval tp;
                        gettimeofday(&tp, NULL);
                        sprintf(custom_path, "/sdcard/clockworkmod/zips/%d", tp.tv_sec);
                    }
                    else
                    {
                        strftime(custom_path, sizeof(custom_path), "/sdcard/clockworkmod/zips/%F.%H.%M.%S", tmp);
                    }
                    create_customzip(custom_path);
		ui_print("custom zip created in /sdcard/clockworkmod/zips/\n");
	}
		ensure_path_unmounted("/system");
		break;
	    case 6:
		show_custom_ors_menu("/sdcard");
		break;
	    case 7:
                ui_print(EXPAND(RECOVERY_VERSION)"\n");
                ui_print("Build version: "EXPAND(SK8S_BUILD)" - "EXPAND(TARGET_DEVICE)"\n");
                ui_print("CWM Base version: "EXPAND(CWM_BASE_VERSION)"\n");
                //ui_print(EXPAND(BUILD_DATE)"\n");
                //ui_print("Build Date: %s at %s\n", __DATE__, __TIME__);
		ui_print("Build Date: 12/17/2012 6:20 pm\n");
	    case 8:
		if (volume_for_path("/misc") != NULL) {
                    show_misc_menu();
		}
		else if (volume_for_path("/efs") != NULL) {
		    show_efs_menu();
		}
                break;
	}
    }
}
