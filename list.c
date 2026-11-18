// list/list.c
//
// Implementation for linked list.
//
// Memory ownership rules used throughout this file:
//   - A list owns its nodes. Removing a node from a list frees the node.
//   - A node does NOT own its block. Blocks move between the free list and the
//     allocated list, so removing a node hands the block back to the caller
//     instead of freeing it. node_free() therefore frees only the node.
//   - A block is freed only when it stops existing: when coalescing merges it
//     into its neighbour, or when list_free() tears down a whole list.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "list.h"

/* Creates an empty list. */
list_t *list_alloc() {
  list_t *list = malloc(sizeof(list_t));
  list->head = NULL;
  return list;
}

/* Creates a node that points to blk. The node does not copy the block. */
node_t *node_alloc(block_t *blk) {
  node_t *node = malloc(sizeof(node_t));
  node->next = NULL;
  node->blk = blk;
  return node;
}

/* Frees one node but not its block (see the ownership rules at the top). */
void node_free(node_t *node){
  free(node);
}

/* Frees the whole list: every node, every block the nodes point to, and the
 * list struct itself. Only call this when the blocks are no longer needed. */
void list_free(list_t *l){
  node_t *current = l->head;
  while (current != NULL) {
    node_t *next = current->next;
    free(current->blk);
    node_free(current);
    current = next;
  }
  free(l);
}

/* Prints every block in the list, one per line. */
void list_print(list_t *l) {
  node_t *current = l->head;

  if (current == NULL){
    printf("list is empty\n");
  }
  while (current != NULL){
    block_t *b = current->blk;
    printf("PID=%d START:%d END:%d\n", b->pid, b->start, b->end);
    current = current->next;
  }
}

/* Returns the number of nodes in the list. */
int list_length(list_t *l) {
  node_t *current = l->head;
  int i = 0;
  while (current != NULL){
    i++;
    current = current->next;
  }
  return i;
}

/* Adds blk at the end of the list. Used by First Fit, which keeps free
 * blocks in the order they were returned (FIFO). */
void list_add_to_back(list_t *l, block_t *blk){
  node_t *newNode = node_alloc(blk);

  if (l->head == NULL){
    l->head = newNode;
    return;
  }

  node_t *current = l->head;
  while (current->next != NULL){
    current = current->next;
  }
  current->next = newNode;
}

/* Adds blk at the start of the list. */
void list_add_to_front(list_t *l, block_t *blk){
  node_t *newNode = node_alloc(blk);
  newNode->next = l->head;
  l->head = newNode;
}

/* Inserts blk so that it ends up at position index (0 = new head).
 * An index past the end adds blk to the back. */
void list_add_at_index(list_t *l, block_t *blk, int index){
  // Index 0, or an empty list: blk becomes the new head.
  if (index <= 0 || l->head == NULL) {
    list_add_to_front(l, blk);
    return;
  }

  // Stop on the node just before the target position.
  node_t *current = l->head;
  int i = 0;
  while (i < index - 1 && current->next != NULL){
    current = current->next;
    i++;
  }

  node_t *newNode = node_alloc(blk);
  newNode->next = current->next;
  current->next = newNode;
}

/* Inserts newblk in ascending order of START address. Used for the allocated
 * list, and to sort the free list by address before coalescing. */
void list_add_ascending_by_address(list_t *l, block_t *newblk){
  node_t *newNode = node_alloc(newblk);

  // Empty list, or newblk starts before the head: it becomes the new head.
  if (l->head == NULL || newblk->start < l->head->blk->start) {
    newNode->next = l->head;
    l->head = newNode;
    return;
  }

  // Walk forward while the next node still starts before newblk.
  node_t *current = l->head;
  while (current->next != NULL && current->next->blk->start < newblk->start) {
    current = current->next;
  }

  // Link newblk in right after current.
  newNode->next = current->next;
  current->next = newNode;
}

/* Returns the number of memory units a block covers. START and END are both
 * inclusive, so a block from 0 to 49 holds 50 units. */
