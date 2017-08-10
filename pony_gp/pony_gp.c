#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <assert.h>
#include <string.h>
#include <float.h>
#include <math.h>
#include <ctype.h>
#include "binary_tree.h"
#include "util.h"
#include "hashmap.h"
#include "csv_reader.h"

#define DEFAULT_FITNESS -DBL_MAX
#define PRINT_SPACE 4

/**
* A struct to contain the different functions, terminals, and arities.
*	*terminals: An array of constant numbers that can be used in the program.
*	*functions: An array of functions that can be used in the program.
*	  *arities: A hashmap containing the arities of different symbols
*			    to be used in the program.
*/
struct symbols {
	char *terminals;
	char *functions;
	struct hashmap *arities;
} symbols;


/**
* An individual is a struct that contains:
*		 *genome: A tree.
*		 fitness: The fitness of the evaluated tree.
*/
struct individual {
	struct node *genome;
	double fitness;
};

/**
* A struct to contain the data collected from the CSV file.
*	       **test_cases: The test cases.
*	      *test_targets: The test targets.
*	   **training_cases: The training cases.
*	  *training_targets: The training targets.
*/
struct csv_data {
	double **test_cases;
	double *test_targets;
	double **training_cases;
	double *training_targets;
} *data;


// User defined program parameters
struct hashmap *params;


void grow(struct node *node, int curr_depth, int max_depth, bool full);
char get_random_symbol(int curr_depth, int max_depth, bool full);
void subtree_mutation(struct node **node);
void subtree_crossover(struct node **parent1, struct node **parent2);
struct individual *init_population(void);
char *get_terminals(void); 
char *get_functions(void);
struct hashmap *get_arities(void);
struct hashmap *get_params(void);
char *get_fitness_file(void);
double ***parse_exemplars(char *file_name);
struct csv_data *get_test_and_train_data(char *file_name, double split);
double evaluate(struct node *node, double *fitness_case);
void evaluate_individual(struct individual *individual);
void evaluate_fitness(struct individual *individuals, int size, struct hashmap *cache);

void setup(void);
void exit_and_cleanup(void);
int test_grow(void);
int test_subtree_mutation(void);
int test_subtree_crossover(void);
int test(void);


/*
 * Recursively grow a node to max depth in a pre-order, ie. depth-first
 * left-to-right traversal.
		      *node: The root node of the subtree.
	     curr_depth: The current depth in the tree. Must be passed in as 0 to work as
				     intended.
	 max_tree_depth: The current max depth that the function has reached.
	                 Must be passed in initially as 0 to work as intended.
	    should_fill: Grows the tree to max depth when true.
 */
void grow(struct node *node, int curr_depth, int max_depth, bool must_fill) {
	char node_symbol = node->value;
	char node_arr[2];
	node_arr[0] = node_symbol;
	node_arr[1] = '\0';

	char symbol;

	if (curr_depth >= max_depth) return;

	// grow is called recursively in the loop. The loop iterates
	// "arity" number of times, given by the node symbol.
	for (int side = 0; side < hashmap_get(symbols.arities, node_arr); side++) {
		
		symbol = get_random_symbol(curr_depth, max_depth, must_fill);

		// new_node is not set with append_node() because it returns a copy of
		// the pointer to the new node, not the new node itself.
		append_node(node, symbol, side);
		struct node *new_node = (!side ? node->left : node->right);

		// call grow with the child node as the current node.
		grow(new_node, curr_depth+1, max_depth, must_fill);

	}

	assert(curr_depth <= max_depth);
}

/**
 * Subtree mutation. Pick a node and grow it.
	   **root: The root node of the tree.
 */
