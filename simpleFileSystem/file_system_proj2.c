#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h> 
#include <time.h>

// Superblock struct
int free_block_counter=0;
typedef struct  {	
	unsigned int nfree;
	unsigned int freeArray[150];
	unsigned int ninoded;
	unsigned int isize;
	unsigned int fsize;
	char flock;
	char ilock;
	char filemod;
	unsigned int times;
	unsigned int ninode[100];
}superblocktype;

// Initialize base superblock
superblocktype sblock = {
	.nfree = 0,
	.ninoded = 0,
	.isize = 0,
	.fsize = 0,
	.ilock = ' ',
	.times = 0
};

struct inode{ /*inode size is 512 byte, which means 2 inode per block(block size is 1024 byte)*/
unsigned short flags;
unsigned short nlinks;
unsigned short uid;
unsigned short gid;
int size;
unsigned int addr[123]; /* use all 123 slots for double indirect block table when it is large file
                        sum up the double indirect block we have 123 *2^8 * 2 ^8 * 2 ^10=2^6 * 2^26 is about 2 ^33,
                        which means we can access file at least 4 GB larger.
                        
                        when it is small file, 123*1024 is the largest file size that can be accessed/ 
                         */
unsigned int actime;
unsigned int modtime;
};

struct dir{
	unsigned int inode_num;
	char file_path[28];
};


// Function to get a block from the free list
unsigned int get_block_from_freelist(int fd){
	sblock.nfree-=1;
	unsigned int temp;
	if(sblock.nfree==0){
		lseek(fd,(sblock.freeArray[0])*1024,SEEK_SET);
		read(fd,&temp,4);
		unsigned int holder =sblock.freeArray[0];
		//check the first 4 byte content of the freeArray[0], if not 150, we read the very fisrt free_blcok availablee
		if(temp==150){
			sblock.nfree=150;
			//copy the following byte into the freeArray
			for(int i = 0 ; i< 150 ; i++){
				read(fd,&temp,4);
				sblock.freeArray[i] = temp;
			}
			free_block_counter--;
			return holder;
		}
		else{
			sblock.nfree=-1;
			return -1;		
		}
			
	}
	else{
		free_block_counter--;
		return sblock.freeArray[sblock.nfree];
		
		
	}
		
}


	
// makedir function
int mkdir(char dir_input[],int fd){


	char input_copy[1000];
	strcpy(input_copy,dir_input); 
	//input check:
	if(dir_input[0]!='/'){
		printf("input format wrong, must start with / root directory");
		return -1;
	}
	//slice the input directory with delimiter '/'
	char *token;
	const char s[2] = "/";   
	int counter = 0;
    /* get the first token */
    token = strtok(dir_input, s);
    /* walk through other tokens */
    while( token != NULL ) {

    	counter++;
    	token = strtok(NULL, s);
    }
	//counter is the count of all slice of directory 
	char* array_path_slice[counter];
	counter = 0;
    /* get the first token */
	token = strtok(input_copy, s);
    /* walk through other tokens */
	while( token != NULL ) {
   		array_path_slice[counter] = token;
    	counter++;
    	token = strtok(NULL, s);
   	}
	int cur_inode = 1;//making directory starting from root directory , root directory is inode 1;
	for(int i=0; i<counter;i++){
		int temp=find_block_at_parent(fd,cur_inode,array_path_slice[i]);
		if(temp>0){//this means we found the directory.
			cur_inode=temp;
		}
		else{ //we need to add this directory 
			int new_inode =find_free_inode(fd);
			printf(" we need add this direcotry slice %s , find a free cur_inode to sotre it %d \n",array_path_slice[i],new_inode);
			initlize_inode(fd,new_inode,cur_inode);//initialize this inode
			add_at_parent(fd,cur_inode,array_path_slice[i],new_inode);//add this inode at current parent directory 		
			cur_inode=new_inode;//cur inode now is the new inode we just initilized.
		}
	}	
	return cur_inode;
	}


	
