#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "myalloc.h"

pthread_mutex_t mut;

struct header
{
    size_t size;
};

struct list
{
    void *chunk;
    struct list *next;
};

struct Myalloc
{
    enum allocation_algorithm aalgorithm;
    int size;
    void *memory;
    // Some other data members you want,
    // such as lists to record allocated/free memory
    struct list *freelist;
    struct list *alloclist;
};

struct Myalloc myalloc;

//Simple function to insert node into our list. Had to be done many times throughout the project
void listInsert(struct list **head, struct list *node)
{
    if (*head == NULL || (*head)->chunk > node->chunk)
    {
        node->next = *head;
        *head = node;
    }
    else
    {
        struct list *prev = *head;
        struct list *t = (*head)->next;
        while (t != NULL && t->chunk < node->chunk)
        {
            prev = t;
            t = t->next;
        }
        prev->next = node;
        prev->next->next = t;
    }
}

//Simple function to remove node from our list. Had to be done many times throughout the project
void listRemove(struct list **head, struct list *node)
{
    if (*head == node)
    {
        *head = node->next;
    }
    else
    {
        struct list *prev = *head;
        while (prev->next != NULL && prev->next != node)
        {
            prev = prev->next;
        }
        prev->next = prev->next->next;
    }
}

//Searches list for a chunk value and returns entry in the list with that chunk
struct list *listSearch(struct list *head, void *address)
{
    struct list *temp = head;
    while (temp->chunk != address)
    {
        temp = temp->next;
    }
    return temp;
}

//A function used for testing purposes
void listcheck(struct list *head)
{
    int num = 0;
    struct list *current = head;
    while (current != NULL)
    {
        printf("%p\n", current->chunk);
        printf("%d\n", (int)((struct header *)(current->chunk - 8))->size);
        num += 1;
        current = current->next;
    }
    printf("Number of items: %d\n", num);
}

//Initializes our allocator
void initialize_allocator(int _size, enum allocation_algorithm _aalgorithm)
{
    assert(_size > 0);
    myalloc.aalgorithm = _aalgorithm;
    myalloc.size = _size;
    myalloc.memory = malloc((size_t)myalloc.size);

    // Add some other initialization
    memset(myalloc.memory, 0, (size_t)myalloc.size);
    myalloc.freelist = malloc(sizeof(struct list));
    myalloc.freelist->chunk = myalloc.memory + sizeof(struct header);
    myalloc.freelist->next = NULL;

    //Making and inserting the first header into memory
    struct header head;
    head.size = (size_t)myalloc.size - sizeof(struct header);
    memcpy(myalloc.memory, &head, sizeof(struct header));

    myalloc.alloclist = NULL;

}

//Frees the memory we have stored in our lists to avoid memory leaks
void destroy_allocator()
{
    pthread_mutex_lock(&mut);
    free(myalloc.memory);

    // free other dynamic allocated memory to avoid memory leak
    //Freeing allocated list
    while (myalloc.alloclist != NULL)
    {
        struct list *t = myalloc.alloclist;
        myalloc.alloclist = myalloc.alloclist->next;
        free(t);
    }
    //Freeing freelist
    while (myalloc.freelist != NULL)
    {
        struct list *t = myalloc.freelist;
        myalloc.freelist = myalloc.freelist->next;
        free(t);
    }
    pthread_mutex_unlock(&mut);
}

