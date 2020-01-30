all: make runsc runcs clean

make: system_call.c context_switch.c
        gcc -o system_call system_call.c
        gcc -o context_switch context_switch.c

runsc:
        ./system_call

runcs:
        ./context_switch

clean:
        rm -f temp.txt
