// Name: Tingting Fang
// Course: CS344 Operating System
// Assignment: 3
// Description: Implement a shell.
// Program Functions:
//	--Provide a prompt for running commands
//	--Handle blank lines and comments, which are lines beginning with the # character
//	--Provide expansion for the variable $$
//	--Execute 3 commands exit, cd, and status via code built into the shell
//	--Execute other commands by creating new processes using a function from the exec family of functions
//	--Support input and output redirection
//	--Support running commands in foreground and background processes
//	--Implement custom handlers for 2 signals, SIGINT and SIGTSTP

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>
#include <linux/limits.h>
#include <sys/wait.h>
#include <fcntl.h>

#define BG '&'
#define IN "<"
#define OUT ">"
#define COMMENT "#"
#define NEWLINE "\n"
#define VAR "$"
#define EXIT "exit"
#define CHDIR "cd"
#define ARR_END "end"
#define STATUS "status"

//declare user's command
struct command
{
	char *name;
	int argc;
	char *argv[512];
	char *input;
	char *output;
	int bg;
};

//declare background process
struct process
{
	pid_t ID;
	int status;
	struct process* next;
};

//function interface
void str_copy(char *dest, char *src, pid_t process_ID);
void remove_process(struct process* child);

//global variables
struct process* head = NULL;
struct process* tail = NULL;

int fg = -5; //initialize foreground process exit value;
int sig = 0; //1 for interrupt by signal 
int f_only = 0; //0 for turn off f_only mode, 1 for turn on f_only mode
pid_t last_fg_process = 0;


char *get_command(){
	char *command_line = calloc(2049, sizeof(char));
	//prompt user	
	printf(": ");
	fflush(stdout);
	fgets(command_line, 2049, stdin);
	return command_line;
}


char* trim_command(struct command* currCd, char* command_line){
	int len = strlen(command_line);
	//foreground mode off
	if (f_only == 0){
		if(command_line[len - 2] == BG){
			//set bg value, delete "&"
			currCd->bg = 1;
			command_line[len - 2] = '\0';
		}
	//foreground mode on
	} else {
		if (command_line[len - 2] == BG){
			//delete "&"
			command_line[len - 2] = '\0';
		}
	}
	len = strlen(command_line);
	//check command length
	if (strlen(command_line) <= 0){
		free(command_line);
		command_line = NULL;
	}
	return command_line;
}


struct command* get_command_info(struct command* currCd, char* command_line){
	char *token = strtok(command_line, " \n");
	currCd->name = calloc(strlen(token) + 1, sizeof(char));
		
	pid_t process_ID = getpid();
	str_copy(currCd->name, token, process_ID);
	
	int i = 0; 
	//parse user's command arguments
	while (token != NULL && i < 512){
		token = strtok(NULL, " \n");
		if (token != NULL){
			if (strcmp(token, IN) == 0){
				token = strtok(NULL, " \n");
				currCd->input = calloc(strlen(token) + 1, sizeof(char));
				str_copy(currCd->input, token, process_ID);
			} else if(strcmp(token, OUT) == 0){
				token = strtok(NULL, " \n");
				currCd->output = calloc(strlen(token) + 1, sizeof(char));
				str_copy(currCd->output, token, process_ID);
			} else {
				currCd->argv[i] = calloc(strlen(token) + 1, sizeof(char));
				str_copy(currCd->argv[i], token, process_ID);
				currCd->argc++;
			}
			i++;
		}
	}
	free(command_line);
	return currCd;	
} 


struct command*  parse_command(char *command_line){
	struct command *currCd = malloc(sizeof(struct command));
	currCd->argc = 0;
	currCd->bg = 0;//0 for not background running, 1 for background running
	currCd->input = NULL;
	currCd->output = NULL;
	
	command_line = trim_command(currCd, command_line);
	if (command_line == NULL){
		free(currCd);
		currCd = NULL;
	} else {
		currCd = get_command_info(currCd, command_line);
	}
	return currCd;	
}


void str_copy(char *dest, char *src, pid_t process_ID){
	dest[0] = '\0';  //initialize dest to empty array
	int cont = 0;
	int len = strlen(src);
	
	for (int i = 0; i < len; i++){
		if (strncmp(src + i, VAR, 1) == 0 && cont == 0){
			cont = 1;
			strncat(dest, VAR, 1);
		} else if (strncmp(src + i, VAR, 1) == 0 && cont == 1){			
			dest[strlen(dest) - 1] = '\0'; //delete the previous added "$". 
			dest[strlen(dest) - 1] = '\0'; //delete the previous added "$". 
			char *tmp = calloc(50, sizeof(char));
			sprintf(tmp, "%d", process_ID);			
			strcat(dest, tmp);
			cont = 0;
			free(tmp);
		} else if (strncmp(src + i, VAR, 1) != 0 && cont == 1){
			cont = 0;
			strncat(dest, src + i, 1);
		} else {
			cont = 0;
			strncat(dest, src + i, 1);
		}
	}
}