// Find Inode using directory input parameter	
int find_inode_given_dir(char dir_input[],int fd){
	char input_copy[1000];
	strcpy(input_copy,dir_input); 
	//input check:
	if(dir_input[0]!='/'){
		printf("input format wrong, must start with / root directory\n");
		return -1;
	}
	char *token;
	const char s[2] = "/";   
	int counter = 0;
	/* get the first token */
	token = strtok(dir_input, s);
    /* walk through other tokens */
    while( token != NULL ) {
    	counter++;
    	token = strtok(NULL, s);
    }
	//counter is the count of all parent directory 
	char* array_path_slice[counter];
	counter = 0;
    /* get the first token */
	token = strtok(input_copy, s);
    /* walk through other tokens */
	while( token != NULL ) {
   		array_path_slice[counter] = token;
    	counter++;
    	token = strtok(NULL, s);
    }
	//counter is the count of all parent directory 
	int cur_inode = 1;//search from root inode directory
	for(int i=0; i<counter-1;i++){
		int temp=find_block_at_parent(fd,cur_inode,array_path_slice[i]);
		if(temp>0){//this means we found the directory.
			cur_inode=temp;
		}	
		else{ 
			return -1;
		}
	}

	return cur_inode;
}








int initlize_inode(int fd,int cur_inode_index , int parent_inode_index){
		printf("inilizing inode with its parent %d and its index is %d\n",parent_inode_index );
        struct inode cur_inode;
        
        cur_inode.size=2;//defualt two entries;
        cur_inode.flags=0b1100000111000000; //this inode is already allocated 
        cur_inode.actime = ( unsigned  int)(time(NULL));
        cur_inode.modtime = ( unsigned  int)(time(NULL));
		int temp=get_block_from_freelist(fd);
		printf("allocate block%d \n",temp);
		if(temp<0){
			printf ("no more free block available \n");
			return -1;
		}
		else {
        cur_inode.addr[0] = temp;
        
        //write inode back with all new attribute
        lseek(fd,2048+(cur_inode_index-1)*512,SEEK_SET);
        write(fd,&cur_inode,512);

		//jump to first first block of addr and add first two default entries;
		struct dir dir1,dir2;
		char a[28] = ".";
        char b[28] ="..";
        dir1.inode_num=cur_inode_index; // initialize first two entry of directory data block for root directory
        memcpy(&dir1.file_path,a,28);
        dir2.inode_num=parent_inode_index;
        memcpy(&dir2.file_path,b,28);
        lseek(fd,1024*temp,SEEK_SET);
        struct dir * dir1_p = &dir1;
        struct dir * dir2_p = &dir2;
        //write the two default entry at that addr[0] for root inode
        write(fd,dir1_p,32);
        write(fd,dir2_p,32);
        printf(" inilized an inode addr block %d at parent %d with size %d\n " , cur_inode_index , parent_inode_index,cur_inode.size);
        return 1;
		}
}











// This function will access the filepath slice from the parent's inode table
int find_block_at_parent(int fd, unsigned int cur_inode, char temp_path[]) {
	struct inode temp_inode;
	lseek(fd, 2048 + (cur_inode - 1) * 512, SEEK_SET);
	read(fd, &temp_inode, 512);
	int temp_block_num = (temp_inode.size - 1) / 32;
	int block_offset = (temp_inode.size - 1) % 32;
	for (int i = 0; i < temp_block_num; i++) {
		int temp_block_index = temp_inode.addr[i];
		lseek(fd, temp_block_index * 1024, SEEK_SET);
		for (int j = 0; j < 32; j++) {
			struct dir* temp_dir_slice;
			read(fd, temp_dir_slice, 32);
			char temp_str[28];
			strcpy(temp_str, temp_dir_slice->file_path);
			if (strcmp(temp_str, temp_path) == 0) {
				return temp_dir_slice->inode_num;
			}
		}
	}

	int temp_block_index = temp_inode.addr[temp_block_num];
	lseek(fd, (temp_block_index) * 1024, SEEK_SET);
	for (int k = 0; k <= block_offset; k++) {
		struct dir temp_dir_slice;
		read(fd, &temp_dir_slice, 32);
		if (strcmp(temp_path, temp_dir_slice.file_path) == 0) {
			return temp_dir_slice.inode_num;
		}

	}
	printf("not find done %s \n",temp_path);
	return -1;
}




