#define FUSE_USE_VERSION 26
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <fuse.h>
#include <sys/mman.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>

typedef unsigned long fs_addr;
#include <sys/types.h>
#define FILENAMEMAX 256
#define BLOCKSIZE 4096
#define BLOCKNR 1024*1024
#define BLOCKLENGTH (BLOCKSIZE-sizeof(fs_addr))
//BLOCKLENGTH is the true length of a block
#define FORMAL_DATA_NUMBER (BLOCKNR/BLOCKSIZE/8)
#define allused 0xffffffff
//per block has 4096 bytes each byte has 8 bits
//4GB memory
//per block's size is 4096 bytes

struct filenode {
    char filename [FILENAMEMAX];
    fs_addr firstcontentnode;
    fs_addr position;// marks the position in the array
    //at first I'd like to use struct contentnode * as the pointer to point to the son table However, as it is difficult to express it on mem so I changed it to fs_addr.
    struct stat st;
    fs_addr next;
    //at first I also use the pointer to
};


static fs_addr find_the_last_contentnode(struct filenode * firstfile);
static struct filenode *get_filenode(const char *name);
void * create_new_block();
unsigned int move(unsigned int choice,fs_addr blockposition);
void markbit (fs_addr blockposition);
void demarkbit(fs_addr blockposition);
fs_addr lookupfreeblock();
void init_prologue_block(fs_addr a,fs_addr b);
static void* oshfs_init(struct fuse_conn_info *conn);
static int oshfs_getattr(const char *path, struct stat *stbuf);
void blockfree(fs_addr address);
static int oshfs_mknod(const char *path, mode_t mode, dev_t dev);
static int oshfs_truncate(const char* path, off_t size);
static int oshfs_unlink(const char *path);
static int oshfs_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi);
static int oshfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi);
static int oshfs_open(const char *path, struct fuse_file_info *fi);
static int oshfs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi);

fs_addr * ip;//ip points to the first block of the memory and ip[0] is the number of all the blocks
//ip[1] is the number of the used blocks
//ip[2] is the place to store where the root is
struct contentnode {
    fs_addr next;
    char data [BLOCKSIZE-sizeof(fs_addr)];
};
//content node作为内容指针,也就是块指针


//这里采用广义表的结构来写


static void *mem [BLOCKNR];

#define root ip[2]
static struct filenode *get_filenode(const char *name)
{
    fprintf(stderr,"%ld\n",root);


    struct filenode *node = (struct filenode *)mem[root];
    while(node) {
        //fprintf(stderr,"this is the next node %ld",node->next);
        if(strcmp(node->filename, name + 1) != 0)
            node = (struct filenode *)mem[node->next];
        else  {
            fprintf(stderr,"found the node\n");
            fprintf(stderr,"%p\n",node);
            return node;
        }
    }
    return NULL;
}