void subtree_mutation(struct node **root) {

	// Check if mutation should be applied
	if (get_rand_probability() < hashmap_get(params, "mutation_probability")) {
		// Pick a node
		int end_node_i = get_number_of_nodes(*root) - 1;
		int node_i = get_randint(0, end_node_i);
		struct node *old_node = get_node_at_index(root, node_i);

		// Get new subtree
		int node_depth = get_depth_at_index(*root, node_i);
		
		char new_symbol = get_random_symbol(
				node_depth, (int)hashmap_get(params, "max_depth") - node_depth, false
		);

		struct node *new_subtree = new_node(new_symbol, NULL, NULL);

		// Grow the subtree
		grow(new_subtree, 0, (int)hashmap_get(params, "max_depth"), false);
		
		// Replace the old subtree with the new one
		*old_node = *new_subtree;
	}
}


/**
 * Swap two random nodes from the parents and swapping the subtrees.
 *		**parent1: Parent to crossover
 *		**parent2: Other parent to crossover
 */
void subtree_crossover(struct node **parent1, struct node **parent2) {
	
	// Check if crossover will occur
	if (get_rand_probability() < hashmap_get(params, "crossover_probability")) {
	
		struct node *xo_nodes[2];
		int node_depths[2][2];

		for (int i = 0; i < 2; i++) {
			struct node **parent = (i ? parent2 : parent1);

			// Pick a crossover point
			int end_node_i = get_number_of_nodes(*parent) - 1;
			int node_i = get_randint(0, end_node_i);

			// Find the subtree at the crossover point
			xo_nodes[i] = get_node_at_index(parent, node_i);

			node_depths[i][0] = get_depth_at_index(*parent, node_i);
			node_depths[i][1] = get_max_tree_depth(xo_nodes[i]);
		}

		int max_depth = (int)hashmap_get(params, "max_depth");

		// Make sure the trees will not exceed the max depth
		if ((node_depths[0][1] + node_depths[1][0] > max_depth) ||
			(node_depths[1][1] + node_depths[0][0] > max_depth)) {

			if (hashmap_get(params, "verbose")) {
				printf("\n-----------------Crossover too deep-----------------\n");
			}

			return;
		}

		// Swap the nodes
		struct node tmp = *xo_nodes[0];

		*xo_nodes[0] = *xo_nodes[1];
		*xo_nodes[1] = tmp;

		assert(
			get_max_tree_depth(xo_nodes[0]) <= max_depth &&
			get_max_tree_depth(xo_nodes[1]) <= max_depth
		);
	}
}


/**
 * Ramped half-half initialization. The individuals in the population
 * are initialized using the grow or the full method for each depth
 * value (ramped) up to max_depth.
 */
struct individual *init_population() {
	int pop_size = (int)hashmap_get(params, "population_size");

	struct individual *population = malloc(sizeof(struct node) * pop_size);

	for (int i = 0; i < pop_size; i++) {
		
		// Pick full or grow method
		bool full = get_randint(0, 1);

		// Ramp the depth
		int max_depth = (i % (int)hashmap_get(params, "max_depth")) + 1;

		// Create the root node
		char symbol = get_random_symbol(0, max_depth, full);
		struct node *tree = new_node(symbol, NULL, NULL);

		// Grow the tree if the root is a function symbol.
		if (max_depth > 0 && strchr(symbols.functions, symbol)) {
			grow(tree, 0, max_depth, full);

			assert(get_max_tree_depth(tree) < max_depth + 1);
		}

		struct individual individual = (struct individual) { tree, DEFAULT_FITNESS };

		population[i] = individual;
	}

	return population;
}

/**
 * Return a randomly chosen symbol. The depth determines if a terminal
 * must be chosen. If `full` is specified a function will be chosen
 * until the max depth is reached. The symbol is picked with a uniform
 * probability.
	   curr_depth: The current depth. Must be passed in as 0 to perform
				   as intended.
	    max_depth: The max depth that the tree is allowed to reach.
	         full: True if function symbols should be used until max depth
				   is reached.
 */
