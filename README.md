# small shell
<ul>
<li>- has a subset features of well-known shells such as bash</li>
<li>- command `exit`, `cd`, `status` executed via code</li>
<li>- other commands using functions from `exec` family</li>
<li>- support input and output redirection</li>
<li>- support foreground and background process</li>
<li>- custom handlers for signal SIGINT and SIGTSTP</li>
    - SIGINT behavior: parent process ignore the signal
                       child process running background ignore the signal
                       child process running foreground terminate itself
                       parent process print the number of signal that killed its foreground child
    - SIGTSTP behavior: child process running background or foreground ignore the signal
                        when parent process receives the signal, the shell display "Entering foreground-only mode (& is
                        now ignored)". 
                        subsequent commands can no longer run in the background
                        if user send the signal again, the shell display message "Exiting foreground-only mode"
                        shell return to normal condition by using `&` operator to execute in the background 


## command for compile:
`gcc --std=gnu99 -g smallsh.c -o smallsh`

## command for execute the program:
`./smallsh`
