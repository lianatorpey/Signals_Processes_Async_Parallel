#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include "signal_process_handling.h"

#define UTIL_DIR "util/"

// the following two variables are shared variables so must protect all modifications to prevent race conditions
volatile sig_atomic_t active_cooks = 0;
volatile sig_atomic_t completed_count = 0;

RECIPE **completed_recipes;

COOKBOOK *cookbook_pid;
RECIPE *main_recipe;

volatile sig_atomic_t sigchld_flag = 0;

// Function to set the pid in the recipe's state
void set_pid_of_recipe(RECIPE *recipe, pid_t pid) {
    if (recipe == NULL) {
        return;
    }

    // Store the pid in the state
    *(pid_t *)(recipe->state) = pid;
}

// Function to get the pid from the recipe's state
pid_t get_pid_of_recipe(RECIPE *recipe) {
    if (recipe == NULL || recipe->state == NULL) {
        return -1;  // Return an invalid pid value if recipe or state is NULL
    }
    // fprintf(stderr, "Accessing recipe (%s) state (%d) to get pid\n", recipe->name, *(pid_t *)recipe->state);
    return *(pid_t *)(recipe->state);
}

RECIPE *get_recipe_by_pid(COOKBOOK *cookbook, pid_t pid) {
    // fprintf(stderr, "Getting recipe by pid (%d)\n", pid);

    if (cookbook == NULL || cookbook->recipes == NULL) {
        // fprintf(stderr, "Cookbook pointer or cookbook recipes is NULL\n");
        return NULL;
    }

    RECIPE *current = cookbook->recipes;

    while (current != NULL) {
        /*
        fprintf(stderr, "Recipe (%s)", current->name);
        if(current->state!=NULL)
            fprintf(stderr, "state (%d)\n", *(pid_t *)current->state);
        else
            fprintf(stderr, " null\n");
        */

        if (get_pid_of_recipe(current) == pid) {
            // fprintf(stderr, "Found recipe corresponding to pid (%d) \n", pid);
            return current;  // Found the recipe corresponding to the pid
        }
        current = current->next;  // Move to the next recipe in the cookbook
    }

    // fprintf(stderr, "recipe (%s) will be null pid searched (%d) in cookbook (%d) \n", current->name, pid, *(pid_t *)current->state);
    return NULL;  // Return NULL if no matching recipe is found
}

// Signal handler for SIGCHLD to handle completed child processes
void sigchld_handler(int sig) { // for the main cook that is tracking completed recipes
    sigchld_flag++;
}

void sigchld_handler_cook(int sig) {
    // prevents program from crashing
} // for the cooks executing the tasks and steps for recipes

void print_step_words(STEP *step) {
    // fprintf(stderr, "%p\n", step);
    if (step == NULL || step->words == NULL) {
        fprintf(stderr, "ERROR: No words initialized in step\n");
        return;
    }

    char **word = step->words;
    while (*word != NULL) {
        fprintf(stderr, "%s ", *word);
        word++;
    }
    fprintf(stderr, "\n"); // Print newline after all words are printed.
}

