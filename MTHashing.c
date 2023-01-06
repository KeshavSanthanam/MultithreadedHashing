#include <stdio.h>     
#include <string.h>
#include <stdlib.h>   
#include <stdint.h>  
#include <inttypes.h>  
#include <errno.h>     
#include <fcntl.h>     
#include <unistd.h>    
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/mman.h>
#include <pthread.h>
#include "common.h"
#include "common_threads.h"

#define BLOCK_SIZE 4096

// 11-12-22
pthread_mutex_t mylock;
//pthread_mutex_t mymutex;	

void *tree(void *arg);
void *node(void *arg);
int gettid();
void Usage(char*);
uint32_t jenkins_one_at_a_time_hash(const uint8_t* , uint64_t );
typedef struct thread_info {
	// each thread needs to know where to start reading from the memory map
	int thread_value;
	long start_location;
	long end_location; 
	uint32_t computed_hash;
} ti;
ti threads[200];

int thread_counter = 0;		// this variable just assigns a sequence number to the thread
int numThreads = 0;			// global value indicating the number of threads
char *ptr = NULL;			// pointer to the global memory map
uint32_t* htable;
long file_size = 0;

int main(int argc, char** argv) 
{
	int32_t fd;
  	uint32_t nblocks;
	uint32_t hash = 0;
	double start = 0, end = 0;
	Pthread_mutex_init(&mylock, NULL);
	//Pthread_mutex_init(&mymutex, NULL);
  	// input checking 
  	if (argc != 3)
    	Usage(argv[0]);

	// inputs to the program are filename and number of threads
	// to build a tree we need to know the height
	// log(numThreads + 1) to base 2 - 1 should give us the height
	
	int ht = 0;
  	char filename[32];
  	strcpy (filename, argv[1]);	  // input file whose hash is being computed
	//printf ("Input File name : %s \n", filename);
	numThreads = atoi(argv[2]);  		//numThreads value determines ht
	printf ("numThreads =  %d \n", numThreads);
	//use fstat to get file size
  FILE *fp = fopen(filename, "r");
	if (fp == NULL) {
		printf("Unable to open input file");
		exit(1);
	}

	// Compute the size of the input file
	fd = fileno(fp); 
	struct stat buf;
	fstat(fd, &buf);
	file_size = buf.st_size;
  //printf ("Size of input file is [%ld] bytes\n", file_size);
	//printf ("Each block is [%d] bytes \n", BLOCK_SIZE);

	// load the input file into memory map
	//printf ("Now loading the input file into memory map.... \n");
	ptr = mmap(NULL, file_size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
  if (ptr == MAP_FAILED) {
		printf("Mapping Failed\n");
		return 1;
	}

  // calculate nblocks 
	nblocks = file_size / BLOCK_SIZE;
	printf ("Number of blocks in input file is [%u] \n", nblocks);
	
	if ((nblocks < numThreads) || numThreads == 1) { // no multithreading
		// just call jenkins func and get a hash 
		printf ("Smaller input file or numThreads is 1 : so computing the hash directly... \n");
		start = GetTime();
		hash = jenkins_one_at_a_time_hash((uint8_t*)ptr, file_size);
		end = GetTime();
  		
		printf("hash value = %u \n", hash);
  	printf("time taken = %f \n", (end - start));
	
		close(fd);
  	fclose(fp);
	
		int err = munmap(ptr, buf.st_size);
  		if(err != 0) {
  			printf("UnMapping Failed\n");
    		return 1;
  		}
  		return EXIT_SUCCESS;
	}

  	//printf ("Number of threads handling work is [%d] \n", numThreads);
  	printf ("Number of blocks processed by each thread is [%d] \n", nblocks / numThreads);
 	
  	switch (numThreads) {
		case 1: 
			ht = 1;
			break;
		case 4:
			ht = 3;
			break;
		case 16:
			ht = 5;
			break;
  		case 32:
			ht = 6;
			break;
		case 64:
			ht = 7;
			break;
		case 128:
			ht = 8;
			break;
		case 256:
			ht = 9;
			break;
		case 512:
			ht = 10;
			break;
		case 1024:
			ht = 11;
			break;
		case 4096:
			ht = 12;
			break;
		default:
			printf ("numThreads should be one of: [1, 4, 16, 32, 64, 128, 256, 512, 1024, 4096] \n");
			exit(1);
	}
	//printf ("For [%d] threads the computed height of the tree is [%d] \n", numThreads, ht);
	// for larger files the following processing happens in the program
	// create a complete, balanced tree with calculated height

	start = GetTime();	// start the processing clock
	pthread_t p1;
	Pthread_create(&p1, NULL, tree, &ht);
	Pthread_join(p1, NULL);
	// when the tree is done computing temporary hashes for each node, we parse the array 
	char buffer[1024]; char l[15]; char r[15]; char ch[15];
	uint32_t current_hash = 0, left_hash = 0, right_hash = 0;
	for (int i = ( numThreads - 1 )/ 2; i >= 0; i--) {
		current_hash = threads[i].computed_hash;
		if ((i * 2)+1 < numThreads)
			left_hash = threads[(i * 2) + 1].computed_hash; 
		if ((i * 2)+2 < numThreads)
			right_hash = threads[(i * 2) + 2].computed_hash;
		sprintf(ch, "%u", current_hash);
		if (left_hash > 0)
			sprintf(l, "%u", left_hash);
		if (right_hash > 0)
			sprintf(r, "%u", right_hash);
		strcpy(buffer, ch);
		if (strlen(l) >= 1)
			strcat(buffer, l);
		if (strlen(r) >= 1)
			strcat(buffer, r);
		// now compute the hash of the hashes
		uint32_t new_hash = jenkins_one_at_a_time_hash((uint8_t*)buffer, strlen(buffer));
		//printf ("New computed hash value for the thread [%d] is [%u] \n", i, new_hash);
		threads[i].computed_hash = new_hash;
	} 
  end = GetTime();		// stop the processing clock
	hash = threads[0].computed_hash;
  printf("Final hash value of the file %s = [%u] \n", filename, hash);
  printf("time taken to compute hash value for the whole file = [%f] seconds\n", (end - start));
		
	
	// close files and exit
	close(fd);
  fclose(fp);
	int err = munmap(ptr, buf.st_size);
  	if(err != 0) {
  		printf("UnMapping Failed\n");
    	return 1;
  	}
	return EXIT_SUCCESS;
}

uint32_t jenkins_one_at_a_time_hash(const uint8_t* key, uint64_t length) 
{
	Pthread_mutex_lock(&mylock);
  	uint64_t i = 0;
  	uint32_t hash = 0;

  	while (i != length) {
    	hash += key[i++];
    	hash += hash << 10;
    	hash ^= hash >> 6;
  	}
  	hash += hash << 3;
  	hash ^= hash >> 11;
  	hash += hash << 15;
	Pthread_mutex_unlock(&mylock);
  	return hash;
}

void Usage(char* s) 
{
  fprintf(stderr, "Usage: %s filename num_threads \n", s);
  exit(EXIT_FAILURE);
}

struct Node
{
    int info;
    int level;
    struct Node* left, * right;
};

struct Node* newNode(int info, int ht)
{
    struct Node* node = (struct Node*) malloc(sizeof(struct Node));
    node->info = info;
    node->level = ht - 1;
    node->left = node->right = NULL;
    return (node);
}

struct Node* insert(ti arr[], int i, int n, int ht)
{
	struct Node *root = NULL;
	if (i < n) {
 		root = newNode(arr[i].thread_value, ht);
		root->left = insert(arr, 2 * i + 1, n, ht-1); 
		root->right = insert(arr, 2 * i + 2, n, ht-1);
  }
  return root;
}

int height(struct Node* node)
{
    if (node == NULL)
        return 0;
    else {
        /* compute the height of each subtree */
        int l = height(node->left);
        int r = height(node->right);

        /* use the larger one */
        if (l > r) {
            return (l + 1);
        }
        else {
            return (r + 1);
        }
    }
}

void print(struct Node* root, int level)
{
    if (root == NULL)
        return;
    if (level == 1)
        printf("%s Thread (id: %d) at height %d \n", root->level ? "Int.":"Leaf", root->info, root->level);
    else if (level > 1) {
        print(root->left, level - 1);
        print(root->right, level - 1);
    }
}

void levelOrder(struct Node* root) {
    int i;
		int ht = height(root);
    for (i = 1; i <= ht; i++) {
        print(root, i);
    }
}

int power(int a) {
	if (a < 0) {
		return -1;
	}
	if (a == 0) {
		return 1;
	}
	int ret = 1;
	int k;
	for (k = 0; k < a; k++) {
		ret = 2*ret;
	}
	return ret;
}

void* tree(void* arg) 
{
  int ht = *(int *)arg;	
	pthread_t thread_arr[numThreads];
	int q = file_size / numThreads;
	long start_index = 0, end_index = q - 1;
	// processing each thread
	for (int i = 0; i < numThreads; i++) {
		// printf ("Thread [%d] will process from [%ld] to [%ld] \n", i, start_index, end_index);
		threads[i].thread_value = thread_counter++;
		threads[i].start_location = start_index;
		threads[i].end_location = end_index;
		start_index += q;
		end_index += q;
		Pthread_create(&thread_arr[i], NULL, node, &i);
		Pthread_join(thread_arr[i], NULL);
	}
	struct Node* root = insert(threads, 0, numThreads, ht);
	levelOrder(root);
	return (void*)NULL;
}

void* node(void* arg)
{
	// here i can now refer to the threads array of structs to get the values to parse the memory map
	int my_identity = *(int *)arg;
	
	printf (" I am [%d] thread computing the hash for [%ld] to [%ld] ", 
					threads[my_identity].thread_value, 
					threads[my_identity].start_location,
					threads[my_identity].end_location);

	// each thread will now compute the hash for their chunk of memory
	long chunk_size = threads[my_identity].end_location - threads[my_identity].start_location;
	if (chunk_size < 4096) {
		// just give the whole chunk to one call to jenkins_func and we are returning the hash
		threads[my_identity].computed_hash = jenkins_one_at_a_time_hash((uint8_t*)ptr + threads[my_identity].start_location, chunk_size);
		printf (" and my computed hash is [%u] \n", threads[my_identity].computed_hash);
	}
	else {
		//Pthread_mutex_lock(&mymutex);
		// for larger chunks, we divide the total chunk by 4096, fill another array of hashes
		int numchunks = chunk_size / 4096;			// how big is the memory and how can we break into blocks
		uint32_t interim_hash[numchunks];			// each chunk of the big chunk gives a hash of type uint32_t
		char buffer[numchunks * sizeof(uint32_t)];
		if (buffer == NULL) {
			printf ("Could not allocate memory in thread [%d] for building temporary hash value string \n", my_identity);
			pthread_exit((void *)NULL);
		}
		char temp_hash_str[200];
		long start_at = threads[my_identity].start_location;
		for (int h = 0; h < numchunks; h++) {
			// first compute the interim hash
			interim_hash[h] = jenkins_one_at_a_time_hash((uint8_t*)ptr + start_at, 4096);
			start_at += 4096;
			// now copy all the interim hashes one behind the other to find the new value to be hashed
			sprintf(temp_hash_str, "%u", interim_hash[h]);
			if (h == 0)
				strcpy (buffer, temp_hash_str);
			else if (h >= 1)	
				strcat (buffer, temp_hash_str);
			temp_hash_str[0] = '\0';
		}
		threads[my_identity].computed_hash = jenkins_one_at_a_time_hash((uint8_t*)buffer, strlen(buffer));
		printf (" and my computed hash is [%u] \n", threads[my_identity].computed_hash);
		//free(buffer);
		//Pthread_mutex_unlock(&mymutex);
	}
	pthread_exit((void*)NULL);	
}
