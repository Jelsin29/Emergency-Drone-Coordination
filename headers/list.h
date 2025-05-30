/**
 * @file list.h
 * @author Amar Daskin - Wilmer Cuevas - Jelsin Sanchez
 * @brief Thread-safe doubly linked list implementation stored in contiguous memory
 * @version 0.2
 * @date 2025-05-22
 * 
 * @copyright Copyright (c) 2024
 */
#ifndef LIST_H
#define LIST_H

#include <time.h>
#include <pthread.h>
#include <semaphore.h>

/**
 * @struct node
 * @brief Structure representing a node in the linked list
 */
typedef struct node {
    struct node *prev; /**< Pointer to previous node */
    struct node *next; /**< Pointer to next node */
    char occupied;     /**< Flag indicating if the node is used (1) or free (0) */
    char data[];       /**< Flexible array member for storing data */
} Node;

/**
 * @struct list
 * @brief Thread-safe doubly linked list with synchronized access
 */
typedef struct list {
    //clang-format off
    Node *head;             /**< Pointer to the first node in the list */
    Node *tail;             /**< Pointer to the last node in the list */
    int number_of_elements; /**< Current number of elements in the list */
    int capacity;           /**< Maximum number of elements the list can hold */
    int datasize;           /**< Size of each data element in bytes */
    int nodesize;           /**< Total size of node including header and data */
    char *startaddress;     /**< Start address of allocated memory block */
    char *endaddress;       /**< End address of allocated memory block */
    Node *lastprocessed;    /**< Last node that was processed */
    Node *free_list;        /**< List of free nodes available for reuse */

    pthread_mutex_t lock; /**< Mutex for thread-safe access */
    sem_t elements_sem;   /**< Semaphore counting available elements */
    sem_t spaces_sem;     /**< Semaphore counting free spaces */

    /**< Function pointers for list operations */
    Node *(*add)(struct list *list, void *data);
    int (*removedata)(struct list *list, void *data);
    int (*removenode)(struct list *list, Node *node);
    void *(*pop)(struct list *list, void *dest);
    void *(*peek)(struct list *list);
    void (*destroy)(struct list *list);
    void (*printlist)(struct list *list, void (*print)(void *));
    void (*printlistfromtail)(struct list *list, void (*print)(void *));

    struct list *self; /**< Self reference for consistency checks */
    //clang-format on
} List;

/**
 * @brief Create a new thread-safe linked list
 * @param datasize Size of each data element in bytes
 * @param capacity Maximum number of elements the list can hold
 * @return Pointer to the new list or NULL on failure
 */
// clang-format off
List *create_list(size_t datasize, int capacity);

/**
 * @brief Remove a node from the list
 * @param list List to remove from
 * @param node Node to remove
 * @return 0 on success, 1 if node not found
 */
int removenode(List *list, Node *node);

/**
 * @brief Add a data element to the head of the list
 * @param list List to add to
 * @param data Pointer to data to add
 * @return Pointer to the new node or NULL on failure
 */
Node *add(List *list, void *data);

/**
 * @brief Remove a data element from the list
 * @param list List to remove from
 * @param data Pointer to data to match and remove
 * @return 0 on success, 1 if data not found
 */
int removedata(List *list, void *data);

/**
 * @brief Remove and return the head element
 * @param list List to pop from
 * @param dest Destination buffer for the data (can be NULL)
 * @return Pointer to dest or NULL if list is empty
 */
void *pop(List *list, void *dest);

/**
 * @brief Get the head element without removing it
 * @param list List to peek at
 * @return Pointer to the data in the head node or NULL if list is empty
 */
void *peek(List *list);

/**
 * @brief Free all resources used by the list
 * @param list List to destroy
 */
void destroy(List *list);

/**
 * @brief Print all elements in the list from head to tail
 * @param list List to print
 * @param print Function to print each element
 */
void printlist(List *list, void (*print)(void*));

/**
 * @brief Print all elements in the list from tail to head
 * @param list List to print
 * @param print Function to print each element
 */
void printlistfromtail(List *list, void (*print)(void*));
// clang-format on
#endif // LIST_H