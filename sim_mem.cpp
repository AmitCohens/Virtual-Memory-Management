//
// Created by Amit Cohen on 04/06/2021.
//
#include "sim_mem.h"

#include <unistd.h>

#include <fcntl.h>

#include <malloc.h>

#include <iostream>

#include <cstring>

sim_mem::sim_mem(char exe_file_name[], char swap_file_name[], int text_size, int data_size, int bss_size, int heap_stack_size, int num_of_pages, int page_size) {
    this -> program_fd = open(exe_file_name, O_RDONLY, S_IRUSR);
    if (this -> program_fd == -1) {
        perror("The open failed");
        destroy();
        return;
    }
    this -> swapfile_fd = open(swap_file_name, O_RDWR | O_CREAT, S_IRWXU);
    if (this -> swapfile_fd == -1) {
        perror("The open failed");
        destroy();
        return;
    }
    this -> text_size = text_size;
    this -> num_of_pages = num_of_pages;
    this -> page_size = page_size;
    this -> bss_size = bss_size;
    this -> data_size = data_size;
    this -> heap_stack_size = heap_stack_size;
    this -> page_table = new page_descriptor[this -> num_of_pages];
    this -> frameManagerLRU = new int[num_of_pages];
    this -> freeFrame = new bool[MEMORY_SIZE / page_size];
    this -> swap_size = page_size * (num_of_pages - (text_size / page_size));
    this -> freeInSwap = new bool[swap_size / page_size];
    this -> clock = 1;
    for (int i = 0; i < this -> num_of_pages; i++) {

        this -> page_table[i].V = 0;
        this -> page_table[i].frame = -1;
        this -> page_table[i].swap_index = -1;
        this -> page_table[i].D = 0;
        this -> frameManagerLRU[i] = 0;
        if (i < (this -> text_size / this -> page_size)) {
            this -> page_table[i].P = Read;
        } else
            this -> page_table[i].P = Write;
    }
    for (int i = 0; i < MEMORY_SIZE; i++) {
        if (i < (MEMORY_SIZE / page_size))
            this ->freeFrame[i] = false;
        main_memory[i] = '0';
    }
    int check;
    lseek(this -> swapfile_fd, 0, SEEK_SET);
    for (int index = 0; index < this ->swap_size; index++) {
        check = write(this ->swapfile_fd, "0", 1);
        if (index < this ->swap_size / page_size)
            this ->freeInSwap[index] = false;
        if (check == -1)
            perror("Writing failed");
    }
}
sim_mem::~sim_mem() {
    destroy();
}
char sim_mem::load(int address) {//Extracting an address from memory
    if(checkTheAddress(address)){
        fprintf(stderr,"The requested address does not exist.");
        return '0';
    }
    int pageAndOffset[2];
    pageAndOffset[0] = address / this ->page_size; //page
    pageAndOffset[1] = address % this ->page_size; //offset
    char buff[this ->page_size];
    int var = whatPageIsThis(address);
    this ->frameManagerLRU[pageAndOffset[0]] = this ->clock++;
    if (pageInMM(pageAndOffset[0]))//Check, if the page is already in the main memory, go to it and return the character from it
        return getCharFromMM(pageAndOffset[0], pageAndOffset[1]);
    if (this ->page_table[pageAndOffset[0]].P == Read) {//Check if the page is read-only, then it is safe in the EXE file and we will read it from there
        loadThePageFromLM(pageAndOffset[0], buff);
        loadPageIntoMM(buff, pageAndOffset[0]);
        return getCharFromMM(pageAndOffset[0], pageAndOffset[1]);
    } else {
        if (this ->page_table[pageAndOffset[0]].D == 1) {//Check if the page is "dirty", then go to take it from SWAP
            loadThePageFromSwap(pageAndOffset[0], buff);
            loadPageIntoMM(buff, pageAndOffset[0]);
            return getCharFromMM(pageAndOffset[0], pageAndOffset[1]);
        } else {
            if (var == 1) {//If it is a DATA file, go and get it from the EXE
                loadThePageFromLM(pageAndOffset[0], buff);
                loadPageIntoMM(buff, pageAndOffset[0]);
                return getCharFromMM(pageAndOffset[0], pageAndOffset[1]);
            } else if (var > 1) {
                if (var == 3)
                    perror("Reading from memory before it was malloc");
                else {
                    allocationMemory(pageAndOffset[0]);
                    return getCharFromMM(pageAndOffset[0], pageAndOffset[1]);
                }
            }
        }
    }
    return '\0';
}
void sim_mem::store(int address, char value) {//Loading information into memory
    if(checkTheAddress(address)){
        fprintf(stderr,"The requested address does not exist.");
        return;
    }
    int pageAndOffset[2];
    pageAndOffset[0] = address / this ->page_size; //page
    pageAndOffset[1] = address % this ->page_size; //offset
    int  var = whatPageIsThis(address);
    char buff[this ->page_size];
    this ->frameManagerLRU[pageAndOffset[0]] = this ->clock++;
    if (this ->page_table[pageAndOffset[0]].P == Read) {
        perror("No write permissions");
        return;
    }
    if (pageInMM(pageAndOffset[0]))//Check, if the page is already in the main memory
        setCharIntoMM(pageAndOffset[0], pageAndOffset[1], value);
    else if (this ->page_table[pageAndOffset[0]].D == 1) {//Check if the page is "dirty", then go to take it from SWAP
        loadThePageFromSwap(pageAndOffset[0], buff);
        loadPageIntoMM(buff, pageAndOffset[0]);
        setCharIntoMM(pageAndOffset[0], pageAndOffset[1], value);
    } else {
        if (var == 1) {//If it is a DATA file, go and get it from the EXE
            loadThePageFromLM(pageAndOffset[0], buff);
            loadPageIntoMM(buff, pageAndOffset[0]);
            setCharIntoMM(pageAndOffset[0], pageAndOffset[1], value);
        } else if (var > 1) {
            allocationMemory(pageAndOffset[0]);
            setCharIntoMM(pageAndOffset[0], pageAndOffset[1], value);
        }
    }
}
void sim_mem::print_memory() {
    int i;
    printf("\n Physical memory\n");
    for (i = 0; i < MEMORY_SIZE; i++) {
        printf("[%c]\n", main_memory[i]);
    }
}
void sim_mem::print_swap() {
    char * str = (char * )(malloc(this -> page_size * sizeof(char)));
    int i;
    printf("\n Swap memory\n");
    lseek(swapfile_fd, 0, SEEK_SET); // go to the start of the file
    while (read(swapfile_fd, str, this -> page_size) == this -> page_size) {
        for (i = 0; i < page_size; i++) {
            printf("%d - [%c]\t", i, str[i]);
        }
        printf("\n");
    }
    free(str);
}
void sim_mem::print_page_table() {
    int i;
    printf("\n page table \n");
    printf("Valid\t Dirty\t Permission \t Frame\t Swap index\n");
    for (i = 0; i < num_of_pages; i++) {
        printf("[%d]\t[%d]\t[%d]\t\t[%d]\t\t[%d]\n", page_table[i].V, page_table[i].D, page_table[i].P, page_table[i].frame, page_table[i].swap_index);
    }
}
char sim_mem::getCharFromMM(int page, int offset) {//This function returns the required character from the main memory
    int frame;
    frame = this ->page_table[page].frame;
    return main_memory[(frame * this ->page_size) + offset];
}
bool sim_mem::pageInMM(int page) {//This function checks if the required page is already in the main memory
    if (this ->page_table[page].V == 1)
        return true;
    return false;
}
void sim_mem::loadThePageFromLM(int page, char * a) {//This function loads into the buff the relevant page from the logical memory
    char var [this ->page_size];
    int check ;
    lseek(this -> program_fd, (page * this -> page_size), SEEK_SET);
    check = read(this ->program_fd,var, this ->page_size);
    var [this ->page_size] = '\0';
    if (check == -1) {
        perror("The reading failed");
        destroy();
        return;
    }
    strcpy(a,
           var);
    a[this ->page_size] = '\0';
}
void sim_mem::loadPageIntoMM(const char * a, int page) {//This function loads the page inside the buff into the main memory
    int i = 0, index = findFree(this ->freeFrame, MEMORY_SIZE / this ->page_size);
    if (index == -1) {
        index = LRU_Management();
        if (this ->page_table[index].D == 1)
            loadPageIntoSwap(index);
    }
    for (; this ->freeFrame[i] && i < (MEMORY_SIZE / this ->page_size); i++);
    for (int j = 0, ix = (i * this ->page_size); j < this ->page_size; j++, ix++) {
        main_memory[ix] = a[j];
    }
    this ->freeFrame[i] = true;
    this ->page_table[page].V = 1; //
    this ->page_table[page].frame = i;
}
int sim_mem::LRU_Management() {//This function implements the LRU principle, searches for the page that appears the longest and overrides it / swaps it
    int index = findMin(this->frameManagerLRU);
    this ->frameManagerLRU[index] = 0;
    this ->freeFrame[page_table[index].frame] = false;
    this ->page_table[index].V = 0;
    return index;
}
int sim_mem::findMin(const int * a) const {//This function checks the LRU for the lowest clock water (that is, it has been used for the longest time).
    int minInd = 0,
            var = this ->clock;
    for (int i = 0; i < this ->num_of_pages; i++) {
        if (a[i] != 0 && a[i] <var) {
            var = a[i];
            minInd = i;
        }
    }
    return minInd;
}
void sim_mem::setCharIntoMM(int page, int offset, char value) {//This function inserts the character into the required space in the main memory
    int frame;
    frame = this ->page_table[page].frame;
    main_memory[(frame * this ->page_size) + offset] = value;
    this -> page_table[page].D = 1;
}
int sim_mem::whatPageIsThis(int address) { //return: 0=text,1=data,2=bss,3stack_heap
    int page = address / this ->page_size, size = ((this ->text_size) / this ->page_size);
    if (page < size)
        return 0;
    size = ((this ->text_size + this ->data_size) / this ->page_size);
    if (page < size)
        return 1;
    size = ((this ->text_size + this ->data_size + this ->bss_size) / this ->page_size);
    if (page < size)
        return 2;
    size = ((this ->text_size + this ->data_size + this ->bss_size + this ->heap_stack_size) / this ->page_size);
    if (page < size)
        return 3;
    else return -1;
}
int sim_mem::findFree(const bool * a, int size) {//This function runs on the Boolean array and checks where there is a false and returns its index.
    for (int i = 0; i < size; i++) {
        if (!a[i])
            return (i);
    }
    return -1;
}
void sim_mem::loadThePageFromSwap(int page, char * a) {//This function loads the relevant page from the swap file into the main memory
    char var [this ->page_size], check;
    lseek(this -> swapfile_fd, (this -> page_table[page].swap_index * this -> page_size), SEEK_SET);
    check = read(this ->swapfile_fd,var, this ->page_size);
    if (check == -1) {
        perror("The reading failed");
        destroy();
        return;
    }

    lseek(this -> swapfile_fd, (this -> page_table[page].swap_index * this -> page_size), SEEK_SET);
    for (int j = 0; j < this ->page_size; j++) {
        check = write(this -> swapfile_fd, "0", 1);
        if (check != 1) {
            perror("The Writing failed");
            destroy();
            return;
        }
    }
    this -> freeInSwap[this -> page_table[page].swap_index] = false;
    this -> page_table[page].swap_index = -1;
    this -> page_table[page].D = 0;
    strcpy(a,
           var);
}
void sim_mem::loadPageIntoSwap(int page) {//This function loads a page that you want to clear from the main memory on the swap file
    char buff[this->page_size];
    int index = findFree(this->freeInSwap, this->swap_size), check;
    for (int i = 0; i < this->page_size; i++) {
        buff[i] = main_memory[(this->page_table[page].frame * page_size) + i];
    }
    lseek(this->swapfile_fd, index * this->page_size, SEEK_SET);
    check = write(this->swapfile_fd, buff, this->page_size);
    if (check == -1) {
        perror("The Writing failed");
        destroy();
    }
    this -> freeInSwap[index] = true;
    this -> page_table[page].swap_index = index;
    this -> page_table[page].frame = -1;
}
void sim_mem::allocationMemory(int page) {//This function simulates memory allocation, actually creating a new page that is all zeros within the main memory
    char buff[this->page_size];
    for (int j = 0; j < this->page_size; j++)
        buff[j] = '0';
    loadPageIntoMM(buff, page);
}
void sim_mem::destroy() {//This function shuts down all resources when there is an error reading, writing or opening a file
    delete[] this -> page_table;
    close(this -> program_fd);
    close((this -> swapfile_fd));
}
bool sim_mem::checkTheAddress(int address) const{
    if(address>(this->num_of_pages*this->page_size))
        return true;
    else
        return false;
}