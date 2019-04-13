#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>

/* command	action
 * -------	------
 *  root	initialize root directory
 *  print	print current working directory and all descendants
 *  chdir	change current working directory (.. refers to parent directory)
 *  mkdir	sub-directory create 
 *  rmdir	              delete 
 *  mvdir	              rename 
 *  mkfil	file create
 *  rmfil	     delete
 *  mvfil	     rename
 *  szfil	     resize 
 *  exit        quit the program
 */

int debug = 1;	// extra output; 1 = on, 0 = off

int do_root (char *name, char *size);
int do_print(char *name, char *size);
int do_chdir(char *name, char *size);
int do_mkdir(char *name, char *size);
int do_rmdir(char *name, char *size);
int do_mvdir(char *name, char *size);
int do_mkfil(char *name, char *size);
int do_rmfil(char *name, char *size);
int do_mvfil(char *name, char *size);
int do_szfil(char *name, char *size);
int do_exit (char *name, char *size);
/*
    returns 0 (success) or -1 (failure)
*/

struct action {
  char *cmd;					// pointer to string
  int (*action)(char *name, char *size);	// pointer to function
} table[] = {
    { "root" , do_root  },
    { "print", do_print },
    { "chdir", do_chdir },
    { "mkdir", do_mkdir },
    { "rmdir", do_rmdir },
    { "mvdir", do_mvdir },
    { "mkfil", do_mkfil },
    { "rmfil", do_rmfil },
    { "mvfil", do_mvfil },
    { "szfil", do_szfil },
    { "exit" , do_exit  },
    { NULL, NULL }	// end mark, do not remove ,gives wierd errors! :(
};

/*--------------------------------------------------------------------------------*/
void printing(char *name);
void print_descriptor ( );
void parse(char *buf, int *argc, char *argv[]);
int allocate_block (char *name, bool directory ) ;
void unallocate_block ( int offset );
int find_block ( char* name, bool directory );

int add_descriptor ( char * name );
int edit_descriptor ( int free_index, bool free, int name_index, char * name );
int edit_descriptor_name (int index, char* new_name);
int add_directory( char * name );
int remove_directory( char * name );
int rename_directory( char *name, char *new_name );
int edit_directory ( char * name,  char*subitem_name, char *new_name, bool name_change, bool directory );
int add_file( char * name, int size );
int edit_file ( char * name, int size, char *new_name );
int remove_file (char* name);
int edit_directory_subitem (char* name, char* sub_name, char* new_sub_name);

void print_directory ( char *name);
char * get_directory_name ( char*name );
char * get_directory_top_level ( char*name);
char * get_directory_subitem ( char*name, int subitem_index, char * subitem );
int get_directory_subitem_count ( char*name);

char * get_file_name ( char*name );
char * get_file_top_level ( char*name);
int get_file_size( char*name);
void print_file ( char *name);

/************************** Defining Constants for fs *******************/
#define LINESIZE 128
#define DISK_PARTITION 4000000
#define BLOCK_SIZE 5000
#define BLOCKS 4000000/5000
#define MAX_STRING_LENGTH 20
#define MAX_FILE_DATA_BLOCKS (BLOCK_SIZE-64*59) //Hard-coded as of now
#define MAX_SUBDIRECTORIES  (BLOCK_SIZE - 136)/MAX_STRING_LENGTH

typedef struct {
	char directory[MAX_STRING_LENGTH];
	int directory_index;
	char parent[MAX_STRING_LENGTH];
	int parent_index;
} working_directory;


typedef struct dir_type {
	char name[MAX_STRING_LENGTH];		//Name of file or dir
	char top_level[MAX_STRING_LENGTH];	//Name of directory one level up(immediate parent)
	char (*subitem)[MAX_STRING_LENGTH];
	bool subitem_type[MAX_SUBDIRECTORIES];	//true if directory, false if file
	int subitem_count;
	struct dir_type *next;
} dir_type;


typedef struct file_type {
	char name[MAX_STRING_LENGTH];		//Name of file or dir
	char top_level[MAX_STRING_LENGTH];	//Name of directory one level up 
	int data_block_index[MAX_FILE_DATA_BLOCKS];
	int data_block_count;
	int size;
	struct file_type *next;
} file_type;

typedef struct {
	bool free[BLOCKS];
	bool directory[BLOCKS];
	char (*name)[MAX_STRING_LENGTH];
} descriptor_block;

char *disk;
working_directory current;
bool disk_allocated = false; // makes sure that do_root is first thing being called and only called once

/*--------------------------------------------------------------------------------*/

int main(int argc, char *argv[])
{
	(void)argc;
	(void)*argv;
    char in[LINESIZE];
    char *cmd, *fnm, *fsz;
    char dummy[] = "";

	//printf("sizeof file_type = %d\nsizeof dir_type = %d\nsize of descriptor_block = %d\nMAX_FILE_SIZE %d\n", sizeof(file_type), sizeof(dir_type), sizeof(descriptor_block), MAX_FILE_DATA_BLOCKS );

	printf("Welcome to your file system\n");
    int n;
    char *a[LINESIZE];
  
   while (fgets(in, LINESIZE, stdin) != NULL)
    {
      // commands are all of form "cmd filename filesize\n" with whitespace as a delimiter

      // parse input
      parse(in, &n, a);

      cmd = (n > 0) ? a[0] : dummy;
      fnm = (n > 1) ? a[1] : dummy;
      fsz = (n > 2) ? a[2] : dummy;

      if (debug) printf(":%s:%s:%s:\n", cmd, fnm, fsz);

      if (n == 0) continue;	// blank line

      int found = 0;
     
      for (struct action *ptr = table; ptr->cmd != NULL; ptr++){  
            if (strcmp(ptr->cmd, cmd) == 0)
                {
                    found = 1;
                    
                    int ret = (ptr->action)(fnm, fsz);
                    //every function returns -1 on failure
                    if (ret == -1)
                        { printf("  %s %s %s: failed\n", cmd, fnm, fsz); }
                    break;
                }
	    }
      if (!found) { printf("command not found: %s\n", cmd);}
    }

  return 0;
}


