#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>

#define MAX_INPUT 1024
#define MAX_ARGS 6

double last_time = 0.00000;
double min_time = 1.00000;
double max_time = 0.00000;
double avg_time = 0.00000;
double total_time = 0.00000;
int cmdCount = 0;
int dcbc = 0;
char **args;

typedef struct{
	int seconds;
	int nanoseconds;
}tv_time;

void printPrompt(){    // Displays the prompt
    printf("#cmd:%d|#dangerous_cmd_blocked:%d|last_cmd_time:%.5f|avg_time:%.5f|min_time:%.5f|max_time%.5f >> ", 
    cmdCount, dcbc, last_time, avg_time, min_time, max_time);
    fflush(stdout);
}
//  Reads lines from file into a dynamic array of strings
int readFile(const char *filename, char ***commands){   
    FILE *file = fopen(filename, "r");  // Open file for reading
    
    if (!file){ // File didnt open
        perror("Error opening file");
        return -1;
    }
    
    char **lines = NULL; // Dynamic array of lines
    int count = 0;  // Number of lines read
    char buffer[MAX_INPUT]; // Temp buffer for reading lines
    
    while(fgets(buffer, MAX_INPUT, file)){     // Read each line
        char *line = strdup(buffer);    // Duplicate the line    
        
        if (!line){ // Memory allocation failed 
            perror("Memory allocation failed");
            fclose(file);
            return -1;
        }
        
        char **temp = realloc(lines, (count + 1) * sizeof(char *)); // Resize lines array
        
        if (!temp){ // Resize failed
            perror("Realloc failed");
            free(line);
            fclose(file);
            return -1;
        }
        
        lines = temp; // Update pointer
        lines[count++] = line; // Add the new line in to lines
    }
    
    // Reading the file ended
    fclose(file); // Close the file
    *commands = lines;  // Output the array of lines
    return count;   // Return the number of lines read      
}

void getUserInput(char *input, int max_length){
    fgets(input, max_length, stdin); // Read user input 
    input[strcspn(input, "\r\n")] = '\0'; // Remove '\r\n' 
}

// Check the input for dangerous commands
int searchLines(const char *input, char **lines, int count){    
    // Take the first string from the input and put in to inputWord
    char inputWord[MAX_INPUT];
    sscanf(input, "%s", inputWord); 
    for(int i = 0; i < count; i++){
	lines[i][strcspn(lines[i], "\n")] = '\0';// Remove newline symbol
        if (strcmp(lines[i], input) == 0)return 2; // The line is an exact command
        //Take the first string from lines[i] and put in to lineWord
        char lineWord[MAX_INPUT];
        sscanf(lines[i], "%s", lineWord); 
        if (strcmp(lineWord, inputWord) == 0) return 1; // The first word is a match
    }
    
    return 0; // No full or partial match found
}

char *simular_to(const char *input, char **lines, int count){         
    // Take the first string from the input and put in to inputWord
	char inputWord[MAX_INPUT];
    	sscanf(input, "%s", inputWord);

    for(int i = 0; i < count; i++){
        //Take the first string from lines[i] and put in to lineWord
        char lineWord[MAX_INPUT];
        sscanf(lines[i], "%s", lineWord);
        if (strcmp(lineWord, inputWord) == 0)
	{
		return lines[i]; // The first word is a match
	}
    }
    return NULL; // No full or partial match found
}

//  Frees memeory allocated for lines read from file
void freeLines(char **lines, int count){
    for(int i = 0; i < count; i ++){
        free(lines[i]);     // Free each line
    }
    free(lines);    // Free the lines array
}


int run_command(char **args,int argc){
	pid_t pid = fork();
	if (pid<0)
	{
		perror("fork");
		return -1;
	}
	if(pid == 0){
		execvp(args[0],args);
		perror(args[0]);
		exit(1);
	}
	int status;
	waitpid(pid, &status, 0);
	if(WIFEXITED(status))
	{
		int code = WEXITSTATUS(status);
		if(code !=0)
		{
		       	fprintf(stderr,"process exited with error code : %d\n",code);
			
		}
	}
	else if(WIFSIGNALED(status))
	{
		fprintf(stderr,"terminated by signal: %s\n",strsignal(WTERMSIG(status)));
	}
return 0;
}

int pipe_run_command(char **args1, char **args2){
        int fd[2];
	//checks for good pipe
	if(pipe(fd)==-1){
		perror("pipe");
		return -1;
	}

	pid_t pid1 = fork();
        if (pid1<0)
        {
                perror("fork");
                return -1;
        }

	// child procces
        if(pid1 == 0){
        
		dup2(fd[1],STDOUT_FILENO);
		close(fd[0]);
	        close(fd[1]);
        	execvp(args1[0],args1);
		perror(args1[0]);
		exit(1);
        }

	//checks second child procces is good
	pid_t pid2 = fork();
	if(pid2 < 0)
	{
		perror("fork");
		return -1;
	}

	if(pid2==0)//second child procces
	{
		dup2(fd[0],STDIN_FILENO);
		close(fd[0]);
		close(fd[1]);
		execvp(args2[0],args2);
		perror(args2[0]);
		exit(1);
	}
	close(fd[0]);
	close(fd[1]);

	int status;
	waitpid(pid1,NULL,0);
	if(WIFSIGNALED(status))
		fprintf(stderr, "terminated by signal: %s\n", strsignal(WTERMSIG(status)));

	waitpid(pid2, &status,0);
	
	if(WIFEXITED(status)){
		int code = WEXITSTATUS(status);
		if(code!=0)
			fprintf(stderr, "process exited with error code: %d\n",code);
	}
	else if(WIFSIGNALED(status))
		fprintf(stderr, "terminated by signal: %s\n", strsignal(WTERMSIG(status)));

        return 0;	
}

