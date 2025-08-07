#ifndef OPERATIONS_H
#define OPERATIONS_H

#include "memoryPool.h"

extern int totalStudents;

typedef struct student{
    char name[20];
    int age;
}student_t;

void addDetails(memory_pool_t *recordPool);
void displayDetails(memory_pool_t *recordPool);

#endif