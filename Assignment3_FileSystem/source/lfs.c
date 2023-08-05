#include "lfs.h"
int isFullwrite(size_t size, off_t offset, union type_block* wr_node,int wr_node_id){

 
     int extra_blocks = (int)ceil((double)(offset + size - (wr_node->inode.blocks_count_inode - 2)*512)/(double)ONE_BLOCK_SIZE);
//allocate new blocks
    for(int i = 0; i < extra_blocks; i++)
    {
     
      wr_node = read_from(wr_node_id);
     
       int empty_spot = 0;
 
  while(wr_node->inode.data[empty_spot] != 0 && wr_node->inode.data[empty_spot] < BIG_NUM*6)
  {
    empty_spot += 1;
  }

      
      if(empty_spot < 0)
      {
        return empty_spot;  //No empty, return
      }
      
      int b_id = getblock();
      
      wr_node->inode.data[empty_spot] = b_id;
      write_toBlock(wr_node, wr_node_id);
      

      union type_block* block = calloc(1, ONE_BLOCK_SIZE);
     
      write_toBlock(block, b_id);
     
     free(block);
    }
   
    wr_node = read_from(wr_node_id);
     


  if(offset + size > wr_node->inode.size)
  {
    
    wr_node->inode.size = offset + size;
  }
 
  
    wr_node->inode.blocks_count_inode += extra_blocks;
    write_toBlock(wr_node, wr_node_id);
    
    return 0;

}



int lfs_write( const char *path, const char *buffer, size_t size, off_t offset,
              struct fuse_file_info *fi)
{
  int initial_off = offset;
  int wr_node_id = find_block_for_path(path);
  if(wr_node_id < 0)
  {
    return -EAGAIN;
  }
  union type_block* wr_node = read_from(wr_node_id);
  if(wr_node < 0)
  {
    return wr_node;
  }

  
  int old_size = wr_node->inode.size;
  int old_blocks = wr_node->inode.blocks_count_inode;


  if((wr_node->inode.blocks_count_inode - 2)*512 < offset + size )
  {
   isFullwrite(size,offset,wr_node,wr_node_id);
  }


  

   wr_node = read_from(wr_node_id);
  int block_count = (int) (size/ONE_BLOCK_SIZE);


  for(int i = (offset/ONE_BLOCK_SIZE) + 1; i < block_count + (offset / ONE_BLOCK_SIZE) + 1; i++) 
  {
    union type_block* b_id = read_from(wr_node->inode.data[i]);
  
    memcpy(&b_id->data[offset % ONE_BLOCK_SIZE], &buffer[offset - initial_off], ONE_BLOCK_SIZE - (offset % ONE_BLOCK_SIZE) );
    write_toBlock(b_id->data, wr_node->inode.data[i]);
    if(size < ONE_BLOCK_SIZE){
      offset += size;
      size =0;
    }
    else
    {
      
      offset =offset+  ONE_BLOCK_SIZE;
      size =size-  ONE_BLOCK_SIZE;
     }
  }
  clock_gettime(CLOCK_REALTIME, &wr_node->inode.modif_time);
 
  clock_gettime(CLOCK_REALTIME, &wr_node->inode.acces_time);
  
  write_toBlock(wr_node, wr_node_id);
  
 update_block_num(wr_node_id, wr_node->inode.blocks_count_inode - old_blocks, offset - old_size);
  return (offset - old_size); //how much byte is written?
}

//**********************************************************************************************************************************

int main( int argc, char *argv[] )
{
  file_system = fopen("file", "r+");
  
  if(file_system == NULL)
  {
    printf("File system is ready. \n");
    return -ENOENT;
  }

  union type_block* root = read_from(6);  //Root is at sixth block
  int count=root->inode.blocks_count_inode;
  if( count> 1 ) {
    if(count < BIG_NUM*ONE_BLOCK_SIZE ){
  
      fuse_main( argc, argv, &lfs_oper );
      return 0;
    }
  }
  
  set(); //If opens initially
  fuse_main( argc, argv, &lfs_oper );
	return 0;
}


