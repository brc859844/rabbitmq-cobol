        identification division.
        program-id.    demo02.
        data division.
        working-storage section.

        01 rv                   binary-long.
        01 len                  binary-long.

        01 url                  pic x(50) value "amqp://guest:guest@10.10.116.196:5672".
        01 exchange             pic x(50) value "amq.direct".
        01 routing-key          pic x(50) value "SVC1".
        01 rqst                 pic x(50) value "RPC test message".

        01 repl                 pic x(100).
        01 error-text           pic x(100).

        01 conn                 usage pointer.


        procedure division.

        move length of url to len.
        call "RMQ_CONNECT" using
                        by reference conn
                        by reference url
                        by value len
                        giving rv.

        if rv = 0
           call "RMQ_STRERROR" using
                        by value 0
                        by reference error-text
                        by value 50
           end-call

           display error-text
           stop run
        end-if.

        move 100 to len.

        call "RMQ_RPC_CALL" using
                        by value conn
                        by reference exchange
                        by value 10
                        by reference routing-key
                        by value 4 
                        by reference rqst
                        by value 16
                        by reference repl
                        by reference len
                        giving rv.

        if rv = 0
           call "RMQ_STRERROR" using
                        by value 0
                        by reference error-text
                        by value 50
           end-call

           display error-text
           stop run
        end-if.


        display repl(1:len).

        call "RMQ_DISCONNECT" using by value conn.
        stop run.

end program demo02.
