// Simple shell interpreter implementation
// @author Stepan Belousov
#include <fcntl.h>
#include <limits.h>
#include <pwd.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

const size_t max_input_len = 255;
const size_t max_args_count = 100;
const size_t max_hostname_len = 255;

typedef enum { INTERPRETER_OK, INTERPRETER_ERROR, INTERPRETER_EOF } status_t;

// Read one line from the input
status_t get_line(char result[]) {
	size_t ind = 0;
	bool good = false;
	while (true) {
		int c;
		if ((c = getchar()) == EOF) {
			if (good) {
				break;
			} else {
				return INTERPRETER_EOF;
			}
		}
		if (c == '\n') {
			break;
		}
		if (c != ' ') {
			good = true;
		}
		if (ind == max_input_len) {
			while ((c = getchar()) != EOF && c != '\n');
			fprintf(stderr, "Error: the command is too long!\n");
			return INTERPRETER_ERROR;
		}
		result[ind++] = (char)c;
	} 
	if (!good) {
		return INTERPRETER_ERROR;
	}
	result[ind] = '\0';
	return INTERPRETER_OK;
}

// Replace ~ with full path
void replace_homepath(char result[], char* src) {
	if (!src) {
		src = getenv("HOME");
	}
	if (src[0] == '~') {
		strcpy(result, getenv("HOME"));
		strcat(result, src + 1);
	} else {
		strcpy(result, src);
	}
}

int main() {
	pid_t cpid;
	int wait_st, fd[2];
	char *argv[max_args_count + 1], *arg;
	char line[max_input_len + 1], dir[PATH_MAX], host[max_hostname_len + 1];
	const char* delimiters = " \t";

	while (true) {
		// Print prompt
		if (!getcwd(dir, PATH_MAX)) {
			perror("Get_cur_dir error");
			break;
		}
		if (gethostname(host, max_hostname_len + 1)) {
			perror("Get_hostname error");
			break;
		}
		struct passwd* pwd = getpwuid(getuid());
		if (!pwd || !pwd->pw_name) {
			perror("Get_login error");
			break;
		}
		fprintf(stderr,"%s@%s:", pwd->pw_name, strtok(host, "."));
		fprintf(stderr,"%s$ ", dir);

		// Read input
		status_t st = get_line(line);
		if (st == INTERPRETER_ERROR) {
			continue;
		}
		if (st == INTERPRETER_EOF) {
			fprintf(stderr,"(finished)\n");
			break;
		}
		argv[0] = strtok(line, delimiters);

		// Exit call
		if (!strcmp(argv[0], "exit")) {
			fprintf(stderr,"(finished)\n");
			break;
		}

		// Parse args
		bool bad = false;
		size_t ind = 1;
		while (true) {
			arg = strtok(NULL, delimiters);
			if (!arg) {
				break;
			}
			if (ind == max_args_count) {
				bad = true;
				break;
			}
			argv[ind++] = arg;
		}
		if (bad) {
			fprintf(stderr,"Error: too many arguments!\n");
			continue;
		}
		argv[ind] = NULL;
		size_t argc = ind + 1;

		// Change dir
		if (!strcmp(argv[0], "cd") || !strcmp(argv[0], "chdir")) {
			if (argv[2] && argv[1]) {
				fprintf(stderr,"Error: too many arguments for 'cd'!\n");
				continue;
			}
			replace_homepath(dir, argv[1]);
			if (chdir(dir)) {
				perror("Change dir error");
			}
			continue;
		}

		// Pipeline implementation
		size_t ppos = 0;
		for (size_t i = 1;i + 1 < argc;i++) {
			if (!strcmp(argv[i], "|")) {
				ppos = i;
				break;
			}
		}
		if (ppos) {
			pipe(fd);
			// Set end of the first args array
			argv[ppos] = NULL;

			// First process
			cpid = fork();
			if (cpid == -1) {
				perror("Fork() error");
				return 0;
			}
			if (!cpid) {
				close(fd[0]);
				if (dup2(fd[1], 1) == -1) {
					perror("Dup() error");
					return 0;
				}
				close(fd[1]);
				execvp(argv[0], argv);
				perror("Command error");
				_exit(1);
			}
			
			// Second process
			cpid = fork();
			if (cpid == -1) {
				perror("Fork() error");
				return 0;
			}
			if (!cpid) {
				close(fd[1]);
				if (dup2(fd[0], 0) == -1) {
					perror("Dup() error");
					return 0;
				}
				close(fd[0]);
				execvp(argv[ppos + 1], argv + (ppos + 1) );
				perror("Command error");
				_exit(1);
			}
			close(fd[0]);
			close(fd[1]);
			// Double wait because there are 2 processes
			wait(&wait_st);
			wait(&wait_st);
			continue;
		}

		cpid = fork();
		if (cpid == -1) {
			perror("Fork() error");
			return 0;
		}
		if (cpid) {
			// In parent process
			wait(&wait_st);
		} else {
			// In child process
			execvp(argv[0], argv);
			replace_homepath(dir, argv[0]);
			if (!chdir(dir)) {
				fprintf(stderr,"%s: is a directory\n", argv[0]);
				exit(0);
			}
			perror("Command error");
			_exit(1);
		}
	}
	return 0;
}