/*--------------------------------------------------------------------------------*/

void parse(char *buf, int *argc, char *argv[])
{
  char *delim;          // points to first space delimiter
  int count = 0;        // number of args

  char whsp[] = " \t\n\v\f\r";          // whitespace characters

  while (1)                             // build the argv list
    {
      buf += strspn(buf, whsp);         // skip leading whitespace
      delim = strpbrk(buf, whsp);       // next whitespace char or NULL
      if (delim == NULL)                // end of line, input parsed
        { break; }
      argv[count++] = buf;              
      *delim = '\0';                    
      buf = delim + 1;                  
    }
  argv[count] = NULL;

  *argc = count;

  return;
}

/*--------------------------------------------------------------------------------*/

//This function initializes the disk, descriptor within the disk, as well as the root directory.
int do_root(char *name, char *size)
{
	(void)*name;
	(void)*size;
	if ( disk_allocated == true )
		return 0;
		
	//Initialize disk
	disk = (char*)malloc ( DISK_PARTITION );
		if ( debug ) printf("\t[%s] Allocating [%d] Bytes of memory to the disk\n", __func__, DISK_PARTITION );

	//Add descriptor and root directory to disk
	add_descriptor("descriptor");
		if ( debug ) printf("\t[%s] Creating Descriptor Block\n", __func__ );
	add_directory("root");
		if ( debug ) printf("\t[%s] Creating Root Directory\n", __func__ );
	
	//Set up the working_directory structure
	strcpy(current.directory, "root");
	current.directory_index = 3;
	strcpy(current.parent, "" );
	current.parent_index = -1;
		if ( debug ) printf("\t[%s] Set Current Directory to [%s], with Parent Directory [%s]\n", __func__, "root", "" );
	
	if ( debug ) printf("\t[%s] Disk Successfully Allocated\n", __func__ );
	disk_allocated = true;
	
 	return 0;
}

/*--------------------------------------------------------------------------------*/

int do_print(char *name, char *size)
{
	(void)*name;
	(void)*size;
	if ( disk_allocated == false ) {
		printf("Error: Disk not allocated\n");
		return 0;
	}
	//Start with the root directory, which is directory type (true)
	printing("root");
	
	if (debug) if ( debug ) printf("\n\t[%s] Finished printing\n", __func__);
	return 0;
}

/*--------------------------------------------------------------------------------*/

int do_chdir(char *name, char *size)
{
	(void)*size;
	if ( disk_allocated == false ) {
		printf("Error: Disk not allocated\n");
		return 0;
	}
	
	//Case if ".." is the argument to "chdir"
	if ( strcmp(name, ".." ) == 0 ) {
		
		//If we are in root directory then we can't go back
		if ( strcmp(current.directory, "root") == 0 )
			return 0;
		
		//Adjust the working_directory struct
		strcpy ( current.directory, current.parent );	
		strcpy (current.parent, get_directory_top_level( current.parent) );
			if ( debug ) printf ("\t[%s] Current Directory is now [%s], Parent Directory is [%s]\n", __func__, current.directory, current.parent);
		return 0;
	}
	else
	{	
		char tmp[20];

		//Check to make sure it is a subdirectory that is to be changed
		//If name is not in the current directory then returns -1, else return 0
		if ( (strcmp(get_directory_subitem(current.directory, -1, name), "-1") == 0) && strcmp( current.parent, name ) != 0 ) {
			if ( debug ) printf( "\t\t\t[%s] Cannot Change to Directory [%s]\n", __func__, name );
			if (!debug ) printf( "%s: %s: No such file or directory\n", "chdir", name );
			return 0;
		}
	
		strcpy( tmp, get_directory_name(name));
		if ( strcmp(tmp, "") == 0 )
			return 0;
		
		if ( strcmp( tmp, name ) == 0 ) {
			//Adjust the working_directory struct
			strcpy ( current.directory, tmp);
			

			strcpy(current.parent, get_directory_top_level(name) );
				if ( debug ) printf ("\t[%s] Current Directory is now [%s], Parent Directory is [%s]\n", __func__, current.directory, current.parent);
			return 1;
		}
		return -1;
	}
  return 0;
}

/*--------------------------------------------------------------------------------*/

int do_mkdir(char *name, char *size)
{
	(void)*size;
	if ( disk_allocated == false ) {
		printf("Error: Disk not allocated\n");
		return 0;
	}	

	//If it returns 0, there is a subitem with that name already
	if ( get_directory_subitem(current.directory, -1, name) == 0  ) {
			if ( debug ) printf( "\t\t\t[%s] Cannot Make Directory [%s]\n", __func__, name );
			if (!debug ) printf( "%s: cannot create directory '%s': Folder exists\n", "mkdir", name );
			return 0;
		}

	//Call add directory
	if ( debug ) printf("\t[%s] Creating Directory: [%s]\n", __func__, name );
	if ( add_directory( name ) != 0 ) {
		if (!debug ) printf("%s: missing operand\n", "mkdir");
		return 0;
	}
	
	//Edit the current directory to add our new directory to the current directory's "subdirectory" member. 
	//NULL ==> for just editing the subdirectory
	edit_directory( current.directory, name, NULL, false, true );
		if ( debug ) printf("\t[%s] Updating Parents Subitem content\n", __func__ );
		
	if ( debug ) printf("\t[%s] Directory Created Successfully\n", __func__ );
	if( debug ) print_directory(name);
	
  	return 0;
}

/*--------------------------------------------------------------------------------*/

