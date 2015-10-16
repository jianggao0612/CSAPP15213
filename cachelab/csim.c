/* Gao Jiang - gaoj */

#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include <strings.h>
#include <math.h>

#include "cachelab.h"

/*
 * Define a struct containing the input params to build a cache
 */
typedef struct _cache_param_t {

	int s; // 2^s sets
	int E; // number of lines in a set
	int b; // 2^b blocks in a line

} cache_param_t;

/*
 * Define a struct containing the output params of the simulated cache
 */
typedef struct _output_param_t {

	int hits; // number of hits in simulated cache
	int misses; // number of misses in simulated cache
	int evicts; // number of evicts in simulated cache

} output_param_t;

/*
 * Define a struct representing a line in a set
 */
typedef struct _cache_line_t {

	int valid; // valid bit of a line
	unsigned long long tag; // tag of a line

} cache_line_t;

/*
 * Define a struct representing the used index queue in a set to implement LRU
 */
typedef struct _cache_set_queue_t {

	int* indexes;
	int rear;

} cache_set_queue_t;

/*
 * Define a struct representing the set in cache
 */
typedef struct _cache_set_t {

	cache_line_t* lines;
	cache_set_queue_t queue;

} cache_set_t;

/*
 * Define a strudt representing the cache
 */
typedef struct _cache_t {

	cache_set_t* sets;

} cache_t;

/*
 * pow_result - Caculate the 2 to bth power for the needed parameters in building cache
 */
long long pow_result(int b) {

	long long result = 1;
	return (result << b); // get 2 to bth power for the result 

}

/*
 * create_cache - Create a simulated cache according to the params got from command line
 */
cache_t create_cache(cache_param_t* param) {

	cache_t new_cache;
	cache_set_t set;
	cache_line_t line;

	long long S = pow_result(param -> s);

	int set_index;
	int line_index;

	// allocate space for the cache
	new_cache.sets = (cache_set_t *) malloc(sizeof(cache_set_t) * S);

	// for each set, initialize the lines
	for (set_index = 0; set_index < S; set_index++) {

		set.lines = (cache_line_t *) malloc(sizeof(cache_line_t) * (param -> E)); // allocate space for lines in a set
		set.queue.indexes = (int *) malloc(sizeof(int) * (param -> E));

		/*
		 * initialize each line and the used line queue in the set
		 */
		for (line_index = 0; line_index < (param -> E); line_index++) {

			line.valid = 0;
			line.tag = 0;
			set.lines[line_index] = line;
			set.queue.indexes[line_index] = -1;

		}
		set.queue.rear = -1;


		new_cache.sets[set_index] = set;
	}

	return new_cache;

}


/*
 * free_cache - Free the space of a cache 
 */
void free_cache(cache_t* sim_cache, cache_param_t* param) {

	int set_index;
	long long S = pow_result(param -> s);

	/*
	 * Free the space of lines and index queue in sets
	 */
	for (set_index = 0; set_index < S; set_index++) {

		if ((sim_cache -> sets[set_index]).queue.indexes != NULL){

			free((sim_cache -> sets[set_index]).queue.indexes);

		}

		if ((sim_cache -> sets[set_index]).lines != NULL) {

			free((sim_cache -> sets[set_index]).lines);

		}
	}

	/*
	 * Free the space of the sets
	 */
	if ((sim_cache -> sets) != NULL) {

		free(sim_cache -> sets);

	}

	return;
}
/*
 * find_empty_line - find an empty line in a set to put in new line when missed
 */
int find_empty_line(cache_t* sim_cache, long long set_index, int line_num) {

	int line_index;
	int empty_line = -1;
	cache_line_t line;

	/*
	 * find the empty line in the set (when valid bit is 0)
	 */
	 for (line_index = 0; line_index < line_num; line_index++) {

	 	line = (sim_cache -> sets[set_index]).lines[line_index];

	 	if (line.valid == 0) {

	 		empty_line = line_index;
	 		break;

	 	}

	 }

	 return empty_line;

}

/*
 * update_queue - Update the queue when access a line
 */
void update_queue(cache_t* sim_cache, long long set_index, int line_index) {

	int rear = (sim_cache -> sets[set_index]).queue.rear;
	int isExisted = 0;

	int i, j;
	for (i = 0; i <= rear; i++) {

		/*
		 * if line index is existed, move it the the rear of the queue
		 */
		 if ((sim_cache -> sets[set_index]).queue.indexes[i] == line_index) {

		 	isExisted = 1;

		 	for (j = i; j < rear; j++) {

		 		(sim_cache -> sets[set_index]).queue.indexes[j] = (sim_cache -> sets[set_index]).queue.indexes[j + 1];
		 	
		 	}

		 	(sim_cache -> sets[set_index]).queue.indexes[rear] = line_index;
		 	break;
		 }

	}

	if (!isExisted) {

		(sim_cache -> sets[set_index]).queue.rear++;
		(sim_cache -> sets[set_index]).queue.indexes[(sim_cache -> sets[set_index]).queue.rear] = line_index;

	}

	return;

}