//exit command
void all_exit(void){
	struct process* child = head;
	//kill background child
	while (child != NULL) {
		if (child->status == -1){
			kill(child->ID, 9);
			child->status = 0;
			remove_process(child);			
		}
		child = child->next;
	}
}


void remove_process(struct process* child){
	struct process* list = head;
	struct process* prev = NULL;
	while (list != NULL){
		struct process* tmp = list;
		if (list  == child){
			if (list == head){
				head = list->next;
				prev = NULL;
			} else {
				prev->next = list->next;
			}
			list = list->next;
			free(tmp);
			break;
		} else {
			prev = list;
			list = list->next;
		}
	}
}


void change_dir(int argc, char *arg){
	char *home = getenv("HOME");
	char *path = calloc(PATH_MAX, sizeof(char));
	char *curr = calloc(PATH_MAX, sizeof(char));
	
	int len = strlen(home);
	int res;
	
	//no argument
	if ((argc == 0) || (strncmp(arg, IN, 1) == 0) || \
		(strncmp(arg, OUT, 1) == 0)){
		res = chdir(home);
	//with one argument
	} else {
		//argument is an absolute path
		if (strncmp(arg, home, len) == 0) {
			res = chdir(arg);
		} else {
			//path build on current directory
			getcwd(curr, PATH_MAX);
			strcpy(path, curr);
			strcat(path, "/");
			strcat(path, arg);
			res = chdir(path);
		}
	}
	if (res != 0){
		printf("error in redirectory\n");
		fflush(stdout);
	}
	free(curr);
	free(path);
}


void get_status(int status){
	if (sig == 1){
		printf("terminated by signal %d\n", status);
		fflush(stdout);
	} else {
		printf("exit value %d\n", status);
		fflush(stdout);
	}
}


void redirect_input(char *input){
	//redirect input files to stdin
	if (input != NULL){
		int file_in = open(input, O_RDONLY);
		if (file_in == -1) {
			printf("cannot open %s for input\n", input);
			fflush(stdout);
			exit(1);
		} else {
			//printf("redirect input success\n");
			int new_in_fd = dup2(file_in, 0);
			if (new_in_fd == -1) {
				perror("dup2() error");
				printf("cannot redirect input\n");
				fflush(stdout);
			}
		}
	}
}


void redirect_output(char *output){
	//redirect stdout to output file
	if (output != NULL) {
		//debug only
		int file_out = open(output, O_RDWR | O_CREAT | O_TRUNC, 0666);
		//int file_out = open(output, O_WRONLY | O_CREAT | O_TRUNC, 0240);
		if (file_out == -1) {
			printf("cannot open %s for output\n", output);
			fflush(stdout);
			exit(1);
		} else {
			//printf("redirect output success\n");
			int new_out_fd = dup2(file_out, 1);
			if (new_out_fd == -1) {
				perror("dup2() error");
				printf("cannot redirect output\n");
				fflush(stdout);
			}	
		}
	}					
}


void check_background_status(void){
	struct process* prev = NULL;
	int childStatus;

	struct process* list = head;

	while (list != NULL) {
		//process already being killed
		if (list->status == 0) {
			struct process* tmp = list;
			if(list == head) {
				head = list->next;//delete head
				prev = NULL;
			} else {
				prev->next = list->next; //delete terminated process.
			}
			free(tmp);
		//process not being killed
		} else {
			pid_t res = waitpid(list->ID, &childStatus, WNOHANG);
			if (res == -1) {
				perror("waitpid() error");
			} else if (res > 0) {
				int exit_value;
				if (WIFEXITED(childStatus)){
					exit_value = WEXITSTATUS(childStatus);
					printf("Background pid %d is done: exit value %d\n", res, exit_value);
					fflush(stdout);
				} else {
					exit_value = WTERMSIG(childStatus);
					printf("Background pid %d is done: terminated by signal %d\n", res, exit_value);
					fflush(stdout);
				}
				list->status = 0;
			}
			prev = list;
		}
		list = list->next;
	}
}


void SIGTSTP_handler(int sig){
	char* message_on = "\nEntering foreground-only mode (& is now ignored)\n";
	char* message_off = "\nExiting foreground-only mode\n";
	
	if (f_only == 0){
		f_only = 1;
		write(STDOUT_FILENO, message_on, strlen(message_on) + 1);
		fflush(stdout);
	} else {
		f_only = 0;
		write(STDOUT_FILENO, message_off, strlen(message_off) + 1);
		fflush(stdout);
	}
}


int check_special_cases(char *command_line){
	//check empty command
	if (strlen(command_line) == 0) {
		free(command_line);
		return 1;
	//check newline/comment line
	} else if (strncmp(command_line, COMMENT, 1) == 0 \
			|| strncmp(command_line, NEWLINE, 1) == 0){
		free(command_line);	
		return 1;
	} else {
		return 0;
	}
}


void redirect_bg_process(struct command* currCd){
	//redirect input and output
	if (currCd->input == NULL){
		char *default_in = "/dev/null";
		redirect_input(default_in);
	} else {
		redirect_input(currCd->input);
	}
	if (currCd->output == NULL) {
		char *default_out = "/dev/null";
		redirect_output(default_out);
	} else {
		redirect_output(currCd->output);					
	}
}


