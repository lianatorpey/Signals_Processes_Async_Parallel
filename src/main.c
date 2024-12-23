#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <signal.h>

#include "signal_process_handling.h"

int main(int argc, char *argv[]) {
    /*
    pid_t cpid = getpid();
    pid_t ppid = getppid();
    fprintf(stderr, "AT START! (main process) Current Process ID: %d\n", cpid);
    fprintf(stderr, "AT START! Parent Process ID: %d\n", ppid);
    */
    int err = 0;
    FILE *file_open;

    char *cookbook = "cookbook.ckb";
    char *recipe_name = "";
    int max_cooks = 1;

    // VALIDATE the command line arguments
    if ((max_cooks = validargs(&cookbook, &recipe_name, argc, argv)) == -1) {
        fprintf(stderr, "ERROR: Invalid argument combination passed on command line - failed to validate. \n");
        exit(EXIT_FAILURE);
    }

    // PARSING THE COOKBOOK
    if((file_open = fopen(cookbook, "r")) == NULL) {
       fprintf(stderr, "ERROR: Can't open cookbook '%s': %s\n", cookbook, strerror(errno));
       exit(EXIT_FAILURE);
    }

    COOKBOOK *cookbook_parsed;

    cookbook_parsed = parse_cookbook(file_open, &err); // the result is the cookbook data structure
    if(err) { // err non zero value because error detected in parsing the cookbook
       fprintf(stderr, "ERROR: error parsing cookbook '%s'\n", cookbook);
       fclose(file_open); // close the file after an error is caught
       exit(EXIT_FAILURE);
    }

    // fprintf(stderr, "%s\n", cookbook_parsed->recipes->tasks->steps->words[0]);

    // CLOSING INPUT STREAM
    if (fclose(file_open) != 0) { // close the file and handle if there is an error
        fprintf(stderr, "ERROR: error in closing the file after parsed. \n");
        free_cookbook(cookbook_parsed); // cookbook was parsed and mem allocated correctly but now since file can't be closed must free cookbook stuff to exit
        exit(EXIT_FAILURE);
    }

    initialize_cookbook_states(cookbook_parsed);

    // fprintf(stderr, "%s\n", cookbook_parsed->recipes->tasks->steps->words[0]);

    // FIND RECIPE SELECTED
    RECIPE *recipe_selected = find_recipe(cookbook_parsed, recipe_name);
    if (recipe_selected == NULL) {
        fprintf(stderr, "ERRROR: Recipe '%s' not found in cookbook. \n", recipe_name);
        free_cookbook(cookbook_parsed); // cookbook was parsed and mem allocated correctly but now since file can't be closed must free cookbook stuff to exit
        exit(EXIT_FAILURE);
    }

    // Initializing the work queue
    WORK_QUEUE *work_queue = init_work_queue(); // work queue will be edited as recipe subrecipes have dependencies completed

    // check for the edge case of circular dependency in the tree cookbook data structure
    if (check_circular_tree_cycle(recipe_selected) != 0) {
        free(work_queue);
        free_cookbook(cookbook_parsed);
        exit(EXIT_FAILURE);
    }

    // ANALYSIS PHASE: STACK struct for recursive tree traversal and WORK QUEUE implemented
    int recipe_count = stack_analysis_traversal(recipe_selected, work_queue); // work queue initially populated with leaf nodes
    // fprintf(stderr, "RECIPE COUNT: %d\n", recipe_count);

    if (is_work_queue_empty(work_queue)) {
        fprintf(stderr, "ERROR: No Leaf Nodes Detected from Tree Traversal - Work Queue initialized to empty when should be populated with leaf nodes\n");
        free(work_queue);
        free_cookbook(cookbook_parsed);
        exit(EXIT_FAILURE);
    }

    // print_queue(&work_queue); // checking the first initialization of the work queue - should just be populated with the leaf nodes at first

    RECIPE **completed_recipes;
    completed_recipes = calloc(recipe_count, sizeof(RECIPE *));
    if (!completed_recipes) {
        perror("Failed to allocate completed recipes array");
        free(work_queue);
        free_cookbook(cookbook_parsed);
        free(completed_recipes);
        exit(EXIT_FAILURE);
    }

    // MAIN PROCESSING LOOP
    main_processing_loop(work_queue, max_cooks, cookbook_parsed, recipe_selected, completed_recipes);
/*
    // UNPARSING THE COOKBOOK
    unparse_cookbook(cookbook_parsed, stdout); // error handling below
    if (ferror(stdout)) { // checks for error in stdout
        fprintf(stderr, "ERROR: Error writing to stdout. \n");
        clearerr(stdout); // clear the error
        exit(EXIT_FAILURE);
    }
    if (fflush(stdout) != 0) { // flush the output in stdout
        fprintf(stderr, "ERROR: Error flushing the output in stdout. \n");
        exit(EXIT_FAILURE);
    }
*/
    // fprintf(stderr, "Main processing loop returned, freeing data structures\n");
    // FREE WORK QUEUE STRUCTURE (this is good!)
    free(work_queue);

    // FREE LIST STRUCTURE (this is good!)
    free(completed_recipes); // just a list of pointers

    // FREE STACK STRUCTURE (should already be free - i think it is malloc in push and freed in push? and also locally defined in stack)

    // FREE COOKBOOK TREE STRUCTURE
    free_cookbook(cookbook_parsed);

    exit(EXIT_SUCCESS);
}