void simulated_cache(cache_t* sim_cache, cache_param_t* param, output_param_t* output, unsigned long long address) {

	int line_index;
	int replace_line_index = -1; // represent the line in the set to be replaced
	int last_hits = output -> hits;
	cache_line_t line;

	// get the params of the cache
	int line_num = param -> E;
	long long tag_zize = 64 - (param -> s) - (param -> b);

	// get tags and set index in input address
	unsigned long long input_tag = address >> ((param -> s) + (param -> b));
	unsigned long long set_index = address << (tag_zize) >> (tag_zize + param -> b);

	for (line_index = 0; line_index < line_num; line_index++) {

		line = (sim_cache -> sets[set_index]).lines[line_index];

		/*
		 * if the line is valid, go on check the line tag
		 * otherwise, the line is empty, just fetch a new line in this line
		 */
		 if (line.valid) {

		 	/*
			 * if the line's flag is equal to the input flag, it's a hit. Add the line index into the set's accessed queue
			 * otherwise, it's a miss. Fetch a new line to replace a possible empty line
			 */
			 if (line.tag == input_tag) {

			 	(output -> hits) += 1;
			 	printf("Got a hit. Hit %d.\n", output -> hits);
			 	update_queue(sim_cache, set_index, line_index);
			 	break;

			 }
		 }
	}

	/*
	 * if this trace is a miss and there is a empty line, fetch a new line and put into the empty line
	 * if this trace is a miss and there is no empty line, fetch a new line and put into the LRU line, it's a miss + evict
	 */
	 if (last_hits == (output -> hits)) {

	 	(output -> misses) += 1;
	 	printf("Got a miss. Misses %d.\n", output -> misses);

	 	replace_line_index = find_empty_line(sim_cache, set_index, line_num);

	 	if (replace_line_index != -1) {

	 		(sim_cache -> sets[set_index]).lines[replace_line_index].valid = 1;
			(sim_cache -> sets[set_index]).lines[replace_line_index].tag = input_tag;

			update_queue(sim_cache, set_index, replace_line_index);

		} else {

			(output -> evicts) += 1;
			printf("Got a evict. Evicts %d.\n", output -> evicts);

			replace_line_index = (sim_cache -> sets[set_index]).queue.indexes[0];
			(sim_cache -> sets[set_index]).lines[replace_line_index].tag = input_tag;

			update_queue(sim_cache, set_index, replace_line_index);

		}
	 }

}

int main(int argc, char **argv) {

	cache_param_t param;
	output_param_t output;
	output.hits = 0;
	output.misses = 0;
	output.evicts = 0;

	char *trace_file;
	FILE *input_file;

	/*
	 * Variables to get trace argument in trace file 
	 */
	char trace_op;
	unsigned long long address;
	int size;

	char c;
	while ((c = getopt(argc, argv, "s: E: b: t: hv")) != -1) {

		switch (c) {

			case 's':
				param.s = atoi(optarg);
				break;
			case 'E':
				param.E = atoi(optarg);
				break;
			case 'b':
				param.b = atoi(optarg);
				break;
			case 't':
				trace_file = optarg;
				break;
			default:
				break;

		}

	}

	/*
	 * check whether the input parameters are legal and complete
	 */
	if (param.s == 0 || param.E == 0 || param.b == 0 || trace_file == NULL) {

		printf("Miss required command line argument");
		exit(1);

	}

	// use the input params to create a cache
	cache_t new_cache = create_cache(&param);

	// open the input trace file
	input_file = fopen(trace_file, "r");

	if (input_file != NULL) {

		/* 
		 * get the trace command from each line
		 */
		while(fscanf(input_file, " %c %llx,%d", &trace_op, &address, &size) == 3) {

			switch(trace_op) {

				case 'I':
					break;
				case 'S':
					printf("%c %llx %d\n", trace_op, address, size);
					simulated_cache(&new_cache, &param, &output, address);
					break;
				case 'L':
					printf("%c %llx %d\n", trace_op, address, size);
					simulated_cache(&new_cache, &param, &output, address);
					break;
				case 'M':
					printf("%c %llx %d\n", trace_op, address, size);
					simulated_cache(&new_cache, &param, &output, address);
					simulated_cache(&new_cache, &param, &output, address);
					break;
				default:
					break;

			}

		}

		printSummary(output.hits, output.misses, output.evicts); // print out the result
		free_cache(&new_cache, &param);
		fclose(input_file);

		return 0;

	} else {

		printf("Cannnot open trace file.\n");
		return 1;

	}

}