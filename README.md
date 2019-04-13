# Simulation of File System!

This repo is a simulation of File System in C Language.

- Following basic operations can be performed :

|COMMAND                          |ACTION                      |
|----------------|-------------------------------
|`root`            |initialize root directory            
|`print`            |print current working directory and all descendants            
|`chdir`|change current working directory (.. refers to parent directory)
|`mkdir`            |sub-directory create  
|`rmdir`            |delete a directory     
|`mvdir`            |move a directory  
|`mkfil`		| file create
|`rmfil`| file delete
|`mvfil` | file rename
|`szfil` | file resize
|`exit`| quit the program

- To run the file system, run in the terminal the following commands: 
	> **`gcc -o fs simulatedFileSystem.c && ./fs `**