void * create_new_block()
{
    void * newblock;
    newblock = mmap(NULL,BLOCKSIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    memset(newblock,0,BLOCKSIZE);
    fprintf(stderr,"%p\n",newblock);

    return newblock;
}

unsigned int move(unsigned int choice,fs_addr blockposition)
{
    fprintf(stderr, "here comes the move part");

    unsigned int result;
    // This function is used to move 1 and the choice is determined by whether to markbit or demarkbit
    result = ((unsigned int)1)<<((sizeof(unsigned int) * 8-1)-blockposition % (8*BLOCKSIZE) %(sizeof(unsigned int) * 8));
    switch (choice){
        case 1:return result;
        default: return ~result;

    }
}

void markbit (fs_addr blockposition)
{
    fprintf(stderr,"markbit");

    unsigned int *pointer;
    fs_addr inside_blockposition;
    int markbit =1+ blockposition/BLOCKSIZE/8;
    unsigned middle;


    pointer = (unsigned int *)mem[markbit];



    if (blockposition < BLOCKNR){
        inside_blockposition = blockposition % (8 * BLOCKSIZE) / (sizeof(unsigned int) * 8);


        middle = move(1,blockposition);


        pointer [inside_blockposition] |= middle;



        ip[1] ++;



        // here divide the block into unsigned array , and each unit's bit is operated like this
    }
}
//demarkbit is very similar to markbit just need to change | to &
void demarkbit(fs_addr blockposition)
{


    fprintf(stderr, "here comes the demarkbit part");


    unsigned int *pointer;
    fs_addr inside_blockposition;
    int markbit =1+ blockposition/8/BLOCKSIZE;
    pointer = (unsigned int *)mem[markbit];
    if (blockposition < BLOCKNR){
        inside_blockposition = blockposition % (8 * BLOCKSIZE) / (sizeof(unsigned int) * 8);
        pointer [inside_blockposition] &= move(0,blockposition);
        ip[1] --;
        // here divide the block into unsigned array , and each unit's bit is operated like this
    }
}


fs_addr lookupfreeblock()//This function is used to look for the free block
{

    fprintf(stderr, "here comes the lookupfreeblock part");

    //we use the markbit here to judge if the block is free or not
    fs_addr i;
    int j;
    int position2=0;
    int position=0;
    int foundornot=0;
    int number;
    //judge if has found the free block
    unsigned int *pointer;
    for (i=1;i<=FORMAL_DATA_NUMBER;i++)
    {
        pointer = (unsigned int *) mem[i];
        //as each block has some small ints so use j to try every int
        for (j=0;j<=(BLOCKSIZE/(sizeof(unsigned int)*8));j++)
            if (*(pointer+j)!=(allused)){
                foundornot = 1;
                break;
            }
        if (foundornot) break;
    }
    if (foundornot) {
        number = (int) *(pointer+j);
        while (number<0) {
            position2++;
            number=number<<1;
        }
        return (8*sizeof(unsigned int)*j+position2+BLOCKSIZE*i);
    }
    if (!foundornot) return 0;

}

static int create_filenode(const char *filename, const struct stat *st)
{
    struct filenode * node;
    fs_addr address;
    address = lookupfreeblock();
    if (address) {
        node = (struct filenode *) mem[address];
        mem[address] = create_new_block();
        markbit(address);
        strncpy(node->filename,filename,strlen(filename));
        node->st = *st;
        node->firstcontentnode = 0;
        node->next = root;
        root = address;
        node->position = address;
        return 0;
    }
    else return 1;

}

void init_prologue_block(fs_addr a,fs_addr b)
{
    fs_addr i;
    for (i=a;i<=b;i++)
        mem[i] = create_new_block();
}

static void* oshfs_init(struct fuse_conn_info *conn)
{
    fprintf(stderr, "here comes the init part");


    //initialize the oshfs and set the first few blocks as the prologue_block
    int i;
    mem[0]=create_new_block();
    init_prologue_block(1,FORMAL_DATA_NUMBER);
    ip = (fs_addr *) mem[0];
    fprintf(stderr,"1");

    markbit(0);

    fprintf(stderr,"2");




    fprintf(stderr,"3");


    for (i=1; i<=FORMAL_DATA_NUMBER;i++)
        markbit(i);

    ip[1] = FORMAL_DATA_NUMBER+1;
    ip[0] = BLOCKNR;
    ip[2] = 0;
    return 0;
}


static int oshfs_getattr(const char *path, struct stat *stbuf)
{
    fprintf(stderr, "here comes the getattr part\n");


    int ret = 0;
    struct filenode *node = get_filenode(path);
    if(strcmp(path, "/") == 0) {

        fprintf(stderr, "here comes the getattr part1\n");
        memset(stbuf, 0, sizeof(struct stat));
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
    }
    else if(node) {
        fprintf(stderr, "here comes the getattr part2\n");
        memcpy(stbuf, &(node->st), sizeof(struct stat));
    }
    else {
        fprintf(stderr, "here comes the getattr part3\n");
        ret = -ENOENT;
    }
    return ret;
}

void blockfree(fs_addr address)
{
    demarkbit(address);
    munmap(mem[address],BLOCKSIZE);
}


static int oshfs_mknod(const char *path, mode_t mode, dev_t dev)
{

    fprintf(stderr, "here comes the mknod part");


    int count;
    int check;
    struct stat st;
    st.st_mode = S_IFREG | 0644;
    st.st_uid = fuse_get_context()->uid;
    st.st_gid = fuse_get_context()->gid;
    st.st_nlink = 1;
    st.st_size = 0;
    check = create_filenode(path + 1, &st);
    if (!check) return 0;
    else return -errno;
}

void freealltheblocks(struct contentnode *block)
{

    fprintf(stderr,"here comes the freealltheblocks part");


    fs_addr address;
    while (block-> next != 0)
    {
        address = block -> next;
        block -> next = ((struct contentnode *) mem[address])->next;
        blockfree(address);
    }

}
static int oshfs_truncate(const char* path, off_t size)
{
    fprintf(stderr, "here comes the truncate part");



    off_t number=0;
    fs_addr addr_a;
    fs_addr addr_b;
    fs_addr number1,number2;
    fs_addr addr;
    struct contentnode * nodea;
    struct filenode * node = get_filenode(path);
    number1 = size/BLOCKLENGTH;
    number2 = node->st.st_size/BLOCKLENGTH;
    if (node == NULL) return -ENOENT;
    if (node -> firstcontentnode ==0)
    {
        if (size ==0)
            return 0;
        else
        {
            addr_b = lookupfreeblock();
            mem[addr_b] = create_new_block();
            markbit(addr_b);
            node -> firstcontentnode = addr_b;
        }
    }
    addr_a = node -> firstcontentnode;
    //if number1 >= number2 means we need more blocks so let addr_a points to the last block
    if (number1 >= number2)
    {
        //this can save a lot of times
        number = ((node->st.st_size+BLOCKLENGTH-1)/BLOCKLENGTH) *BLOCKLENGTH;
        addr_a = node->firstcontentnode;
        while (((struct contentnode *)mem[addr_a])->next!=0)
        {
            addr_a = ((struct contentnode *)mem[addr_a])->next;
        }
    }

    for (;number<size;number+=BLOCKLENGTH)
    {
        if (size-number<BLOCKLENGTH) break;
        nodea = (struct contentnode *) mem[addr_a];
        if (nodea -> next == 0)
        {
            addr_b = lookupfreeblock();
            mem[addr_b] = create_new_block();
            markbit(addr_b);
            addr_a = addr_b;
        }
        else
            addr_a = nodea ->next;
    }
    nodea = (struct contentnode*) mem[addr_a];
    freealltheblocks(nodea);
    //free all the blocks

    node ->st.st_size = size;
    return 0;
}

static int oshfs_unlink(const char *path)
{

    fprintf(stderr, "here comes the unlink part");


    int mark=0;
    struct filenode * node = (struct filenode *)mem[root];
    struct contentnode * block;
    fs_addr a;
    fs_addr b;
    fs_addr position;
    // three situations
    // root doesn't exist
    if (root == 0)
        return -ENOENT;
    // the root node is the node which should be deleted
    if (strcmp(node -> filename,path+1) ==0)
    {
        root = node -> next;
        a = node -> firstcontentnode;
        blockfree(node->position);
        mark = 1;
    }
    // here finds the first headnode which meets the need
    else {
        while (node)
        {
            if (strcmp(node->filename,path +1) !=0)
                node = (struct filenode *)mem[node -> next];
            else {
                position = node -> position;
                a = node ->firstcontentnode;
                node = (struct filenode *) mem[node -> next];
                blockfree(position);
                mark = 1;
                break;
            }
        }
    }
    if (!mark) return -ENOENT;
    freealltheblocks((struct contentnode *) mem[a]);
    return 0;
}

static fs_addr createcontentblock()
{
    fprintf(stderr,"here comes the createcontentblock part");
    fs_addr newcontentnode;
    newcontentnode = lookupfreeblock();
    mem[newcontentnode] = create_new_block();
    markbit(newcontentnode);
    return(newcontentnode);
}

static fs_addr find_the_last_contentnode(struct filenode * firstfile)
{
    fprintf(stderr,"here comes the find_the_last_contentnode part");
    fs_addr i;
    struct contentnode * ctnode;
    ctnode = (struct contentnode *)mem[firstfile -> firstcontentnode];
    while (ctnode -> next)
    {
        i = ctnode -> next;
        ctnode = (struct contentnode *)mem[i];
    }
    return i;
}

static int oshfs_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{

    fprintf(stderr, "here comes the write part");


    char * data;
    fs_addr allblocknumber,usednumber;
    fs_addr written_size=0;
    fs_addr write_begin_place;
    fs_addr address_next;
    fs_addr num;
    fs_addr backup;
    fs_addr thelastbytes = BLOCKLENGTH + offset % BLOCKLENGTH;
    struct filenode *node = get_filenode(path);
    if (node == NULL)
        return -ENOENT;
    fs_addr * ip = (fs_addr *)mem[0];
    allblocknumber = ip[0];
    usednumber = ip[1];
    //This means the memory is not enough at all
    if (((offset + size - node->st.st_size + BLOCKSIZE - 1) / BLOCKSIZE)>
        (allblocknumber-usednumber))
        return -errno;
    //here handles the write place
    fs_addr placea = node -> firstcontentnode;
    fs_addr newcontentnode;
    fs_addr b;
    struct contentnode * newnode;
    if (placea == 0) {
        placea = createcontentblock();
        node->firstcontentnode = placea;
    }
    fs_addr i;
    if (offset/BLOCKLENGTH <= node->st.st_size / BLOCKLENGTH)
    {
        i = BLOCKLENGTH;
        while (i < offset)
        {
            i = i + BLOCKLENGTH;
            newnode = (struct contentnode *) mem[placea];
            placea = newnode -> next;
        }
    }
    else {
        while (i<offset)
        {
            i = i + BLOCKLENGTH;
            newnode = (struct contentnode *) mem[placea];
            //if placea!=0 means it is not the end of the knowing queue
            if (placea!=0) {

                placea = newnode ->next;
            }
            //else here should set up new block to save the message
            else {
                b = lookupfreeblock();
                mem[b] = create_new_block();
                markbit(b);
                newnode ->next = b;
                placea = b;
            }
        }
    }
    //now we have found the offset bit place
    while (written_size<size)
    {
        if ((size - written_size) <= thelastbytes) {
            data = (char *) mem [placea] + sizeof(fs_addr);// as the mem have some spaces for fs_addr so the data should jump over with it
            memcpy(data,buf+written_size,size-written_size);
            written_size = size;
        }
        else {
            char * data = (char *) mem [placea] + sizeof(fs_addr);
            memcpy(data, buf + written_size,BLOCKLENGTH);
            written_size += BLOCKLENGTH;
            address_next = ((struct contentnode *)  mem[written_size])->next;
            if (!address_next)  {
                backup = createcontentblock();
                ((struct contentnode *)mem[placea])->next = backup;
                address_next = backup;

            }

            placea = address_next;

        }
    }
    if (node->st.st_size < offset)
        num = node -> st.st_size;
    else num = offset;
    if (node ->st.st_size > num + size)
        node ->st.st_size= node ->st.st_size;
    else node ->st.st_size = num + size;
    node -> st.st_blocks = node->st.st_size / BLOCKLENGTH;

    return written_size;
}

static int oshfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi){
  fprintf(stderr,"readdir is ok0\n");
    struct filenode *node=(root ==0)? NULL: (struct filenode *)mem[root];

    struct contentnode * cb ;

	fprintf(stderr,"readdir is ok\n");

    cb=(struct contentnode * ) mem[0];

    fprintf(stderr,"ok1\n");

    filler(buf, ".", NULL, 0);

	fprintf(stderr,"ok2\n");

    filler(buf, "..", NULL, 0);

	fprintf(stderr,"ok3\n");

    while(node){
        filler(buf, node->filename, &(node->st), 0);
        node=mem[node->next] ;
	fprintf(stderr,"oknow");
    }
}

