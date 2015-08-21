CFLAGS=-Wall -Werror

tvpn: tvpn.c
test:
	unshare -Ump -f --propagation slave --mount-proc  -r tests/run.sh 