// Add the inode to the parent's inode table
int add_at_parent(int fd,unsigned int cur_inode, char temp_path[], int free_inode_index){  //cur_inode is the parent inode index;
	struct inode temp_inode;
	lseek(fd,2048 +(cur_inode-1) * 512,SEEK_SET);
	read(fd,&temp_inode,512);
	temp_inode.size = temp_inode.size+1;
	temp_inode.actime = ( unsigned  int)(time(NULL));
    temp_inode.modtime = ( unsigned  int)(time(NULL));
	if(	temp_inode.size>32*123){
		printf("while adding a entry for inode, entries over flow,this file system maxium support directory of maxium 32*123 entries\n");
		return -1;
	}
	//update the parent inode size time;
	lseek(fd,2048 + (cur_inode -1)* 512,SEEK_SET);
	write(fd,&temp_inode,512);
	int temp_block_num = (temp_inode.size-1)/32 ;
	int block_offset = (temp_inode.size-1)%32 ;
	//struct a new entry and added to parent inode
	struct dir temp_dir;
	unsigned int temp_block;
	memcpy(&temp_dir.file_path,temp_path, 28);
	if(block_offset==0){ //we need allocate a block for this one
		temp_block = get_block_from_freelist(fd);
		printf("added when allocate %d\n",temp_block);
		temp_inode.addr[temp_block_num] = temp_block;
	}
	else {  //we do not need to allocate a block for this one;
		temp_block = temp_inode.addr[temp_block_num];
		
	}
	memcpy(&temp_dir.inode_num,&free_inode_index, 4);
	lseek(fd,temp_block*1024+block_offset*32,SEEK_SET);
	write(fd,&temp_dir,32);
	return 0;
}



void add_block_to_freelist(long block_num, int fd){
    /*adda block to the free list*/
    free_block_counter++;
    if (sblock.nfree==150){ //check if the freeArray is full
        lseek(fd,(block_num)*1024,SEEK_SET);
        write(fd,&sblock,151*4); //151 *4 are first 151 *4 bytes that stores the nfree and freeArray[150] 
        sblock.freeArray[0]=block_num;
        sblock.nfree=1;
        
    }
    else{
        sblock.freeArray[sblock.nfree]=block_num;
        sblock.nfree++;
    }
    sblock.times= ( unsigned long)(time(NULL));
   
}

