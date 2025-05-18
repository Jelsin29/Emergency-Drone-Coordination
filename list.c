/**
 * @file list.c
 * @author adaskin
 * @brief  a simple doubly linked list stored in an array(contiguous
 * memory). this program is written for educational purposes 
 * and may include some bugs.
 * @version 0.2
 * @date 2024-05-19
 *
 * @copyright Copyright (c) 2024
 *
 */
#include "headers/list.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <semaphore.h>

// Forward declarations for internal functions
static Node *find_memcell_fornode(List *list);
static Node *get_free_node(List *list);

/**
 * @brief Create a list object, allocates new memory for list, and
 * sets its data members
 *
 * @param datasize: size of data in each node
 * @param capacity: maximum number of nodes can be stored in this list
 * @return List*
 */
List *create_list(size_t datasize, int capacity) {
    List *list = malloc(sizeof(List));
    if (!list) {
        perror("Failed to allocate memory for list");
        return NULL;
    }
    
    memset(list, 0, sizeof(List));

    // Initialize mutex
    pthread_mutex_init(&list->lock, NULL);
    
    // Initialize semaphores for overflow/underflow protection
    sem_init(&list->elements_sem, 0, 0);  // Initially empty
    sem_init(&list->spaces_sem, 0, capacity);  // Initially all spaces available

    list->datasize = datasize;
    list->nodesize = sizeof(Node) + datasize;

    list->startaddress = malloc(list->nodesize * capacity);
    if (!list->startaddress) {
        perror("Failed to allocate memory for list nodes");
        free(list);
        return NULL;
    }
    
    list->endaddress = list->startaddress + (list->nodesize * capacity);
    memset(list->startaddress, 0, list->nodesize * capacity);

    list->lastprocessed = (Node *)list->startaddress;
    list->free_list = NULL;  // Initialize the free list to NULL

    list->number_of_elements = 0;
    list->capacity = capacity;
    
    // Initialize all nodes as free and add them to the free list
    for (int i = 0; i < capacity; i++) {
        Node *node = (Node *)(list->startaddress + (i * list->nodesize));
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
 * @brief finds a memory cell in the mem area of list
 * @param list
 * @return Node*
 */
static Node *find_memcell_fornode(List *list) {
    Node *node = NULL;
    /*search lastprocessed---end*/
    Node *temp = list->lastprocessed;
    while ((char *)temp < list->endaddress) {
        if (temp->occupied == 0) {
            node = temp;
            break;
        } else {
            temp = (Node *)((char *)temp + list->nodesize);
        }
    }
    if (node == NULL) {
        /*search startaddress--lastprocessed*/
        temp = (Node *)list->startaddress;
        while (temp < list->lastprocessed) {
            if (temp->occupied == 0) {
                node = temp;
                break;
            } else {
                temp = (Node *)((char *)temp + list->nodesize);
            }
        }
    }
    return node;
}

/**
 * @brief Gets a node from the free list or finds one in memory
 * @param list
 * @return Node*
 */
static Node *get_free_node(List *list) {
    // First try to get a node from the free list
    if (list->free_list) {
        Node *node = list->free_list;
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
 * @brief find an unoccupied node in the array, and makes a node with
 * the given data and ADDS it to the HEAD of the list
 * @param list:
 * @param data: a data addrress, its size is determined from
 * list->datasize
 * @return * find,*
 */
Node *add(List *list, void *data) {
    Node *node = NULL;

    // Wait for an available space (semaphore)
    if (sem_wait(&list->spaces_sem) != 0) {
        perror("sem_wait failed in add");
        return NULL;
    }

    // Lock the list during operation
    pthread_mutex_lock(&list->lock);

    /*Check capacity (redundant with semaphore but kept for safety)*/
    if (list->number_of_elements >= list->capacity) {
        pthread_mutex_unlock(&list->lock);
        sem_post(&list->spaces_sem); // Release the space we waited for
        perror("list is full!");
        return NULL;
    }

    /*Get a free node from the free list or memory*/
    node = get_free_node(list);

    if (node != NULL) {
        /*create_node*/
        node->occupied = 1;
        memcpy(node->data, data, list->datasize);

        /*change new node into head*/
        if (list->head != NULL) {
            Node *oldhead = list->head;
            oldhead->prev = node;
            node->prev = NULL;
            node->next = oldhead;
        }

        list->head = node;
        list->lastprocessed = node;
        list->number_of_elements += 1;
        if (list->tail == NULL) {
            list->tail = list->head;
        }
        
        // Signal that we have an element
        sem_post(&list->elements_sem);
    } else {
        pthread_mutex_unlock(&list->lock);
        sem_post(&list->spaces_sem); // Release the space we waited for
        perror("Failed to find free node!");
        return NULL;
    }

    pthread_mutex_unlock(&list->lock);
    return node;
}

/**
 * @brief finds the node with the value same as the mem pointed by
 * data and removes that node. it returns temp->node
 * @param list
 * @param data
 * @return int: in success, it returns 0; if not found it returns 1.
 */
int removedata(List *list, void *data) {
    // Lock the list during operation
    pthread_mutex_lock(&list->lock);
    
    Node *temp = list->head;
    while (temp != NULL &&
           memcmp(temp->data, data, list->datasize) != 0) {
        temp = temp->next;
    }
    
    int result = 1; // Default: not found
    
    if (temp != NULL) {
        Node *prevnode = temp->prev;
        Node *nextnode = temp->next;
        if (prevnode != NULL) {
            prevnode->next = nextnode;
        }
        if (nextnode != NULL) {
            nextnode->prev = prevnode;
        }

        if (temp == list->head) {
            list->head = nextnode;
        }
        
        if (temp == list->tail) {
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
 * @brief removes the node from list->head, and copies its data into
 * dest, also returns it.
 * @param list
 * @param dest: address to cpy data
 * @return void*: if there is data, it returns address of dest; else
 * it returns NULL.
 */
void *pop(List *list, void *dest) {
    // Wait for an element to be available
    if (sem_wait(&list->elements_sem) != 0) {
        perror("sem_wait failed in pop");
        return NULL;
    }
    
    pthread_mutex_lock(&list->lock);
    
    void *result = NULL;
    
    if (list->head != NULL) {
        Node *node = list->head;
        
        // Update the head pointer
        list->head = node->next;
        if (list->head != NULL) {
            list->head->prev = NULL;
        } else {
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
        if (dest != NULL) {
            memcpy(dest, node->data, list->datasize);
            result = dest;
        }
        
        // Signal that we have a space
        sem_post(&list->spaces_sem);
    } else {
        // Should never happen due to semaphore, but just in case
        sem_post(&list->elements_sem); // Put the element back since we didn't use it
    }
    
    pthread_mutex_unlock(&list->lock);
    return result;
}

/**
 * @brief returns the data stored in the head of the list
 * @param list
 * @return void*: returns the address of head->data
 */
void *peek(List *list) {
    pthread_mutex_lock(&list->lock);
    
    void *result = NULL;
    if (list->head != NULL) {
        result = list->head->data;
    }
    
    pthread_mutex_unlock(&list->lock);
    return result;
}

/**
 * @brief removes the given node from the list, it returns removed
 * node.
 * @param list
 * @param node
 * @return int: in sucess, it returns 0; if node not found, it
 * returns 1.
 */
int removenode(List *list, Node *node) {
    pthread_mutex_lock(&list->lock);
    
    int result = 1; // Default: failure
    
    if (node != NULL) {
        Node *prevnode = node->prev;
        Node *nextnode = node->next;
        
        if (prevnode != NULL) {
            prevnode->next = nextnode;
        }
        
        if (nextnode != NULL) {
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
        if (node == list->tail) {
            list->tail = prevnode;
        }

        if (node == list->head) {
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
 * @brief deletes everything
 *
 * @param list
 */
void destroy(List *list) {
    pthread_mutex_destroy(&list->lock);
    sem_destroy(&list->elements_sem);
    sem_destroy(&list->spaces_sem);
    
    if (list->startaddress) {
        free(list->startaddress);
        list->startaddress = NULL;
    }
    
    free(list);
}

/**
 * @brief prints list starting from head
 *
 * @param list
 * @param print: aprint function for the object data.
 */
void printlist(List *list, void (*print)(void *)) {
    pthread_mutex_lock(&list->lock);
    
    Node *temp = list->head;
    while (temp != NULL) {
        print(temp->data);
        temp = temp->next;
    }
    
    pthread_mutex_unlock(&list->lock);
}

/**
 * @brief print list starting from tail
 *
 * @param list
 * @param print: print function
 */
void printlistfromtail(List *list, void (*print)(void *)) {
    pthread_mutex_lock(&list->lock);
    
    Node *temp = list->tail;
    while (temp != NULL) {
        print(temp->data);
        temp = temp->prev;
    }
    
    pthread_mutex_unlock(&list->lock);
}