void redirect_fg_process(struct command* currCd){
	redirect_input(currCd->input);
	redirect_output(currCd->output);					
}


void parent_behavior_for_fg_child(pid_t spawnpid){
	int childStatus;
	waitpid(spawnpid, &childStatus, 0);
	//set exit value
	if (WIFEXITED(childStatus)){
		fg = WEXITSTATUS(childStatus);
		sig = 0;//not stop by signal 
	} else {
		fg = WTERMSIG(childStatus);
		sig = 1;//stop by signal
		printf("terminated by signal %d\n", fg);
		fflush(stdout);
	}

}


void parent_behavior_for_bg_child(pid_t spawnpid){
	printf("background pid is %d\n", spawnpid);
	fflush(stdout);
	
	//add in background process list
	struct process* new_pid = malloc(sizeof(struct process));
	new_pid->ID = spawnpid;
	new_pid->status = -1;
	new_pid->next = NULL;
	
	//build linked list of background processes
	if (head == NULL){
		head = new_pid;
		tail = new_pid;
	} else {
		tail->next = new_pid;
		tail = new_pid;
	}
}


void free_memory(struct command* currCd){
	//free memory
	for (int i = 0; i < currCd->argc; i++){
		free(currCd->argv[i]);
	}		
	free(currCd->input);
	free(currCd->output);
	free(currCd->name);	
	free(currCd);
}


int main(void){
	
	//initialize signal action
	struct sigaction SIGINT_act = {0}; 
	struct sigaction SIGTSTP_act = {0}; 
	
	//handle SIGINT signal
	SIGINT_act.sa_handler = SIG_IGN;	
	sigfillset(&SIGINT_act.sa_mask);
	SIGINT_act.sa_flags = 0;
	sigaction(SIGINT, &SIGINT_act, NULL);
	
	//handle SIGTSTP signal
	SIGTSTP_act.sa_handler = SIGTSTP_handler;	
	sigfillset(&SIGTSTP_act.sa_mask);
	SIGTSTP_act.sa_flags = 0;
	sigaction(SIGTSTP, &SIGTSTP_act, NULL);
	
	while(1){
		//check background running process 
		check_background_status();
			
		//prompt user for command
		char *command_line = get_command();
		
 		int res = check_special_cases(command_line);
		if (res == 1){
			continue;
		}		
		
		//parse command
		struct command *currCd = parse_command(command_line);	
		
		if(currCd == NULL){
			continue;	
		}
		
		//build-in command: exit all the process
		if (strcmp(currCd->name, EXIT) == 0){
			free_memory(currCd);
			if (atexit(all_exit) == 0){
				exit(EXIT_SUCCESS);
			} else {
				exit(EXIT_FAILURE);
			}
		
		//build-in command: change directory
		} else if (strcmp(currCd->name, CHDIR) == 0){
			change_dir(currCd->argc, currCd->argv[0]);
		
		//build-in command: get exit status
		} else if (strcmp(currCd->name, STATUS) == 0){
			//keep track of last foreground process
			get_status(fg);	
		
		//all the other commands			
		} else {
			//prepare arguments vector for execvp()
			int len = currCd->argc + 2;
			char *child_argv[len];

			for (int i = 0; i < len; i ++){
				if (i == 0){
					child_argv[i] = currCd->name;
				} else if (i == len - 1) {
					child_argv[i] = NULL;
				} else {
					child_argv[i] = currCd->argv[i - 1]; 
				}
			}
		
			//create child process
			pid_t spawnpid = -5; 
			spawnpid = fork();
			switch (spawnpid){
				case -1:
					//in parent process
					perror("fork failed");
					exit(1);
					break;
				
				case 0:
					//in child process
						
					//handle SIGTSTP signal, inherit SIGINT from parent
					SIGTSTP_act.sa_handler = SIG_IGN;	
					sigaction(SIGTSTP, &SIGTSTP_act, NULL);//register SIGINT handling
					
					//redirect input/output						
					if (currCd->bg == 1){
						redirect_bg_process(currCd);
					} else {
						//handle SIGINT signal
						SIGINT_act.sa_handler = SIG_DFL;	
						sigaction(SIGINT, &SIGINT_act, NULL);//register SIGINT handling
						redirect_fg_process(currCd);				
					}
					
					//run command in currCd->name 
					execvp(currCd->name, child_argv);
					perror(currCd->name);
					exit(1);
				
				default:;
					//in parent process
					//register SIGINT and SIGTSTP signals
					sigaction(SIGINT, &SIGINT_act, NULL);
					sigaction(SIGTSTP, &SIGTSTP_act, NULL);
					
					//foreground child
					if (currCd->bg == 0) {
						parent_behavior_for_fg_child(spawnpid);
					//child in background
					} else {
						parent_behavior_for_bg_child(spawnpid);
					}
				}
			}
		//free memory
		free_memory(currCd);
	}
}
