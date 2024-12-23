#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>

/*
 * Test program to be used as a "generic step" in a recipe task.
 * When invoked, it announces itself using the name by which it was invoked,
 * then delays for a randomly chosen interval.
 *   If the argument '-' is given, then it copies stdin to stdout until EOF is seen.
 *   If the argument '-m message' is given, the specified message will be printed.
 *   If the argument '-d' is given, the random delay is disabled (delay set to 0).
 *   If the argument '-c' is given, the remaining arguments starting are treated as
 *     a command to be run in a child process.
 *   If the argument '-c' is not given, then any arguments starting from the first
 *     non-option argument are are simply ignored.
 */

/*
 * Set the following to limit the size of the timestamp that is output
 * and make it more readable.  Previously, this was necessary to make the
 * output fit in a fixed-size buffer, but this has now been fixed, so it
 * should be just a convenience.
 */
#define BASE_SECONDS (0)
int get_ms(struct timespec *);

int main(int argc, char *argv[]) {
    int c;
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    srandom((unsigned int)ts.tv_nsec);
    int delay = random() % 10;
    int copy = 0;
    int options = 0;
    int cmd = 0;
    char *message = 0;

    for(options = 1; options < argc; options++) {
        if (argv[options][0] == '-') {
            switch(argv[options][1]) {
                case 'm':
		    options++ ;
                    message = argv[options];
                    break;
                case 'd':
                    delay = 0;
		    break;
	        case 'c':
		    cmd = 1;
		    break;
		case 0:
                    copy = 1;
		    break;
            }
        } else {
	    break;
	}
	if(cmd) {
	    options++;
	    break;
	}
    }
    //fprintf(stderr, "argc = %d, options = %d, delay = %d, copy = %d, message = %s\n",
    //	    argc, options, delay, copy, message);

    char *buffer = NULL;
    size_t size;
    FILE *out = open_memstream(&buffer, &size);
    int ms = get_ms(&ts);
    fprintf(out, "START\t[%ld.%03d,%6d,%2d]",
	    ts.tv_sec-BASE_SECONDS, ms, getpid(), delay);
    for(int i = 0; i < argc; i++)
	fprintf(out, " %s", argv[i]);
    fprintf(out, "\n");
    fclose(out);
    write(STDERR_FILENO, buffer, size);
    free(buffer);
    if (message != NULL) {
        printf("%s\n", message);
    }
    usleep(delay * 100000);
    if(copy) {
	// Copy stdin to stdout.
	while((c = getchar()) != EOF)
	    putchar(c);
    }
    if(cmd && argc > options) {
	// Treat remainder of arguments as a command to be run.
	int pid;
	if((pid = fork()) == 0) {
	    execvp(argv[options], &argv[options]);
	    perror("");
	    fprintf(stderr, "%s: execvp '%s' failed\n", argv[0], argv[options]);
	    exit(1);
	} else {
	    if(pid < 0) {
		fprintf(stderr, "%s: fork() failed\n", argv[0]);
		exit(1);
	    } else {
		int status;
		if(wait(&status) < 0) {
		    fprintf(stderr, "%s: wait() failed\n", argv[0]);
		    exit(1);
		}
		if(!WIFEXITED(status) || WEXITSTATUS(status) != EXIT_SUCCESS) {
		    fprintf(stderr, "%s: command '%s' failed (status = 0x%x)\n",
			    argv[0], argv[options], status);
		    exit(1);
		}
	    }
	}
    }
    clock_gettime(CLOCK_REALTIME, &ts);
    ms = get_ms(&ts);
    fprintf(stderr, "END\t[%ld.%03d,%6d,%2d]\n",
	    ts.tv_sec-BASE_SECONDS, ms, getpid(), delay);
}

int get_ms(struct timespec * ts){
    int ms = (int) (ts->tv_nsec / 1.0e6); // Convert nanoseconds to milliseconds
    if (ms > 999) {
        ts->tv_sec++;
        ms = 0;
    }
    return ms;
}