int do_rmdir(char *name, char *size)
{
	(void)*size;
	if ( disk_allocated == false ) {
		printf("Error: Disk not allocated\n");
		return 0;
	}
		
	if ( strcmp(name,"") == 0 ) {
		if ( debug ) printf("\t[%s] Invalid Command\n", __func__ );
		if (!debug ) printf("%s: missing operand\n", "rmdir");
		return 0;
	}
	
	if( (strcmp(name, ".") == 0) || (strcmp(name, "..") == 0) ) {
		if ( debug ) printf("\t[%s] Invalid command [%s] Will not remove directory\n", __func__, name);
		if (!debug ) printf( "%s: %s: No such file or directory\n", "rmdir", name );
		return 0;
	}
	
	//Check to make sure it is a subdirectory that is to be changed
	//If name is not in the current directory then returns -1, else return 0
	if ( strcmp(get_directory_subitem(current.directory, -1, name), "-1") == 0 ) {
		if ( debug ) printf( "\t[%s] Cannot Remove Directory [%s]\n", __func__, name );
		if (!debug ) printf( "%s: %s: No such file or directory\n", "rmdir", name );
		return 0;
	}
	
	//Remove directory from the parent's subitems.
	dir_type *folder = malloc ( BLOCK_SIZE );
	int block_index = find_block(name, true);
	memcpy( folder, disk + block_index*BLOCK_SIZE, BLOCK_SIZE );

	dir_type *top_folder = malloc ( BLOCK_SIZE );

	//The top_level is created based off the folder 
	int top_block_index = find_block(folder->top_level, true);
	memcpy( disk + block_index*BLOCK_SIZE, folder, BLOCK_SIZE );
	memcpy( top_folder, disk + top_block_index*BLOCK_SIZE, BLOCK_SIZE );

	char subitem_name[MAX_STRING_LENGTH]; // holds the current subitem in the parent directory array
	const int subcnt = top_folder->subitem_count; // no. of subitems
	int j;
	int k=0;

	//iterate through the subitem count
	for(j = 0; j<subcnt; j++) {
		strcpy(subitem_name, top_folder->subitem[j]);
		if (strcmp(subitem_name, name) != 0)
		{
			strcpy(top_folder->subitem[k],subitem_name);
			//printf("------ Subitem [%s] copied ------\n", subitem_name);
			k++;
		}
	}
	
	//Remove the directory subitem from the parent
	strcpy(top_folder->subitem[k], "");

	top_folder->subitem_count--;
	memcpy( disk + top_block_index*BLOCK_SIZE, top_folder, BLOCK_SIZE );
	free(top_folder);
	
	//Remove the directory with its contents
	if ( debug ) printf("\t[%s] Removing Directory: [%s]\n", __func__, name );
	if( remove_directory( name ) == -1 ) {
		return 0;
	}
	
	if (debug) printf("\t[%s] Directory Removed Successfully\n", __func__);
	return 0;
}

/*--------------------------------------------------------------------------------*/

int do_mvdir(char *name, char *size) //"size" is actually the new name
{
	if ( disk_allocated == false ) {
		printf("Error: Disk not allocated\n");
		return 0;
	}

	//Rename the directory
	if ( debug ) printf("\t[%s] Renaming Directory: [%s]\n", __func__, name );
	//if the directory "name" is not found, return -1
	if( edit_directory( name, "", size, true, true ) == -1 ) {
		if (!debug ) printf( "%s: cannot rename file or directory '%s'\n", "mvdir", name );
		return 0;
	}
	
	//else the directory is renamed
	if (debug) printf( "\t[%s] Directory Renamed Successfully: [%s]\n", __func__, size );
	if (debug) print_directory(size); 
	return 0;
}

/*--------------------------------------------------------------------------------*/

int do_mkfil(char *name, char *size)
{
	if ( disk_allocated == false ) {
		printf("Error: Disk not allocated\n");
		return 0;
	}
	
	if ( debug ) printf("\t[%s] Creating File: [%s], with Size: [%s]\n", __func__, name, size );
	
	//If it returns 0, there is a subitem with that name already
	if ( get_directory_subitem(current.directory, -1, name) == 0  ) {
			if ( debug ) printf( "\t\t\t[%s] Cannot make file [%s], a file or directory [%s] already exists\n", __func__, name, name );
			if (!debug ) printf( "%s: cannot create file '%s': File exists\n", "mkfil", name );
			return 0;
		}
	
	if ( add_file ( name, atoi(size)) != 0 )
		return 0;
	
	//Edit the current directory to add our new file to the current directory's "subdirectory" member.
  	edit_directory( current.directory, name, NULL, false, false);
  		if ( debug ) printf("\t[%s] Updating Parents Subitem content\n", __func__ );
  	
  	if ( debug ) print_file(name);
  	return 0;
}

/*--------------------------------------------------------------------------------*/

// Remove a file
int do_rmfil(char *name, char *size)
{
	if ( disk_allocated == false ) {
		printf("Error: Disk not allocated\n");
		return 0;
	}
	
	(void)*size;
	if ( debug ) printf("\t[%s] Removing File: [%s]\n", __func__, name);

		//If the file to be removed actually exists in current directory, remove it
	if ( get_directory_subitem(current.directory, -1, name) == 0  ) {
			remove_file(name);
			return 0;
		}
	else{ // If it doesn't exist, print error and return 0
		if ( debug ) printf( "\t\t\t[%s] Cannot remove file [%s], it does not exist in this directory\n", __func__, name );
		if (!debug ) printf( "%s: %s: No such file or directory\n", "rmfil", name );
		return 0;
	}
}

/*--------------------------------------------------------------------------------*/

// Rename a file
int do_mvfil(char *name, char *size)
{
	if ( disk_allocated == false ) {
		printf("Error: Disk not allocated\n");
		return 0;
	}
	
	if ( debug ) printf("\t[%s] Renaming File: [%s], to: [%s]\n", __func__, name, size );

	//If it returns 0, there is a subitem with that name already
	if ( get_directory_subitem(current.directory, -1, size) == 0  ) {
			if ( debug ) printf( "\t\t\t[%s] Cannot rename file [%s], a file or directory [%s] already exists\n", __func__, name, size );
			if (!debug ) printf( "%s: cannot rename file or directory '%s'\n", "mvfil", name );
			return 0;
		}

	int er = edit_file( name, 0, size);
	
	if (er == -1) return -1;
	if (debug) print_file(size);

	return 0;
}