// Function to set up and execute a single task's steps in a pipeline
int execute_task(TASK *task) {

    //fprintf(stderr, "PROCESSING THE TASK - Executing Steps: \n");

    int pipe_fds[2], input_fd = -1, output_fd = -1, prev_fd = -1;
    pid_t pid;
    STEP *step = task->steps;

    //fprintf(stderr, "******************************************\n");
    //print_step_words(step);
    //fprintf(stderr, "******************************************\n");

    // Set up input redirection if specified
    // If the task specifies an input file, file descriptor is opened with O_RDONLY and later redirected to the standard input of the first process in pipeline using dup2.
    if (task->input_file) {
        input_fd = open(task->input_file, O_RDONLY);
        if (input_fd < 0) return -1;
    }

    // Set up output redirection if specified - later redirected to the standard output of the last process in the pipeline.
    /*
        O_WRONLY: Open the file for writing.
        O_CREAT: Create the file if it does not exist.
        O_TRUNC: Truncate (empty) the file if it already exists.
        0666: Sets the file's initial permissions = The file must be writable, allowing the output from the process to be stored there (owner, group, others = read/write)
    */
    if (task->output_file) {
        output_fd = open(task->output_file, O_WRONLY | O_CREAT | O_TRUNC, 0666);
        if (output_fd < 0) return -1;
    }

    // Process each step, creating a pipeline - A pipe is created for each step in the task using pipe()
    while (step != NULL) {

        //fprintf(stderr, "PROCESSING THE STEPS OF THE TASK\n");

        pipe(pipe_fds);  // Create a pipe for this step
        pid = fork(); // Each step in the task is executed as a separate child process

        if (pid == 0) {  // Child process - The output of the current process is connected to the input of the next process using dup2

            // Redirect input for the first step
            if (step == task->steps && input_fd != -1) dup2(input_fd, STDIN_FILENO);

            // Redirect output for the last step or pipe to the next step
            if (step->next == NULL && output_fd != -1) dup2(output_fd, STDOUT_FILENO);
            else if (step->next != NULL) dup2(pipe_fds[1], STDOUT_FILENO);

            // Set up input from previous step if needed
            if (prev_fd != -1) dup2(prev_fd, STDIN_FILENO);

            close(pipe_fds[0]); // read end

            close(pipe_fds[1]); // write end

            // Try executing from util directory first, then standard path
            char util_path[256];
            snprintf(util_path, sizeof(util_path), "%s%s", UTIL_DIR, step->words[0]);

            // execvp used to execute the step, first attempting from the util/ directory and then falling back to the system's search path.
            execvp(util_path, step->words);
            execvp(step->words[0], step->words);

            fprintf(stderr, "ERROR: execvp failed on program executable for step both from util path and step->words[]\n");
            exit(EXIT_FAILURE);
        } else { // parent process - waits for each child process to complete
            int status;

            waitpid(pid, &status, WNOHANG); // waiting for child process above to finish
        }

        if (prev_fd != -1) close(prev_fd); // close read end of the pipe from previous step
        close(pipe_fds[1]); // close write end of current pipe
        prev_fd = pipe_fds[0]; // update to read end of current pipe
        step = step->next; // moves to next step in pipeline
    }

    // Wait for all child processes in the pipeline - If any process in the pipeline fails (non-zero exit status or abnormal termination), returns -1, causing the program to terminate
    int pipeline_status = 0; // store status information
    while (wait(&pipeline_status) > 0) { // waits for any child process to terminate and returns child PID (if no child processes left returns -1)
        if (!WIFEXITED(pipeline_status) || WEXITSTATUS(pipeline_status) != 0) return -1; // checks child process terminate normally
        // if process terminated normally extracts exit status, else catches pipeline failed
    }
    return 0;
}

#if 0
void update_work_queue_signal_block(WORK_QUEUE *work_queue) {
    sigset_t block_set, old_set;

    // initialize the signal set to block sigchld
    sigemptyset(&block_set);
    sigaddset(&block_set, SIGCHLD);

    sigprocmask(SIG_BLOCK, &block_set, &old_set);

    update_work_queue(work_queue, completed_recipes, completed_count, main_recipe);

    sigprocmask(SIG_SETMASK, &old_set, NULL);
}
#endif

