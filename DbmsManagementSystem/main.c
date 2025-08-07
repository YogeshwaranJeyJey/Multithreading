#include "memoryPool.h"
#include "operations.h"

int main()
{
    bool loopFlag = true, alreadyExited = false;
    int userChoice = 0, contChoice = 0;
    memory_pool_t *recordPool = pool_create(1024 * 1024, 256);
    do
    {
        printf("----------Welcome to DBMS!----------\n");
        printf("1.Add Details\n");
        printf("2.Display Details\n");
        printf("3.Exit\n");
        printf("Enter your choice: ");
        scanf("%d", &userChoice);

        switch (userChoice)
        {
        case 1:
            addDetails(recordPool);
            break;
        case 2:
            displayDetails(recordPool);
            break;
        case 3:
            loopFlag = false;
            alreadyExited = true;
            pool_destroy(recordPool);
            printf("Exiting\n");
            break;
        default:
            printf("Enter a valid option!\n");
        }
        if (!alreadyExited)
        {
            printf("Press '1' to continue or '0' to exit!\n");
            printf("Enter your choice: ");
            scanf("%d", &contChoice);
            if (contChoice == 0)
            {
                loopFlag = false;
                pool_destroy(recordPool);
                printf("Exiting from DB!");
            }
        }
    } while (loopFlag);
    return 0;
}