/*--------------------------------------------------------------------------------*/

// Resize a file -- addon to be implemented
int do_szfil(char *name, char *size)
{
	if ( disk_allocated == false ) {
		printf("Error: Disk not allocated\n");
		return 0;
	}
	
	if ( debug ) printf("\t[%s] Resizing File: [%s], to: [%s]\n", __func__, name, size );
	//remove file; make new file with updated size
	if (remove_file(name) != -1)  do_mkfil(name, size);

	else {
		if ( debug ) printf("\t[%s] File: [%s] does not exist. Cannot resize.\n", __func__, name);
		if (!debug ) printf( "%s: cannot resize '%s': No such file or directory\n", "szfil", name );
	}

	return 0;
}

/*--------------------------------------------------------------------------------*/

int do_exit(char *name, char *size)
{
	(void)*name;
	(void)*size;
	if (debug) printf("\t[%s] Exiting\n", __func__);
	exit(0);
	return 0;
}

/*--------------------------------------------------------------------------------*/


/******************************* Helper Functions Start *****************************/

//Prints the information of directories and files starting at the root
void printing(char *name) {
	//Allocate memory to a dir_type so that we can copy the folder from memory into this variable.
	dir_type *folder = malloc (BLOCK_SIZE);
	int block_index = find_block(name, true);

	memcpy( folder, disk + block_index*BLOCK_SIZE, BLOCK_SIZE);
		
	printf("%s:\n", folder->name);
	for( int i = 0; i < folder->subitem_count; i++ ) {
		 printf("\t%s\n", folder->subitem[i]);
	}

	//Go through again if there is a subdirectory
	for( int i = 0; i < folder->subitem_count; i++ ) {
		if( folder->subitem_type[i] == true ) {
			//Recursively call the function! can be improved further ;)
			printing(folder->subitem[i]);
		}
	}
}

/*--------------------------------------------------------------------------------*/

//Displays the content of the descriptor block and free block table.
void print_descriptor ( ) {
	descriptor_block *descriptor = malloc( BLOCK_SIZE*2 );

	memcpy ( descriptor, disk, BLOCK_SIZE*2 );

	printf("Disk Descriptor Free Table:\n");
	
	for ( int i = 0; i < BLOCKS ; i++ ) {
		printf("\tIndex %d : %d\n", i, descriptor->free[i]);
	}
	
	free(descriptor);
}

/*--------------------------------------------------------------------------------*/

//finds the first free block on the disk; the free block index is returned
int allocate_block ( char *name, bool directory ) { 

	descriptor_block *descriptor = malloc( BLOCK_SIZE*2);

	memcpy ( descriptor, disk, BLOCK_SIZE*2 );
	
	//Goes through every block until free one is found	
	if ( debug ) printf("\t\t\t[%s] Finding Free Memory Block in the Descriptor\n", __func__ );
	for ( int i = 0; i < BLOCKS; i++ ) {
		 if ( descriptor->free[i] ) {
		 	//Once free block is found, update descriptor information
			descriptor->free[i] = false;
			descriptor->directory[i] = directory;
			strcpy(descriptor->name[i], name);
			
			//update descriptor back to the beginning of the disk
			memcpy(disk, descriptor, BLOCK_SIZE*2);
				if ( debug ) printf("\t\t\t[%s] Allocated [%s] at Memory Block [%d]\n", __func__, name, i );

			free(descriptor);
			return i; 
		}
	}
	free(descriptor);
	if ( debug ) printf("\t\t\t[%s] No Free Space Found: Returning -1\n", __func__);
	return -1;
}

/*--------------------------------------------------------------------------------*/

//updates the descriptor block on disk to reflect that the block is no longer in use. 
void unallocate_block ( int offset ) { 
	descriptor_block *descriptor = malloc( BLOCK_SIZE*2 );
	
	memcpy ( descriptor, disk, BLOCK_SIZE*2 );
	
	//TODO: check if the block holds a file, and then unallocate all its sub-block
	if ( debug ) printf("\t\t\t[%s] Unallocating Memory Block [%d]\n", __func__, offset );
	descriptor->free[offset] = true;
	strcpy( descriptor->name[offset], "" );

	memcpy ( disk, descriptor, BLOCK_SIZE*2 );	
	
	free(descriptor);
}

/*--------------------------------------------------------------------------------*/

//Takes in a name, and searches through descriptor block to find the block that contains the item
int find_block ( char *name, bool directory ) {

	descriptor_block *descriptor = malloc( BLOCK_SIZE*2 );

	memcpy ( descriptor, disk, BLOCK_SIZE*2 );
	
	if ( debug ) printf("\t\t\t[%s] Searching Descriptor for [%s], which is a [%s]\n", __func__, name, directory == true ? "Folder": "File" );
	for ( int i = 0; i < BLOCKS; i++ ) {
		if ( strcmp(descriptor->name[i], name) ==0 ){
			//Make sure it is of the type that we are searching for
			if ( descriptor->directory[i] == directory ) {
				if ( debug ) printf("\t\t\t[%s] Found [%s] at Memory Block [%d]\n", __func__, name, i );
				free(descriptor);
				//Return the block index where the item resides in memory
				return i;
			}
		}
	}
	
	free(descriptor);
	if ( debug ) printf("\t\t\t[%s] Block Not Found: Returning -1\n", __func__);
	return -1;
}

/*--------------------------------------------------------------------------------*/

