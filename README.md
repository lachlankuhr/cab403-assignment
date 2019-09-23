# Process Management and Distributed Process Management and Distributed Computing - CAB403 Computing - CAB403

## Project Layout
### Source Code
Source code is to be included in the "src" directory under the respective programs. "client.c" is the client program. "server.c" is the server program. Functions should not have a header comment in the source code. Provide this in the header file. Using intellisense, the comment can be viewed by hovering for the function in the source code. 

### Header Files
All function prototypes are to be included in the header files found in the "include" directory. Any function that is wrote should be found here with comments. 

## Building & Running
### Building
The make file has 4 targets for "all", "server", "client", and "clean". The names alongside the code is self documenting. 

* Run ```make``` in the top level directory to build the client and server. 
* Run ```make server``` the top level directory to just build the server. 
* Run ```make server``` the top level directory to just build the client.
* Run ```make clean``` the top level directory to delete all contents in the build folder.  

### Running

* ```./run_client.sh``` runs the client with the command line parameters 127.0.0.1 and 7777.
* ```./run_server.sh``` runs the server with command line paramater 7777.

In order to run this files, you may need to execute

```
sudo chmod +x run_client.sh
sudo chmod +x run_server.sh
```

to give it the appropariate permissions to run. 