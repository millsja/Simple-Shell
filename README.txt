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
	 
	hints and tips (for me):
	 * fflush() after each and every output of text
	 * commands are/can be:
		- max 2048 characters
		- max 512 args
		- blank lines
		- comments starting with #
	 * consider using the following structure:
		- parent process (shell) continues running
		- non-built in commands cause fork
			- this child handles i/o then...
			- calls exec
	 * note: redirection symbols and dest/source are
			 NOT passed on to children i.e. you must
			 handle that yourself
