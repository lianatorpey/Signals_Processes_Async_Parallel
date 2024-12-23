/*
	This is the c file that contains the driver code for the process management and signal handler code
	Mainly this will be the primary caller and return function
*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <string.h>

#include "cookbook.h"
#include "stack_queue_tree_traversal.h"

// Recursive helper function to initialize the state of each recipe and its dependencies.
void initialize_recipe_states(RECIPE *recipe) {
    while (recipe != NULL) {
    	// fprintf(stderr, "%s\n", recipe->name);
        recipe->state = NULL;
        recipe = recipe->next;
    }
}

// Function to initialize all states in the cookbook.
void initialize_cookbook_states(COOKBOOK *cookbook) {
    if (cookbook == NULL) return;

    cookbook->state = NULL;

    // Initialize all recipes in the cookbook.
    initialize_recipe_states(cookbook->recipes);
}

/*
	Function to validate the arguments passed on the command line
	From the README.md assignment description, all of the parseable flags are optional
	If argument is missing then default values are used as defined

	char *cookbook = "/cookbook.ckb";
    char *recipe_name = "";

    return the number of max cooks
*/
int validargs(char **cookbook, char **recipe_name, int argc, char **argv) {
	int max_cooks = 1;
	int recipe_name_parsed = 0;

	for (int i = 1; i < argc; i++) { // argument 0 is the name of the executable
		if (strcmp(argv[i], "-f") == 0) {
			if (i + 1 < argc) {
				*cookbook = argv[i + 1];
				i++;
			} else {
				fprintf(stderr, "ERROR: -f flag was passed but the cookbook name was not given. \n");
				return -1;
			}
		} else if (strcmp(argv[i], "-c") == 0) {
			if (i + 1 < argc) {
				max_cooks = atoi(argv[i + 1]);
				i++;
			} else {
				fprintf(stderr, "ERROR: -c flag was passed but max_cooks number was not given. \n");
				return -1; // the -f flag was passed but the cookbook name was not given
			}
		} else {
			if (recipe_name_parsed) {
				fprintf(stderr, "ERROR: There was already a recipe name provided. \n");
				return -1;
			}
			*recipe_name = argv[i];
			recipe_name_parsed = 1;
		}
	}


	if (max_cooks <= 0) {
		fprintf(stderr, "ERROR: Invalid number of cooks specified. \n");
		return -1;
	}

	return max_cooks;
}

/*
	Function to find a recipe by name in the COOKBOOK structure
	If recipe_name is empty or NULL, returns the first recipe in the cookbook

	Returns pointer to the matching recipe that was found (if NULL then recipe is not found and error handled)
*/
RECIPE *find_recipe(COOKBOOK *cookbook, const char *recipe_name) {
	if (cookbook == NULL || cookbook->recipes == NULL) {
		fprintf(stderr, "ERROR: Selected Recipe is not in the parsed cookbook. \n");
		return NULL;
	}

	// if the recipe_name is empty or it is null then return the first recipe in cookbook
	if (recipe_name == NULL || strcmp(recipe_name, "") == 0) {
		return cookbook->recipes;
	}

	// iterate through list of recipes in cookbook and find the first one with the matching name
	RECIPE *current_recipe = cookbook->recipes;
	while (current_recipe != NULL) {
		if (current_recipe->name != NULL && strcmp(current_recipe->name, recipe_name) == 0) {
			return current_recipe;
		}
		current_recipe = current_recipe->next;
	}

	return NULL;
}

