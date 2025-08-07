#include "memoryPool.h"
#include "operations.h"

int totalStudents = 0;
void addDetails(memory_pool_t *recordPool){
    char name[20];
    int age = 0;
    printf("Enter Name: ");
    scanf(" %[^\n]", name);

    printf("Enter Age: ");
    scanf("%d", &age);

    student_t *record = (student_t *)pool_alloc(recordPool);
    if(record){
        totalStudents++;
        strncpy(record->name, name, sizeof(record->name) - 1);
        record->name[sizeof(record->name) - 1] = '\0';
        record->age = age;
        printf("Details added successfully!\n");
    }
    else
        printf("Error: memory allocation failed!\n");
}

void displayDetails(memory_pool_t *recordPool){
    student_t *s = (student_t *)recordPool->memory;
    for(int i = 0; i < totalStudents; i++){
        student_t *s = (student_t *)(recordPool->memory + i * recordPool->block_size);
        printf("NAME: %s    AGE: %d\n", s->name, s->age);
    }
}