// Main processing loop
void main_processing_loop(WORK_QUEUE *work_queue, int max_cooks, COOKBOOK *cookbook_parsed, RECIPE *recipe_selected, RECIPE **completed_list) {

    // fprintf(stderr, "Inside the main processing loops\n");

    cookbook_pid = cookbook_parsed;
    main_recipe = recipe_selected;
    completed_recipes = completed_list;

    // main cook gets separate handler with the write permissions to the shared resource
    struct sigaction sa; // structure for signal handling
    sigemptyset(&sa.sa_mask); // initializes the signal mask set in the sa structure to be empty so no signals are blocked while the handler executes
    sa.sa_flags = SA_RESTART; // ensures that interuppted system calls will automatically restart instead of failing with error
    sa.sa_handler = sigchld_handler; // assigns sigchld_handler as the handler for sigchld signal, called when a child process exits for main cook
    sigaction(SIGCHLD, &sa, NULL); // registers the sigchld_handler to handle the sigchld signals

    sigset_t block_mask, orig_mask;
    sigemptyset(&block_mask); // initializes the block mask to an empty set of signals (so can check multiple signals)
    sigaddset(&block_mask, SIGCHLD); // adds sigchld to block_mask allowing it to be blocked or unblocked as needed below

    // Strategy: Have signals masked all the time except for when suspending

    // signals are masked all time in main program (except for in suspend - avoid spinning) - deals with races when signals terminating when suspending
    sigprocmask(SIG_BLOCK, &block_mask, &orig_mask); // blocks sigchld by setting the signal mask to block mask (orig mask stores the previous mask)

    while (1) {

        if (is_work_queue_empty(work_queue) && active_cooks == 0) {
            break; // ending case to end the main processing loop: when there is nothing left to complete in work queue and no active cooks
        }

        if (!is_work_queue_empty(work_queue) && active_cooks < max_cooks) { // the work queue has recipes that need to be execute and there are cooks available

            RECIPE *recipe = dequeue(work_queue);

            if (recipe != NULL) {

                // ensure recipe->state has allocated memory
                if (recipe->state == NULL) {
                    recipe->state = malloc(sizeof(pid_t));
                    if (recipe->state == NULL) {
                        fprintf(stderr, "ERROR: Failed to allocate memory for the recipe state");
                        exit(EXIT_FAILURE);
                    }
                }

                pid_t pid = fork();

                if (pid == 0) { //  child process (returns 0)

                    // child processes get own handler (for each cook doing each recipe it was assigned)
                    // no write permissions to shared resource for this handler (really only here so program does not crash)

                    set_pid_of_recipe(recipe, getpid());

                    TASK *task = recipe->tasks;
                    while (task != NULL) {

                        if (execute_task(task) != 0) exit(EXIT_FAILURE);

                        task = task->next;
                    }
                    exit(EXIT_SUCCESS);

                } else if (pid > 0) { // parent process (returns pid of child)

                    active_cooks++;
                    set_pid_of_recipe(recipe, pid);

                } else { // invalid return for process id - put back stuff into work queue?
                    fprintf(stderr, "ERRROR: Fork failed\n");
                    abort();
                }
            } else {
                // This shouldn't happen because there should be something in the work queue
                abort();
            }

        } else { // at max capacity of active cooks (equal to max cooks) waits for cook to finish
            sigsuspend(&orig_mask); // waits for any unblocked signals to arrive allowing the handler to execute

            int status;
            pid_t pid;

            while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {

                // fprintf(stderr, "Waitpid returns %d and status: %x\n", pid, status);

                active_cooks--;

                if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {

                    RECIPE *recipe = get_recipe_by_pid(cookbook_pid, pid);

                    completed_recipes[completed_count++] = recipe;

                } else {
                    fprintf(stderr, "ERROR: Recipe process %d failed.\n", pid);
                    // send signal to all child processes cooks
                    // gets cooks to kill all signals
                    // Iterate through all recipes in the cookbook and kill their processes
                    while (recipe_selected != NULL) {
                        if (recipe_selected->state != NULL) {
                            pid_t *child_pid = (pid_t *)recipe_selected->state;

                            if (*child_pid != pid) { // Don't send the signal to the failed process
                                if (kill(*child_pid, SIGKILL) == -1) {
                                    fprintf(stderr, "Failed to terminate child process\n");
                                }
                            }
                        }
                        recipe_selected = recipe_selected->next; // Move to the next recipe in the cookbook
                    }

                    // free all the resources and then exit failure
                    // FREE WORK QUEUE STRUCTURE (this is good!)
                    free(work_queue);

                    // FREE LIST STRUCTURE (this is good!)
                    free(completed_recipes); // just a list of pointers

                    // FREE STACK STRUCTURE (should already be free - i think it is malloc in push and freed in push? and also locally defined in stack)

                    // FREE COOKBOOK TREE STRUCTURE
                    free_cookbook(cookbook_parsed);

                    exit(EXIT_FAILURE); // program ends with graceful cleaning up of system resources
                }

            }

            update_work_queue(work_queue, completed_recipes, completed_count, main_recipe); // ensures that all child processes are accounted for

            sigchld_flag--;
        }
    }


    // wrapped while loops with masking and unmasking signals
    sigprocmask(SIG_SETMASK, &orig_mask, NULL); // unblocks sigchld signals so parent process can handle them

/*
    fprintf(stderr, "******************************************\n");
    fprintf(stderr, "Completed count: %d\n", completed_count);
    fprintf(stderr, "Completed recipes: \n");
    for (int i = 0; i < completed_count; i++) {
        RECIPE *recipe = completed_recipes[i];
        fprintf(stderr, "Recipe %d: Name = %s\n", i, recipe->name);
    }
    fprintf(stderr, "******************************************\n");
*/
}