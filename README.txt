Filename: README.txt
Author: James Mills
Date: Summer 2016

Overview:
	smallsh - minimal bash shell, which features:
	 * following built-in commands - these don't support
	   i/o redirection and do not have exit status
		- exit
			- exits shell
			- kills all processes and jobs first
		- cd
			- changes HOME env variablea
			- supports one argument for either
		- status
			- prints out exit status OR terminating
			  signal of last fg process
	 * command execution, with:
	 	- redirection of standard i/o
	 	- foreground / background processes
			- bg: last arg must be &
	 	- signal handling:
	 		- CTRL-C sends SIGINT to parent process and
		  children; foreground process handles its
		  own SIGINT i.e. parent doesn't kill fg