/*
	Functions to perform tree traversal and perform recursive analysis with a stack
*/
void push(STACK *stack, RECIPE *recipe) {
	// fprintf(stderr, "ALLOCATE THE STACK ELEMENT PUSH!\n");
	STACK_NODE *new_node = malloc(sizeof(STACK_NODE));
	new_node->recipe = recipe;
	new_node->next = stack->top;
	stack->top = new_node;
}
RECIPE *pop(STACK *stack) {
	if (stack->top == NULL) return NULL;
	STACK_NODE *node = stack->top;
	RECIPE *recipe = node->recipe;
	stack->top = node->next;
	// fprintf(stderr, "FREE THE STACK ELEMENT POP\n");
	free(node);
	return recipe;
}
int is_stack_empty(STACK *stack) {
	return stack->top == NULL;
}
void mark_visited(RECIPE *recipe) {
	recipe->state = (void *)1;
}
int is_visited(RECIPE *recipe) {
	return recipe->state != NULL;
}

/*
	Function to perform stack tree recursive traversal: covers dependency analysis phase
	identifies all sub recipes needed to complete the main recipe
	populates the work queue with leaf recipe nodes that have no dependencies (resolves interlinked dependencies)

	starting from the main recipe - traversal explores each recipe dependencies sub recipes
	uses a STACK to manage the traversal pushing recipes onto the stack as it encouners new recipes
	For each recupe it checks if it has already been visited using the state field and marks if not
	any recipe with no further dependencies (this_depends_on == NULL) is a leaf node and is added to WORK_QUEUE

	work queue after the traversal only contains the leaf recipes
	recipes meet the outlined criteria (1) required by main recipe (2) reach for task processing since no pending dependencies (3) not yet processed
*/
int stack_analysis_traversal(RECIPE *recipe_selected, WORK_QUEUE *work_queue) {
	STACK stack = { NULL }; // initialize the stack for recursive tree traversal

	push(&stack, recipe_selected);
	int recipe_count = 1;

	while (!is_stack_empty(&stack)) {
		RECIPE *current = pop(&stack);

		if (is_visited(current)) continue;
		mark_visited(current);

		// check if the current recipe is a leaf node, there are no dependencies
		if (current->this_depends_on == NULL) {
			enqueue(work_queue, current);
		}

		// traverse dependencies
		RECIPE_LINK *dep = current->this_depends_on;
		while (dep != NULL) {
			if (!is_visited(dep->recipe)) {
				push(&stack, dep->recipe);
				recipe_count++;
			}
			dep = dep->next;
		}
	}

	// after traversal reset visited status in all nodes of the tree
	initialize_recipe_states(recipe_selected);

	// Just checking the contents of the stack
    // print_stack(&stack);
    return recipe_count;
}

int detect_cycle_dfs(RECIPE *recipe, STACK *stack) {
    if (is_visited(recipe)) {
        return 0; // Already processed, no cycle detected
    }

    // checks for self dependency
    RECIPE_LINK *link = recipe->this_depends_on;
    while (link != NULL) {
    	if (link->recipe == recipe) {
    		fprintf(stderr, "ERROR: Self-Dependency detected in recipe \n");
    		return -1;
    	}
    	link = link->next;
    }

    // Check if the recipe is already in the visiting stack (cycle detected)
    STACK_NODE *node = stack->top;
    while (node != NULL) {
        if (node->recipe == recipe) {
            fprintf(stderr, "ERROR: Circular Dependency Caught in Tree Cookbook Data Structure\n");
            return -1;
        }
        node = node->next;
    }

    // Mark as visiting by pushing onto the stack
    push(stack, recipe);

    // Traverse dependencies
    RECIPE_LINK *dep = recipe->this_depends_on;
    while (dep != NULL) {
    	if (dep->recipe == NULL) {
    		fprintf(stderr, "ERROR: Missing recipe dependency in recipe\n");
    		return -1;
    	}
        detect_cycle_dfs(dep->recipe, stack);
        dep = dep->next;
    }

    // Mark as fully processed and remove from visiting stack
    mark_visited(recipe);
    pop(stack);

    return 0;
}

int check_circular_tree_cycle(RECIPE *recipe_root) {
    STACK visiting_stack = { NULL }; // Initialize an empty stack
    int ret = detect_cycle_dfs(recipe_root, &visiting_stack);
    initialize_recipe_states(recipe_root);
    return ret;
}

