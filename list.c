/**
 * @file list.c
 * @author adaskin
 * @brief  a simple doubly linked list stored in an array(contigous
 * memory). this program is written for educational purposes 
 * and may include some bugs.
 * TODO: add synchronization
 * @version 0.1
 * @date 2024-04-21
 *
 * @copyright Copyright (c) 2024
 *
 */
#include "headers/list.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


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
    memset(list, 0, sizeof(List));

    pthread_mutex_init(&list->lock, NULL);

    list->datasize = datasize;
    list->nodesize = sizeof(Node) + datasize;

    list->startaddress = malloc(list->nodesize * capacity);
    list->endaddress =
        list->startaddress + (list->nodesize * capacity);
    memset(list->startaddress, 0, list->nodesize * capacity);

    list->lastprocessed = (Node *)list->startaddress;

    list->number_of_elements = 0;
    list->capacity = capacity;

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
    return list;  // Added missing return statement
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
 * @brief find an unoccupied node in the array, and makes a node with
 * the given data and ADDS it to the HEAD of the list
 * @param list:
 * @param data: a data addrress, its size is determined from
 * list->datasize
 * @return * find,*
 */
Node *add(List *list, void *data) {
    Node *node = NULL;

    // Lock the list during operation
    pthread_mutex_lock(&list->lock);

    /*Check capacity*/
    if (list->number_of_elements >= list->capacity) {
        pthread_mutex_unlock(&list->lock);
        perror("list is full!");
        return NULL;
    }

    /*first find an unoccupied memcell and insert into it*/
    node = find_memcell_fornode(list);

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
    } else {
        pthread_mutex_unlock(&list->lock);
        perror("list is full!");
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

        temp->next = NULL;
        temp->prev = NULL;
        temp->occupied = 0;
        list->number_of_elements--;
        list->lastprocessed = temp;
        result = 0; // Success
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
        
        node->next = NULL;
        node->prev = NULL;
        node->occupied = 0;
        list->number_of_elements--;
        list->lastprocessed = node;
        
        // Copy data if destination is provided
        if (dest != NULL) {
            memcpy(dest, node->data, list->datasize);
            result = dest;
        }
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
        
        node->next = NULL;
        node->prev = NULL;
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
    free(list->startaddress);
    // Removed redundant memset before free
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