int add_descriptor ( char * name ) {
	//Allocate memory to a descriptor_block type so that we start assigning values to its members.
	descriptor_block *descriptor = malloc( BLOCK_SIZE*2);
		if ( debug ) printf("\t\t[%s] Allocating Space for Descriptor Block\n", __func__);
	
	//Allocate memory to the array of strings within the descriptor block, which holds the name of each block
	descriptor->name = malloc ( sizeof*name*BLOCKS );
		if ( debug ) printf("\t\t[%s] Allocating Space for Descriptor's Name Member\n", __func__);
	
	//initialize each block ==> that it is free
	if ( debug ) printf("\t\t[%s] Initializing Descriptor to Have All of Memory Available\n", __func__);
	for (int i = 0; i < BLOCKS; i++ ) {
		descriptor->free[i] = true;
		descriptor->directory[i] = false;
	}

	//descriptor occupied space on the disk 
	int limit = (int)(sizeof(descriptor_block)/BLOCK_SIZE) + 1;
	
	if ( debug ) printf("\t\t[%s] Updating Descriptor to Show that first [%d] Memory Blocks Are Taken\n", __func__, limit+1);
	for ( int i = 0; i < limit; i ++ ) {
		descriptor->free[i]= false; //marking space occupied by descriptor as used
	}
	
	strcpy(descriptor->name[0], "descriptor"); 	
	
	//writing new updated descriptor to the beginning of the disk
	//may encounter error here, to be fixed
	memcpy ( disk, descriptor, (BLOCK_SIZE*(limit+1)));

	return 0;	
}

/*--------------------------------------------------------------------------------*/

//Allows us to directly update values in the descriptor block. 
int edit_descriptor ( int free_index, bool free, int name_index, char * name ) {

	descriptor_block *descriptor = malloc( BLOCK_SIZE*2 );
	
	//Copy descriptor on disk to our descriptor_block type, 
	memcpy ( descriptor, disk, BLOCK_SIZE*2 );

	//Each array in the descriptor block will be updated
	if ( free_index > 0 ) {
		descriptor->free[free_index] = free;
			if ( debug ) printf("\t\t[%s] Descriptor Free Member now shows Memory Block [%d] is [%s]\n", __func__, free_index, free == true ? "Free": "Used");
	}
	if ( name_index > 0 ) {
		strcpy(descriptor->name[name_index], name );
			if ( debug ) printf("\t\t[%s] Descriptor Name Member now shows Memory Block [%d] has Name [%s]\n", __func__, name_index, name);	
	}
		
	// write the new updated descriptor back to the beginning of the disk
	memcpy(disk, descriptor, BLOCK_SIZE*2);

	return 0;
}

/*--------------------------------------------------------------------------------*/

// This changes the name of a file in the descriptor; used for moving files;
int edit_descriptor_name (int index, char* new_name)
{
	descriptor_block *descriptor = malloc( BLOCK_SIZE*2 );

	memcpy ( descriptor, disk, BLOCK_SIZE*2 );

	// Change the name of the file at index to the new_name
	strcpy(descriptor->name[index], new_name);

	memcpy(disk, descriptor, BLOCK_SIZE*2);

	free(descriptor);
	return 0;
}

/*--------------------------------------------------------------------------------*/

//Allows us to add a folder to the disk.
int add_directory( char * name ) {
	
	if ( strcmp(name,"") == 0 ) {
		if ( debug ) printf("\t\t[%s] Invalid Command\n", __func__ );
		return -1;
	}
	
	//Allocating memory for new folder
	dir_type *folder = malloc ( BLOCK_SIZE);
		if ( debug ) printf("\t\t[%s] Allocating Space for New Folder\n", __func__);
	
	//Initialize our new folder
	strcpy(folder->name, name);					
	strcpy(folder->top_level, current.directory);
	folder->subitem = malloc ( sizeof*(folder->subitem)*MAX_SUBDIRECTORIES);
	folder->subitem_count = 0;					// Imp : Initialize subitem array to have 0 elements
	

	//Find free block in disk to store our folder; true => mark the block as directory
	int index = allocate_block(name, true);
		if ( debug ) printf("\t\t[%s] Assigning New Folder to Memory Block [%d]\n", __func__, index);
		
	//Copy our folder to the disk
	memcpy( disk + index*BLOCK_SIZE, folder, BLOCK_SIZE);
	
	if ( debug ) printf("\t\t[%s] Folder [%s] Successfully Added\n", __func__, name);
	free(folder);
	return 0;
}

/*--------------------------------------------------------------------------------*/

//Allows to remove a directory folder from the disk.
int remove_directory( char * name ) {
	
	dir_type *folder = malloc (BLOCK_SIZE);
	int block_index = find_block(name, true);
	
	//If there was no subdirectory found, then return -1
	if( block_index == -1 ) {
		if ( debug ) printf("\t\t[%s] Directory [%s] does not exist in the current folder [%s]\n", __func__, name, current.directory);
		return -1;
	}

	memcpy( folder, disk + block_index*BLOCK_SIZE, BLOCK_SIZE );

	//Go through again if there is a subdirectory ==> as implemented in Unix
	for( int i = 0; i < folder->subitem_count; i++ ) {
		if( folder->subitem_type[i] == true ) {
			//Recursively call the function to remove the subitem
			remove_directory(folder->subitem[i]);
		}
		else {
			//Remove the subitem that is a file
			remove_file(folder->subitem[i]);
		}
	}
	unallocate_block(block_index);
	free(folder);
	
	return 0;
}

/*--------------------------------------------------------------------------------*/