// Helper function to check if a recipe is in the completed_recipes list
int is_in_completed_recipes(RECIPE *recipe, RECIPE **completed_recipes, int completed_count) {
	// fprintf(stderr, "(iterations: %d) CHECKING IF RECIPE (%s) IS COMPLETED: \n", completed_count, recipe->name);
    for (int i = 0; i <= completed_count - 1; i++) {
        if (completed_recipes[i] == recipe) {
        	// fprintf(stderr, "Returning TRUE\n");
            return 1;
        }
    }
    // fprintf(stderr, "Returning FALSE\n");
    return 0;
}

int is_reaches_main(RECIPE *recipe_completed, RECIPE *main_recipe) {
    // Base case: if recipe_completed is NULL, return false
    if (recipe_completed == NULL) {
        return 0;
    }

    if (recipe_completed == main_recipe) return 1;

    // Traverse the 'depend_on_this' list
    RECIPE_LINK *current = recipe_completed->depend_on_this;
    while (current != NULL) {
        // Check if the current recipe matches main_recipe
        if (current->recipe == main_recipe ||
            is_reaches_main(current->recipe, main_recipe)) {
            return 1;
        }
        current = current->next; // Move to the next dependency
    }

    // fprintf(stderr, "no match found return false\n");
    // If no match is found, return false
    return 0;
}

/*
	Function that updates the work queue
	It inputs a list of completed recipes []
	And then it uses the stack data structure again to traverse the cookbok tree
	It looks at depend_on_this  attribute to see which subrecipes can be completed now that the passed in subrecipe has completed
	It then adds the subrecipes that can be completed now to the work queue
	The work queue should have now updated sub recipes that can be completed because of the completed recipes
*/
void update_work_queue(WORK_QUEUE *work_queue, RECIPE **completed_recipes, int completed_count, RECIPE *main_recipe) {
/*
	fprintf(stderr, "UPDATING THE WORK QUEUE AFTER COMPLETETION (%d) \n", completed_count);
	print_queue(work_queue);

	fprintf(stderr, "******************************************\n");
	fprintf(stderr, "Completed count: %d\n", completed_count);
	fprintf(stderr, "Completed recipes: \n");
	for (int i = 0; i < completed_count; i++) {
		RECIPE *recipe = completed_recipes[i];
		fprintf(stderr, "Recipe %d: Name = %s\n", i, recipe->name);
	}
	fprintf(stderr, "******************************************\n");
*/

    // Step 1: Create a temporary stack for traversal
    STACK stack = { NULL };

    // below should only look at most recently finished recipe
    RECIPE *completed_recipe = completed_recipes[completed_count-1];
    dequeue_recipe(work_queue, completed_recipe);

    // if (completed_recipe == main_recipe) return;

    RECIPE_LINK *dependent = completed_recipe->depend_on_this;

    while (dependent != NULL) {
    	// fprintf(stderr, "MEEP\n");
        // push each dependent recipe onto the stack to expand and explore further
        if (!is_in_completed_recipes(dependent->recipe, completed_recipes, completed_count) &&
        	is_reaches_main(dependent->recipe, main_recipe)) {
        	// fprintf(stderr, "RETURNS HERE\n");
        	push(&stack, dependent->recipe);
        }
        dependent = dependent->next;
    }


    // fprintf(stderr, "STACK UPDATED\n");
    // print_stack(&stack);
/*
    fprintf(stderr, "******************************************\n");
    fprintf(stderr, "STACK SHOULD HAVE BEEN UPDATED!\n");
    print_stack(&stack);
    fprintf(stderr, "******************************************\n");
*/

    // Step 3: Traverse the stack to check each dependent recipe's dependencies
    while (!is_stack_empty(&stack)) {
    	// fprintf(stderr, "while stack is not empty\n");
        RECIPE *recipe = pop(&stack);
        // fprintf(stderr, "after stack operation\n");
        // print_stack(&stack);

        if (is_visited(recipe)) {
        	// fprintf(stderr, "Recipe is visited its state is not null\n");
        	continue;
        }

        // Check if all dependencies for this recipe are in completed_recipes
        int can_be_completed = 1;
        RECIPE_LINK *dep = recipe->this_depends_on;

        while (dep != NULL) {
        	// fprintf(stderr, "merp\n");
            if (!is_in_completed_recipes(dep->recipe, completed_recipes, completed_count)) {
            	// fprintf(stderr, "whelp\n");
                can_be_completed = 0;
                break;
            }
            // fprintf(stderr, "here we are\n");
            dep = dep->next;
        }

        if (can_be_completed) {
        	// fprintf(stderr, "hello hello hello\n");
            enqueue(work_queue, recipe);

            // Also, check if any recipes dependent on this one can now be completed
            RECIPE_LINK *next_dependent = recipe->depend_on_this;
            while (next_dependent != NULL) {
                if (!is_visited(next_dependent->recipe)) {
                	// fprintf(stderr, "check this\n");
                    push(&stack, next_dependent->recipe);
                }
                next_dependent = next_dependent->next;
            }
        }
    }

/*
    fprintf(stderr, "MAKE SURE THE STACK IS EMPTY?\n");
    print_stack(&stack);

    // Step 4: The work queue now contains the union of original leaf nodes and new ready subrecipes
    fprintf(stderr, "******************************************\n");
    fprintf(stderr, "WORK QUEUE SHOULD BE UPDATED!\n");
    print_queue(work_queue);
    fprintf(stderr, "******************************************\n");
*/
}

