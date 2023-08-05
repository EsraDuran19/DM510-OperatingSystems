#include <math.h>
#include <fuse.h>
 #include <errno.h>
#include <string.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>

#define ONE_BLOCK_SIZE 512
#define INODE_INDICES 200
#define BIG_NUM 4096


FILE * file_system;

int lfs_getattr( const char *, struct stat * );
int lfs_readdir( const char *, void *, fuse_fill_dir_t, off_t, struct fuse_file_info * );
int lfs_open( const char *, struct fuse_file_info * );
int lfs_read( const char *, char *, size_t, off_t, struct fuse_file_info * );
int lfs_release(const char *path, mode_t mode, struct fuse_file_info *fi);



static struct fuse_operations lfs_oper = {
	.getattr	= lfs_getattr,
	.readdir	= lfs_readdir,
	.mknod = NULL,
	.mkdir = NULL,
	.unlink = NULL,
	.rmdir = NULL,
	.truncate = NULL,
	.open	= lfs_open,
	.read	= lfs_read,
	.release = lfs_release,
	.write = NULL,
	.rename = NULL,
	.utime = NULL
};

typedef struct inode_type //block size  //Tree structure
{
  
  
  int size;  //4 bt
  short blocks_count_inode; //#blocks for the inode
  unsigned short data[INODE_INDICES]; //one inode can have (INODE_INDICES)*ONE_BLOCK_SIZE mb.
  int parent;  // parent number (id)
  unsigned char typeofinode;     //1 bt
  int lengthofname;
  
  struct timespec acces_time; //access time
  struct timespec modif_time; //modification time
} inode_type;

typedef union type_block
{
  inode_type inode;
 unsigned char data[ONE_BLOCK_SIZE];  //unsigned
} type_block;


int write_toBlock(void* buffer, int block)
{
	if ( block < 0 ||  buffer == NULL)
	{
		return -EINVAL;
	}
  
  int offset = block*ONE_BLOCK_SIZE;
  fseek(file_system, offset, SEEK_SET);
  if(fwrite(buffer, ONE_BLOCK_SIZE, 1, file_system) == 1)
  {
  return 1;
  }
  else{
    return -EAGAIN;
  }
	return 1;
}

// reads `size` bytes from disk and returns a void* pointer to the data
void* read_from(int block)
{
  if (block >= 0)
	{
		
  void* buffer = calloc(1, sizeof(char) * ONE_BLOCK_SIZE);  //allocate place to a buffer

	int offset = ONE_BLOCK_SIZE*block;
  int result=fseek(file_system, offset, SEEK_SET);
  if(result < 0 || fread(buffer, ONE_BLOCK_SIZE, 1, file_system) != 1){
    free(buffer);
    return -EAGAIN;
  }

	return buffer;
 }
 
 else{
 return  -EINVAL;
	}
}


int getblock()
{
  union type_block* data_bits;
  
  int data_bits_id=0;
  int data_bit_space=6;
  int num_free = -1;
  while (data_bits_id < data_bit_space) {       //First 6 blocks are for data_bits to store
      data_bits = read_from(data_bits_id);
    if(data_bits >= 0)
    {
     
    
    size_t a =0;
    while ( a < ONE_BLOCK_SIZE)
    {
      if(data_bits->data[a] < 252) // get a byte among free ones
      {
        
       num_free = a;
        break;
      }
      a++;
     }
  
    free(data_bits);
    }
    else{
     return data_bits;
    }
      data_bits_id++;
  }
  //we found the free place, let's copy this
  unsigned char temprary = 0;
  memcpy(&temprary, &data_bits->data[num_free], sizeof(char));
  int curr_bit = 0;
  for (size_t i = 0; i < data_bit_space+2; i++) {  //send temprary[i] to curr_bit
     curr_bit = (temprary >> i) ;
    curr_bit= curr_bit & 1U;
 
  }
  //save new curr_bitmap to storage disc, return it.
  memcpy(&data_bits->data[num_free], &temprary, sizeof(char));
  write_toBlock(data_bits, data_bits_id);
  free(data_bits);
  return data_bits_id*BIG_NUM + num_free*(data_bit_space+3) + curr_bit;
}

int free_block(unsigned int block)
{
  //Find block id
  int block_id = block/BIG_NUM;
  //Find indice in tree
  int surplus=block % BIG_NUM;
  int ind= surplus/8;
  
  union type_block* data_curr_bits = read_from(block_id);
  if(data_curr_bits > 0)
  {
  
  unsigned char temprary;
  memcpy(&temprary, &data_curr_bits->data[ind], sizeof(char));
  int a=(block % 8);
  temprary ^= (1UL << a);
  
  memcpy(&data_curr_bits->data[ind], &temprary, sizeof(char));
  //write ind to the disk
  write_toBlock(data_curr_bits, block_id);
  free(data_curr_bits);
  return 0;
  }
  else{
    return data_curr_bits;
  }
  return 0;
}

