
cat System.map | sed -n 's/ffffff\(\w*\)* T __init_begin/\1/p' | tr [a-z] [A-Z] > /tmp/__init_begin.txt
cat System.map | sed -n 's/ffffff\(\w*\)* t _head/\1/p' | tr [a-z] [A-Z] > /tmp/_head.txt
cat System.map | sed -n 's/ffffff\(\w*\)* D _sdata/\1/p' | tr [a-z] [A-Z] > /tmp/_sdata.txt
cat System.map | sed -n 's/ffffff\(\w*\)* D _edata/\1/p' | tr [a-z] [A-Z] > /tmp/_edata.txt
#caculate data section size
echo ibase=16 > /tmp/bc.txt
#echo "`cat /tmp/_edata.txt` - `cat /tmp/_sdata.txt`" >> /tmp/bc.txt
echo "`cat /tmp/_edata.txt` - `cat /tmp/__init_begin.txt`" >> /tmp/bc.txt
echo quit >> /tmp/bc.txt
bc /tmp/bc.txt -q > /tmp/data_sec_size.txt


#caculate data section offset
echo ibase=16 > /tmp/bc.txt
#echo "`cat /tmp/_sdata.txt` - `cat /tmp/_head.txt`" >> /tmp/bc.txt
echo "`cat /tmp/__init_begin.txt` - `cat /tmp/_head.txt`" >> /tmp/bc.txt
echo quit >> /tmp/bc.txt
bc /tmp/bc.txt -q > /tmp/data_sec_offset.txt

#generate image
echo "data section size `cat /tmp/data_sec_size.txt`"
echo "data section offset `cat /tmp/data_sec_offset.txt`"
dd skip=`cat /tmp/data_sec_offset.txt` count=`cat /tmp/data_sec_size.txt` if=arch/arm64/boot/Image of=Image_data_sec bs=1

cp Image_data_sec ../verify_platform/verify/TestSuite/suite_data/sys_suite_data/
cp /tmp/data_sec_offset.txt ../verify_platform/verify/TestSuite/suite_data/sys_suite_data/Image_data_sec_offset.txt