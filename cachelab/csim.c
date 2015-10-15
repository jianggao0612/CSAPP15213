/* Gao Jiang - gaoj */

#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include <strings.h>
#include <math.h>

#include "cachelab.h"

/* 
 * Define a struct containing the params needed to build a cache
 */
typedef struct {
	int s; // 2^s sets
	int b; // 2^b blocks in each line
	int S; // number of sets in the cache 
	int E; // number of lines in each set
	int B; // number of blocks in each line
} cache_built_param;

/*
 * Define a line in a set 
 */
typedef struct {
	int valid; // valid bit in a line 
	unsigned long long int tag; // tag bits in a line
} cache_line;

/* 
 * Define a array-base queue to store the used line index in a set
 */
typedef struct {
	int *indexes;
	int rear;
} queue;

/* 
 * Define a set in a cache 
 */
typedef struct {
	cache_line *lines;
	queue used_line_queue; // a queue in a set storing the used line index for inplementing LRU
} cache_set;

/* 
 * Define a complete cache 
 */
typedef struct {
	cache_set *sets;
} cache;

/* 
 * Define a struct containing the params for output 
 */
typedef struct {
	int hits;
	int misses;
	int evicts;
} cache_output_param;

/*
 * block_size_pow - Caculate the 2 to bth power for the needed parameters in building cache
 */
long long int pow_result(int b) {
	long long int result = 1;
	return (result << b); // get 2 to bth power for the result 
}

/*
 * create_cache - Create a simulated cache according to the params got from command line
 */
cache create_cache(long long s, long long E, long long b) {
	
	cache new_cache;
	cache_set set; // a set for initializing each set in the cache
	cache_line line; // a line for initializing each line in a set
	int S = pow_result(s); // number of sets S is 2 to sth

	int set_index;
	int line_index;

	// allocate space for the cache
	new_cache.sets = (cache_set *) malloc(sizeof(cache_set) * S);

	// for each set, initialize the lines
	for (set_index = 0; set_index < S; set_index++) {
		
		set.lines = (cache_line *) malloc(sizeof(cache_line) * E); // allocate space for lines in a set
		set.used_line_queue.indexes = (int *) malloc(sizeof(int) * E);

		/*
		 * initialize each line and the used line queue in the set
		 */
		for (line_index = 0; line_index < E; line_index++) {
			
			line.valid = 0;
			line.tag = 0;
			set.lines[line_index] = line;
			set.used_line_queue.indexes[line_index] = -1;

		}
		set.used_line_queue.rear = -1;

		new_cache.sets[set_index] = set;

	}

	return new_cache;

}

/*
 * free_cache - Free the space of a cache 
 */
void free_cache(cache used_cache, long long s, long long E, long long b) {

	int set_index;
	cache_set set;
	int S = pow_result(s);

	/*
	 * Free the space of lines and index queue in sets
	 */
	for (set_index = 0; set_index < S; set_index++) {
		
		set = used_cache.sets[set_index];

		if (set.used_line_queue.indexes != NULL) {
			free(set.used_line_queue.indexes);
		}

		if (set.lines != NULL) {
			free(set.lines);
		} 

	}

	/*
	 * Free the space of the sets
	 */
	if (used_cache.sets != NULL) {
		free(used_cache.sets);
	}

}

/*
 * find_empty_line - find an empty line in a set to put in new line when missed
 */
int find_empty_line(cache simulated_cache, long long set_index, int lines_num) {
	
	int line_index; // loop variable
	int empty_line = -1; // result empty line index
	cache_line line;

	/*
	 * find the empty line in the set (when valid bit is 0)
	 */
	for (line_index = 0; line_index < lines_num; line_index++) {
		
		line = simulated_cache.sets[set_index].lines[line_index];

		if (line.valid == 0) {
			empty_line = line_index;
			break;
		}
	
	}

	// printf("Empty line: %d\n", empty_line);
	return empty_line;

}

/*
 * find_evict_line - find an line in a set to put in new line when missed based LRU
 */
int find_evict_line(cache simulated_cache, long long set_index, int lines_num) {

	int rear = simulated_cache.sets[set_index].used_line_queue.rear;

	int evict_line_index = simulated_cache.sets[set_index].used_line_queue.indexes[0]; // get the first line index in the index queue

	/*
	 * remove the first elements int the queue
	 */
	int i;
	for (i = 0; i < rear - 1; i++) {

		simulated_cache.sets[set_index].used_line_queue.indexes[i] = simulated_cache.sets[set_index].used_line_queue.indexes[i + 1];

	}
	simulated_cache.sets[set_index].used_line_queue.indexes[rear] = -1;
	simulated_cache.sets[set_index].used_line_queue.rear--;

	// printf("Evicted line: %d\n", evict_line_index);

	return evict_line_index;

}