//setup the root node
int set()
{

  union type_block* data_curr_bits = calloc(1, sizeof(union type_block));
  if(!data_curr_bits)
  {
    return -ENOMEM;
  }
  //Write empty blocks to the 2nd through fifth blocks
  int data_bit_space=6;
  for (size_t i = 1; i < data_bit_space; i++) {
    write_toBlock(data_curr_bits, i);  //The 6 blocks are for the curr_bitmap
  }
  
  unsigned char temprary = 0;
  for (size_t i = 0; i < data_bit_space+1; i++) {
     (temprary |= 1) << i; //Left shift with pipe equal operator
  }
  memcpy(&data_curr_bits->data[0], &temprary, sizeof(char));
  write_toBlock(data_curr_bits, 0);
  free(data_curr_bits);
  
  //******************************
    union type_block* init_block = calloc(1, sizeof(union type_block));
 
  int block_id=getblock();
   init_block->inode.data[0] = block_id;
   
  //initial block
    if( getblock() < 0)
    {
      free(init_block);
      return block_id;
    }
    init_block->inode.blocks_count_inode = 2;
  	init_block->inode.typeofinode = 1;
   	init_block->inode.parent = 0;
    
    
    
    
  	union type_block *block_name = calloc(1, ONE_BLOCK_SIZE);
  
    //Set name length
    init_block->inode.lengthofname = 2;
    
    memcpy(block_name,"/", sizeof(char)*init_block->inode.lengthofname);

  	write_toBlock(block_name, init_block->inode.data[0]);
    free(block_name);

    //Store access and modification times.
  	clock_gettime(CLOCK_REALTIME, &init_block->inode.acces_time);
  	clock_gettime(CLOCK_REALTIME, &init_block->inode.modif_time);

    //write to disc
  	write_toBlock(init_block, data_bit_space);
    free(init_block);


	return 0;
}


int get_file_name(unsigned int traverse_block_id, char* name)
{
   //traverse blocks to look for the name
  union type_block* traverse_block = read_from(traverse_block_id);
  if(traverse_block < 0)
  {
    return -EINVAL;
  }

 
  int curr_block;
  int id=1;
  while ( id < INODE_INDICES)
  {
    //get current block id
    curr_block = traverse_block->inode.data[id];
    if( curr_block < BIG_NUM*6) 
    {
      //current inode
      union type_block* curr_inode = read_from(curr_block);
     
      //name
      union type_block* name_block = read_from(curr_inode->inode.data[0]);
      if(name_block < 0)
      {
        return name_block;
      }
      //copy the name into array to compare
      char name1[curr_inode->inode.lengthofname];
      memcpy(&name1, &name_block->data, curr_inode->inode.lengthofname);
      //if the names and data match, then the candidate block is correct
      free(curr_inode);
        free(name_block); 
      if(strcmp(name1, name) == 0)
      {
     
        free(traverse_block);
        return curr_block;
      }
      
    }
    
    id++;
  }
  //if we could not find
  free(traverse_block);
  return -ENOENT;
}



int find_block_for_path(const char* path)       //***********
{
if(strcmp(path, "/") == 0) // compare, if it is root folder
  {
    return 6;
  }

char path_copy[ONE_BLOCK_SIZE];
  strcpy(path_copy, path); //copy of path


 
  char *path_arr= strtok((char *) path, "/"); 
  
  int b_id = get_file_name(6, path_arr);
  
  while( path_arr != NULL )
  {
    path_arr = strtok(NULL, "/");
    if(path_arr != NULL)
    {
    
    b_id = get_file_name(b_id, path_arr);
    if(b_id < 7)
    {//path does not exist
      return b_id;
    }
    }
    else{
      return b_id;
    }
    
    
    
  }//path is modified, so recopy.
  strcpy((char*) path, path_copy);
  return -ENOENT;
}



//update block and byte count
int update_block_num(int block, int difference_blocks, int difference_size)
{
  union type_block* leaf_node = read_from(block);
  if(leaf_node< 0)
  {
    return leaf_node;
  }
  union type_block* leaf_name = read_from(leaf_node->inode.data[0]);
  if(leaf_name < 0)
  {
    return leaf_name;
  }
 
  char name[ONE_BLOCK_SIZE];
  memcpy(name, leaf_name->data, leaf_node->inode.lengthofname);
  if(strcmp(name, "/") != 0)
  {
    union type_block* parent_node = read_from(leaf_node->inode.parent);
    if(parent_node < 0)
    {
      return parent_node;
    }
    parent_node->inode.size += difference_size;
    parent_node->inode.blocks_count_inode += difference_blocks;
    
    write_toBlock(parent_node, leaf_node->inode.parent);
    short parent = leaf_node->inode.parent;

    free(parent_node);
    free(leaf_node);
    
   update_block_num(parent, difference_blocks, difference_size);
   
  }
  else
  {
    
    
     union type_block* root = read_from(6);
    if(root < 0)
    {
      return root;
    }
    root->inode.blocks_count_inode += difference_blocks;
    root->inode.size += difference_size;
    
    write_toBlock(root, 6); //update
   free(root);
  }
  return 0;
}