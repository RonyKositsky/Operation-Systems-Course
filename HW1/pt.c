#include "os.h"

//205817893 רוני קוסיצקי

/*
 * Checks the valid bit, the LSB.
 */
int is_pte_valid(uint64_t pte) {
    return pte & 0x1;
}

/*
 * Setting the valid bit to false.
 */
uint64_t page_disable(uint64_t pte){
    return pte = pte & ~((uint64_t)1); //Setting LSB to 0.
}

/*
 * Getting vpn and returning array with the 5 relevant addresses.
 */
uint64_t get_index_by_level(uint64_t vpn, int level){
    //Each level contains 9 bit of representation. For each level we want to offset for the desired data.
    int offset = 45  - (level+1) * 9;

    vpn = vpn >> offset;
    vpn = vpn & 0x1FF; // vpn & 9 1s to take only first 9 bits.

    return vpn;
}

/*
 * Getting vpn and pointer to table entry and performing page walk.
 * Returning the last frame in the walk(fifth frame).
 * Optional destroy - optional parameter - if set to true destroy the last PTE if found.
 */
uint64_t page_walk(uint64_t* pt_pointer,uint64_t vpn, int optional_destroy){
    uint64_t* pointer = pt_pointer;
    int address;

    for (int i = 0; i < 4; i++) {
        address = get_index_by_level(vpn, i);

        if(pointer == 0 || !is_pte_valid(pointer[address])){
            return 0;
        }

        pointer = phys_to_virt(pointer[address]);
    }


    address = get_index_by_level(vpn, 4);
    if(!is_pte_valid(pointer[address])) { //Checking if valid pte.
        return 0;
    }

    //Destroying page.
    if(optional_destroy){
        pointer[address] = page_disable(pointer[address]);
    }

    return (pointer[address] >> 12); //12 LSB are for the page entry.
}

/*
 * Creating trie tree for physical ppn.
 */
void create_table_entry(uint64_t* pt_pointer, uint64_t vpn, uint64_t ppn){
    uint64_t* pointer = pt_pointer;
    int address;

    for (int i = 0; i < 4; i++) {
        address = get_index_by_level(vpn, i);

        if(pointer[address] == 0) {
            pointer[address] = (alloc_page_frame() << 12) + 0x1;
        }

        pointer = phys_to_virt(pointer[address]);
    }

    address = get_index_by_level(vpn, 4);
    pointer[address] = (ppn << 12) + 1;
}

void page_table_update(uint64_t pt, uint64_t vpn, uint64_t ppn){
    uint64_t* pt_pointer = phys_to_virt(pt); //No need for the offset.
    if(ppn == NO_MAPPING){
        //Destroy vpn in table if exists
        page_walk(pt_pointer,vpn,1);
    }
    else{
        //Map vpn -> ppn
        create_table_entry(pt_pointer,vpn,ppn);
    }
}

uint64_t page_table_query(uint64_t pt, uint64_t vpn){
    uint64_t *pt_pointer = phys_to_virt(pt);

    uint64_t ppn = page_walk(pt_pointer,vpn,0);
    if(ppn == 0){
        return NO_MAPPING; //Could not find the ppn.
    }else{
        return ppn;
    }
}

