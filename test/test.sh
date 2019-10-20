xterm -title "Server" -hold -e "./build/server 7777" &
xterm -title "Client" -hold -e "./build/client 127.0.0.1 7777; SUB 2; SEND 2 HI; NEXT 2" 