void update_queue(cache simulated_cache, long long set_index, int line_index) {

	int rear = simulated_cache.sets[set_index].used_line_queue.rear;

	int i, j;
	for (i = 0; i <= rear; i++) {

		if(simulated_cache.sets[set_index].used_line_queue.indexes[i] == line_index) {
			for (j = i; j < rear; j++) {

				simulated_cache.sets[set_index].used_line_queue.indexes[j] = simulated_cache.sets[set_index].used_line_queue.indexes[j + 1];

			}

			simulated_cache.sets[set_index].used_line_queue.indexes[rear] = line_index;

		} else {

			simulated_cache.sets[set_index].used_line_queue.rear++;
			simulated_cache.sets[set_index].used_line_queue.indexes[simulated_cache.sets[set_index].used_line_queue.rear] = line_index;
		}

	}
}
/*
 * simulated_cache - simulate the cache based on the given parameters from command line
 */
cache_output_param simulated_cache(cache sim_cache, cache_built_param params, cache_output_param output, unsigned long long int address) {
	
	int line_index;
	int replace_line_index = -1; // flag - represent the line in the set to be replaced
	int last_hits = output.hits; // number of hits in last trace used to determine whether it's hit or miss in this trace


	// get the params of the cache
	int lines_num = params.E;
	long long int tag_size = 64 - (params.b) - (params.s);

	// get the tags and set index in input address
	unsigned long long int input_tag = address >> (params.b + params.s);
	unsigned long long int input_setIndex = address << (tag_size) >> (tag_size + params.b);

	// find the set in the cache to be accessed
	cache_set target_set = sim_cache.sets[input_setIndex];
	cache_line line;

	/*
	 * implement the cache policies
	 */
	for (line_index = 0; line_index < lines_num; line_index++) {
		
		line = target_set.lines[line_index];

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

				output.hits++;
				printf("Got a hit. Hit %d.\n", output.hits);
				update_queue(sim_cache, input_setIndex, line_index);

			}
		}

	}

	/*
	 * if this trace is a miss and there is a empty line, fetch a new line and put into the empty line
	 * if this trace is a miss and there is no empty line, fetch a new line and put into the LRU line, it's a miss + evict
	 */
	if (last_hits == output.hits) {

		output.misses++;
		printf("Got a miss. Misses %d.\n", output.misses);

		replace_line_index = find_empty_line(sim_cache, input_setIndex, lines_num);

		if(replace_line_index != -1) {

			sim_cache.sets[input_setIndex].lines[replace_line_index].valid = 1;
			sim_cache.sets[input_setIndex].lines[replace_line_index].tag = input_tag;
			sim_cache.sets[input_setIndex].used_line_queue.rear++;
			sim_cache.sets[input_setIndex].used_line_queue.indexes[sim_cache.sets[input_setIndex].used_line_queue.rear] = replace_line_index;

		} else {

			replace_line_index = find_evict_line(sim_cache, input_setIndex, lines_num);
			sim_cache.sets[input_setIndex].lines[replace_line_index].tag = input_tag;
			output.evicts++;
			sim_cache.sets[input_setIndex].used_line_queue.rear++;
			sim_cache.sets[input_setIndex].used_line_queue.indexes[sim_cache.sets[input_setIndex].used_line_queue.rear] = replace_line_index;
			printf("Got a evict. Evicts %d.\n", output.evicts);

		}

	}

	return output;

}


int main(int argc, char **argv) {

	cache_built_param param;
	cache_output_param output;
	output.hits = 0;
	output.misses = 0;
	output.evicts = 0;
	
	char *trace_file;
	FILE *input_file;

	char trace_op;
	unsigned long long int address; // hold for 64-bytes address
	int size;

	char c;
	while ((c = getopt(argc, argv, "s: E: b: t: hv")) != -1) {

		switch(c) {
			case 's':
				param.s = atoi(optarg); // transfer input string argument to integer
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

	// create a cache according to the given parameters
	cache new_cache = create_cache(param.s, param.E, param.b);

	// open the input file to get operations
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
					output = simulated_cache(new_cache, param, output, address);
					break;
				case 'L':
					printf("%c %llx %d\n", trace_op, address, size);
					output = simulated_cache(new_cache, param, output, address);
					break;
				case 'M':
					printf("%c %llx %d\n", trace_op, address, size);
					output = simulated_cache(new_cache, param, output, address);
					output = simulated_cache(new_cache, param, output, address);
					break;
				default:
					break;

			}

		}

		printSummary(output.hits, output.misses, output.evicts); // print out the result
		free_cache(new_cache, param.s, param.E, param.b);
		fclose(input_file);

		return 0;

	}
}