/*
	Functions to use the work queue
*/
WORK_QUEUE *init_work_queue() {
	WORK_QUEUE *queue = malloc(sizeof(WORK_QUEUE));
	queue->front = NULL;
	queue->back = NULL;
	return queue;
}
void enqueue(WORK_QUEUE *queue, RECIPE *recipe) {
	QUEUE_NODE *new_node = malloc(sizeof(QUEUE_NODE));
	new_node->recipe = recipe;
	new_node->next = NULL;

	if (queue->back == NULL) {
		queue->front = new_node;
		queue->back = new_node;
	} else {
		queue->back->next = new_node;
		queue->back = new_node;
	}
}
RECIPE *dequeue_recipe(WORK_QUEUE *queue, RECIPE *target_recipe) {
	// fprintf(stderr, "REMOVING A SPECIFIC RECIPE FROM WORK QUEUE\n");
	if (queue->front == NULL) return NULL;

	QUEUE_NODE *current = queue->front;
	QUEUE_NODE *previous = NULL;

	while (current != NULL) {
		if (current->recipe == target_recipe) {
			// Found the target recipe to remove
			if (previous == NULL) {
				// fprintf(stderr, "meep\n");
				// Removing the front of the queue
				queue->front = current->next;
			} else {
				// fprintf(stderr, "merp\n");
				// Removing a recipe in the middle or end
				previous->next = current->next;
			}

			// If the target recipe is at the back, update the back pointer
			if (current->next == NULL) {
				queue->back = previous;
			}

			RECIPE *recipe = current->recipe;
			free(current);
			return recipe;
		}

		// Move to the next node
		previous = current;
		current = current->next;
	}

	// If we reach here, the recipe was not found in the queue
	return NULL;
}
RECIPE *dequeue(WORK_QUEUE *queue) {
	if (queue->front == NULL) return NULL;
	QUEUE_NODE *node = queue->front;
	RECIPE *recipe = node->recipe;
	queue->front = node->next;

	if (queue->front == NULL) {
		queue->back = NULL;
	}

	free(node);

	return recipe;
}
int is_work_queue_empty(WORK_QUEUE *queue) {
	return queue->front == NULL;
}
int is_ready_for_work_queue(RECIPE *recipe) {
	return (long)recipe->state == 0;
}

