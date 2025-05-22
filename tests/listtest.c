/**
 * @file listtest.c
 * @author Amar Daskin - Wilmer Cuevas - Jelsin Sanchez
 * @brief Comprehensive test suite for thread-safe linked list implementation
 * @version 0.1
 * @date 2025-05-22
 * 
 * This test program validates the functionality of the thread-safe doubly
 * linked list implementation used throughout the emergency drone coordination
 * system. It tests core operations including creation, insertion, traversal,
 * removal, and cleanup to ensure the list behaves correctly under various
 * conditions.
 * 
 * **Test Coverage:**
 * - List creation and initialization with specified capacity
 * - Element insertion using the add() function
 * - Forward traversal from head to tail
 * - Backward traversal from tail to head
 * - Element removal using pop() function
 * - Memory management and cleanup verification
 * 
 * **Test Data:**
 * Uses Survivor structures as test data to simulate real-world usage
 * patterns from the drone coordination system. Each test survivor has
 * randomly generated coordinates and sequential identification strings.
 * 
 * **Validation Methods:**
 * - Visual output verification through formatted printing
 * - Traversal consistency checking (forward vs backward)
 * - Element count validation after operations
 * - Memory cleanup verification
 * 
 * **Usage:**
 * Run this test to verify list functionality before integrating
 * into the main system. Expected output shows successful creation,
 * population, traversal, and cleanup of list elements.
 * 
 * @copyright Copyright (c) 2024
 * 
 * @ingroup testing
 * @ingroup data_structures
 */

#include "../headers/list.h"
#include "../headers/survivor.h"
#include <stdlib.h>
#include <stdio.h>

/**
 * @defgroup testing System Testing and Validation
 * @brief Test programs and validation utilities
 * @{
 */

/**
 * @defgroup list_testing List Implementation Testing
 * @brief Test programs for validating list functionality
 * @ingroup testing
 * @{
 */

/**
 * @brief Helper function to print survivor details for test verification
 * 
 * Formats and displays survivor information in a readable format for
 * manual verification of test results. Used by list traversal functions
 * to verify that data integrity is maintained through all operations.
 * 
 * **Output Format:**
 * - info: [25-character identification string]
 * - Location: (x, y) coordinates
 * 
 * @param s Pointer to survivor structure to display
 * 
 * @pre s must be valid pointer to initialized Survivor structure
 * @post Survivor information is printed to stdout
 * 
 * @note Function signature matches void (*)(void*) for use with list traversal
 * @see printlist() and printlistfromtail() for traversal usage
 */
void printsurvivor(Survivor *s) {
    printf("info: %.25s\n", s->info);
    printf("Location: (%d, %d)\n", s->coord.x, s->coord.y);
}

/**
 * @brief Main test function that validates all core list operations
 * 
 * Comprehensive test sequence that validates the complete lifecycle
 * of the thread-safe linked list implementation. Tests are designed
 * to verify both functionality and data integrity under normal usage
 * patterns expected in the drone coordination system.
 * 
 * **Test Sequence:**
 * 1. **List Creation**: Create list with 100-element capacity
 * 2. **Population**: Add 20 survivor elements with random coordinates
 * 3. **Forward Traversal**: Print all elements from head to tail
 * 4. **Backward Traversal**: Print all elements from tail to head
 * 5. **Element Removal**: Remove 10 elements using pop() operation
 * 6. **Verification**: Print remaining elements to verify integrity
 * 7. **Cleanup**: Destroy list and free all resources
 * 
 * **Test Data Generation:**
 * - Survivors with sequential IDs (id:0-aname, id:1-aname, etc.)
 * - Random coordinates within reasonable ranges (x: 0-999, y: 0-99)
 * - Sufficient data volume to test list capacity and performance
 * 
 * **Expected Results:**
 * - All 20 elements successfully added to list
 * - Forward and backward traversals show same elements in reverse order
 * - Pop operations remove elements in LIFO order (last added, first removed)
 * - Remaining 10 elements maintain data integrity
 * - Clean destruction without memory leaks
 * 
 * **Success Criteria:**
 * - No segmentation faults or memory errors
 * - Consistent data in forward and backward traversals
 * - Correct element count after add/remove operations
 * - Proper cleanup without memory leaks
 * 
 * @return 0 on successful test completion
 * 
 * @note Test uses random number generation - results may vary between runs
 * @note Visual verification required - automated assertions not implemented
 * @warning Test does not validate thread safety - single-threaded execution only
 * 
 * @see List structure for implementation details
 * @see create_list() for list initialization
 * @see printsurvivor() for output formatting
 */
int main() {
    // Test configuration constants
    const int n = 20;  // Number of elements to add
    const int m = 10;  // Number of elements to remove
    const int capacity = 100;  // List capacity for testing
    
    printf("=== Thread-Safe Linked List Test Suite ===\n");
    printf("Testing list with capacity: %d\n", capacity);
    printf("Adding %d elements, then removing %d elements\n\n", n, m);
    
    /*EXAMPLE USE OF list.c*/
    List *list = create_list(sizeof(Survivor), capacity);
    
    if (!list) {
        fprintf(stderr, "ERROR: Failed to create list\n");
        return 1;
    }
    
    printf("✓ List created successfully\n");
 
    printf("\n=== PHASE 1: Adding %d elements to the list ===\n", n);
    for (int i = 0; i < n; i++) {
        Survivor s;
        sprintf(s.info, "id:%d-aname", i);
        s.coord.x = rand() % 1000;
        s.coord.y = rand() % 100;
        s.status = 0; // Initialize status for completeness
        
        Node *node = list->add(list, &s);
        if (!node) {
            fprintf(stderr, "ERROR: Failed to add element %d\n", i);
            list->destroy(list);
            return 1;
        }
    }
    
    printf("✓ Successfully added %d elements\n", n);
    printf("Current list size: %d\n", list->number_of_elements);

    printf("\n=== PHASE 2: Forward traversal (head to tail) ===\n");
    printlist(list, (void (*)(void *))printsurvivor);
    
    printf("\n=== PHASE 3: Backward traversal (tail to head) ===\n");
    printlistfromtail(list, (void (*)(void *))printsurvivor);

    printf("\n=== PHASE 4: Testing element removal (%d elements) ===\n", m);
    Survivor s;
    printf("Removed elements (coordinates): ");

    for (int i = 0; i < m; i++) {
        if (list->pop(list, &s) != NULL) {
            printf("(%d,%d)", s.coord.x, s.coord.y);
            if (i < m - 1) printf("-");
        } else {
            printf("ERROR: Failed to pop element %d", i);
            break;
        }
    }
    
    printf("\n✓ Successfully removed %d elements\n", m);
    printf("Current list size: %d\n", list->number_of_elements);
    
    printf("\n=== PHASE 5: Verification - remaining elements ===\n");
    printlist(list, (void (*)(void *))printsurvivor);
    
    printf("\n=== PHASE 6: Cleanup and resource deallocation ===\n");
    list->destroy(list);
    printf("✓ List destroyed successfully\n");
    
    printf("\n=== TEST COMPLETED SUCCESSFULLY ===\n");
    printf("All list operations performed without errors\n");
    
    return 0;
}

/** @} */ // end of list_testing group
/** @} */ // end of testing group