int run_time(char **args1,char **args2,int argc1,int argc2,char *logfile, int ispipe, int backGround){
	//printf("im in the runtime function\n");
	struct timespec start,end;
	clock_gettime(CLOCK_MONOTONIC,&start);
	if(ispipe){
		pipe_run_command(args1, args2);
	}
	else
	{
		run_command(args1, argc1);
	}
	clock_gettime(CLOCK_MONOTONIC,&end);
	double last_time = (end.tv_sec + end.tv_nsec/1000000000.0) - (start.tv_sec + start.tv_nsec/1000000000.0);	

	if(last_time<min_time) min_time=last_time;
	if(last_time>max_time) max_time=last_time;

	avg_time +=last_time;
	cmdCount++;
	avg_time=avg_time/(cmdCount+dcbc);
	FILE *log = fopen(logfile,"a");
        if (!log)
        {
                perror("error in opening file");
        	return -1;
        }
	fprintf(log,"%f\n ",last_time);
        fclose(log);
	return 0;
}

int ERR_SPACE(char *input){
	int str_len = strlen(input);
	for(int i = 0;i < str_len;i++)
	{
		if(input[i]==' ' && input[i+1] == ' ')
		{
			printf("ERR_SPACES\n");
			return -1;
		}
	}
	return 0;
}

int devide_command(char *input,char **args,int *argc){
        int argCount=0;
        char *token = strtok(input," \t\n");
	while (token!= NULL && argCount < MAX_ARGS+1)
        {
        	args[argCount++] = token;
		token = strtok(NULL," \t\n");
        }
        if(token!=NULL){
	        printf(" err_args\n");
                return -1;
	}
        args[argCount]=NULL;
        *argc = argCount-1;
	printf("this is args before the if satatement: %s \n",args[argCount-1]);
	if(strcmp("&",args[argCount-1]))
	{
		printf("this is args after the statement: %s\n",args[argCount-1]);
		printf("im in the first if command\n and should return 1");
		args[argCount-1]=NULL;
		return 1;
	}
	wait(NULL);
        return 0;
}


int main(int argc, char *argv[]){
    if (argc != 3){
        fprintf(stderr, "Usage: %s <dangerous_commands.txt> <exec_times.log>\n", argv[0]);
        return 1;
    }
    
    char *args[MAX_ARGS + 2];   // Array to store command args
    int argCount;
    char **lines = NULL;    // Dangerous commands
    char input[MAX_INPUT];  // Buffer for user input
    const char *filepath = argv[1]; // Path to dangerous commands file
    int lineCount = readFile(filepath, &lines); // Load dangerous commands
    char *args1[MAX_ARGS+2];
    char *args2[MAX_ARGS+2];
    int backGround = 0;

    while(1){
        printPrompt();  // Disply the prompt
	getUserInput(input, MAX_INPUT);// Get users commands
	double time;
	if(lineCount< 0)break;
	if(strlen(input) == 0) continue;// Empty string
	if(strcmp(input, "done") == 0){ 
		printf("amount of dangerouse command blocked: %d\n and number of commands ran are: %d\n",dcbc, cmdCount);
		break;    // Exit condition
	}
	int searchResults = searchLines(input, lines, lineCount);   // Check if input is dangerouse
	char input_copy[MAX_INPUT];
	strcpy(input_copy,input);
	if (devide_command(input_copy,args,&argCount)==-1)
	{	
		continue;
	}
        if (ERR_SPACE(input)==-1) continue;
	char *command = simular_to(input,lines,lineCount);
        if (searchResults == 2){    // If input is identical to the dangerous command, block it.
           printf("ERR: Dangerous command detected (\"%s\"). Execution prevened.\n", command);
            dcbc++; // Update dangerous command count.
            continue;
        }
        else if(searchResults == 1){    // If input is similar to the dangerous command, flag warning.
            printf("WARNING: Command similar to dangerous command (\"%s\"). Proceed with caution.\n", command);
	    dcbc++;
	    cmdCount--;
        }
	
	char *pipe_pos = strchr(input,'|');
	if(pipe_pos)
	{
		*pipe_pos = '\0';
		char *input1 = input;
    		char *input2 = pipe_pos+3;

    		int argCount1 = 0;
    		int argCount2 = 0;
		char *args1[MAX_ARGS+2];
    		char *args2[MAX_ARGS+2];	
		
		if(devide_command(input1,args1, &argCount1)== -1 || devide_command(input2,args2, &argCount2)== -1){
			continue;
		}
		if(devide_command(input1,args1, &argCount1) == 1 || devide_command(input2, args2, &argCount2) == 1)
		{
			backGround = 1;
			printf("im in the second if command \n");
			run_command(input1, argCount1);
			run_command(input2, argCount2);
			time = run_time(args1, args2, argCount1, argCount2, argv[2], 1, backGround);
				
		}
		continue;
	}
	devide_command(input, args, &argCount);
	char **argsPlaceHolder = NULL;
	int argCountPlaceHolder = 0;
	int is_pipe = 0;
	time = run_time(args, argsPlaceHolder, argCount, argCountPlaceHolder, argv[2], is_pipe, backGround);    	
    }
    freeLines(lines, lineCount);
    return 0;
}