char get_random_symbol(int curr_depth, int max_depth, bool full) {
	char symbol;
	int n;

	// Pick a terminal if max depth has been reached
	if (curr_depth >= (max_depth - 1)) {
		n = rand_index(get_char_arr_length(symbols.terminals)-1);
		symbol = symbols.terminals[n];
	} else {
		// 50% chance it will be a terminal if max depth has
		// not been reached
		if (!full && get_randint(0, 1)) {
			n = rand_index(get_char_arr_length(symbols.terminals)-1);
			symbol = symbols.terminals[n];
		} else {
			// Pick a random function
			n = rand_index(get_char_arr_length(symbols.functions)-1);
			symbol = symbols.functions[n];
		}
	}

	return symbol;
}
 

/**
 * Temporary function to get terminals. Will modify to read from
 * a config document.
 */
char *get_terminals() {
	char *terminals = malloc(3);
	terminals[0] = '0';
	terminals[1] = '1';
	terminals[2] = '\0';

	return terminals;
}


/**
* Temporary function to get functions. Will modify to read from
* a config document.
*/
char *get_functions() {
	char *functions = malloc(5);
	functions[0] = '+';
	functions[1] = '*';
	functions[2] = '/';
	functions[3] = '-';
	functions[4] = '\0';

	return functions;
}


/**
 * Temporary function to get functions. Will modify to read from
 * a config document.
 */
struct hashmap *get_arities() {
	struct hashmap *h = hashmap_init();

	hashmap_put(h, "+", 2);
	hashmap_put(h, "*", 2);
	hashmap_put(h, "/", 2);
	hashmap_put(h, "-", 2);
	hashmap_put(h, "0", 0);
	hashmap_put(h, "1", 0);

	return h;
}


/**
* Temporary function to get parameters. Will modify to read from
* a config document.
*/
struct hashmap *get_params() {
	struct hashmap *h = hashmap_init();

	hashmap_put(h, "population_size", 100);
	hashmap_put(h, "max_depth", 3);
	hashmap_put(h, "elite_size", 2);
	hashmap_put(h, "generations", 20);
	hashmap_put(h, "tournament_size", 3);
	hashmap_put(h, "crossover_probability", 1);
	hashmap_put(h, "mutation_probability", 1);
	hashmap_put(h, "test_train_split", 0.7);
	hashmap_put(h, "verbose", 1);
	hashmap_put(h, "seed", 0);

	return h;
}


/**
* Temporary function to get the fitness case file. Will modify to read
* from a config document.
*/
char *get_fitness_file() {
	return "fitness_cases.csv";
}

/**
 * Parse a CSV file. Parse the fitness case and split the data into
 * test and train data. in the fitness case file each row is an exemplar
 * and each dimension is in a column. The last column is the target value
 * of the exemplar. 
	 *file_name: Name of CSV file with a header.
 */
double ***parse_exemplars(char *file_name) {
	csv_reader *reader = init_csv(file_name, ',');

	double **fitness_cases, *targets;
	int num_columns = get_num_column(reader);
	int num_lines = get_num_lines(reader);

	// leave space for NULL
	fitness_cases = malloc(sizeof(double *) * (num_lines));

	for (int i = 0; i < num_lines; i++) {
		fitness_cases[i] = malloc(sizeof(double) * (num_columns-1));
	}

	// leave space for NAN
	targets = malloc(sizeof(double) * (num_lines));

	csv_line *row;
	int f_i = 0;
	int t_i = 0;

	// Ignore the header
	next_line(reader);

	while ((row = readline(reader))) {
		int i;
		for (i = 0; i < num_columns; i++) {
			if (i == num_columns - 1) {
				targets[t_i++] = atof(row->content[i]);
			}
			else {
				fitness_cases[f_i][i] = atof(row->content[i]);
			}
		}
		fitness_cases[f_i][i-1] = NAN;
		f_i++;
	}
	fitness_cases[f_i] = NULL;
	targets[t_i] = NAN;

	double ***results = malloc(sizeof(double **) * 2);
	double *tmp[] = { targets };
	results[0] = fitness_cases;
	results[1] = tmp;

	return results;
}

