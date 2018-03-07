#include <stdio.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<fcntl.h>
#include<unistd.h>

int main() {
	int fd;
	char s[20];
	fd = open("/dev/MyPipe/in", O_RDONLY, S_IREAD);
	
	if(fd != -1) {
		read(fd, &s, 12);
		printf("%s\n", s);
		return 0;
	}
	else {
		printf("Can't open device file\n");
		return -1;
	}
}