//Allows you to directly add items to a folders subitem array, or change the folder's name
int edit_directory (char * name,  char*subitem_name, char *new_name, bool name_change, bool directory ) {
	
	if( strcmp(name,"") == 0 ) {
		if( debug ) printf("\t\t[%s] Invalid Command\n", __func__ );
		return -1;
	}

	dir_type *folder = malloc ( BLOCK_SIZE);
	
	//Find where the folder is on disk	
	int block_index = find_block(name, true);
	//If the directory is not found, should return
	if( block_index == -1 ) {
		if ( debug ) printf("\t\t[%s] Directory [%s] does not exist\n", __func__, name);
		return -1;
	}
		if ( debug ) printf("\t\t[%s] Folder [%s] Found At Memory Block [%d]\n", __func__, name, block_index);
	
	memcpy( folder, disk + block_index*BLOCK_SIZE, BLOCK_SIZE);

	if ( strcmp(subitem_name, "") != 0 ) {	//Case that we are adding subitem to the descriptor block
		
		if ( !name_change ) {     //Case adding subitem
			if ( debug ) printf("\t\t[%s] Added Subitem [%s] at Subitem index [%d] to directory [%s]\n", __func__, subitem_name, folder->subitem_count, folder->name );
			strcpy (folder->subitem[folder->subitem_count], subitem_name );
			folder->subitem_type[folder->subitem_count] = directory;
			folder->subitem_count++;
				if ( debug ) printf("\t\t[%s] Folder [%s] Now Has [%d] Subitems\n", __func__, name, folder->subitem_count);

			//update the disk too!	
			memcpy( disk + block_index*BLOCK_SIZE, folder, BLOCK_SIZE);
			
			free(folder);
			return 0;
		}
		else {				//Case editing a subitem's name
			for ( int i =0; i < folder->subitem_count; i++ ) {
				if ( strcmp(folder->subitem[i], subitem_name) == 0 ) {
					strcpy( folder->subitem[i], new_name);
						if ( debug ) printf("\t\t[%s] Edited Subitem [%s] to [%s] at Subitem index [%d] for directory [%s]\n", __func__, subitem_name, new_name, i, folder->name );	

					memcpy( disk + block_index*BLOCK_SIZE, folder, BLOCK_SIZE);
					free(folder);
					return 0;
				}
			}

			if ( debug ) printf("\t\t[%s] Subitem Does Not Exist in Directory [%s]\n", __func__, folder->name );
			free(folder);
			return -1;
		}
	}
	else {						//Case we are changing the folders name
		//if directory with given name already exists, don't allow
		int block_index2 = find_block(new_name, true);

		//If the directory for the new name already exists, should return -1
		if( block_index2 != -1 ) {
			if ( debug ) printf("\t\t[%s] Directory [%s] already exists. Choose a different name\n", __func__, new_name);
			return -1;
		}
	
		strcpy(folder->name, new_name );
			if ( debug ) printf("\t\t[%s] Folder [%s] Now Has Name [%s]\n", __func__, name, folder->name);
		
		memcpy( disk + block_index*BLOCK_SIZE, folder, BLOCK_SIZE);
		
		//edit descriptors
		edit_descriptor(-1, false, block_index, new_name );
			if ( debug ) printf("\t\t[%s] Updated Descriptor's Name Member\n", __func__);
			if ( debug ) print_directory(folder->name);
		
		//changing parents name
		edit_directory(folder->top_level, name, new_name, true, true );
			if ( debug ) printf("\t\t[%s] Updated Parents Subitem Name\n", __func__);
		
		int child_index;

		//Iterates through to change the subitems' top_level name
		for ( int i = 0; i < folder->subitem_count; i++) {
			file_type *child_file = malloc ( BLOCK_SIZE);
			dir_type *child_folder = malloc ( BLOCK_SIZE);
			
			child_index = find_block ( folder->subitem[i], folder->subitem_type);
			if ( folder->subitem_type[i] ) {
				//if type == folder
				memcpy( child_folder, disk + child_index*BLOCK_SIZE, BLOCK_SIZE);
				strcpy( child_folder->top_level, new_name );
				
				memcpy( disk + child_index*BLOCK_SIZE, child_folder, BLOCK_SIZE);
				free ( child_folder );
				free ( child_file );
			}
			else {
				//if type == file
				memcpy( child_file, disk + child_index*BLOCK_SIZE, BLOCK_SIZE);
				strcpy( child_file->top_level, new_name );
			
				memcpy( disk + child_index*BLOCK_SIZE, child_file, BLOCK_SIZE);	
				free ( child_folder );
				free ( child_file );
			} 
		}
			
		free(folder);
		return 0;
	}
		
	free ( folder );
}

/*--------------------------------------------------------------------------------*/

//Allows us to add a file to our disk; This function will allocate this file descriptor block (holds file info), as well as data blocks 
int add_file( char * name, int size ) {
	char subname[20];
	
	if ( size < 0 || strcmp(name,"") == 0 ) {
		if ( debug ) printf("\t\t[%s] Invalid command\n", __func__);
		if (!debug ) printf("%s: missing operand\n", "mkfil");
		return 1;
	}
		
	
	//Allocate memory to a file_type
	file_type *file = malloc ( BLOCK_SIZE );
		if ( debug ) printf("\t\t[%s] Allocating Space for New File\n", __func__);
		
	//Initialize all the members of our new file
	strcpy( file->name, name);	
	strcpy ( file->top_level, current.directory );
	file->size = size;		
	file->data_block_count = 0;
		if ( debug ) printf("\t\t[%s] Initializing File Members\n", __func__);
				
	//Find free block to put this file descriptor block in memory, false ==> indicates a file
	int index = allocate_block(name, false);
	
	//Find free blocks to put the file data into
	if ( debug ) printf("\t\t[%s] Allocating [%d] Data Blocks in Memory for File Data\n", __func__, (int)size/BLOCK_SIZE);
	for ( int i = 0; i < size/BLOCK_SIZE + 1; i++ ) {
		sprintf(subname, "%s->%d", name, i);
		file->data_block_index[i] = allocate_block(subname, false);
		file->data_block_count++;
	}  
	//data blocks in memory not copied to disk
	memcpy( disk + index*BLOCK_SIZE, file, BLOCK_SIZE);
	
	if ( debug ) printf("\t\t[%s] File [%s] Successfully Added\n", __func__, name);
	
	free(file);
	return 0;
}

/*--------------------------------------------------------------------------------*/