/**
 * Return test and train data. Random selection or exemplars(ros)
 * from file containing data.
	 *file_name: Name of CSV file with a header.
		  split: Percentage of exemplar data used for training.
 */
struct csv_data *get_test_and_train_data(char *file_name, double split) {
	double ***exemplars = parse_exemplars(file_name);

	double **fitness = exemplars[0];
	double *targs = *exemplars[1];

	int fitness_len = get_2d_arr_length(fitness);
	int targs_len = get_double_arr_length(targs);

	int col_size = get_double_arr_length(fitness[0]);

	// randomize the index order
	int fits_split_i = (int)(floor(fitness_len * split));
	int *fits_rand_idxs = random_indexes(fitness_len);
	int targs_split_i = (int)(floor(targs_len * split));
	int *targs_rand_idxs = random_indexes(targs_len);

	double **training_cases = malloc((sizeof(double *) * fits_split_i ) + 1);
	double **test_cases = malloc((sizeof(double *) * (fitness_len - fits_split_i)) + 1);

	for (int i = 0; i < 2; i++) {
		training_cases[i] = malloc(sizeof(double) * col_size);
		test_cases[i] = malloc(sizeof(double) * col_size);
	}

	double *training_targets = malloc((sizeof(double) * targs_split_i) + 1);
	double *test_targets = malloc(sizeof(double) * (targs_len - targs_split_i) + 1);

	int rand_i;
	int i;

	for (i = 0; i < fitness_len; i++) {
		rand_i = fits_rand_idxs[i];

		if (i >= fits_split_i) {
			test_cases[i - fits_split_i] = fitness[rand_i];
		}
		else {
			training_cases[i] = fitness[rand_i];
		}
	}

	for (i = 0; i < targs_len; i++) {
		rand_i = targs_rand_idxs[i];

		if (i >= targs_split_i) {
			test_targets[i - targs_split_i] = targs[rand_i];
		}
		else {
			training_targets[i] = targs[rand_i];
		}
	}

	// Set last index to NULL/NAN to allow for easier looping of arrays
	training_cases[fits_split_i] = NULL;
	test_cases[fitness_len - fits_split_i] = NULL;
	training_targets[targs_split_i] = NAN;
	test_targets[targs_len - targs_split_i] = NAN;

	struct csv_data *results = malloc(sizeof(struct csv_data));
	results->test_cases = test_cases;
	results->training_cases = training_cases;
	results->training_targets = training_targets;
	results->test_targets = test_targets;

	return results;

}

/**
* Evaluate a node recursively. The node's symbol char is evaluated.
*	*node: The evaluate node.
*	*fitness_case: Current fitness case.
*/
double evaluate(struct node *node, double *fitness_case) {

	if (!node) return -DBL_MAX;

	char symbol = node->value;

	if (symbol == '+') {
		return evaluate(node->left, fitness_case) + evaluate(node->right, fitness_case);
	}

	else if (symbol == '-') {
		return evaluate(node->left, fitness_case) - evaluate(node->right, fitness_case);
	}

	else if (symbol == '*') {
		return evaluate(node->left, fitness_case) * evaluate(node->right, fitness_case);
	}

	else if (symbol == '/') {
		double numerator = evaluate(node->left, fitness_case);
		double denominator = evaluate(node->right, fitness_case);

		if (fabs(denominator) < 0.00001) {
			denominator = 1.0;
		}
		return numerator / denominator;
	}

	else if (isalpha(symbol)) {

		// Fitness case variables must be in alphabetical order
		// for this to work correctly.
		if (symbol >= 'A') {
			return fitness_case[symbol - 'a'];
		}
		else {
			return fitness_case[symbol - 'A'];
		}
	}

	else {
		return (double)symbol - '0';
	}
}