int lfs_getattr( const char *path, struct stat *stbuf )
{
  int res = 0;
	printf("getattr: (path=%s)\n", path);
 
	memset(stbuf, 0, sizeof(struct stat));
 
  int id = find_block_for_path(path);
 
  union type_block* block = read_from(id);
 
 // Arrange its mode
  if(block->inode.typeofinode == 1)
  {
   stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
  }
  else if(block->inode.typeofinode == 0)
  {
   
    stbuf->st_mode = S_IFREG | 0777;
		stbuf->st_nlink = 1;
		stbuf->st_size = 12;
  }
  else
  {
    res= -EINVAL;
  }
  stbuf->st_blksize = ONE_BLOCK_SIZE;
  stbuf->st_size = block->inode.size;
  stbuf->st_atim = block->inode.acces_time;
  stbuf->st_mtim = block->inode.modif_time;
  stbuf->st_blocks = block->inode.blocks_count_inode;

  free(block);

	return res;
}



int lfs_mkdir(const char* path, mode_t mode)
{
  
  int block = getblock();     //create new block
 
  char path_copy[ONE_BLOCK_SIZE];
  strcpy(path_copy, path);
  
  short parent_folder_id = find_block_for_path(dirname(path_copy));
  
  union type_block* parent_folder = read_from(parent_folder_id);
  
    int empty_spot = 0;
 
  while(parent_folder->inode.data[empty_spot] != 0 && parent_folder->inode.data[empty_spot] < BIG_NUM*6)
  {
    empty_spot += 1;
  }

  
  
  parent_folder->inode.data[empty_spot] = block;
 
  write_toBlock(parent_folder, parent_folder_id); // Save the new folder and parent
  free(parent_folder); 

  
  union type_block* new_fold = calloc(1, ONE_BLOCK_SIZE);

 new_fold->inode.blocks_count_inode = 2;
  new_fold->inode.parent = parent_folder_id;
  new_fold->inode.typeofinode = 1;
 
  new_fold->inode.size = 0;
	clock_gettime(CLOCK_REALTIME, &new_fold->inode.acces_time);
	clock_gettime(CLOCK_REALTIME, &new_fold->inode.modif_time);

  
  int b_id = getblock();
 
  new_fold->inode.data[0] = b_id;

  //set the length of the name, and copy the name into its block
  new_fold->inode.lengthofname = strlen(basename((char *) path)) + 1;
   union type_block* fold_name = calloc(1,ONE_BLOCK_SIZE);
  memcpy(fold_name->data, basename((char *) path), new_fold->inode.lengthofname);

 

  write_toBlock(fold_name, new_fold->inode.data[0]);
 
  write_toBlock(new_fold, block);
  

 update_block_num(block, 2, 0); //

  return 0;
}


int lfs_rmdir(const char* path)  //Remove the directory
{
  int id = find_block_for_path(path);
 
  union type_block* block_rmv = read_from(id);
 
  if(block_rmv->inode.typeofinode != 1)   //type of the folder does not allow to delete.
  {
    return -ENOTDIR;
  }
  
  if(block_rmv->inode.blocks_count_inode < 2) //there are blocks in inode, not empty
  {
  update_block_num(id, -2, 0);    // Delete
  }else{
    return -ENOTEMPTY;
  }
 

  union type_block* parent_rmv = read_from(block_rmv->inode.parent);

  for(int i = 0; i < INODE_INDICES; i++)
  {
    
    if(parent_rmv->inode.data[i] == id)
    {
      parent_rmv->inode.data[i] = 0;  
      break;
    }
  }
  
  write_toBlock(parent_rmv,block_rmv->inode.parent);  //Update disc and free
  free(parent_rmv);


  free_block(block_rmv->inode.data[0]);
 

  return 0;
}

int lfs_readdir( const char *path, void *buffer, fuse_fill_dir_t filler,
                off_t offset, struct fuse_file_info *fi )
{
printf("readdir: (path=%s)\n", path);

	if(strcmp(path, "/") != 0){
		return -ENOENT;}
   
   
  filler(buffer, ".", NULL, 0);
  filler(buffer, "..", NULL, 0);

  int direct_id=find_block_for_path(path);
  if( direct_id < 0)
  {
    return direct_id; 
  }

  union type_block* directory = read_from(direct_id);
  if(directory < 0)
  {
    return directory;
  }
int i;
  for (i = 1; i < INODE_INDICES; i++) {
   
    if(directory->inode.data[i] <! 0) {
    if(directory->inode.data[i] < BIG_NUM*6)   //Checks validty
    {
     
       union type_block* copy_block = read_from(directory->inode.data[i]);
      
      
      union type_block* name = read_from(copy_block->inode.data[0]);
    
     
      char data[copy_block->inode.lengthofname];
      memcpy(&data, &name->data, copy_block->inode.lengthofname);
      filler(buffer, data, NULL, 0);

      free(copy_block);
      free(name);
    }
    }
  }


	return 0;
}



