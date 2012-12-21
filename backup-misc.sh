#!/sbin/sh

#######################################
# Do not remove this credits header #
# sk8erwitskil : first release #
# PhilZ-cwm6 : multi device support #
#######################################

MISC_PATH=`cat /etc/recovery.fstab | grep -v "#" | grep /misc | awk '{print $3}'`;

mkdir -p "$1"/clockworkmod/.miscbackup;

echo "">>"$1"/clockworkmod/.miscbackup/log.txt;
echo "Backup MISC ($MISC_PATH) to $1/clockworkmod/.miscbackup/misc.img">>"$1"/clockworkmod/.miscbackup/log.txt;
(cat "$MISC_PATH" > "$1"/clockworkmod/.miscbackup/misc.img) 2>> "$1"/clockworkmod/.miscbackup/log.txt;

if [ $? = 0 ];
     then echo "Success!">>$1/clockworkmod/.miscbackup/log.txt;
     else echo "Error!">>$1/clockworkmod/.miscbackup/log.txt;
fi;