/**
* Evaulate fitness absed on fitness cases and target values. Fitness
* cases are a set of exemplars (input and ouput points) by
* comparing the error between the output of an individual (symbolic
* expression) and the target values.
* Evaluates and sets the fitness in an individual. Fitness is the
* negative mean square error (MSE).
*	     *individual: The individual solution to evaluate.
*	 **fitness_cases: Input for the evaluation.
*	        *targets: Output corresponding to the input.
*/
void evaluate_individual(struct individual *individual) {
	// Initial fitness value
	double fitness = 0.0;
	int targs_len = get_double_arr_length(data->training_targets);

	// Calculate the error between the output of the individual solution and
	// the target for each input.
	for (int i = 0; i < targs_len; i++) {
		double output = evaluate(individual->genome,data->training_cases[i]);
		
		// Get the squared error
		double error = output -data->training_targets[i];
		fitness += error * error;
	}

	assert(fitness >= 0);

	// Get the mean fitness and assign it to the individual.
	individual->fitness = (fitness * -1) / targs_len;

	assert(individual->fitness <= 0);
}

/**
* Evaluate each individual of the population.
* Uses a simple cache for reducing unmber evaluations of individuals.
*	 *individuals: Population to evaluate.
*	         size: Population size.
*		   *cache: Cache for fitness evaluation.
*/
void evaluate_fitness(struct individual *individuals, int size, struct hashmap *cache) {
	for (int i = 0; i < size; i++) {

		char *key = to_string(individuals[i].genome);
		double fitness = hashmap_get(cache, key);

		if (!isnan(fitness)) {
			individuals[i].fitness = fitness;
		}
		else {
			evaluate_individual(&individuals[i]);
			hashmap_put(cache, key, individuals[i].fitness);
		}
	}
}

/**
* Wrapper for setup.
*/
void setup() {
	symbols.functions = get_functions();
	symbols.terminals = get_terminals();
	symbols.arities = get_arities();
	params = get_params();
	set_seed(hashmap_get(params, "seed"));

	data = get_test_and_train_data(get_fitness_file(), hashmap_get(params, "test_train_split"));
}

/**
* Wrapper for deallocating memory and exiting.
*/
void exit_and_cleanup() {
	free(symbols.arities);
	free(symbols.functions);
	free(symbols.terminals);
	exit(EXIT_SUCCESS);
}


main() {
	setup();

	struct node *test = new_node('+', NULL, NULL);
	test->left = new_node('*', NULL, NULL);
	test->right = new_node('*', NULL, NULL);
	test->left->left = new_node('a', NULL, NULL);
	test->left->right = new_node('a', NULL, NULL);
	test->right->left = new_node('b', NULL, NULL);
	test->right->right = new_node('b', NULL, NULL);

	struct individual *individuals = init_population();
	struct hashmap *cache = hashmap_init();

	evaluate_fitness(individuals, (int)hashmap_get(params, "population_size"), cache);

	for (int i = 0; i < (int)hashmap_get(params, "population_size"); i++) {
		printf("%f\n", individuals[i].fitness);
	}

	exit_and_cleanup();
}


/**
 * Run all tests on current seed.
 */
int test() {
	if (test_grow() || test_subtree_mutation() || test_subtree_crossover()) {
		return (EXIT_FAILURE);
	}

	return (EXIT_SUCCESS);
	
}


/**
 * A function to test the grow function.
 * Valid seeds: -24, -5, -3, 1, 2, 4, 7
 * Max depth must be 3.
 * Return EXIT_FAILURE if seed provided is invalid or max depth is not 3.
 */
