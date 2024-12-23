/*
	Contains structure for work queue and stack for tree traversal
*/
#ifndef CONST_H
#define CONST_h

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/stat.h>
#include <dirent.h>

#include "cookbook.h"

void initialize_recipe_states(RECIPE *recipe);
void initialize_cookbook_states(COOKBOOK *cookbook);

int validargs(char **cookbook, char **recipe_name, int argc, char **argv);

RECIPE *find_recipe(COOKBOOK *cookbook, const char *recipe_name);

// functions and structs for work queue structure used for maintaining leaf nodes
typedef struct queue_node {
	RECIPE *recipe;
	struct queue_node *next;
} QUEUE_NODE;

typedef struct {
	QUEUE_NODE *front;
	QUEUE_NODE *back;
} WORK_QUEUE;

WORK_QUEUE *init_work_queue();
void enqueue(WORK_QUEUE *queue, RECIPE *recipe);
RECIPE *dequeue_recipe(WORK_QUEUE *queue, RECIPE *target_recipe); // remove specific recipe from queue
RECIPE *dequeue(WORK_QUEUE *queue);
int is_work_queue_empty(WORK_QUEUE *queue);
int is_ready_for_work_queue(RECIPE *recipe);

void initialize_dependency_count(RECIPE *recipe);
void mark_completed(RECIPE *recipe);

// functions and structs for stack data structure used for analysis in tree traversal - recursive analysis
typedef struct stack_node {
	RECIPE *recipe;
	struct stack_node *next;
} STACK_NODE;

typedef struct {
	STACK_NODE *top;
} STACK;

void push(STACK *stack, RECIPE *recipe);
RECIPE *pop(STACK *stack);
int is_stack_empty(STACK *stack);
void mark_visited(RECIPE *recipe);
int is_visited(RECIPE *recipe);

int stack_analysis_traversal(RECIPE *recipe_selected, WORK_QUEUE *work_queue);

int check_circular_tree_cycle(RECIPE *recipe_root);
int detect_cycle_dfs(RECIPE *recipe, STACK *stack);

void update_work_queue(WORK_QUEUE *work_queue, RECIPE **completed_recipes, int completed_count, RECIPE *main_recipe);
int is_in_completed_recipes(RECIPE *recipe, RECIPE **completed_recipes, int completed_count);

/*
	Functions for freeing data structures
*/
void free_steps(STEP *step);
void free_tasks(TASK *task);
void free_recipe_links(RECIPE_LINK *link, int y);
void free_recipes(RECIPE *recipe);
void free_cookbook(COOKBOOK *cookbook);

/*
	Functions just for debugging purposes like printing data structures
*/
void print_stack(STACK *stack);
void print_queue(WORK_QUEUE *queue);

#endif