static int oshfs_open(const char *path, struct fuse_file_info *fi)
{


    return 0;
}

static int oshfs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    char * data;
    fs_addr read_position;
    fs_addr read_size;
    fs_addr address_b;
    //very similar to ohsfs_write
    //however in read we doesn't concern with the length problem
    struct filenode *node = get_filenode(path);

    fprintf(stderr, "here comes the read part");

    if (node == NULL) return -ENOENT;
    if (offset + size > node->st.st_size)
        size = node ->st.st_size - offset;
    //here handles the read place
    fs_addr placea = node -> firstcontentnode;
    fs_addr newcontentnode;
    fs_addr b;
    struct contentnode * newnode;


    if (placea == 0)
    {
        placea = createcontentblock();
        node->firstcontentnode = placea;
    }
    int i;
    if (offset/BLOCKLENGTH <= node->st.st_size / BLOCKLENGTH)
    {
        i = BLOCKLENGTH;
        while (i < offset)
        {
            i = i + BLOCKLENGTH;
            newnode = (struct contentnode *) mem[placea];
            placea = newnode->next;
        }
    }
    else {
        while (i<offset)
        {
            i = i + BLOCKLENGTH;
            newnode = (struct contentnode *) mem[placea];
            //if placea!=0 means it is not the end of the knowing queue
            if (placea!=0) {

                placea = newnode ->next;
            }
            //else here should set up new block to save the message
            else {
                b = lookupfreeblock();
                mem[b] = create_new_block();
                markbit(b);
                newnode ->next = b;
                placea = b;
            }
        }
    }
    //now we have found the offset bit place which is placea
    //I should have written it into a function however due to the time ,I jst put it into the function
    read_position = placea;
    while (read_size < size)
    {
        // if the rest of the data is smaller than one block length just get them out
        if (size-read_size<BLOCKLENGTH)
        {
            data = (char *) mem[read_position] + sizeof(fs_addr);
            memcpy(buf + read_size, data, size-read_size);
            read_size = size;
        }
        else {
            data = (char *) mem[read_position] + sizeof(fs_addr);
            memcpy(buf + read_size, data, BLOCKLENGTH);
            read_size += BLOCKLENGTH;
            address_b = ((struct contentnode *)mem[read_position])->next;
            if (!address_b) return -errno;
            read_position = address_b;
        }
    }



}


static const struct fuse_operations op = {
    .init = oshfs_init,
    .getattr = oshfs_getattr,
    .readdir = oshfs_readdir,
    .mknod = oshfs_mknod,
    .open = oshfs_open,
    .write = oshfs_write,
    .truncate =oshfs_truncate,
    .read = oshfs_read,
    .unlink = oshfs_unlink,
};

int main(int argc, char *argv[])
{
    return fuse_main(argc, argv, &op, NULL);
}