int test_grow() {
	if (hashmap_get(params, "max_depth") != 3) {
		printf("Please set max_depth to 3");
		return(EXIT_FAILURE);
	}

	struct node *test = new_node('+', NULL, NULL);
	grow(test, 0, (int)hashmap_get(params, "max_depth"), false);

	char seed_neg24[] = { '+', '+', '+', '0', '0', '+', '0', '0', '+', '+', '0', '0', '+', '0', '0' };
	char seed_neg5[] = { '+', '/', '/', '0', '0', '/', '0', '0', '/', '/', '0', '0', '/', '0', '0' };
	char seed_neg3[] = { '+', '+', '+', '0', '0', '+', '0', '0', '+', '+', '0', '0', '+', '0', '0' };
	char seed_1[] = { '+', '1', '1' };
	char seed_2[] = { '+', '1', '1' };
	char seed_4[] = { '+', '1', '1' };
	char seed_7[] = { '+', '1', '1' };

	char *arr_to_use;

	switch ((int)hashmap_get(params, "seed")) {
		case -3:
			arr_to_use = seed_neg3;
			break;
		case -5:
			arr_to_use = seed_neg5;
			break;
		case 4:
			arr_to_use = seed_4;
			break;
		case 2:
			arr_to_use = seed_2;
			break;
		case -24:
			arr_to_use = seed_neg24;
			break;
		case 7:
			arr_to_use = seed_7;
			break;
		case 1:
			arr_to_use = seed_1;
			break;
		default:
			printf("Current seed is not testable\n");
			return(EXIT_FAILURE);
	};

	for (int i = 0; i < get_number_of_nodes(test); i++) {
		assert(get_node_at_index(&test, i)->value == arr_to_use[i]);
	}

	free(test);

	return(EXIT_SUCCESS);

}


/**
 * A function to test the subtree_mutation function.
 * Valid seeds: -24, -5, -3, 1, 2, 4, 7
 * Max depth must be 3.
 * Return EXIT_FAILURE if seed provided is invalid or max depth is not 3.
 */
int test_subtree_mutation() {
	if (hashmap_get(params, "max_depth") != 3) {
		printf("Please set max_depth to 3");
		return(EXIT_FAILURE);
	}

	struct node *test = new_node('+', NULL, NULL);
	test->left = new_node('*', NULL, NULL);
	test->right = new_node('2', NULL, NULL);
	test->left->left = new_node('6', NULL, NULL);
	test->left->right = new_node('*', NULL, NULL);
	test->left->right->left = new_node('5', NULL, NULL);
	test->left->right->right = new_node('7', NULL, NULL);

	subtree_mutation(&test);

	char seed_neg3[] = { '+', '+', '+', '0', '0', '+', '0', '0', '+', '+', '0', '0', '+', '0', '0' };
	char seed_neg5[] = { '+', '0', '2' };
	char seed_4[] = { '+', '*', '1', '*', '5', '7', '2' };
	char seed_2[] = { '+', '*', '6', '1', '2' };
	char seed_neg24[] = { '+', '*', '6', '0', '2' };
	char seed_7[] = { '+', '*', '6', '*', '5', '1', '2' };
	char seed_1[] = { '+', '*', '6', '*', '5', '7', '1' };

	char *arr_to_use;

	switch ((int)hashmap_get(params, "seed")) {
		case -3:
			arr_to_use = seed_neg3;
			break;
		case -5:
			arr_to_use = seed_neg5;
			break;
		case 4:
			arr_to_use = seed_4;
			break;
		case 2:
			arr_to_use = seed_2;
			break;
		case -24:
			arr_to_use = seed_neg24;
			break;
		case 7:
			arr_to_use = seed_7;
			break;
		case 1:
			arr_to_use = seed_1;
			break;
		default:
			printf("Current seed is not testable\n");
			return(EXIT_FAILURE);
	};



	for (int i = 0; i < get_number_of_nodes(test); i++) {
		assert(get_node_at_index(&test, i)->value == arr_to_use[i]);
	}

	free(test);

	return(EXIT_SUCCESS);

}


