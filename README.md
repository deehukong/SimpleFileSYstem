

# SimpleFileSystem

this is a file system implemeted similar to unix V6 file system <br /> 
supporting mkdir, exporting, importing file, and remove files. <br />
large and small file has different structre to have better space efficiency. <br /><br />
small file: direct address blcok<br/>
large file : triple indrect address blocks : supporting single file with size up to 4GB.


## purpose of this project:

put a file under same directory, renamed it as large.bin <br />
importing this external file inside the system(the system is the file fileSystem.data) and then exporting the file out named as copyed.data <br />
then use cmp command to compare the exported file with the original file.


## how to compile :

gcc file_system_proj2.c  <br/>
then run the a.out file

