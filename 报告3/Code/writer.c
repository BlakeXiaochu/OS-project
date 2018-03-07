#include <stdio.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<fcntl.h>
#include<unistd.h>

int main() {
	int fd;
	char s[] = "Hello World!";
	fd = open("/dev/MyPipe/out", O_WRONLY, S_IWRITE);
	
	if(fd != -1) {
		write(fd, &s, 12);
		return 0;
	}
	else {
		printf("Can't open device file\n");
		return -1;
	}
}