/**
 * A function to test the subtree_crossover function.
 * Valid seeds: -24, -5, -3, 1, 2, 4, 7
 * Max depth must be 3.
 * Return EXIT_FAILURE if seed provided is invalid or max depth is not 3.
 */
int test_subtree_crossover() {
	if (hashmap_get(params, "max_depth") != 3) {
		printf("Please set max_depth to 3");
		return(EXIT_FAILURE);
	}

	struct node *test1 = new_node('+', NULL, NULL);
	test1->left = new_node('*', NULL, NULL);
	test1->right = new_node('2', NULL, NULL);
	test1->left->left = new_node('6', NULL, NULL);
	test1->left->right = new_node('*', NULL, NULL);
	test1->left->right->left = new_node('5', NULL, NULL);
	test1->left->right->right = new_node('7', NULL, NULL);

	struct node *test2 = new_node('-', NULL, NULL);
	test2->left = new_node('-', NULL, NULL);
	test2->right = new_node('1', NULL, NULL);
	test2->left->left = new_node('3', NULL, NULL);
	test2->left->right = new_node('/', NULL, NULL);
	test2->left->right->left = new_node('4', NULL, NULL);
	test2->left->right->right = new_node('8', NULL, NULL);

	char seed1_neg3[] = { '-', '-', '3', '/', '4', '8', '1' };
	char seed1_neg5[] = { '+', '-', '3', '/', '4', '8', '2' };
	char seed1_4[] = { '+', '*', '3', '*', '5', '7', '2' };
	char seed1_2[] = { '+', '*', '6', '/', '4', '8', '2' };
	char seed1_neg24[] = { '+', '*', '6', '/', '4', '8', '2' };
	char seed1_7[] = { '+', '*', '6', '*', '5', '8', '2' };
	char seed1_1[] = { '+', '*', '6', '*', '5', '7', '1' };

	char seed2_neg3[] = { '+', '*', '6', '*', '5', '7', '2' };
	char seed2_neg5[] = { '+', '-', '3', '/', '4', '8', '2' };
	char seed2_4[] = { '-', '-', '6', '/', '4', '8', '1' };
	char seed2_2[] = { '-', '-', '3', '*', '5', '7', '1' };
	char seed2_neg24[] = { '-', '-', '3', '*', '5', '7', '1' };
	char seed2_7[] = { '-', '-', '3', '/', '4', '7', '1' };
	char seed2_1[] = { '-', '-', '3', '/', '4', '8', '2' };

	char *arr1_to_use;
	char *arr2_to_use;

	subtree_crossover(&test1, &test2);

	switch ((int)hashmap_get(params, "seed")) {
		case -3:
			arr1_to_use = seed1_neg3;
			arr2_to_use = seed2_neg3;
			break;
		case -5:
			arr1_to_use = seed1_neg5;
			arr2_to_use = seed2_neg5;
			break;
		case 4:
			arr1_to_use = seed1_4;
			arr2_to_use = seed2_4;
			break;
		case 2:
			arr1_to_use = seed1_2;
			arr2_to_use = seed2_2;
			break;
		case -24:
			arr1_to_use = seed1_neg24;
			arr2_to_use = seed2_neg24;
			break;
		case 7:
			arr1_to_use = seed1_7;
			arr2_to_use = seed2_7;
			break;
		case 1:
			arr1_to_use = seed1_1;
			arr2_to_use = seed2_1;
			break;
		default:
			printf("Current seed is not testable\n");
			return(EXIT_FAILURE);
	};

	for (int i = 0; i < get_number_of_nodes(test1); i++) {
		assert(get_node_at_index(&test1, i)->value == arr1_to_use[i]);
	}

	for (int i = 0; i < get_number_of_nodes(test2); i++) {
		assert(get_node_at_index(&test2, i)->value == arr2_to_use[i]);
	}

	free(test1);
	free(test2);

	return (EXIT_SUCCESS);
}