/*
	Set a recipe's initial dependency count, using state as counter
*/
void initialize_dependency_count(RECIPE *recipe) {
	int count = 0;
	RECIPE_LINK *dep = recipe->this_depends_on;
	while (dep != NULL) {
		count++;
		dep = dep->next;
	}
	recipe->state = (void *)(long)count;
}

// Helper function print the stack
void print_stack(STACK *stack) {
	STACK_NODE *current = stack->top;
	fprintf(stderr, "PRINTING the CONTENTS of the STACK!\n");
	while (current != NULL) {
		fprintf(stderr, "Recipe: %s\n", current->recipe->name);
		current = current->next;
	}
}
// Helper function print the queue
void print_queue(WORK_QUEUE *queue) {
	QUEUE_NODE *current = queue->front;
	fprintf(stderr, "PRINTING the CONTENTS of the WORK QUEUE!\n");
	while (current != NULL) {
		fprintf(stderr, "Recipe: %s\n", current->recipe->name);
		current = current->next;
	}
}

// Helper function to free all steps in a task
void free_steps(STEP *step) {
	// fprintf(stderr, "freeing steps\n");
    while (step != NULL) {
        STEP *next_step = step->next;
        if (step->words) {
            char **words = step->words;
            while (*words) {
                free(*words);
                *words = NULL;
                words++;
            }
            free(step->words);
            step->words = NULL;
        }
        free(step);
        step = next_step;
    }
}

// Helper function to free all tasks in a recipe
void free_tasks(TASK *task) {
	// fprintf(stderr, "freeing tasks\n");
    while (task != NULL) {
        TASK *next_task = task->next;
        free_steps(task->steps);
        if (task->input_file) {
            free(task->input_file);
            task->input_file = NULL;
        }
        if (task->output_file) {
            free(task->output_file);
            task->output_file = NULL;
        }
        free(task);
        task = next_task;
    }
}

// Helper function to free all recipe links
void free_recipe_links(RECIPE_LINK *link, int y) {
	// fprintf(stderr, "freeing recipe link\n");
    while (link != NULL) {
        RECIPE_LINK *next_link = link->next;
        if (y) {
		    if (link->name!=NULL) {
		    	// fprintf(stderr, "freeing (%s) link @ (%p)\n", link->name, link->name);
		        free(link->name);
		        link->name = NULL;
		    }
    	} else {

    	}
        free(link);
        link = next_link;
    }
    // fprintf(stderr, "FINISH\n");
}

// Helper function to free all recipes in a cookbook
void free_recipes(RECIPE *recipe) {
	// fprintf(stderr, "freeing recipe\n");
    while (recipe != NULL) {
        RECIPE *next_recipe = recipe->next;
        if (recipe->name) {
        	// fprintf(stderr, "freeing recipe name ************ (%s)\n", recipe->name);
            free(recipe->name);
            recipe->name = NULL;
        }

        // fprintf(stderr, "freeing depend on this list\n");
        free_recipe_links(recipe->depend_on_this, 0);

        // fprintf(stderr, "freeing this depends on list\n");
        free_recipe_links(recipe->this_depends_on, 1);

        free_tasks(recipe->tasks);

        if (recipe->state) {
        	// fprintf(stderr, "freeing recipe state\n");
        	free(recipe->state);
        	recipe->state = NULL;
    	}
        free(recipe);
        recipe = next_recipe;
    }
}

// Function to free a complete cookbook structure
void free_cookbook(COOKBOOK *cookbook) {
	// fprintf(stderr, "freeing the cookbook\n");
    if (cookbook == NULL) {
    	// fprintf(stderr, "cookbook was null so don't free\n");
        return;
    }
    free_recipes(cookbook->recipes);
    if (cookbook->state) {
    	// fprintf(stderr, "freeing cookbook state\n");
        free(cookbook->state);
        cookbook->state = NULL;
    }
    // fprintf(stderr, "freeing cookbook struct\n");
    free(cookbook);
}