// Initialize file system function, takes filepath parameter, # of blocks, # of inodes
int initfs(char file_name[] , int n1 ,int n2){
	//check parameter value
    if(n1>0 && n2>0){
    	//parameter value is valid 
        int fd;
        //open file if not exist creat it.
        fd=open(file_name,O_RDWR|O_CREAT, 0666);
		if (fd < 0 ) {
			printf("cannot inilizing with given input\n");
			return fd;
		}
		//add all the blocks to free_list 
        int inode_block_num = (n2+1)/2; //get the ceiling of the n2/
		sblock.isize = n2;
		sblock.fsize = n1;
		int data_block_num = n1 -2 -inode_block_num; /* first two are boot block and super block*/
        for(int i = 2 + inode_block_num; i<n1; i++){  //data block from 2 + inode_block_num
            add_block_to_freelist(i,fd);
        }
        //initialize the root directory 
        struct inode root_dir;
        root_dir.flags=0b1100000111000000;  //small file with wre permission for user
        struct dir dir1,dir2;
        char a[28] = ".";
        char b[28] ="..";
        dir1.inode_num=2; // initlize first two entry of directory data block for root directory
        memcpy(&dir1.file_path,a,28);
        dir2.inode_num=2;
        memcpy(&dir2.file_path,b,28);
        root_dir.addr[0]=get_block_from_freelist(fd);
        lseek(fd,root_dir.addr[0]*1024,SEEK_SET);
        struct dir * dir1_p = &dir1;
        struct dir * dir2_p = &dir2;
		root_dir.size = 2;
        //write the two default entry at that addr[0] for root inode
        write(fd,dir1_p,32);
        write(fd,dir2_p,32);
        lseek(fd,2*1024,SEEK_SET);
        struct inode * root_dir_p = &root_dir;
        //write the root inode 
        write(fd,root_dir_p,512);
        //initilize the following inode flags;
        unsigned short defualt_flag=0b0100000111000000;
        unsigned short* defualt_flag_p = & defualt_flag;
        for(int i = 2; i<=sblock.isize;i++){
        	 lseek(fd,2*1024+ (i-1)*512,SEEK_SET);
        	 write(fd,defualt_flag_p,2);
  	
        }
        return fd;

    }
    else{
        printf("Parameter value is not valid\n");
    	return -1;
    }

}
int find_free_inode(int fd){
	//sequential search for free_inode 
	for(int i = 1; i<sblock.isize;i++){
		lseek(fd,2048+(i-1)*512,SEEK_SET);
		unsigned short temp_flag;
		read(fd,&temp_flag,2);
		//get flags of each inode, then check if its 1st digit is 1, 1 means it is already allocated.
		temp_flag=temp_flag&0x8000;

		temp_flag=temp_flag>>15;
		if(temp_flag==0){
			return i;
			break;// i_th inode is free;
		}
	}
	return -1;
}
// Save & Quit function to write superblock and save state
void save_quit(int fd){
    lseek(fd,1024,SEEK_SET);
    write(fd, &sblock, sizeof(superblocktype));
    close(fd);
}