static int blocksize(block_t *blk) {
  int size = blk->end - blk->start;
  size = size + 1;
  return size;
}

/* Inserts newblk in ascending order of block size (smallest first). Used by
 * Best Fit, so the first block that fits is also the tightest fit.
 * Blocks of equal size keep their arrival order: newblk goes after them. */
void list_add_ascending_by_blocksize(list_t *l, block_t *newblk){
  node_t *newNode = node_alloc(newblk);
  int newSize = blocksize(newblk);

  // Empty list: newblk becomes the head.
  if (l->head == NULL) {
    l->head = newNode;
    return;
  }

  // newblk is smaller than the head: it becomes the new head.
  if (newSize < blocksize(l->head->blk)) {
    newNode->next = l->head;
    l->head = newNode;
    return;
  }

  // Walk forward while the next block is the same size or smaller.
  node_t *current = l->head;
  while (current->next != NULL && blocksize(current->next->blk) <= newSize) {
    current = current->next;
  }

  // Link newblk in right after current.
  newNode->next = current->next;
  current->next = newNode;
}

/* Inserts newblk in descending order of block size (largest first). Used by
 * Worst Fit, so the first block that fits is also the largest one.
 * Blocks of equal size keep their arrival order: newblk goes after them.
 * (The starter version computed the new block's size without the +1, which
 * made it disagree with every other size calculation in this file.) */
void list_add_descending_by_blocksize(list_t *l, block_t *newblk){
  node_t *newNode = node_alloc(newblk);
  int newSize = blocksize(newblk);

  // Empty list: newblk becomes the head.
  if (l->head == NULL) {
    l->head = newNode;
    return;
  }

  // newblk is larger than the head: it becomes the new head.
  if (newSize > blocksize(l->head->blk)) {
    newNode->next = l->head;
    l->head = newNode;
    return;
  }

  // Walk forward while the next block is the same size or larger.
  node_t *current = l->head;
  while (current->next != NULL && blocksize(current->next->blk) >= newSize) {
    current = current->next;
  }

  // Link newblk in right after current.
  newNode->next = current->next;
  current->next = newNode;
}

/* Merges physically adjacent free blocks. The list must already be sorted by
 * address. Two blocks are adjacent when the first ends right before the
 * second starts (prev.END + 1 == current.START). The first block is stretched
 * to cover the second, and the second block and its node are both freed. */
void list_coalese_nodes(list_t *l){
  if (l->head == NULL) {
    return;
  }

  node_t *prev = l->head;
  node_t *current = prev->next;

  while (current != NULL) {
    if (prev->blk->end + 1 == current->blk->start) {
      // Adjacent: stretch prev to cover current.
      prev->blk->end = current->blk->end;

      // Unlink current, then free its block and its node. The block is
      // freed here because its memory range now belongs to prev's block.
      prev->next = current->next;
      free(current->blk);
      node_free(current);

      // Stay on prev, since the new next block may also be adjacent.
      current = prev->next;
    }
    else {
      // Not adjacent: move both pointers forward one node.
      prev = current;
      current = current->next;
    }
  }
}

/* Removes the last node and returns its block, or NULL if the list is empty.
 * (The starter version left a dangling head on a one-node list and returned
 * the second-to-last block instead of the last one.) */
block_t* list_remove_from_back(list_t *l){
  if (l->head == NULL) {
    return NULL;
  }

  // One node: the list becomes empty.
  if (l->head->next == NULL) {
    block_t *value = l->head->blk;
    node_free(l->head);
    l->head = NULL;
    return value;
  }

  // Two or more nodes: stop on the second-to-last node.
  node_t *current = l->head;
  while (current->next->next != NULL){
    current = current->next;
  }
  block_t *value = current->next->blk;
  node_free(current->next);
  current->next = NULL;
  return value;
}