void *allocate(int _size)
{
    pthread_mutex_lock(&mut);
    void *ptr = NULL;
    size_t remainder = 0;
    // Allocate memory from myalloc.memory
    // ptr = address of allocated memory
    if (_size != 0)
    {
        if (myalloc.aalgorithm == 0)
        {
            struct list *t = myalloc.freelist;
            //Iterating through the list, finding where the allocation will fit
            while (t != NULL)
            {
                //CHeck if allocation will fit
                if ((int)((struct header *)(t->chunk - sizeof(struct header)))->size >= _size)
                {
                    //Check if there is enough room for another chunk or not. If so, create a new header and place it in memory
                    if ((int)((struct header *)(t->chunk - sizeof(struct header)))->size - (size_t)_size > 8)
                    {
                        struct header head;
                        head.size = ((struct header *)(t->chunk - sizeof(struct header)))->size - (size_t)_size - (size_t)8;
                        memcpy(t->chunk + _size, &head, sizeof(struct header));
                        struct list *newnode = malloc(sizeof(struct list));
                        newnode->chunk = t->chunk + (size_t)(_size + 8);
                        listInsert(&myalloc.freelist, newnode);
                    }
                    else //Case where we have 8 or less bytes left over, allocate them to the user
                    {
                        remainder = ((struct header *)(t->chunk - sizeof(struct header)))->size - (size_t)_size;
                    }
                    //Finally remove from our free list, add to allocated list and set the header value properly
                    listRemove(&myalloc.freelist, t);
                    listInsert(&myalloc.alloclist, t);
                    ((struct header *)(t->chunk - sizeof(struct header)))->size = (size_t)(_size + remainder);
                    ptr = t->chunk;
                    break;
                }
                t = t->next;
            }
        } //Best fit algorithm
        else if (myalloc.aalgorithm == 1)
        {
            struct list *t = myalloc.freelist;
            struct list *best = NULL;
            int fragment = myalloc.size;
            //Iterating to find the best fit entry
            while (t != NULL)
            {
                if (((struct header *)(t->chunk - sizeof(struct header)))->size >= (size_t)_size && (int)((struct header *)(t->chunk - sizeof(struct header)))->size - _size < fragment)
                {
                    fragment = ((struct header *)(t->chunk - sizeof(struct header)))->size - _size;
                    best = t;
                }
                t = t->next;
            }
            //Now we have the list entry with the best fit, set it up left side oriented with header as done above in alg 0
            if (best != NULL)
            {
                if (((struct header *)(best->chunk - sizeof(struct header)))->size - (size_t)_size > (size_t)8)
                {
                    struct header head;
                    head.size = ((struct header *)(best->chunk - sizeof(struct header)))->size - (size_t)_size - (size_t)8;
                    memcpy(best->chunk + _size, &head, sizeof(struct header));
                    struct list *newnode = malloc(sizeof(struct list));
                    newnode->chunk = best->chunk + (size_t)(_size + 8);
                    listInsert(&myalloc.freelist, newnode);
                }
                else //Case where we have 8 or less bytes left over, allocate them to the user
                {
                    remainder = ((struct header *)(best->chunk - sizeof(struct header)))->size - (size_t)_size;
                }
                //Finally remove from freelist and insert into allocated list
                listRemove(&myalloc.freelist, best);
                listInsert(&myalloc.alloclist, best);
                ((struct header *)(best->chunk - sizeof(struct header)))->size = (size_t)(_size + remainder);
                ptr = best->chunk;
            }
        }//Worst fit algorithm
        else if (myalloc.aalgorithm == 2)
        {
            struct list *t = myalloc.freelist;
            struct list *worst = NULL;
            int fragment = -1;
            //Iterating to find the worst fit entry
            while (t != NULL)
            {
                if (((struct header *)(t->chunk - sizeof(struct header)))->size >= (size_t)_size && (int)((struct header *)(t->chunk - sizeof(struct header)))->size - _size > fragment)
                {
                    fragment = ((struct header *)(t->chunk - sizeof(struct header)))->size - _size;
                    worst = t;
                }
                t = t->next;
            }
            //Now we have the list entry with the worst fit, set it up left side oriented with header as done above in alg 0
            if (worst != NULL)
            {
                if (((struct header *)(worst->chunk - sizeof(struct header)))->size - (size_t)_size > (size_t)8)
                {
                    struct header head;
                    head.size = ((struct header *)(worst->chunk - sizeof(struct header)))->size - (size_t)_size - (size_t)8;
                    memcpy(worst->chunk + _size, &head, sizeof(struct header));
                    struct list *newnode = malloc(sizeof(struct list));
                    newnode->chunk = worst->chunk + (size_t)(_size + 8);
                    listInsert(&myalloc.freelist, newnode);
                }
                else //Case where we have 8 or less bytes left over, allocate them to the user
                {
                    remainder = ((struct header *)(worst->chunk - sizeof(struct header)))->size - (size_t)_size;
                }
                //Finally remove from free list and insert into allocated list
                listRemove(&myalloc.freelist, worst);
                listInsert(&myalloc.alloclist, worst);
                ((struct header *)(worst->chunk - sizeof(struct header)))->size = (size_t)(_size + remainder);
                ptr = worst->chunk;
            }
        }
    }
    pthread_mutex_unlock(&mut);
    return ptr;
}
//Deallocated memory at chunk value ptr
void deallocate(void *_ptr)
{
    //There are two cases, either this was the last allocated chunk and we need to merge all free chunks,
    // or this was not the last and we need to iterate and find them
    pthread_mutex_lock(&mut);
    assert(_ptr != NULL);
    int size = (int)((struct header *)((char *)_ptr - 8))->size;
    struct list *temp = listSearch(myalloc.alloclist, _ptr);
    listRemove(&myalloc.alloclist, temp);
    listInsert(&myalloc.freelist, temp);
    memset(_ptr, 0, (size_t)size);
    //This is the case where it was the last allocated entry, we just merge all adjacent free list entries
    if (myalloc.alloclist == NULL)
    {
        while (myalloc.freelist != NULL)
        {
            struct list *t = myalloc.freelist;
            myalloc.freelist = myalloc.freelist->next;
            free(t);
        }
        memset(myalloc.memory, 0, (size_t)myalloc.size);
        myalloc.freelist = malloc(sizeof(struct list));
        myalloc.freelist->next = NULL;
        myalloc.freelist->chunk = myalloc.memory + sizeof(struct header);
        struct header newHeader;
        newHeader.size = (size_t)myalloc.size - sizeof(struct header);
        memcpy(myalloc.memory, &newHeader, sizeof(struct header));
    }

    //Other case, we need to check if adjacent entries in the free list are adjacent in memory, and if so merge them
    struct list *current = myalloc.freelist;
    while (current->next != NULL)
    {
        if (current->chunk + ((struct header *)(current->chunk - 8))->size + sizeof(struct header) == current->next->chunk)
        {
            void *t = current->next->chunk;
            ((struct header *)(current->chunk - 8))->size += ((struct header *)(current->next->chunk - 8))->size + 8;
            struct list *temp = listSearch(myalloc.freelist, t);
            listRemove(&myalloc.freelist, temp);
            free(temp);
            continue;
        }
        current = current->next;
    }
    pthread_mutex_unlock(&mut);
}
//Compacting all allocations into the beginning of the memory space
int compact_allocation(void **_before, void **_after)
{
    pthread_mutex_lock(&mut);
    int compacted_size = 0;
    struct list *current = myalloc.alloclist;
    void *destination = myalloc.memory;
    //Iterating through the list and checking if the entry needs to be moved back
    while (current != NULL)
    {
        if (destination < current->chunk - 8)
        {
            memmove(destination, current->chunk - 8, ((struct header *)(current->chunk - 8))->size + 8);
            _before[compacted_size] = current->chunk;
            current->chunk = destination + 8;
            _after[compacted_size] = current->chunk;
            compacted_size += 1;
        }
        current = current->next;
        destination += ((struct header *)(destination))->size + 8;
    }
    //Freeing all entries in the free list, to create one new one
    while (myalloc.freelist != NULL)
    {
        struct list *t = myalloc.freelist;
        myalloc.freelist = myalloc.freelist->next;
        free(t);
    }

    //Creating the new entry in the free list and setting up the header
    memset(destination, 0, (size_t)myalloc.size + myalloc.memory - destination);
    myalloc.freelist = malloc(sizeof(struct list));
    myalloc.freelist->next = NULL;
    myalloc.freelist->chunk = destination + sizeof(struct header);
    struct header newHeader;
    newHeader.size = (size_t)myalloc.size + myalloc.memory - destination - sizeof(struct header);
    memcpy(destination, &newHeader, sizeof(struct header));
    pthread_mutex_unlock(&mut);

    return compacted_size;
}
//Checking size of available memory
int available_memory()
{
    pthread_mutex_lock(&mut);
    int available_memory_size = 0;
    // Calculate available memory size
    struct list *current = myalloc.freelist;
    //Iterate through free list and sum header values
    while (current != NULL)
    {
        available_memory_size += ((struct header *)(current->chunk - sizeof(struct header)))->size;
        current = current->next;
    }

    pthread_mutex_unlock(&mut);

    return available_memory_size;
}