int remove_file (char* name)
{
	if (strcmp(name,"") == 0 ) {
		if ( debug ) printf("\t\t[%s] Invalid command\n", __func__);
		if (!debug ) printf("%s: missing operand\n", "rmfil");
		return 1;
	}

	file_type *file = malloc ( BLOCK_SIZE);
	dir_type *folder = malloc ( BLOCK_SIZE);
	
	int file_index = find_block(name, false);
	
	// If the file is not found, error, return -1
	if ( file_index == -1 )  {
		if ( debug ) printf("\t\t\t[%s] File [%s] not found\n", __func__, name);
		return -1;
	}
	
	if ( debug ) printf("\t\t[%s] File [%s] Found At Memory Block [%d]\n", __func__, name, file_index);
	
	memcpy( file, disk + file_index*BLOCK_SIZE, BLOCK_SIZE);
	
	//Find the top_level folder on disk
	int folder_index = find_block(file->top_level, true);
	
	if ( debug ) printf("\t\t[%s] Folder [%s] Found At Memory Block [%d]\n", __func__, name, folder_index);
	memcpy( folder, disk + folder_index*BLOCK_SIZE, BLOCK_SIZE);
	
	
	// Go through the parent directory's subitem array and remove our file
	char subitem_name[MAX_STRING_LENGTH]; 
	const int subcnt = folder->subitem_count; // no of subitems
	int j;
	int k=0;
	for(j = 0; j<subcnt; j++)
		{
			strcpy(subitem_name, folder->subitem[j]);
			if (strcmp(subitem_name, name) != 0) 
			// if this element is not the one we are removing, copy back
			{
				strcpy(folder->subitem[k],subitem_name);
				k++;
			}
		}
	strcpy(folder->subitem[k], "");
	folder->subitem_count--;

	memcpy(disk + folder_index*BLOCK_SIZE, folder, BLOCK_SIZE); // Update the folder in memory


	//Imp :  Unallocate all of the data blocks from the file that we are deleting
	int i = 0;
	while(file->data_block_count != 0)
	{
		unallocate_block(file->data_block_index[i]);
		file->data_block_count--;
		i++;
	}
	
	unallocate_block(file_index); // Deallocate the file control block
	
	free(folder);
	free(file);
	return 0;
}

/*--------------------------------------------------------------------------------*/

//Allows you to directly edit a file and change its size or its name
int edit_file ( char * name, int size, char *new_name ) {
	file_type *file = malloc ( BLOCK_SIZE);
	
	//Find the block in memory where this file is written	
	int block_index = find_block(name, false);
	if ( block_index == -1 )  {
		if ( debug ) printf("\t\t\t[%s] File [%s] not found\n", __func__, name);
		return -1;
	}
	if ( debug ) printf("\t\t[%s] File [%s] Found At Memory Block [%d]\n", __func__, name, block_index);

	memcpy( file, disk + block_index*BLOCK_SIZE, BLOCK_SIZE);
	
	if ( size > 0 ) { 
		//If size is greater than zero, then the files size will be updated
		file->size = size;
		if ( debug ) printf("\t\t[%s] File [%s] Now Has Size [%d]\n", __func__, name, size);
		free(file);
		return 0;
	}
	else {		  
		//Otherwise, the file's name will be updated
		char top_level[MAX_STRING_LENGTH];
		strcpy(top_level, get_file_top_level(name));

		// Change the name of the directory's subitem
		edit_directory_subitem(top_level, name, new_name); 

		// Change the name of the actual file descriptor
		edit_descriptor_name(block_index, new_name); 

		strcpy(file->name, new_name );
		memcpy( disk + block_index*BLOCK_SIZE, file, BLOCK_SIZE);	

		if ( debug ) printf("\t\t\t[%s] File [%s] Now Has Name [%s]\n", __func__, name, file->name);

		free(file);
		return 0;
	}
}

/*--------------------------------------------------------------------------------*/

/************************** Getter functions ************************************/
char * get_directory_name ( char*name ) {
	dir_type *folder = malloc ( BLOCK_SIZE);
	char *tmp = malloc(sizeof(char)*MAX_STRING_LENGTH); 
		
	//True arguement tells the find function that we are looking
	//for a directory not a file
	int block_index = find_block(name, true);
	if ( block_index == -1 )  {
		if ( debug ) printf("\t\t\t[%s] Folder [%s] not found\n", __func__, name);
		strcpy ( tmp, "");
		return tmp;
	}
	
	memcpy( folder, disk + block_index*BLOCK_SIZE, BLOCK_SIZE);
	
	strcpy( tmp, folder->name);
		if ( debug ) printf("\t\t\t[%s] Name [%s] found for [%s] folder\n", __func__, tmp, name );
		
	free ( folder );
	return tmp;
}

/*--------------------------------------------------------------------------------*/

char * get_directory_top_level ( char*name) {
	dir_type *folder = malloc ( BLOCK_SIZE);
	char *tmp = malloc(sizeof(char)*MAX_STRING_LENGTH); 

	//true ==> indicates a folder and not a file
	int block_index = find_block(name, true);
	if ( block_index == -1 )  {
		if ( debug ) printf("\t\t\t[%s] Folder [%s] not found\n", __func__,name);
		strcpy ( tmp, "");
		return tmp;
	}
	
	memcpy( folder, disk + block_index*BLOCK_SIZE, BLOCK_SIZE);
	
	strcpy( tmp, folder->top_level);
		if ( debug ) printf("\t\t\t[%s] top_level [%s] found for [%s] folder\n", __func__, tmp, name );
	
	free ( folder );
	return tmp;
}

/*--------------------------------------------------------------------------------*/

char * get_directory_subitem ( char*name, int subitem_index, char*subitem_name ) {
	dir_type *folder = malloc ( BLOCK_SIZE);
	char *tmp = malloc(sizeof(char)*MAX_STRING_LENGTH); 

	int block_index = find_block(name, true);
	if ( block_index == -1 ) {
		if ( debug ) printf("\t\t\t[%s] Folder [%s] not found\n", __func__, name);
		strcpy ( tmp, "");
		return tmp;
	}
	
	memcpy( folder, disk + block_index*BLOCK_SIZE, BLOCK_SIZE);
	
	if ( subitem_index >= 0 ) { 
		//Case we are changing the name of a subitem
		strcpy( tmp, folder->subitem[subitem_index]);
		if ( debug ) printf("\t\t\t[%s] subitem[%d] = [%s] for [%s] folder\n", __func__, subitem_index, tmp, name );
		free(folder);
		return tmp;
	}
	else { 			 
		//Case that we Are Searching for a Sub Item
		for ( int i =0; i < folder->subitem_count; i ++ ) {
			if ( strcmp( folder->subitem[i], subitem_name ) == 0 ) {
				if ( debug ) printf( "\t\t\t[%s] Found [%s] as a Subitem of Directory [%s]\n", __func__, subitem_name, name );
				return "0";
			}
		}
		if ( debug ) printf( "\t\t\t[%s] Did Not Find [%s] as a Subitem of Directory [%s]\n", __func__, subitem_name, name );
		free ( folder );
		return "-1";
	}
	free ( folder );
	return tmp;
}

