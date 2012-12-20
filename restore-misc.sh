#!/sbin/sh
# *** Thanks to PhilZ for this! *** #

MISC_PATH=`cat /etc/recovery.fstab | grep -v "#" | grep /misc | awk '{print $3}'`;

mkdir -p "$1"/clockworkmod/.miscbackup/;

echo "">>"$1"/clockworkmod/.miscbackup/log.txt;
echo "Restore $1/clockworkmod/.miscbackup/misc.img to $MISC_PATH">>"$1"/clockworkmod/.miscbackup/log.txt;
(cat "$1"/clockworkmod/.miscbackup/misc.img > "$MISC_PATH") 2>> "$1"/clockworkmod/.miscbackup/log.txt;

if [ $? = 0 ];
     then echo "Success!">>"$1"/clockworkmod/.miscbackup/log.txt;
     else echo "Error!">>"$1"/clockworkmod/.miscbackup/log.txt;
fi;