void print_statistics()
{
    pthread_mutex_lock(&mut);
    int allocated_size = 0;
    int allocated_chunks = 0;
    int free_size = 0;
    int free_chunks = 0;
    int smallest_free_chunk_size = myalloc.size;
    int largest_free_chunk_size = 0;
    // Calculate the statistics
    struct list *currentalloc = myalloc.alloclist;
    struct list *currentfree = myalloc.freelist;

    while (currentalloc != NULL)
    {
        allocated_size += (int)((struct header *)(currentalloc->chunk - 8))->size;
        allocated_chunks += 1;
        currentalloc = currentalloc->next;
    }

    while (currentfree != NULL)
    {
        free_size += (int)((struct header *)(currentfree->chunk - 8))->size;
        if ((int)((struct header *)(currentfree->chunk - 8))->size < smallest_free_chunk_size)
        {
            smallest_free_chunk_size = (int)((struct header *)(currentfree->chunk - 8))->size;
        }
        if ((int)((struct header *)(currentfree->chunk - 8))->size > largest_free_chunk_size)
        {
            largest_free_chunk_size = (int)((struct header *)(currentfree->chunk - 8))->size;
        }
        free_chunks += 1;
        currentfree = currentfree->next;
    }

    pthread_mutex_unlock(&mut);
    free_size = available_memory();
    printf("Allocated size = %d\n", allocated_size);
    printf("Allocated chunks = %d\n", allocated_chunks);
    printf("Free size = %d\n", free_size);
    printf("Free chunks = %d\n", free_chunks);
    printf("Largest free chunk size = %d\n", largest_free_chunk_size);
    printf("Smallest free chunk size = %d\n", smallest_free_chunk_size);
}
