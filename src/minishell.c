#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <limits.h>
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>
#include <sys/wait.h>
#define BRIGHTBLUE "\x1b[34;1m"
#define DEFAULT    "\x1b[0m"

volatile sig_atomic_t signal_val = 0;

/* Signal handler */
void catch_signal(int sig) {
	signal_val = sig;
	fprintf(stdout, "\n");
	fflush(stdout);
}

int main() {

	// Set up signals for exiting
	struct sigaction action;

	memset(&action, 0, sizeof(struct sigaction));
	action.sa_handler = catch_signal;

	if (sigaction(SIGINT, &action, NULL) == -1) {
    	perror("sigaction(SIGINT)");
        return EXIT_FAILURE;
    }

	// Set up buffers to store input, working directory
	char input_buf[128];
   	char wd_buf[PATH_MAX];
	char input_copy[128];

	while (true) {
		// Print working directory (in color)
		if (getcwd(wd_buf, PATH_MAX - 1) == NULL) {
			fprintf(stderr, "Error: cannot get current working directory. %s.\n", strerror(errno));
			return EXIT_FAILURE;
		}

		printf("[");
		printf("%s%s", BRIGHTBLUE, wd_buf);
		printf("%s]$ ", DEFAULT);
		fflush(stdout);

		// Draw input
		if (fgets(input_buf, sizeof(input_buf), stdin) == NULL) {
			if (errno == EINTR) {
					//printf("Read interrupted.\n");
					errno = 0;
					continue;
			} else if (feof(stdin)) {
				printf("\n");
				exit(EXIT_SUCCESS);
			} else if (ferror(stdin)) {
				printf("\n");
				exit(EXIT_FAILURE);
			}
		}	

		// Null terminate input
		char *eoln = strchr(input_buf, '\n');
		if (eoln != NULL) {
			*eoln = '\0';
		}
		
		// Case of enter
		if (!strcmp(input_buf, "\n")) {
			printf("\n");
			break;
		}
			
		// Exit if input is "exit"
		if (!strncmp(input_buf, "exit", 5)) {
			break;
		}

		// Copy of input
		strcpy(input_copy, input_buf);

		// cd method
		uid_t uid = getuid();
		struct passwd *cd_buf = getpwuid(uid);
		if (cd_buf == NULL) {
			fprintf(stderr, "Error: Cannot get passwd entry. %s.\n", strerror(errno));
		}

		if (!strcmp(input_buf, "cd ~") || !strcmp(input_buf, "cd")) {
				if (chdir(cd_buf->pw_dir) == -1) {
				fprintf(stderr, "Error: Cannot change directory to '%s'. %s.\n", cd_buf->pw_dir, strerror(errno));
			}
		}		

		else if (input_buf[0] == 'c' && input_buf[1] == 'd') {
			char new_dir[128];
			int in_quotes = 0;
	    	if (input_buf[3] == '~' || input_buf[3] == ' ' || input_buf[2] == '\0') {
				memcpy(wd_buf, cd_buf->pw_dir, 128);
				for (int i = 0; i < sizeof(input_buf); i++) {
					new_dir[i] = input_buf[i+5];
				}
	    	}
			else {
        		char *current = input_copy;
				current += 3;
				int i = 0;
				while (*current != '\0') {
                    if (*current == '\"') {
                        in_quotes++;
					} else {
						new_dir[i++] = *current;
                    }
                	current++;
                }

				new_dir[i++] = '\0';
				if (in_quotes % 2 != 0) {
                    fprintf(stderr, "Error: Incomplete set of quotes entered after 'cd'. Exit failure.\n");
            		return EXIT_FAILURE;
				
                } else if (*new_dir == '\0' && in_quotes > 0) {
					fprintf(stderr, "Error: No arguments in quotes entered after 'cd'. Exit failure.\n");
					return EXIT_FAILURE;		
				}
			}

	    	strcat(wd_buf, "/");	    	
			strcat(wd_buf, new_dir); // cd_location.

	    	if (in_quotes == 0 && strchr(new_dir, ' ') != NULL) {
	    		fprintf(stderr, "Error: Too many arguments to cd.\n");
			} else if (chdir(wd_buf) == -1) {
				fprintf(stderr, "Error: Cannot change directory to '%s'. %s.\n", new_dir, strerror(errno));
			}
		}

		// Forking/execing with child
		else {
	    	char *all_args[128];
	    	int i = 0;
			char *args = strtok(input_buf, " ");

			while (args != NULL) {
				all_args[i++] = args;
				args = strtok(NULL, " ");
			}

			all_args[i] = "\0";
			char *all_args_copy[i];
			int j = 0;
			for (j = 0; j < i; j++) {
				all_args_copy[j] = all_args[j];
			}
			all_args_copy[j] = NULL;

			// testing all_args_copy contents
			int stat;
			pid_t p = fork();
			if (p > 0) {
				waitpid(p, &stat, 0);
			}

			else if (p == 0) {
				if (strcmp(all_args[1], "\0") == 0) {
					execlp(all_args[0], all_args[0], NULL);
				} else {
					execvp(all_args[0], all_args_copy);
				}
			
				fprintf(stderr, "Error: exec() failed. %s.\n", strerror(errno));
				exit(0);
			} else {
				fprintf(stderr, "Error: fork() failed. %s.\n", strerror(errno));
			}
		}
    }
	
	return EXIT_SUCCESS;
}