// cpin function - copy an external file's contents to an internal file
int cpin(int fd,char externalfile[], char v6_file[]){
	char input_copy[1000];//make two copies of input v6_file becuase strtok will alter its input string.
	char input_copy2[1000];
	strcpy(input_copy,v6_file); 
	strcpy(input_copy2,v6_file); 
	//input check:
	if(v6_file[0]!='/'){
		printf("input format wrong, must start with / root directory\n");
		return -1;
	}
	char *token;
	const char s[2] = "/";   
	int input_counter = 0;
    /* get the first token */
	token = strtok(v6_file, s);
    /* walk through other tokens */
    while( token != NULL ) {
     	input_counter++;
     	token = strtok(NULL, s);
    }
	//counter is the count of all parent directory slice
	char* array_path_slice[input_counter];
	input_counter = 0;
   /* get the first token */
	token = strtok(input_copy, s);
   /* walk through other tokens */
	while( token != NULL ) {
   		array_path_slice[input_counter] = token;
    	input_counter++;
    	token = strtok(NULL, s);
    }
	int parent_inode_index=find_inode_given_dir(input_copy2,fd); //find the parent directory's inode;
	int externalfd =open(externalfile,O_RDONLY);//open the external file
	if(externalfd<0){
	 	printf("cannot open external file, check agian\n");
	 	return -1;
	 }
	int external_size= lseek(externalfd,0,SEEK_END);
	int num_block = (external_size-1)/1024 +1; // number of data block needed to store the external file
	int num_block_remainder =external_size%1024;	
	if(free_block_counter<num_block){
		printf("cannot cpin because file to large to be fited inside need  % d blocks and %d available\n",num_block,free_block_counter);
		return -1;
	}
	if(num_block_remainder==0){
		num_block_remainder=1024;
	}
	struct inode parent_inode;
	struct inode file_inode;
	lseek(fd,2048+(parent_inode_index-1)*512,SEEK_SET);//read parent inode 
	read(fd,&parent_inode,512);
	//set  external file offset to beginning of the file
	lseek(externalfd,0,SEEK_SET);
	int file_inode_index =find_free_inode(fd); 
	//find  a free inode to cpin
	//initilize this inode
    printf("number of block to be cpin %d  \n",num_block);
	file_inode.size=0;
    int step=1024;
	if(num_block<123){//small file 
		printf("this is a small file.starting cpin\n");
		char temp_external_slice[1024];
		for(int i =0; i<=num_block;i++){
			if(i==num_block){step=num_block_remainder;}//when file size is not divied by 1024, change the size of last read external and write inside V6
			read(externalfd,&temp_external_slice,step);
			add_content_to_small_file(fd,temp_external_slice,&file_inode,step);//helper function to add the content.
		}
		//update attribute of the file_inode
	    file_inode.flags=0b1000000111000000; //this inode is already allocated as small file
	    }
    else{
		printf("this is a large file with %d block need to be cpin\n",num_block);
		//we need use the double indirect block format.
		//dealing with full double block first
		int double_index_amount = (num_block-1)/(256*256)+1; //number of element inside fileinode.addr 
		int double_index_amount_remainder =(num_block-1)%(256*256);
		for(int i =0; i <double_index_amount-1 ; i++ ){
			int double_index_block = get_block_from_freelist(fd);//allocate a block inside fileinode.addr to store double indrect blocks
			file_inode.addr[i]=double_index_block;//update the inode.addr
			for(int j = 0 ; j <256 ; j++){
				int temp_j_index = get_block_from_freelist(fd); //allocate the block for singular indirect blocks
				lseek(fd,double_index_block*1024 + j*4,SEEK_SET);
				write(fd,&temp_j_index,4);//write this block index inside  element inside addr
				for(int k = 0 ; k <256 ; k++){
					char temp_external_slice[1024];
					int actucal_block_index = get_block_from_freelist(fd);//get a free block to be written as data block
					lseek(fd,temp_j_index*1024+k*4,SEEK_SET);//write this block idnex inside singular block
					write(fd,&actucal_block_index,4);
					read(externalfd,&temp_external_slice,1024);//read a blockside slice from external and write it to file system
					lseek(fd,actucal_block_index*1024,SEEK_SET);
					write(fd,&temp_external_slice,1024);
				}
						
			}
		}
	
		
		// considering edge case not fullfilled block
		//get a free block to be used as an element of addr and update inode
		int temp_i_inode_addr = get_block_from_freelist(fd);
		file_inode.addr[double_index_amount-1]=temp_i_inode_addr;
		double_index_amount_remainder=(num_block-1)%(256*256);
		int double_index = double_index_amount_remainder/256; //number of singular blocks
		int double_index_reminder = double_index_amount_remainder%256;  //index of last block inside last singular block
		for(int i =0; i <= double_index;i++){
			int temp_i = get_block_from_freelist(fd); //this is the block number of singular table
			lseek(fd,temp_i_inode_addr*1024+i*4,SEEK_SET);
			write(fd,&temp_i,4);//write the block index of singular block inside double indirect blcok
			int j_val;
			if(i==double_index){
				j_val = double_index_reminder;// if this is last singular block the number of block inside is different
			}
			else{
				j_val=255;
			}
			for(int j = 0; j <=j_val; j++){
				char temp_external_slice[1024];
				int temp_k = get_block_from_freelist(fd); //this is the block number of data block
				lseek(fd,temp_k*1024,SEEK_SET);
				//change the size of last read or write to remainder of byte_size of the file %1024
				if(i==double_index&&j==j_val){
					read(externalfd,&temp_external_slice,num_block_remainder);
					write(fd,&temp_external_slice,num_block_remainder);}
				else{
					read(externalfd,&temp_external_slice,1024);
					write(fd,&temp_external_slice,1024);}
				lseek(fd,temp_i*1024+j*4,SEEK_SET);
				write(fd,&temp_k,4);//write the data blcok index inside signular block
			
			}
		}
	}	
	//update the file_inode 
	file_inode.size=external_size;
    file_inode.actime = ( unsigned  int)(time(NULL));
    file_inode.modtime = ( unsigned  int)(time(NULL));
	lseek(fd,2048+512*(file_inode_index-1),SEEK_SET);
	struct inode* file_inode_p =&file_inode;
	write(fd,file_inode_p,512);
	//update its parents' directory 
	add_at_parent(fd,parent_inode_index,array_path_slice[input_counter-1] ,file_inode_index);
	
};
	
	
	
	


