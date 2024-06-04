// CompressAlgorithm.cpp : This file contains the 'main' function. Program execution begins and ends there.
//


#include <stdint.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h>
#include "cd.h"

void dump_frame_hex(void* buf, int size)
{
    uint8_t* b = buf;
    for (int i = 0; i < size; i++)
    {
        if (i % 16 == 0)
        {
            printf("\n");
        
        }
        printf("%02X ", *b++);
        
    }
    printf("\n");
}

long getFileSize(const char* filename) {
    FILE* file = fopen(filename, "rb");
    if (file == NULL) {
        printf("Unable to open file");
        return -1;
    }

    // Przejœcie na koniec pliku
    if (fseek(file, 0, SEEK_END) != 0) {
        printf("fseek failed");
        fclose(file);
        return -1;
    }

    // Pobranie pozycji wskaŸnika, która odpowiada rozmiarowi pliku
    long fileSize = ftell(file);
    if (fileSize == -1) {
        printf("ftell failed");
    }

    // Zamkniêcie pliku
    fclose(file);

    return fileSize;
}

int main()
{
    int8_t ret;
    cd_rect_t rect;
    uint32_t  size_buf_encoded = (CD_SCREEN_SIZE_WIDTH * CD_SCREEN_SIZE_HEIGHT * CD_PIXEL_COLOR_DEPTH) / 8 + 10;
    uint32_t size_buf_screen = (CD_SCREEN_SIZE_WIDTH * CD_SCREEN_SIZE_HEIGHT * CD_PIXEL_COLOR_DEPTH) / 8;

    void* buf_encoded = calloc(size_buf_encoded, 1);

    cd_conv_xywh2rect(0,0, CD_SCREEN_SIZE_WIDTH, CD_SCREEN_SIZE_HEIGHT, &rect);

    char filename[] = "D:\\Desktop\\Helena\\Repositories\\Compress Algorithm\\examples\\photo";
    char filename2[] = "D:\\Desktop\\Helena\\Repositories\\Compress Algorithm\\examples\\data_encoded";
    char filename3[] = "D:\\Desktop\\Helena\\Repositories\\Compress Algorithm\\examples\\data_decoded";

    
    long size = getFileSize(filename);
    printf("Rozmiar pliku %d \n", size);
    void* buf_screen = calloc(size, 1);

    int fd = open(filename, O_RDWR);
    if (fd == -1) {
        printf("Error opening file");
        return EXIT_FAILURE;
    }
    read(fd, buf_screen, size);
    close(fd);
// -------------------------------------------- KODOWANIE --------------------------------------------
    ret = cd_encode(
        buf_screen,                   //wskaznik na caly obraz
        &rect,                  //prostokat ktory wskazuje jakie dane zakodowac
        buf_encoded,            //wsk bufora na dane zakodowane
        &size_buf_encoded       //rozmiar pamieci wskazywany przez wskaznik buf_encoded i jednoczesnie pozakonczeniu dzialania funkcji jest to rozmiar zakodowanych danych
    );

    if (ret == 0)
    {
        printf("Zakodowane dane: ");
        dump_frame_hex(buf_encoded, /*size_buf_encoded*/256);
    }

    fd = open(filename2, O_WRONLY | O_CREAT);
    if (fd == -1) {
        printf("Error opening file");
        //return EXIT_FAILURE;
    }
    else
    {
        write(fd, buf_encoded, size_buf_encoded);
        close(fd);
    }

    memset(buf_screen, 0, size_buf_screen);
// ------------------------------------------- DEKODOWANIE -------------------------------------------
    ret = cd_decode(
        buf_encoded,            //wsk bufora na dane zakodowane
        size_buf_encoded,       //rozmiar pamieci wskazywany przez wskaznik buf_encoded i jednoczesnie pozakonczeniu dzialania funkcji jest to rozmiar zakodowanych danych
        buf_screen,
        size_buf_screen
    );

    fd = open(filename3, O_WRONLY | O_CREAT);
    if (fd == -1) {
        printf("Error opening file");
        //return EXIT_FAILURE;
    }
    else
    {
        write(fd, buf_screen, size_buf_screen);
        close(fd);
    }

    printf("code returned %d\n", (int)ret);
}