/* Returns the first block without removing it, or NULL if the list is empty. */
block_t* list_get_from_front(list_t *l) {
  if (l->head == NULL){
    return NULL;
  }
  return l->head->blk;
}

/* Removes the first node and returns its block, or NULL if the list is empty. */
block_t* list_remove_from_front(list_t *l) {
  if (l->head == NULL){
    return NULL;
  }
  node_t *current = l->head;
  block_t *value = current->blk;
  l->head = current->next;
  node_free(current);
  return value;
}

/* Removes the node at position index and returns its block, or NULL if the
 * index is out of range. */
block_t* list_remove_at_index(list_t *l, int index) {
  if (l->head == NULL || index < 0){
    return NULL;
  }
  if (index == 0){
    return list_remove_from_front(l);
  }

  // Stop on the node just before the one to remove.
  node_t *prev = l->head;
  int i = 0;
  while (prev->next != NULL && i < index - 1) {
    prev = prev->next;
    i++;
  }

  // index is past the end of the list.
  if (prev->next == NULL) {
    return NULL;
  }

  node_t *current = prev->next;
  block_t *value = current->blk;
  prev->next = current->next;
  node_free(current);
  return value;
}

/* Returns true if blocks a and b have the same pid, start, and end. */
bool compareBlks(block_t* a, block_t *b) {
  if (a->pid == b->pid && a->start == b->start && a->end == b->end)
     return true;
  return false;
}

/* Returns true if block b can hold a request of size a (b is a or larger). */
bool compareSize(int a, block_t *b) {
  if (a <= blocksize(b))
     return true;
  return false;
}

/* Returns true if block b belongs to process a. */
bool comparePid(int a, block_t *b) {
  if (a == b->pid)
     return true;
  return false;
}

/* Returns true if a block equal to value is in the list. */
bool list_is_in(list_t *l, block_t* value) {
  node_t *current = l->head;
  while (current != NULL){
    if (compareBlks(value, current->blk)){
      return true;
    }
    current = current->next;
  }
  return false;
}

/* Returns the block at position index, or NULL if out of range. */
block_t* list_get_elem_at(list_t *l, int index) {
  node_t *current = l->head;
  int i = 0;
  while (current != NULL){
    if (i == index)
      return current->blk;
    current = current->next;
    i++;
  }
  return NULL;
}

/* Returns the position of a block equal to value, or -1 if not found. */
int list_get_index_of(list_t *l, block_t* value){
  node_t *current = l->head;
  int i = 0;
  while (current != NULL){
    if (compareBlks(value, current->blk)){
      return i;
    }
    current = current->next;
    i++;
  }
  return -1;
}

/* Checks to see if block of Size or greater exists in the list. */
bool list_is_in_by_size(list_t *l, int Size){
  node_t *current = l->head;
  while (current != NULL){
    if (compareSize(Size, current->blk)){
      return true;
    }
    current = current->next;
  }
  return false;
}

/* Checks to see if pid of block exists in the list. */
bool list_is_in_by_pid(list_t *l, int pid){
  node_t *current = l->head;
  while (current != NULL) {
    if (comparePid(pid, current->blk)) {
      return true;
    }
    current = current->next;
  }
  return false;
}

/* Returns the index of the first block of Size or greater, or -1. Because
 * this always scans from the head, the free list's order decides the policy:
 * insertion order gives First Fit, smallest-first gives Best Fit, and
 * largest-first gives Worst Fit. */
int list_get_index_of_by_Size(list_t *l, int Size){
  node_t *current = l->head;
  int i = 0;
  while (current != NULL){
    if (compareSize(Size, current->blk)){
      return i;
    }
    current = current->next;
    i++;
  }
  return -1;
}

/* Returns the index of the block owned by pid, or -1 if not found. */
int list_get_index_of_by_Pid(list_t *l, int pid){
  node_t *current = l->head;
  int i = 0;
  while (current != NULL){
    if (comparePid(pid, current->blk)){
      return i;
    }
    current = current->next;
    i++;
  }
  return -1;
}