int add_content_to_small_file(int fd,char block_wise_slice[],struct inode*  file_inode,int step){
	unsigned int temp_block =get_block_from_freelist(fd);
	printf("line648 get a free block %d\n",temp_block);
	lseek(fd,temp_block*1024,SEEK_SET);
	write(fd,block_wise_slice,step);
	file_inode->addr[file_inode->size]=temp_block;
	file_inode->size++;
	
}

	
	
	





int cpout(int fd,char externalfile[], char v6_file[]){
	char input_copy2[1000];
	char input_copy[1000];
	strcpy(input_copy,v6_file); 
	strcpy(input_copy2,v6_file); 
	//input check:
	if(v6_file[0]!='/'){
		printf("input format wrong, must start with / root directory\n");
		return -1;
	}
	char *token;
	const char s[2] = "/";   
	int input_counter = 0;
    /* get the first token */
	token = strtok(v6_file, s);
    /* walk through other tokens */
    while( token != NULL ) {
     	input_counter++;
     	token = strtok(NULL, s);
    }
	//counter is the count of all parent directory slice
	char* array_path_slice[input_counter];
	input_counter = 0;
   /* get the first token */
	token = strtok(input_copy, s);
   /* walk through other tokens */
	while( token != NULL ) {
   		array_path_slice[input_counter] = token;
    	input_counter++;
    	token = strtok(NULL, s);
    }
    //find_inode_given_dir will give its parent inode_index when passing through the absolute path of the file 
	int parent_index=find_inode_given_dir(input_copy2,fd);
	int inner_inode_index = find_block_at_parent(fd,parent_index,array_path_slice[input_counter-1]);
	if(inner_inode_index<0){
		printf("no file inside file system cannot cpout\n");
		return -1;
	}
	//find file to be cpout under its parent direcotry 
	int externalfd =open(externalfile,O_WRONLY|O_CREAT,0666);
	struct inode inner_inode;//new inode for the file to be cpout
	lseek(fd,2048+512*(inner_inode_index-1),SEEK_SET);
	read(fd,&inner_inode,512);
	int file_block_size =  (inner_inode.size-1)/1024 +1; //size meansured in 1024 bytes block;
	int file_block_size_remainder =  inner_inode.size%1024;
	if(file_block_size_remainder==0){file_block_size_remainder=1024;}
	//small or large file
	if(file_block_size<=123){//if the file is small 
		int step = 1024;
		for(int i =0;i<file_block_size;i++){
			if(i==file_block_size-1)
				{step=file_block_size_remainder;}//change the stepsie of last read write
			char temp_data_slice[1024];
			lseek(fd,inner_inode.addr[i]*1024,SEEK_SET);
			read(fd,&temp_data_slice,step);//get slice by using element inside addr
			write(externalfd,&temp_data_slice,step);
		}
    }
	
	else{//file is large we use double indirect format  
		//first considering the double indirect blocks that are full
		int double_index_amount=(file_block_size-1)/(256*256)+1; //number of double indirect element insdie addr 
		for(int i =0; i < double_index_amount-1 ; i++ ){
			int temp_i_inode = inner_inode.addr[i];
			for(int j = 0 ; j <256 ; j++){
				lseek(fd,temp_i_inode*1024 + j*4,SEEK_SET);
				int temp_j_inode_index;
				read(fd,&temp_j_inode_index,4); //get the singular block index
				for(int k = 0 ; k <256 ; k++){
					char temp_external_slice[1024];
					int actucal_block_index;
					lseek(fd,temp_j_inode_index*1024+k*4,SEEK_SET);
					read(fd,&actucal_block_index,4);// get the block index of the data block 
					lseek(fd,actucal_block_index*1024,SEEK_SET);
					read(fd,&temp_external_slice,1024);
					write(externalfd,&temp_external_slice,1024); //read the content inside the data blcok and write it to external file
				
				}
			}
		}
		// considering edge case which is not fullfilled double indirect block 
		int temp_i_inode_addr = inner_inode.addr[double_index_amount-1];
		int double_index_amount_remainder=file_block_size%(256*256);
		int single_index = (double_index_amount_remainder-1)/256 +1;
		int single_index_reminder = (double_index_amount_remainder-1)%256;
		for(int i =0; i < single_index;i++){
			lseek(fd,temp_i_inode_addr*1024+i*4,SEEK_SET);
			int temp_i; //this is the block number of singular table
			read(fd,&temp_i,4);
			int j_val;
			if(i==single_index-1){
				j_val = single_index_reminder; //adjust when last singular block is not full
			}
			else{
				j_val=255;
			}
			for(int j = 0; j <=j_val; j++){
				lseek(fd,temp_i*1024+j*4,SEEK_SET);
				char temp_external_slice[1024];
				int temp_k; //this is the block number of data block
				read(fd,&temp_k,4); 
				lseek(fd,temp_k*1024,SEEK_SET);
				//adjust the stepsize of last read or write
				if(i==single_index-1&&j==j_val){
					read(fd,&temp_external_slice,file_block_size_remainder);
					write(externalfd,&temp_external_slice,file_block_size_remainder);
				}
				else{
					
					read(fd,&temp_external_slice,1024);
					write(externalfd,&temp_external_slice,1024);
				}
					
			}
		}
			
			printf("cpout finished  \n");
		
	}
}


