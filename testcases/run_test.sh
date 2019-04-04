#!/bin/bash

#set -x
cur_path=`pwd`
db_name='testdb'

cd ..
sh build.sh
cp build_*/cubrid-backup-api-*.tar.gz testcases/
cd testcases
tar xfz cubrid-backup-api-*.tar.gz
cmake .
make

expect_val=""
function restoredb_exe()
{
	expect_val=`expect -c "
        	spawn cubrid restoredb $1 $db_name
                expect {
			The { send \"0\r\"; exp_continue }
                        eof
                }
		"`
}

rm -rf ./backup_dir/* ./restore_dir/* *_result $CUBRID/log/* $CUBRID/conf/cubrid_backup.conf

cubrid service stop
cubrid createdb -r --db-volume-size=100M --log-volume-size=100M $db_name en_US
cubrid server start $db_name

echo ""
echo "============================"
echo "==cubrid backup api test start"
echo ""
echo "==run backup_tc01"
./backup_tc01 $db_name 0 ./backup_dir/${db_name}_bk0v000 > backup_tc01_result 2>&1 
sleep 1
./backup_tc01 $db_name 1 ./backup_dir/${db_name}_bk1v000 >> backup_tc01_result 2>&1 
sleep 1
./backup_tc01 $db_name 2 ./backup_dir/${db_name}_bk2v000 >> backup_tc01_result 2>&1 

sleep 1
if [ `grep "cubrid backupdb" $CUBRID/log/cubrid_utility.log | wc -l` -eq 3 ]; then
	echo "[OK] cubrid_utility.log" >> backup_tc01_result
else
	echo "[NOK] cubrid_utility.log" >> backup_tc01_result
fi
echo ""
cubrid server stop $db_name
rm -rf $db_name
restoredb_exe "-B ./backup_dir -l 2"
cubrid server start $db_name
if [ `cubrid server status $db_name |grep "Server $db_name" |wc -l` -eq 0 ]; then
	echo "[NOK] run restoredb" >> backup_tc01_result
	cubrid deletedb $db_name
	cubrid createdb -r --db-volume-size=100M --log-volume-size=100M $db_name en_US
	cubrid server start $db_name
else
	echo "[OK] run restoredb" >> backup_tc01_result
fi
echo ""

echo "==run restore_tc01"
./restore_tc01 $db_name 0 ./backup_dir/${db_name}_bk0v000 0 ./restore_dir/ > restore_tc01_result 2>&1
if [ -z "`cmp ./backup_dir/${db_name}_bk0v000 ./restore_dir/${db_name}_bk0v000`" ]; then
	echo "[OK] compare restore file of level 0" >> restore_tc01_result
else
	echo "[NOK] compare restore file of level 0" >> restore_tc01_result
fi
./restore_tc01 $db_name 1 ./backup_dir/${db_name}_bk1v000 0 ./restore_dir/ >> restore_tc01_result 2>&1
if [ -z "`cmp ./backup_dir/${db_name}_bk1v000 ./restore_dir/${db_name}_bk1v000`" ]; then
	echo "[OK] compare restore file of level 1" >> restore_tc01_result
else
	echo "[NOK] compare restore file of level 1" >> restore_tc01_result
fi
./restore_tc01 $db_name 2 ./backup_dir/${db_name}_bk2v000 0 ./restore_dir/ >> restore_tc01_result 2>&1
if [ -z "`cmp ./backup_dir/${db_name}_bk2v000 ./restore_dir/${db_name}_bk2v000`" ]; then
	echo "[OK] compare restore file of level 2" >> restore_tc01_result
else
	echo "[NOK] compare restore file of level 2" >> restore_tc01_result
fi
echo ""
echo "==run backup_tc02"
./backup_tc02 $db_name 0 > backup_tc02_result 2>&1 
sleep 5
./backup_tc02 $db_name 1 >> backup_tc02_result 2>&1 
sleep 1
./backup_tc02 $db_name 2 >> backup_tc02_result 2>&1 
echo ""

echo "==run restore_tc02"
./restore_tc02 $db_name 0 0 ./restore_dir > restore_tc02_result 2>&1 
./restore_tc02 $db_name 1 0 ./restore_dir >> restore_tc02_result 2>&1 
./restore_tc02 $db_name 2 0 ./restore_dir >> restore_tc02_result 2>&1 
echo ""

echo "==run backup_tc03"
./backup_tc03 ${db_name} > backup_tc03_result 2>&1 
echo ""

echo "==run restore_tc03"
./restore_tc03 ${db_name} > restore_tc03_result 2>&1 
echo ""

echo "==run backup_tc04"
./backup_tc04 $db_name 0 -1 -1 -1 -1 ./backup_dir/${db_name}_bk0v000 > backup_tc04_result 2>&1 

echo ""
cubrid server stop $db_name
rm -rf $db_name
restoredb_exe "-B ./backup_dir -l 0"
cubrid server start $db_name
if [ `cubrid server status $db_name |grep "Server $db_name" |wc -l` -eq 0 ]; then
	echo "[NOK] run restoredb" >> backup_tc04_result
	cubrid deletedb $db_name
	cubrid createdb -r --db-volume-size=100M --log-volume-size=100M $db_name en_US
	cubrid server start $db_name
else
	echo "[OK] run restoredb" >> backup_tc04_result
fi
sleep 1

./backup_tc04 $db_name 0 1 0 1 1 ./backup_dir/${db_name}_bk0v000 >> backup_tc04_result 2>&1 

echo ""
cubrid server stop $db_name
rm -rf $db_name
restoredb_exe "-B ./backup_dir -l 0"
cubrid server start $db_name
if [ `cubrid server status $db_name |grep "Server $db_name" |wc -l` -eq 0 ]; then
	echo "[NOK] run restoredb" >> backup_tc04_result
	cubrid deletedb $db_name
	cubrid createdb -r --db-volume-size=100M --log-volume-size=100M $db_name en_US
	cubrid server start $db_name
else
	echo "[OK] run restoredb" >> backup_tc04_result
fi
sleep 1

./backup_tc04 $db_name 0 1 0 0 0 ./backup_dir/${db_name}_bk0v000 >> backup_tc04_result 2>&1 

echo ""
cubrid server stop $db_name
rm -rf $db_name
restoredb_exe "-B ./backup_dir -l 0"
cubrid server start $db_name
if [ `cubrid server status $db_name |grep "Server $db_name" |wc -l` -eq 0 ]; then
	echo "[NOK] run restoredb" >> backup_tc04_result
	cubrid deletedb $db_name
	cubrid createdb -r --db-volume-size=100M --log-volume-size=100M $db_name en_US
	cubrid server start $db_name
else
	echo "[OK] run restoredb" >> backup_tc04_result
fi
sleep 1

./backup_tc04 $db_name 0 0 0 1 0 ./backup_dir/${db_name}_bk0v000 >> backup_tc04_result 2>&1 

echo ""
cubrid server stop $db_name
rm -rf $db_name
restoredb_exe "-B ./backup_dir -l 0"
cubrid server start $db_name
if [ `cubrid server status $db_name |grep "Server $db_name" |wc -l` -eq 0 ]; then
	echo "[NOK] run restoredb" >> backup_tc04_result
	cubrid deletedb $db_name
	cubrid createdb -r --db-volume-size=100M --log-volume-size=100M $db_name en_US
	cubrid server start $db_name
else
	echo "[OK] run restoredb" >> backup_tc04_result
fi
sleep 1

./backup_tc04 $db_name 0 0 0 0 1 ./backup_dir/${db_name}_bk0v000 >> backup_tc04_result 2>&1 

echo ""
cubrid server stop $db_name
rm -rf $db_name
restoredb_exe "-B ./backup_dir -l 0"
cubrid server start $db_name
if [ `cubrid server status $db_name |grep "Server $db_name" |wc -l` -eq 0 ]; then
	echo "[NOK] run restoredb" >> backup_tc04_result
	cubrid deletedb $db_name
	cubrid createdb -r --db-volume-size=100M --log-volume-size=100M $db_name en_US
	cubrid server start $db_name
else
	echo "[OK] run restoredb" >> backup_tc04_result
fi
sleep 1

echo ""

echo "==run conf_test"
echo ""
sh conf_test.sh $db_name
cubrid server stop $db_name

for i in $(seq 1 10); do
	restoredb_exe "-B ./backup_dir/$i"
	if [ `echo $expect_val | grep "The following" | wc -l` -eq 1 ]; then
		echo "[NOK] set cubrid_backup.conf : restoredb_exe ($i)" >> conf_test_result
	else
		echo "[OK] set cubrid_backup.conf : restoredb_exe ($i)" >> conf_test_result
	fi
done
echo ""
echo "==cubrid backup api test end"
echo "=========================="

echo ""
cubrid service stop
cubrid deletedb $db_name
rm -rf ${db_name}_bkvinf ./backup_dir/* ./restore_dir/* $CUBRID/log/*

fail_count=`grep "\[NOK\]" *_result |wc -l`
echo ""
echo "==================="
if [ $fail_count -ne 0 ]; then
echo "FAILED TEST SUMMARY"
echo "-------------------"
for result in `ls *_result`
do
	if [ `cat $result |grep "\[NOK\]" |wc -l` -ne 0 ]; then
		echo ${result%_*}
	fi
done
else
	echo "ALL PASSED"
fi
echo "==================="
echo ""

