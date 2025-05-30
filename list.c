/**
 * @file list.c
 * @brief Thread-safe doubly linked list implementation with contiguous memory allocation
 * @author Amar Daskin - Wilmer Cuevas - Jelsin Sanchez
 * @version 0.2
 * @date 2025-05-22
 * 
 * This module provides a high-performance, thread-safe doubly linked list
 * implementation designed for the emergency drone coordination system. It
 * features contiguous memory allocation, semaphore-based flow control,
 * and comprehensive synchronization for multi-threaded environments.
 * 
 * **Key Features:**
 * - Contiguous memory allocation for cache efficiency
 * - Thread-safe operations with mutex and semaphore protection
 * - Semaphore-based overflow/underflow prevention
 * - Free list management for efficient node reuse
 * - Function pointer interface for object-oriented usage
 * - Support for arbitrary data types through flexible array members
 * 
 * **Memory Management:**
 * - Pre-allocated contiguous memory block for all nodes
 * - Free list for efficient node recycling
 * - Zero-copy operations where possible
 * - Predictable memory usage with fixed capacity
 * 
 * **Thread Safety:**
 * - Mutex protection for all structural modifications
 * - Semaphore-based flow control (elements_sem, spaces_sem)
 * - Atomic operations for thread-safe access patterns
 * - Deadlock prevention through consistent locking order
 * 
 * **Performance Characteristics:**
 * - O(1) insertion and deletion at head/tail
 * - O(n) search operations for data matching
 * - Cache-friendly memory layout for iteration
 * - Minimal dynamic allocation during runtime
 * 
 * @copyright Copyright (c) 2024
 * 
 * @ingroup core_modules
 * @ingroup data_structures
 */

#include "headers/list.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <semaphore.h>

// Forward declarations for internal functions
/**
 * @brief Finds an unoccupied memory cell in the list's memory area
 * @param list The list to search in
 * @return Pointer to an unoccupied node or NULL if none available
 */
// clang-format off
static Node *find_memcell_fornode(List *list);

/**
 * @brief Gets a node from the free list or finds one in memory
 * @param list The list to get a node from
 * @return Pointer to a free node or NULL if none available
 */
static Node *get_free_node(List *list);

/**
 * @brief Create a list object, allocates new memory for list, and
 * sets its data members
 *
 * @param datasize Size of data in each node
 * @param capacity Maximum number of nodes can be stored in this list
 * @return Pointer to the new list or NULL on failure
 */
List *create_list(size_t datasize, int capacity)
// clang-format on
{
    // clang-format off
    List *list = malloc(sizeof(List));
    // clang-format on
    if (!list)
    {
        perror("Failed to allocate memory for list");
        return NULL;
    }

    memset(list, 0, sizeof(List));

    // Initialize mutex
    pthread_mutex_init(&list->lock, NULL);

    // Initialize semaphores for overflow/underflow protection
    sem_init(&list->elements_sem, 0, 0);      // Initially empty
    sem_init(&list->spaces_sem, 0, capacity); // Initially all spaces available

    list->datasize = datasize;
    list->nodesize = sizeof(Node) + datasize;

    list->startaddress = malloc(list->nodesize * capacity);
    if (!list->startaddress)
    {
        perror("Failed to allocate memory for list nodes");
        free(list);
        return NULL;
    }

    list->endaddress = list->startaddress + (list->nodesize * capacity);
    memset(list->startaddress, 0, list->nodesize * capacity);

    list->lastprocessed = (Node *)list->startaddress;
    list->free_list = NULL; // Initialize the free list to NULL

    list->number_of_elements = 0;
    list->capacity = capacity;

    // Initialize all nodes as free and add them to the free list
    for (int i = 0; i < capacity; i++)
    {
        // clang-format off
        Node *node = (Node *)(list->startaddress + (i * list->nodesize));
        // clang-format on
        node->occupied = 0;
        node->next = list->free_list;
        node->prev = NULL;
        if (list->free_list)
            list->free_list->prev = node;
        list->free_list = node;
    }

    /*ops*/
    list->self = list;
    list->add = add;
    list->removedata = removedata;
    list->removenode = removenode;
    list->pop = pop;
    list->peek = peek;
    list->destroy = destroy;
    list->printlist = printlist;
    list->printlistfromtail = printlistfromtail;
    return list;
}

/**
 * @brief Finds a memory cell in the mem area of list
 * 
 * Searches through the list's memory area for an unoccupied node
 * 
 * @param list The list to search in
 * @return Pointer to an unoccupied node or NULL if none available
 */