int rm(int fd,char v6_file[]){

	char input_copy[1000];//make two copies of input v6_file becuase strtok will alter its input string.
	char input_copy2[1000];
	memset(input_copy, '\0', sizeof(input_copy));
	memset(input_copy2, '\0', sizeof(input_copy));
	strcpy(input_copy,v6_file); 
	strcpy(input_copy2,v6_file); 
	//input check:
	if(v6_file[0]!='/'){
		printf("input format wrong, must start with / root directory");
		return -1;
	}
	char *token;
	const char s[2] = "/";   
	int input_counter = 0;
    /* get the first token */
	token = strtok(v6_file, s);
    /* walk through other tokens */
    while( token != NULL ) {
     	input_counter++;
     	token = strtok(NULL, s);
    }
	//counter is the count of all parent directory slice
	char* array_path_slice[input_counter];
	input_counter = 0;
   /* get the first token */
	token = strtok(input_copy, s);
   /* walk through other tokens */
	while( token != NULL ) {
   		array_path_slice[input_counter] = token;
    	input_counter++;
    	token = strtok(NULL, s);
    }
    //find_inode_given_dir will give its parent inode_index when passing through the absolute path of the file 
	int parent_index=find_inode_given_dir(input_copy2,fd);
	int file_inode_index = find_block_at_parent(fd,parent_index,array_path_slice[input_counter-1]);
	//find file to be removed under its parent direcotry 
	if(parent_index<0){
		printf("The direcotry of the file to be removed does not exist.\n");
	}

	if(file_inode_index<0){
		printf("the file to be removed does not eixst.\n");
		return -1;
	}
	struct inode inner_inode;  //inner_inode is the inode of the file to be removed 
	struct inode parent_inode; //its parent inode;
	//read its parent inode;
	lseek(fd,2048+512*(parent_index-1),SEEK_SET);
	read(fd,&parent_inode,512);
	parent_inode.size =	parent_inode.size-1;
	int block_index=parent_inode.size/32;
	int block_remainder = parent_inode.size%32;
	if(block_remainder ==0){
		add_block_to_freelist(parent_inode.addr[block_index],fd);
		//if the parent inode with size 32*k +1,  we only need to delete last block of its addr to update this inode;
	}
	//write back the updated parent inode;
	lseek(fd,2048+512*(parent_index-1),SEEK_SET);
	write(fd,&parent_inode,512);
	//read the file inode;
	lseek(fd,2048+512*(file_inode_index-1),SEEK_SET);
	read(fd,&inner_inode,512);
	int file_block_size =  (inner_inode.size-1)/1024 +1; //size meansured in 1024 bytes block;
	int file_block_size_remainder =  inner_inode.size%1024;
	if(file_block_size_remainder==0){file_block_size_remainder=1024;}
	printf("size deleted %d\n",file_block_size);
	//different case under small or large file
	if(file_block_size<=123){// under small format the max block size is 123 because addr has length 123;
		for(int i =0;i<file_block_size;i++){
			add_block_to_freelist(inner_inode.addr[i],fd);//add each block to freelist
		}
     }
	else{//file is large, and we use doulbe indirect block to format the file
		int double_index_amount=(file_block_size-1)/(256*256)+1;//number of blocks need inside addr
		//we first considering all the block inside addr thats full
		for(int i =0; i < double_index_amount-1 ; i++ ){
			int temp_i_inode = inner_inode.addr[i];//get its current addr element
			for(int j = 0 ; j <256 ; j++){
				lseek(fd,temp_i_inode*1024 + j*4,SEEK_SET);
				int temp_j_inode_index;
				read(fd,&temp_j_inode_index,4);//get the value of singular indirect block
				for(int k = 0 ; k <256 ; k++){
					char temp_external_slice[1024];
					int actucal_block_index;
					lseek(fd,temp_j_inode_index*1024+k*4,SEEK_SET); //get actucal block index inside the singular block
					read(fd,&actucal_block_index,4);
					add_block_to_freelist(actucal_block_index,fd); //add the block to free list;	
				}
			}
		}
		// considering edge case not fullfilled block
		int temp_i_inode_addr = inner_inode.addr[double_index_amount-1];
		int double_index_amount_remainder=file_block_size%(256*256);
		int single_index = (double_index_amount_remainder-1)/256 +1;
		int single_index_reminder = (double_index_amount_remainder-1)%256;
		for(int i =0; i < single_index;i++){
			
			lseek(fd,temp_i_inode_addr*1024+i*4,SEEK_SET);
				
			int temp_i; //this is the block number of singular table
			read(fd,&temp_i,4);
			
			int j_val;
			if(i==single_index-1){
				j_val = single_index_reminder;//edge case when the singular block is not full 
			}
			else{
				j_val=255; //when its full
			}
			
			for(int j = 0; j <=j_val; j++){
				lseek(fd,temp_i*1024+j*4,SEEK_SET);
				int temp_k; //this is the block number of data block
				read(fd,&temp_k,4);
				add_block_to_freelist(temp_k,fd);//free that block
			}
		}
			

	}
	return 0;
	};


// Main function to run the program
main(){
	int fd;
	fd= initfs("fileSystem.data",200000,300);
	char  directory[] = "/v6/V5"; // new directory under the file system
	mkdir(directory,fd);
	char newfile1[]=  "/v6/V5/file";
	char newfile2[]=  "/v6/V5/file";
	cpin(fd,"large.bin",newfile1); // copy into the system
	//rm(fd,v6_file);
	cpout(fd,"copyed.data",newfile2);//copy out of the system
	return 0;
}
            