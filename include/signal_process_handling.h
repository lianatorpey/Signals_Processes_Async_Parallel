/*
	Contains structure for work queue and stack for tree traversal
*/
#ifndef CONST_H
#define CONST_h

#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#include "cookbook.h"
#include "stack_queue_tree_traversal.h"

void main_processing_loop(WORK_QUEUE *work_queue, int max_cooks, COOKBOOK *cookbook, RECIPE *recipe_selected, RECIPE **completed_recipes);

void sigchld_handler(int sig);
void sigchld_handler_cook(int sig);

RECIPE *get_recipe_by_pid(COOKBOOK *cookbook, pid_t pid);
pid_t get_pid_of_recipe(RECIPE *recipe);
void set_pid_of_recipe(RECIPE *recipe, pid_t pid);

int execute_task(TASK *task);

void update_work_queue_signal_block(WORK_QUEUE *work_queue);
/*
	Functions just for debugging purposes like printing functions
*/
void print_step_words(STEP *step);

#endif