// clang-format off
static Node *find_memcell_fornode(List *list)
// clang-format on
{
    // clang-format off
    Node *node = NULL;
    /*search lastprocessed---end*/
    Node *temp = list->lastprocessed;
    while ((char *)temp < list->endaddress)
    // clang-format on
    {
        if (temp->occupied == 0)
        {
            node = temp;
            break;
        }
        else
        {
            // clang-format off
            temp = (Node *)((char *)temp + list->nodesize);
            // clang-format on
        }
    }
    if (node == NULL)
    {
        /*search startaddress--lastprocessed*/
        // clang-format off
        temp = (Node *)list->startaddress;
        // clang-format on
        while (temp < list->lastprocessed)
        {
            if (temp->occupied == 0)
            {
                node = temp;
                break;
            }
            else
            {
                // clang-format off
                temp = (Node *)((char *)temp + list->nodesize);
                // clang-format on
            }
        }
    }
    return node;
}

/**
 * @brief Gets a node from the free list or finds one in memory
 * 
 * First tries to retrieve a node from the free list, and if that fails,
 * searches for an unoccupied memory cell in the list's memory area
 * 
 * @param list The list to get a node from
 * @return Pointer to a free node or NULL if none available
 */
// clang-format off
static Node *get_free_node(List *list)
// clang-format on
{
    // First try to get a node from the free list
    if (list->free_list)
    {
        // clang-format off
        Node *node = list->free_list;
        // clang-format on
        list->free_list = node->next;
        if (list->free_list)
            list->free_list->prev = NULL;
        node->next = NULL;
        node->prev = NULL;
        return node;
    }

    // If free list is empty, use the original approach
    return find_memcell_fornode(list);
}

/**
 * @brief Find an unoccupied node in the array, and makes a node with
 * the given data and ADDS it to the HEAD of the list
 * 
 * Thread-safe implementation with semaphore protection against overflow
 * 
 * @param list The list to add to
 * @param data A data address, its size is determined from list->datasize
 * @return Pointer to the new node or NULL on failure
 */
// clang-format off
Node *add(List *list, void *data)
// clang-format on
{
    // clang-format off
    Node *node = NULL;
    // clang-format on

    // Wait for an available space (semaphore)
    if (sem_wait(&list->spaces_sem) != 0)
    {
        perror("sem_wait failed in add");
        return NULL;
    }

    // Lock the list during operation
    pthread_mutex_lock(&list->lock);

    /*Check capacity (redundant with semaphore but kept for safety)*/
    if (list->number_of_elements >= list->capacity)
    {
        pthread_mutex_unlock(&list->lock);
        sem_post(&list->spaces_sem); // Release the space we waited for
        perror("list is full!");
        return NULL;
    }

    /*Get a free node from the free list or memory*/
    node = get_free_node(list);

    if (node != NULL)
    {
        /*create_node*/
        node->occupied = 1;
        memcpy(node->data, data, list->datasize);

        /*change new node into head*/
        if (list->head != NULL)
        {
            // clang-format off
            Node *oldhead = list->head;
            // clang-format on
            oldhead->prev = node;
            node->prev = NULL;
            node->next = oldhead;
        }

        list->head = node;
        list->lastprocessed = node;
        list->number_of_elements += 1;
        if (list->tail == NULL)
        {
            list->tail = list->head;
        }

        // Signal that we have an element
        sem_post(&list->elements_sem);
    }
    else
    {
        pthread_mutex_unlock(&list->lock);
        sem_post(&list->spaces_sem); // Release the space we waited for
        perror("Failed to find free node!");
        return NULL;
    }

    pthread_mutex_unlock(&list->lock);
    return node;
}

/**
 * @brief Finds the node with the value same as the mem pointed by
 * data and removes that node
 * 
 * Thread-safe implementation that safely removes a node matching the provided data
 * 
 * @param list The list to remove from
 * @param data Pointer to data to match and remove
 * @return 0 on success, 1 if node not found
 */
int removedata(List *list, void *data)
{
    // Lock the list during operation
    pthread_mutex_lock(&list->lock);
    // clang-format off
    Node *temp = list->head;
    // clang-format on
    while (temp != NULL && memcmp(temp->data, data, list->datasize) != 0)
    {
        temp = temp->next;
    }

    int result = 1; // Default: not found

    if (temp != NULL)
    {
        // clang-format off
        Node *prevnode = temp->prev;
        Node *nextnode = temp->next;
        // clang-format on
        if (prevnode != NULL)
        {
            prevnode->next = nextnode;
        }
        if (nextnode != NULL)
        {
            nextnode->prev = prevnode;
        }

        if (temp == list->head)
        {
            list->head = nextnode;
        }

        if (temp == list->tail)
        {
            list->tail = prevnode;
        }

        // Add the node to the free list
        temp->next = list->free_list;
        temp->prev = NULL;
        if (list->free_list)
            list->free_list->prev = temp;
        list->free_list = temp;

        temp->occupied = 0;
        list->number_of_elements--;
        list->lastprocessed = temp;
        result = 0; // Success

        // Signal that we have a space
        sem_post(&list->spaces_sem);
    }

    pthread_mutex_unlock(&list->lock);
    return result;
}