/*--------------------------------------------------------------------------------*/

int edit_directory_subitem (char* name, char* sub_name, char* new_sub_name)
{
	dir_type *folder = malloc ( BLOCK_SIZE);

	//True argument tells the find function that we are looking
	//for a directory not a file
	int block_index = find_block(name, true);
	if ( block_index == -1 ) {
		if ( debug ) printf("\t\t\t[%s] Folder [%s] not found\n", __func__, name);
	}
	
	memcpy( folder, disk + block_index*BLOCK_SIZE, BLOCK_SIZE);

	const int cnt = folder->subitem_count;	
	int i;
	for (i=0; i < cnt; i++)
	{
		if (strcmp(folder->subitem[i], sub_name) == 0)
		{
			strcpy(folder->subitem[i], new_sub_name);
			if (debug) printf("\t\t\t[%s] Edited subitem in %s from %s to %s\n", __func__, folder->name, sub_name, folder->subitem[i]);

			memcpy(disk + block_index*BLOCK_SIZE ,folder, BLOCK_SIZE);
			free(folder);
			return i;
		}
	}

	free(folder);
	return -1;
}

/*--------------------------------------------------------------------------------*/

int get_directory_subitem_count( char*name) {
	
	dir_type *folder = malloc ( BLOCK_SIZE);
	int tmp;

	int block_index = find_block(name, true);
	if ( block_index == -1 ) {
		if ( debug ) printf("\t\t\t[%s] Folder [%s] not found\n", __func__, name);
		return -1;
	}
	
	memcpy( folder, disk + block_index*BLOCK_SIZE, BLOCK_SIZE);
 	
 	tmp = folder->subitem_count;
		if ( debug ) printf("\t\t\t[%s] subitem_count [%d] found for [%s] folder\n", __func__, folder->subitem_count, name );
	
	free ( folder );
	return tmp;
}

/*--------------------------------------------------------------------------------*/

char * get_file_name ( char*name ) {
	file_type *file = malloc ( BLOCK_SIZE);
	char *tmp = malloc(sizeof(char)*MAX_STRING_LENGTH); 
		
	//false ==> indicates a file and not a folder 
	int block_index = find_block(name, false);
	if ( block_index == -1 ) {
		if ( debug ) printf("\t\t\t[%s] File [%s] not found\n", __func__, name);
		strcpy ( tmp, "");
		return tmp;
	}
				
	memcpy( file, disk + block_index*BLOCK_SIZE, BLOCK_SIZE);
	
	strcpy( tmp, file->name);
		if ( debug ) printf("\t\t\t[%s] Name [%s] found for [%s] file\n", __func__, tmp, name );
		
	free ( file );
	return tmp;
}

/*--------------------------------------------------------------------------------*/

char * get_file_top_level ( char*name) {
	file_type *file = malloc ( BLOCK_SIZE);
	char *tmp = malloc(sizeof(char)*MAX_STRING_LENGTH); 

	int block_index = find_block(name, false);
	if ( block_index == -1 ) {
		if ( debug ) printf("\t\t\t[%s] File [%s] not found\n", __func__, name);
		strcpy ( tmp, "");
		return tmp;
	}
		
	memcpy( file, disk + block_index*BLOCK_SIZE, BLOCK_SIZE);
	
	strcpy( tmp, file->top_level);
		if ( debug ) printf("\t\t\t[%s] top_level [%s] found for [%s] file\n", __func__, tmp, name );
	
	free ( file );
	return tmp;
}

/*--------------------------------------------------------------------------------*/

int get_file_size( char*name) {
	
	file_type *file = malloc ( BLOCK_SIZE);
	int tmp;

	int block_index = find_block(name, false);
	if ( block_index == -1 ) {
		if ( debug ) printf("\t\t\t[%s] File [%s] not found\n", __func__, name);
		return -1;
	}
		
	memcpy( file, disk + block_index*BLOCK_SIZE, BLOCK_SIZE);
 	
 	tmp = file->size;
		if ( debug ) printf("\t\t\t[%s] size of [%d] found for [%s] file\n", __func__, tmp, name );
	
	free ( file );
	return tmp;
}

/*--------------------------------------------------------------------------------*/

/********************************* Print Functions ********************************/
void print_directory ( char *name) {
	dir_type *folder = malloc( BLOCK_SIZE);
	int block_index = find_block(name, true);
	memcpy( folder, disk + block_index*BLOCK_SIZE, BLOCK_SIZE);
	
	printf("	-----------------------------\n");
	printf("	New Folder Attributes:\n\n\tname = %s\n\ttop_level = %s\n\tsubitems = ", folder->name, folder->top_level);
	for (int i = 0; i < folder->subitem_count; i++) {
		printf( "%s ", folder->subitem[i]);
	}
	printf("\n\tsubitem_count = %d\n", folder->subitem_count);
	printf("	-----------------------------\n");
	
	free(folder);
}

void print_file ( char *name) {
	file_type *file = malloc( BLOCK_SIZE);
	int block_index = find_block(name, false);
	memcpy( file, disk + block_index*BLOCK_SIZE, BLOCK_SIZE);
	
	printf("	-----------------------------\n");
	printf("	New File Attributes:\n\n\tname = %s\n\ttop_level = %s\n\tfile size = %d\n\tblock count = %d\n", file->name, file->top_level, file->size, file->data_block_count);
	printf("	-----------------------------\n");
	
	free(file);
}

/*--------------------------------------------------------------------------------*/