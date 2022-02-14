//
// Created by Amit Cohen on 04/06/2021.
//

#ifndef EX6_SIM_MEM_H
#define EX6_SIM_MEM_H
#define MEMORY_SIZE 50
#define Write 1
#define Read 0
extern char main_memory[MEMORY_SIZE];

typedef struct page_descriptor {
    int V; // valid
    int D; // dirty
    int P; // permission
    int frame; //the number of a frame if in case it is page-mapped
    int swap_index; // where the page is located in the swap file.
}
        page_descriptor;

class sim_mem {
    int swapfile_fd; //swap file fd
    int program_fd; //executable file fd
    int text_size;
    int data_size;
    int bss_size;
    int heap_stack_size;
    int num_of_pages;
    int page_size;
    page_descriptor *page_table; //pointer to page table
    int *frameManagerLRU;
    bool *freeFrame;
    bool *freeInSwap;
    int clock;
    int swap_size;
public:
    sim_mem(char exe_file_name[], char swap_file_name[], int text_size, int data_size, int bss_size,
            int heap_stack_size, int num_of_pages, int page_size);

    ~sim_mem();

    char load(int address);

    void store(int address, char value);

    void print_memory();

    void print_swap();

    void print_page_table();

    char getCharFromMM(int page, int offset);

    bool pageInMM(int page);

    void loadThePageFromLM(int page, char *);

    void loadPageIntoMM(const char *, int page);

    int LRU_Management();

    int findMin(const int *) const;

    void setCharIntoMM(int page, int offset, char value);

    int whatPageIsThis(int address);

    int findFree(const bool *a, int size);

    void loadThePageFromSwap(int page, char *a);

    void loadPageIntoSwap(int page);

    void allocationMemory(int page);

    void destroy();

    bool checkTheAddress(int address) const;
};
#endif //EX6_SIM_MEM_H