int lfs_release(const char* path, mode_t mode, struct fuse_file_info *fi)
{
  char copy_path[ONE_BLOCK_SIZE];
  strcpy(copy_path, path);
  
  int parent_folder_id = find_block_for_path(dirname(copy_path));
   union type_block* parent_dir = read_from(parent_folder_id);
  int new_id = getblock();
 
  
  
    int empty_spot = 0;
 
  while(parent_dir->inode.data[empty_spot] != 0 && parent_dir->inode.data[empty_spot] < BIG_NUM*6)
  {
    empty_spot += 1;
  }


  if(empty_spot < 0)
  {  //no empty spot
    return empty_spot;
  }
  
  parent_dir->inode.data[empty_spot] = new_id;
  write_toBlock(parent_dir, parent_folder_id);  //write and save
  free(parent_dir);


  union type_block* new_file = calloc(1, ONE_BLOCK_SIZE);  // Give pointers to a new created file
 
 
  union type_block* new_name = calloc(1, ONE_BLOCK_SIZE);
 
  new_file->inode.lengthofname = strlen(basename((char *) path)) ;  //+1


  memcpy(new_name->data, basename((char *) path), new_file->inode.lengthofname);
   int new_name_id = getblock();
  write_toBlock(new_name, new_name_id);
  free(new_name);

  
  
  
  clock_gettime(CLOCK_REALTIME, &new_file->inode.acces_time);
	clock_gettime(CLOCK_REALTIME, &new_file->inode.modif_time);
  new_file->inode.data[0] = new_name_id;
  new_file->inode.typeofinode = 0;
  new_file->inode.blocks_count_inode = 2;
  new_file->inode.size = 0;
  new_file->inode.parent = parent_folder_id;
 
  write_toBlock(new_file, new_id);
 update_block_num(new_id, new_file->inode.blocks_count_inode, new_file->inode.size);
  free(new_file);

  return 0;
}

int lfs_unlink(const char *path)
{
 
  int id = find_block_for_path(path);
  union type_block* block_rmv = read_from(id);
  if(block_rmv < 0)
  {
    return block_rmv;
  }

 
 update_block_num(id, -block_rmv->inode.blocks_count_inode, -block_rmv->inode.size);

  
  union type_block* parent_rmv = read_from(block_rmv->inode.parent);  //remove dependencies
 
  for(int i = 0; i < INODE_INDICES; i++)
  {
    if(block_rmv->inode.data[i] > 0){
      if( block_rmv->inode.data[i] < BIG_NUM*6){
        free_block(block_rmv->inode.data[i]);
      }
    }
    if(parent_rmv->inode.data[i] == id)
    {
      parent_rmv->inode.data[i] = 0;
    }
    
  }
  
  write_toBlock(parent_rmv, block_rmv->inode.parent);
 
  return 0;
}


int lfs_open( const char *path, struct fuse_file_info *fi )
{
 printf("open: (path=%s)\n", path);
   
  

  return 0;
}


int lfs_read( const char *path, char *buffer, size_t size, off_t offset,
              struct fuse_file_info *fi )
{
   printf("read: (path=%s)\n", path);
   
  int initial_off = offset;
  int returnPointer;

 
  int found_block = find_block_for_path(path);
  union type_block* readnode = read_from(found_block);

  if(readnode->inode.size < size ) // If the user wants to read more than exist, read all.
  {
    returnPointer = size;
    size = readnode->inode.size;
  }

  int block_count = (int)  ((double)size/ ONE_BLOCK_SIZE);

  
  for(int i = offset/ONE_BLOCK_SIZE + 1; i < block_count + offset/ONE_BLOCK_SIZE + 1; i++)
  {
    union type_block* b_id = read_from(readnode->inode.data[i]);

    memcpy(&buffer[offset - initial_off], &b_id->data[offset % ONE_BLOCK_SIZE], ONE_BLOCK_SIZE);
    if(size - offset > ONE_BLOCK_SIZE){
      offset +=  ONE_BLOCK_SIZE - (offset % ONE_BLOCK_SIZE);
    }
    else
    {
      offset += (long) size;
    }
    
  }
  
  clock_gettime(CLOCK_REALTIME, &readnode->inode.acces_time);
  write_toBlock(readnode, found_block);
 
  return returnPointer;
}