/**
 * @brief Removes the node from the head of the list and copies its data into dest
 * 
 * Thread-safe implementation with semaphore protection against underflow
 * 
 * @param list The list to pop from
 * @param dest Address to copy data to (can be NULL)
 * @return If there is data, it returns address of dest; else it returns NULL
 */
// clang-format off
void *pop(List *list, void *dest)
// clang-format on
{
    // Wait for an element to be available
    if (sem_wait(&list->elements_sem) != 0)
    {
        perror("sem_wait failed in pop");
        return NULL;
    }

    pthread_mutex_lock(&list->lock);

    // clang-format off
    void *result = NULL;
    // clang-format on

    if (list->head != NULL)
    {
        // clang-format off
        Node *node = list->head;
        // clang-format on
        // Update the head pointer
        list->head = node->next;
        if (list->head != NULL)
        {
            list->head->prev = NULL;
        }
        else
        {
            // List is now empty
            list->tail = NULL;
        }

        // Add the node to the free list
        node->next = list->free_list;
        node->prev = NULL;
        if (list->free_list)
            list->free_list->prev = node;
        list->free_list = node;

        node->occupied = 0;
        list->number_of_elements--;
        list->lastprocessed = node;

        // Copy data if destination is provided
        if (dest != NULL)
        {
            memcpy(dest, node->data, list->datasize);
            result = dest;
        }

        // Signal that we have a space
        sem_post(&list->spaces_sem);
    }
    else
    {
        // Should never happen due to semaphore, but just in case
        sem_post(&list->elements_sem); // Put the element back since we didn't use it
    }

    pthread_mutex_unlock(&list->lock);
    return result;
}

/**
 * @brief Returns the data stored in the head of the list without removing it
 * 
 * Thread-safe implementation to examine the head element
 * 
 * @param list The list to peek at
 * @return Address of head->data or NULL if list is empty
 */
// clang-format off
void *peek(List *list)
// clang-format on
{
    pthread_mutex_lock(&list->lock);
    // clang-format off
    void *result = NULL;
    // clang-format on
    if (list->head != NULL)
    {
        result = list->head->data;
    }

    pthread_mutex_unlock(&list->lock);
    return result;
}

/**
 * @brief Removes the given node from the list
 * 
 * Thread-safe implementation to remove a specific node
 * 
 * @param list The list to remove from
 * @param node The node to remove
 * @return 0 on success, 1 if node not found
 */
// clang-format off
int removenode(List *list, Node *node)
// clang-format on
{
    pthread_mutex_lock(&list->lock);

    int result = 1; // Default: failure

    if (node != NULL)
    {
        // clang-format off
        Node *prevnode = node->prev;
        Node *nextnode = node->next;
        // clang-format on

        if (prevnode != NULL)
        {
            prevnode->next = nextnode;
        }

        if (nextnode != NULL)
        {
            nextnode->prev = prevnode;
        }

        // Add the node to the free list
        node->next = list->free_list;
        node->prev = NULL;
        if (list->free_list)
            list->free_list->prev = node;
        list->free_list = node;

        node->occupied = 0;

        list->number_of_elements--;

        /*update head, tail, lastprocess*/
        if (node == list->tail)
        {
            list->tail = prevnode;
        }

        if (node == list->head)
        {
            list->head = nextnode;
        }

        list->lastprocessed = node;
        result = 0; // Success

        // Signal that we have a space
        sem_post(&list->spaces_sem);
    }

    pthread_mutex_unlock(&list->lock);
    return result;
}

/**
 * @brief Deletes the list and all its resources
 * 
 * Frees all memory and destroys synchronization objects
 *
 * @param list The list to destroy
 */
// clang-format off
void destroy(List *list)
// clang-format on
{
    pthread_mutex_destroy(&list->lock);
    sem_destroy(&list->elements_sem);
    sem_destroy(&list->spaces_sem);

    if (list->startaddress)
    {
        free(list->startaddress);
        list->startaddress = NULL;
    }

    free(list);
}

/**
 * @brief Prints all elements in the list from head to tail
 *
 * @param list The list to print
 * @param print Function to print each element
 */
// clang-format off
void printlist(List *list, void (*print)(void *))
// clang-format on
{
    pthread_mutex_lock(&list->lock);

    // clang-format off
    Node *temp = list->head;
    // clang-format on
    while (temp != NULL)
    {
        print(temp->data);
        temp = temp->next;
    }

    pthread_mutex_unlock(&list->lock);
}

/**
 * @brief Prints all elements in the list from tail to head
 *
 * @param list The list to print
 * @param print Function to print each element
 */
// clang-format off
void printlistfromtail(List *list, void (*print)(void *))
// clang-format on
{
    pthread_mutex_lock(&list->lock);
    // clang-format off
    Node *temp = list->tail;
    // clang-format on
    while (temp != NULL)
    {
        print(temp->data);
        temp = temp->prev;
    }

    pthread_mutex_unlock(&list->lock);
}