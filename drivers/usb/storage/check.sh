i=1
while(($i<100))
do
	dmesg |tail -n 999 -f
        sudo dmesg -C
done
